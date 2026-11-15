/**************************************************************************/
/*  test_graph_same_buffer_copy.cpp                                           */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "servers/rendering/rendering_device_graph.h"
#include "tests/servers/rendering/transfer_test_driver_base.h"
#include "tests/test_macros.h"

TEST_FORCE_LINK(test_graph_same_buffer_copy)

namespace TestGraphSameBufferCopy {

using RDG = RenderingDeviceGraph;
using RDD = RenderingDeviceDriver;

struct Driver : TransferTestDriverBase {
	bool require_transitions = false;
	BitField<BarrierAccessBits> pending_source = {};
	BitField<BarrierAccessBits> pending_destination = {};
	struct CopyObservation {
		BufferID source;
		BufferID destination;
		BufferCopyRegion region;
		BitField<BarrierAccessBits> source_access;
		BitField<BarrierAccessBits> destination_access;
	};
	CopyObservation copies[8];
	uint32_t copy_count = 0;

	void command_pipeline_barrier(CommandBufferID, BitField<PipelineStageBits>, BitField<PipelineStageBits>, VectorView<MemoryAccessBarrier> p_memory, VectorView<BufferBarrier>, VectorView<TextureBarrier>, VectorView<AccelerationStructureBarrier>) override {
		for (uint32_t i = 0; i < p_memory.size(); ++i) {
			pending_source.set_flag(p_memory[i].src_access);
			pending_destination.set_flag(p_memory[i].dst_access);
		}
	}

	void command_copy_buffer(CommandBufferID, BufferID p_source, BufferID p_destination, VectorView<BufferCopyRegion> p_regions) override {
		REQUIRE(copy_count < 8);
		REQUIRE(p_regions.size() == 1);
		copies[copy_count++] = { p_source, p_destination, p_regions[0], pending_source, pending_destination };
		pending_source.clear();
		pending_destination.clear();
	}

	uint64_t api_trait_get(ApiTrait p_trait) override {
		if (p_trait == API_TRAIT_HONORS_PIPELINE_BARRIERS) {
			return true;
		}
		if (p_trait == API_TRAIT_BUFFERS_REQUIRE_TRANSITIONS) {
			return require_transitions;
		}
		return TransferTestDriverBase::api_trait_get(p_trait);
	}
};

struct ErrorCapture {
	uint32_t count = 0;
	ErrorHandlerList handler;

	static void capture(void *p_user, const char *, const char *, int, const char *, const char *, bool, ErrorHandlerType p_type) {
		if (p_type == ERR_HANDLER_ERROR) {
			++static_cast<ErrorCapture *>(p_user)->count;
		}
	}

	ErrorCapture() {
		handler.errfunc = &capture;
		handler.userdata = this;
		add_error_handler(&handler);
	}

	~ErrorCapture() {
		remove_error_handler(&handler);
	}
};

struct Fixture {
	Driver driver;
	RDG graph;
	RDG::ResourceTracker buffer;
	RDG::ResourceTracker before_destination;
	RDG::ResourceTracker after_destination;

	explicit Fixture(bool p_require_transitions) {
		driver.require_transitions = p_require_transitions;
		graph.initialize(&driver, [](RDD *, VectorView<RDD::AttachmentLoadOp>, VectorView<RDD::AttachmentStoreOp>, void *) { return RDD::RenderPassID(); }, 1, RDD::CommandQueueFamilyID(1), 0);
		graph.begin();
		buffer.buffer_driver_id = RDD::BufferID(71);
		before_destination.buffer_driver_id = RDD::BufferID(72);
		after_destination.buffer_driver_id = RDD::BufferID(73);
	}

	RDG::RecordHandle write() {
		RDG::RecordedBufferCopy upload;
		upload.source = RDD::BufferID(74);
		upload.region = { 0, 0, 64 };
		return graph.add_buffer_update(buffer.buffer_driver_id, &buffer, VectorView(&upload, 1));
	}

	RDG::RecordHandle same_buffer_copy() {
		// Within a 64-byte buffer, [0, 16) and [32, 48) are disjoint.
		// One real buffer therefore supplies both the source and destination tracker.
		return graph.add_buffer_copy(buffer.buffer_driver_id, &buffer, buffer.buffer_driver_id, &buffer, { 0, 32, 16 });
	}

	RDG::RecordHandle read(RDG::ResourceTracker &p_destination) {
		return graph.add_buffer_copy(buffer.buffer_driver_id, &buffer, p_destination.buffer_driver_id, &p_destination, { 32, 0, 16 });
	}

