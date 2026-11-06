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
	static void sampled_disposal();
	static void sampled_failure();
	static void sampled_identity();
	static void sampler_disposal();
	static void sampler_factory_failures();
	static void sampler_identity();
	static void sampler_ordinary();
	static void geometry_direct();
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

// Observe the real sampler factory and both original pending-free drains.
// Reusing native id 17 deliberately leaves incarnation identity to the owner.
class SamplerDisposalDriver : public TransferTestDriverBase {
public:
	Vector<NativeDisposalWitness *> free_witnesses;
	bool fail_create = false;
	uint32_t creates = 0, frees = 0, names = 0;
	String last_name;
	SamplerState last_state;
	SamplerID sampler_create(const SamplerState &p_state) override {
		++creates;
		last_state = p_state;
		return fail_create ? SamplerID() : SamplerID(17);
	}
	void sampler_free(SamplerID p_id) override {
		CHECK(p_id == SamplerID(17));
		REQUIRE(frees < uint32_t(free_witnesses.size()));
		if (auto *witness = free_witnesses[frees]) {
			CHECK(TestNativeDisposalAccess::state(*witness) == NativeDisposalWitness::State::ENQUEUED);
			CHECK(TestNativeDisposalAccess::refs(*witness) >= 2);
		}
		++frees;
	}
	void set_object_name(ObjectType p_type, ID p_id, const String &p_name) override {
		CHECK(p_type == OBJECT_TYPE_SAMPLER);
		CHECK(p_id.id == 17);
		CHECK(p_name.begins_with("RID:"));
		++names;
		last_name = p_name;
	}
};

TEST_CASE("[RenderingDevice][NativeDisposal][Sampler] actual driver return owns ordinary and bounded receipts") {
	TestNativeDisposalAccess::sampler_disposal();
}
TEST_CASE("[RenderingDevice][NativeDisposal][Sampler] failed factories leave original rollback ownership intact") {
	TestNativeDisposalAccess::sampler_factory_failures();
}
TEST_CASE("[RenderingDevice][NativeDisposal][Sampler] foreign stale and forged identities cannot sign returned") {
	TestNativeDisposalAccess::sampler_identity();
}
TEST_CASE("[RenderingDevice][NativeDisposal][Sampler] ordinary creation and free retain their original path") {
	TestNativeDisposalAccess::sampler_ordinary();
}

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

// Real serialized reflection/native shader factory; CPU fixture, no compiler.
class SampledDepthContainer : public RenderingShaderContainer {
protected:
	uint32_t _format() const override { return 0x4453504c; }
	uint32_t _format_version() const override { return 1; }
	bool _set_code_from_spirv(const ReflectShader &) override { return false; }

public:
	void initialize(uint32_t p_length = 1, bool p_writable = false) {
		reflection_data.pipeline_type =
				RenderingDeviceCommons::PIPELINE_TYPE_RASTERIZATION;
		reflection_data.stage_count = 2;
		reflection_data.set_count = 1;
		reflection_binding_set_uniforms_count.push_back(1);
		ReflectionBindingData binding;
		binding.type = RenderingDeviceCommons::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE;
		binding.binding = 3;
		binding.length = p_length;
		binding.writable = p_writable;
		binding.stages = 1u << RenderingDeviceCommons::SHADER_STAGE_FRAGMENT;
		reflection_binding_set_uniforms_data.push_back(binding);
		for (auto stage : { RenderingDeviceCommons::SHADER_STAGE_VERTEX,
					 RenderingDeviceCommons::SHADER_STAGE_FRAGMENT }) {
			reflection_shader_stages.push_back(stage);
			Shader shader;
			shader.shader_stage = stage;
			shader.code_compressed_bytes.push_back(42);
			shader.code_decompressed_size = 1;
			shaders.push_back(shader);
		}
	}
};
class SampledDepthFormat : public RenderingShaderContainerFormat {
public:
	Ref<RenderingShaderContainer> create_container() const override {
		return memnew(SampledDepthContainer);
	}
	ShaderLanguageVersion get_shader_language_version() const override {
		return SHADER_LANGUAGE_VULKAN_VERSION_1_1;
	}
	ShaderSpirvVersion get_shader_spirv_version() const override {
		return SHADER_SPIRV_VERSION_1_3;
	}
};
class SampledDepthDriver : public DepthDisposalDriver {
public:
	uint64_t api_trait_get(ApiTrait p_trait) override { return p_trait == API_TRAIT_TEXTURE_OUTPUTS_REQUIRE_CLEARS ? 1 : DepthDisposalDriver::api_trait_get(p_trait); }
	SampledDepthFormat format;
	NativeDisposalWitness *sampler_witness = nullptr, *set_witness = nullptr;
	bool fail_set = false;
	uint32_t sampler_creates = 0, sampler_frees = 0, set_creates = 0,
			 set_frees = 0, shader_frees = 0;
	uint32_t clear_calls = 0, prepare_calls = 0;
	const RenderingShaderContainerFormat &
	get_shader_container_format() const override {
		return format;
	}
	ShaderID shader_create_from_container(
			const Ref<RenderingShaderContainer> &p_container,
			const Vector<ImmutableSampler> &p_immutable) override {
		CHECK(p_immutable.is_empty());
		const auto &reflection = p_container->get_shader_reflection();
		CHECK(reflection.pipeline_type == PIPELINE_TYPE_RASTERIZATION);
		REQUIRE(reflection.uniform_sets.size() == 1);
		REQUIRE(reflection.uniform_sets[0].size() == 1);
		CHECK(reflection.uniform_sets[0][0].binding == 3);
		return ShaderID(301);
	}
	uint32_t shader_get_layout_hash(ShaderID p_id) override {
		CHECK(p_id == ShaderID(301));
		return 123;
	}
	bool shader_retire_ready_nonblocking(ShaderID) const override { return true; }
	void shader_free(ShaderID p_id) override {
		CHECK(p_id == ShaderID(301));
		++shader_frees;
	}
	SamplerID sampler_create(const SamplerState &) override {
		++sampler_creates;
		return SamplerID(17);
	}
	void sampler_free(SamplerID p_id) override {
		CHECK(p_id == SamplerID(17));
		REQUIRE(sampler_witness != nullptr);
		CHECK(TestNativeDisposalAccess::state(*sampler_witness) ==
				NativeDisposalWitness::State::ENQUEUED);
		CHECK(TestNativeDisposalAccess::refs(*sampler_witness) >= 2);
		++sampler_frees;
	}
	UniformSetID uniform_set_create(VectorView<BoundUniform> p_uniforms,
			ShaderID p_shader, uint32_t p_set,
			int p_pool) override {
		++set_creates;
		CHECK(p_shader == ShaderID(301));
		CHECK(p_set == 0);
		CHECK(p_pool == -1);
		REQUIRE(p_uniforms.size() == 1);
		CHECK(p_uniforms[0].type == UNIFORM_TYPE_SAMPLER_WITH_TEXTURE);
		CHECK(p_uniforms[0].binding == 3);
		CHECK_FALSE(p_uniforms[0].immutable_sampler);
		REQUIRE(p_uniforms[0].ids.size() == 2);
		CHECK(p_uniforms[0].ids[0].id == 17);
		CHECK(p_uniforms[0].ids[1].id == 51);
		return fail_set ? UniformSetID() : UniformSetID(61);
	}
	void uniform_set_free(UniformSetID p_id) override {
		CHECK(p_id == UniformSetID(61));
		REQUIRE(set_witness != nullptr);
		CHECK(TestNativeDisposalAccess::state(*set_witness) ==
				NativeDisposalWitness::State::ENQUEUED);
		CHECK(TestNativeDisposalAccess::refs(*set_witness) >= 2);
		++set_frees;
	}
	void command_clear_depth_stencil_texture(
			CommandBufferID, TextureID, TextureLayout, float, uint8_t,
			const TextureSubresourceRange &) override {
		++clear_calls;
	}
	void command_uniform_set_prepare_for_use(CommandBufferID, UniformSetID,
			ShaderID, uint32_t) override {
		++prepare_calls;
	}
	void set_object_name(ObjectType p_type, ID p_id,
			const String &p_name) override {
		if (p_type == OBJECT_TYPE_TEXTURE) {
			DepthDisposalDriver::set_object_name(p_type, p_id, p_name);
			return;
		}
		CHECK(p_name.begins_with("RID:"));
		if (p_type == OBJECT_TYPE_SAMPLER) {
			CHECK(p_id.id == 17);
		} else if (p_type == OBJECT_TYPE_SHADER) {
			CHECK(p_id.id == 301);
		} else {
			CHECK(p_type == OBJECT_TYPE_UNIFORM_SET);
			CHECK(p_id.id == 61);
		}
	}
};

