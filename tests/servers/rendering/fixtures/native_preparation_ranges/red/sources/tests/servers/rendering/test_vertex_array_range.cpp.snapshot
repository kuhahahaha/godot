/**************************************************************************/
/*  test_vertex_array_range.cpp                                           */
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

#include "core/os/thread.h"
#include "servers/rendering/rendering_device.h"
#include "tests/servers/rendering/transfer_test_driver_base.h"
#include "tests/test_macros.h"

TEST_FORCE_LINK(test_vertex_array_range)

// Real public factories with a controlled native allocation boundary. Pending
// transfer metadata is seeded on the factory-created buffer to check that
// admission does not consume it; this does not execute an upload or a GPU draw.
class TestVertexArrayRangeAccess {
	using RD = RenderingDevice;
	using RDD = RenderingDeviceDriver;
	using RDG = RenderingDeviceGraph;
	struct Driver : TransferTestDriverBase {
		uint32_t buffers_created = 0, formats_created = 0;
		BufferID buffer_create(uint64_t p_size, BitField<BufferUsageBits> p_usage, MemoryAllocationType p_allocation, uint64_t) override {
			CHECK(p_size == 32);
			CHECK(p_usage.has_flag(BUFFER_USAGE_VERTEX_BIT));
			CHECK(p_allocation == MEMORY_ALLOCATION_TYPE_GPU);
			++buffers_created;
			return BufferID(31);
		}
		void buffer_free(BufferID p_buffer) override { CHECK(p_buffer == BufferID(31)); }
		VertexFormatID vertex_format_create(Span<VertexAttribute> p_attributes, const VertexAttributeBindingsMap &) override {
			CHECK(p_attributes.size() == 1);
			++formats_created;
			return VertexFormatID(41);
		}
		void vertex_format_free(VertexFormatID p_format) override { CHECK(p_format == VertexFormatID(41)); }
		void set_object_name(ObjectType p_type, ID p_id, const String &) override {
			CHECK(p_type == OBJECT_TYPE_BUFFER);
			CHECK(p_id.id == 31);
		}
	};
	struct Fixture {
		RD device;
		Driver driver;
		Fixture() {
			device.driver = &driver;
			device.render_thread_id = Thread::get_caller_id();
			auto *worker = memnew(RD::TransferWorker);
			worker->index = 0;
			device.transfer_worker_pool.push_back(worker);
			device.transfer_worker_pool_size = 1;
			device.transfer_worker_operation_used_by_draw.resize(1);
			device.transfer_worker_operation_used_by_draw[0] = 0;
		}
		~Fixture() {
			// Release only this fixture's original owners and native allocations.
			for (RID array : device.vertex_array_owner.get_owned_list()) {
				device.vertex_array_owner.free(array);
			}
			device.dependency_map.clear();
			device.reverse_dependency_map.clear();
			for (RID rid : device.vertex_buffer_owner.get_owned_list()) {
				auto *buffer = device.vertex_buffer_owner.get_or_null(rid);
				RDG::resource_tracker_free(buffer->draw_tracker);
				driver.buffer_free(buffer->driver_id);
				device.vertex_buffer_owner.free(rid);
			}
			device.buffer_memory = 0;
			for (const auto &entry : device.vertex_formats) {
				driver.vertex_format_free(entry.value.driver_id);
			}
			device.vertex_formats.clear();
			device.vertex_format_cache.clear();
			memdelete(device.transfer_worker_pool[0]);
			device.transfer_worker_pool.clear();
			device.transfer_worker_pool_size = 0;
			device.driver = nullptr;
		}
	};

