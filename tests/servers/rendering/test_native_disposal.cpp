/**************************************************************************/
/*  test_native_disposal.cpp                                              */
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

#include "core/os/os.h"
#include "servers/rendering/rendering_device.h"
#include "servers/rendering/rendering_shader_container.h"
#include "tests/servers/rendering/transfer_test_driver_base.h"
#include "tests/test_macros.h"

TEST_FORCE_LINK(test_native_disposal)

class TestNativeDisposalAccess {
public:
	static void depth_disposal();
	static void depth_factory_failures();
	static void depth_identity();
	static void pipeline_rid_capacity();
	static void foreign_pending_owner_lookup();
	static void foreign_pending_uniform_sets();
	static void foreign_pending_export_and_callback();
	static void attach(RenderingDevice &p_rd, RenderingDeviceDriver &p_driver) {
		p_rd.driver = &p_driver;
		p_rd.is_main_instance = true;
		p_rd.render_thread_id = Thread::get_caller_id();
		p_rd.frames.resize(1);
	}
	static void detach(RenderingDevice &p_rd) {
		p_rd.frames.clear();
		p_rd.driver = nullptr;
	}
	static Error create(RenderingDevice &p_rd, RID &r_rid, NativeDisposalWitness &r_witness) {
		return p_rd._native_disposal_storage_create(128, {}, r_rid, r_witness);
	}
	static Error release(RenderingDevice &p_rd, const NativeDisposalWitness &p_witness) { return p_rd._native_disposal_free(p_witness); }
	static Error observe(RenderingDevice &p_rd, const NativeDisposalWitness &p_witness, NativeDisposalWitness::Report &r_report) { return p_rd._native_disposal_observe(p_witness, r_report); }
	static void dispose(RenderingDevice &p_rd) { p_rd._free_pending_resources(0); }
	static Error step(RenderingDevice &p_rd, uint32_t p_receipts, uint32_t p_passes, uint64_t p_deadline, uint32_t &r_receipts) {
		RenderingDevice::NativeDisposalBudget budget{ p_receipts, p_passes, p_deadline };
		RenderingDevice::NativeDisposalUsage usage;
		const Error error = p_rd._free_pending_resources_step(0, budget, usage);
		r_receipts = usage.polls + usage.frees + usage.issues + usage.retains + usage.releases + usage.recycles;
		CHECK(r_receipts <= p_receipts);
		CHECK(usage.passes <= p_passes);
		return error;
	}
	static bool draining(RenderingDevice &p_rd) { return p_rd.frames[0].disposal.active; }
	static void enqueue_shader(RenderingDevice &p_rd, RID p_shader) { p_rd.free_rid(p_shader); }
	static Error unconfigured_maintenance(RenderingDevice &p_rd) {
		p_rd.frames.clear();
		const Error result = p_rd._native_disposal_maintenance();
		p_rd.frames.resize(1);
		return result;
	}
	static void local(RenderingDevice &p_rd, bool p_local) { p_rd.is_main_instance = !p_local; }
	static void worker(RenderingDevice &p_rd, RID p_rid, bool p_active) { p_rd.storage_buffer_owner.get_or_null(p_rid)->transfer_worker_index = p_active ? 0 : -1; }
	static void poison(RenderingDevice &p_rd, RID p_rid) { ++p_rd.storage_buffer_owner.get_or_null(p_rid)->backing_serial; }
	static int refs(const NativeDisposalWitness &p_witness) { return p_witness.cell->get_reference_count(); }
	static NativeDisposalWitness::State state(const NativeDisposalWitness &p_witness) { return p_witness.cell->state; }
	static void repair_for_fixture_cleanup(RenderingDevice &p_rd, RID p_rid) {
		// Deliberate fixture-only undo; the production API cannot clear UNKNOWN.
		auto *buffer = p_rd.storage_buffer_owner.get_or_null(p_rid);
		--buffer->backing_serial;
		buffer->native_disposal.cell->state = NativeDisposalWitness::State::LIVE;
	}
	static RID shader(RenderingDevice &p_rd, RenderingDevice::UniformType p_type = RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER) {
		RenderingDevice::Shader shader;
		shader.driver_id = RenderingDeviceDriver::ShaderID(31);
		shader.uniform_sets.resize(1);
		RenderingDevice::ShaderUniform uniform;
		uniform.type = p_type;
		uniform.binding = 0;
		uniform.length = p_type == RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER ? 128 : 1;
		uniform.writable = true;
		shader.uniform_sets.write[0].push_back(uniform);
		shader.set_formats.push_back(1);
		return p_rd.shader_owner.make_rid(shader);
	}
	static void release_shader(RenderingDevice &p_rd, RID p_shader) {
		p_rd.free_rid(p_shader);
		dispose(p_rd);
		CHECK(p_rd.dependency_map.is_empty());
		CHECK(p_rd.reverse_dependency_map.is_empty());
	}
	static Error set(RenderingDevice &p_rd, RID p_buffer, RID p_shader, RID &r_set, NativeDisposalWitness &r_witness, bool p_unsupported = false) {
		RenderingDevice::Uniform uniform;
		uniform.uniform_type = p_unsupported ? RenderingDevice::UNIFORM_TYPE_UNIFORM_BUFFER : RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER;
		uniform.binding = 0;
		uniform.append_id(p_buffer);
		return p_rd._native_disposal_uniform_set_create(VectorView<RenderingDevice::Uniform>(&uniform, 1), p_shader, 0, r_set, r_witness);
	}
};