TEST_CASE("[RenderingDevice][NativeDisposal][D32Sample] original descriptor "
		  "payload and independent cascade receipts") {
	TestNativeDisposalAccess::sampled_disposal();
}
TEST_CASE("[RenderingDevice][NativeDisposal][D32Sample] failed set preserves "
		  "successful parents for original rollback") {
	TestNativeDisposalAccess::sampled_failure();
}
TEST_CASE("[RenderingDevice][NativeDisposal][D32Sample] bounded reflection and "
		  "foreign identity refuse before native create") {
	TestNativeDisposalAccess::sampled_identity();
}

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

// GeometryDirect is a narrow capture profile over original draw-list state.
// This fixture checks original factory provenance and graph execution against a
// strict driver. It does not provide source authority or GPU completion evidence.
void TestNativeDisposalAccess::geometry_direct() {
	using namespace TestNativeDisposal;
	RenderingDevice rd;
	struct GeometryDriver : RasterCapacityDriver {
		uint64_t next_buffer = 10;
		uint32_t buffer_frees = 0;
		uint32_t vertex_format_frees = 0;
		Vector<uint64_t> payload;
		CommandBufferID command_buffer_create(CommandPoolID) override { return CommandBufferID(12); }
		bool command_buffer_begin(CommandBufferID) override { return true; }
		void command_buffer_end(CommandBufferID) override {}
		SemaphoreID semaphore_create() override { return SemaphoreID(13); }
		void command_insert_breadcrumb(CommandBufferID, uint32_t) override {}
		void command_pipeline_barrier(CommandBufferID, BitField<PipelineStageBits>, BitField<PipelineStageBits>, VectorView<MemoryAccessBarrier>, VectorView<BufferBarrier>, VectorView<TextureBarrier>, VectorView<AccelerationStructureBarrier>) override {}
		void command_begin_render_pass(CommandBufferID p_buffer, RenderPassID p_pass, FramebufferID p_framebuffer, CommandBufferType, const Rect2i &, VectorView<RenderPassClearValue>) override {
			CHECK(p_buffer == CommandBufferID(11));
			CHECK(p_pass == RenderPassID(1));
			CHECK(p_framebuffer == FramebufferID(2));
			payload.push_back(10);
		}
		void command_end_render_pass(CommandBufferID p_buffer) override {
			CHECK(p_buffer == CommandBufferID(11));
			payload.push_back(20);
		}
		void command_bind_render_pipeline(CommandBufferID p_buffer, PipelineID p_pipeline) override {
			CHECK(p_buffer == CommandBufferID(11));
			payload.push_back(1);
			payload.push_back(p_pipeline.id);
		}
		void command_render_bind_vertex_buffers(CommandBufferID p_buffer, uint32_t p_count, const BufferID *p_buffers, const uint64_t *p_offsets, uint64_t p_dynamic_offsets) override {
			CHECK(p_buffer == CommandBufferID(11));
			payload.push_back(2);
			payload.push_back(p_count);
			for (uint32_t i = 0; i < p_count; ++i) {
				payload.push_back(p_buffers[i].id);
				payload.push_back(p_offsets[i]);
			}
			payload.push_back(p_dynamic_offsets);
		}
		void command_render_bind_index_buffer(CommandBufferID p_buffer, BufferID p_index, IndexBufferFormat p_format, uint64_t p_offset) override {
			CHECK(p_buffer == CommandBufferID(11));
			payload.push_back(3);
			payload.push_back(p_index.id);
			payload.push_back(p_format);
			payload.push_back(p_offset);
		}
		void command_render_draw_indexed(CommandBufferID p_buffer, uint32_t p_count, uint32_t p_instances, uint32_t p_first_index, int32_t p_vertex_offset, uint32_t p_first_instance) override {
			CHECK(p_buffer == CommandBufferID(11));
			payload.push_back(4);
			payload.push_back(p_count);
			payload.push_back(p_instances);
			payload.push_back(p_first_index);
			payload.push_back(p_vertex_offset);
			payload.push_back(p_first_instance);
		}
		void command_render_draw(CommandBufferID p_buffer, uint32_t p_count, uint32_t p_instances, uint32_t p_base_vertex, uint32_t p_first_instance) override {
			CHECK(p_buffer == CommandBufferID(11));
			payload.push_back(5);
			payload.push_back(p_count);
			payload.push_back(p_instances);
			payload.push_back(p_base_vertex);
			payload.push_back(p_first_instance);
		}
		BufferID buffer_create(uint64_t, BitField<BufferUsageBits>, MemoryAllocationType, uint64_t) override { return BufferID(next_buffer++); }
		void buffer_free(BufferID p_buffer) override {
			CHECK((p_buffer == BufferID(10) || p_buffer == BufferID(11)));
			++buffer_frees;
		}
		VertexFormatID vertex_format_create(Span<VertexAttribute> p_attributes, const VertexAttributeBindingsMap &) override {
			CHECK(p_attributes.size() == 1);
			return VertexFormatID(7);
		}
		void vertex_format_free(VertexFormatID p_vertex_format) override {
			CHECK(p_vertex_format == VertexFormatID(7));
			++vertex_format_frees;
		}
		void set_object_name(ObjectType p_type, ID p_id, const String &p_name) override {
			if (p_type == OBJECT_TYPE_BUFFER) {
				CHECK((p_id.id == 10 || p_id.id == 11));
				return;
			}
			RasterCapacityDriver::set_object_name(p_type, p_id, p_name);
		}
		uint64_t buffer_get_dynamic_offsets(Span<BufferID>) override { return 0; }
		uint64_t api_trait_get(ApiTrait) override { return 0; }
	} driver;
	attach(rd, driver);
	Ref<RasterContainer> container;
	container.instantiate();
	container->initialize();
	const RID shader_rid = rd.shader_create_from_bytecode(container->to_bytes());
	REQUIRE(shader_rid.is_valid());
	const auto framebuffer_format = rd.framebuffer_format_create(Vector<RenderingDevice::AttachmentFormat>());
	rd.draw_graph.initialize(&driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
	rd.draw_graph.begin(true);
	const RDG::RecordHandle begin = rd.draw_graph.add_draw_list_begin(RDD::RenderPassID(1), RDD::FramebufferID(2), Rect2i(), {}, {}, {}, 0, false);
	rd.draw_list.begin_handle = begin;
	rd.draw_list.active = true;
	rd.draw_list_framebuffer_format = framebuffer_format;
	rd.draw_list_current_subpass = 0;

	RenderingDevice::VertexAttribute attribute;
	attribute.location = 0;
	attribute.format = RenderingDevice::DATA_FORMAT_R32G32B32_SFLOAT;
	attribute.stride = 12;
	attribute.frequency = RenderingDevice::VERTEX_FREQUENCY_VERTEX;
	const auto vertex_format = rd.vertex_format_create(Vector<RenderingDevice::VertexAttribute>{ attribute });
	REQUIRE(bool(vertex_format != RenderingDevice::INVALID_ID));
	const RID vertex_buffer = rd.vertex_buffer_create(128);
	REQUIRE(vertex_buffer.is_valid());
	const RID vertex_rid = rd.vertex_array_create(4, vertex_format, Vector<RID>{ vertex_buffer });
	REQUIRE(vertex_rid.is_valid());
	const RID index_buffer = rd.index_buffer_create(8, RenderingDevice::INDEX_BUFFER_FORMAT_UINT32);
	REQUIRE(index_buffer.is_valid());
	// Two elements of prefix belong to the index bind, not draw first_index.
	const RID index_rid = rd.index_array_create(index_buffer, 2, 6);
	REQUIRE(index_rid.is_valid());
	const RID pipeline_rid = rd.render_pipeline_create(shader_rid, framebuffer_format, vertex_format, RenderingDevice::RENDER_PRIMITIVE_TRIANGLES,
			RenderingDevice::PipelineRasterizationState(), RenderingDevice::PipelineMultisampleState(), RenderingDevice::PipelineDepthStencilState(), RenderingDevice::PipelineColorBlendState());
	REQUIRE(pipeline_rid.is_valid());
	rd.draw_list.state.set_count = 0;
#ifdef DEBUG_ENABLED
	rd.draw_list.validation.pipeline_active = true;
	rd.draw_list.validation.pipeline_vertex_format = vertex_format;
	rd.draw_list.validation.vertex_format = vertex_format;
	rd.draw_list.validation.pipeline_primitive_minimum = 3;
	rd.draw_list.validation.pipeline_primitive_divisor = 3;
	rd.draw_list.validation.pipeline_push_constant_size = 0;
#endif
	const auto pipeline_bind = rd._draw_list_bind_render_pipeline_recorded((int64_t(RenderingDevice::ID_TYPE_DRAW_LIST) << RenderingDevice::ID_BASE_SHIFT), pipeline_rid);
	CHECK(pipeline_bind.error == OK);
	CHECK(pipeline_bind.effect == RenderingDevice::NativeAppendEffect::Recorded);
	CHECK(pipeline_bind.capture_complete);
	CHECK(pipeline_bind.draw_handle.is_valid());
	CHECK(rd.draw_graph.contains_record(pipeline_bind.draw_handle));
	const auto vertex_bind = rd._draw_list_bind_vertex_array_recorded((int64_t(RenderingDevice::ID_TYPE_DRAW_LIST) << RenderingDevice::ID_BASE_SHIFT), vertex_rid, nullptr, 0);
	CHECK(vertex_bind.error == OK);
	CHECK(vertex_bind.effect == RenderingDevice::NativeAppendEffect::Recorded);
	CHECK(vertex_bind.capture_complete);
	CHECK(vertex_bind.bind_handle.is_valid());
	CHECK(rd.draw_graph.contains_record(vertex_bind.bind_handle));
	const auto index_bind = rd._draw_list_bind_index_array_recorded((int64_t(RenderingDevice::ID_TYPE_DRAW_LIST) << RenderingDevice::ID_BASE_SHIFT), index_rid, nullptr, 0);
	CHECK(index_bind.error == OK);
	CHECK(index_bind.effect == RenderingDevice::NativeAppendEffect::Recorded);
	CHECK(index_bind.capture_complete);
	CHECK(index_bind.bind_handle.is_valid());
	CHECK(rd.draw_graph.contains_record(index_bind.bind_handle));

	const uint32_t before = rd.draw_graph.qualification_command_count();
	const auto indexed = rd._draw_list_draw_geometry_direct_recorded((int64_t(RenderingDevice::ID_TYPE_DRAW_LIST) << RenderingDevice::ID_BASE_SHIFT), true);
	CHECK(indexed.error == OK);
	CHECK(indexed.effect == RenderingDevice::NativeAppendEffect::Recorded);
	CHECK(indexed.capture_complete);
	CHECK(indexed.draw_handle.is_valid());
	CHECK(rd.draw_graph.contains_record(indexed.draw_handle));
	CHECK(rd.draw_graph.qualification_command_count() == before);
	CHECK(rd.draw_list.state.draw_count == 1);
	CHECK(rd.draw_graph.record_mapping_state(indexed.draw_handle) == RDG::RecordMappingState::Pending);

	// The checked profile must reject a stale cached index count before the
	// shared draw tail can append a native instruction. Restore the original
	// count immediately so the subsequent cases retain the original factory
	// state.
	const uint32_t cached_index_count = rd.draw_list.validation.index_array_count;
	rd.draw_list.validation.index_array_count = cached_index_count - 3;
	const uint32_t draws_before_stale_count = rd.draw_list.state.draw_count;
	ERR_PRINT_OFF;
	const auto stale_count = rd._draw_list_draw_geometry_direct_recorded((int64_t(RenderingDevice::ID_TYPE_DRAW_LIST) << RenderingDevice::ID_BASE_SHIFT), true);
	ERR_PRINT_ON;
	CHECK(stale_count.error == ERR_INVALID_DATA);
	CHECK(stale_count.effect == RenderingDevice::NativeAppendEffect::NoNewEffect);
	CHECK_FALSE(stale_count.capture_complete);
	CHECK_FALSE(stale_count.draw_handle.is_valid());
	CHECK(rd.draw_list.state.draw_count == draws_before_stale_count);
	rd.draw_list.validation.index_array_count = cached_index_count;

	// A RID from a different owner must be refused by the original index-array
	// owner check, leaving the previously recorded graph prefix untouched.
	const RID saved_index = rd.draw_list.state.index_array;
	rd.draw_list.state.index_array = vertex_rid;
	const uint32_t draws_before_foreign = rd.draw_list.state.draw_count;
	ERR_PRINT_OFF;
	const auto foreign = rd._draw_list_draw_geometry_direct_recorded((int64_t(RenderingDevice::ID_TYPE_DRAW_LIST) << RenderingDevice::ID_BASE_SHIFT), true);
	ERR_PRINT_ON;
	CHECK(foreign.error == ERR_INVALID_PARAMETER);
	CHECK(foreign.effect == RenderingDevice::NativeAppendEffect::NoNewEffect);
	CHECK_FALSE(foreign.capture_complete);
	CHECK_FALSE(foreign.draw_handle.is_valid());
	CHECK(rd.draw_list.state.draw_count == draws_before_foreign);
	rd.draw_list.state.index_array = saved_index;

	// A 4-vertex original VA cannot issue a non-indexed triangle-list draw.
	rd.draw_list.state.index_array = RID();
	const uint32_t before_refused = rd.draw_graph.qualification_command_count();
	ERR_PRINT_OFF;
	const auto refused = rd._draw_list_draw_geometry_direct_recorded((int64_t(RenderingDevice::ID_TYPE_DRAW_LIST) << RenderingDevice::ID_BASE_SHIFT), false);
	ERR_PRINT_ON;
	CHECK(refused.error == ERR_INVALID_PARAMETER);
	CHECK(refused.effect == RenderingDevice::NativeAppendEffect::NoNewEffect);
	CHECK_FALSE(refused.capture_complete);
	CHECK_FALSE(refused.draw_handle.is_valid());
	CHECK(rd.draw_graph.qualification_command_count() == before_refused);

	// A separate original VA with three vertices is accepted for the same
	// no-descriptor GeometryDirect profile.
	const RID triangle_rid = rd.vertex_array_create(3, vertex_format, Vector<RID>{ vertex_buffer });
	REQUIRE(triangle_rid.is_valid());
	const auto triangle_bind = rd._draw_list_bind_vertex_array_recorded((int64_t(RenderingDevice::ID_TYPE_DRAW_LIST) << RenderingDevice::ID_BASE_SHIFT), triangle_rid, nullptr, 0);
	CHECK(triangle_bind.error == OK);
	const auto triangle_draw = rd._draw_list_draw_geometry_direct_recorded((int64_t(RenderingDevice::ID_TYPE_DRAW_LIST) << RenderingDevice::ID_BASE_SHIFT), false);
	CHECK(triangle_draw.error == OK);
	CHECK(triangle_draw.effect == RenderingDevice::NativeAppendEffect::Recorded);
	CHECK(triangle_draw.capture_complete);
	CHECK(triangle_draw.draw_handle.is_valid());
	CHECK(rd.draw_graph.contains_record(triangle_draw.draw_handle));

	// A stale vertex count that is itself triangle-list aligned must not reach
	// the unchanged tail, which draws from this cached count in all builds.
	const uint32_t cached_vertex_count = rd.draw_list.validation.vertex_array_size;
	rd.draw_list.validation.vertex_array_size = cached_vertex_count + 3;
	const uint32_t draws_before_stale_vertex = rd.draw_list.state.draw_count;
	ERR_PRINT_OFF;
	const auto stale_vertex = rd._draw_list_draw_geometry_direct_recorded((int64_t(RenderingDevice::ID_TYPE_DRAW_LIST) << RenderingDevice::ID_BASE_SHIFT), false);
	ERR_PRINT_ON;
	CHECK(stale_vertex.error == ERR_INVALID_DATA);
	CHECK(stale_vertex.effect == RenderingDevice::NativeAppendEffect::NoNewEffect);
	CHECK_FALSE(stale_vertex.capture_complete);
	CHECK_FALSE(stale_vertex.draw_handle.is_valid());
	CHECK(rd.draw_list.state.draw_count == draws_before_stale_vertex);
	rd.draw_list.validation.vertex_array_size = cached_vertex_count;

	// Required descriptor state is refused before any draw graph effect.
	rd.draw_list.state.set_count = 1;
	rd.draw_list.state.sets[0].pipeline_expected_format = 1;
	const uint32_t before_set_refused = rd.draw_graph.qualification_command_count();
	ERR_PRINT_OFF;
	const auto set_refused = rd._draw_list_draw_geometry_direct_recorded((int64_t(RenderingDevice::ID_TYPE_DRAW_LIST) << RenderingDevice::ID_BASE_SHIFT), false);
	ERR_PRINT_ON;
	CHECK(set_refused.error == ERR_UNAVAILABLE);
	CHECK(set_refused.effect == RenderingDevice::NativeAppendEffect::NoNewEffect);
	CHECK_FALSE(set_refused.draw_handle.is_valid());
	CHECK(rd.draw_graph.qualification_command_count() == before_set_refused);

	// Finish the same original list and execute its stored instructions. The
	// failed captures above must not add any native draw to this exact payload.
	CHECK(driver.payload.is_empty());
	const auto end = rd._draw_list_end_recorded(true);
	CHECK(end.error == OK);
	CHECK(end.capture_complete);
	CHECK_FALSE(rd.draw_list.active);
	RenderingDeviceGraph::CommandBufferPool pool;
	RDD::CommandBufferID primary(11);
	rd.draw_graph.end(false, false, primary, pool);
	const uint64_t expected[] = {
		10, 1, 400, 2, 1, 10, 0, 0,
		3, 11, RDD::INDEX_BUFFER_FORMAT_UINT32, 8,
		4, 6, 1, 0, 0, 0,
		2, 1, 10, 0, 0,
		5, 3, 1, 0, 0, 20
	};
	REQUIRE(driver.payload.size() == int(sizeof(expected) / sizeof(expected[0])));
	for (int i = 0; i < driver.payload.size(); ++i) {
		CHECK(driver.payload[i] == expected[i]);
	}
	for (const auto &record : { begin, pipeline_bind.draw_handle, vertex_bind.bind_handle, index_bind.bind_handle, indexed.draw_handle, triangle_bind.bind_handle, triangle_draw.draw_handle, end.draw_handle }) {
		CHECK(rd.draw_graph.contains_record(record));
		CHECK(rd.draw_graph.record_mapping_state(record) == RDG::RecordMappingState::Recorded);
		uint32_t slot = UINT32_MAX;
		CHECK(rd.draw_graph.resolve_record_command_buffer_slot(record, slot));
		CHECK(slot == 0);
		RDD::CommandBufferID actual;
		CHECK(rd.draw_graph.resolve_record_command_buffer(record, actual));
		CHECK(actual == primary);
	}
	rd.draw_graph.begin();
	rd.free_rid(vertex_rid);
	rd.free_rid(triangle_rid);
	rd.free_rid(index_rid);
	rd.free_rid(pipeline_rid);
	rd.free_rid(vertex_buffer);
	rd.free_rid(index_buffer);
	rd.free_rid(shader_rid);
	dispose(rd);
	CHECK_FALSE(rd.vertex_array_owner.owns(vertex_rid));
	CHECK_FALSE(rd.vertex_array_owner.owns(triangle_rid));
	CHECK_FALSE(rd.index_array_owner.owns(index_rid));
	CHECK_FALSE(rd.render_pipeline_owner.owns(pipeline_rid));
	CHECK(driver.pipeline_frees == 1);
	CHECK(driver.shader_frees == 1);
	CHECK(driver.buffer_frees == 2);
	CHECK(rd.dependency_map.is_empty());
	CHECK(rd.reverse_dependency_map.is_empty());
	for (auto &entry : rd.framebuffer_formats) {
		driver.render_pass_free(entry.value.render_pass);
	}
	rd.framebuffer_formats.clear();
	rd.framebuffer_format_cache.clear();
	// detach() removes the driver; release the original cached vertex format
	// first so RenderingDevice's destructor has no native cache left to free.
	for (auto &entry : rd.vertex_formats) {
		driver.vertex_format_free(entry.value.driver_id);
	}
	rd.vertex_formats.clear();
	rd.vertex_format_cache.clear();
	CHECK(driver.vertex_format_frees == 1);
	detach(rd);
}

TEST_CASE("[RenderingDevice][NativeDisposal][GeometryDirect] original factories append quad and triangle and refuse unsupported state") {
	TestNativeDisposalAccess::geometry_direct();
}

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

void TestNativeDisposalAccess::sampler_disposal() {
	using namespace TestNativeDisposal;
	RenderingDevice rd;
	SamplerDisposalDriver driver;
	attach(rd, driver);
	RID first, second;
	NativeDisposalWitness first_witness, second_witness;
	RenderingDevice::SamplerState sampler_state;
	sampler_state.enable_compare = true;
	sampler_state.compare_op = RenderingDevice::COMPARE_OP_LESS;
	REQUIRE(rd._native_disposal_sampler_create(sampler_state, first, first_witness) == OK);
	REQUIRE(rd._native_disposal_sampler_create(sampler_state, second, second_witness) == OK);
	CHECK(driver.last_state.enable_compare);
	CHECK(driver.last_state.compare_op == RenderingDevice::COMPARE_OP_LESS);
	CHECK(first_witness.cell != second_witness.cell);
	CHECK(first_witness.cell->serial != second_witness.cell->serial);
	CHECK(rd.sampler_owner.get_or_null(first)->native_disposal.cell == first_witness.cell);
	NativeDisposalWitness::Report report;
	REQUIRE(observe(rd, first_witness, report) == OK);
	CHECK(report.state == NativeDisposalWitness::State::LIVE);
	CHECK(report.rid == first.get_id());
	CHECK(report.serial != 0);
#ifdef DEV_ENABLED
	CHECK(driver.names == 2);
	CHECK(driver.last_name == "RID:" + itos(second.get_id()));
#else
	CHECK(driver.names == 0);
#endif
	driver.free_witnesses.push_back(&first_witness);
	driver.free_witnesses.push_back(&second_witness);
	REQUIRE(release(rd, first_witness) == OK);
	REQUIRE(release(rd, second_witness) == OK);
	CHECK_FALSE(rd.sampler_owner.owns(first));
	CHECK_FALSE(rd.sampler_owner.owns(second));
	CHECK(state(first_witness) == NativeDisposalWitness::State::ENQUEUED);
	CHECK(state(second_witness) == NativeDisposalWitness::State::ENQUEUED);
	CHECK(driver.frees == 0);
	CHECK(release(rd, first_witness) == OK);
	SUBCASE("ordinary original queue") {
		dispose(rd);
	}
	SUBCASE("bounded original queue returns one sampler at a time") {
		uint32_t used = 0;
		CHECK(step(rd, 1, 1, UINT64_MAX, used) == ERR_BUSY);
		CHECK(used == 1);
		CHECK(driver.frees == 1);
		CHECK(state(first_witness) == NativeDisposalWitness::State::RETURNED);
		CHECK(state(second_witness) == NativeDisposalWitness::State::ENQUEUED);
		CHECK(step(rd, 1, 1, UINT64_MAX, used) == OK);
		CHECK(used == 1);
		CHECK_FALSE(draining(rd));
	}
	CHECK(driver.frees == 2);
	CHECK(state(first_witness) == NativeDisposalWitness::State::RETURNED);
	CHECK(state(second_witness) == NativeDisposalWitness::State::RETURNED);
	CHECK(refs(first_witness) == 1);
	CHECK(refs(second_witness) == 1);
	CHECK(release(rd, first_witness) == OK);
	CHECK(driver.frees == 2);
	CHECK(rd.frames[0].samplers_to_dispose_of.is_empty());
	CHECK(rd.sampler_owner.get_rid_count() == 0);
	CHECK(rd.dependency_map.is_empty());
	CHECK(rd.reverse_dependency_map.is_empty());
	detach(rd);
}

void TestNativeDisposalAccess::sampler_factory_failures() {
	using namespace TestNativeDisposal;
	RenderingDevice rd;
	SamplerDisposalDriver driver;
	attach(rd, driver);
	RID rid;
	NativeDisposalWitness witness;
	const RenderingDevice::SamplerState sampler_state;
	local(rd, true);
	CHECK(rd._native_disposal_sampler_create(sampler_state, rid, witness) == ERR_UNAUTHORIZED);
	local(rd, false);
	CHECK(driver.creates == 0);
	rd.draw_list.active = true;
	CHECK(rd._native_disposal_sampler_create(sampler_state, rid, witness) == ERR_BUSY);
	rd.draw_list.active = false;
	CHECK(driver.creates == 0);
	// Original validation can fail before driver creation. A reserved witness
	// and RID must roll back on every such return, not just native failure.
	for (uint32_t invalid_field = 0; invalid_field < 5; ++invalid_field) {
		RenderingDevice::SamplerState invalid = sampler_state;
		switch (invalid_field) {
			case 0:
				invalid.repeat_u = RenderingDevice::SAMPLER_REPEAT_MODE_MAX;
				break;
			case 1:
				invalid.repeat_v = RenderingDevice::SAMPLER_REPEAT_MODE_MAX;
				break;
			case 2:
				invalid.repeat_w = RenderingDevice::SAMPLER_REPEAT_MODE_MAX;
				break;
			case 3:
				invalid.compare_op = RenderingDevice::COMPARE_OP_MAX;
				break;
			case 4:
				invalid.border_color = RenderingDevice::SAMPLER_BORDER_COLOR_MAX;
				break;
		}
		ERR_PRINT_OFF;
		CHECK(rd._native_disposal_sampler_create(invalid, rid, witness) == ERR_CANT_CREATE);
		ERR_PRINT_ON;
		CHECK(rid.is_null());
		CHECK(witness.cell.is_null());
		CHECK(driver.creates == 0);
		CHECK(driver.frees == 0);
		CHECK(rd.sampler_owner.get_rid_count() == 0);
		CHECK(rd.frames[0].samplers_to_dispose_of.is_empty());
	}
	driver.fail_create = true;
	ERR_PRINT_OFF;
	CHECK(rd._native_disposal_sampler_create(sampler_state, rid, witness) == ERR_CANT_CREATE);
	ERR_PRINT_ON;
	CHECK(rid.is_null());
	CHECK(witness.cell.is_null());
	CHECK(driver.creates == 1);
	CHECK(driver.frees == 0);
	CHECK(driver.names == 0);
	CHECK(rd.sampler_owner.get_rid_count() == 0);
	CHECK(rd.frames[0].samplers_to_dispose_of.is_empty());
	driver.fail_create = false;
	REQUIRE(rd._native_disposal_sampler_create(sampler_state, rid, witness) == OK);
	const uint64_t serial = witness.cell->serial;
	CHECK(rd._native_disposal_sampler_create(sampler_state, rid, witness) == ERR_INVALID_PARAMETER);
	CHECK(witness.cell->serial == serial);
	CHECK(state(witness) == NativeDisposalWitness::State::LIVE);
	CHECK(driver.creates == 2);
	RID empty_rid;
	NativeDisposalWitness empty_witness;
	CHECK(rd._native_disposal_sampler_create(sampler_state, rid, empty_witness) == ERR_INVALID_PARAMETER);
	CHECK(empty_witness.cell.is_null());
	CHECK(rd._native_disposal_sampler_create(sampler_state, empty_rid, witness) == ERR_INVALID_PARAMETER);
	CHECK(empty_rid.is_null());
	rd.next_backing_serial = 0;
	CHECK(rd._native_disposal_sampler_create(sampler_state, empty_rid, empty_witness) == ERR_OUT_OF_MEMORY);
	CHECK(empty_rid.is_null());
	CHECK(empty_witness.cell.is_null());
	CHECK(driver.creates == 2);
	CHECK(driver.frees == 0);
	// The earlier successful factory remains the original caller's rollback.
	driver.free_witnesses.push_back(&witness);
	CHECK(release(rd, witness) == OK);
	dispose(rd);
	CHECK(driver.frees == 1);
	CHECK(state(witness) == NativeDisposalWitness::State::RETURNED);
	CHECK(rd.sampler_owner.get_rid_count() == 0);
	detach(rd);
}

void TestNativeDisposalAccess::sampler_identity() {
	using namespace TestNativeDisposal;
	RenderingDevice rd, foreign;
	SamplerDisposalDriver driver;
	attach(rd, driver);
	attach(foreign, driver);
	const RenderingDevice::SamplerState sampler_state;
	RID rid;
	NativeDisposalWitness witness;
	REQUIRE(rd._native_disposal_sampler_create(sampler_state, rid, witness) == OK);
	NativeDisposalWitness::Report report;
	CHECK(observe(foreign, witness, report) == ERR_UNAUTHORIZED);
	CHECK(release(foreign, witness) == ERR_UNAUTHORIZED);
	CHECK(state(witness) == NativeDisposalWitness::State::LIVE);
	CHECK(driver.frees == 0);
	// Matching numeric identity is insufficient: the original cell must match.
	NativeDisposalWitness forged;
	forged.cell.instantiate();
	forged.cell->device = witness.cell->device;
	forged.cell->rid = witness.cell->rid;
	forged.cell->serial = witness.cell->serial;
	forged.cell->backing = witness.cell->backing;
	CHECK(release(rd, forged) == ERR_INVALID_DATA);
	CHECK(state(forged) == NativeDisposalWitness::State::UNKNOWN);
	CHECK(state(witness) == NativeDisposalWitness::State::LIVE);
	CHECK(rd.sampler_owner.owns(rid));
	CHECK(driver.frees == 0);
	SUBCASE("original immutable hold defers sampler enqueue") {
		RenderingDevice::NativeImmutableHold hold;
		REQUIRE(rd.native_immutable_hold_acquire(VectorView<RID>(&rid, 1), hold) == OK);
		CHECK(release(rd, witness) == ERR_BUSY);
		CHECK(state(witness) == NativeDisposalWitness::State::LIVE);
		CHECK(rd.native_immutable_resource_is_retiring(rid));
		CHECK(rd.sampler_owner.owns(rid));
		dispose(rd);
		CHECK(driver.frees == 0);
		CHECK(rd.native_immutable_hold_release(hold) == OK);
		CHECK(state(witness) == NativeDisposalWitness::State::ENQUEUED);
		CHECK(driver.frees == 0);
	}
	SUBCASE("owner serial mismatch") {
		auto *owner = rd.sampler_owner.get_or_null(rid);
		++owner->native_disposal_serial;
		CHECK(release(rd, witness) == ERR_INVALID_DATA);
		CHECK(state(witness) == NativeDisposalWitness::State::UNKNOWN);
		CHECK(observe(rd, witness, report) == ERR_INVALID_DATA);
		CHECK(driver.frees == 0);
		CHECK(rd.frames[0].samplers_to_dispose_of.is_empty());
		// Fixture-only repair permits teardown; no production reset is exposed.
		--owner->native_disposal_serial;
		witness.cell->state = NativeDisposalWitness::State::LIVE;
	}
	SUBCASE("exact driver backing mismatch") {
		++witness.cell->backing;
		CHECK(release(rd, witness) == ERR_INVALID_DATA);
		CHECK(state(witness) == NativeDisposalWitness::State::UNKNOWN);
		CHECK(driver.frees == 0);
		CHECK(rd.frames[0].samplers_to_dispose_of.is_empty());
		--witness.cell->backing;
		witness.cell->state = NativeDisposalWitness::State::LIVE;
	}
	driver.free_witnesses.push_back(&witness);
	REQUIRE(release(rd, witness) == OK);
	dispose(rd);
	CHECK(state(witness) == NativeDisposalWitness::State::RETURNED);
	const uint64_t old_serial = witness.cell->serial;
	// A fresh real factory may reuse native id 17; an old receipt cannot free it.
	RID next;
	NativeDisposalWitness next_witness;
	REQUIRE(rd._native_disposal_sampler_create(sampler_state, next, next_witness) == OK);
	CHECK(next_witness.cell->serial != old_serial);
	CHECK(next_witness.cell != witness.cell);
	CHECK(release(rd, witness) == OK);
	CHECK(release(rd, forged) == ERR_INVALID_DATA);
	CHECK(driver.frees == 1);
	CHECK(rd.sampler_owner.owns(next));
	CHECK(state(next_witness) == NativeDisposalWitness::State::LIVE);
	driver.free_witnesses.push_back(&next_witness);
	CHECK(release(rd, next_witness) == OK);
	dispose(rd);
	CHECK(driver.frees == 2);
	CHECK(state(next_witness) == NativeDisposalWitness::State::RETURNED);
	CHECK(rd.sampler_owner.get_rid_count() == 0);
	detach(foreign);
	detach(rd);
}

void TestNativeDisposalAccess::sampler_ordinary() {
	using namespace TestNativeDisposal;
	RenderingDevice rd;
	SamplerDisposalDriver driver;
	attach(rd, driver);
	RenderingDevice::SamplerState sampler_state;
	sampler_state.mag_filter = RenderingDevice::SAMPLER_FILTER_LINEAR;
	const RID ordinary = rd.sampler_create(sampler_state);
	REQUIRE(ordinary.is_valid());
	CHECK(driver.creates == 1);
	CHECK(driver.last_state.mag_filter == RenderingDevice::SAMPLER_FILTER_LINEAR);
	CHECK(rd.sampler_owner.get_or_null(ordinary)->native_disposal.cell.is_null());
	driver.free_witnesses.push_back(nullptr);
	rd.free_rid(ordinary);
	CHECK_FALSE(rd.sampler_owner.owns(ordinary));
	CHECK(driver.frees == 0);
	dispose(rd);
	CHECK(driver.frees == 1);
	driver.fail_create = true;
	const RID failed = rd.sampler_create(sampler_state);
	CHECK(failed.is_null());
	CHECK(driver.creates == 2);
	CHECK(driver.frees == 1);
	CHECK(rd.sampler_owner.get_rid_count() == 0);
	CHECK(rd.frames[0].samplers_to_dispose_of.is_empty());
	detach(rd);
}

void TestNativeDisposalAccess::sampled_disposal() {
	using namespace TestNativeDisposal;
	RenderingDevice rd;
	SampledDepthDriver driver;
	attach(rd, driver);
	RID sampler, texture, framebuffer, set;
	NativeDisposalWitness sampler_witness, texture_witness, framebuffer_witness,
			set_witness;
	REQUIRE(rd._native_disposal_sampler_create(RenderingDevice::SamplerState(),
					sampler, sampler_witness) == OK);
	REQUIRE(rd._native_disposal_depth_texture_create(8, 4, texture,
					texture_witness) == OK);
	Ref<SampledDepthContainer> container;
	container.instantiate();
	container->initialize();
	const RID shader = rd.shader_create_from_bytecode(container->to_bytes());
	REQUIRE(shader.is_valid());
	const uint32_t original_commands =
			rd.draw_graph.qualification_command_count();
	REQUIRE(rd.texture_owner.get_or_null(texture)->pending_clear);
	REQUIRE(rd._native_disposal_depth_sample_set_create(
					sampler_witness, texture_witness, shader, 0, 3, set,
					set_witness) == OK);
	CHECK(driver.set_creates == 1);
	CHECK(rd.draw_graph.qualification_command_count() == original_commands);
	CHECK(rd.texture_owner.get_or_null(texture)->pending_clear);
	REQUIRE(rd._native_disposal_depth_framebuffer_create(
					texture_witness, framebuffer, framebuffer_witness) == OK);
	auto *original_set = rd.uniform_set_owner.get_or_null(set);
	REQUIRE(original_set != nullptr);
	REQUIRE(original_set->pending_clear_textures.size() == 1);
	CHECK(original_set->pending_clear_textures[0] == texture);
	CHECK(original_set->writable_resources.is_empty());
	CHECK(original_set->shared_textures_to_update.is_empty());
	CHECK(driver.clear_calls == 0);
	CHECK(driver.prepare_calls == 0);
	CHECK(rd.dependency_map[shader].has(set));
	CHECK(rd.dependency_map[sampler].has(set));
	CHECK(rd.dependency_map[texture].has(set));
	CHECK(rd.dependency_map[texture].has(framebuffer));
	CHECK(rd.reverse_dependency_map[set].size() == 3);
	driver.sampler_witness = &sampler_witness;
	driver.texture_witness = &texture_witness;
	driver.framebuffer_witness = &framebuffer_witness;
	driver.set_witness = &set_witness;
	struct Callback {
		NativeDisposalWitness *set;
		bool called = false;
	} callback{ &set_witness };
	rd.uniform_set_set_invalidation_callback(
			set,
			[](void *p_data) {
				auto &event = *static_cast<Callback *>(p_data);
				CHECK(state(*event.set) == NativeDisposalWitness::State::ENQUEUED);
				event.called = true;
			},
			&callback);
	bool sampler_first = false, bounded = false;
	SUBCASE("texture cascade with ordinary drain") {}
	SUBCASE("sampler cascade with ordinary drain") {
		sampler_first = true;
	}
	SUBCASE("texture cascade with bounded drain") {
		bounded = true;
	}
	SUBCASE("sampler cascade with bounded drain") {
		sampler_first = true;
		bounded = true;
	}
	if (!sampler_first) {
		REQUIRE(release(rd, texture_witness) == OK);
		CHECK(state(set_witness) == NativeDisposalWitness::State::ENQUEUED);
		CHECK(state(framebuffer_witness) == NativeDisposalWitness::State::ENQUEUED);
		CHECK(state(sampler_witness) == NativeDisposalWitness::State::LIVE);
		CHECK(rd.sampler_owner.owns(sampler));
		REQUIRE(release(rd, sampler_witness) == OK);
	} else {
		REQUIRE(release(rd, sampler_witness) == OK);
		CHECK(state(set_witness) == NativeDisposalWitness::State::ENQUEUED);
		CHECK(state(texture_witness) == NativeDisposalWitness::State::LIVE);
		CHECK(state(framebuffer_witness) == NativeDisposalWitness::State::LIVE);
		CHECK(rd.texture_owner.owns(texture));
		CHECK(rd.framebuffer_owner.owns(framebuffer));
		REQUIRE(release(rd, texture_witness) == OK);
	}
	CHECK(callback.called);
	CHECK(driver.set_frees == 0);
	CHECK(driver.sampler_frees == 0);
	CHECK(driver.texture_frees == 0);
	if (!bounded) {
		dispose(rd);
	} else {
		uint32_t used = 0;
		CHECK(step(rd, 1, 1, UINT64_MAX, used) == ERR_BUSY);
		CHECK(used == 1);
		CHECK(driver.set_frees == 1);
		CHECK(state(set_witness) == NativeDisposalWitness::State::RETURNED);
		CHECK(state(sampler_witness) == NativeDisposalWitness::State::ENQUEUED);
		CHECK(state(texture_witness) == NativeDisposalWitness::State::ENQUEUED);
		for (uint32_t turn = 0; turn < 16 && draining(rd); ++turn) {
			const Error result = step(rd, 1, 1, UINT64_MAX, used);
			if (result != OK) {
				CHECK(result == ERR_BUSY);
			}
		}
		CHECK_FALSE(draining(rd));
	}
	CHECK(driver.set_frees == 1);
	CHECK(driver.sampler_frees == 1);
	CHECK(driver.texture_frees == 1);
	CHECK(state(set_witness) == NativeDisposalWitness::State::RETURNED);
	CHECK(state(sampler_witness) == NativeDisposalWitness::State::RETURNED);
	CHECK(state(framebuffer_witness) == NativeDisposalWitness::State::RETURNED);
	CHECK(state(texture_witness) == NativeDisposalWitness::State::RETURNED);
	CHECK(refs(set_witness) == 1);
	CHECK(refs(sampler_witness) == 1);
	CHECK(refs(texture_witness) == 1);
	rd.free_rid(shader);
	dispose(rd);
	CHECK(driver.shader_frees == 1);
	CHECK(rd.dependency_map.is_empty());
	CHECK(rd.reverse_dependency_map.is_empty());
	for (const auto &entry : rd.framebuffer_formats) {
		driver.render_pass_free(entry.value.render_pass);
	}
	rd.framebuffer_formats.clear();
	rd.framebuffer_format_cache.clear();
	detach(rd);
}

void TestNativeDisposalAccess::sampled_failure() {
	using namespace TestNativeDisposal;
	RenderingDevice rd;
	SampledDepthDriver driver;
	attach(rd, driver);
	RID sampler, texture, framebuffer, set;
	NativeDisposalWitness sampler_witness, texture_witness, framebuffer_witness,
			set_witness;
	REQUIRE(rd._native_disposal_sampler_create(RenderingDevice::SamplerState(),
					sampler, sampler_witness) == OK);
	REQUIRE(rd._native_disposal_depth_texture_create(8, 4, texture,
					texture_witness) == OK);
	REQUIRE(rd._native_disposal_depth_framebuffer_create(
					texture_witness, framebuffer, framebuffer_witness) == OK);
	Ref<SampledDepthContainer> container;
	container.instantiate();
	container->initialize();
	const RID shader = rd.shader_create_from_bytecode(container->to_bytes());
	REQUIRE(shader.is_valid());
	driver.fail_set = true;
	const uint32_t commands = rd.draw_graph.qualification_command_count();
	ERR_PRINT_OFF;
	CHECK(rd._native_disposal_depth_sample_set_create(
				  sampler_witness, texture_witness, shader, 0, 3, set, set_witness) ==
			ERR_CANT_CREATE);
	ERR_PRINT_ON;
	CHECK(set.is_null());
	CHECK(set_witness.cell.is_null());
	CHECK(driver.set_creates == 1);
	CHECK(driver.set_frees == 0);
	CHECK(rd.uniform_set_owner.get_rid_count() == 0);
	CHECK(rd.frames[0].uniform_sets_to_dispose_of.is_empty());
	CHECK(state(sampler_witness) == NativeDisposalWitness::State::LIVE);
	CHECK(state(texture_witness) == NativeDisposalWitness::State::LIVE);
	CHECK(state(framebuffer_witness) == NativeDisposalWitness::State::LIVE);
	CHECK(rd.texture_owner.get_or_null(texture)->pending_clear);
	CHECK(rd.draw_graph.qualification_command_count() == commands);
	CHECK_FALSE(rd.dependency_map.has(sampler));
	CHECK_FALSE(rd.dependency_map.has(shader));
	CHECK(rd.dependency_map[texture].size() == 1); // Only original framebuffer.
	CHECK(driver.clear_calls == 0);
	CHECK(driver.prepare_calls == 0);
	driver.sampler_witness = &sampler_witness;
	driver.texture_witness = &texture_witness;
	driver.framebuffer_witness = &framebuffer_witness;
	CHECK(release(rd, texture_witness) == OK);
	CHECK(state(sampler_witness) == NativeDisposalWitness::State::LIVE);
	CHECK(release(rd, sampler_witness) == OK);
	dispose(rd);
	CHECK(driver.texture_frees == 1);
	CHECK(driver.sampler_frees == 1);
	CHECK(state(framebuffer_witness) == NativeDisposalWitness::State::RETURNED);
	rd.free_rid(shader);
	dispose(rd);
	CHECK(driver.shader_frees == 1);
	CHECK(rd.dependency_map.is_empty());
	CHECK(rd.reverse_dependency_map.is_empty());
	for (const auto &entry : rd.framebuffer_formats) {
		driver.render_pass_free(entry.value.render_pass);
	}
	rd.framebuffer_formats.clear();
	rd.framebuffer_format_cache.clear();
	detach(rd);
}

void TestNativeDisposalAccess::sampled_identity() {
	using namespace TestNativeDisposal;
	RenderingDevice rd, foreign;
	SampledDepthDriver driver;
	attach(rd, driver);
	attach(foreign, driver);
	RID sampler, texture, set;
	NativeDisposalWitness sampler_witness, texture_witness, set_witness;
	REQUIRE(rd._native_disposal_sampler_create(RenderingDevice::SamplerState(),
					sampler, sampler_witness) == OK);
	REQUIRE(rd._native_disposal_depth_texture_create(8, 4, texture,
					texture_witness) == OK);
	Ref<SampledDepthContainer> container;
	container.instantiate();
	uint32_t reflected_length = 1;
	bool writable = false;
	SUBCASE("reflected sampler array unavailable") {
		reflected_length = 2;
	}
	SUBCASE("reflected writable descriptor unavailable") {
		writable = true;
	}
	container->initialize(reflected_length, writable);
	const RID shader = rd.shader_create_from_bytecode(container->to_bytes());
	REQUIRE(shader.is_valid());
	const uint32_t commands = rd.draw_graph.qualification_command_count();
	CHECK(foreign._native_disposal_depth_sample_set_create(
				  sampler_witness, texture_witness, shader, 0, 3, set, set_witness) ==
			ERR_UNAUTHORIZED);
	NativeDisposalWitness forged;
	forged.cell.instantiate();
	forged.cell->device = sampler_witness.cell->device;
	forged.cell->rid = sampler_witness.cell->rid;
	forged.cell->serial = sampler_witness.cell->serial;
	forged.cell->backing = sampler_witness.cell->backing;
	CHECK(rd._native_disposal_depth_sample_set_create(
				  forged, texture_witness, shader, 0, 3, set, set_witness) ==
			ERR_INVALID_DATA);
	CHECK(rd._native_disposal_depth_sample_set_create(
				  sampler_witness, texture_witness, shader, 0, 4, set, set_witness) ==
			ERR_UNAVAILABLE);
	CHECK(rd._native_disposal_depth_sample_set_create(
				  sampler_witness, texture_witness, shader, 0, 3, set, set_witness) ==
			ERR_UNAVAILABLE);
	CHECK(driver.set_creates == 0);
	CHECK(set.is_null());
	CHECK(set_witness.cell.is_null());
	CHECK(rd.uniform_set_owner.get_rid_count() == 0);
	CHECK(rd.texture_owner.get_or_null(texture)->pending_clear);
	CHECK(rd.draw_graph.qualification_command_count() == commands);
	CHECK(state(sampler_witness) == NativeDisposalWitness::State::LIVE);
	CHECK(state(texture_witness) == NativeDisposalWitness::State::LIVE);
	CHECK(driver.clear_calls == 0);
	CHECK(driver.prepare_calls == 0);
	driver.sampler_witness = &sampler_witness;
	driver.texture_witness = &texture_witness;
	CHECK(release(rd, sampler_witness) == OK);
	CHECK(rd._native_disposal_depth_sample_set_create(
				  sampler_witness, texture_witness, shader, 0, 3, set, set_witness) ==
			ERR_INVALID_DATA);
	CHECK(release(rd, texture_witness) == OK);
	dispose(rd);
	rd.free_rid(shader);
	dispose(rd);
	CHECK(driver.sampler_frees == 1);
	CHECK(driver.texture_frees == 1);
	CHECK(driver.set_frees == 0);
	CHECK(rd.dependency_map.is_empty());
	CHECK(rd.reverse_dependency_map.is_empty());
	detach(foreign);
	detach(rd);
}
