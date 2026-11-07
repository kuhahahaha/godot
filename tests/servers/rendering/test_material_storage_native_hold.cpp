/**************************************************************************/
/*  test_material_storage_native_hold.cpp                                 */
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
#include "servers/rendering/renderer_rd/shader_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/material_storage.h"
#include "servers/rendering/rendering_shader_container.h"
#include "tests/servers/rendering/test_external_output_custody_bridge.h"
#include "tests/servers/rendering/transfer_test_driver_base.h"
#include "tests/test_macros.h"

TEST_FORCE_LINK(test_material_storage_native_hold)

TEST_CASE("[MaterialNativeHold] inactive hold cannot release or expose native inputs") {
	RendererRD::MaterialStorage::NativeMaterialHold hold;
	CHECK_FALSE(hold.is_active());
	CHECK_FALSE(hold.is_valid());
	CHECK(hold.get_snapshot().shader.is_null());
	CHECK(hold.get_snapshot().uniform_bytes.is_empty());
	CHECK(RendererRD::MaterialStorage::material_native_hold_release(hold) == ERR_INVALID_PARAMETER);
}

// Original Core/RID metadata enrollment with deterministic CPU native objects.
// This does not run a shader compiler, publish a packet, or grant Runtime work.
class MaterialHoldShaderContainer : public RenderingShaderContainer {
protected:
	uint32_t _format() const override { return 0x4d484f4c; }
	uint32_t _format_version() const override { return 1; }
	bool _set_code_from_spirv(const ReflectShader &) override { return false; }

public:
	void initialize(uint32_t ubo_bytes, bool embedded_sampler = false) {
		reflection_data.pipeline_type = RenderingDeviceCommons::PIPELINE_TYPE_COMPUTE;
		reflection_data.stage_count = 1;
		reflection_data.set_count = 1;
		reflection_data.compute_local_size[0] = reflection_data.compute_local_size[1] = reflection_data.compute_local_size[2] = 1;
		reflection_shader_stages.push_back(RenderingDeviceCommons::SHADER_STAGE_COMPUTE);
		reflection_binding_set_uniforms_count.push_back(embedded_sampler ? 3 : 2);
		ReflectionBindingData ubo;
		ubo.type = RenderingDeviceCommons::UNIFORM_TYPE_UNIFORM_BUFFER;
		ubo.binding = 0;
		ubo.length = ubo_bytes;
		ubo.stages = 1u << RenderingDeviceCommons::SHADER_STAGE_COMPUTE;
		reflection_binding_set_uniforms_data.push_back(ubo);
		ReflectionBindingData texture;
		texture.type = RenderingDeviceCommons::UNIFORM_TYPE_TEXTURE;
		texture.binding = 1;
		texture.length = 1;
		texture.stages = ubo.stages;
		reflection_binding_set_uniforms_data.push_back(texture);
		if (embedded_sampler) {
			ReflectionBindingData sampler;
			sampler.type = RenderingDeviceCommons::UNIFORM_TYPE_SAMPLER;
			sampler.binding = 2;
			sampler.length = 1;
			sampler.stages = ubo.stages;
			reflection_binding_set_uniforms_data.push_back(sampler);
		}
		Shader shader;
		shader.shader_stage = RenderingDeviceCommons::SHADER_STAGE_COMPUTE;
		shader.code_compressed_bytes.push_back(42);
		shaders.push_back(shader);
	}
};
class MaterialHoldShaderFormat : public RenderingShaderContainerFormat {
public:
	Ref<RenderingShaderContainer> create_container() const override { return memnew(MaterialHoldShaderContainer); }
	ShaderLanguageVersion get_shader_language_version() const override { return SHADER_LANGUAGE_VULKAN_VERSION_1_1; }
	ShaderSpirvVersion get_shader_spirv_version() const override { return SHADER_SPIRV_VERSION_1_3; }
};

class TestMaterialStorageNativeHoldDeviceAccess {
public:
	static Error capture_pipeline_profile(GDExtensionProductionPhysicalContextRefV1 context, GDExtensionDeviceExit::ProductionOutputPin &pin, RenderingDevice::NativeImmutableHold &hold, uint64_t &group) {
		return RenderingDevice::get_singleton()->_production_output_hold_capture(context, pin, hold, group);
	}
#ifdef THREADS_ENABLED
	struct MutexProbe {
		RenderingDevice *device = nullptr;
		bool acquired = false;
	};
	static void mutex_probe_run(void *data) {
		auto &probe = *static_cast<MutexProbe *>(data);
		probe.acquired = probe.device->_thread_safe_.try_lock();
		if (probe.acquired) {
			probe.device->_thread_safe_.unlock();
		}
	}
	static bool mutex_available_on_worker(RenderingDevice &device) {
		MutexProbe probe{ &device };
		Thread worker;
		const Thread::ID started = worker.start(&mutex_probe_run, &probe);
		CHECK(started != 0);
		if (started == 0) {
			return false;
		}
		worker.wait_to_finish();
		return probe.acquired;
	}
	static void check_mutex_probe_control(RenderingDevice &device) {
		CHECK(mutex_available_on_worker(device));
		device._thread_safe_.lock();
		CHECK_FALSE(mutex_available_on_worker(device));
		device._thread_safe_.unlock();
	}
#endif
	static void prepare(RenderingDevice &rd, RenderingDeviceDriver &driver) {
		rd.singleton = &rd;
		rd.driver = &driver;
		rd.render_thread_id = Thread::get_caller_id();
		rd.frames.resize(1);
		rd.frame = 0;
		rd.pipeline_cache_enabled = false;
		rd.draw_graph.initialize(&driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
		rd.draw_graph.begin();
	}
	static RID shader(RenderingDevice &rd, uint32_t ubo_bytes = 16, RID immutable_sampler = RID()) {
		Ref<MaterialHoldShaderContainer> container;
		container.instantiate();
		container->initialize(ubo_bytes, immutable_sampler.is_valid());
		Vector<RenderingDevice::PipelineImmutableSampler> samplers;
		if (immutable_sampler.is_valid()) {
			RenderingDevice::PipelineImmutableSampler uniform;
			uniform.uniform_type = RenderingDevice::UNIFORM_TYPE_SAMPLER;
			uniform.binding = 2;
			uniform.append_id(immutable_sampler);
			samplers.push_back(uniform);
		}
		return rd.shader_create_from_bytecode_with_samplers(container->to_bytes(), RID(), samplers);
	}
	static bool callback_origin_matches(RenderingDevice &rd, RID set, RID material, uint64_t generation) {
		const auto *actual = rd.uniform_set_owner.get_or_null(set);
		return actual && actual->production_material_origin == material && actual->production_material_origin_generation == generation;
	}
	static bool callback_origin_empty(RenderingDevice &rd, RID set) {
		const auto *actual = rd.uniform_set_owner.get_or_null(set);
		return actual && actual->production_material_origin.is_null() && actual->production_material_origin_generation == 0;
	}
	static bool callback_matches(RenderingDevice &rd, RID set, void *userdata) {
		const auto *uniform_set = rd.uniform_set_owner.get_or_null(set);
		return uniform_set && uniform_set->invalidated_callback && uniform_set->invalidated_callback_userdata == userdata;
	}
	static bool callback_cleared(RenderingDevice &rd, RID set) {
		const auto *uniform_set = rd.uniform_set_owner.get_or_null(set);
		return uniform_set && !uniform_set->invalidated_callback && !uniform_set->invalidated_callback_userdata;
	}

	static RID texture(RenderingDevice &rd) {
		RenderingDevice::Texture texture;
		texture.driver_id = RenderingDeviceDriver::TextureID(84);
		texture.width = texture.height = texture.depth = texture.layers = texture.mipmaps = 1;
		texture.format = RenderingDevice::DATA_FORMAT_R8_UNORM;
		texture.type = RenderingDevice::TEXTURE_TYPE_2D;
		texture.usage_flags = RenderingDevice::TEXTURE_USAGE_SAMPLING_BIT;
		return rd.texture_owner.make_rid(texture);
	}
	static Error begin_finalization(RenderingDevice &rd) { return rd._begin_production_finalization(); }
	static uint32_t native_holds(RenderingDevice &rd) { return rd.native_immutable_hold_count; }
	static bool finalization_returned(RenderingDevice &rd) { return rd.native_finalization_returned; }
	static bool finalization_started(RenderingDevice &rd) { return rd.production_finalization_started.load(std::memory_order_acquire); }
	static bool sampler_exists(RenderingDevice &rd, RID sampler) { return rd.sampler_owner.owns(sampler); }
	static uint32_t captured_sampler_count(RenderingDevice &rd, RID shader) { return rd.shader_owner.get_or_null(shader)->native_immutable_samplers.size(); }
	static bool captured_sampler(RenderingDevice &rd, RID shader, RID sampler) { return rd.shader_owner.get_or_null(shader)->native_immutable_samplers.has(sampler); }
	static bool buffer_exists(RenderingDevice &rd, RID buffer) { return rd.uniform_buffer_owner.owns(buffer); }
	static bool set_uses_buffer(RenderingDevice &rd, RID set, RID buffer) {
		const auto *deps = rd.reverse_dependency_map.getptr(set);
		return deps && deps->has(buffer);
	}
	static bool set_exists(RenderingDevice &rd, RID set) { return rd.uniform_set_owner.owns(set); }
	static bool free_in_progress(RenderingDevice &rd, RID set) { return rd.resource_frees_in_progress.has(set); }
	static bool set_dependencies_cleared(RenderingDevice &rd, RID set) {
		if (rd.reverse_dependency_map.has(set)) {
			return false;
		}
		for (const auto &entry : rd.dependency_map) {
			if (entry.value.has(set)) {
				return false;
			}
		}
		return true;
	}
	static void mark_pending_texture_worker(RenderingDevice &rd, RID texture) {
		CHECK(rd.transfer_worker_pool.is_empty());
		auto *worker = memnew(RenderingDevice::TransferWorker);
		worker->index = 0;
		rd.transfer_worker_pool.push_back(worker);
		rd.transfer_worker_operation_used_by_draw.push_back(0);
		auto *original = rd.texture_owner.get_or_null(texture);
		original->transfer_worker_index = 0;
		original->transfer_worker_operation = 1;
	}
	static void drain(RenderingDevice &rd) { rd._free_pending_resources(0); }
	static bool empty(RenderingDevice &rd) { return rd.dependency_map.is_empty() && rd.reverse_dependency_map.is_empty(); }
	static void restore(RenderingDevice *previous) { RenderingDevice::singleton = previous; }
	static void detach(RenderingDevice &rd) {
		for (auto *worker : rd.transfer_worker_pool) {
			memdelete(worker);
		}
		rd.transfer_worker_pool.clear();
		rd.transfer_worker_operation_used_by_draw.clear();
		rd.draw_graph.finalize();
		rd.frames.clear();
		rd.driver = nullptr;
	}
};

namespace RendererRD {
class TestMaterialStorageNativeHoldAccess {
	using RD = RenderingDevice;
	using RDD = RenderingDeviceDriver;
	class Driver : public TransferTestDriverBase {
	public:
		uint32_t buffer_frees = 0, set_frees = 0, texture_frees = 0, sampler_frees = 0, shader_frees = 0, pipeline_frees = 0;
		HashMap<uint64_t, String> names;
		uint32_t set_creates = 0, pipeline_creates = 0, shader_creates = 0, sampler_creates = 0, immutable_sampler_inputs = 0;
		MaterialHoldShaderFormat shader_format;
		HashSet<uint64_t> freed_shaders, freed_pipelines;
		const RenderingShaderContainerFormat &get_shader_container_format() const override { return shader_format; }
		ShaderID shader_create_from_container(const Ref<RenderingShaderContainer> &container, const Vector<ImmutableSampler> &samplers) override {
			const auto &sets = container->get_shader_reflection().uniform_sets;
			CHECK(sets.size() == 1);
			if (sets.size() != 1 || sets[0].size() != (samplers.is_empty() ? 2 : 3)) {
				return {};
			}
			if (!samplers.is_empty()) {
				CHECK(samplers.size() == 1);
				CHECK(samplers[0].type == UNIFORM_TYPE_SAMPLER);
				CHECK(samplers[0].binding == 2);
				CHECK(samplers[0].ids.size() == 1);
				if (samplers[0].ids.size() != 1) {
					return {};
				}
				CHECK(samplers[0].ids[0].id == 83);
				++immutable_sampler_inputs;
				CHECK(sets[0][2].type == UNIFORM_TYPE_SAMPLER);
				CHECK(sets[0][2].binding == 2);
			}
			const uint32_t ubo_bytes = sets[0][0].length;
			++shader_creates;
			if (shader_creates == 1) {
				CHECK(ubo_bytes == 16);
				return ShaderID(80);
			}
			if (shader_creates == 2) {
				CHECK(ubo_bytes == 32);
				return ShaderID(86);
			}
			CHECK(shader_creates == 3);
			CHECK(ubo_bytes == 16);
			return ShaderID(88);
		}
		uint32_t shader_get_layout_hash(ShaderID id) override {
			CHECK((id == ShaderID(80) || id == ShaderID(86) || id == ShaderID(88)));
			return id == ShaderID(86) ? 102 : 101;
		}
		uint8_t bytes[16] = {};
		BufferID buffer_create(uint64_t size, BitField<BufferUsageBits> usage, MemoryAllocationType, uint64_t) override {
			CHECK(size == 16);
			CHECK(usage.has_flag(BUFFER_USAGE_UNIFORM_BIT));
			for (uint8_t &byte : bytes) {
				byte = 42;
			}
			return BufferID(1);
		}
		void buffer_free(BufferID id) override {
			CHECK(id == BufferID(1));
			CHECK(buffer_frees++ == 0);
		}
		PipelineID compute_pipeline_create(ShaderID id, VectorView<PipelineSpecializationConstant>) override {
			CHECK((id == ShaderID(80) || id == ShaderID(86) || id == ShaderID(88)));
			++pipeline_creates;
			return PipelineID(id.id + 1);
		}