namespace TestNativeDisposal {
class Driver : public TransferTestDriverBase {
public:
	NativeDisposalWitness *buffer_witness = nullptr;
	NativeDisposalWitness *set_witness = nullptr;
	int creates = 0, frees = 0, set_creates = 0, set_frees = 0, shader_frees = 0;
	bool fail_create = false;
	bool unique_buffers = false;
	bool shader_ready = false;
	RenderingDevice *enqueue_rd = nullptr;
	NativeDisposalWitness *enqueue_witness = nullptr;
	uint32_t buffer_names = 0, set_names = 0;
	String last_buffer_name, last_set_name;
	BufferID buffer_create(uint64_t, BitField<BufferUsageBits>, MemoryAllocationType, uint64_t) override {
		++creates;
		return fail_create ? BufferID() : BufferID(unique_buffers ? 19 + creates : 19);
	}
	void set_object_name(ObjectType p_type, ID p_driver_id, const String &p_name) override {
		// Only the real DEV buffer/set naming calls belong to this fixture.
		CHECK(p_name.begins_with("RID:"));
		if (p_type == OBJECT_TYPE_BUFFER) {
			CHECK(creates > 0);
			CHECK_FALSE(fail_create);
			CHECK((unique_buffers ? (p_driver_id.id > 19 && p_driver_id.id <= uint64_t(19 + creates)) : p_driver_id.id == 19));
			++buffer_names;
			last_buffer_name = p_name;
		} else {
			REQUIRE(p_type == OBJECT_TYPE_UNIFORM_SET);
			CHECK(set_creates > 0);
			CHECK(p_driver_id.id == 23);
			++set_names;
			last_set_name = p_name;
		}
	}
	void buffer_free(BufferID p_id) override {
		CHECK((unique_buffers ? (p_id.id > 19 && p_id.id <= uint64_t(19 + creates)) : p_id.id == 19));
		if (buffer_witness) {
			CHECK(TestNativeDisposalAccess::state(*buffer_witness) == NativeDisposalWitness::State::ENQUEUED);
			CHECK(TestNativeDisposalAccess::refs(*buffer_witness) >= 2);
		}
		++frees;
		if (enqueue_witness) {
			auto *witness = enqueue_witness;
			enqueue_witness = nullptr;
			CHECK(TestNativeDisposalAccess::release(*enqueue_rd, *witness) == OK);
		}
	}
	UniformSetID uniform_set_create(VectorView<BoundUniform>, ShaderID, uint32_t, int p_pool) override {
		CHECK(p_pool == -1);
		++set_creates;
		return UniformSetID(23);
	}
	void uniform_set_free(UniformSetID) override {
		if (set_witness) {
			CHECK(TestNativeDisposalAccess::state(*set_witness) == NativeDisposalWitness::State::ENQUEUED);
			CHECK(TestNativeDisposalAccess::refs(*set_witness) >= 2);
		}
		++set_frees;
	}
	void sampler_free(SamplerID p_id) override { CHECK(p_id.id == 17); }
	uint64_t get_resource_native_handle(DriverResource p_type, ID p_id) override {
		CHECK(p_type == DRIVER_RESOURCE_BUFFER);
		CHECK(p_id.id == 19);
		return 919;
	}
	uint64_t api_trait_get(ApiTrait p_trait) override { return p_trait == API_TRAIT_HONORS_PIPELINE_BARRIERS ? 1 : 0; }
	bool shader_retire_ready_nonblocking(ShaderID) const override { return shader_ready; }
	void shader_free(ShaderID p_id) override {
		CHECK(p_id.id == 31);
		++shader_frees;
	}
};

// CPU driver-return fixture. It exercises real RD factories and disposal
// queues; cache children model driver-created variants, never GPU completion.
class DepthDisposalDriver : public TransferTestDriverBase {
public:
	NativeDisposalWitness *texture_witness = nullptr;
	NativeDisposalWitness *framebuffer_witness = nullptr;
	bool fail_texture = false, fail_pass = false;
	uint32_t texture_creates = 0, texture_frees = 0, framebuffer_creates = 0, framebuffer_frees = 0, pass_creates = 0, pass_frees = 0;
	HashSet<uint64_t> freed_framebuffers, freed_passes;
	TextureID texture_create(const TextureFormat &format, const TextureView &view) override {
		++texture_creates;
		CHECK(format.format == DATA_FORMAT_D32_SFLOAT);
		CHECK(view.format == DATA_FORMAT_D32_SFLOAT);
		CHECK(format.texture_type == TEXTURE_TYPE_2D);
		CHECK(format.width == 8);
		CHECK(format.height == 4);
		CHECK(format.samples == TEXTURE_SAMPLES_1);
		CHECK(format.usage_bits == (TEXTURE_USAGE_SAMPLING_BIT | TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT));
		return fail_texture ? TextureID() : TextureID(51);
	}
	TextureID texture_create_shared(TextureID original, const TextureView &) override {
		CHECK(original == TextureID(51));
		return TextureID(52);
	}
	void texture_free(TextureID id) override {
		CHECK((id == TextureID(51) || id == TextureID(52)));
		if (texture_witness && id == TextureID(51)) {
			CHECK(TestNativeDisposalAccess::state(*texture_witness) == NativeDisposalWitness::State::ENQUEUED);
			CHECK(TestNativeDisposalAccess::refs(*texture_witness) >= 2);
		}
		if (framebuffer_witness && id == TextureID(51)) {
			CHECK(TestNativeDisposalAccess::state(*framebuffer_witness) == NativeDisposalWitness::State::RETURNED);
			CHECK(framebuffer_frees == framebuffer_creates);
			// The device-wide format render pass is deliberately still alive.
			CHECK(pass_frees + 1 == pass_creates);
		}
		++texture_frees;
	}
	uint64_t texture_get_allocation_size(TextureID id) override { return id == TextureID(51) ? 128 : 0; }
	BitField<TextureUsageBits> texture_get_usages_supported_by_format(DataFormat format, bool readable) override {
		CHECK(format == DATA_FORMAT_D32_SFLOAT);
		CHECK_FALSE(readable);
		return TEXTURE_USAGE_SAMPLING_BIT | TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	}
	uint64_t limit_get(Limit) override { return 8192; }
	uint64_t api_trait_get(ApiTrait trait) override { return trait == API_TRAIT_HONORS_PIPELINE_BARRIERS ? 1 : 0; }
	void set_object_name(ObjectType type, ID id, const String &name) override {
		CHECK(type == OBJECT_TYPE_TEXTURE);
		CHECK((id.id == 51 || id.id == 52));
		CHECK(name.begins_with("RID:"));
	}
	RenderPassID render_pass_create(VectorView<Attachment>, VectorView<Subpass>, VectorView<SubpassDependency>, uint32_t, AttachmentReference) override {
		++pass_creates;
		return fail_pass ? RenderPassID() : RenderPassID(100 + pass_creates);
	}
	FramebufferID framebuffer_create(RenderPassID pass, VectorView<TextureID> textures, uint32_t width, uint32_t height) override {
		CHECK(pass.id > 101);
		CHECK(textures.size() == 1);
		CHECK(textures[0] == TextureID(51));
		CHECK(width == 8);
		CHECK(height == 4);
		return FramebufferID(200 + ++framebuffer_creates);
	}
	void framebuffer_free(FramebufferID id) override {
		CHECK(id.id > 200);
		CHECK_FALSE(freed_framebuffers.has(id.id));
		freed_framebuffers.insert(id.id);
		if (framebuffer_witness) {
			CHECK(TestNativeDisposalAccess::state(*framebuffer_witness) == NativeDisposalWitness::State::ENQUEUED);
			CHECK(TestNativeDisposalAccess::refs(*framebuffer_witness) >= 2);
		}
		++framebuffer_frees;
	}
	void render_pass_free(RenderPassID id) override {
		CHECK_FALSE(freed_passes.has(id.id));
		freed_passes.insert(id.id);
		if (framebuffer_witness && id.id != 101) {
			CHECK(TestNativeDisposalAccess::state(*framebuffer_witness) == NativeDisposalWitness::State::ENQUEUED);
		}
		++pass_frees;
	}
};

TEST_CASE("[RenderingDevice][NativeDisposal][D32] exact framebuffer cache children precede texture return") {
	TestNativeDisposalAccess::depth_disposal();
}
TEST_CASE("[RenderingDevice][NativeDisposal][D32] failed factories preserve original rollback ownership") {
	TestNativeDisposalAccess::depth_factory_failures();
}
TEST_CASE("[RenderingDevice][NativeDisposal][D32] foreign stale and copied view cannot borrow a disposal witness") {
	TestNativeDisposalAccess::depth_identity();
}

// Real public shader/pipeline factories with deterministic CPU driver objects.
// These objects carry no GPU execution or source-admission claim.
class RasterContainer : public RenderingShaderContainer {
protected:
	uint32_t _format() const override { return 0x52504350; }
	uint32_t _format_version() const override { return 1; }
	bool _set_code_from_spirv(const ReflectShader &) override { return false; }

public:
	void initialize() {
		reflection_data.pipeline_type = RenderingDeviceCommons::PIPELINE_TYPE_RASTERIZATION;
		reflection_data.stage_count = 2;
		for (auto stage : { RenderingDeviceCommons::SHADER_STAGE_VERTEX, RenderingDeviceCommons::SHADER_STAGE_FRAGMENT }) {
			reflection_shader_stages.push_back(stage);
			Shader shader;
			shader.shader_stage = stage;
			shader.code_compressed_bytes.push_back(42);
			shader.code_decompressed_size = 1;
			shaders.push_back(shader);
		}
	}
};
class RasterFormat : public RenderingShaderContainerFormat {
public:
	Ref<RenderingShaderContainer> create_container() const override { return memnew(RasterContainer); }
	ShaderLanguageVersion get_shader_language_version() const override { return SHADER_LANGUAGE_VULKAN_VERSION_1_1; }
	ShaderSpirvVersion get_shader_spirv_version() const override { return SHADER_SPIRV_VERSION_1_3; }
};
class RasterCapacityDriver : public TransferTestDriverBase {
public:
	RasterFormat format;
	bool fail_pipeline = false, quiet_failure = false, cleanup_orphan = false;
	uint32_t pipeline_calls = 0, pipeline_frees = 0, orphan_cleanup_frees = 0, shader_frees = 0;
	uint64_t next_pipeline = 400;
	PipelineID live_pipeline;
	const RenderingShaderContainerFormat &get_shader_container_format() const override { return format; }
	ShaderID shader_create_from_container(const Ref<RenderingShaderContainer> &container, const Vector<ImmutableSampler> &) override {
		CHECK(container->get_shader_reflection().pipeline_type == PIPELINE_TYPE_RASTERIZATION);
		return ShaderID(301);
	}
	uint32_t shader_get_layout_hash(ShaderID id) override {
		CHECK(id == ShaderID(301));
		return 123;
	}
	void shader_free(ShaderID id) override {
		CHECK(id == ShaderID(301));
		++shader_frees;
	}
	bool shader_retire_ready_nonblocking(ShaderID) const override { return true; }
	RenderPassID render_pass_create(VectorView<Attachment>, VectorView<Subpass>, VectorView<SubpassDependency>, uint32_t, AttachmentReference) override { return RenderPassID(302); }
	void render_pass_free(RenderPassID id) override { CHECK(id == RenderPassID(302)); }
	void set_object_name(ObjectType type, ID id, const String &name) override {
		CHECK(name.begins_with("RID:"));
		CHECK(((type == OBJECT_TYPE_SHADER && id.id == 301) || (type == OBJECT_TYPE_PIPELINE && live_pipeline && id.id == live_pipeline.id)));
	}
	DriverWorkarounds get_driver_workarounds() const override {
		DriverWorkarounds value;
		value.dont_print_on_render_pipeline_creation_failure = quiet_failure;
		return value;
	}
	PipelineID render_pipeline_create(ShaderID shader, VertexFormatID, RenderPrimitive, PipelineRasterizationState, PipelineMultisampleState, PipelineDepthStencilState, PipelineColorBlendState, VectorView<int32_t>, BitField<PipelineDynamicStateFlags>, RenderPassID pass, uint32_t, VectorView<PipelineSpecializationConstant>) override {
		CHECK(shader == ShaderID(301));
		CHECK(pass == RenderPassID(302));
		++pipeline_calls;
		if (fail_pipeline) {
			return {};
		}
		CHECK_FALSE(bool(live_pipeline));
		live_pipeline = PipelineID(next_pipeline++);
		return live_pipeline;
	}
	void pipeline_free(PipelineID id) override {
		CHECK(bool(live_pipeline));
		CHECK(id == live_pipeline);
		live_pipeline = {};
		if (cleanup_orphan) {
			++orphan_cleanup_frees;
		} else {
			++pipeline_frees;
		}
	}
};

TEST_CASE("[RenderingDevice][NativeDisposal][PipelineRIDCapacity] ordinary factory retains a real RID before native creation") {
	ERR_PRINT_OFF;
	TestNativeDisposalAccess::pipeline_rid_capacity();
	ERR_PRINT_ON;
}

TEST_CASE("[RenderingDevice][NativeDisposal] large accepted queue drains with single-call turns") {
	RenderingDevice rd;
	Driver driver;
	driver.unique_buffers = true;
	TestNativeDisposalAccess::attach(rd, driver);
	NativeDisposalWitness witnesses[257];
	for (auto &witness : witnesses) {
		RID rid;
		REQUIRE(TestNativeDisposalAccess::create(rd, rid, witness) == OK);
		REQUIRE(TestNativeDisposalAccess::release(rd, witness) == OK);
	}
	uint32_t used = 99;
	CHECK(TestNativeDisposalAccess::step(rd, 0, 0, 0, used) == ERR_BUSY);
	CHECK(used == 0);
	CHECK_FALSE(TestNativeDisposalAccess::draining(rd));
	CHECK(TestNativeDisposalAccess::step(rd, 1, 1, OS::get_singleton()->get_ticks_usec(), used) == ERR_BUSY);
	CHECK(driver.frees == 0);
	CHECK_FALSE(TestNativeDisposalAccess::draining(rd));
	for (uint32_t i = 0; i < 257; ++i) {
		const Error error = TestNativeDisposalAccess::step(rd, 1, 1, UINT64_MAX, used);
		CHECK(error == (i == 256 ? OK : ERR_BUSY));
		CHECK(used == 1);
		CHECK(driver.frees == int(i + 1));
		CHECK(TestNativeDisposalAccess::state(witnesses[i]) == NativeDisposalWitness::State::RETURNED);
		if (i + 1 < 257) {
			CHECK(TestNativeDisposalAccess::state(witnesses[i + 1]) == NativeDisposalWitness::State::ENQUEUED);
		}
	}
	CHECK_FALSE(TestNativeDisposalAccess::draining(rd));
	TestNativeDisposalAccess::detach(rd);
}

TEST_CASE("[RenderingDevice][NativeDisposal] reentrant enqueue stays beyond the original cutoff") {
	RenderingDevice rd;
	Driver driver;
	driver.unique_buffers = true;
	TestNativeDisposalAccess::attach(rd, driver);
	NativeDisposalWitness witnesses[4];
	for (uint32_t i = 0; i < 4; ++i) {
		RID rid;
		REQUIRE(TestNativeDisposalAccess::create(rd, rid, witnesses[i]) == OK);
		if (i < 3) {
			REQUIRE(TestNativeDisposalAccess::release(rd, witnesses[i]) == OK);
		}
	}
	driver.enqueue_rd = &rd;
	driver.enqueue_witness = &witnesses[3];
	uint32_t used = 0;
	CHECK(TestNativeDisposalAccess::step(rd, 8, 8, UINT64_MAX, used) == OK);
	CHECK(used == 3);
	CHECK(driver.frees == 3);
	CHECK(TestNativeDisposalAccess::state(witnesses[3]) == NativeDisposalWitness::State::ENQUEUED);
	CHECK(TestNativeDisposalAccess::step(rd, 1, 1, UINT64_MAX, used) == OK);
	CHECK(used == 1);
	CHECK(driver.frees == 4);
	CHECK(TestNativeDisposalAccess::state(witnesses[3]) == NativeDisposalWitness::State::RETURNED);
	TestNativeDisposalAccess::detach(rd);
}

TEST_CASE("[RenderingDevice][NativeDisposal] pending shader readiness never signs a physical return") {
	RenderingDevice rd;
	Driver driver;
	TestNativeDisposalAccess::attach(rd, driver);
	TestNativeDisposalAccess::enqueue_shader(rd, TestNativeDisposalAccess::shader(rd));
	uint32_t used = 0;
	CHECK(TestNativeDisposalAccess::step(rd, 1, 1, UINT64_MAX, used) == ERR_BUSY);
	CHECK(used == 1);
	CHECK(driver.shader_frees == 0);
	driver.shader_ready = true;
	CHECK(TestNativeDisposalAccess::step(rd, 1, 1, UINT64_MAX, used) == ERR_BUSY);
	CHECK(used == 1);
	CHECK(driver.shader_frees == 0);
	CHECK(TestNativeDisposalAccess::step(rd, 1, 1, UINT64_MAX, used) == OK);
	CHECK(used == 1);
	CHECK(driver.shader_frees == 1);
	TestNativeDisposalAccess::detach(rd);
}

TEST_CASE("[RenderingDevice][NativeDisposal] real buffer queue retains metadata until driver free returns") {
	RenderingDevice rd;
	Driver driver;
	TestNativeDisposalAccess::attach(rd, driver);
	NativeDisposalWitness witness;
	RID rid;
	REQUIRE(TestNativeDisposalAccess::create(rd, rid, witness) == OK);
#ifdef DEV_ENABLED
	CHECK(driver.buffer_names == 1);
	CHECK(driver.set_names == 0);
	CHECK(driver.last_buffer_name == "RID:" + itos(rid.get_id()));
#else
	CHECK(driver.buffer_names == 0);
	CHECK(driver.set_names == 0);
#endif
	driver.buffer_witness = &witness;
	NativeDisposalWitness::Report report;
	CHECK(TestNativeDisposalAccess::observe(rd, witness, report) == OK);
	CHECK(report.state == NativeDisposalWitness::State::LIVE);
	CHECK(report.rid == rid.get_id());
	CHECK(report.serial != 0);
	CHECK(TestNativeDisposalAccess::release(rd, witness) == OK);
	CHECK(TestNativeDisposalAccess::state(witness) == NativeDisposalWitness::State::ENQUEUED);
	CHECK(driver.frees == 0);
	CHECK(TestNativeDisposalAccess::release(rd, witness) == OK);
	TestNativeDisposalAccess::dispose(rd);
	CHECK(driver.frees == 1);
	CHECK(TestNativeDisposalAccess::state(witness) == NativeDisposalWitness::State::RETURNED);
	CHECK(TestNativeDisposalAccess::refs(witness) == 1);
	TestNativeDisposalAccess::detach(rd);
}

TEST_CASE("[RenderingDevice][NativeDisposal] set cascade reports each actual free independently") {
	RenderingDevice rd;
	Driver driver;
	TestNativeDisposalAccess::attach(rd, driver);
	NativeDisposalWitness buffer, set;
	RID rid, set_rid;
	REQUIRE(TestNativeDisposalAccess::create(rd, rid, buffer) == OK);
	const RID shader = TestNativeDisposalAccess::shader(rd);
	CHECK(TestNativeDisposalAccess::set(rd, rid, shader, set_rid, set, true) == ERR_UNAVAILABLE);
	CHECK(driver.set_creates == 0);
	TestNativeDisposalAccess::worker(rd, rid, true);
	CHECK(TestNativeDisposalAccess::set(rd, rid, shader, set_rid, set) == ERR_UNAVAILABLE);
	CHECK(driver.set_creates == 0);
	TestNativeDisposalAccess::worker(rd, rid, false);
	REQUIRE(TestNativeDisposalAccess::set(rd, rid, shader, set_rid, set) == OK);
#ifdef DEV_ENABLED
	CHECK(driver.buffer_names == 1);
	CHECK(driver.set_names == 1);
	CHECK(driver.last_buffer_name == "RID:" + itos(rid.get_id()));
	CHECK(driver.last_set_name == "RID:" + itos(set_rid.get_id()));
#else
	CHECK(driver.buffer_names == 0);
	CHECK(driver.set_names == 0);
#endif
	driver.buffer_witness = &buffer;
	driver.set_witness = &set;
	CHECK(TestNativeDisposalAccess::release(rd, buffer) == OK);
	CHECK(TestNativeDisposalAccess::state(set) == NativeDisposalWitness::State::ENQUEUED);
	CHECK(TestNativeDisposalAccess::state(buffer) == NativeDisposalWitness::State::ENQUEUED);
	CHECK(driver.frees == 0);
	CHECK(driver.set_frees == 0);
	TestNativeDisposalAccess::dispose(rd);
	CHECK(driver.set_frees == 1);
	CHECK(driver.frees == 1);
	CHECK(TestNativeDisposalAccess::state(set) == NativeDisposalWitness::State::RETURNED);
	CHECK(TestNativeDisposalAccess::state(buffer) == NativeDisposalWitness::State::RETURNED);
	CHECK(driver.shader_frees == 0);
	TestNativeDisposalAccess::release_shader(rd, shader);
	CHECK(driver.shader_frees == 1);
	TestNativeDisposalAccess::detach(rd);
}

TEST_CASE("[RenderingDevice][NativeDisposal] preflight and serial mismatch cannot manufacture returned") {
	RenderingDevice rd;
	Driver driver;
	TestNativeDisposalAccess::attach(rd, driver);
	NativeDisposalWitness witness;
	RID rid;
	CHECK(TestNativeDisposalAccess::unconfigured_maintenance(rd) == ERR_UNCONFIGURED);
	TestNativeDisposalAccess::local(rd, true);
	CHECK(TestNativeDisposalAccess::create(rd, rid, witness) == ERR_UNAUTHORIZED);
	CHECK(driver.creates == 0);
	TestNativeDisposalAccess::local(rd, false);
	REQUIRE(TestNativeDisposalAccess::create(rd, rid, witness) == OK);
	TestNativeDisposalAccess::poison(rd, rid);
	CHECK(TestNativeDisposalAccess::release(rd, witness) == ERR_INVALID_DATA);
	CHECK(TestNativeDisposalAccess::state(witness) == NativeDisposalWitness::State::UNKNOWN);
	CHECK(driver.frees == 0);
	CHECK(TestNativeDisposalAccess::release(rd, witness) == ERR_INVALID_DATA);
	TestNativeDisposalAccess::repair_for_fixture_cleanup(rd, rid);
	CHECK(TestNativeDisposalAccess::release(rd, witness) == OK);
	TestNativeDisposalAccess::dispose(rd);
	TestNativeDisposalAccess::detach(rd);
}
TEST_CASE("[RenderingDevice][NativeDisposal] failed native creation leaves no witness or hidden pending resource") {
	RenderingDevice rd;
	Driver driver;
	TestNativeDisposalAccess::attach(rd, driver);
	NativeDisposalWitness witness;
	RID rid;
	driver.fail_create = true;
	ERR_PRINT_OFF;
	const Error failed = TestNativeDisposalAccess::create(rd, rid, witness);
	ERR_PRINT_ON;
	CHECK(failed == ERR_CANT_CREATE);
	CHECK(rid.is_null());
	CHECK(driver.creates == 1);
	CHECK(driver.frees == 0);
	NativeDisposalWitness::Report report;
	CHECK(TestNativeDisposalAccess::observe(rd, witness, report) == ERR_UNAUTHORIZED);
	driver.fail_create = false;
	REQUIRE(TestNativeDisposalAccess::create(rd, rid, witness) == OK);
	CHECK(TestNativeDisposalAccess::release(rd, witness) == OK);
	TestNativeDisposalAccess::dispose(rd);
	CHECK(driver.frees == 1);
	TestNativeDisposalAccess::detach(rd);
}

} // namespace TestNativeDisposal