	static void check(uint64_t p_binding_offset, uint32_t p_attribute_offset, uint32_t p_stride, uint32_t p_vertices, bool p_instance, bool p_accept, uint32_t p_instances = UINT32_MAX, RD::DataFormat p_format = RD::DATA_FORMAT_R32_SFLOAT) {
		Fixture f;
		RD::VertexAttribute attribute;
		attribute.location = 0;
		attribute.binding = 0;
		attribute.format = p_format;
		attribute.offset = p_attribute_offset;
		attribute.stride = p_stride;
		attribute.frequency = p_instance ? RD::VERTEX_FREQUENCY_INSTANCE : RD::VERTEX_FREQUENCY_VERTEX;
		const auto format = f.device.vertex_format_create(Vector<RD::VertexAttribute>{ attribute });
		REQUIRE(format != uint64_t(RD::INVALID_ID));
		const RID buffer_rid = f.device.vertex_buffer_create(32);
		REQUIRE(buffer_rid.is_valid());
		auto *buffer = f.device.vertex_buffer_owner.get_or_null(buffer_rid);
		REQUIRE(buffer != nullptr);
		CHECK(buffer->size == 32);
		CHECK(buffer->draw_tracker != nullptr);
		buffer->transfer_worker_index = 0;
		buffer->transfer_worker_operation = 9;
		const auto arrays_before = f.device.vertex_array_owner.get_rid_count();
		const auto dependencies_before = f.device.dependency_map.size();
		const auto reverse_dependencies_before = f.device.reverse_dependency_map.size();
		ERR_PRINT_OFF;
		const RID array_rid = f.device.vertex_array_create(p_vertices, format, Vector<RID>{ buffer_rid }, Vector<uint64_t>{ p_binding_offset });
		ERR_PRINT_ON;
		CHECK(array_rid.is_valid() == p_accept);
		CHECK(buffer->transfer_worker_index == 0);
		CHECK(buffer->transfer_worker_operation == 9);
		CHECK(f.device.transfer_worker_operation_used_by_draw[0] == 0);
		CHECK(f.driver.buffers_created == 1);
		CHECK(f.driver.formats_created == 1);
		if (p_accept) {
			REQUIRE(array_rid.is_valid());
			const auto *array = f.device.vertex_array_owner.get_or_null(array_rid);
			REQUIRE(array != nullptr);
			CHECK(array->offsets[0] == p_binding_offset);
			CHECK(array->vertex_count == p_vertices);
			CHECK(array->max_instances_allowed == p_instances);
			CHECK(array->draw_trackers.size() == 1);
			CHECK(array->draw_trackers[0] == buffer->draw_tracker);
			CHECK(array->transfer_worker_indices.size() == 1);
			CHECK(array->transfer_worker_indices[0] == 0);
			CHECK(array->transfer_worker_operations[0] == 9);
		} else {
			CHECK(f.device.vertex_array_owner.get_rid_count() == arrays_before);
			CHECK(f.device.dependency_map.size() == dependencies_before);
			CHECK(f.device.reverse_dependency_map.size() == reverse_dependencies_before);
		}
	}

public:
	static void per_vertex() {
		SUBCASE("nonzero binding offset ends exactly at buffer end") { check(12, 0, 8, 3, false, true); }
		SUBCASE("binding and attribute offsets both contribute to exact end") { check(8, 4, 8, 3, false, true); }
		SUBCASE("last element crosses tail by one byte") { check(13, 0, 8, 3, false, false); }
		SUBCASE("binding offset exceeds the buffer") { check(33, 0, 4, 3, false, false); }
		SUBCASE("binding plus attribute offset overflows uint64") { check(UINT64_MAX - 3, 8, 4, 3, false, false); }
		SUBCASE("binding plus strided extent overflows uint64") { check(UINT64_MAX - 7, 0, 4, 3, false, false); }
		SUBCASE("large stride and count cannot wrap the range") { check(UINT64_MAX - 7, 0, UINT32_MAX, UINT32_MAX, false, false); }
		SUBCASE("zero stride repeats a fitting vertex element") { check(24, 4, 0, 3, false, true); }
	}
	static void per_instance() {
		// Zero vertex count is used by point-size emulation with all-instance formats.
		SUBCASE("all-instance zero vertex count remains valid at exact end") { check(24, 4, 8, 0, true, true, 1); }
		SUBCASE("base and attribute offsets reduce reachable instance count") { check(8, 4, 8, 0, true, true, 3); }
		SUBCASE("partial final stride admits its complete element") { check(4, 0, 8, 0, true, true, 4); }
		SUBCASE("overlapping aligned elements count only complete reads") { check(0, 0, 4, 0, true, true, 7, RD::DATA_FORMAT_R32G32_SFLOAT); }
		SUBCASE("first instance crosses the tail") { check(25, 4, 8, 0, true, false); }
		SUBCASE("attribute offset exceeds buffer without unsigned subtraction wrap") { check(0, 36, 8, 0, true, false); }
		SUBCASE("binding offset exceeds the buffer") { check(33, 0, 8, 0, true, false); }
		SUBCASE("instance attribute address addition overflows uint64") { check(UINT64_MAX - 3, 8, 8, 0, true, false); }
	}
	static void constant_instance() {
		SUBCASE("fitting constant instance element can repeat") { check(24, 4, 0, 0, true, true, UINT32_MAX); }
		SUBCASE("constant instance element still needs a complete element") { check(25, 4, 0, 0, true, false); }
	}
};

TEST_CASE("[RenderingDevice][VertexArrayRange] public factories include binding offsets in vertex ranges") {
	TestVertexArrayRangeAccess::per_vertex();
}
TEST_CASE("[RenderingDevice][VertexArrayRange] public factories retain reachable instance ranges") {
	TestVertexArrayRangeAccess::per_instance();
}
TEST_CASE("[RenderingDevice][VertexArrayRangeConstant] public factories support zero stride instance elements") {
	TestVertexArrayRangeAccess::constant_instance();
}