		UniformSetID uniform_set_create(VectorView<BoundUniform> uniforms, ShaderID shader, uint32_t set, int) override {
			CHECK(shader == ShaderID(80));
			CHECK(set == 0);
			CHECK(uniforms.size() == 2);
			if (uniforms.size() != 2) {
				return {};
			}
			CHECK(uniforms[0].type == UNIFORM_TYPE_UNIFORM_BUFFER);
			CHECK(uniforms[0].ids.size() == 1);
			if (uniforms[0].ids.size() != 1 || uniforms[1].ids.size() != 1) {
				return {};
			}
			CHECK(uniforms[0].ids[0].id == 1);
			CHECK(uniforms[1].type == UNIFORM_TYPE_TEXTURE);
			CHECK(uniforms[1].ids[0].id == 85);
			CHECK(set_creates++ == 0);
			return UniformSetID(82);
		}
		SamplerID sampler_create(const SamplerState &) override {
			CHECK(sampler_creates++ == 0);
			return SamplerID(83);
		}
		void uniform_set_free(UniformSetID id) override {
			CHECK(id == UniformSetID(82));
			CHECK(set_frees++ == 0);
		}
		uint32_t texture_creates = 0, native_exports = 0;
		TextureID texture_create(const TextureFormat &format, const TextureView &view) override {
			CHECK(texture_creates++ == 0);
			CHECK(format.width == 1);
			CHECK(format.height == 1);
			CHECK(format.array_layers == 1);
			CHECK(format.mipmaps == 1);
			CHECK(format.samples == TEXTURE_SAMPLES_1);
			CHECK((format.format == DATA_FORMAT_R8_UNORM || format.format == DATA_FORMAT_D32_SFLOAT));
			CHECK(view.format == format.format);
			return TextureID(84);
		}
		BitField<TextureUsageBits> texture_get_usages_supported_by_format(DataFormat format, bool readable) override {
			CHECK((format == DATA_FORMAT_R8_UNORM || format == DATA_FORMAT_D32_SFLOAT));
			CHECK_FALSE(readable);
			return TEXTURE_USAGE_SAMPLING_BIT | TEXTURE_USAGE_CAN_UPDATE_BIT | TEXTURE_USAGE_DEPTH_RESOLVE_ATTACHMENT_BIT;
		}
		uint64_t get_resource_native_handle(DriverResource type, ID id) override {
			const bool known_resource = ((type == DRIVER_RESOURCE_TEXTURE || type == DRIVER_RESOURCE_TEXTURE_VIEW) && (id.id == 84 || id.id == 85 || id.id == 86)) || (type == DRIVER_RESOURCE_SAMPLER && id.id == 83);
			CHECK(known_resource);
			++native_exports;
			return 0x10000 + id.id;
		}
		uint32_t views = 0;
		bool freed_parent = false, freed_view = false, freed_sibling = false;
		TextureID texture_create_shared(TextureID parent, const TextureView &) override {
			CHECK(parent == TextureID(84));
			CHECK(views < 2);
			return TextureID(85 + views++);
		}
		uint64_t texture_get_allocation_size(TextureID id) override {
			CHECK((id == TextureID(84) || id == TextureID(85) || id == TextureID(86)));
			return 0;
		}
		bool texture_can_make_shared_with_format(TextureID id, DataFormat, bool &raw) override {
			CHECK(id == TextureID(84));
			raw = false;
			return true;
		}
		void texture_free(TextureID id) override {
			CHECK((id == TextureID(84) || id == TextureID(85) || id == TextureID(86)));
			bool &freed = id == TextureID(84) ? freed_parent : id == TextureID(85) ? freed_view
																				   : freed_sibling;
			CHECK_FALSE(freed);
			freed = true;
			++texture_frees;
		}
		void sampler_free(SamplerID id) override {
			CHECK(id == SamplerID(83));
			CHECK(sampler_frees++ == 0);
		}
		void shader_free(ShaderID id) override {
			CHECK((id == ShaderID(80) || id == ShaderID(86) || id == ShaderID(88)));
			CHECK_FALSE(freed_shaders.has(id.id));
			freed_shaders.insert(id.id);
			++shader_frees;
		}
		void pipeline_free(PipelineID id) override {
			CHECK((id == PipelineID(81) || id == PipelineID(87) || id == PipelineID(89)));
			CHECK_FALSE(freed_pipelines.has(id.id));
			freed_pipelines.insert(id.id);
			++pipeline_frees;
		}
		bool shader_retire_ready_nonblocking(ShaderID id) const override {
			CHECK((id == ShaderID(80) || id == ShaderID(86) || id == ShaderID(88)));
			return true;
		}