void TestNativeDisposalAccess::pipeline_rid_capacity() {
	using namespace TestNativeDisposal;
	RenderingDevice rd;
	RasterCapacityDriver driver;
	attach(rd, driver);
	Ref<RasterContainer> container;
	container.instantiate();
	container->initialize();
	const RID shader = rd.shader_create_from_bytecode(container->to_bytes());
	CHECK(shader.is_valid());
	if (shader.is_null()) {
		detach(rd);
		return;
	}
	const auto format = rd.framebuffer_format_create(Vector<RenderingDevice::AttachmentFormat>());
	auto create_pipeline = [&]() {
		return rd.render_pipeline_create(shader, format, RenderingDevice::INVALID_ID, RenderingDevice::RENDER_PRIMITIVE_TRIANGLES,
				RenderingDevice::PipelineRasterizationState(), RenderingDevice::PipelineMultisampleState(), RenderingDevice::PipelineDepthStencilState(), RenderingDevice::PipelineColorBlendState());
	};

	// Exercise the unchanged default thread-safe RID_Owner constructor. Its
	// last chunk is rounded UP, so 262144 is not necessarily the real capacity.
	constexpr uint32_t chunk_elements = sizeof(RenderingDevice::RenderPipeline) > 65536 ? 1 : 65536 / sizeof(RenderingDevice::RenderPipeline);
	constexpr uint32_t arena_capacity = (262144 / chunk_elements + 1) * chunk_elements;
	INFO("Default pipeline arena slots=", arena_capacity, ", sizeof pipeline=", sizeof(RenderingDevice::RenderPipeline));
	LocalVector<RID> fillers;
	for (uint32_t i = 0; i < arena_capacity; ++i) {
		const RID rid = rd.render_pipeline_owner.make_rid(RenderingDevice::RenderPipeline());
		if (rid.is_null()) {
			break;
		}
		fillers.push_back(rid);
	}
	CHECK(fillers.size() == arena_capacity);
	CHECK(rd.render_pipeline_owner.get_rid_count() == arena_capacity);
	const RID beyond = rd.render_pipeline_owner.allocate_rid();
	CHECK(beyond.is_null());
	if (beyond.is_valid()) {
		// A changed constructor contract is a failed test, not a leaked slot.
		rd.render_pipeline_owner.initialize_rid(beyond, RenderingDevice::RenderPipeline());
		fillers.push_back(beyond);
	}
	const uint32_t before_full = driver.pipeline_calls;
	const RID refused = create_pipeline();
	CHECK(refused.is_null());
	// RED: original code calls the driver once and then loses the RID root.
	// GREEN: actual RID reservation refuses before any driver allocation.
	CHECK_MESSAGE(driver.pipeline_calls == before_full, "Full actual RID owner must refuse before native pipeline creation");
	CHECK(rd.render_pipeline_owner.get_rid_count() == fillers.size());
	CHECK(rd.frames[0].render_pipelines_to_dispose_of.is_empty());
	if (refused.is_valid()) {
		rd.free_rid(refused);
		dispose(rd);
	}
	if (driver.live_pipeline) {
		// Retain the exact failed-prefix object in the CPU backend solely for
		// negative-fixture teardown. Do not fabricate a replacement RID/receipt.
		CHECK(refused.is_null());
		CHECK(driver.pipeline_calls == before_full + 1);
		bool rooted = false;
		for (RID rid : fillers) {
			const auto *pipeline = rd.render_pipeline_owner.get_or_null(rid);
			rooted |= pipeline && pipeline->driver_id == driver.live_pipeline;
		}
		CHECK_FALSE(rooted);
		driver.cleanup_orphan = true;
		driver.pipeline_free(driver.live_pipeline);
		driver.cleanup_orphan = false;
		// RED also inserts an empty-RID dependency after make_rid fails.
		// Remove that exact failed edge through the original dependency cleanup;
		// this is test teardown after assertions, never successful factory work.
		MutexLock lock(rd._thread_safe_);
		CHECK(rd._free_dependencies(RID(), lock));
	}
	CHECK_FALSE(bool(driver.live_pipeline));
	const uint32_t frees_before = driver.pipeline_frees;

	if (!fillers.is_empty()) {
		const RID last = fillers[fillers.size() - 1];
		rd.render_pipeline_owner.free(last);
		fillers.resize(fillers.size() - 1);
		CHECK(rd.render_pipeline_owner.get_rid_count() == arena_capacity - 1);
		driver.fail_pipeline = true;
		for (uint32_t quiet = 0; quiet < 2; ++quiet) {
			driver.quiet_failure = quiet != 0;
			for (uint32_t attempt = 0; attempt < 3; ++attempt) {
				const uint32_t calls = driver.pipeline_calls;
				CHECK(create_pipeline().is_null());
				CHECK(driver.pipeline_calls == calls + 1);
				CHECK(rd.render_pipeline_owner.get_rid_count() == arena_capacity - 1);
				CHECK_FALSE(bool(driver.live_pipeline));
				CHECK(driver.pipeline_frees == frees_before);
				CHECK(rd.frames[0].render_pipelines_to_dispose_of.is_empty());
			}
		}
		driver.fail_pipeline = driver.quiet_failure = false;
		const uint32_t calls = driver.pipeline_calls;
		const RID pipeline = create_pipeline();
		CHECK(pipeline.is_valid());
		CHECK(driver.pipeline_calls == calls + 1);
		CHECK(rd.render_pipeline_owner.get_rid_count() == arena_capacity);
		if (pipeline.is_valid()) {
			const auto *actual = rd.render_pipeline_owner.get_or_null(pipeline);
			CHECK(actual != nullptr);
			if (actual) {
				CHECK(actual->driver_id == driver.live_pipeline);
				CHECK(actual->shader == shader);
			}
			CHECK(rd.render_pipeline_is_valid(pipeline));
			// The only available slot was reused by the real public factory.
			CHECK(uint32_t(pipeline.get_id()) == uint32_t(last.get_id()));
			rd.free_rid(pipeline);
			CHECK_FALSE(rd.render_pipeline_owner.owns(pipeline));
			CHECK(rd.frames[0].render_pipelines_to_dispose_of.size() == 1);
			CHECK(driver.pipeline_frees == frees_before);
			dispose(rd); // Actual normal pending-free path, not orphan teardown.
			CHECK(driver.pipeline_frees == frees_before + 1);
			CHECK_FALSE(bool(driver.live_pipeline));
			CHECK(rd.frames[0].render_pipelines_to_dispose_of.is_empty());
		}
	}
	for (RID rid : fillers) {
		rd.render_pipeline_owner.free(rid);
	}
	CHECK(rd.render_pipeline_owner.get_rid_count() == 0);
	rd.free_rid(shader);
	dispose(rd);
	CHECK(driver.shader_frees == 1);
	CHECK(rd.dependency_map.is_empty());
	CHECK(rd.reverse_dependency_map.is_empty());
	for (auto &entry : rd.framebuffer_formats) {
		driver.render_pass_free(entry.value.render_pass);
	}
	rd.framebuffer_formats.clear();
	rd.framebuffer_format_cache.clear();
	detach(rd);
}