	void check_path(const RDG::RecordHandle &p_before, const RDG::RecordHandle &p_after) const {
		uint32_t distances[8] = {};
		uint32_t pending[8] = {};
		uint32_t length = 0;
		const RDG::DependencyPathScratch scratch{ distances, pending, 8 };
		CHECK(graph.query_dependency_path(p_before, p_after, scratch, length) == RDG::DependencyPathResult::Found);
		CHECK(length > 0);
		CHECK(graph.query_dependency_path(p_after, p_before, scratch, length) == RDG::DependencyPathResult::NotFound);
	}
};

TEST_CASE("[R02][GraphSameBufferCopy] nonoverlapping copy retains both transfer accesses without self dependency") {
	for (bool require_transitions : { false, true }) {
		INFO("buffers require transitions: ", require_transitions);
		Fixture fixture(require_transitions);
		ErrorCapture errors;
		const auto copy = fixture.same_buffer_copy();
		CHECK(copy.is_valid());
		CHECK(fixture.graph.qualification_command_count() == 1);
		CHECK(errors.count == 0);
		// A dependency-only assertion misses the old partial append: its second
		// tracker visit reports a self edge after overwriting COPY_WRITE with READ.
		CHECK(fixture.buffer.usage_access.has_flag(RDD::BARRIER_ACCESS_COPY_READ_BIT));
		CHECK(fixture.buffer.usage_access.has_flag(RDD::BARRIER_ACCESS_COPY_WRITE_BIT));
		CHECK(fixture.buffer.write_command_or_list_index == int32_t(copy.index));
	}
}

TEST_CASE("[R02][GraphSameBufferCopy] same buffer copy orders prior and following readers and writers") {
	for (bool require_transitions : { false, true }) {
		INFO("buffers require transitions: ", require_transitions);
		Fixture fixture(require_transitions);
		ErrorCapture errors;
		const auto before_write = fixture.write();
		const auto before_read = fixture.read(fixture.before_destination);
		const auto copy = fixture.same_buffer_copy();
		CHECK(fixture.buffer.usage_access.has_flag(RDD::BARRIER_ACCESS_COPY_READ_BIT));
		CHECK(fixture.buffer.usage_access.has_flag(RDD::BARRIER_ACCESS_COPY_WRITE_BIT));
		const auto after_read = fixture.read(fixture.after_destination);
		const auto after_write = fixture.write();
		CHECK(errors.count == 0);
		CHECK(fixture.graph.qualification_command_count() == 5);
		fixture.check_path(before_write, before_read);
		fixture.check_path(before_read, copy);
		fixture.check_path(copy, after_read);
		fixture.check_path(after_read, after_write);
		fixture.check_path(before_write, after_write);
	}
}

TEST_CASE("[R02][GraphSameBufferCopy] emitted barriers preserve incoming and outgoing transfer hazards") {
	for (bool require_transitions : { false, true }) {
		INFO("buffers require transitions: ", require_transitions);
		Fixture fixture(require_transitions);
		ErrorCapture errors;
		fixture.write();
		fixture.same_buffer_copy();
		fixture.read(fixture.after_destination);
		fixture.write();
		RDD::CommandBufferID command_buffer(81);
		RDG::CommandBufferPool pool;
		// Keep one command per barrier group so adjacent commands cannot mask
		// missing access bits by merging their barriers. This emits mock calls only.
		fixture.graph.end(false, false, command_buffer, pool);
		CHECK(errors.count == 0);
		REQUIRE(fixture.driver.copy_count == 4);
		const auto &copy = fixture.driver.copies[1];
		CHECK(copy.source.id == fixture.buffer.buffer_driver_id.id);
		CHECK(copy.destination.id == fixture.buffer.buffer_driver_id.id);
		CHECK(copy.region.src_offset == 0);
		CHECK(copy.region.dst_offset == 32);
		CHECK(copy.region.size == 16);
		CHECK(copy.source_access.has_flag(RDD::BARRIER_ACCESS_COPY_WRITE_BIT));
		CHECK(copy.destination_access.has_flag(RDD::BARRIER_ACCESS_COPY_READ_BIT));
		CHECK(copy.destination_access.has_flag(RDD::BARRIER_ACCESS_COPY_WRITE_BIT));
		// The next read must see the same-buffer copy's write, even if it uses
		// the same COPY_FROM usage that used to overwrite the tracker.
		CHECK(fixture.driver.copies[2].source_access.has_flag(RDD::BARRIER_ACCESS_COPY_WRITE_BIT));
		CHECK(fixture.driver.copies[2].destination_access.has_flag(RDD::BARRIER_ACCESS_COPY_READ_BIT));
		CHECK(fixture.driver.copies[3].source_access.has_flag(RDD::BARRIER_ACCESS_COPY_READ_BIT));
		CHECK(fixture.driver.copies[3].destination_access.has_flag(RDD::BARRIER_ACCESS_COPY_WRITE_BIT));
	}
}

} // namespace TestGraphSameBufferCopy