		uint64_t api_trait_get(ApiTrait trait) override {
			if (trait == API_TRAIT_HONORS_PIPELINE_BARRIERS) {
				return 0;
			}
			if (trait == API_TRAIT_SHADER_CHANGE_INVALIDATION) {
				return SHADER_CHANGE_INVALIDATION_ALL_BOUND_UNIFORM_SETS;
			}
			if (trait == API_TRAIT_BUFFERS_REQUIRE_TRANSITIONS || trait == API_TRAIT_BUFFER_CLEARS_WITH_COPY_ENGINE || trait == API_TRAIT_TEXTURE_OUTPUTS_REQUIRE_CLEARS) {
				return 0;
			}
			FAIL_CHECK("Unexpected material hold CPU driver trait: ", uint32_t(trait));
			return 0;
		}
		uint64_t limit_get(Limit limit) override {
			CHECK((limit == LIMIT_MAX_UNIFORM_BUFFER_SIZE || limit == LIMIT_MAX_TEXTURE_SIZE_2D || limit == LIMIT_MAX_TEXTURE_ARRAY_LAYERS));
			return limit == LIMIT_MAX_UNIFORM_BUFFER_SIZE ? 65536 : 64;
		}
		void set_object_name(ObjectType type, ID id, const String &name) override {
			CHECK(((type == OBJECT_TYPE_BUFFER && id.id == 1) || (type == OBJECT_TYPE_UNIFORM_SET && id.id == 82) || (type == OBJECT_TYPE_TEXTURE && (id.id == 84 || id.id == 85 || id.id == 86)) || (type == OBJECT_TYPE_PIPELINE && (id.id == 81 || id.id == 87 || id.id == 89)) || (type == OBJECT_TYPE_SHADER && (id.id == 80 || id.id == 86 || id.id == 88)) || (type == OBJECT_TYPE_SAMPLER && id.id == 83)));
			CHECK_FALSE(names.has(id.id));
			names.insert(id.id, name);
			CHECK(name.begins_with("RID:"));
		}
	};
	class ShaderOwner : public ShaderRD {};
	class ShaderData : public MaterialStorage::ShaderData {
	public:
		ShaderRD *owner = nullptr;
		RID version;
		uint32_t changes = 0;
		void set_code(const String &) override { ++changes; }
		bool is_animated() const override { return false; }
		bool casts_shadows() const override { return true; }
		RenderingServerTypes::ShaderNativeSourceCode get_native_source_code() const override { return {}; }
		Pair<ShaderRD *, RID> get_native_shader_and_version() const override { return { owner, version }; }
		~ShaderData() { owner->version_free(version); }
	};
	class MaterialData : public MaterialStorage::MaterialData {
	public:
		RID shader, set;
		uint32_t parameter_updates = 0;
		Vector<ShaderCompiler::GeneratedCode::Texture> reflected_textures;
		HashMap<StringName, ShaderLanguage::ShaderNode::Uniform> reflected_uniforms;
		void set_render_priority(int) override {}
		void set_next_pass(RID) override {}
		bool update_parameters(const HashMap<StringName, Variant> &parameters, bool, bool) override {
			++parameter_updates;
			return update_parameters_uniform_set(parameters, false, false, reflected_uniforms, nullptr, reflected_textures, {}, 16, set, shader, 0, false, false);
		}
		~MaterialData() { free_parameters_uniform_set(set); }
	};
#ifdef THREADS_ENABLED
	struct WrongThread {
		MaterialStorage *storage;
		RID material, shader, pipeline, set;
		Error error = OK;
		static void run(void *userdata) {
			auto &test = *static_cast<WrongThread *>(userdata);
			MaterialStorage::NativeMaterialHold hold;
			test.error = test.storage->material_native_hold_acquire(test.material, test.shader, test.pipeline, test.set, hold);
			CHECK_FALSE(hold.is_active());
		}
	};
#endif
	struct CallbackTrace {
		uint32_t calls = 0;
		bool alive = true;
	};
	struct CallbackOwner {
		CallbackTrace *trace;
		RD *device;
		CallbackOwner(CallbackTrace &p_trace, RD &p_device) : trace(&p_trace), device(&p_device) {}
		~CallbackOwner() { trace->alive = false; }
	};
	static void invalidated_probe(void *userdata) {
		auto *owner = static_cast<CallbackOwner *>(userdata);
		CHECK(owner->trace->alive);
#ifdef THREADS_ENABLED
		CHECK(TestMaterialStorageNativeHoldDeviceAccess::mutex_available_on_worker(*owner->device));
#endif
		++owner->trace->calls;
	}
	struct LastReturnTrace {
		uint32_t calls = 0;
		Error returned = ERR_UNCONFIGURED, finalization = ERR_UNCONFIGURED;
		bool destroyed = false, zero_holds = false, logical_set_present = false, admissions_sealed = false, device_closed = false;
		RID uniform_buffer;
		bool material_removed = false, nested_free_scope = false, buffer_still_present = false, buffer_retiring = false;
	};
	struct LastReturnOwner {
		LastReturnTrace *trace;
		RD *device;
		MaterialStorage *storage;
		RID material, set;
		bool retire_material = false;
		MaterialStorage::NativeMaterialHold hold;
		LastReturnOwner(LastReturnTrace &p_trace, RD &p_device, MaterialStorage &p_storage, RID p_material, RID p_set) : trace(&p_trace), device(&p_device), storage(&p_storage), material(p_material), set(p_set) {}
		~LastReturnOwner() {
			if (retire_material) {
				trace->nested_free_scope = TestMaterialStorageNativeHoldDeviceAccess::free_in_progress(*device, set);
				// Original MaterialData teardown owns the actual bound UBO free.
				storage->material_free(material);
				trace->material_removed = !storage->owns_material(material);
				trace->buffer_still_present = TestMaterialStorageNativeHoldDeviceAccess::buffer_exists(*device, trace->uniform_buffer);
				trace->buffer_retiring = device->native_immutable_resource_is_retiring(trace->uniform_buffer);
			}
			trace->destroyed = true;
		}
	};
	static void last_return_probe(void *userdata) {
		auto *packet = static_cast<LastReturnOwner *>(userdata);
		auto *trace = packet->trace;
		++trace->calls;
#ifdef THREADS_ENABLED
		CHECK(TestMaterialStorageNativeHoldDeviceAccess::mutex_available_on_worker(*packet->device));
#endif
		CHECK(packet->hold.is_valid());
		CHECK(TestMaterialStorageNativeHoldDeviceAccess::set_uses_buffer(*packet->device, packet->set, trace->uniform_buffer));
		trace->returned = MaterialStorage::material_native_hold_release(packet->hold);
		CHECK(trace->returned == OK);
		if (trace->returned != OK) {
			return;
		} // The enclosing fixture still owns this packet/token for an actual retry.
		trace->zero_holds = TestMaterialStorageNativeHoldDeviceAccess::native_holds(*packet->device) == 0;
		trace->logical_set_present = TestMaterialStorageNativeHoldDeviceAccess::set_exists(*packet->device, packet->set);
		CHECK(trace->zero_holds);
		CHECK(TestMaterialStorageNativeHoldDeviceAccess::free_in_progress(*packet->device, packet->set));
		CHECK(trace->logical_set_present); // Only pins/metadata returned; the outer free still owns the RID.
		trace->finalization = TestMaterialStorageNativeHoldDeviceAccess::begin_finalization(*packet->device);
		CHECK(trace->finalization == ERR_BUSY); // free-in-progress, with no remaining immutable hold.
		trace->admissions_sealed = TestMaterialStorageNativeHoldDeviceAccess::finalization_started(*packet->device);
		trace->device_closed = TestMaterialStorageNativeHoldDeviceAccess::finalization_returned(*packet->device);
		CHECK(trace->admissions_sealed); // An actual close preflight seals even when it returns Busy.
		CHECK_FALSE(trace->device_closed);
		memdelete(packet); // Real userdata/packet destruction retires its original material/UBO inside this callback.
		CHECK(trace->nested_free_scope);
		CHECK(trace->material_removed);
		CHECK(trace->buffer_still_present);
		CHECK(trace->buffer_retiring);
	}
	static void last_return_from_callback(MaterialStorage &storage, RD &rd, Driver &driver, RID material, RID shader, RID pipeline, RID set) {
#ifdef THREADS_ENABLED
		TestMaterialStorageNativeHoldDeviceAccess::check_mutex_probe_control(rd);
#endif
		LastReturnTrace trace;
		auto *packet = memnew(LastReturnOwner(trace, rd, storage, material, set));
		const Error acquired = storage.material_native_hold_acquire(material, shader, pipeline, set, packet->hold);
		CHECK(acquired == OK);
		if (acquired != OK) {
			memdelete(packet);
			return;
		}
		trace.uniform_buffer = packet->hold.get_snapshot().uniform_buffer;
		packet->retire_material = true;
		CHECK(trace.uniform_buffer.is_valid());
		CHECK(TestMaterialStorageNativeHoldDeviceAccess::set_uses_buffer(rd, set, trace.uniform_buffer));
		CHECK(TestMaterialStorageNativeHoldDeviceAccess::native_holds(rd) == 1);
		rd.uniform_set_set_invalidation_callback(set, &last_return_probe, packet);
		rd.free_rid(set); // The callback itself must return the last hold; no owner-side service turn.
		CHECK(trace.calls == 1);
		CHECK(trace.returned == OK);
		CHECK(trace.zero_holds);
		CHECK(trace.finalization == ERR_BUSY);
		CHECK(trace.admissions_sealed);
		CHECK_FALSE(trace.device_closed);
		CHECK(trace.destroyed);
		if (!trace.destroyed) {
			// Diagnose first, then preserve and actually retry the same token after
			// the outer scope. Never destroy a packet holding a Busy return.
			if (TestMaterialStorageNativeHoldDeviceAccess::set_exists(rd, set)) {
				rd.uniform_set_set_invalidation_callback(set, nullptr, nullptr);
			}
			const Error retried = MaterialStorage::material_native_hold_release(packet->hold);
			CHECK(retried == OK);
			CRASH_COND_MSG(retried != OK, "The failing fixture still owns its native return token.");
			memdelete(packet);
		}
		CHECK_FALSE(TestMaterialStorageNativeHoldDeviceAccess::set_exists(rd, set));
		CHECK_FALSE(TestMaterialStorageNativeHoldDeviceAccess::buffer_exists(rd, trace.uniform_buffer));
		CHECK(TestMaterialStorageNativeHoldDeviceAccess::set_dependencies_cleared(rd, set));
		CHECK(TestMaterialStorageNativeHoldDeviceAccess::set_dependencies_cleared(rd, trace.uniform_buffer));
		CHECK(TestMaterialStorageNativeHoldDeviceAccess::native_holds(rd) == 0);
		CHECK(driver.set_frees == 0); // Logical outer free only queued its original disposer.
		CHECK(driver.buffer_frees == 0);
		TestMaterialStorageNativeHoldDeviceAccess::drain(rd);
		CHECK(driver.buffer_frees == 1);
		CHECK(driver.set_frees == 1); // Actual native free, independently of callback/metadata return.
		CHECK(trace.calls == 1);
	}
	static Error physical_hold(RD &rd, RID root, RD::NativeImmutableHold &hold, uint32_t limit = 256) {
		Vector<RID> roots;
		roots.push_back(root);
		return rd.native_immutable_hold_acquire(roots, hold, limit);
	}
	static void expect_profile_rejected(RD &rd, RID root, Error expected) {
		RD::NativeImmutableHold hold;
		const Error error = physical_hold(rd, root, hold);
		CHECK(error == expected);
		CHECK_FALSE(hold.is_valid());
		if (hold.is_valid()) {
			CHECK(rd.native_immutable_hold_release(hold) == OK);
		} // Cleanup after a diagnosed erroneous admission.
		CHECK(TestMaterialStorageNativeHoldDeviceAccess::native_holds(rd) == 0);
	}

public:
	static void run_texture_profile(uint32_t mode) {
		auto *previous = RD::get_singleton();
		RD rd;
		Driver driver;
		TestMaterialStorageNativeHoldDeviceAccess::prepare(rd, driver);
		RD::TextureFormat format;
		format.texture_type = RD::TEXTURE_TYPE_2D;
		format.width = format.height = format.depth = format.array_layers = format.mipmaps = 1;
		format.samples = RD::TEXTURE_SAMPLES_1;
		format.format = mode == 3 ? RD::DATA_FORMAT_D32_SFLOAT : RD::DATA_FORMAT_R8_UNORM;
		format.usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT;
		if (mode == 3) {
			format.usage_bits |= RD::TEXTURE_USAGE_DEPTH_RESOLVE_ATTACHMENT_BIT;
		}
		const RID parent = rd.texture_create(format, RD::TextureView());
		const RID view = rd.texture_create_shared(RD::TextureView(), parent);
		const RID sibling = rd.texture_create_shared(RD::TextureView(), parent);
		CHECK(parent.is_valid());
		CHECK(view.is_valid());
		CHECK(sibling.is_valid());
		CHECK(driver.texture_creates == 1);
		CHECK(driver.views == 2);
#ifdef DEV_ENABLED
		CHECK(driver.names.size() == 3);
		CHECK(driver.names[84] == "RID:" + itos(parent.get_id()));
		CHECK(driver.names[85] == "RID:" + itos(view.get_id()));
		CHECK(driver.names[86] == "RID:" + itos(sibling.get_id()));
#else
		CHECK(driver.names.is_empty());
#endif
		if (mode == 1) {
			CHECK(rd.get_driver_resource(RD::DRIVER_RESOURCE_TEXTURE, view) == 0x10000 + 85);
			CHECK(driver.native_exports == 1); // Ordinary unheld native interop remains legal.
			expect_profile_rejected(rd, parent, ERR_UNAVAILABLE);
			expect_profile_rejected(rd, sibling, ERR_UNAVAILABLE);
			rd.free_rid(view);
			expect_profile_rejected(rd, parent, ERR_UNAVAILABLE);
			expect_profile_rejected(rd, sibling, ERR_UNAVAILABLE); // Sticky after exported view removal.
		} else if (mode == 2) {
			RD::NativeImmutableHold held;
			const Error acquired = physical_hold(rd, parent, held);
			CHECK(acquired == OK);
			if (acquired == OK) {
				Vector<uint8_t> bytes;
				bytes.push_back(7);
				ERR_PRINT_OFF;
				CHECK(rd.get_driver_resource(RD::DRIVER_RESOURCE_TEXTURE_VIEW, sibling) == 0);
				CHECK(rd.texture_update(sibling, 0, bytes) == ERR_BUSY);
				ERR_PRINT_ON;
				CHECK(driver.native_exports == 0);
				CHECK(rd.native_immutable_resource_is_held(sibling));
				CHECK(rd.native_immutable_hold_release(held) == OK);
			}
			CHECK(rd.get_driver_resource(RD::DRIVER_RESOURCE_TEXTURE_VIEW, sibling) == 0x10000 + 86);
			CHECK(driver.native_exports == 1);
			expect_profile_rejected(rd, parent, ERR_UNAVAILABLE);
		} else if (mode == 3) {
			expect_profile_rejected(rd, parent, ERR_UNAVAILABLE);
			expect_profile_rejected(rd, sibling, ERR_UNAVAILABLE);
		} else {
			CHECK(mode == 4);
			RD::NativeImmutableHold stable;
			const Error acquired = physical_hold(rd, parent, stable);
			CHECK(acquired == OK);
			if (acquired == OK) {
				CHECK(rd.native_immutable_hold_release(stable) == OK);
			} // Actual factory's stable worker=-1 neighbor.
			TestMaterialStorageNativeHoldDeviceAccess::mark_pending_texture_worker(rd, parent);
			expect_profile_rejected(rd, parent, ERR_UNAVAILABLE);
			expect_profile_rejected(rd, sibling, ERR_UNAVAILABLE);
		}
		if (mode != 1) {
			rd.free_rid(view);
		}
		rd.free_rid(sibling);
		rd.free_rid(parent);
		CHECK(driver.texture_frees == 0);
		TestMaterialStorageNativeHoldDeviceAccess::drain(rd);
		CHECK(driver.texture_frees == 3);
		CHECK(TestMaterialStorageNativeHoldDeviceAccess::empty(rd));
		TestMaterialStorageNativeHoldDeviceAccess::detach(rd);
		TestMaterialStorageNativeHoldDeviceAccess::restore(previous);
	}