void TestNativeDisposalAccess::foreign_pending_owner_lookup() {
	using namespace TestNativeDisposal;
	RenderingDevice rd;
	Driver driver;
	attach(rd, driver);
	struct Errors {
		uint32_t count = 0;
		static void capture(void *data, const char *, const char *, int, const char *, const char *, bool, ErrorHandlerType type) {
			if (type == ERR_HANDLER_ERROR) {
				++static_cast<Errors *>(data)->count;
			}
		}
	} errors;
	ErrorHandlerList handler{ &Errors::capture, &errors };
	add_error_handler(&handler);
	auto exercise = [&](auto &pending_owner, const auto &empty_resource) {
		// The pending foreign slot has the same table index as the live buffer.
		// Its validator belongs to another RID: lookup must not inspect it as ours.
		const RID pending = pending_owner.allocate_rid();
		const RID pending_child = pending_owner.allocate_rid();
		CHECK(pending.is_valid());
		CHECK(pending_child.is_valid());
		NativeDisposalWitness witness, child_witness;
		RID buffer, child;
		CHECK(create(rd, buffer, witness) == OK);
		CHECK(create(rd, child, child_witness) == OK);
		CHECK(uint32_t(pending_child.get_id()) == uint32_t(child.get_id()));
		rd._add_dependency(child, buffer);
		CHECK(uint32_t(pending.get_id()) == uint32_t(buffer.get_id()));
		CHECK_FALSE(pending_owner.owns(buffer));
		CHECK_FALSE(rd.native_immutable_resource_is_held(buffer));
		CHECK_FALSE(rd.native_immutable_resource_is_retiring(buffer));
		uint32_t kind;
		uint64_t serial, driver_id;
		CHECK(rd._production_disposal_head(buffer, kind, serial, driver_id) != nullptr);
		CHECK_FALSE(rd._qualification_resource_busy(buffer));
		// Acquire the child's real dependency closure, then retire the held parent.
		// Both slots must remain live until this original hold releases them.
		RenderingDevice::NativeImmutableHold hold;
		CHECK(rd.native_immutable_hold_acquire(VectorView<RID>(&child, 1), hold, 4) == OK);
		CHECK(rd.native_immutable_resource_is_held(buffer));
		CHECK(rd.native_immutable_resource_is_held(child));
		const int before_frees = driver.frees;
		CHECK(release(rd, witness) == ERR_BUSY);
		CHECK(rd.native_immutable_resource_is_retiring(buffer));
		CHECK(rd.native_immutable_resource_is_retiring(child));
		dispose(rd);
		CHECK(driver.frees == before_frees);
		CHECK(rd.native_immutable_hold_release(hold) == OK);
		dispose(rd);
		CHECK(driver.frees == before_frees + 2);
		CHECK(rd.dependency_map.is_empty());
		CHECK(rd.reverse_dependency_map.is_empty());
		CHECK(errors.count == 0);
		// Matching pending RIDs still belong to their owner; the init guard stays.
		ERR_PRINT_OFF;
		CHECK(pending_owner.get_or_null(pending) == nullptr);
		ERR_PRINT_ON;
		CHECK(errors.count == 1);
		errors.count = 0;
		pending_owner.initialize_rid(pending, empty_resource);
		pending_owner.free(pending);
		pending_owner.initialize_rid(pending_child, empty_resource);
		pending_owner.free(pending_child);
	};
	exercise(rd.shader_owner, RenderingDevice::Shader());
	exercise(rd.texture_owner, RenderingDevice::Texture());
	exercise(rd.vertex_buffer_owner, RenderingDevice::Buffer());
	exercise(rd.uniform_set_owner, RenderingDevice::UniformSet());
	remove_error_handler(&handler);
	CHECK(driver.creates == 8);
	CHECK(driver.frees == 8);
	detach(rd);
}

TEST_CASE("[RenderingDevice][NativeDisposal] pending foreign RID slots do not poison native retirement") {
	TestNativeDisposalAccess::foreign_pending_owner_lookup();
}

void TestNativeDisposalAccess::foreign_pending_uniform_sets() {
	using namespace TestNativeDisposal;
	for (bool sampler_binding : { false, true }) {
		RenderingDevice rd;
		Driver driver;
		attach(rd, driver);
		struct Errors {
			uint32_t count = 0;
			static void capture(void *data, const char *, const char *, int, const char *, const char *, bool, ErrorHandlerType type) {
				if (type == ERR_HANDLER_ERROR) {
					++static_cast<Errors *>(data)->count;
				}
			}
		} errors;
		ErrorHandlerList handler{ &Errors::capture, &errors };
		add_error_handler(&handler);

		const RID pending_texture = rd.texture_owner.allocate_rid();
		RID pending_sampler, pending_storage;
		RID resource;
		NativeDisposalWitness buffer_witness;
		if (sampler_binding) {
			pending_storage = rd.storage_buffer_owner.allocate_rid();
			RenderingDevice::Sampler sampler;
			sampler.driver_id = RenderingDeviceDriver::SamplerID(17);
			resource = rd.sampler_owner.make_rid(sampler);
			CHECK(uint32_t(pending_storage.get_id()) == uint32_t(resource.get_id()));
		} else {
			pending_sampler = rd.sampler_owner.allocate_rid();
			CHECK(create(rd, resource, buffer_witness) == OK);
			CHECK(uint32_t(pending_sampler.get_id()) == uint32_t(resource.get_id()));
		}
		CHECK(uint32_t(pending_texture.get_id()) == uint32_t(resource.get_id()));
		const auto type = sampler_binding ? RenderingDevice::UNIFORM_TYPE_SAMPLER : RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER;
		const RID shader_rid = shader(rd, type);
		RenderingDevice::Uniform uniform;
		uniform.uniform_type = type;
		uniform.binding = 0;
		uniform.append_id(resource);
		const RID set_rid = rd.uniform_set_create(VectorView<RenderingDevice::Uniform>(&uniform, 1), shader_rid, 0);
		CHECK(set_rid.is_valid());
		CHECK(driver.set_creates == 1);
		CHECK(errors.count == 0);
		rd.free_rid(set_rid);
		dispose(rd);
		CHECK(driver.set_frees == 1);
		rd.free_rid(resource);
		dispose(rd);
		CHECK(errors.count == 0);

		// A genuine witnessed set can also share its index with pending storage.
		if (!sampler_binding) {
			RID buffer;
			NativeDisposalWitness buffer_receipt, set_receipt;
			CHECK(create(rd, buffer, buffer_receipt) == OK);
			const RID set_filler = rd.uniform_set_owner.allocate_rid();
			pending_storage = rd.storage_buffer_owner.allocate_rid();
			RID witnessed_set;
			CHECK(set(rd, buffer, shader_rid, witnessed_set, set_receipt) == OK);
			CHECK(uint32_t(pending_storage.get_id()) == uint32_t(witnessed_set.get_id()));
			driver.set_witness = &set_receipt;
			CHECK(release(rd, set_receipt) == OK);
			dispose(rd);
			CHECK(state(set_receipt) == NativeDisposalWitness::State::RETURNED);
			CHECK(driver.set_frees == 2);
			CHECK(errors.count == 0);
			driver.set_witness = nullptr;
			CHECK(release(rd, buffer_receipt) == OK);
			dispose(rd);
			rd.uniform_set_owner.initialize_rid(set_filler, RenderingDevice::UniformSet());
			rd.uniform_set_owner.free(set_filler);
		}
		release_shader(rd, shader_rid);
		rd.texture_owner.initialize_rid(pending_texture, RenderingDevice::Texture());
		rd.texture_owner.free(pending_texture);
		if (pending_sampler.is_valid()) {
			rd.sampler_owner.initialize_rid(pending_sampler, RenderingDevice::Sampler());
			rd.sampler_owner.free(pending_sampler);
		}
		rd.storage_buffer_owner.initialize_rid(pending_storage, RenderingDevice::Buffer());
		rd.storage_buffer_owner.free(pending_storage);
		CHECK(errors.count == 0);
		remove_error_handler(&handler);
		detach(rd);
	}
}