	static void run_sampler_profile(uint32_t mode) {
		auto *previous = RD::get_singleton();
		RD rd;
		Driver driver;
		TestMaterialStorageNativeHoldDeviceAccess::prepare(rd, driver);
		const RID sampler = rd.sampler_create(RD::SamplerState());
		const bool embedded = mode != 0;
		const RID shader = TestMaterialStorageNativeHoldDeviceAccess::shader(rd, 16, embedded ? sampler : RID());
		const RID pipeline = rd.compute_pipeline_create(shader);
		CHECK(sampler.is_valid());
		CHECK(shader.is_valid());
		CHECK(pipeline.is_valid());
		CHECK(driver.sampler_creates == 1);
		if (!sampler.is_valid() || !shader.is_valid() || !pipeline.is_valid()) {
			if (shader.is_valid()) {
				rd.free_rid(shader);
			}
			if (sampler.is_valid()) {
				rd.free_rid(sampler);
			}
			TestMaterialStorageNativeHoldDeviceAccess::drain(rd);
			TestMaterialStorageNativeHoldDeviceAccess::detach(rd);
			TestMaterialStorageNativeHoldDeviceAccess::restore(previous);
			return;
		}
		CHECK(driver.immutable_sampler_inputs == uint32_t(embedded));
		CHECK(TestMaterialStorageNativeHoldDeviceAccess::captured_sampler_count(rd, shader) == uint32_t(embedded));
		if (embedded) {
			CHECK(TestMaterialStorageNativeHoldDeviceAccess::captured_sampler(rd, shader, sampler));
		}
		CHECK_FALSE(TestMaterialStorageNativeHoldDeviceAccess::set_uses_buffer(rd, shader, sampler)); // No legacy _add_dependency policy change.
#ifdef DEV_ENABLED
		CHECK(driver.names.size() == 3);
		CHECK(driver.names[80] == "RID:" + itos(shader.get_id()));
		CHECK(driver.names[81] == "RID:" + itos(pipeline.get_id()));
		CHECK(driver.names[83] == "RID:" + itos(sampler.get_id()));
#else
		CHECK(driver.names.is_empty());
#endif
		if (mode == 2) {
			CHECK(rd.get_driver_resource(RD::DRIVER_RESOURCE_SAMPLER, sampler) == 0x10000 + 83);
			CHECK(driver.native_exports == 1);
			expect_profile_rejected(rd, pipeline, ERR_UNAVAILABLE);
			rd.free_rid(sampler);
		} else {
			if (embedded) {
				RD::NativeImmutableHold undersized;
				const Error quoted = physical_hold(rd, pipeline, undersized, 2);
				CHECK(quoted == ERR_OUT_OF_MEMORY);
				CHECK_FALSE(rd.native_immutable_resource_is_held(sampler));
				CHECK_FALSE(rd.native_immutable_resource_is_held(shader));
				if (undersized.is_valid()) {
					CHECK(rd.native_immutable_hold_release(undersized) == OK);
				} // Cleanup only after diagnosed missing closure/credit.
			}
			RD::NativeImmutableHold held;
			const Error acquired = physical_hold(rd, pipeline, held, 3);
			CHECK(acquired == OK);
			if (acquired == OK) {
				CHECK(rd.native_immutable_resource_is_held(sampler) == embedded); // Roots contain only the actual pipeline, never the sampler/default table.
				rd.free_rid(sampler);
				if (embedded) {
					CHECK(TestMaterialStorageNativeHoldDeviceAccess::sampler_exists(rd, sampler));
					CHECK(rd.native_immutable_resource_is_retiring(sampler));
					RD::NativeImmutableHold refused;
					const Error retired = physical_hold(rd, shader, refused);
					CHECK(retired == ERR_BUSY);
					CHECK_FALSE(refused.is_valid());
					if (refused.is_valid()) {
						CHECK(rd.native_immutable_hold_release(refused) == OK);
					}
					TestMaterialStorageNativeHoldDeviceAccess::drain(rd);
					CHECK(driver.sampler_frees == 0);
				} else {
					TestMaterialStorageNativeHoldDeviceAccess::drain(rd);
					CHECK(driver.sampler_frees == 1);
					CHECK(held.is_valid());
				}
				CHECK(rd.native_immutable_hold_release(held) == OK);
				CHECK_FALSE(TestMaterialStorageNativeHoldDeviceAccess::sampler_exists(rd, sampler));
				if (embedded) {
					CHECK(driver.sampler_frees == 0);
					expect_profile_rejected(rd, shader, ERR_DOES_NOT_EXIST);
				}
			}
			if (TestMaterialStorageNativeHoldDeviceAccess::sampler_exists(rd, sampler)) {
				rd.free_rid(sampler);
			}
		}
		TestMaterialStorageNativeHoldDeviceAccess::drain(rd);
		CHECK(driver.sampler_frees == 1);
		rd.free_rid(shader);
		TestMaterialStorageNativeHoldDeviceAccess::drain(rd);
		CHECK(driver.shader_frees == 1);
		CHECK(driver.pipeline_frees == 1);
		CHECK(driver.sampler_frees == 1);
		CHECK(TestMaterialStorageNativeHoldDeviceAccess::empty(rd));
		TestMaterialStorageNativeHoldDeviceAccess::detach(rd);
		TestMaterialStorageNativeHoldDeviceAccess::restore(previous);
	}

private:
	static MaterialStorage::MaterialData *make_material(MaterialStorage::ShaderData *) { return memnew(MaterialData); }
	static RID version(ShaderOwner &owner, const Vector<RID> &shaders) {
		ShaderRD::Version value;
		value.mutex = memnew(Mutex);
		value.dirty = false;
		value.valid = true;
		value.initialize_needed = false;
		value.embedded = false;
		value.variants = shaders;
		owner.variant_defines.resize(shaders.size());
		return owner.version_owner.make_rid(value);
	}

public:
	struct SelectionMaterials {
		MaterialStorage *storage = nullptr, *previous = nullptr;
		RID shader, materials[3];
		uint32_t material_count = 2;
	};
	class SelectionShaderData : public MaterialStorage::ShaderData {
	public:
		void set_code(const String &) override {}
		bool is_animated() const override { return false; }
		bool casts_shadows() const override { return false; }
		RenderingServerTypes::ShaderNativeSourceCode get_native_source_code() const override { return {}; }
		Pair<ShaderRD *, RID> get_native_shader_and_version() const override { return {}; }
	};
	class SelectionMaterialData : public MaterialStorage::MaterialData {
	public:
		void set_render_priority(int) override {}
		void set_next_pass(RID) override {}
		bool update_parameters(const HashMap<StringName, Variant> &, bool, bool) override { return false; }
	};
	static void *selection_create(uint32_t material_count) {
		CRASH_COND(material_count < 2 || material_count > 3);
		auto *p = memnew(SelectionMaterials);
		p->material_count = material_count;
		p->previous = MaterialStorage::get_singleton();
		p->storage = memnew(MaterialStorage(MaterialStorage::NativeHoldTestTag{}));
		auto &storage = *p->storage;
		storage.shader_data_request_func[MaterialStorage::SHADER_TYPE_3D] = []() -> MaterialStorage::ShaderData * { return memnew(SelectionShaderData); };
		storage.material_data_request_func[MaterialStorage::SHADER_TYPE_3D] = [](MaterialStorage::ShaderData *) -> MaterialStorage::MaterialData * { return memnew(SelectionMaterialData); };
		p->shader = storage.shader_allocate();
		storage.shader_initialize(p->shader, false);
		storage.shader_set_code(p->shader, "shader_type spatial;");
		for (uint32_t i = 0; i < p->material_count; ++i) {
			RID &material = p->materials[i];
			material = storage.material_allocate();
			storage.material_initialize(material);
			storage.material_set_shader(material, p->shader);
		}
		storage._update_queued_materials();
		return p;
	}
	static RID selection_rid(void *opaque, uint32_t index) { return static_cast<SelectionMaterials *>(opaque)->materials[index]; }
	static uint32_t selection_holds(void *opaque, uint32_t index) {
		auto &p = *static_cast<SelectionMaterials *>(opaque);
		return p.storage->get_material(p.materials[index])->selection_holds;
	}
	static uint32_t selection_shader_count(void *opaque) {
		auto &p = *static_cast<SelectionMaterials *>(opaque);
		return p.storage->get_shader(p.shader)->selection_holds;
	}
	static void selection_destroy(void *opaque) {
		auto *p = static_cast<SelectionMaterials *>(opaque);
		for (uint32_t i = 0; i < p->material_count; ++i) {
			p->storage->material_free(p->materials[i]);
		}
		p->storage->shader_free(p->shader);
		memdelete(p->storage);
		MaterialStorage::singleton = p->previous;
		memdelete(p);
	}
	static void run_preparation_data_boundary() {
		class PreparationShaderData : public MaterialStorage::ShaderData {
		public:
			mutable uint32_t native_queries = 0;
			void set_code(const String &) override {}
			bool is_animated() const override { return false; }
			bool casts_shadows() const override { return false; }
			RenderingServerTypes::ShaderNativeSourceCode get_native_source_code() const override { return {}; }
			Pair<ShaderRD *, RID> get_native_shader_and_version() const override {
				++native_queries;
				return {};
			}
		};
		class PreparationMaterialData : public MaterialStorage::MaterialData {
		public:
			uint32_t updates = 0;
			void set_render_priority(int) override {}
			void set_next_pass(RID) override {}
			bool update_parameters(const HashMap<StringName, Variant> &, bool, bool) override {
				++updates;
				return true;
			}
		};
		auto *previous = MaterialStorage::get_singleton();
		{
			// Original test-only storage and actual RID owners, no native factories.
			MaterialStorage storage(MaterialStorage::NativeHoldTestTag{});
			PreparationShaderData shader_data;
			PreparationMaterialData original_data;
			const RID shader_rid = storage.shader_allocate();
			storage.shader_initialize(shader_rid, false);
			const RID material_rid = storage.material_allocate();
			storage.material_initialize(material_rid);
			auto *shader = storage.shader_owner.get_or_null(shader_rid);
			auto *material = storage.material_owner.get_or_null(material_rid);
			CHECK(shader != nullptr);
			CHECK(material != nullptr);
			if (shader && material) {
				// Private boundary metadata only; no preparation/hold/source grant.
				shader->data = &shader_data;
				material->shader = shader;
				material->data = &original_data;
				material->shader_type = MaterialStorage::SHADER_TYPE_3D;
				const uint64_t generation = material->generation;
				auto expect = [&](RID target, uint64_t expected_generation, Error expected) {
					// A valid stale output pointer must be cleared on EVERY failure.
					MaterialStorage::MaterialData *result = &original_data;
					const uint64_t before_generation = material->generation;
					const auto before_type = material->shader_type;
					auto *before_data = material->data;
					auto *before_shader = material->shader;
					const bool before_uniform = material->uniform_dirty, before_texture = material->texture_dirty;
					const RID before_next = material->next_pass;
					CHECK(storage.material_get_native_preparation_data(target, expected_generation, result) == expected);
					CHECK(result == (expected == OK ? &original_data : nullptr));
					CHECK(material->generation == before_generation);
					CHECK(material->shader_type == before_type);
					CHECK(material->data == before_data);
					CHECK(material->shader == before_shader);
					CHECK(material->uniform_dirty == before_uniform);
					CHECK(material->texture_dirty == before_texture);
					CHECK(material->next_pass == before_next);
					CHECK(material->native_immutable_holds == 0);
				};
				expect(material_rid, generation, OK);
				expect(RID(), generation, ERR_INVALID_DATA);
				expect(material_rid, generation + 1, ERR_INVALID_DATA);
				expect(material_rid, 0, ERR_INVALID_DATA); // No wildcard generation.
				expect(material_rid, UINT64_MAX, ERR_INVALID_DATA);
				material->generation = UINT64_MAX;
				expect(material_rid, UINT64_MAX, ERR_INVALID_DATA); // Matching sentinel still refuses.
				material->generation = generation;
				material->shader_type = MaterialStorage::SHADER_TYPE_2D;
				expect(material_rid, generation, ERR_INVALID_DATA);
				material->shader_type = MaterialStorage::SHADER_TYPE_3D;
				material->data = nullptr;
				expect(material_rid, generation, ERR_INVALID_DATA);
				material->data = &original_data;
				material->shader = nullptr;
				expect(material_rid, generation, ERR_INVALID_DATA);
				material->shader = shader;
				shader->data = nullptr;
				expect(material_rid, generation, ERR_INVALID_DATA);
				shader->data = &shader_data;
				material->uniform_dirty = true;
				expect(material_rid, generation, ERR_BUSY);
				material->next_pass = material_rid;
				expect(material_rid, generation, ERR_BUSY); // Dirty precedes next pass.
				expect(material_rid, generation + 1, ERR_INVALID_DATA); // Invalid precedes both.
				material->shader_type = MaterialStorage::SHADER_TYPE_2D;
				expect(material_rid, generation, ERR_INVALID_DATA);
				material->shader_type = MaterialStorage::SHADER_TYPE_3D;
				material->uniform_dirty = false;
				material->texture_dirty = true;
				expect(material_rid, generation, ERR_BUSY); // Texture dirty also precedes next pass.
				material->next_pass = RID();
				expect(material_rid, generation, ERR_BUSY);
				material->texture_dirty = false;
				material->next_pass = material_rid;
				expect(material_rid, generation, ERR_UNAVAILABLE);
				material->next_pass = RID();
				expect(material_rid, generation, OK);
				CHECK(shader_data.native_queries == 0);
				CHECK(original_data.updates == 0);
				// Stack-owned test data never goes through material_free deletion.
				material->data = nullptr;
				material->shader = nullptr;
				shader->data = nullptr;
			}
			storage.material_free(material_rid);
			storage.shader_free(shader_rid);
			CHECK(storage.material_owner.get_rid_count() == 0);
			CHECK(storage.shader_owner.get_rid_count() == 0);
		}
		MaterialStorage::singleton = previous;
	}
	class EmptyPipelineMaterialData : public MaterialStorage::MaterialData {
	public:
		RID native_shader, set;
		void set_render_priority(int) override {}
		void set_next_pass(RID) override {}
		bool update_parameters(const HashMap<StringName, Variant> &parameters, bool, bool) override {
			return update_parameters_uniform_set(parameters, false, false, {}, nullptr, {}, {}, 0, set, native_shader, 0, false, false);
		}
		~EmptyPipelineMaterialData() { free_parameters_uniform_set(set); }
	};
	struct PipelineMaterialProfile {
		ShaderOwner shader_owner;
		MaterialStorage *previous = nullptr, *storage = nullptr;
		RID material, shader, native_shader;
	};
	static MaterialStorage::MaterialData *make_empty_pipeline_material(MaterialStorage::ShaderData *) { return memnew(EmptyPipelineMaterialData); }
	static void *create_pipeline_profile(RID native_shader) {
		auto *profile = memnew(PipelineMaterialProfile);
		profile->previous = MaterialStorage::get_singleton();
		profile->storage = memnew(MaterialStorage(MaterialStorage::NativeHoldTestTag{}));
		auto &storage = *profile->storage;
		profile->native_shader = native_shader;
		profile->shader = storage.shader_allocate();
		storage.shader_initialize(profile->shader, false);
		auto *shader = storage.shader_owner.get_or_null(profile->shader);
		auto *data = memnew(ShaderData);
		data->owner = &profile->shader_owner;
		Vector<RID> variants;
		variants.push_back(native_shader);
		data->version = version(profile->shader_owner, variants);
		shader->data = data;
		shader->type = MaterialStorage::SHADER_TYPE_3D;
		storage.material_data_request_func[MaterialStorage::SHADER_TYPE_3D] = &make_empty_pipeline_material;
		profile->material = storage.material_allocate();
		storage.material_initialize(profile->material);
		storage.material_set_shader(profile->material, profile->shader);
		auto *material = storage.material_owner.get_or_null(profile->material);
		static_cast<EmptyPipelineMaterialData *>(material->data)->native_shader = native_shader;
		storage._update_queued_materials(); // Original empty-profile capture path.

		CHECK(material->data->native_uniform_sets[0].supported);
		return profile;
	}
	static Error acquire_pipeline_profile(void *opaque, RID pipeline, uint32_t max_resources, MaterialStorage::NativeMaterialHold &hold) {
		auto &p = *static_cast<PipelineMaterialProfile *>(opaque);
		return p.storage->material_native_hold_acquire(p.material, p.native_shader, pipeline, RID(), hold, max_resources);
	}
	static Error capture_pipeline_profile(GDExtensionProductionPhysicalContextRefV1 context, GDExtensionDeviceExit::ProductionOutputPin &pin, MaterialStorage::NativeMaterialHold &hold, uint64_t &group) {
		auto *source = MaterialStorage::_native_hold_transfer_source(hold);
		if (!source) {
			return ERR_INVALID_DATA;
		}
		const Error captured = TestMaterialStorageNativeHoldDeviceAccess::capture_pipeline_profile(context, pin, *source, group);
		return captured == OK ? MaterialStorage::_native_hold_mark_transferred(hold) : captured;
	}
	static void destroy_pipeline_profile(void *opaque) {
		auto *p = static_cast<PipelineMaterialProfile *>(opaque);
		p->storage->material_free(p->material);
		p->storage->shader_free(p->shader);
		memdelete(p->storage);
		MaterialStorage::singleton = p->previous;
		memdelete(p);
	}
	static void run(bool unsupported, bool cross_variants = false, uint32_t callback_lifetime = 0, bool generation_regression = false) {
		RenderingDevice *previous = RD::get_singleton();
		RD rd;
		Driver driver;
		TestMaterialStorageNativeHoldDeviceAccess::prepare(rd, driver);
		ShaderOwner shader_owner;
		auto *previous_texture_storage = TextureStorage::get_singleton();
		auto *textures = memnew(TextureStorage(TextureStorage::NativeHoldTestTag{}));
		auto *previous_storage = MaterialStorage::get_singleton();
		auto *storage = memnew(MaterialStorage(MaterialStorage::NativeHoldTestTag{}));
		const RID native_shader = TestMaterialStorageNativeHoldDeviceAccess::shader(rd);
		const RID native_parent = TestMaterialStorageNativeHoldDeviceAccess::texture(rd);
		const RID texture = textures->texture_allocate();
		const bool texture_initialized = textures->texture_rd_initialize(texture, native_parent);
		CHECK(texture_initialized);
		const RID native_texture = textures->texture_get_rd_texture(texture);
		const RID original_pipeline = rd.compute_pipeline_create(native_shader);
		Vector<RID> variants;
		variants.push_back(native_shader);
		RID incompatible_shader, incompatible_pipeline;
		RID selected_shader = native_shader, pipeline = original_pipeline;
		if (cross_variants) {
			incompatible_shader = TestMaterialStorageNativeHoldDeviceAccess::shader(rd, 32);
			incompatible_pipeline = rd.compute_pipeline_create(incompatible_shader);
			selected_shader = TestMaterialStorageNativeHoldDeviceAccess::shader(rd, 16);
			pipeline = rd.compute_pipeline_create(selected_shader);
			variants.push_back(incompatible_shader);
			variants.push_back(selected_shader);
		}
		const RID ubo = rd.uniform_buffer_create(16);
		const RID sampler = rd.sampler_create(RD::SamplerState());
		storage->default_samplers.rids[0][0] = sampler;
		const RID shader = storage->shader_allocate();
		storage->shader_initialize(shader, false);
		auto *shader_cell = storage->shader_owner.get_or_null(shader);
		auto *shader_data = memnew(ShaderData);
		shader_data->owner = &shader_owner;
		shader_data->version = version(shader_owner, variants);
		shader_cell->data = shader_data;
		shader_cell->type = MaterialStorage::SHADER_TYPE_3D;
		storage->material_data_request_func[MaterialStorage::SHADER_TYPE_3D] = &make_material;
		const RID material = storage->material_allocate();
		storage->material_initialize(material);
		storage->material_set_shader(material, shader);
		auto *material_cell = storage->material_owner.get_or_null(material);
		auto *data = static_cast<MaterialData *>(material_cell->data);
		// Controlled pre-uploaded CPU native metadata, not a completed source.
		data->shader = native_shader;
		data->uniform_buffer[0] = ubo;
		data->ubo_data[0].resize(16);
		data->ubo_data[0].fill(42);
		data->texture_cache.push_back(native_texture);
		ShaderCompiler::GeneratedCode::Texture reflected_texture;
		reflected_texture.type = ShaderLanguage::TYPE_SAMPLER2D;
		reflected_texture.array_size = 0;
		data->reflected_textures.push_back(reflected_texture);
		if (unsupported) {
			ShaderLanguage::ShaderNode::Uniform global;
			global.scope = ShaderLanguage::ShaderNode::Uniform::SCOPE_GLOBAL;
			data->reflected_uniforms.insert("global_input", global);
		}
		storage->_update_queued_materials(); // Actual original uniform-set creator/capture.
		CHECK(data->set.is_valid());
		// CPU selection retains original owners, not an additional native pipeline.
		MaterialStorage::NativeSelectionLease selection, sibling;
		const uint64_t selected_generation = material_cell->generation;
		const uint32_t physical_before = material_cell->native_immutable_holds;
		CHECK(storage->material_selection_acquire(material, selection, selected_generation + 1) == ERR_INVALID_DATA);
		CHECK_FALSE(selection.is_active());
		// Explicit CPU counter-boundary model, not billions of admitted leases.
		// Each rejected acquisition must leave the other original owner untouched.
		const uint32_t material_selections = material_cell->selection_holds;
		const uint32_t shader_selections = shader_cell->selection_holds;
		material_cell->selection_holds = UINT32_MAX;
		CHECK(storage->material_selection_acquire(material, selection, selected_generation) == ERR_PARAMETER_RANGE_ERROR);
		CHECK_FALSE(selection.is_active());
		CHECK(material_cell->selection_holds == UINT32_MAX);
		CHECK(shader_cell->selection_holds == shader_selections);
		CHECK(material_cell->generation == selected_generation);
		CHECK(material_cell->native_immutable_holds == physical_before);
		material_cell->selection_holds = material_selections;
		shader_cell->selection_holds = UINT32_MAX;
		CHECK(storage->material_selection_acquire(material, selection, selected_generation) == ERR_PARAMETER_RANGE_ERROR);
		CHECK_FALSE(selection.is_active());
		CHECK(material_cell->selection_holds == material_selections);
		CHECK(shader_cell->selection_holds == UINT32_MAX);
		CHECK(material_cell->generation == selected_generation);
		CHECK(material_cell->native_immutable_holds == physical_before);
		shader_cell->selection_holds = shader_selections;
		CHECK(storage->material_selection_acquire(material, selection, selected_generation) == OK);
		CHECK(storage->material_selection_acquire(material, selection, selected_generation) == ERR_ALREADY_IN_USE);
		CHECK(storage->material_selection_acquire(material, sibling, selected_generation) == OK);
		CHECK(material_cell->selection_holds == 2);
		CHECK(shader_cell->selection_holds == 2);
		CHECK(material_cell->native_immutable_holds == physical_before);
		ERR_PRINT_OFF;
		storage->material_free(material);
		storage->shader_free(shader);
		storage->material_set_shader(material, RID());
		storage->material_set_param(material, "selection_probe", 1);
		storage->material_set_next_pass(material, material);
		storage->material_set_render_priority(material, 2);
		storage->shader_set_code(shader, "shader_type spatial;");
		storage->shader_set_default_texture_parameter(shader, "selection_probe", RID(), 0);
		ERR_PRINT_ON;
		CHECK(storage->material_owner.get_or_null(material) == material_cell);
		CHECK(storage->shader_owner.get_or_null(shader) == shader_cell);
		CHECK(material_cell->generation == selected_generation);
		CHECK(material_cell->shader == shader_cell);
		CHECK(storage->material_selection_release(selection) == OK);
		CHECK(material_cell->selection_holds == 1);
		CHECK(storage->material_selection_release(selection) == ERR_UNAUTHORIZED);
		CHECK(storage->material_selection_release(sibling) == OK);
		CHECK(material_cell->selection_holds == 0);
		CHECK(shader_cell->selection_holds == 0);
		class SelectionOnlyMaterialData : public MaterialStorage::MaterialData {
		public:
			void set_render_priority(int) override {}
			void set_next_pass(RID) override {}
			bool update_parameters(const HashMap<StringName, Variant> &, bool, bool) override { return false; }
		};
		storage->material_data_request_func[MaterialStorage::SHADER_TYPE_3D] = [](MaterialStorage::ShaderData *) -> MaterialStorage::MaterialData * { return memnew(SelectionOnlyMaterialData); };
		const RID second_material = storage->material_allocate();
		storage->material_initialize(second_material);
		storage->material_set_shader(second_material, shader);
		storage->material_data_request_func[MaterialStorage::SHADER_TYPE_3D] = &make_material;
		storage->_update_queued_materials();
		auto *second_cell = storage->material_owner.get_or_null(second_material);
		CHECK(storage->material_selection_acquire(material, selection, selected_generation) == OK);
		CHECK(storage->material_selection_acquire(second_material, sibling, second_cell->generation) == OK);
		CHECK(shader_cell->selection_holds == 2);
		CHECK(storage->material_selection_release(selection) == OK);
		CHECK(shader_cell->selection_holds == 1);
		ERR_PRINT_OFF;
		storage->shader_free(shader);
		ERR_PRINT_ON;
		CHECK(storage->shader_owner.get_or_null(shader) == shader_cell);
		CHECK(second_cell->shader == shader_cell);
		CHECK(storage->material_selection_release(sibling) == OK);
		CHECK(shader_cell->selection_holds == 0);
		storage->material_free(second_material);
		// Original dirty queue service is deferred while the material selection
		// lives, and resumes after its actual return. The probe counts calls into
		// the real existing parameter/uniform-set update, not a mirrored scheduler.
		CHECK(storage->material_selection_acquire(material, selection, selected_generation) == OK);
		const uint32_t updates_before_selection = data->parameter_updates;
		const uint32_t sets_before_selection = driver.set_creates;
		const RID set_before_selection = data->set;
		storage->_material_queue_update(material_cell, true, false);
		CHECK(material_cell->uniform_dirty);
		CHECK(storage->material_selection_acquire(material, sibling, selected_generation) == ERR_BUSY);
		CHECK_FALSE(sibling.is_active());
		CHECK(material_cell->selection_holds == 1);
		CHECK(shader_cell->selection_holds == 1);
		for (uint32_t turn = 0; turn < 3; ++turn) {
			storage->_update_queued_materials();
			CHECK(data->parameter_updates == updates_before_selection);
			CHECK(material_cell->uniform_dirty);
			CHECK(material_cell->update_element.in_list());
			CHECK(storage->material_update_list.first() == &material_cell->update_element);
			CHECK(material_cell->update_element.next() == nullptr);
			CHECK(material_cell->generation == selected_generation);
			CHECK(data->set == set_before_selection);
			CHECK(driver.set_creates == sets_before_selection);
		}
		CHECK(storage->material_selection_release(selection) == OK);
		storage->_update_queued_materials();
		CHECK(data->parameter_updates == updates_before_selection + 1);
		CHECK_FALSE(material_cell->uniform_dirty);
		CHECK_FALSE(material_cell->texture_dirty);
		CHECK_FALSE(material_cell->update_element.in_list());
		CHECK(storage->material_update_list.first() == nullptr);
		CHECK(data->set == set_before_selection);
		CHECK(driver.set_creates == sets_before_selection);
		CHECK(material_cell->selection_holds == 0);
		CHECK(shader_cell->selection_holds == 0);
		storage->_update_queued_materials();
		CHECK(data->parameter_updates == updates_before_selection + 1);
		// Actual original factory metadata, never a test-supplied callback grant.
		const uint64_t set_generation = data->native_uniform_sets[0].owner_generation;
		CHECK(set_generation != 0);
		CHECK(TestMaterialStorageNativeHoldDeviceAccess::callback_origin_matches(rd, data->set, material, set_generation));
		CHECK(MaterialStorage::_native_hold_callback_is_owned(material, set_generation, MaterialStorage::_material_uniform_set_erased, &data->self, data->set));
		uint64_t unrelated_userdata = 0;
		CHECK_FALSE(MaterialStorage::_native_hold_callback_is_owned(material, set_generation, MaterialStorage::_material_uniform_set_erased, &unrelated_userdata, data->set));
		CHECK_FALSE(MaterialStorage::_native_hold_callback_is_owned(RID(), set_generation, MaterialStorage::_material_uniform_set_erased, &data->self, data->set));
		CHECK_FALSE(MaterialStorage::_native_hold_callback_is_owned(material, set_generation + 1, MaterialStorage::_material_uniform_set_erased, &data->self, data->set));
		CHECK_FALSE(MaterialStorage::_native_hold_callback_is_owned(material, set_generation, nullptr, &data->self, data->set));
		// Ordinary UBO-only refresh keeps the actual set even after callback
		// replacement. Such a refresh cannot acquire factory provenance.
		const RID reused_set = data->set;
		const uint32_t set_creates_before_refresh = driver.set_creates;
		rd.uniform_set_set_invalidation_callback(reused_set, nullptr, nullptr);
		storage->_material_queue_update(material_cell, false, false);
		storage->_update_queued_materials();
		CHECK(data->set == reused_set);
		CHECK(driver.set_creates == set_creates_before_refresh);
		CHECK(TestMaterialStorageNativeHoldDeviceAccess::callback_origin_empty(rd, reused_set));
		const auto foreign_callback = +[](void *) {};
		rd.uniform_set_set_invalidation_callback(reused_set, foreign_callback, &unrelated_userdata);
		storage->_material_queue_update(material_cell, false, false);
		storage->_update_queued_materials();
		CHECK(data->set == reused_set);
		CHECK(driver.set_creates == set_creates_before_refresh);
		CHECK(TestMaterialStorageNativeHoldDeviceAccess::callback_origin_empty(rd, reused_set));
		CHECK_FALSE(MaterialStorage::_native_hold_callback_is_owned(material, data->native_uniform_sets[0].owner_generation, foreign_callback, &unrelated_userdata, reused_set));
		// Explicit callback replacement clears factory origin even if identical.
		rd.uniform_set_set_invalidation_callback(data->set, MaterialStorage::_material_uniform_set_erased, &data->self);
		CHECK(TestMaterialStorageNativeHoldDeviceAccess::callback_origin_empty(rd, data->set));
		storage->_material_queue_update(material_cell, false, false);
		storage->_update_queued_materials();
		CHECK(TestMaterialStorageNativeHoldDeviceAccess::callback_origin_matches(rd, data->set, material, data->native_uniform_sets[0].owner_generation));

#ifdef DEV_ENABLED
		CHECK(driver.names.size() == (cross_variants ? 10 : 6));
		CHECK(driver.names[80] == "RID:" + itos(native_shader.get_id()));
		CHECK(driver.names[1] == "RID:" + itos(ubo.get_id()));
		CHECK(driver.names[81] == "RID:" + itos(original_pipeline.get_id()));
		if (cross_variants) {
			CHECK(driver.names[86] == "RID:" + itos(incompatible_shader.get_id()));
			CHECK(driver.names[87] == "RID:" + itos(incompatible_pipeline.get_id()));
			CHECK(driver.names[88] == "RID:" + itos(selected_shader.get_id()));
			CHECK(driver.names[89] == "RID:" + itos(pipeline.get_id()));
		}
		CHECK(driver.names[82] == "RID:" + itos(data->set.get_id()));
		CHECK(driver.names[83] == "RID:" + itos(sampler.get_id()));
		CHECK(driver.names[85] == "RID:" + itos(native_texture.get_id()));
#else
		CHECK(driver.names.is_empty());
#endif
		CHECK((driver.set_creates == 1 && driver.pipeline_creates == (cross_variants ? 3u : 1u)));
		CHECK_FALSE(rd.native_immutable_uniform_set_matches_pipeline(RID(), pipeline, 0));
		CHECK(rd.native_immutable_uniform_set_matches_pipeline(RID(), pipeline, 1));
#ifdef THREADS_ENABLED
		WrongThread wrong{ storage, material, native_shader, pipeline, data->set };
		Thread worker;
		worker.start(&WrongThread::run, &wrong);
		worker.wait_to_finish();
		CHECK(wrong.error == ERR_UNAUTHORIZED);
		CHECK_FALSE(rd.native_immutable_resource_is_held(ubo));
#endif

		if (generation_regression) {
			const uint64_t selected_generation = storage->material_get_generation(material);
			const String edited_code = "shader_type spatial; void fragment() { ALBEDO = vec3(0.5); }";
			storage->shader_set_code(shader, edited_code);
			CHECK(storage->shader_get_code(shader) == edited_code);
			CHECK(shader_data->changes == 1);
			storage->_update_queued_materials();
			MaterialStorage::NativeMaterialHold stale;
			CHECK(storage->material_native_hold_acquire(material, native_shader, pipeline, data->set, stale, 256, selected_generation) == ERR_INVALID_DATA);
			CHECK_FALSE(stale.is_active());
			if (stale.is_active()) {
				CHECK(MaterialStorage::material_native_hold_release(stale) == OK);
			}
			const uint64_t reselected_generation = storage->material_get_generation(material);
			CHECK(reselected_generation != selected_generation);
			CHECK(storage->material_native_hold_acquire(material, native_shader, pipeline, data->set, stale, 256, reselected_generation) == OK);
			CHECK(stale.is_active());
			CHECK(stale.get_snapshot().material_generation == reselected_generation);
			CHECK(rd.native_immutable_resource_is_held(ubo));
			if (stale.is_active()) {
				CHECK(MaterialStorage::material_native_hold_release(stale) == OK);
			}
			storage->shader_set_default_texture_parameter(shader, "new_default", texture, 0);
			CHECK(storage->shader_get_default_texture_parameter(shader, "new_default", 0) == texture);
			storage->_update_queued_materials();
			CHECK(storage->material_native_hold_acquire(material, native_shader, pipeline, data->set, stale, 256, reselected_generation) == ERR_INVALID_DATA);
			CHECK_FALSE(stale.is_active());
			if (stale.is_active()) {
				CHECK(MaterialStorage::material_native_hold_release(stale) == OK);
			}
			const uint64_t current_generation = storage->material_get_generation(material);
			CHECK(current_generation != reselected_generation);
			CHECK(storage->material_native_hold_acquire(material, native_shader, pipeline, data->set, stale, 256, current_generation) == OK);
			CHECK(stale.is_active());
			CHECK(stale.get_snapshot().material_generation == current_generation);
			CHECK(rd.native_immutable_resource_is_held(ubo));
			if (stale.is_active()) {
				CHECK(MaterialStorage::material_native_hold_release(stale) == OK);
			}
			// Boundary injection only; the mutation and rejection remain original paths.
			material_cell->generation = UINT64_MAX;
			storage->shader_set_code(shader, "shader_type spatial;");
			CHECK(storage->shader_get_code(shader) == "shader_type spatial;");
			CHECK(storage->material_get_generation(material) == UINT64_MAX);
			storage->shader_set_default_texture_parameter(shader, "max_default", texture, 0);
			CHECK(storage->shader_get_default_texture_parameter(shader, "max_default", 0) == texture);
			storage->_update_queued_materials();
			CHECK(storage->material_get_generation(material) == UINT64_MAX);
			CHECK(storage->material_native_hold_acquire(material, native_shader, pipeline, data->set, stale, 256, UINT64_MAX) == ERR_INVALID_DATA);
			CHECK_FALSE(stale.is_active());
			if (stale.is_active()) {
				CHECK(MaterialStorage::material_native_hold_release(stale) == OK);
			}
			material_cell->generation = current_generation;
			data->generation = current_generation;
		}
		MaterialStorage::NativeMaterialHold first, second;
		if (callback_lifetime == 4) {
#ifdef THREADS_ENABLED
			TestMaterialStorageNativeHoldDeviceAccess::check_mutex_probe_control(rd);
#endif
			CallbackTrace callback_trace;
			auto *callback_owner = memnew(CallbackOwner(callback_trace, rd));
			rd.uniform_set_set_invalidation_callback(data->set, &invalidated_probe, callback_owner);
			rd.free_rid(data->set); // Original unheld callback path, independently of immutable retirement.
			CHECK(callback_trace.calls == 1);
			CHECK_FALSE(TestMaterialStorageNativeHoldDeviceAccess::set_exists(rd, data->set));
			CHECK(driver.set_frees == 0);
			if (TestMaterialStorageNativeHoldDeviceAccess::set_exists(rd, data->set)) {
				rd.uniform_set_set_invalidation_callback(data->set, nullptr, nullptr);
			}
			memdelete(callback_owner);
			CHECK_FALSE(callback_trace.alive);
		} else if (callback_lifetime == 3) {
			last_return_from_callback(*storage, rd, driver, material, native_shader, pipeline, data->set);
		} else if (unsupported) {
			CHECK(storage->material_native_hold_acquire(material, native_shader, pipeline, data->set, first) == ERR_UNAVAILABLE);
			CHECK_FALSE(rd.native_immutable_resource_is_held(ubo));
		} else {
			CHECK(storage->material_native_hold_acquire(material, native_shader, pipeline, data->set, first, 1) != OK);
			CHECK_FALSE(rd.native_immutable_resource_is_held(ubo));
			CHECK(storage->material_native_hold_acquire(material, native_shader, RID(), data->set, first) == ERR_INVALID_DATA);
			CHECK(storage->material_native_hold_acquire(material, sampler, pipeline, data->set, first) == ERR_INVALID_DATA);
			if (cross_variants) {
				CHECK(storage->material_native_hold_acquire(material, incompatible_shader, incompatible_pipeline, data->set, first) == ERR_INVALID_DATA);
				CHECK_FALSE(first.is_active());
				CHECK_FALSE(rd.native_immutable_resource_is_held(ubo));
				if (first.is_active()) {
					CHECK(MaterialStorage::material_native_hold_release(first) == OK);
				} // Release only after the diagnosed erroneous admission.
			}
			const Error acquired = storage->material_native_hold_acquire(material, selected_shader, pipeline, data->set, first);
			CHECK(acquired == OK);
			if (acquired == OK) {
				const Error second_error = storage->material_native_hold_acquire(material, selected_shader, pipeline, data->set, second);
				CHECK(second_error == OK);
				if (second_error != OK) {
					CHECK(MaterialStorage::material_native_hold_release(first) == OK);
				}
				if (second_error == OK) {
					const auto &snapshot = first.get_snapshot();
					CHECK(snapshot.shader == selected_shader);
					CHECK(snapshot.pipeline == pipeline);
					CHECK(snapshot.uniform_set_index == 0);
					if (callback_lifetime == 1) {
						CHECK(TestMaterialStorageNativeHoldDeviceAccess::callback_matches(rd, data->set, &data->self));
					}
					CHECK((snapshot.uniform_buffer == ubo && snapshot.uniform_set == data->set));
					CHECK((snapshot.textures.size() == 1 && snapshot.textures[0] == native_texture));
					CHECK((snapshot.samplers.size() == 1 && snapshot.samplers[0] == sampler));
					CHECK(snapshot.uniform_bytes.size() == 16);
					for (int i = 0; i < snapshot.uniform_bytes.size(); ++i) {
						CHECK(snapshot.uniform_bytes[i] == driver.bytes[i]);
					}
					const uint64_t generation = material_cell->generation;
					const uint32_t shader_changes = shader_data->changes;
					const String shader_code = storage->shader_get_code(shader);
					ERR_PRINT_OFF;
					uint8_t replacement[16] = {};
					CHECK(rd.buffer_update(ubo, 0, 16, replacement) == ERR_BUSY);
					storage->material_set_param(material, "new_value", 99);
					storage->shader_set_code(shader, "shader_type spatial; void fragment() { ALBEDO = vec3(0.25); }");
					storage->shader_set_default_texture_parameter(shader, "held_rejected", texture, 0);
					textures->texture_2d_update(texture, Image::create_empty(1, 1, false, Image::FORMAT_L8));
					ERR_PRINT_ON;
					CHECK(material_cell->generation == generation);
					CHECK_FALSE(material_cell->params.has("new_value"));
					CHECK(shader_data->changes == shader_changes);
					CHECK(storage->shader_get_code(shader) == shader_code);
					CHECK(storage->shader_get_default_texture_parameter(shader, "held_rejected", 0).is_null());
					CallbackTrace callback_trace;
					if (callback_lifetime == 2) {
						auto *callback_owner = memnew(CallbackOwner(callback_trace, rd));
						rd.uniform_set_set_invalidation_callback(snapshot.uniform_set, &invalidated_probe, callback_owner);
						rd.free_rid(snapshot.uniform_set);
						CHECK(callback_trace.calls == 1);
						CHECK(TestMaterialStorageNativeHoldDeviceAccess::callback_cleared(rd, snapshot.uniform_set));
						// Clean after a diagnosed failure before destroying userdata.
						if (callback_trace.calls != 1 || !TestMaterialStorageNativeHoldDeviceAccess::callback_cleared(rd, snapshot.uniform_set)) {
							rd.uniform_set_set_invalidation_callback(snapshot.uniform_set, nullptr, nullptr);
						}
						memdelete(callback_owner);
						CHECK_FALSE(callback_trace.alive);
						CHECK(driver.set_frees == 0);
					}
					storage->material_free(material);
					if (callback_lifetime == 1) {
						// The original Data is gone; this held native set is still live.
						const bool cleared = TestMaterialStorageNativeHoldDeviceAccess::callback_cleared(rd, snapshot.uniform_set);
						CHECK(cleared);
						if (!cleared) {
							rd.uniform_set_set_invalidation_callback(snapshot.uniform_set, nullptr, nullptr);
						} // Failed-case cleanup, never a successful callback claim.
					}
					storage->shader_free(shader);
					CHECK_FALSE(storage->owns_material(material));
					MaterialStorage::NativeMaterialHold refused;
					CHECK(storage->material_native_hold_acquire(material, native_shader, pipeline, snapshot.uniform_set, refused) == ERR_INVALID_PARAMETER);
					CHECK_FALSE(refused.is_active());
					textures->texture_free(texture);
					rd.free_rid(native_parent);
					rd.free_rid(sampler);
					CHECK(rd.native_immutable_resource_is_retiring(ubo));
					CHECK(MaterialStorage::material_native_hold_release(first) == OK);
					TestMaterialStorageNativeHoldDeviceAccess::drain(rd);
					CHECK((driver.buffer_frees == 0 && driver.set_frees == 0 && driver.texture_frees == 0));
					CHECK(second.get_snapshot().uniform_bytes.size() == 16);
					if (!second.get_snapshot().uniform_bytes.is_empty()) {
						CHECK(second.get_snapshot().uniform_bytes[0] == 42);
					}
					CHECK(MaterialStorage::material_native_hold_release(second) == OK);
					CHECK(MaterialStorage::material_native_hold_release(second) == ERR_INVALID_PARAMETER);
					if (callback_lifetime == 2) {
						CHECK(callback_trace.calls == 1);
						CHECK_FALSE(callback_trace.alive);
					}
				}
			}
			if (!storage->owns_material(material)) {
				goto finished;
			}
		}
		if (storage->owns_material(material)) {
			storage->material_free(material);
		}
		storage->shader_free(shader);
		textures->texture_free(texture);
		rd.free_rid(native_parent);
		rd.free_rid(sampler);
	finished:
		TestMaterialStorageNativeHoldDeviceAccess::drain(rd);
		CHECK((driver.buffer_frees == 1 && driver.set_frees == 1 && driver.texture_frees == 2 && driver.sampler_frees == 1 && driver.shader_frees == (cross_variants ? 3u : 1u) && driver.pipeline_frees == (cross_variants ? 3u : 1u)));
		CHECK(TestMaterialStorageNativeHoldDeviceAccess::empty(rd));
		memdelete(storage);
		MaterialStorage::singleton = previous_storage;
		memdelete(textures);
		TextureStorage::singleton = previous_texture_storage;
		TestMaterialStorageNativeHoldDeviceAccess::detach(rd);
		// The RenderingDevice fixture owns no native driver after original frees.
		TestMaterialStorageNativeHoldDeviceAccess::restore(previous);
	}
};
void *selection_materials_create(uint32_t material_count) {
	return TestMaterialStorageNativeHoldAccess::selection_create(material_count);
}
RID selection_material_rid(void *p, uint32_t i) {
	return TestMaterialStorageNativeHoldAccess::selection_rid(p, i);
}
uint32_t selection_material_holds(void *p, uint32_t i) {
	return TestMaterialStorageNativeHoldAccess::selection_holds(p, i);
}
uint32_t selection_shader_holds(void *p) {
	return TestMaterialStorageNativeHoldAccess::selection_shader_count(p);
}
void selection_materials_destroy(void *p) {
	TestMaterialStorageNativeHoldAccess::selection_destroy(p);
}
void *pipeline_material_profile_create(RID shader) {
	return TestMaterialStorageNativeHoldAccess::create_pipeline_profile(shader);
}
Error pipeline_material_profile_acquire(void *profile, RID pipeline, uint32_t max_resources, MaterialStorage::NativeMaterialHold &hold) {
	return TestMaterialStorageNativeHoldAccess::acquire_pipeline_profile(profile, pipeline, max_resources, hold);
}
Error pipeline_material_profile_capture(GDExtensionProductionPhysicalContextRefV1 context, GDExtensionDeviceExit::ProductionOutputPin &pin, MaterialStorage::NativeMaterialHold &hold, uint64_t &group) {
	return TestMaterialStorageNativeHoldAccess::capture_pipeline_profile(context, pin, hold, group);
}
void pipeline_material_profile_destroy(void *profile) {
	TestMaterialStorageNativeHoldAccess::destroy_pipeline_profile(profile);
}

} // namespace RendererRD

TEST_CASE("[MaterialNativeHold] exact native inputs survive original owner free until last hold") {
	SUBCASE("original exact inputs and physical return") {
		RendererRD::TestMaterialStorageNativeHoldAccess::run(false);
	}
	SUBCASE("same-version variants require actual compatible set format") {
		RendererRD::TestMaterialStorageNativeHoldAccess::run(false, true);
	}
	SUBCASE("logical owner destruction clears held native callback userdata") {
		RendererRD::TestMaterialStorageNativeHoldAccess::run(false, false, 1);
	}
	SUBCASE("direct held set free consumes callback before userdata destruction") {
		RendererRD::TestMaterialStorageNativeHoldAccess::run(false, false, 2);
	}
	SUBCASE("ordinary unheld invalidation callback runs outside the actual RD mutex") {
		RendererRD::TestMaterialStorageNativeHoldAccess::run(false, false, 4);
	}
	SUBCASE("callback returns its last hold and destroys packet before outer physical free") {
		RendererRD::TestMaterialStorageNativeHoldAccess::run(false, false, 3);
	}
	SUBCASE("shared-view export is sticky on its physical root and siblings") {
		RendererRD::TestMaterialStorageNativeHoldAccess::run_texture_profile(1);
	}
	SUBCASE("held parent blocks existing sibling export and writes") {
		RendererRD::TestMaterialStorageNativeHoldAccess::run_texture_profile(2);
	}
	SUBCASE("depth-resolve attachment profile cannot become immutable sampling") {
		RendererRD::TestMaterialStorageNativeHoldAccess::run_texture_profile(3);
	}
	SUBCASE("unsettled original texture worker prevents immutable capture") {
		RendererRD::TestMaterialStorageNativeHoldAccess::run_texture_profile(4);
	}
	SUBCASE("factory immutable sampler belongs to shader closure until final return") {
		RendererRD::TestMaterialStorageNativeHoldAccess::run_sampler_profile(1);
	}
	SUBCASE("ordinary shader does not retain an unrelated sampler") {
		RendererRD::TestMaterialStorageNativeHoldAccess::run_sampler_profile(0);
	}
	SUBCASE("escaped factory immutable sampler rejects future shader hold") {
		RendererRD::TestMaterialStorageNativeHoldAccess::run_sampler_profile(2);
	}
}

TEST_CASE("[MaterialNativeHold] unsupported dynamic profile rejects before native pins") {
	RendererRD::TestMaterialStorageNativeHoldAccess::run(true);
}

TEST_CASE("[MaterialNativeHold] shader edits invalidate selected material generation before native hold") {
	RendererRD::TestMaterialStorageNativeHoldAccess::run(false, false, 0, true);
}

TEST_CASE("[MaterialNativeHold][PreparationData] original material lookup preserves validation order and clears failed output") {
	RendererRD::TestMaterialStorageNativeHoldAccess::run_preparation_data_boundary();
}