TEST_CASE("[RenderingDevice][NativeDisposal] uniform creation and witnessed set release ignore foreign pending slots") {
	TestNativeDisposalAccess::foreign_pending_uniform_sets();
}

void TestNativeDisposalAccess::foreign_pending_export_and_callback() {
	using namespace TestNativeDisposal;
	RenderingDevice rd;
	Driver driver;
	attach(rd, driver);
	struct Errors {
		uint32_t count = 0;
		static void capture(void *data, const char *, const char *, int, const char *, const char *, bool, ErrorHandlerType type) {
			if (type == ERR_HANDLER_ERROR) {
				++static_cast<Errors *>(data)->count;
			}
		}
	} errors;
	ErrorHandlerList handler{ &Errors::capture, &errors };
	add_error_handler(&handler);
	const RID pending_texture = rd.texture_owner.allocate_rid();
	const RID pending_storage = rd.storage_buffer_owner.allocate_rid();
	RenderingDevice::Buffer buffer;
	buffer.driver_id = RenderingDeviceDriver::BufferID(19);
	buffer.size = 128;
	buffer.usage = RenderingDeviceDriver::BUFFER_USAGE_VERTEX_BIT;
	const RID vertex = rd.vertex_buffer_owner.make_rid(buffer);
	CHECK(uint32_t(vertex.get_id()) == uint32_t(pending_texture.get_id()));
	CHECK(uint32_t(vertex.get_id()) == uint32_t(pending_storage.get_id()));
	CHECK(rd.get_driver_resource(RenderingDevice::DRIVER_RESOURCE_BUFFER, vertex) == 919);
	CHECK(rd.vertex_buffer_owner.get_or_null(vertex)->native_external_access);
	CHECK(errors.count == 0);

	rd.draw_graph.initialize(&driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
	rd.draw_graph.begin();
	RenderingDevice::CallbackResource callback;
	callback.type = RenderingDevice::CALLBACK_RESOURCE_TYPE_BUFFER;
	callback.usage = RenderingDevice::CALLBACK_RESOURCE_USAGE_VERTEX_BUFFER_READ;
	callback.rid = vertex;
	CHECK(rd.driver_callback_add(nullptr, nullptr, VectorView<RenderingDevice::CallbackResource>(&callback, 1)) == OK);
	CHECK(errors.count == 0);
	rd.draw_graph.begin();
	rd.draw_graph.finalize();
	rd.free_rid(vertex);
	dispose(rd);
	CHECK(driver.frees == 1);
	rd.texture_owner.initialize_rid(pending_texture, RenderingDevice::Texture());
	rd.texture_owner.free(pending_texture);
	rd.storage_buffer_owner.initialize_rid(pending_storage, RenderingDevice::Buffer());
	rd.storage_buffer_owner.free(pending_storage);
	CHECK(errors.count == 0);
	remove_error_handler(&handler);
	detach(rd);
}

TEST_CASE("[RenderingDevice][NativeDisposal] native buffer export and callbacks ignore foreign pending slots") {
	TestNativeDisposalAccess::foreign_pending_export_and_callback();
}

void TestNativeDisposalAccess::depth_disposal() {
	using namespace TestNativeDisposal;
	RenderingDevice rd;
	DepthDisposalDriver driver;
	attach(rd, driver);
	RID texture, framebuffer;
	NativeDisposalWitness texture_witness, framebuffer_witness;
	REQUIRE(rd._native_disposal_depth_texture_create(8, 4, texture, texture_witness) == OK);
	REQUIRE(rd._native_disposal_depth_framebuffer_create(texture_witness, framebuffer, framebuffer_witness) == OK);
	driver.texture_witness = &texture_witness;
	driver.framebuffer_witness = &framebuffer_witness;
	struct InvalidationState {
		NativeDisposalWitness *witness;
		bool returned = false;
	} invalidation{ &framebuffer_witness };
	rd.framebuffer_set_invalidation_callback(framebuffer, [](void *data) {
		auto &event = *static_cast<InvalidationState *>(data);
		CHECK(state(*event.witness) == NativeDisposalWitness::State::ENQUEUED);
		event.returned = true; }, &invalidation);
	// Two distinct native cache variants, each with its own driver-created
	// framebuffer and render pass. Neither is the shared format render pass.
	auto *cache = rd.framebuffer_owner.get_or_null(framebuffer)->framebuffer_cache;
	for (uint64_t key = 1; key <= 2; ++key) {
		const auto pass = driver.render_pass_create({}, {}, {}, 1, {});
		const auto child = driver.framebuffer_create(pass, cache->textures, cache->width, cache->height);
		cache->storage_map.insert(key, { child, pass });
	}
	RenderingDevice::NativeImmutableHold immutable;
	CHECK(rd.native_immutable_hold_acquire(VectorView<RID>(&texture, 1), immutable) == ERR_UNAVAILABLE);
	CHECK_FALSE(immutable.is_active());
	CHECK(rd.native_immutable_hold_acquire(VectorView<RID>(&framebuffer, 1), immutable) == ERR_DOES_NOT_EXIST);
	CHECK_FALSE(immutable.is_active());
	CHECK(state(texture_witness) == NativeDisposalWitness::State::LIVE);
	CHECK(state(framebuffer_witness) == NativeDisposalWitness::State::LIVE);
	// Free the original texture. The original dependency cascade owns the fb.
	REQUIRE(release(rd, texture_witness) == OK);
	CHECK(invalidation.returned);
	CHECK(state(framebuffer_witness) == NativeDisposalWitness::State::ENQUEUED);
	CHECK(state(texture_witness) == NativeDisposalWitness::State::ENQUEUED);
	CHECK(driver.framebuffer_frees == 0);
	CHECK(driver.texture_frees == 0);
	CHECK(release(rd, framebuffer_witness) == OK);
	SUBCASE("ordinary original queue") {
		dispose(rd);
	}
	SUBCASE("bounded original queue pauses inside cache child pair") {
		uint32_t used = 0;
		CHECK(step(rd, 1, 1, UINT64_MAX, used) == ERR_BUSY);
		CHECK(used == 1);
		CHECK(driver.framebuffer_frees == 1);
		CHECK(driver.pass_frees == 0);
		CHECK(state(framebuffer_witness) == NativeDisposalWitness::State::ENQUEUED);
		CHECK(state(texture_witness) == NativeDisposalWitness::State::ENQUEUED);
		// Existing cursor resumes after the already returned framebuffer free.
		CHECK(step(rd, 1, 1, UINT64_MAX, used) == ERR_BUSY);
		CHECK(driver.framebuffer_frees == 1);
		CHECK(driver.pass_frees == 1);
		CHECK(state(framebuffer_witness) == NativeDisposalWitness::State::ENQUEUED);
		for (uint32_t turn = 0; turn < 8 && draining(rd); ++turn) {
			const Error result = step(rd, 1, 1, UINT64_MAX, used);
			CHECK((result == OK || result == ERR_BUSY));
		}
		CHECK_FALSE(draining(rd));
	}
	CHECK(driver.framebuffer_frees == 2);
	CHECK(driver.pass_frees == 2);
	CHECK(driver.texture_frees == 1);
	CHECK(state(framebuffer_witness) == NativeDisposalWitness::State::RETURNED);
	CHECK(state(texture_witness) == NativeDisposalWitness::State::RETURNED);
	CHECK(refs(framebuffer_witness) == 1);
	CHECK(refs(texture_witness) == 1);
	CHECK(rd.texture_owner.get_rid_count() == 0);
	CHECK(rd.framebuffer_owner.get_rid_count() == 0);
	CHECK(rd.dependency_map.is_empty());
	CHECK(rd.reverse_dependency_map.is_empty());
	CHECK(rd.texture_memory == 0);
	// Separate shared device-format owner teardown is not part of either receipt.
	for (const auto &entry : rd.framebuffer_formats) {
		driver.render_pass_free(entry.value.render_pass);
	}
	rd.framebuffer_formats.clear();
	rd.framebuffer_format_cache.clear();
	detach(rd);
}

void TestNativeDisposalAccess::depth_factory_failures() {
	using namespace TestNativeDisposal;
	RenderingDevice rd;
	DepthDisposalDriver driver;
	attach(rd, driver);
	RID texture, framebuffer;
	NativeDisposalWitness texture_witness, framebuffer_witness;
	CHECK(rd._native_disposal_depth_texture_create(0, 4, texture, texture_witness) == ERR_INVALID_PARAMETER);
	CHECK(driver.texture_creates == 0);
	driver.fail_texture = true;
	ERR_PRINT_OFF;
	CHECK(rd._native_disposal_depth_texture_create(8, 4, texture, texture_witness) == ERR_CANT_CREATE);
	ERR_PRINT_ON;
	CHECK(texture.is_null());
	CHECK(texture_witness.cell.is_null());
	CHECK(rd.texture_owner.get_rid_count() == 0);
	CHECK(rd.frames[0].textures_to_dispose_of.is_empty());
	CHECK(driver.texture_frees == 0);
	driver.fail_texture = false;
	REQUIRE(rd._native_disposal_depth_texture_create(8, 4, texture, texture_witness) == OK);
	const uint64_t texture_serial = texture_witness.cell->serial;
	CHECK(rd._native_disposal_depth_texture_create(8, 4, texture, texture_witness) == ERR_INVALID_PARAMETER);
	CHECK(texture_witness.cell->serial == texture_serial);
	CHECK(driver.texture_creates == 2);
	driver.fail_pass = true;
	ERR_PRINT_OFF;
	CHECK(rd._native_disposal_depth_framebuffer_create(texture_witness, framebuffer, framebuffer_witness) == ERR_CANT_CREATE);
	ERR_PRINT_ON;
	CHECK(framebuffer.is_null());
	CHECK(framebuffer_witness.cell.is_null());
	CHECK(rd.framebuffer_owner.get_rid_count() == 0);
	CHECK(rd.frames[0].framebuffers_to_dispose_of.is_empty());
	CHECK(state(texture_witness) == NativeDisposalWitness::State::LIVE);
	CHECK(driver.texture_frees == 0);
	CHECK(rd.framebuffer_formats.is_empty());
	// Failure of the second factory must not hide or self-ack the first object.
	driver.texture_witness = &texture_witness;
	CHECK(release(rd, texture_witness) == OK);
	CHECK(state(texture_witness) == NativeDisposalWitness::State::ENQUEUED);
	dispose(rd);
	CHECK(state(texture_witness) == NativeDisposalWitness::State::RETURNED);
	CHECK(driver.texture_frees == 1);
	CHECK(rd.texture_memory == 0);
	// Serial exhaustion refuses without another driver allocation or RID.
	rd.next_backing_serial = 0;
	RID exhausted;
	NativeDisposalWitness empty;
	CHECK(rd._native_disposal_depth_texture_create(8, 4, exhausted, empty) == ERR_OUT_OF_MEMORY);
	CHECK(exhausted.is_null());
	CHECK(empty.cell.is_null());
	CHECK(driver.texture_creates == 2);
	detach(rd);
}

void TestNativeDisposalAccess::depth_identity() {
	using namespace TestNativeDisposal;
	RenderingDevice rd, foreign;
	DepthDisposalDriver driver;
	attach(rd, driver);
	attach(foreign, driver);
	RID texture, framebuffer;
	NativeDisposalWitness texture_witness, framebuffer_witness;
	REQUIRE(rd._native_disposal_depth_texture_create(8, 4, texture, texture_witness) == OK);
	CHECK(foreign._native_disposal_depth_framebuffer_create(texture_witness, framebuffer, framebuffer_witness) == ERR_UNAUTHORIZED);
	CHECK(release(foreign, texture_witness) == ERR_UNAUTHORIZED);
	CHECK(framebuffer.is_null());
	CHECK(framebuffer_witness.cell.is_null());
	CHECK(state(texture_witness) == NativeDisposalWitness::State::LIVE);
	// Original view factory must clear the copied parent's disposal cell.
	const RID view = rd.texture_create_shared(RenderingDevice::TextureView(), texture);
	REQUIRE(view.is_valid());
	CHECK(rd.texture_owner.get_or_null(view)->native_disposal.cell.is_null());
	rd.free_rid(view);
	dispose(rd);
	CHECK(state(texture_witness) == NativeDisposalWitness::State::LIVE);
	CHECK(driver.texture_frees == 1);
	// Corrupt the actual owner incarnation, not the caller's projected report.
	auto *owner = rd.texture_owner.get_or_null(texture);
	++owner->native_disposal_serial;
	CHECK(release(rd, texture_witness) == ERR_INVALID_DATA);
	CHECK(state(texture_witness) == NativeDisposalWitness::State::UNKNOWN);
	CHECK(driver.texture_frees == 1);
	CHECK(rd.texture_owner.get_or_null(texture) == owner);
	// Fixture-only repair enables deterministic teardown; no public reset exists.
	--owner->native_disposal_serial;
	texture_witness.cell->state = NativeDisposalWitness::State::LIVE;
	driver.texture_witness = &texture_witness;
	CHECK(release(rd, texture_witness) == OK);
	CHECK(rd._native_disposal_depth_framebuffer_create(texture_witness, framebuffer, framebuffer_witness) == ERR_INVALID_DATA);
	dispose(rd);
	CHECK(state(texture_witness) == NativeDisposalWitness::State::RETURNED);
	CHECK(driver.texture_frees == 2);
	CHECK(rd.texture_memory == 0);
	detach(foreign);
	detach(rd);
}
