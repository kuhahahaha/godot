/**************************************************************************/
/*  rendering_device.cpp                                                  */
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

#include "rendering_device.h"
#include "rendering_device.compat.inc"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/extension/gdextension_external_output_interface.h"
#include "core/extension/gdextension_library_exit_guard.h"
#include "core/extension/gdextension_production_request.h"
#include "core/extension/gdextension_qualification_internal.h"
#include "core/extension/gdextension_record_source.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/native_drain_diagnostic.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "core/profiling/profiling.h"
#include "core/templates/fixed_vector.h"
#include "servers/rendering/renderer_rd/storage_rd/particles_storage.h"
#include "servers/rendering/renderer_rd/height_capture_checked_sink.h"
#include "servers/rendering/rendering_device_binds.h"
#include "servers/rendering/rendering_shader_container.h"
#include "servers/rendering/shader_include_db.h"

#include "modules/modules_enabled.gen.h"

#if defined(TESTS_ENABLED) && defined(MODULE_SKINNED_MESH_CAPTURE_TESTS_ENABLED)
#include "servers/rendering/render_capture_test_hooks.h"
#endif

#ifdef MODULE_GLSLANG_ENABLED
#include "modules/glslang/shader_compile.h"
#endif

#define FORCE_SEPARATE_PRESENT_QUEUE 0
#define PRINT_FRAMEBUFFER_FORMAT 0

#define ERR_RENDER_THREAD_MSG String("This function (") + String(__func__) + String(") can only be called from the render thread. ")
#define ERR_RENDER_THREAD_GUARD() ERR_FAIL_COND_MSG(render_thread_id != Thread::get_caller_id(), ERR_RENDER_THREAD_MSG);
#define ERR_RENDER_THREAD_GUARD_V(m_ret) ERR_FAIL_COND_V_MSG(render_thread_id != Thread::get_caller_id(), (m_ret), ERR_RENDER_THREAD_MSG);

/**************************/
/**** HELPER FUNCTIONS ****/
/**************************/

static String _get_device_vendor_name(const RenderingContextDriver::Device &p_device) {
	switch (p_device.vendor) {
		case RenderingContextDriver::Vendor::VENDOR_AMD:
			return "AMD";
		case RenderingContextDriver::Vendor::VENDOR_IMGTEC:
			return "ImgTec";
		case RenderingContextDriver::Vendor::VENDOR_APPLE:
			return "Apple";
		case RenderingContextDriver::Vendor::VENDOR_NVIDIA:
			return "NVIDIA";
		case RenderingContextDriver::Vendor::VENDOR_ARM:
			return "ARM";
		case RenderingContextDriver::Vendor::VENDOR_MICROSOFT:
			return "Microsoft";
		case RenderingContextDriver::Vendor::VENDOR_QUALCOMM:
			return "Qualcomm";
		case RenderingContextDriver::Vendor::VENDOR_INTEL:
			return "Intel";
		default:
			return "Unknown";
	}
}

static String _get_device_type_name(const RenderingContextDriver::Device &p_device) {
	switch (p_device.type) {
		case RenderingContextDriver::DEVICE_TYPE_INTEGRATED_GPU:
			return "Integrated";
		case RenderingContextDriver::DEVICE_TYPE_DISCRETE_GPU:
			return "Discrete";
		case RenderingContextDriver::DEVICE_TYPE_VIRTUAL_GPU:
			return "Virtual";
		case RenderingContextDriver::DEVICE_TYPE_CPU:
			return "CPU";
		case RenderingContextDriver::DEVICE_TYPE_OTHER:
		default:
			return "Other";
	}
}

static uint32_t _get_device_type_score(const RenderingContextDriver::Device &p_device) {
	static const bool prefer_integrated = OS::get_singleton()->get_user_prefers_integrated_gpu();
	switch (p_device.type) {
		case RenderingContextDriver::DEVICE_TYPE_INTEGRATED_GPU:
			return prefer_integrated ? 5 : 4;
		case RenderingContextDriver::DEVICE_TYPE_DISCRETE_GPU:
			return prefer_integrated ? 4 : 5;
		case RenderingContextDriver::DEVICE_TYPE_VIRTUAL_GPU:
			return 3;
		case RenderingContextDriver::DEVICE_TYPE_CPU:
			return 2;
		case RenderingContextDriver::DEVICE_TYPE_OTHER:
		default:
			return 1;
	}
}

static uint32_t _decode_hit_sbt_range_offset(RD::HitShaderBindingTableRange p_range) {
	return uint32_t(uint64_t(p_range) & 0xFFFFFFFF);
}

static uint32_t _decode_hit_sbt_range_count(RD::HitShaderBindingTableRange p_range) {
	return uint32_t((uint64_t(p_range) >> 32) & 0xFFFFFFFF);
}

static RD::HitShaderBindingTableRange _encode_hit_sbt_range(uint32_t p_offset, uint32_t p_count) {
	return RD::HitShaderBindingTableRange((uint64_t(p_count) << 32) | uint64_t(p_offset));
}

/**************************/
/**** RENDERING DEVICE ****/
/**************************/

// When true, the command graph will attempt to reorder the rendering commands submitted by the user based on the dependencies detected from
// the commands automatically. This should improve rendering performance in most scenarios at the cost of some extra CPU overhead.
//
// This behavior can be disabled if it's suspected that the graph is not detecting dependencies correctly and more control over the order of
// the commands is desired (e.g. debugging).

#define RENDER_GRAPH_REORDER 1

// Synchronization barriers are issued between the graph's levels only with the necessary amount of detail to achieve the correct result. If
// it's suspected that the graph is not doing this correctly, full barriers can be issued instead that will block all types of operations
// between the synchronization levels. This setting will have a very negative impact on performance when enabled, so it's only intended for
// debugging purposes.

#define RENDER_GRAPH_FULL_BARRIERS 0

// The command graph can automatically issue secondary command buffers and record them on background threads when they reach an arbitrary
// size threshold. This can be very beneficial towards reducing the time the main thread takes to record all the rendering commands. However,
// this setting is not enabled by default as it's been shown to cause some strange issues with certain IHVs that have yet to be understood.

#define SECONDARY_COMMAND_BUFFERS_PER_FRAME 0

RenderingDevice *RenderingDevice::singleton = nullptr;

RenderingDevice *RenderingDevice::get_singleton() {
	return singleton;
}

/***************************/
/**** ID INFRASTRUCTURE ****/
/***************************/

void RenderingDevice::_add_dependency(RID p_id, RID p_depends_on) {
	_THREAD_SAFE_METHOD_

	HashSet<RID> *set = dependency_map.getptr(p_depends_on);
	if (set == nullptr) {
		set = &dependency_map.insert(p_depends_on, HashSet<RID>())->value;
	}
	set->insert(p_id);

	set = reverse_dependency_map.getptr(p_id);
	if (set == nullptr) {
		set = &reverse_dependency_map.insert(p_id, HashSet<RID>())->value;
	}
	set->insert(p_depends_on);
}

void RenderingDevice::_remove_dependency(RID p_id, RID p_depends_on) {
	_THREAD_SAFE_METHOD_

	HashSet<RID> *set = dependency_map.getptr(p_depends_on);
	if (set != nullptr) {
		set->erase(p_id);
	}

	set = reverse_dependency_map.getptr(p_id);
	if (set != nullptr) {
		set->erase(p_depends_on);
	}
}

bool RenderingDevice::_free_dependencies(RID p_id, const MutexLock<Mutex> &p_outer_lock) {
	MutexLock lock(_thread_safe_);

	// Direct dependencies must be freed.
	while (true) {
		HashMap<RID, HashSet<RID>>::Iterator E = dependency_map.find(p_id);
		if (!E) {
			break;
		}
		if (E->value.is_empty()) {
			dependency_map.remove(E);
			break;
		}
		if (native_disposal_budget && !_native_disposal_admit(NativeDisposalWork::WORK)) {
			return false;
		}
		const RID dependent = *E->value.begin();
		if (production_disposal_drop_owner && !_production_disposal_belongs(dependent, production_disposal_drop_owner)) {
			return false;
		}
		if (resource_frees_in_progress.has(dependent)) {
			return false; // The enclosing original free owns this edge until its callback returns.
		}
		// Each child owns its own lock. Its synchronous callback must not inherit
		// this dependency traversal's recursive RD lock. Re-find the map afterward.
		lock.temp_unlock();
		p_outer_lock.temp_unlock();
		free_rid(dependent);
		p_outer_lock.temp_relock();
		lock.temp_relock();
		// A synchronous invalidation callback can mutate the dependency map.
		E = dependency_map.find(p_id);
		if (E && E->value.has(dependent) && native_disposal_budget) {
			return false;
		}
		ERR_FAIL_COND_V_MSG(E && E->value.has(dependent), false, "A dependent resource could not be freed.");
	}

	// A dependent's synchronous invalidation callback can acquire this resource
	// after the initial busy check. Preserve its parent edges if that happened.
	ERR_FAIL_COND_V_MSG(_qualification_resource_busy(p_id), false, "The resource became busy during dependent cleanup.");
	ERR_FAIL_COND_V(native_immutable_resource_is_held(p_id), false);

	// Reverse dependencies must be unreferenced.
	HashMap<RID, HashSet<RID>>::Iterator E = reverse_dependency_map.find(p_id);

	if (E) {
		for (const RID &F : E->value) {
			HashMap<RID, HashSet<RID>>::Iterator G = dependency_map.find(F);
			ERR_CONTINUE(!G);
			ERR_CONTINUE(!G->value.has(p_id));
			G->value.erase(p_id);
		}

		reverse_dependency_map.remove(E);
	}
	return true;
}

/*******************************/
/**** SHADER INFRASTRUCTURE ****/
/*******************************/

Vector<uint8_t> RenderingDevice::shader_compile_spirv_from_source(ShaderStage p_stage, const String &p_source_code, ShaderLanguage p_language, String *r_error, bool p_allow_cache) {
	switch (p_language) {
#ifdef MODULE_GLSLANG_ENABLED
		case ShaderLanguage::SHADER_LANGUAGE_GLSL: {
			ShaderLanguageVersion language_version = driver->get_shader_container_format().get_shader_language_version();
			ShaderSpirvVersion spirv_version = driver->get_shader_container_format().get_shader_spirv_version();
			return compile_glslang_shader(p_stage, ShaderIncludeDB::parse_include_files(p_source_code), language_version, spirv_version, r_error);
		}
#endif
		default:
			ERR_FAIL_V_MSG(Vector<uint8_t>(), "Shader language is not supported.");
	}
}

RID RenderingDevice::shader_create_from_spirv(const Vector<ShaderStageSPIRVData> &p_spirv, const String &p_shader_name) {
	return shader_create_from_spirv_with_options(p_spirv, p_shader_name, {});
}

RID RenderingDevice::shader_create_from_spirv_with_options(const Vector<ShaderStageSPIRVData> &p_spirv, const String &p_shader_name, BitField<ShaderCompileFlags> p_flags) {
	Vector<uint8_t> bytecode = shader_compile_binary_from_spirv_with_options(p_spirv, p_shader_name, p_flags);
	ERR_FAIL_COND_V(bytecode.is_empty(), RID());
	return shader_create_from_bytecode(bytecode);
}

/********************************/
/**** ACCELERATION STRUCTURE ****/
/********************************/

Error RenderingDevice::_acceleration_structure_scratch_buffer_create(AccelerationStructure *p_acceleration_structure) {
	uint64_t scratch_size = driver->acceleration_structure_get_scratch_size_bytes(p_acceleration_structure->driver_id);

	if (p_acceleration_structure->scratch_buffer) {
		uint64_t scratch_buffer_size = driver->buffer_get_allocation_size(p_acceleration_structure->scratch_buffer);
		if (scratch_buffer_size < scratch_size) {
			Buffer to_dispose = Buffer();
			to_dispose.driver_id = p_acceleration_structure->scratch_buffer;
			DEV_ASSERT(scratch_buffer_size <= UINT32_MAX);
			to_dispose.size = scratch_buffer_size;
			frames[frame].buffers_to_dispose_of.push_back(to_dispose);
			p_acceleration_structure->scratch_buffer = RDD::BufferID();
		}
	}

	if (!p_acceleration_structure->scratch_buffer) {
		p_acceleration_structure->scratch_buffer = driver->buffer_create(scratch_size, RDD::BUFFER_USAGE_STORAGE_BIT | RDD::BUFFER_USAGE_DEVICE_ADDRESS_BIT, RDD::MEMORY_ALLOCATION_TYPE_GPU, frames_drawn);
		ERR_FAIL_COND_V(!p_acceleration_structure->scratch_buffer, ERR_CANT_CREATE);

		_THREAD_SAFE_LOCK_
		buffer_memory += scratch_size;
		_THREAD_SAFE_UNLOCK_
	}

	return OK;
}

void RenderingDevice::_blas_remove_tlas_dependencies(AccelerationStructure *p_blas, RID p_blas_id) {
	for (RID id : p_blas->acceleration_structure_dependencies) {
		AccelerationStructure *tlas = acceleration_structure_owner.get_or_null(id);
		ERR_FAIL_NULL(tlas);
		ERR_FAIL_COND(tlas->type != ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL);

		tlas->invalidated = true;
		tlas->acceleration_structure_dependencies.erase(p_blas_id);
	}

	p_blas->acceleration_structure_dependencies.clear();
}

void RenderingDevice::_tlas_remove_blas_dependencies(AccelerationStructure *p_tlas, RID p_tlas_id) {
	for (RID id : p_tlas->acceleration_structure_dependencies) {
		AccelerationStructure *blas = acceleration_structure_owner.get_or_null(id);
		ERR_FAIL_NULL(blas);
		ERR_FAIL_COND(blas->type != ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL);

		blas->acceleration_structure_dependencies.erase(p_tlas_id);
	}

	p_tlas->acceleration_structure_dependencies.clear();
}

RID RenderingDevice::blas_create(Span<AccelerationStructureGeometry> p_geometries, BitField<AccelerationStructureFlagBits> p_flags) {
	ERR_FAIL_COND_V_MSG(!has_feature(SUPPORTS_RAYTRACING_PIPELINE) && !has_feature(SUPPORTS_RAY_QUERY), RID(), "The current rendering device has neither raytracing pipeline nor ray query support.");

	thread_local LocalVector<RDD::AccelerationStructureGeometry> rdd_geometries;
	rdd_geometries.resize(p_geometries.size());

	Vector<RDG::ResourceTracker *> draw_trackers;
	HashSet<RID> untracked_buffers;

	for (uint64_t i = 0; i < p_geometries.size(); i++) {
		const AccelerationStructureGeometry &rd_geometry = p_geometries[i];
		if (rd_geometry.index_buffer.is_valid()) {
			ERR_FAIL_COND_V_MSG((rd_geometry.index_count % 3) != 0, RID(), "An indexed geometry must have an index count that is a multiple of 3.");
		} else {
			ERR_FAIL_COND_V_MSG((rd_geometry.vertex_count % 3) != 0, RID(), "A non-indexed geometry must have a vertex count that is a multiple of 3.");
		}

		RDD::AccelerationStructureGeometry &rdd_geometry = rdd_geometries[i];
		rdd_geometry = {};
		rdd_geometry.flags = rd_geometry.flags;

		Buffer *vertex_buffer = vertex_buffer_owner.get_or_null(rd_geometry.vertex_buffer);
		ERR_FAIL_NULL_V(vertex_buffer, RID());

		ERR_FAIL_COND_V_MSG((rd_geometry.vertex_offset + rd_geometry.vertex_count * rd_geometry.vertex_stride) > vertex_buffer->size, RID(), "The specified vertex offset and count are outside the range of the vertex buffer.");
		ERR_FAIL_COND_V_MSG(rd_geometry.vertex_format >= DataFormat::DATA_FORMAT_MAX, RID(), "An invalid vertex format was specified.");

		rdd_geometry.vertex_buffer = vertex_buffer->driver_id;
		rdd_geometry.vertex_offset = rd_geometry.vertex_offset;
		rdd_geometry.vertex_stride = rd_geometry.vertex_stride;
		rdd_geometry.vertex_count = rd_geometry.vertex_count;
		rdd_geometry.vertex_format = rd_geometry.vertex_format;

		if (vertex_buffer->draw_tracker != nullptr) {
			draw_trackers.push_back(vertex_buffer->draw_tracker);
		} else {
			untracked_buffers.insert(rd_geometry.vertex_buffer);
		}

		{
			const Error dependency_error = _check_transfer_worker_buffer(vertex_buffer);
			if (dependency_error != OK) {
				return RID();
			}
		}

		if (rd_geometry.index_buffer.is_valid()) {
			IndexBuffer *index_buffer = index_buffer_owner.get_or_null(rd_geometry.index_buffer);
			ERR_FAIL_NULL_V(index_buffer, RID());

			uint32_t index_stride = (index_buffer->format == INDEX_BUFFER_FORMAT_UINT32 ? sizeof(uint32_t) : sizeof(uint16_t));
			ERR_FAIL_COND_V_MSG((rd_geometry.index_offset + rd_geometry.index_count * index_stride) > index_buffer->size, RID(), "The specified index offset and count are outside the range of the index buffer.");
			ERR_FAIL_COND_V_MSG(index_buffer->max_index >= rd_geometry.vertex_count, RID(), "The index buffer contains an index that is outside the specified vertex range.");

			rdd_geometry.index_buffer = index_buffer->driver_id;
			rdd_geometry.index_offset = rd_geometry.index_offset;
			rdd_geometry.index_count = rd_geometry.index_count;
			rdd_geometry.index_format = index_buffer->format;

			if (index_buffer->draw_tracker != nullptr) {
				draw_trackers.push_back(index_buffer->draw_tracker);
			} else {
				untracked_buffers.insert(rd_geometry.index_buffer);
			}

			{
				const Error dependency_error = _check_transfer_worker_buffer(index_buffer);
				if (dependency_error != OK) {
					return RID();
				}
			}
		}
	}

	AccelerationStructure acceleration_structure;
	acceleration_structure.type = RDD::ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
	acceleration_structure.driver_id = driver->blas_create(rdd_geometries, p_flags);
	ERR_FAIL_COND_V_MSG(!acceleration_structure.driver_id, RID(), "Failed to create BLAS.");

	acceleration_structure.draw_tracker = RDG::resource_tracker_create();
	acceleration_structure.draw_tracker->acceleration_structure_driver_id = acceleration_structure.driver_id;
	// Assume we are going to build this acceleration structure
	acceleration_structure.draw_tracker->usage = RDG::RESOURCE_USAGE_ACCELERATION_STRUCTURE_READ_WRITE;

	acceleration_structure.draw_trackers = std::move(draw_trackers);
	acceleration_structure.untracked_buffers = std::move(untracked_buffers);

	RID id = acceleration_structure_owner.make_rid(acceleration_structure);
#ifdef DEV_ENABLED
	set_resource_name(id, "RID:" + itos(id.get_id()));
#endif

	for (uint32_t i = 0; i < p_geometries.size(); i++) {
		const AccelerationStructureGeometry &geometry = p_geometries[i];
		_add_dependency(id, geometry.vertex_buffer);

		if (geometry.index_buffer.is_valid()) {
			_add_dependency(id, geometry.index_buffer);
		}
	}

	return id;
}

BitField<RDD::BufferUsageBits> RenderingDevice::_creation_to_usage_bits(BitField<RD::BufferCreationBits> p_creation_bits) {
	BitField<RDD::BufferUsageBits> usage = 0;

	if (p_creation_bits.has_flag(BUFFER_CREATION_AS_STORAGE_BIT)) {
		usage.set_flag(RDD::BUFFER_USAGE_STORAGE_BIT);
	}

	if (p_creation_bits.has_flag(BUFFER_CREATION_DEVICE_ADDRESS_BIT)) {
#ifdef DEBUG_ENABLED
		ERR_FAIL_COND_V_MSG(!has_feature(SUPPORTS_BUFFER_DEVICE_ADDRESS), 0,
				"The GPU doesn't support buffer address flag.");
#endif
		usage.set_flag(RDD::BUFFER_USAGE_DEVICE_ADDRESS_BIT);
	}

	if (p_creation_bits.has_flag(BUFFER_CREATION_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT)) {
#ifdef DEBUG_ENABLED
		ERR_FAIL_COND_V_MSG(!has_feature(SUPPORTS_RAYTRACING_PIPELINE) && !has_feature(SUPPORTS_RAY_QUERY), 0,
				"The GPU doesn't support acceleration structure build input flag.");
#endif
		usage.set_flag(RDD::BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT);
	}

	return usage;
}

RID RenderingDevice::tlas_create(uint32_t p_max_instance_count, BitField<AccelerationStructureFlagBits> p_flags) {
	ERR_FAIL_COND_V_MSG(!has_feature(SUPPORTS_RAYTRACING_PIPELINE) && !has_feature(SUPPORTS_RAY_QUERY), RID(), "The current rendering device has neither raytracing pipeline nor ray query support.");

	AccelerationStructure acceleration_structure;
	acceleration_structure.type = RDD::ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	acceleration_structure.driver_id = driver->tlas_create(p_max_instance_count, p_flags);
	ERR_FAIL_COND_V_MSG(!acceleration_structure.driver_id, RID(), "Failed to create TLAS.");

	acceleration_structure.draw_tracker = RDG::resource_tracker_create();
	acceleration_structure.draw_tracker->acceleration_structure_driver_id = acceleration_structure.driver_id;
	// Assume we are going to build this acceleration structure
	acceleration_structure.draw_tracker->usage = RDG::RESOURCE_USAGE_ACCELERATION_STRUCTURE_READ_WRITE;

	acceleration_structure.max_instance_count = p_max_instance_count;

	RID id = acceleration_structure_owner.make_rid(acceleration_structure);
#ifdef DEV_ENABLED
	set_resource_name(id, "RID:" + itos(id.get_id()));
#endif
	return id;
}

Error RenderingDevice::blas_build(RID p_blas) {
	ERR_RENDER_THREAD_GUARD_V(ERR_UNAVAILABLE);
	if (_recording_error() != OK) {
		return _recording_error();
	}

	ERR_FAIL_COND_V_MSG(draw_list.active, ERR_INVALID_PARAMETER, "Building BLAS is forbidden during creation of a draw list.");
	ERR_FAIL_COND_V_MSG(compute_list.active, ERR_INVALID_PARAMETER, "Building BLAS is forbidden during creation of a compute list.");
	ERR_FAIL_COND_V_MSG(raytracing_list.active, ERR_INVALID_PARAMETER, "Building BLAS is forbidden during creation of a raytracing list.");

	AccelerationStructure *blas = acceleration_structure_owner.get_or_null(p_blas);
	ERR_FAIL_NULL_V_MSG(blas, ERR_INVALID_PARAMETER, "BLAS argument is not valid.");

	Error err = _acceleration_structure_scratch_buffer_create(blas);
	ERR_FAIL_COND_V(err != OK, err);

	draw_graph.add_blas_build(blas->driver_id, blas->scratch_buffer, blas->draw_tracker, blas->draw_trackers);

	blas->invalidated = false;
	_blas_remove_tlas_dependencies(blas, p_blas);

	return OK;
}

Error RenderingDevice::tlas_build(RID p_tlas, Span<AccelerationStructureInstance> p_instances) {
	ERR_RENDER_THREAD_GUARD_V(ERR_UNAVAILABLE);
	if (_recording_error() != OK) {
		return _recording_error();
	}

	ERR_FAIL_COND_V_MSG(draw_list.active, ERR_INVALID_PARAMETER, "Building TLAS is forbidden during creation of a draw list.");
	ERR_FAIL_COND_V_MSG(compute_list.active, ERR_INVALID_PARAMETER, "Building TLAS is forbidden during creation of a compute list.");
	ERR_FAIL_COND_V_MSG(raytracing_list.active, ERR_INVALID_PARAMETER, "Building TLAS is forbidden during creation of a raytracing list.");

	AccelerationStructure *tlas = acceleration_structure_owner.get_or_null(p_tlas);
	ERR_FAIL_NULL_V_MSG(tlas, ERR_INVALID_PARAMETER, "TLAS argument is not valid.");
	ERR_FAIL_COND_V_MSG(p_instances.size() > tlas->max_instance_count, ERR_INVALID_PARAMETER, "Exceeded the maximum amount of instances allowed in the TLAS.");

	Error err = _acceleration_structure_scratch_buffer_create(tlas);
	ERR_FAIL_COND_V(err != OK, err);

	uint32_t instance_size = driver->api_trait_get(RDD::API_TRAIT_ACCELERATION_STRUCTURE_INSTANCE_SIZE);
	uint32_t instance_buffer_size = instance_size * tlas->max_instance_count;

	uint32_t instance_buffer_offset = 0;
	uint32_t instance_buffer_used_size = instance_size * p_instances.size();

	uint32_t instance_buffer_index = UINT32_MAX;
	for (uint32_t i = 0; i < tlas->instance_buffers.size(); i++) {
		AccelerationStructure::InstanceBuffer &instance_buffer = tlas->instance_buffers[i];

		if ((frames_drawn - instance_buffer.frame_used) >= frames.size()) {
			instance_buffer.frame_used = frames_drawn;
			instance_buffer.used_size = 0;
		}

		if (frames_drawn == instance_buffer.frame_used && (instance_buffer.used_size + instance_buffer_used_size) <= instance_buffer_size) {
			instance_buffer_offset = instance_buffer.used_size;
			instance_buffer.used_size += instance_buffer_used_size;
			instance_buffer_index = i;
			break;
		}
	}

	if (instance_buffer_index == UINT32_MAX) {
		RDD::BufferID instance_buffer_driver_id = driver->buffer_create(instance_buffer_size, RDD::BUFFER_USAGE_TRANSFER_FROM_BIT | RDD::BUFFER_USAGE_DEVICE_ADDRESS_BIT | RDD::BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT, RDD::MEMORY_ALLOCATION_TYPE_CPU, frames_drawn);
		ERR_FAIL_COND_V(!instance_buffer_driver_id, ERR_CANT_CREATE);

		uint8_t *data_ptr = driver->buffer_map(instance_buffer_driver_id);
		if (unlikely(!data_ptr)) {
			driver->buffer_free(instance_buffer_driver_id);
			ERR_FAIL_V(ERR_CANT_CREATE);
		}

		_THREAD_SAFE_LOCK_
		buffer_memory += instance_buffer_size;
		_THREAD_SAFE_UNLOCK_

		AccelerationStructure::InstanceBuffer instance_buffer;
		instance_buffer.driver_id = instance_buffer_driver_id;
		instance_buffer.frame_used = frames_drawn;
		instance_buffer.used_size = 0;
		instance_buffer.data_ptr = data_ptr;

		instance_buffer_index = tlas->instance_buffers.size();
		tlas->instance_buffers.push_back(instance_buffer);
	}

	AccelerationStructure::InstanceBuffer &instance_buffer = tlas->instance_buffers[instance_buffer_index];

	_tlas_remove_blas_dependencies(tlas, p_tlas);

	thread_local LocalVector<RDG::ResourceTracker *> draw_trackers;
	draw_trackers.clear();

	for (uint32_t i = 0; i < p_instances.size(); i++) {
		const AccelerationStructureInstance &rd_instance = p_instances[i];

		RDD::AccelerationStructureInstance rdd_instance;
		rdd_instance.transform = rd_instance.transform;
		rdd_instance.id = rd_instance.id;
		rdd_instance.mask = rd_instance.mask;
		rdd_instance.hit_sbt_offset = _decode_hit_sbt_range_offset(rd_instance.hit_sbt_range);
		rdd_instance.flags = rd_instance.flags;

		if (rd_instance.blas.is_valid()) {
			ERR_FAIL_COND_V_MSG(!rd_instance.hit_sbt_range, ERR_INVALID_PARAMETER, "Instance " + itos(i) + " has an invalid hit shader binding table range.");

			AccelerationStructure *blas = acceleration_structure_owner.get_or_null(rd_instance.blas);
			ERR_FAIL_NULL_V(blas, ERR_INVALID_PARAMETER);
			ERR_FAIL_COND_V(blas->type != RDD::ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL, ERR_INVALID_PARAMETER);
			ERR_FAIL_COND_V_MSG(blas->invalidated, ERR_INVALID_PARAMETER, "BLAS either has not been built yet, or has been invalidated by an operation and needs to be rebuilt.");
			rdd_instance.blas = blas->driver_id;

			if (!tlas->acceleration_structure_dependencies.has(rd_instance.blas)) {
				tlas->acceleration_structure_dependencies.insert(rd_instance.blas);
				blas->acceleration_structure_dependencies.insert(p_tlas);

				if (blas->draw_tracker != nullptr) {
					draw_trackers.push_back(blas->draw_tracker);
				}
			}
		}

		driver->acceleration_structure_instance_write(instance_buffer.data_ptr + instance_buffer_offset + (instance_size * i), rdd_instance);
	}

	draw_graph.add_tlas_build(tlas->driver_id, tlas->scratch_buffer, instance_buffer.driver_id, instance_buffer_offset, p_instances.size(), tlas->draw_tracker, draw_trackers);

	tlas->invalidated = false;

	return OK;
}

/**********************************/
/**** HIT SHADER BINDING TABLE ****/
/**********************************/

RDD::BufferID RenderingDevice::_hit_sbt_buffer_create(uint32_t p_buffer_size) {
	RDD::BufferID buffer = driver->buffer_create(p_buffer_size, RDD::BUFFER_USAGE_TRANSFER_TO_BIT | RDD::BUFFER_USAGE_DEVICE_ADDRESS_BIT | RDD::BUFFER_USAGE_SHADER_BINDING_TABLE_BIT, RDD::MEMORY_ALLOCATION_TYPE_GPU, frames_drawn);
	if (buffer) {
		_THREAD_SAFE_LOCK_
		buffer_memory += p_buffer_size;
		_THREAD_SAFE_UNLOCK_
	}

	return buffer;
}

Error RenderingDevice::_hit_sbt_buffer_update(HitShaderBindingTable *p_hit_sbt, RID p_hit_sbt_id, RDD::ShaderBindingTable &r_sbt) {
	uint32_t shader_group_handle_size = driver->api_trait_get(RDD::API_TRAIT_SHADER_GROUP_HANDLE_SIZE);
	uint32_t shader_group_handle_alignment = driver->api_trait_get(RDD::API_TRAIT_SHADER_GROUP_HANDLE_ALIGNMENT);
	uint32_t shader_group_stride = STEPIFY(shader_group_handle_size, shader_group_handle_alignment);

	uint32_t buffer_size = p_hit_sbt->hit_group_indices.size() * shader_group_stride;

	if (p_hit_sbt->size != buffer_size) {
		RDD::BufferID buffer = _hit_sbt_buffer_create(buffer_size);
		ERR_FAIL_COND_V(!buffer, ERR_CANT_CREATE);

		RDG::resource_tracker_free(p_hit_sbt->draw_tracker);
		_production_disposal_enqueued(p_hit_sbt->production_disposal_uses);
		frames[frame].buffers_to_dispose_of.push_back(*p_hit_sbt);

		p_hit_sbt->driver_id = buffer;
		p_hit_sbt->size = buffer_size;
		p_hit_sbt->draw_tracker = RDG::resource_tracker_create();
		p_hit_sbt->draw_tracker->buffer_driver_id = buffer;

		_hit_sbt_add_dirty_range(p_hit_sbt, 0, p_hit_sbt->used_hit_group_count);
	}

	if (p_hit_sbt->first_dirty_index != UINT32_MAX) {
		uint32_t count = p_hit_sbt->last_dirty_index - p_hit_sbt->first_dirty_index;
		uint32_t offset = shader_group_stride * p_hit_sbt->first_dirty_index;
		uint32_t size = shader_group_stride * count;

		thread_local LocalVector<uint8_t> data;
		data.resize(size);

		bool success = driver->raytracing_pipeline_get_shader_group_handles(
				p_hit_sbt->raytracing_pipeline,
				p_hit_sbt->index_offset,
				VectorView<uint32_t>(p_hit_sbt->hit_group_indices.ptr() + p_hit_sbt->first_dirty_index, count),
				data.ptr(),
				shader_group_stride);
		ERR_FAIL_COND_V(!success, ERR_CANT_CREATE);

		Error err = _buffer_update(p_hit_sbt, p_hit_sbt_id, offset, size, data.ptr());
		ERR_FAIL_COND_V(err != OK, err);

		p_hit_sbt->first_dirty_index = UINT32_MAX;
		p_hit_sbt->last_dirty_index = 0;
	}

	r_sbt.buffer = p_hit_sbt->driver_id;
	r_sbt.offset = 0;
	r_sbt.stride = shader_group_stride;
	r_sbt.size = buffer_size;

	return OK;
}

void RenderingDevice::_hit_sbt_add_dirty_range(HitShaderBindingTable *p_hit_sbt, uint32_t p_offset, uint32_t p_count) {
	p_hit_sbt->first_dirty_index = MIN(p_hit_sbt->first_dirty_index, p_offset);
	p_hit_sbt->last_dirty_index = MAX(p_hit_sbt->last_dirty_index, p_offset + p_count);
}

RID RenderingDevice::hit_sbt_create(RID p_raytracing_pipeline, uint32_t p_initial_hit_group_capacity) {
	ERR_FAIL_COND_V_MSG(_recording_error() != OK, RID(), "Resource recording is suspended during frame fence recovery or readback callbacks.");
	ERR_FAIL_COND_V_MSG(!has_feature(SUPPORTS_RAYTRACING_PIPELINE) && !has_feature(SUPPORTS_RAY_QUERY), RID(), "The current rendering device has neither raytracing pipeline nor ray query support.");

	RaytracingPipeline *raytracing_pipeline = raytracing_pipeline_owner.get_or_null(p_raytracing_pipeline);
	ERR_FAIL_NULL_V(raytracing_pipeline, RID());
	ERR_FAIL_COND_V(p_initial_hit_group_capacity == 0, RID());

	uint32_t shader_group_size = driver->api_trait_get(RDD::API_TRAIT_SHADER_GROUP_HANDLE_SIZE);

	HitShaderBindingTable hit_sbt;
	hit_sbt.size = p_initial_hit_group_capacity * shader_group_size;
	hit_sbt.driver_id = _hit_sbt_buffer_create(hit_sbt.size);
	ERR_FAIL_COND_V(!hit_sbt.driver_id, RID());

	hit_sbt.draw_tracker = RDG::resource_tracker_create();
	hit_sbt.draw_tracker->buffer_driver_id = hit_sbt.driver_id;

	hit_sbt.raytracing_pipeline_id = p_raytracing_pipeline;
	hit_sbt.raytracing_pipeline = raytracing_pipeline->driver_id;
	hit_sbt.index_offset = raytracing_pipeline->raygen_shader_count + raytracing_pipeline->miss_shader_count;
	hit_sbt.raytracing_pipeline_hit_group_count = raytracing_pipeline->hit_group_count;
	hit_sbt.hit_group_indices.resize_initialized(p_initial_hit_group_capacity);

	RID id = hit_sbt_owner.make_rid(hit_sbt);
#ifdef DEV_ENABLED
	set_resource_name(id, "RID:" + itos(id.get_id()));
#endif
	_add_dependency(id, p_raytracing_pipeline);

	return id;
}

Error RenderingDevice::hit_sbt_set_pipeline(RID p_hit_sbt, RID p_raytracing_pipeline) {
	ERR_RENDER_THREAD_GUARD_V(ERR_UNAVAILABLE);

	HitShaderBindingTable *hit_sbt = hit_sbt_owner.get_or_null(p_hit_sbt);
	ERR_FAIL_NULL_V(hit_sbt, ERR_INVALID_PARAMETER);

	if (hit_sbt->raytracing_pipeline_id != p_raytracing_pipeline) {
		RaytracingPipeline *raytracing_pipeline = raytracing_pipeline_owner.get_or_null(p_raytracing_pipeline);
		ERR_FAIL_NULL_V(raytracing_pipeline, ERR_INVALID_PARAMETER);
		ERR_FAIL_COND_V_MSG(raytracing_pipeline->hit_group_count < hit_sbt->raytracing_pipeline_hit_group_count, ERR_INVALID_PARAMETER, "The new pipeline must be a superset of the existing pipeline.");

		_remove_dependency(p_hit_sbt, hit_sbt->raytracing_pipeline_id);

		hit_sbt->raytracing_pipeline_id = p_raytracing_pipeline;
		hit_sbt->raytracing_pipeline = raytracing_pipeline->driver_id;
		hit_sbt->index_offset = raytracing_pipeline->raygen_shader_count + raytracing_pipeline->miss_shader_count;
		hit_sbt->raytracing_pipeline_hit_group_count = raytracing_pipeline->hit_group_count;

		_add_dependency(p_hit_sbt, p_raytracing_pipeline);

		_hit_sbt_add_dirty_range(hit_sbt, 0, hit_sbt->used_hit_group_count);
	}

	return OK;
}

RD::HitShaderBindingTableRange RenderingDevice::hit_sbt_range_alloc(RID p_hit_sbt, uint32_t p_hit_group_count) {
	ERR_RENDER_THREAD_GUARD_V(ERR_UNAVAILABLE);

	HitShaderBindingTable *hit_sbt = hit_sbt_owner.get_or_null(p_hit_sbt);
	ERR_FAIL_NULL_V(hit_sbt, RD::HitShaderBindingTableRange());
	ERR_FAIL_COND_V(p_hit_group_count == 0, RD::HitShaderBindingTableRange());

	uint32_t offset;

	HitShaderBindingTable::FreeList::Element *free_list_elem = hit_sbt->free_list.lower_bound(p_hit_group_count);
	if (free_list_elem) {
		uint32_t free_count = free_list_elem->key();
		RBSet<uint32_t> &free_offsets = free_list_elem->value();
		RBSet<uint32_t>::Element *free_offset_elem = free_offsets.front();
		DEV_ASSERT(free_offset_elem);

		offset = free_offset_elem->get();

		free_offsets.erase(free_offset_elem);
		if (free_offsets.is_empty()) {
			hit_sbt->free_list.erase(free_list_elem);
		}
		hit_sbt->reverse_free_list.erase(offset);

		uint32_t remaining = free_count - p_hit_group_count;
		if (remaining > 0) {
			uint32_t new_offset = offset + p_hit_group_count;
			hit_sbt->free_list[remaining].insert(new_offset);
			hit_sbt->reverse_free_list[new_offset] = remaining;
		}
	} else {
		offset = hit_sbt->used_hit_group_count;
		hit_sbt->used_hit_group_count += p_hit_group_count;

		if (hit_sbt->hit_group_indices.size() < hit_sbt->used_hit_group_count) {
			hit_sbt->hit_group_indices.resize_initialized(MAX(hit_sbt->hit_group_indices.size() * 2, hit_sbt->used_hit_group_count));
		}
	}

	_hit_sbt_add_dirty_range(hit_sbt, offset, p_hit_group_count);

	return _encode_hit_sbt_range(offset, p_hit_group_count);
}

Error RenderingDevice::hit_sbt_range_free(RID p_hit_sbt, HitShaderBindingTableRange p_range) {
	ERR_RENDER_THREAD_GUARD_V(ERR_UNAVAILABLE);

	HitShaderBindingTable *hit_sbt = hit_sbt_owner.get_or_null(p_hit_sbt);
	ERR_FAIL_NULL_V(hit_sbt, ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(!p_range, ERR_INVALID_PARAMETER);

	uint32_t offset = _decode_hit_sbt_range_offset(p_range);
	uint32_t count = _decode_hit_sbt_range_count(p_range);

	ERR_FAIL_COND_V((offset + count) > hit_sbt->used_hit_group_count, ERR_INVALID_PARAMETER);

	HitShaderBindingTable::ReverseFreeList::Element *next_reverse_free_list_elem = hit_sbt->reverse_free_list.lower_bound(offset);
	if (next_reverse_free_list_elem) {
		HitShaderBindingTable::ReverseFreeList::Element *prev_reverse_free_list_elem = next_reverse_free_list_elem->prev();
		if (prev_reverse_free_list_elem) {
			uint32_t prev_free_offset = prev_reverse_free_list_elem->key();
			uint32_t prev_free_count = prev_reverse_free_list_elem->value();

			if ((prev_free_offset + prev_free_count) == offset) {
				offset = prev_free_offset;
				count += prev_free_count;

				HitShaderBindingTable::FreeList::Element *prev_free_list_elem = hit_sbt->free_list.find(prev_free_count);
				DEV_ASSERT(prev_free_list_elem);

				prev_free_list_elem->value().erase(prev_free_offset);
				if (prev_free_list_elem->value().is_empty()) {
					hit_sbt->free_list.erase(prev_free_list_elem);
				}

				hit_sbt->reverse_free_list.erase(prev_reverse_free_list_elem);
			}
		}

		uint32_t next_free_offset = next_reverse_free_list_elem->key();
		uint32_t next_free_count = next_reverse_free_list_elem->value();

		if ((offset + count) == next_free_offset) {
			count += next_free_count;

			HitShaderBindingTable::FreeList::Element *next_free_list_elem = hit_sbt->free_list.find(next_free_count);
			DEV_ASSERT(next_free_list_elem);

			next_free_list_elem->value().erase(next_free_offset);
			if (next_free_list_elem->value().is_empty()) {
				hit_sbt->free_list.erase(next_free_list_elem);
			}

			hit_sbt->reverse_free_list.erase(next_reverse_free_list_elem);
		}
	}

	if ((offset + count) == hit_sbt->used_hit_group_count) {
		hit_sbt->used_hit_group_count = offset;
	} else {
		hit_sbt->free_list[count].insert(offset);
		hit_sbt->reverse_free_list[offset] = count;
	}

	return OK;
}

Error RenderingDevice::hit_sbt_range_update(RID p_hit_sbt, HitShaderBindingTableRange p_range, uint32_t p_offset, Span<uint32_t> p_hit_group_indices) {
	ERR_RENDER_THREAD_GUARD_V(ERR_UNAVAILABLE);

	HitShaderBindingTable *hit_sbt = hit_sbt_owner.get_or_null(p_hit_sbt);
	ERR_FAIL_NULL_V(hit_sbt, ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(!p_range, ERR_INVALID_PARAMETER);

	uint32_t offset = _decode_hit_sbt_range_offset(p_range);
	uint32_t count = _decode_hit_sbt_range_count(p_range);

	ERR_FAIL_COND_V((offset + count) > hit_sbt->used_hit_group_count, ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V((p_offset + p_hit_group_indices.size()) > count, ERR_INVALID_PARAMETER);

#ifdef DEBUG_ENABLED
	for (uint32_t i = 0; i < p_hit_group_indices.size(); i++) {
		ERR_FAIL_COND_V(p_hit_group_indices[i] >= hit_sbt->raytracing_pipeline_hit_group_count, ERR_INVALID_PARAMETER);
	}
#endif

	memcpy(hit_sbt->hit_group_indices.ptrw() + (offset + p_offset), p_hit_group_indices.ptr(), sizeof(uint32_t) * p_hit_group_indices.size());

	_hit_sbt_add_dirty_range(hit_sbt, offset + p_offset, p_hit_group_indices.size());

	return OK;
}

/***************************/
/**** BUFFER MANAGEMENT ****/
/***************************/

RenderingDevice::Buffer *RenderingDevice::_get_buffer_from_owner(RID p_buffer) {
	Buffer *buffer = nullptr;
	if (vertex_buffer_owner.owns(p_buffer)) {
		buffer = vertex_buffer_owner.get_or_null(p_buffer);
	} else if (index_buffer_owner.owns(p_buffer)) {
		buffer = index_buffer_owner.get_or_null(p_buffer);
	} else if (uniform_buffer_owner.owns(p_buffer)) {
		buffer = uniform_buffer_owner.get_or_null(p_buffer);
	} else if (texture_buffer_owner.owns(p_buffer)) {
		DEV_ASSERT(false && "FIXME: Broken.");
		//buffer = texture_buffer_owner.get_or_null(p_buffer)->buffer;
	} else if (storage_buffer_owner.owns(p_buffer)) {
		buffer = storage_buffer_owner.get_or_null(p_buffer);
	}
	return buffer;
}

Error RenderingDevice::_buffer_initialize(Buffer *p_buffer, Span<uint8_t> p_data, uint32_t p_required_align) {
	uint32_t transfer_worker_offset;
	TransferWorker *transfer_worker = _acquire_transfer_worker(p_data.size(), p_required_align, transfer_worker_offset);
	ERR_FAIL_NULL_V(transfer_worker, ERR_BUSY);
	p_buffer->transfer_worker_index = transfer_worker->index;

	{
		MutexLock lock(transfer_worker->operations_mutex);
		p_buffer->transfer_worker_operation = ++transfer_worker->operations_counter;
	}

	// Copy to the worker's staging buffer.
	uint8_t *data_ptr = driver->buffer_map(transfer_worker->staging_buffer);
	if (data_ptr == nullptr) {
		p_buffer->transfer_worker_index = -1;
		p_buffer->transfer_worker_operation = 0;
		_release_transfer_worker(transfer_worker);
		return ERR_CANT_CREATE;
	}
	ERR_FAIL_NULL_V(data_ptr, ERR_CANT_CREATE);

	memcpy(data_ptr + transfer_worker_offset, p_data.ptr(), p_data.size());
	driver->buffer_unmap(transfer_worker->staging_buffer);

	// Copy from the staging buffer to the real buffer.
	RDD::BufferCopyRegion region;
	region.src_offset = transfer_worker_offset;
	region.dst_offset = 0;
	region.size = p_data.size();
	driver->command_copy_buffer(transfer_worker->command_buffer, transfer_worker->staging_buffer, p_buffer->driver_id, region);

	_release_transfer_worker(transfer_worker);

	return OK;
}

Error RenderingDevice::_insert_staging_block(StagingBuffers &p_staging_buffers) {
	StagingBufferBlock block;

	block.driver_id = driver->buffer_create(p_staging_buffers.block_size, p_staging_buffers.usage_bits, RDD::MEMORY_ALLOCATION_TYPE_CPU, frames_drawn);
	ERR_FAIL_COND_V(!block.driver_id, ERR_CANT_CREATE);

	block.frame_used = 0;
	block.fill_amount = 0;
	block.data_ptr = driver->buffer_map(block.driver_id);

	if (block.data_ptr == nullptr) {
		driver->buffer_free(block.driver_id);
		return ERR_CANT_CREATE;
	}

	p_staging_buffers.blocks.insert(p_staging_buffers.current, block);
	return OK;
}

Error RenderingDevice::_staging_buffer_allocate(StagingBuffers &p_staging_buffers, uint32_t p_amount, uint32_t p_required_align, uint32_t &r_alloc_offset, uint32_t &r_alloc_size, StagingRequiredAction &r_required_action, bool p_can_segment) {
	if (_recording_error() != OK) {
		return _recording_error();
	}
	// A single staging allocation request must fit in one staging block. Reject it
	// before selecting or marking a block so callers cannot write past the
	// backing allocation when a configured region is larger than the block.
	if (p_amount > p_staging_buffers.block_size) {
		return ERR_INVALID_PARAMETER;
	}
	// Determine a block to use.

	r_alloc_size = p_amount;
	r_required_action = STAGING_REQUIRED_ACTION_NONE;

	while (true) {
		r_alloc_offset = 0;

		// See if we can use current block.
		if (p_staging_buffers.blocks[p_staging_buffers.current].frame_used == frames_drawn) {
			// We used this block this frame, let's see if there is still room.

			uint32_t write_from = p_staging_buffers.blocks[p_staging_buffers.current].fill_amount;

			{
				uint32_t align_remainder = write_from % p_required_align;
				if (align_remainder != 0) {
					write_from += p_required_align - align_remainder;
				}
			}

			int32_t available_bytes = int32_t(p_staging_buffers.block_size) - int32_t(write_from);

			if ((int32_t)p_amount < available_bytes) {
				// All is good, we should be ok, all will fit.
				r_alloc_offset = write_from;
			} else if (p_can_segment && available_bytes >= (int32_t)p_required_align) {
				// Ok all won't fit but at least we can fit a chunkie.
				// All is good, update what needs to be written to.
				r_alloc_offset = write_from;
				r_alloc_size = available_bytes - (available_bytes % p_required_align);

			} else {
				// Can't fit it into this buffer.
				// Will need to try next buffer.

				p_staging_buffers.current = (p_staging_buffers.current + 1) % p_staging_buffers.blocks.size();

				// Before doing anything, though, let's check that we didn't manage to fill all blocks.
				// Possible in a single frame.
				if (p_staging_buffers.blocks[p_staging_buffers.current].frame_used == frames_drawn) {
					// Guess we did.. ok, let's see if we can insert a new block.
					if ((uint64_t)p_staging_buffers.blocks.size() * p_staging_buffers.block_size < p_staging_buffers.max_size) {
						// We can, so we are safe.
						Error err = _insert_staging_block(p_staging_buffers);
						if (err) {
							return err;
						}
						// Claim for this frame.
						p_staging_buffers.blocks.write[p_staging_buffers.current].frame_used = frames_drawn;
					} else {
						// Ok, worst case scenario, all the staging buffers belong to this frame
						// and this frame is not even done.
						// If this is the main thread, it means the user is likely loading a lot of resources at once,.
						// Otherwise, the thread should just be blocked until the next frame (currently unimplemented).
						r_required_action = STAGING_REQUIRED_ACTION_FLUSH_AND_STALL_ALL;
					}

				} else {
					// Not from current frame, so continue and try again.
					continue;
				}
			}

		} else if (p_staging_buffers.blocks[p_staging_buffers.current].frame_used <= frames_drawn - frames.size()) {
			// This is an old block, which was already processed, let's reuse.
			p_staging_buffers.blocks.write[p_staging_buffers.current].frame_used = frames_drawn;
			p_staging_buffers.blocks.write[p_staging_buffers.current].fill_amount = 0;
		} else {
			// This block may still be in use, let's not touch it unless we have to, so.. can we create a new one?
			if ((uint64_t)p_staging_buffers.blocks.size() * p_staging_buffers.block_size < p_staging_buffers.max_size) {
				// We are still allowed to create a new block, so let's do that and insert it for current pos.
				Error err = _insert_staging_block(p_staging_buffers);
				if (err) {
					return err;
				}
				// Claim for this frame.
				p_staging_buffers.blocks.write[p_staging_buffers.current].frame_used = frames_drawn;
			} else {
				// Oops, we are out of room and we can't create more.
				// Let's flush older frames.
				// The logic here is that if a game is loading a lot of data from the main thread, it will need to be stalled anyway.
				// If loading from a separate thread, we can block that thread until next frame when more room is made (not currently implemented, though).
				r_required_action = STAGING_REQUIRED_ACTION_STALL_PREVIOUS;
			}
		}

		// All was good, break.
		break;
	}

	p_staging_buffers.used = true;

	return OK;
}

Error RenderingDevice::_staging_buffer_execute_required_action(StagingBuffers &p_staging_buffers, StagingRequiredAction p_required_action) {
	switch (p_required_action) {
		case STAGING_REQUIRED_ACTION_NONE: {
			// Do nothing.
		} break;
		case STAGING_REQUIRED_ACTION_FLUSH_AND_STALL_ALL: {
			const Error wait_error = _flush_and_stall_for_all_frames();
			if (wait_error != OK) {
				return wait_error;
			}

			// Clear the whole staging buffer.
			for (int i = 0; i < p_staging_buffers.blocks.size(); i++) {
				p_staging_buffers.blocks.write[i].frame_used = 0;
				p_staging_buffers.blocks.write[i].fill_amount = 0;
			}

			// Claim for current frame.
			p_staging_buffers.blocks.write[p_staging_buffers.current].frame_used = frames_drawn;
		} break;
		case STAGING_REQUIRED_ACTION_STALL_PREVIOUS: {
			const Error wait_error = _stall_for_previous_frames();
			if (wait_error != OK) {
				return wait_error;
			}

			for (int i = 0; i < p_staging_buffers.blocks.size(); i++) {
				// Clear all blocks but the ones from this frame.
				int block_idx = (i + p_staging_buffers.current) % p_staging_buffers.blocks.size();
				if (p_staging_buffers.blocks[block_idx].frame_used == frames_drawn) {
					break; // Ok, we reached something from this frame, abort.
				}

				p_staging_buffers.blocks.write[block_idx].frame_used = 0;
				p_staging_buffers.blocks.write[block_idx].fill_amount = 0;
			}

			// Claim for current frame.
			p_staging_buffers.blocks.write[p_staging_buffers.current].frame_used = frames_drawn;
		} break;
		default: {
			DEV_ASSERT(false && "Unknown required action.");
		} break;
	}
	return OK;
}

Error RenderingDevice::buffer_copy(RID p_src_buffer, RID p_dst_buffer, uint32_t p_src_offset, uint32_t p_dst_offset, uint32_t p_size) {
	return _buffer_copy_recorded(p_src_buffer, p_dst_buffer, p_src_offset, p_dst_offset, p_size).error;
}

RenderingDevice::CopyAppendResult RenderingDevice::_buffer_copy_recorded(RID p_src_buffer, RID p_dst_buffer, uint32_t p_src_offset, uint32_t p_dst_offset, uint32_t p_size) {
	ERR_RENDER_THREAD_GUARD_V(ERR_UNAVAILABLE);
	if (_recording_error() != OK) {
		return _recording_error();
	}

	ERR_FAIL_COND_V_MSG(draw_list.active, ERR_INVALID_PARAMETER,
			"Copying buffers is forbidden during creation of a draw list.");
	ERR_FAIL_COND_V_MSG(compute_list.active, ERR_INVALID_PARAMETER,
			"Copying buffers is forbidden during creation of a compute list.");
	ERR_FAIL_COND_V_MSG(raytracing_list.active, ERR_INVALID_PARAMETER,
			"Copying buffers is forbidden during creation of a raytracing list.");

	Buffer *src_buffer = _get_buffer_from_owner(p_src_buffer);
	if (!src_buffer) {
		ERR_FAIL_V_MSG(ERR_INVALID_PARAMETER, "Source buffer argument is not a valid buffer of any type.");
	}

	if (native_immutable_resource_is_held(p_dst_buffer) || native_immutable_resource_is_retiring(p_dst_buffer)) {
		return ERR_BUSY;
	}
	Buffer *dst_buffer = _get_buffer_from_owner(p_dst_buffer);
	if (!dst_buffer) {
		ERR_FAIL_V_MSG(ERR_INVALID_PARAMETER, "Destination buffer argument is not a valid buffer of any type.");
	}

	ERR_FAIL_COND_V(((src_buffer->qualification_return || (src_buffer->native_disposal.cell.is_valid() && src_buffer->native_disposal.cell->production_request)) && !_production_resource_permitted(p_src_buffer)) || ((dst_buffer->qualification_return || (dst_buffer->native_disposal.cell.is_valid() && dst_buffer->native_disposal.cell->production_request)) && !_production_resource_permitted(p_dst_buffer)), ERR_UNAUTHORIZED);
	// Validate the copy's dimensions for both buffers.
	ERR_FAIL_COND_V_MSG(uint64_t(p_src_offset) + uint64_t(p_size) > src_buffer->size, ERR_INVALID_PARAMETER, "Size is larger than the source buffer.");
	ERR_FAIL_COND_V_MSG(uint64_t(p_dst_offset) + uint64_t(p_size) > dst_buffer->size, ERR_INVALID_PARAMETER, "Size is larger than the destination buffer.");

	CopyAppendResult result;
	for (const Buffer *buffer : { src_buffer, dst_buffer }) {
		if (buffer->transfer_worker_index >= 0) {
			result.transfers[result.transfer_count++] = { uint32_t(buffer->transfer_worker_index), buffer->transfer_worker_operation };
		}
	}
	{
		const Error dependency_error = _check_transfer_worker_buffer(src_buffer);
		if (dependency_error != OK) {
			return dependency_error;
		}
	}
	{
		const Error dependency_error = _check_transfer_worker_buffer(dst_buffer);
		if (dependency_error != OK) {
			return dependency_error;
		}
	}

	// Perform the copy.
	RDD::BufferCopyRegion region;
	region.src_offset = p_src_offset;
	region.dst_offset = p_dst_offset;
	region.size = p_size;

	if (_buffer_make_mutable(dst_buffer, p_dst_buffer)) {
		// The destination buffer must be mutable to be used as a copy destination.
		result.destination_became_mutable = true;
		draw_graph.add_synchronization();
	}

	result.copy_handle = draw_graph.add_buffer_copy(src_buffer->driver_id, src_buffer->draw_tracker, dst_buffer->driver_id, dst_buffer->draw_tracker, region);
#if defined(TESTS_ENABLED) && defined(MODULE_SKINNED_MESH_CAPTURE_TESTS_ENABLED)
	RenderCaptureTestHooks::observe_buffer_copy(p_src_buffer.get_id(), p_dst_buffer.get_id(), p_src_offset, p_dst_offset, p_size, uint64_t(get_instance_id()), src_buffer->capture_test_allocation_serial, dst_buffer->capture_test_allocation_serial);
#endif

	result.effect = NativeAppendEffect::Recorded;
	return result;
}

Error RenderingDevice::_buffer_update(Buffer *p_buffer, RID p_buffer_id, uint32_t p_offset, uint32_t p_size, const void *p_data, QualificationUploadProgress *p_progress, GDExtensionDeviceExit::BufferUploadSourceCapture *p_source) {
	if (native_immutable_resource_is_held(p_buffer_id) || native_immutable_resource_is_retiring(p_buffer_id)) {
		return ERR_BUSY;
	}
	if (_recording_error() != OK) {
		return _recording_error();
	}
	if (p_source) {
		if (p_progress || !p_source->segments || p_source->segment_stride < sizeof(GDExtensionDeviceExit::BufferUploadSourceSegment) || p_source->segment_stride % alignof(GDExtensionDeviceExit::BufferUploadSourceSegment) || !p_source->capacity || p_source->count >= p_source->capacity || p_source->consumed > p_size || p_buffer->usage.has_flag(RDD::BUFFER_USAGE_DYNAMIC_PERSISTENT_BIT)) {
			return ERR_INVALID_PARAMETER;
		}
		if (p_source->consumed == p_size) {
			return OK;
		}
	}
	if (p_progress) {
		if (!qualification_recording || p_progress->consumed > p_size || !p_progress->max_segments || p_progress->max_segments > 4096) {
			return ERR_INVALID_PARAMETER;
		}
		if (p_buffer->usage.has_flag(RDD::BUFFER_USAGE_DYNAMIC_PERSISTENT_BIT)) {
			return ERR_UNAVAILABLE;
		}
		if (p_buffer->transfer_worker_index >= 0) {
			return ERR_BUSY;
		}
		if (p_progress->consumed == p_size) {
			return OK;
		}
	}
	copy_bytes_count += p_size;

	if (p_buffer->usage.has_flag(RDD::BUFFER_USAGE_DYNAMIC_PERSISTENT_BIT)) {
		uint8_t *dst_data = driver->buffer_persistent_map_advance(p_buffer->driver_id, frames_drawn);

		memcpy(dst_data + p_offset, p_data, p_size);
		direct_copy_count++;
#if defined(TESTS_ENABLED) && defined(MODULE_SKINNED_MESH_CAPTURE_TESTS_ENABLED)
		RenderCaptureTestHooks::observe_buffer_write(p_buffer_id.get_id(), p_offset, p_size, true, uint64_t(get_instance_id()), p_buffer->driver_id.id, p_buffer->capture_test_allocation_serial);
#endif
		buffer_flush(p_buffer_id);
		return OK;
	}

	if (p_source && p_buffer->transfer_worker_index >= 0) {
		p_source->transfer_worker = uint32_t(p_buffer->transfer_worker_index);
		p_source->transfer_operation = p_buffer->transfer_worker_operation;
		p_source->prefix_possible = true;
	}
	{
		const Error dependency_error = _check_transfer_worker_buffer(p_buffer);
		if (dependency_error != OK) {
			return dependency_error;
		}
	}

	// Submitting may get chunked for various reasons, so convert this to a task.
	size_t submit_from = p_progress ? p_progress->consumed : p_source ? p_source->consumed
																	  : 0;
	size_t to_submit = p_size - submit_from;

	thread_local LocalVector<RDG::RecordedBufferCopy> command_buffer_copies_vector;
	command_buffer_copies_vector.clear();
	auto publish = [&]() -> Error {
		if (command_buffer_copies_vector.is_empty()) {
			return OK;
		}
		if ((p_progress && p_progress->segments.size() >= p_progress->max_segments) || (p_source && p_source->count >= p_source->capacity)) {
			return ERR_PARAMETER_RANGE_ERROR;
		}
		if (_buffer_make_mutable(p_buffer, p_buffer_id)) {
			if (p_source) {
				p_source->became_mutable = true;
				p_source->prefix_possible = true;
			}
			draw_graph.add_synchronization();
		}
		const auto handle = draw_graph.add_buffer_update(p_buffer->driver_id, p_buffer->draw_tracker, command_buffer_copies_vector);
		if (p_source) {
			const auto &first = command_buffer_copies_vector[0].region;
			const auto &last = command_buffer_copies_vector[command_buffer_copies_vector.size() - 1].region;
			p_source->segment(p_source->count++) = { handle, uint32_t(first.dst_offset - p_offset), uint32_t(last.dst_offset + last.size - p_offset) };
			p_source->consumed = last.dst_offset + last.size - p_offset;
			p_source->prefix_possible = true;
		}
		if (p_progress) {
			const auto &first = command_buffer_copies_vector[0].region;
			const auto &last = command_buffer_copies_vector[command_buffer_copies_vector.size() - 1].region;
			p_progress->segments.push_back({ handle, uint32_t(first.dst_offset - p_offset), uint32_t(last.dst_offset + last.size - p_offset) });
			p_progress->consumed = last.dst_offset + last.size - p_offset;
		}
		command_buffer_copies_vector.clear();
		return OK;
	};

	const uint8_t *src_data = reinterpret_cast<const uint8_t *>(p_data);
	const uint32_t required_align = 32;
	while (to_submit > 0) {
		uint32_t block_write_offset;
		uint32_t block_write_amount;
		StagingRequiredAction required_action;

		Error err = _staging_buffer_allocate(upload_staging_buffers, MIN(to_submit, upload_staging_buffers.block_size), required_align, block_write_offset, block_write_amount, required_action);
		if (err) {
#if defined(TESTS_ENABLED) && defined(MODULE_SKINNED_MESH_CAPTURE_TESTS_ENABLED)
			RenderCaptureTestHooks::observe_buffer_write(p_buffer_id.get_id(), p_offset + submit_from, to_submit, false, uint64_t(get_instance_id()), p_buffer->driver_id.id, p_buffer->capture_test_allocation_serial, submit_from > 0);
#endif
			if (p_progress || p_source) {
				const Error publication = publish();
				if (publication != OK) {
					return publication;
				}
			}
			return err;
		}
		if ((p_progress || p_source) && required_action != STAGING_REQUIRED_ACTION_NONE) {
			const Error publication = publish();
			return publication != OK ? publication : ERR_BUSY;
		}

		if (!command_buffer_copies_vector.is_empty() && required_action == STAGING_REQUIRED_ACTION_FLUSH_AND_STALL_ALL) {
			const Error publication = publish();
			if (publication != OK) {
				return publication;
			}
		}

		const Error staging_error = _staging_buffer_execute_required_action(upload_staging_buffers, required_action);
		if (staging_error != OK) {
			return staging_error;
		}

		// Copy to staging buffer.
		memcpy(upload_staging_buffers.blocks[upload_staging_buffers.current].data_ptr + block_write_offset, src_data + submit_from, block_write_amount);

		// Insert a command to copy this.
		RDD::BufferCopyRegion region;
		region.src_offset = block_write_offset;
		region.dst_offset = submit_from + p_offset;
		region.size = block_write_amount;

		RDG::RecordedBufferCopy buffer_copy;
		buffer_copy.source = upload_staging_buffers.blocks[upload_staging_buffers.current].driver_id;
		buffer_copy.region = region;
		command_buffer_copies_vector.push_back(buffer_copy);
#if defined(TESTS_ENABLED) && defined(MODULE_SKINNED_MESH_CAPTURE_TESTS_ENABLED)
		RenderCaptureTestHooks::observe_buffer_write(p_buffer_id.get_id(), submit_from + p_offset, block_write_amount, true, uint64_t(get_instance_id()), p_buffer->driver_id.id, p_buffer->capture_test_allocation_serial);
#endif

		upload_staging_buffers.blocks.write[upload_staging_buffers.current].fill_amount = block_write_offset + block_write_amount;

		to_submit -= block_write_amount;
		submit_from += block_write_amount;
	}

	const Error publication = publish();
	if (publication != OK) {
		return publication;
	}

	gpu_copy_count++;

	return OK;
}

Error RenderingDevice::buffer_update(RID p_buffer, uint32_t p_offset, uint32_t p_size, const void *p_data, bool p_skip_check) {
	if (native_immutable_resource_is_held(p_buffer) || native_immutable_resource_is_retiring(p_buffer)) {
		return ERR_BUSY;
	}
	ERR_RENDER_THREAD_GUARD_V(ERR_UNAVAILABLE);

	ERR_FAIL_COND_V_MSG(draw_list.active && !p_skip_check, ERR_INVALID_PARAMETER,
			"Updating buffers is forbidden during creation of a draw list.");
	ERR_FAIL_COND_V_MSG(compute_list.active && !p_skip_check, ERR_INVALID_PARAMETER,
			"Updating buffers is forbidden during creation of a compute list.");
	ERR_FAIL_COND_V_MSG(raytracing_list.active && !p_skip_check, ERR_INVALID_PARAMETER,
			"Updating buffers is forbidden during creation of a raytracing list.");

	Buffer *buffer = _get_buffer_from_owner(p_buffer);
	ERR_FAIL_NULL_V_MSG(buffer, ERR_INVALID_PARAMETER, "Buffer argument is not a valid buffer of any type.");
	ERR_FAIL_COND_V_MSG(uint64_t(p_offset) + p_size > buffer->size, ERR_INVALID_PARAMETER, "Attempted to write buffer (" + itos((uint64_t(p_offset) + p_size) - buffer->size) + " bytes) past the end.");

	ERR_FAIL_COND_V(buffer->qualification_return, ERR_UNAUTHORIZED);
#ifdef TESTS_ENABLED
	if (test_buffer_update_fault_count > 0 && test_buffer_update_fault_buffer == p_buffer) {
#if defined(MODULE_PARTICLES_RUNTIME_TESTS_ENABLED)
		if (test_buffer_update_fault_skip > 0) {
			test_buffer_update_fault_skip--;
		} else
#endif
		{
			test_buffer_update_fault_count--;
			test_buffer_update_fault_hits++;
			return ERR_CANT_CREATE;
		}
	}
#endif
	return _buffer_update(buffer, p_buffer, p_offset, p_size, p_data);
}

Error RenderingDevice::driver_callback_add(RDD::DriverCallback p_callback, void *p_userdata, VectorView<CallbackResource> p_resources) {
	ERR_RENDER_THREAD_GUARD_V(ERR_UNAVAILABLE);
	if (_recording_error() != OK) {
		return _recording_error();
	}

	ERR_FAIL_COND_V_MSG(draw_list.active, ERR_INVALID_PARAMETER,
			"Driver callback is forbidden during creation of a draw list.");
	ERR_FAIL_COND_V_MSG(compute_list.active, ERR_INVALID_PARAMETER,
			"Driver callback is forbidden during creation of a compute list.");
	ERR_FAIL_COND_V_MSG(raytracing_list.active, ERR_INVALID_PARAMETER,
			"Driver callback is forbidden during creation of a raytracing list.");

	for (uint32_t i = 0; i < p_resources.size(); ++i) {
		const auto usage = p_resources[i].usage;
		const bool read_only = usage == CALLBACK_RESOURCE_USAGE_COPY_FROM || usage == CALLBACK_RESOURCE_USAGE_RESOLVE_FROM || usage == CALLBACK_RESOURCE_USAGE_UNIFORM_BUFFER_READ || usage == CALLBACK_RESOURCE_USAGE_INDIRECT_BUFFER_READ || usage == CALLBACK_RESOURCE_USAGE_TEXTURE_BUFFER_READ || usage == CALLBACK_RESOURCE_USAGE_STORAGE_BUFFER_READ || usage == CALLBACK_RESOURCE_USAGE_VERTEX_BUFFER_READ || usage == CALLBACK_RESOURCE_USAGE_INDEX_BUFFER_READ || usage == CALLBACK_RESOURCE_USAGE_TEXTURE_SAMPLE || usage == CALLBACK_RESOURCE_USAGE_STORAGE_IMAGE_READ || usage == CALLBACK_RESOURCE_USAGE_ACCELERATION_STRUCTURE_READ;
		ERR_FAIL_COND_V(!read_only && (native_immutable_resource_is_held(p_resources[i].rid) || native_immutable_resource_is_retiring(p_resources[i].rid)), ERR_BUSY);

		if (p_resources[i].type == CALLBACK_RESOURCE_TYPE_BUFFER) {
			auto *buffer = storage_buffer_owner.owns(p_resources[i].rid) ? storage_buffer_owner.get_or_null(p_resources[i].rid) : nullptr;
			ERR_FAIL_COND_V(buffer && buffer->qualification_return, ERR_UNAUTHORIZED);
		} else if (p_resources[i].type == CALLBACK_RESOURCE_TYPE_TEXTURE) {
			auto *texture = texture_owner.get_or_null(p_resources[i].rid);
			ERR_FAIL_COND_V(texture && texture->qualification_return, ERR_UNAUTHORIZED);
		}
	}

	thread_local LocalVector<RDG::ResourceTracker *> trackers;
	thread_local LocalVector<RDG::ResourceUsage> usages;

	uint32_t resource_count = p_resources.size();
	trackers.resize(resource_count);
	usages.resize(resource_count);

	if (resource_count > 0) {
		for (uint32_t i = 0; i < p_resources.size(); i++) {
			const CallbackResource &cr = p_resources[i];
			switch (cr.type) {
				case CALLBACK_RESOURCE_TYPE_BUFFER: {
					Buffer *buffer = _get_buffer_from_owner(cr.rid);
					if (!buffer) {
						ERR_FAIL_V_MSG(ERR_INVALID_PARAMETER, vformat("Argument %d is not a valid buffer of any type.", i));
					}
					if (_buffer_make_mutable(buffer, cr.rid)) {
						draw_graph.add_synchronization();
					}
					trackers[i] = buffer->draw_tracker;
					usages[i] = (RDG::ResourceUsage)cr.usage;
				} break;
				case CALLBACK_RESOURCE_TYPE_TEXTURE: {
					Texture *texture = texture_owner.get_or_null(cr.rid);
					if (!texture) {
						ERR_FAIL_V_MSG(ERR_INVALID_PARAMETER, vformat("Argument %d is not a valid texture.", i));
					}
					if (_texture_make_mutable(texture, cr.rid)) {
						draw_graph.add_synchronization();
					}
					trackers[i] = texture->draw_tracker;
					usages[i] = (RDG::ResourceUsage)cr.usage;
				} break;
				default: {
					CRASH_NOW_MSG("Invalid callback resource type.");
				} break;
			}
		}
	}

	draw_graph.add_driver_callback(p_callback, p_userdata, trackers, usages);
#if defined(TESTS_ENABLED) && defined(MODULE_SKINNED_MESH_CAPTURE_TESTS_ENABLED)
	if (RenderCaptureTestHooks::write_census_active()) {
		for (uint32_t i = 0; i < p_resources.size(); i++) {
			const auto &resource = p_resources[i];
			const auto usage = usages[i];
			if (resource.type != CALLBACK_RESOURCE_TYPE_BUFFER) {
				continue;
			}
			const Buffer *buffer = _get_buffer_from_owner(resource.rid);
			if (!buffer) {
				RenderCaptureTestHooks::write_census_poison();
				continue;
			}
			_capture_test_record_buffer_usage(buffer->driver_id.id, usage, RenderCaptureTestHooks::CALLBACK);
		}
	}
#endif

	return OK;
}

String RenderingDevice::get_perf_report() const {
	String perf_report_text;
	perf_report_text += " gpu:" + String::num_int64(gpu_copy_count);
	perf_report_text += " direct:" + String::num_int64(direct_copy_count);
	perf_report_text += " bytes:" + String::num_int64(copy_bytes_count);

	perf_report_text += " lazily alloc:" + String::num_int64(driver->get_lazily_memory_used());
	return perf_report_text;
}

void RenderingDevice::update_perf_report() {
	prev_gpu_copy_count = gpu_copy_count;
	prev_copy_bytes_count = copy_bytes_count;
	gpu_copy_count = 0;
	direct_copy_count = 0;
	copy_bytes_count = 0;
}

Error RenderingDevice::buffer_clear(RID p_buffer, uint32_t p_offset, uint32_t p_size) {
	if (native_immutable_resource_is_held(p_buffer) || native_immutable_resource_is_retiring(p_buffer)) {
		return ERR_BUSY;
	}
	ERR_RENDER_THREAD_GUARD_V(ERR_UNAVAILABLE);
	if (_recording_error() != OK) {
		return _recording_error();
	}

	ERR_FAIL_COND_V_MSG((p_offset % 4) != 0, ERR_INVALID_PARAMETER,
			"Offset must be a multiple of four.");
	ERR_FAIL_COND_V_MSG((p_size % 4) != 0, ERR_INVALID_PARAMETER,
			"Size must be a multiple of four.");
	ERR_FAIL_COND_V_MSG(draw_list.active, ERR_INVALID_PARAMETER,
			"Updating buffers in is forbidden during creation of a draw list.");
	ERR_FAIL_COND_V_MSG(compute_list.active, ERR_INVALID_PARAMETER,
			"Updating buffers is forbidden during creation of a compute list.");
	ERR_FAIL_COND_V_MSG(raytracing_list.active, ERR_INVALID_PARAMETER,
			"Updating buffers is forbidden during creation of a raytracing list.");

	Buffer *buffer = _get_buffer_from_owner(p_buffer);
	if (!buffer) {
		ERR_FAIL_V_MSG(ERR_INVALID_PARAMETER, "Buffer argument is not a valid buffer of any type.");
	}

	ERR_FAIL_COND_V_MSG(uint64_t(p_offset) + p_size > buffer->size, ERR_INVALID_PARAMETER,
			"Attempted to write buffer (" + itos((uint64_t(p_offset) + p_size) - buffer->size) + " bytes) past the end.");

	ERR_FAIL_COND_V(buffer->qualification_return, ERR_UNAUTHORIZED);
	if (p_size == 0) {
		return OK;
	}

	{
		const Error dependency_error = _check_transfer_worker_buffer(buffer);
		if (dependency_error != OK) {
			return dependency_error;
		}
	}

	if (_buffer_make_mutable(buffer, p_buffer)) {
		// The destination buffer must be mutable to be used as a clear destination.
		draw_graph.add_synchronization();
	}

	draw_graph.add_buffer_clear(buffer->driver_id, buffer->draw_tracker, p_offset, p_size);
#if defined(TESTS_ENABLED) && defined(MODULE_SKINNED_MESH_CAPTURE_TESTS_ENABLED)
	RenderCaptureTestHooks::observe_buffer_clear(p_buffer.get_id(), p_offset, p_size, uint64_t(get_instance_id()), buffer->driver_id.id, buffer->capture_test_allocation_serial);
#endif

	return OK;
}

Vector<uint8_t> RenderingDevice::buffer_get_data(RID p_buffer, uint32_t p_offset, uint32_t p_size) {
	ERR_FAIL_COND_V_MSG(_recording_error() != OK, Vector<uint8_t>(), "A frame fence wait failed; readback recording is suspended.");
	ERR_RENDER_THREAD_GUARD_V(Vector<uint8_t>());

	Buffer *buffer = _get_buffer_from_owner(p_buffer);
	if (!buffer) {
		ERR_FAIL_V_MSG(Vector<uint8_t>(), "Buffer is either invalid or this type of buffer can't be retrieved.");
	}

	ERR_FAIL_COND_V(buffer->qualification_return, Vector<uint8_t>());
	// Size of buffer to retrieve.
	if (!p_size) {
		p_size = buffer->size;
	}
	ERR_FAIL_COND_V_MSG(uint64_t(p_offset) + p_size > buffer->size, Vector<uint8_t>(),
			"Size is larger than the buffer.");

	{
		const Error dependency_error = _check_transfer_worker_buffer(buffer);
		if (dependency_error != OK) {
			return Vector<uint8_t>();
		}
	}

	RDD::BufferID tmp_buffer = driver->buffer_create(buffer->size, RDD::BUFFER_USAGE_TRANSFER_TO_BIT, RDD::MEMORY_ALLOCATION_TYPE_CPU, frames_drawn);
	ERR_FAIL_COND_V(!tmp_buffer, Vector<uint8_t>());

	RDD::BufferCopyRegion region;
	region.src_offset = p_offset;
	region.size = p_size;

	draw_graph.add_buffer_get_data(buffer->driver_id, buffer->draw_tracker, tmp_buffer, region);
#if defined(TESTS_ENABLED) && defined(MODULE_SKINNED_MESH_CAPTURE_TESTS_ENABLED)
	RenderCaptureTestHooks::observe_readback_queued(p_buffer.get_id(), p_size);
#endif

	// Flush everything so memory can be safely mapped.
	const Error wait_error = _flush_and_stall_for_all_frames();
	if (wait_error != OK) {
		Buffer retained_buffer;
		retained_buffer.driver_id = tmp_buffer;
		frames[frame].buffers_to_dispose_of.push_back(retained_buffer);
		ERR_FAIL_V_MSG(Vector<uint8_t>(), "Buffer readback fence wait failed.");
	}

	uint8_t *buffer_mem = driver->buffer_map(tmp_buffer);
	if (buffer_mem == nullptr) {
		// Mapping can fail after the temporary transfer buffer has been issued
		// and waited. Do not strand that allocation when the caller receives the
		// fail-closed empty result.
		driver->buffer_free(tmp_buffer);
		ERR_FAIL_V(Vector<uint8_t>());
	}

	Vector<uint8_t> buffer_data;
	{
		buffer_data.resize(p_size);
		uint8_t *w = buffer_data.ptrw();
		memcpy(w, buffer_mem, p_size);
	}

	driver->buffer_unmap(tmp_buffer);

#if defined(TESTS_ENABLED) && defined(MODULE_SKINNED_MESH_CAPTURE_TESTS_ENABLED)
	RenderCaptureTestHooks::observe_readback_completed(p_buffer.get_id(), p_size);
#endif

	driver->buffer_free(tmp_buffer);

	return buffer_data;
}

Error RenderingDevice::buffer_get_data_async(RID buffer, const Callable &callback, uint32_t offset, uint32_t size) {
	return _buffer_get_data_async(buffer, callback, offset, size);
}

Error RenderingDevice::_buffer_get_data_async(RID p_buffer, const Callable &p_callback, uint32_t p_offset, uint32_t p_size, const Ref<BufferGetDataResult> &p_owned, GDExtensionDeviceExit::BufferReadbackSourceCapture *p_capture) {
	ERR_RENDER_THREAD_GUARD_V(ERR_UNAVAILABLE);
	if (_recording_error() != OK) {
		return _recording_error();
	}
	Buffer *buffer = _get_buffer_from_owner(p_buffer);
	ERR_FAIL_NULL_V(buffer, ERR_INVALID_PARAMETER);
	const bool core = p_owned.is_valid() || p_capture;
	ERR_FAIL_COND_V((buffer->qualification_return || (buffer->native_disposal.cell.is_valid() && buffer->native_disposal.cell->production_request)) && !qualification_recording && !_production_resource_permitted(p_buffer), ERR_UNAUTHORIZED);
	if (!p_size) {
		p_size = buffer->size;
	}
	ERR_FAIL_COND_V(p_offset > buffer->size || p_size > buffer->size - p_offset, ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(p_capture ? (p_owned.is_valid() || p_callback.is_valid() || !p_capture->state || !p_capture->state->request_serial || !p_capture->bytes || p_capture->byte_capacity != p_size || !p_capture->segments || p_capture->segment_stride < sizeof(GDExtensionDeviceExit::BufferUploadSourceSegment) || p_capture->segment_stride % alignof(GDExtensionDeviceExit::BufferUploadSourceSegment) || p_capture->count >= p_capture->capacity || p_capture->state->issued > p_size || p_capture->state->delivered || p_capture->state->error != OK) : core ? (!p_owned->qualification_owned || !qualification_recording || p_callback.is_valid() || p_owned->bytes.size() != p_size || p_owned->issued_size > p_size || p_owned->delivered || p_owned->error != OK)
																																																																																																																																								   : !p_callback.is_valid(),
			ERR_INVALID_PARAMETER);
	if (p_capture) {
		auto &state = *p_capture->state;
		if (!buffer->backing_serial || (state.backing_serial && (state.backing_serial != buffer->backing_serial || state.offset != p_offset || state.size != p_size))) {
			return ERR_INVALID_DATA;
		}
		state.backing_serial = buffer->backing_serial;
		state.offset = p_offset;
		state.size = p_size;
		state.owned_bytes = p_capture->byte_capacity;
	} else if (core) {
		if (!p_owned->request_serial || !buffer->backing_serial) {
			return ERR_INVALID_DATA;
		}
		if (!p_owned->source_backing_serial) {
			if (p_owned->issued_size || !p_owned->copies.is_empty()) {
				return ERR_INVALID_DATA;
			}
			p_owned->source_backing_serial = buffer->backing_serial;
			p_owned->source_offset = p_offset;
			p_owned->source_size = p_size;
		} else if (p_owned->source_backing_serial != buffer->backing_serial || p_owned->source_offset != p_offset || p_owned->source_size != p_size) {
			return ERR_INVALID_DATA;
		}
	}
	// The public legacy route retains its worker submission behavior. Core ticket
	// admission must join a real worker before using it; never silently wait here.
	if (core && !p_capture && buffer->transfer_worker_index >= 0) {
		return ERR_BUSY;
	}
	if (!core || p_capture) {
		if (p_capture && buffer->transfer_worker_index >= 0) {
			p_capture->transfer_worker = uint32_t(buffer->transfer_worker_index);
			p_capture->transfer_operation = buffer->transfer_worker_operation;
		}
		{
			const Error dependency_error = _check_transfer_worker_buffer(buffer);
			if (dependency_error != OK) {
				return dependency_error;
			}
		}
	}
	BufferGetDataRequest request;
	request.buffer = p_buffer;
	request.callback = p_callback;
	request.frame_local_index = frames[frame].download_buffer_copy_regions.size();
	request.capture = p_capture;
	if (core) {
		request.result = p_owned;
	} else {
		request.result.instantiate();
		const Error error = request.result->bytes.resize(p_size);
		if (error != OK) {
			return error;
		}
	}
	auto issued_size = [&]() -> uint32_t { return p_capture ? uint32_t(p_capture->state->issued) : request.result->issued_size; };
	auto error_cell = [&]() -> Error & { return p_capture ? p_capture->state->error : request.result->error; };
	request.result_offset = issued_size();
	uint32_t submit_from = issued_size(), to_submit = p_size - submit_from;
	auto publish = [&](bool final) {
		if (!request.frame_local_count) {
			return;
		}
		if (_buffer_make_mutable(buffer, p_buffer)) {
			if (p_capture) {
				p_capture->became_mutable = true;
			}
			draw_graph.add_synchronization();
		}
		for (uint32_t i = 0; i < request.frame_local_count; ++i) {
			const uint32_t index = request.frame_local_index + i;
			const auto destination = frames[frame].download_buffer_staging_buffers[index];
			const auto region = frames[frame].download_buffer_copy_regions[index];
			const auto handle = draw_graph.add_buffer_get_data(buffer->driver_id, buffer->draw_tracker, destination, region);
			if (p_capture) {
				p_capture->segment(p_capture->count++) = { handle, uint32_t(region.src_offset - p_offset), uint32_t(region.src_offset + region.size - p_offset) };
			} else {
				request.result->copies.push_back({ handle, buffer->driver_id, destination, region, false, RDG::RecordMappingState::Pending, UINT32_MAX, RDD::CommandBufferID(), false, OK, {} });
			}
		}
		request.final_segment = final;
		frames[frame].download_buffer_get_data_requests.push_back(request);
		if (p_capture) {
			p_capture->state->issued += request.size;
		} else {
			request.result->issued_size += request.size;
		}
	};
	while (to_submit) {
		// Every staged region will produce one actual graph-copy handle. Reserve
		// its arena slot before staging consumes bytes or mutability changes.
		if (p_capture && request.frame_local_count >= p_capture->capacity - p_capture->count) {
			publish(false);
			return ERR_BUSY;
		}
		uint32_t offset = 0, amount = 0;
		StagingRequiredAction action;
		Error error = _staging_buffer_allocate(download_staging_buffers, MIN(to_submit, download_staging_buffers.block_size), 32, offset, amount, action);
		if (error != OK) {
			if (core) {
				publish(false);
			}
			error_cell() = error;
			return error;
		}
		if (core && action != STAGING_REQUIRED_ACTION_NONE) {
			// Both exhausted-staging cases preserve this same result/copy set and return
			// before the legacy blocking action. The next call resumes only its suffix.
			publish(false);
			return ERR_BUSY;
		}
		const bool flush = request.frame_local_count > 0 && action == STAGING_REQUIRED_ACTION_FLUSH_AND_STALL_ALL;
		if (flush) {
			publish(false);
		}
		error = _staging_buffer_execute_required_action(download_staging_buffers, action);
		if (error != OK) {
			error_cell() = error;
			return error;
		}
		if (error_cell() != OK) {
			return error_cell();
		}
		buffer = _get_buffer_from_owner(p_buffer);
		if (!buffer) {
			error_cell() = ERR_DOES_NOT_EXIST;
			return ERR_DOES_NOT_EXIST;
		}
		if (flush) {
			request.result_offset = issued_size();
			request.size = 0;
			request.frame_local_count = 0;
			request.frame_local_index = frames[frame].download_buffer_copy_regions.size();
		}
		const RDD::BufferCopyRegion region{ submit_from + p_offset, offset, amount };
		frames[frame].download_buffer_staging_buffers.push_back(download_staging_buffers.blocks[download_staging_buffers.current].driver_id);
		frames[frame].download_buffer_copy_regions.push_back(region);
		++request.frame_local_count;
		request.size += amount;
		download_staging_buffers.blocks.write[download_staging_buffers.current].fill_amount = offset + amount;
		submit_from += amount;
		to_submit -= amount;
	}
	publish(true);
#if defined(TESTS_ENABLED) && defined(MODULE_SKINNED_MESH_CAPTURE_TESTS_ENABLED)
	if (request.frame_local_count) {
		RenderCaptureTestHooks::observe_readback_queued(p_buffer.get_id(), p_size);
	}
#endif
	return OK;
}

uint64_t RenderingDevice::buffer_get_device_address(RID p_buffer) {
	ERR_RENDER_THREAD_GUARD_V(0);

	MutexLock immutable_lock(_thread_safe_);
	ERR_FAIL_COND_V(native_immutable_resource_is_held(p_buffer) || native_immutable_resource_is_retiring(p_buffer), 0);
	Buffer *buffer = _get_buffer_from_owner(p_buffer);
	ERR_FAIL_NULL_V_MSG(buffer, 0, "Buffer argument is not a valid buffer of any type.");
	ERR_FAIL_COND_V(buffer->qualification_return, 0);
	ERR_FAIL_COND_V_MSG(!buffer->usage.has_flag(RDD::BUFFER_USAGE_DEVICE_ADDRESS_BIT), 0, "Buffer was not created with device address flag.");

#if defined(TESTS_ENABLED) && (defined(MODULE_SKINNED_MESH_CAPTURE_TESTS_ENABLED) || defined(MODULE_PARTICLES_RUNTIME_TESTS_ENABLED))
	if (buffer->capture_test_allocation_serial && !capture_test_allocation_registry.mark_access_escaped(buffer->driver_id.id, buffer->capture_test_allocation_serial)) {
		capture_test_coverage_failed.store(true);
	}
#endif
	const uint64_t address = driver->buffer_get_device_address(buffer->driver_id);
	if (address) {
		buffer->native_external_access = true;
	}
	return address;
}

uint8_t *RenderingDevice::buffer_persistent_map_advance(RID p_buffer) {
	ERR_FAIL_COND_V(native_immutable_resource_is_held(p_buffer) || native_immutable_resource_is_retiring(p_buffer), nullptr);
	ERR_RENDER_THREAD_GUARD_V(0);

	Buffer *buffer = _get_buffer_from_owner(p_buffer);
	ERR_FAIL_NULL_V_MSG(buffer, nullptr, "Buffer argument is not a valid buffer of any type.");
	ERR_FAIL_COND_V(buffer->qualification_return, nullptr);
	direct_copy_count++;
#if defined(TESTS_ENABLED) && (defined(MODULE_SKINNED_MESH_CAPTURE_TESTS_ENABLED) || defined(MODULE_PARTICLES_RUNTIME_TESTS_ENABLED))
	if (buffer->capture_test_allocation_serial && !capture_test_allocation_registry.mark_access_escaped(buffer->driver_id.id, buffer->capture_test_allocation_serial)) {
		capture_test_coverage_failed.store(true);
	}
#endif
	uint8_t *mapping = driver->buffer_persistent_map_advance(buffer->driver_id, frames_drawn);
	if (mapping) {
		buffer->native_external_access = true;
	}
	return mapping;
}

void RenderingDevice::buffer_flush(RID p_buffer) {
	ERR_RENDER_THREAD_GUARD();

	Buffer *buffer = _get_buffer_from_owner(p_buffer);
	ERR_FAIL_NULL_MSG(buffer, "Buffer argument is not a valid buffer of any type.");
	ERR_FAIL_COND(buffer->qualification_return);
	driver->buffer_flush(buffer->driver_id);
}

uint64_t RenderingDevice::_reserve_backing_serial() {
	uint64_t serial = next_backing_serial.load(std::memory_order_relaxed);
	while (serial) {
		const uint64_t next = serial == UINT64_MAX ? 0 : serial + 1;
		if (next_backing_serial.compare_exchange_weak(serial, next, std::memory_order_relaxed)) {
			return serial;
		}
	}
	return 0;
}

RID RenderingDevice::storage_buffer_create(uint32_t p_size_bytes, Span<uint8_t> p_data, BitField<StorageBufferUsage> p_usage, BitField<BufferCreationBits> p_creation_bits) {
	return _storage_buffer_create_internal(p_size_bytes, p_data, p_usage, p_creation_bits, nullptr);
}

RID RenderingDevice::_storage_buffer_create_internal(uint32_t p_size_bytes, Span<uint8_t> p_data, BitField<StorageBufferUsage> p_usage, BitField<BufferCreationBits> p_creation_bits, const NativeDisposalWitness *p_native_disposal) {
	ERR_FAIL_COND_V_MSG(_recording_error() != OK, RID(), "Resource recording is suspended during frame fence recovery or readback callbacks.");
	ERR_FAIL_COND_V(p_data.size() && (uint32_t)p_data.size() != p_size_bytes, RID());

	const uint64_t backing_serial = _reserve_backing_serial();
	ERR_FAIL_COND_V_MSG(backing_serial == 0, RID(), "Storage backing serial space exhausted.");
	Buffer buffer;
	buffer.backing_serial = backing_serial;
	buffer.size = p_size_bytes;
	buffer.usage = (RDD::BUFFER_USAGE_TRANSFER_FROM_BIT | RDD::BUFFER_USAGE_TRANSFER_TO_BIT | RDD::BUFFER_USAGE_STORAGE_BIT);
	if (p_creation_bits.has_flag(BUFFER_CREATION_DYNAMIC_PERSISTENT_BIT)) {
		buffer.usage.set_flag(RDD::BUFFER_USAGE_DYNAMIC_PERSISTENT_BIT);

		// This is a precaution: Persistent buffers are meant for frequent CPU -> GPU transfers.
		// Writing to this buffer from GPU might cause sync issues if both CPU & GPU try to write at the
		// same time. It's probably fine (since CPU always advances the pointer before writing) but let's
		// stick to the known/intended use cases and scream if we deviate from it.
		buffer.usage.clear_flag(RDD::BUFFER_USAGE_TRANSFER_TO_BIT);
	}
	if (p_usage.has_flag(STORAGE_BUFFER_USAGE_DISPATCH_INDIRECT)) {
		buffer.usage.set_flag(RDD::BUFFER_USAGE_INDIRECT_BIT);
	}
	if (p_creation_bits.has_flag(BUFFER_CREATION_DEVICE_ADDRESS_BIT)) {
#ifdef DEBUG_ENABLED
		ERR_FAIL_COND_V_MSG(!has_feature(SUPPORTS_BUFFER_DEVICE_ADDRESS), RID(),
				"The GPU doesn't support buffer address flag.");
#endif

		buffer.usage.set_flag(RDD::BUFFER_USAGE_DEVICE_ADDRESS_BIT);
	}
	if (p_creation_bits.has_flag(BUFFER_CREATION_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT)) {
#ifdef DEBUG_ENABLED
		ERR_FAIL_COND_V_MSG(!has_feature(SUPPORTS_RAYTRACING_PIPELINE) && !has_feature(SUPPORTS_RAY_QUERY), RID(),
				"The GPU doesn't support acceleration structure build input flag.");
#endif
		buffer.usage.set_flag(RDD::BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT);
	}

	RID reserved_storage;
	if (p_native_disposal) {
		// The Core factory has no upload prefix. Reserve the actual RID, tracker
		// and observation cell before the first native allocation.
		ERR_FAIL_COND_V(!p_data.is_empty(), RID());
		reserved_storage = storage_buffer_owner.allocate_rid();
		ERR_FAIL_COND_V(reserved_storage.is_null(), RID());
		buffer.draw_tracker = RDG::resource_tracker_create();
		buffer.native_disposal = *p_native_disposal;
		buffer.native_disposal_rid = reserved_storage.get_id();
		p_native_disposal->cell->rid = reserved_storage.get_id();
		p_native_disposal->cell->serial = backing_serial;
	}
	buffer.driver_id = driver->buffer_create(buffer.size, buffer.usage, RDD::MEMORY_ALLOCATION_TYPE_GPU, frames_drawn);
	if (!buffer.driver_id) {
		if (reserved_storage.is_valid()) {
			RDG::resource_tracker_free(buffer.draw_tracker);
			storage_buffer_owner.initialize_rid(reserved_storage, Buffer());
			storage_buffer_owner.free(reserved_storage);
		}
		ERR_FAIL_V(RID());
	}

	// Storage buffers are assumed to be mutable.
	if (!buffer.draw_tracker) {
		buffer.draw_tracker = RDG::resource_tracker_create();
	}
	buffer.draw_tracker->buffer_driver_id = buffer.driver_id;
	if (p_native_disposal) {
		p_native_disposal->cell->backing = buffer.driver_id.id;
	}

	if (p_data.size()) {
		Error initialize_error = _buffer_initialize(&buffer, p_data);
		if (initialize_error != OK) {
			RDG::resource_tracker_free(buffer.draw_tracker);
			driver->buffer_free(buffer.driver_id);
			_production_disposal_returned(buffer.production_disposal_uses);
			return RID();
		}
	}

	_THREAD_SAFE_LOCK_
	buffer_memory += buffer.size;
	_THREAD_SAFE_UNLOCK_

#if defined(TESTS_ENABLED) && (defined(MODULE_SKINNED_MESH_CAPTURE_TESTS_ENABLED) || defined(MODULE_PARTICLES_RUNTIME_TESTS_ENABLED))
	RID id = reserved_storage.is_valid() ? reserved_storage : storage_buffer_owner.allocate_rid();
	buffer.capture_test_allocation_serial = capture_test_allocation_registry.allocate(buffer.driver_id.id, id.get_id());
	if (buffer.capture_test_allocation_serial == 0) {
		capture_test_coverage_failed.store(true);
	}
	storage_buffer_owner.initialize_rid(id, buffer);
#else
	RID id = reserved_storage;
	if (id.is_valid()) {
		storage_buffer_owner.initialize_rid(id, buffer);
	} else {
		id = storage_buffer_owner.make_rid(buffer);
	}
#endif
#if defined(TESTS_ENABLED) && defined(MODULE_SKINNED_MESH_CAPTURE_TESTS_ENABLED)
	RenderCaptureTestHooks::observe_buffer_allocation(id.get_id(), uint64_t(get_instance_id()), buffer.driver_id.id, buffer.capture_test_allocation_serial);
#endif
#ifdef DEV_ENABLED
	set_resource_name(id, "RID:" + itos(id.get_id()));
#endif
	return id;
}

RID RenderingDevice::texture_buffer_create(uint32_t p_size_elements, DataFormat p_format, Span<uint8_t> p_data) {
	ERR_FAIL_COND_V_MSG(_recording_error() != OK, RID(), "Resource recording is suspended during frame fence recovery or readback callbacks.");
	uint32_t element_size = get_format_vertex_size(p_format);
	ERR_FAIL_COND_V_MSG(element_size == 0, RID(), "Format requested is not supported for texture buffers");
	uint64_t size_bytes = uint64_t(element_size) * p_size_elements;

	ERR_FAIL_COND_V(p_data.size() && (uint32_t)p_data.size() != size_bytes, RID());

	Buffer texture_buffer;
	texture_buffer.size = size_bytes;
	BitField<RDD::BufferUsageBits> usage = (RDD::BUFFER_USAGE_TRANSFER_FROM_BIT | RDD::BUFFER_USAGE_TRANSFER_TO_BIT | RDD::BUFFER_USAGE_TEXEL_BIT);
	texture_buffer.driver_id = driver->buffer_create(size_bytes, usage, RDD::MEMORY_ALLOCATION_TYPE_GPU, frames_drawn);
	ERR_FAIL_COND_V(!texture_buffer.driver_id, RID());

	// Texture buffers are assumed to be immutable unless they don't have initial data.
	if (p_data.is_empty()) {
		texture_buffer.draw_tracker = RDG::resource_tracker_create();
		texture_buffer.draw_tracker->buffer_driver_id = texture_buffer.driver_id;
	}

	bool ok = driver->buffer_set_texel_format(texture_buffer.driver_id, p_format);
	if (!ok) {
		RDG::resource_tracker_free(texture_buffer.draw_tracker);
		driver->buffer_free(texture_buffer.driver_id);
		ERR_FAIL_V(RID());
	}

	if (p_data.size()) {
		Error initialize_error = _buffer_initialize(&texture_buffer, p_data);
		if (initialize_error != OK) {
			RDG::resource_tracker_free(texture_buffer.draw_tracker);
			driver->buffer_free(texture_buffer.driver_id);
			return RID();
		}
	}

	_THREAD_SAFE_LOCK_
	buffer_memory += size_bytes;
	_THREAD_SAFE_UNLOCK_

	RID id = texture_buffer_owner.make_rid(texture_buffer);
#ifdef DEV_ENABLED
	set_resource_name(id, "RID:" + itos(id.get_id()));
#endif
	return id;
}

/*****************/
/**** TEXTURE ****/
/*****************/

RID RenderingDevice::texture_create(const TextureFormat &p_format, const TextureView &p_view, const Vector<Vector<uint8_t>> &p_data) {
	return _texture_create_internal(p_format, p_view, p_data, nullptr, RID());
}

RID RenderingDevice::_qualification_texture_create(const TextureFormat &p_format, const TextureView &p_view, GDExtensionDeviceExit::QualificationPhysicalReturn *p_cell) {
	ERR_FAIL_COND_V(!p_cell || !p_cell->bridge || !p_cell->child || !p_cell->serial || p_cell->rid || p_cell->use_closed || p_cell->enqueued || p_cell->free_event, RID());
	// VRS can implicitly initialize image data. This entry reserves an empty
	// texture only; checked upload is a separate original ticket operation.
	ERR_FAIL_COND_V(p_format.usage_bits & TEXTURE_USAGE_VRS_ATTACHMENT_BIT, RID());
	ERR_FAIL_COND_V(!_qualification_reserve_free_event(p_cell), RID());
	RID reserved = texture_owner.allocate_rid();
	if (reserved.is_null()) {
		_qualification_release_free_event(p_cell);
		return RID();
	}
	RID created = _texture_create_internal(p_format, p_view, {}, p_cell, reserved);
	if (created.is_null()) {
		if (texture_owner.owns(reserved)) {
			texture_owner.initialize_rid(reserved, Texture());
			texture_owner.free(reserved);
		}
		_qualification_release_free_event(p_cell);
	}
	return created;
}

RID RenderingDevice::_texture_create_internal(const TextureFormat &p_format, const TextureView &p_view, const Vector<Vector<uint8_t>> &p_data, GDExtensionDeviceExit::QualificationPhysicalReturn *p_cell, RID p_reserved, const NativeDisposalWitness *p_native_disposal) {
	ERR_FAIL_COND_V(p_native_disposal && (p_cell || p_reserved.is_null() || !p_data.is_empty()), RID());
	ERR_FAIL_COND_V_MSG(_recording_error() != OK, RID(), "Resource recording is suspended during frame fence recovery or readback callbacks.");
	// Check the stored enum representation before copying or reading it.
	static_assert(sizeof(p_format.format) == sizeof(uint32_t));
	uint32_t format_value = 0;
	memcpy(&format_value, &p_format.format, sizeof(format_value));
	ERR_FAIL_UNSIGNED_INDEX_V(format_value, DATA_FORMAT_MAX, RID());
	// Some adjustments will happen.
	TextureFormat format = p_format;

	if (format.shareable_formats.size()) {
		ERR_FAIL_COND_V_MSG(!format.shareable_formats.has(format.format), RID(),
				"If supplied a list of shareable formats, the current format must be present in the list");
		ERR_FAIL_COND_V_MSG(p_view.format_override != DATA_FORMAT_MAX && !format.shareable_formats.has(p_view.format_override), RID(),
				"If supplied a list of shareable formats, the current view format override must be present in the list");
	}

	ERR_FAIL_INDEX_V(format.texture_type, RDD::TEXTURE_TYPE_MAX, RID());

	ERR_FAIL_COND_V_MSG(format.width < 1, RID(), "Width must be equal or greater than 1 for all textures");

	if (format.texture_type != TEXTURE_TYPE_1D && format.texture_type != TEXTURE_TYPE_1D_ARRAY) {
		ERR_FAIL_COND_V_MSG(format.height < 1, RID(), "Height must be equal or greater than 1 for 2D and 3D textures");
	}

	if (format.texture_type == TEXTURE_TYPE_3D) {
		ERR_FAIL_COND_V_MSG(format.depth < 1, RID(), "Depth must be equal or greater than 1 for 3D textures");
	}

	ERR_FAIL_COND_V(format.mipmaps < 1, RID());

	if (format.texture_type == TEXTURE_TYPE_1D_ARRAY || format.texture_type == TEXTURE_TYPE_2D_ARRAY || format.texture_type == TEXTURE_TYPE_CUBE_ARRAY || format.texture_type == TEXTURE_TYPE_CUBE) {
		ERR_FAIL_COND_V_MSG(format.array_layers < 1, RID(),
				"Number of layers must be equal or greater than 1 for arrays and cubemaps.");
		ERR_FAIL_COND_V_MSG((format.texture_type == TEXTURE_TYPE_CUBE_ARRAY || format.texture_type == TEXTURE_TYPE_CUBE) && (format.array_layers % 6) != 0, RID(),
				"Cubemap and cubemap array textures must provide a layer number that is multiple of 6");
		ERR_FAIL_COND_V_MSG(((format.texture_type == TEXTURE_TYPE_CUBE_ARRAY || format.texture_type == TEXTURE_TYPE_CUBE)) && (format.width != format.height), RID(),
				"Cubemap and cubemap array textures must have equal width and height.");
		ERR_FAIL_COND_V_MSG(format.array_layers > driver->limit_get(LIMIT_MAX_TEXTURE_ARRAY_LAYERS), RID(), "Number of layers exceeds device maximum.");
	} else {
		format.array_layers = 1;
	}

	ERR_FAIL_INDEX_V(format.samples, TEXTURE_SAMPLES_MAX, RID());

	ERR_FAIL_COND_V_MSG(format.usage_bits == 0, RID(), "No usage bits specified (at least one is needed)");

	format.height = format.texture_type != TEXTURE_TYPE_1D && format.texture_type != TEXTURE_TYPE_1D_ARRAY ? format.height : 1;
	format.depth = format.texture_type == TEXTURE_TYPE_3D ? format.depth : 1;

	uint64_t size_max = 0;
	switch (format.texture_type) {
		case TEXTURE_TYPE_1D:
		case TEXTURE_TYPE_1D_ARRAY:
			size_max = driver->limit_get(LIMIT_MAX_TEXTURE_SIZE_1D);
			break;
		case TEXTURE_TYPE_2D:
		case TEXTURE_TYPE_2D_ARRAY:
			size_max = driver->limit_get(LIMIT_MAX_TEXTURE_SIZE_2D);
			break;
		case TEXTURE_TYPE_CUBE:
		case TEXTURE_TYPE_CUBE_ARRAY:
			size_max = driver->limit_get(LIMIT_MAX_TEXTURE_SIZE_CUBE);
			break;
		case TEXTURE_TYPE_3D:
			size_max = driver->limit_get(LIMIT_MAX_TEXTURE_SIZE_3D);
			break;
		case TEXTURE_TYPE_MAX:
			break;
	}
	ERR_FAIL_COND_V_MSG(format.width > size_max || format.height > size_max || format.depth > size_max, RID(), "Texture dimensions exceed device maximum.");

	uint32_t required_mipmaps = get_image_required_mipmaps(format.width, format.height, format.depth);

	ERR_FAIL_COND_V_MSG(required_mipmaps < format.mipmaps, RID(),
			"Too many mipmaps requested for texture format and dimensions (" + itos(format.mipmaps) + "), maximum allowed: (" + itos(required_mipmaps) + ").");

	Vector<Vector<uint8_t>> data = p_data;
	bool immediate_flush = false;

	// If this is a VRS texture, we make sure that it is created with valid initial data. This prevents a crash on Qualcomm Snapdragon XR2 Gen 1
	// (used in Quest 2, Quest Pro, Pico 4, HTC Vive XR Elite and others) where the driver will read the texture before we've had time to finish updating it.
	if (data.is_empty() && (p_format.usage_bits & TEXTURE_USAGE_VRS_ATTACHMENT_BIT)) {
		immediate_flush = true;
		for (uint32_t i = 0; i < format.array_layers; i++) {
			uint32_t required_size = get_image_format_required_size(format.format, format.width, format.height, format.depth, format.mipmaps);
			Vector<uint8_t> layer;
			layer.resize(required_size);
			layer.fill(255);
			data.push_back(layer);
		}
	}

	uint32_t forced_usage_bits = _texture_vrs_method_to_usage_bits();
	if (data.size()) {
		ERR_FAIL_COND_V_MSG(data.size() != (int)format.array_layers, RID(),
				"Default supplied data for image format is of invalid length (" + itos(data.size()) + "), should be (" + itos(format.array_layers) + ").");

		for (uint32_t i = 0; i < format.array_layers; i++) {
			uint32_t required_size = get_image_format_required_size(format.format, format.width, format.height, format.depth, format.mipmaps);
			ERR_FAIL_COND_V_MSG((uint32_t)data[i].size() != required_size, RID(),
					"Data for slice index " + itos(i) + " (mapped to layer " + itos(i) + ") differs in size (supplied: " + itos(data[i].size()) + ") than what is required by the format (" + itos(required_size) + ").");
		}

		ERR_FAIL_COND_V_MSG(format.usage_bits & TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, RID(),
				"Textures created as depth attachments can't be initialized with data directly. Use RenderingDevice::texture_update() instead.");

		if (!(format.usage_bits & TEXTURE_USAGE_CAN_UPDATE_BIT)) {
			forced_usage_bits |= TEXTURE_USAGE_CAN_UPDATE_BIT;
		}
	}

	{
		// Validate that this image is supported for the intended use.
		bool cpu_readable = (format.usage_bits & RDD::TEXTURE_USAGE_CPU_READ_BIT);
		BitField<RDD::TextureUsageBits> supported_usage = driver->texture_get_usages_supported_by_format(format.format, cpu_readable);

		String format_text = "'" + String(FORMAT_NAMES[format.format]) + "'";

		if ((format.usage_bits & TEXTURE_USAGE_SAMPLING_BIT) && !supported_usage.has_flag(TEXTURE_USAGE_SAMPLING_BIT)) {
			ERR_FAIL_V_MSG(RID(), "Format " + format_text + " does not support usage as sampling texture.");
		}
		if ((format.usage_bits & TEXTURE_USAGE_COLOR_ATTACHMENT_BIT) && !supported_usage.has_flag(TEXTURE_USAGE_COLOR_ATTACHMENT_BIT)) {
			ERR_FAIL_V_MSG(RID(), "Format " + format_text + " does not support usage as color attachment.");
		}
		if ((format.usage_bits & TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) && !supported_usage.has_flag(TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)) {
			ERR_FAIL_V_MSG(RID(), "Format " + format_text + " does not support usage as depth-stencil attachment.");
		}
		if ((format.usage_bits & TEXTURE_USAGE_STORAGE_BIT) && !supported_usage.has_flag(TEXTURE_USAGE_STORAGE_BIT)) {
			ERR_FAIL_V_MSG(RID(), "Format " + format_text + " does not support usage as storage image.");
		}
		if ((format.usage_bits & TEXTURE_USAGE_STORAGE_ATOMIC_BIT) && !supported_usage.has_flag(TEXTURE_USAGE_STORAGE_ATOMIC_BIT)) {
			ERR_FAIL_V_MSG(RID(), "Format " + format_text + " does not support usage as atomic storage image.");
		}
		if ((format.usage_bits & TEXTURE_USAGE_VRS_ATTACHMENT_BIT) && !supported_usage.has_flag(TEXTURE_USAGE_VRS_ATTACHMENT_BIT)) {
			ERR_FAIL_V_MSG(RID(), "Format " + format_text + " does not support usage as variable shading rate attachment.");
		}
	}

	// Transfer and validate view info.

	RDD::TextureView tv;
	if (p_view.format_override == DATA_FORMAT_MAX) {
		tv.format = format.format;
	} else {
		ERR_FAIL_INDEX_V(p_view.format_override, DATA_FORMAT_MAX, RID());
		tv.format = p_view.format_override;
	}
	ERR_FAIL_INDEX_V(p_view.swizzle_r, TEXTURE_SWIZZLE_MAX, RID());
	ERR_FAIL_INDEX_V(p_view.swizzle_g, TEXTURE_SWIZZLE_MAX, RID());
	ERR_FAIL_INDEX_V(p_view.swizzle_b, TEXTURE_SWIZZLE_MAX, RID());
	ERR_FAIL_INDEX_V(p_view.swizzle_a, TEXTURE_SWIZZLE_MAX, RID());
	tv.swizzle_r = p_view.swizzle_r;
	tv.swizzle_g = p_view.swizzle_g;
	tv.swizzle_b = p_view.swizzle_b;
	tv.swizzle_a = p_view.swizzle_a;

	// Create.

	Texture texture;
	format.usage_bits |= forced_usage_bits;
	texture.driver_id = driver->texture_create(format, tv);
	ERR_FAIL_COND_V(!texture.driver_id, RID());
	texture.type = format.texture_type;
	texture.format = format.format;
	texture.width = format.width;
	texture.height = format.height;
	texture.depth = format.depth;
	texture.layers = format.array_layers;
	texture.mipmaps = format.mipmaps;
	texture.base_mipmap = 0;
	texture.base_layer = 0;
	texture.is_resolve_buffer = format.is_resolve_buffer;
	texture.is_discardable = format.is_discardable;
	texture.is_subsampled = format.is_subsampled;
	texture.usage_flags = format.usage_bits & ~forced_usage_bits;
	texture.samples = format.samples;
	texture.allowed_shared_formats = format.shareable_formats;
	texture.has_initial_data = !data.is_empty();

	if (driver->api_trait_get(RDD::API_TRAIT_TEXTURE_OUTPUTS_REQUIRE_CLEARS)) {
		// Check if a clear for this texture must be performed the first time it's used if the driver requires explicit clears after initialization.
		texture.pending_clear = !texture.has_initial_data && (format.usage_bits & (TEXTURE_USAGE_STORAGE_BIT | TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT));
	}

	if ((format.usage_bits & (TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | TEXTURE_USAGE_DEPTH_RESOLVE_ATTACHMENT_BIT))) {
		texture.read_aspect_flags.set_flag(RDD::TEXTURE_ASPECT_DEPTH_BIT);
		texture.barrier_aspect_flags.set_flag(RDD::TEXTURE_ASPECT_DEPTH_BIT);
		if (format_has_stencil(format.format)) {
			texture.barrier_aspect_flags.set_flag(RDD::TEXTURE_ASPECT_STENCIL_BIT);
		}
	} else {
		texture.read_aspect_flags.set_flag(RDD::TEXTURE_ASPECT_COLOR_BIT);
		texture.barrier_aspect_flags.set_flag(RDD::TEXTURE_ASPECT_COLOR_BIT);
	}

	texture.bound = false;

	// Textures are only assumed to be immutable if they have initial data and none of the other bits that indicate write usage are enabled.
	bool texture_mutable_by_default = texture.usage_flags & (TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | TEXTURE_USAGE_DEPTH_RESOLVE_ATTACHMENT_BIT | TEXTURE_USAGE_STORAGE_BIT | TEXTURE_USAGE_STORAGE_ATOMIC_BIT);
	if (data.is_empty() || texture_mutable_by_default) {
		_texture_make_mutable(&texture, RID());
	}

	texture_memory += driver->texture_get_allocation_size(texture.driver_id);

	RID id = p_reserved;
	if (id.is_valid()) {
		texture.qualification_return = p_cell;
		if (p_cell) {
			p_cell->rid = id.get_id();
		}
		if (p_native_disposal) {
			texture.native_disposal = *p_native_disposal;
			texture.native_disposal_rid = id.get_id();
			texture.native_disposal_serial = p_native_disposal->cell->serial;
			p_native_disposal->cell->rid = id.get_id();
			p_native_disposal->cell->backing = texture.driver_id.id;
		}
		texture_owner.initialize_rid(id, texture);
	} else {
		id = texture_owner.make_rid(texture);
	}
#ifdef DEV_ENABLED
	set_resource_name(id, "RID:" + itos(id.get_id()));
#endif

	if (data.size()) {
		const bool use_general_in_copy_queues = driver->api_trait_get(RDD::API_TRAIT_USE_GENERAL_IN_COPY_QUEUES);
		const RDD::TextureLayout dst_layout = use_general_in_copy_queues ? RDD::TEXTURE_LAYOUT_GENERAL : RDD::TEXTURE_LAYOUT_COPY_DST_OPTIMAL;
		auto dispose_failed_texture = [&]() {
			Texture *failed_texture = texture_owner.get_or_null(id);
			if (!failed_texture) {
				return;
			}
			// An earlier layer may have issued a transfer. Keep the driver object
			// alive until the normal frame retirement path has drained that worker.
			{
				const Error dependency_error = _check_transfer_worker_texture(failed_texture);
				if (dependency_error != OK) {
					if (p_cell) {
						failed_texture->qualification_return = nullptr;
						p_cell->rid = 0;
					}
					_qualification_defer_free(id);
					return;
				}
			}
			if (p_cell) {
				// Creation never returned this qualification cell to its caller. Do
				// not publish a physical return for a failed create.
				failed_texture->qualification_return = nullptr;
				p_cell->rid = 0;
			}
			_production_disposal_enqueued(failed_texture->production_disposal_uses);
			frames[frame].textures_to_dispose_of.push_back(*failed_texture);
			texture_owner.free(id);
		};
		for (uint32_t i = 0; i < format.array_layers; i++) {
			const Error initialize_error = _texture_initialize(id, i, data[i], dst_layout, immediate_flush);
			if (initialize_error != OK) {
				// ERR_BUSY can mean an issued transfer is still unresolved. Keep
				// that owner for the existing shutdown/fence recovery path; freeing
				// it here would race the unresolved worker.
				if (initialize_error != ERR_BUSY) {
					dispose_failed_texture();
				}
				return RID();
			}
		}

		if (texture.draw_tracker != nullptr) {
			texture.draw_tracker->usage = use_general_in_copy_queues ? RDG::RESOURCE_USAGE_GENERAL : RDG::RESOURCE_USAGE_COPY_TO;
		}
	}

	return id;
}

RID RenderingDevice::texture_create_shared(const TextureView &p_view, RID p_with_texture) {
	ERR_FAIL_COND_V_MSG(_recording_error() != OK, RID(), "Resource recording is suspended during frame fence recovery or readback callbacks.");
	ERR_FAIL_COND_V(native_immutable_resource_is_retiring(p_with_texture), RID());
	Texture *src_texture = texture_owner.get_or_null(p_with_texture);
	ERR_FAIL_NULL_V(src_texture, RID());
	ERR_FAIL_COND_V(src_texture->qualification_return, RID());

	if (src_texture->owner.is_valid()) { // Ahh this is a share. The RenderingDeviceDriver needs the actual owner.
		p_with_texture = src_texture->owner;
		src_texture = texture_owner.get_or_null(src_texture->owner);
		ERR_FAIL_NULL_V(src_texture, RID()); // This is a bug.
		ERR_FAIL_COND_V(src_texture->qualification_return, RID());
	}

	// Create view.

	Texture texture = *src_texture;
	// A view is another native object; it cannot copy its parent's free receipt.
	texture.native_disposal = NativeDisposalWitness();
	texture.native_disposal_rid = texture.native_disposal_serial = 0;
	texture.production_disposal_uses = nullptr;
	texture.native_immutable_holds = 0;
	texture.native_immutable_retire_requested = false;
	texture.slice_trackers = nullptr;
	texture.shared_fallback = nullptr;

	RDD::TextureView tv;
	bool create_shared = true;
	bool raw_reintepretation = false;
	if (p_view.format_override == DATA_FORMAT_MAX || p_view.format_override == texture.format) {
		tv.format = texture.format;
	} else {
		ERR_FAIL_INDEX_V(p_view.format_override, DATA_FORMAT_MAX, RID());

		ERR_FAIL_COND_V_MSG(!texture.allowed_shared_formats.has(p_view.format_override), RID(),
				"Format override is not in the list of allowed shareable formats for original texture.");
		tv.format = p_view.format_override;
		create_shared = driver->texture_can_make_shared_with_format(texture.driver_id, p_view.format_override, raw_reintepretation);
	}
	tv.swizzle_r = p_view.swizzle_r;
	tv.swizzle_g = p_view.swizzle_g;
	tv.swizzle_b = p_view.swizzle_b;
	tv.swizzle_a = p_view.swizzle_a;

	if (create_shared) {
		texture.driver_id = driver->texture_create_shared(texture.driver_id, tv);
	} else {
		// The regular view will use the same format as the main texture.
		RDD::TextureView regular_view = tv;
		regular_view.format = src_texture->format;
		texture.driver_id = driver->texture_create_shared(texture.driver_id, regular_view);
		ERR_FAIL_COND_V(!texture.driver_id, RID());

		// Create the independent texture for the alias.
		RDD::TextureFormat alias_format = texture.texture_format();
		alias_format.format = tv.format;
		alias_format.usage_bits = TEXTURE_USAGE_SAMPLING_BIT | TEXTURE_USAGE_CAN_COPY_TO_BIT;

		const RDD::TextureID fallback_texture = driver->texture_create(alias_format, tv);
		if (!fallback_texture) {
			driver->texture_free(texture.driver_id);
			return RID();
		}
		const bool source_fallback_was_missing = src_texture->shared_fallback == nullptr;
		_texture_check_shared_fallback(src_texture);
		_texture_check_shared_fallback(&texture);
		texture.shared_fallback->texture = fallback_texture;
		texture.shared_fallback->raw_reinterpretation = raw_reintepretation;
		texture_memory += driver->texture_get_allocation_size(texture.shared_fallback->texture);

		RDG::ResourceTracker *tracker = RDG::resource_tracker_create();
		tracker->texture_driver_id = texture.shared_fallback->texture;
		tracker->texture_size = Size2i(texture.width, texture.height);
		tracker->texture_subresources = texture.barrier_range();
		tracker->texture_usage = alias_format.usage_bits;
		tracker->is_discardable = texture.is_discardable;
		tracker->reference_count = 1;
		texture.shared_fallback->texture_tracker = tracker;
		texture.shared_fallback->revision = 0;

		if (raw_reintepretation && src_texture->shared_fallback->buffer.id == 0) {
			// For shared textures of the same size, we create the buffer on the main texture if it doesn't have it already.
			if (!_texture_create_reinterpret_buffer(src_texture)) {
				_texture_free_shared_fallback(&texture);
				driver->texture_free(texture.driver_id);
				if (source_fallback_was_missing) {
					_texture_free_shared_fallback(src_texture);
				}
				return RID();
			}
		}
	}

	ERR_FAIL_COND_V(!texture.driver_id, RID());

	if (texture.draw_tracker != nullptr) {
		texture.draw_tracker->reference_count++;
	}

	texture.owner = p_with_texture;
	RID id = texture_owner.make_rid(texture);
#ifdef DEV_ENABLED
	set_resource_name(id, "RID:" + itos(id.get_id()));
#endif
	_add_dependency(id, p_with_texture);

	return id;
}

RID RenderingDevice::texture_create_from_extension(TextureType p_type, DataFormat p_format, TextureSamples p_samples, BitField<RenderingDevice::TextureUsageBits> p_usage, uint64_t p_image, uint64_t p_width, uint64_t p_height, uint64_t p_depth, uint64_t p_layers, uint64_t p_mipmaps) {
	ERR_FAIL_COND_V_MSG(_recording_error() != OK, RID(), "Resource recording is suspended during frame fence recovery or readback callbacks.");
	// This method creates a texture object using a VkImage created by an extension, module or other external source (OpenXR uses this).

	Texture texture;
	texture.native_external_access = true;
	texture.type = p_type;
	texture.format = p_format;
	texture.samples = p_samples;
	texture.width = p_width;
	texture.height = p_height;
	texture.depth = p_depth;
	texture.layers = p_layers;
	texture.mipmaps = p_mipmaps;
	texture.usage_flags = p_usage;
	texture.base_mipmap = 0;
	texture.base_layer = 0;
	texture.allowed_shared_formats.push_back(RD::DATA_FORMAT_R8G8B8A8_UNORM);
	texture.allowed_shared_formats.push_back(RD::DATA_FORMAT_R8G8B8A8_SRGB);

	if (p_usage.has_flag(TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) || p_usage.has_flag(TEXTURE_USAGE_DEPTH_RESOLVE_ATTACHMENT_BIT)) {
		texture.read_aspect_flags.set_flag(RDD::TEXTURE_ASPECT_DEPTH_BIT);
		texture.barrier_aspect_flags.set_flag(RDD::TEXTURE_ASPECT_DEPTH_BIT);
		/*if (format_has_stencil(p_format.format)) {
			texture.barrier_aspect_flags.set_flag(RDD::TEXTURE_ASPECT_STENCIL_BIT);
		}*/
	} else {
		texture.read_aspect_flags.set_flag(RDD::TEXTURE_ASPECT_COLOR_BIT);
		texture.barrier_aspect_flags.set_flag(RDD::TEXTURE_ASPECT_COLOR_BIT);
	}

	texture.driver_id = driver->texture_create_from_extension(p_image, p_type, p_format, p_layers, (texture.usage_flags & (TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | TEXTURE_USAGE_DEPTH_RESOLVE_ATTACHMENT_BIT)), p_mipmaps);
	ERR_FAIL_COND_V(!texture.driver_id, RID());

	_texture_make_mutable(&texture, RID());

	RID id = texture_owner.make_rid(texture);
#ifdef DEV_ENABLED
	set_resource_name(id, "RID:" + itos(id.get_id()));
#endif

	return id;
}

RID RenderingDevice::texture_create_shared_from_slice(const TextureView &p_view, RID p_with_texture, uint32_t p_layer, uint32_t p_mipmap, uint32_t p_mipmaps, TextureSliceType p_slice_type, uint32_t p_layers) {
	ERR_FAIL_COND_V_MSG(_recording_error() != OK, RID(), "Resource recording is suspended during frame fence recovery or readback callbacks.");
	ERR_FAIL_COND_V(native_immutable_resource_is_retiring(p_with_texture), RID());
	Texture *src_texture = texture_owner.get_or_null(p_with_texture);
	ERR_FAIL_NULL_V(src_texture, RID());
	ERR_FAIL_COND_V(src_texture->qualification_return, RID());

	if (src_texture->owner.is_valid()) { // // Ahh this is a share. The RenderingDeviceDriver needs the actual owner.
		p_with_texture = src_texture->owner;
		src_texture = texture_owner.get_or_null(src_texture->owner);
		ERR_FAIL_NULL_V(src_texture, RID()); // This is a bug.
		ERR_FAIL_COND_V(src_texture->qualification_return, RID());
	}

	ERR_FAIL_COND_V_MSG(p_slice_type == TEXTURE_SLICE_CUBEMAP && (src_texture->type != TEXTURE_TYPE_CUBE && src_texture->type != TEXTURE_TYPE_CUBE_ARRAY), RID(),
			"Can only create a cubemap slice from a cubemap or cubemap array mipmap");

	ERR_FAIL_COND_V_MSG(p_slice_type == TEXTURE_SLICE_3D && src_texture->type != TEXTURE_TYPE_3D, RID(),
			"Can only create a 3D slice from a 3D texture");

	ERR_FAIL_COND_V_MSG(p_slice_type == TEXTURE_SLICE_2D_ARRAY && (src_texture->type != TEXTURE_TYPE_2D_ARRAY), RID(),
			"Can only create an array slice from a 2D array mipmap");

	// Create view.

	ERR_FAIL_UNSIGNED_INDEX_V(p_mipmap, src_texture->mipmaps, RID());
	ERR_FAIL_COND_V(p_mipmap + p_mipmaps > src_texture->mipmaps, RID());
	ERR_FAIL_UNSIGNED_INDEX_V(p_layer, src_texture->layers, RID());

	int slice_layers = 1;
	if (p_layers != 0) {
		ERR_FAIL_COND_V_MSG(p_layers > 1 && p_slice_type != TEXTURE_SLICE_2D_ARRAY, RID(), "layer slicing only supported for 2D arrays");
		ERR_FAIL_COND_V_MSG(p_layer + p_layers > src_texture->layers, RID(), "layer slice is out of bounds");
		slice_layers = p_layers;
	} else if (p_slice_type == TEXTURE_SLICE_2D_ARRAY) {
		ERR_FAIL_COND_V_MSG(p_layer != 0, RID(), "layer must be 0 when obtaining a 2D array mipmap slice");
		slice_layers = src_texture->layers;
	} else if (p_slice_type == TEXTURE_SLICE_CUBEMAP) {
		slice_layers = 6;
	}

	Texture texture = *src_texture;
	// A view is another native object; it cannot copy its parent's free receipt.
	texture.native_disposal = NativeDisposalWitness();
	texture.native_disposal_rid = texture.native_disposal_serial = 0;
	texture.production_disposal_uses = nullptr;
	texture.native_immutable_holds = 0;
	texture.native_immutable_retire_requested = false;
	texture.slice_trackers = nullptr;
	texture.shared_fallback = nullptr;

	get_image_format_required_size(texture.format, texture.width, texture.height, texture.depth, p_mipmap + 1, &texture.width, &texture.height);
	texture.mipmaps = p_mipmaps;
	texture.layers = slice_layers;
	texture.base_mipmap = p_mipmap;
	texture.base_layer = p_layer;

	if (p_slice_type == TEXTURE_SLICE_2D) {
		texture.type = TEXTURE_TYPE_2D;
	} else if (p_slice_type == TEXTURE_SLICE_3D) {
		texture.type = TEXTURE_TYPE_3D;
	}

	RDD::TextureView tv;
	bool create_shared = true;
	bool raw_reintepretation = false;
	if (p_view.format_override == DATA_FORMAT_MAX || p_view.format_override == texture.format) {
		tv.format = texture.format;
	} else {
		ERR_FAIL_INDEX_V(p_view.format_override, DATA_FORMAT_MAX, RID());

		ERR_FAIL_COND_V_MSG(!texture.allowed_shared_formats.has(p_view.format_override), RID(),
				"Format override is not in the list of allowed shareable formats for original texture.");
		tv.format = p_view.format_override;
		create_shared = driver->texture_can_make_shared_with_format(texture.driver_id, p_view.format_override, raw_reintepretation);
	}

	tv.swizzle_r = p_view.swizzle_r;
	tv.swizzle_g = p_view.swizzle_g;
	tv.swizzle_b = p_view.swizzle_b;
	tv.swizzle_a = p_view.swizzle_a;

	if (p_slice_type == TEXTURE_SLICE_CUBEMAP) {
		ERR_FAIL_COND_V_MSG(p_layer >= src_texture->layers, RID(),
				"Specified layer is invalid for cubemap");
		ERR_FAIL_COND_V_MSG((p_layer % 6) != 0, RID(),
				"Specified layer must be a multiple of 6.");
	}

	if (create_shared) {
		texture.driver_id = driver->texture_create_shared_from_slice(src_texture->driver_id, tv, p_slice_type, p_layer, slice_layers, p_mipmap, p_mipmaps);
	} else {
		// The regular view will use the same format as the main texture.
		RDD::TextureView regular_view = tv;
		regular_view.format = src_texture->format;
		texture.driver_id = driver->texture_create_shared_from_slice(src_texture->driver_id, regular_view, p_slice_type, p_layer, slice_layers, p_mipmap, p_mipmaps);
		ERR_FAIL_COND_V(!texture.driver_id, RID());

		// Create the independent texture for the slice.
		RDD::TextureSubresourceRange slice_range = texture.barrier_range();
		slice_range.base_mipmap = 0;
		slice_range.base_layer = 0;

		RDD::TextureFormat slice_format = texture.texture_format();
		slice_format.width = MAX(texture.width >> p_mipmap, 1U);
		slice_format.height = MAX(texture.height >> p_mipmap, 1U);
		slice_format.depth = MAX(texture.depth >> p_mipmap, 1U);
		slice_format.format = tv.format;
		slice_format.usage_bits = TEXTURE_USAGE_SAMPLING_BIT | TEXTURE_USAGE_CAN_COPY_TO_BIT;

		const RDD::TextureID fallback_texture = driver->texture_create(slice_format, tv);
		if (!fallback_texture) {
			driver->texture_free(texture.driver_id);
			return RID();
		}
		_texture_check_shared_fallback(src_texture);
		_texture_check_shared_fallback(&texture);
		texture.shared_fallback->texture = fallback_texture;
		texture.shared_fallback->raw_reinterpretation = raw_reintepretation;
		texture_memory += driver->texture_get_allocation_size(texture.shared_fallback->texture);

		RDG::ResourceTracker *tracker = RDG::resource_tracker_create();
		tracker->texture_driver_id = texture.shared_fallback->texture;
		tracker->texture_size = Size2i(texture.width, texture.height);
		tracker->texture_subresources = slice_range;
		tracker->texture_usage = slice_format.usage_bits;
		tracker->is_discardable = slice_format.is_discardable;
		tracker->reference_count = 1;
		texture.shared_fallback->texture_tracker = tracker;
		texture.shared_fallback->revision = 0;

		if (raw_reintepretation && src_texture->shared_fallback->buffer.id == 0) {
			// For shared texture slices, we create the buffer on the slice if the source texture has no reinterpretation buffer.
			if (!_texture_create_reinterpret_buffer(&texture)) {
				_texture_free_shared_fallback(&texture);
				driver->texture_free(texture.driver_id);
				return RID();
			}
		}
	}

	ERR_FAIL_COND_V(!texture.driver_id, RID());

	const Rect2i slice_rect(p_mipmap, p_layer, p_mipmaps, slice_layers);
	texture.owner = p_with_texture;
	texture.slice_type = p_slice_type;
	texture.slice_rect = slice_rect;

	// If parent is mutable, make slice mutable by default.
	if (src_texture->draw_tracker != nullptr) {
		texture.draw_tracker = nullptr;
		_texture_make_mutable(&texture, RID());
	}

	RID id = texture_owner.make_rid(texture);
#ifdef DEV_ENABLED
	set_resource_name(id, "RID:" + itos(id.get_id()));
#endif
	_add_dependency(id, p_with_texture);

	return id;
}

static _ALWAYS_INLINE_ void _copy_region(uint8_t const *__restrict p_src, uint8_t *__restrict p_dst, uint32_t p_src_x, uint32_t p_src_y, uint32_t p_src_w, uint32_t p_src_h, uint32_t p_src_full_w, uint32_t p_dst_pitch, uint32_t p_unit_size) {
	uint32_t src_offset = (p_src_y * p_src_full_w + p_src_x) * p_unit_size;
	uint32_t dst_offset = 0;
	for (uint32_t y = p_src_h; y > 0; y--) {
		uint8_t const *__restrict src = p_src + src_offset;
		uint8_t *__restrict dst = p_dst + dst_offset;
		for (uint32_t x = p_src_w * p_unit_size; x > 0; x--) {
			*dst = *src;
			src++;
			dst++;
		}
		src_offset += p_src_full_w * p_unit_size;
		dst_offset += p_dst_pitch;
	}
}

static _ALWAYS_INLINE_ void _copy_region_block_or_regular(const uint8_t *p_read_ptr, uint8_t *p_write_ptr, uint32_t p_x, uint32_t p_y, uint32_t p_width, uint32_t p_region_w, uint32_t p_region_h, uint32_t p_block_w, uint32_t p_block_h, uint32_t p_dst_pitch, uint32_t p_pixel_size, uint32_t p_block_size) {
	if (p_block_w != 1 || p_block_h != 1) {
		// Block format.
		uint32_t xb = p_x / p_block_w;
		uint32_t yb = p_y / p_block_h;
		uint32_t wb = p_width / p_block_w;
		uint32_t region_wb = p_region_w / p_block_w;
		uint32_t region_hb = p_region_h / p_block_h;
		_copy_region(p_read_ptr, p_write_ptr, xb, yb, region_wb, region_hb, wb, p_dst_pitch, p_block_size);
	} else {
		// Regular format.
		_copy_region(p_read_ptr, p_write_ptr, p_x, p_y, p_region_w, p_region_h, p_width, p_dst_pitch, p_pixel_size);
	}
}

uint32_t RenderingDevice::_texture_layer_count(Texture *p_texture) const {
	switch (p_texture->type) {
		case TEXTURE_TYPE_CUBE:
		case TEXTURE_TYPE_CUBE_ARRAY:
			return p_texture->layers * 6;
		default:
			return p_texture->layers;
	}
}

uint32_t greatest_common_denominator(uint32_t a, uint32_t b) {
	// Euclidean algorithm.
	uint32_t t;
	while (b != 0) {
		t = b;
		b = a % b;
		a = t;
	}

	return a;
}

uint32_t least_common_multiple(uint32_t a, uint32_t b) {
	if (a == 0 || b == 0) {
		return 0;
	}

	return (a / greatest_common_denominator(a, b)) * b;
}

uint32_t RenderingDevice::_texture_alignment(Texture *p_texture) const {
	uint32_t alignment = get_compressed_image_format_block_byte_size(p_texture->format);
	if (alignment == 1) {
		alignment = get_image_format_pixel_size(p_texture->format);
	}

	return least_common_multiple(alignment, driver->api_trait_get(RDD::API_TRAIT_TEXTURE_TRANSFER_ALIGNMENT));
}

uint32_t RenderingDevice::_normalize_staging_region_size(int64_t p_size) {
	if (p_size <= 0 || uint64_t(p_size) > UINT32_MAX) {
		return 1;
	}
	const uint32_t normalized = Math::nearest_power_of_2_templated(uint32_t(p_size));
	return normalized == 0 ? 1 : normalized;
}

Error RenderingDevice::_texture_initialize(RID p_texture, uint32_t p_layer, const Vector<uint8_t> &p_data, RDD::TextureLayout p_dst_layout, bool p_immediate_flush) {
	if (native_immutable_resource_is_held(p_texture) || native_immutable_resource_is_retiring(p_texture)) {
		return ERR_BUSY;
	}
	Texture *texture = texture_owner.get_or_null(p_texture);
	ERR_FAIL_NULL_V(texture, ERR_INVALID_PARAMETER);

	if (texture->owner != RID()) {
		p_texture = texture->owner;
		texture = texture_owner.get_or_null(texture->owner);
		ERR_FAIL_NULL_V(texture, ERR_BUG); // This is a bug.
	}

	uint32_t layer_count = _texture_layer_count(texture);
	ERR_FAIL_COND_V(p_layer >= layer_count, ERR_INVALID_PARAMETER);

	uint32_t width, height;
	uint32_t tight_mip_size = get_image_format_required_size(texture->format, texture->width, texture->height, texture->depth, texture->mipmaps, &width, &height);
	uint32_t required_size = tight_mip_size;
	uint32_t required_align = _texture_alignment(texture);

	ERR_FAIL_COND_V_MSG(required_size != (uint32_t)p_data.size(), ERR_INVALID_PARAMETER,
			"Required size for texture update (" + itos(required_size) + ") does not match data supplied size (" + itos(p_data.size()) + ").");

	uint32_t block_w, block_h;
	get_compressed_image_format_block_dimensions(texture->format, block_w, block_h);

	uint32_t pixel_size = get_image_format_pixel_size(texture->format);
	uint32_t pixel_rshift = get_compressed_image_format_pixel_rshift(texture->format);
	uint32_t block_size = get_compressed_image_format_block_byte_size(texture->format);

	// The algorithm operates on two passes, one to figure out the total size the staging buffer will require to allocate and another one where the copy is actually performed.
	uint32_t staging_worker_offset = 0;
	uint32_t staging_local_offset = 0;
	TransferWorker *transfer_worker = nullptr;
	const uint8_t *read_ptr = p_data.ptr();
	uint8_t *write_ptr = nullptr;
	for (uint32_t pass = 0; pass < 2; pass++) {
		const bool copy_pass = (pass == 1);
		if (copy_pass) {
			transfer_worker = _acquire_transfer_worker(staging_local_offset, required_align, staging_worker_offset);
			ERR_FAIL_NULL_V(transfer_worker, ERR_BUSY);
			texture->transfer_worker_index = transfer_worker->index;

			{
				MutexLock lock(transfer_worker->operations_mutex);
				texture->transfer_worker_operation = ++transfer_worker->operations_counter;
			}

			staging_local_offset = 0;

			write_ptr = driver->buffer_map(transfer_worker->staging_buffer);
			if (write_ptr == nullptr) {
				texture->transfer_worker_index = -1;
				texture->transfer_worker_operation = 0;
				_release_transfer_worker(transfer_worker);
				return ERR_CANT_CREATE;
			}
			ERR_FAIL_NULL_V(write_ptr, ERR_CANT_CREATE);

			if (driver->api_trait_get(RDD::API_TRAIT_HONORS_PIPELINE_BARRIERS) && driver->api_trait_get(RDD::API_TRAIT_TEXTURES_REQUIRE_LAYOUT_TRANSITIONS)) {
				// Transition the texture to the optimal layout.
				RDD::TextureBarrier tb;
				tb.texture = texture->driver_id;
				tb.dst_access = RDD::BARRIER_ACCESS_COPY_WRITE_BIT;
				tb.prev_layout = RDD::TEXTURE_LAYOUT_UNDEFINED;
				tb.next_layout = p_dst_layout;
				tb.subresources.aspect = texture->barrier_aspect_flags;
				tb.subresources.mipmap_count = texture->mipmaps;
				tb.subresources.base_layer = p_layer;
				tb.subresources.layer_count = 1;
				driver->command_pipeline_barrier(transfer_worker->command_buffer, RDD::PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, RDD::PIPELINE_STAGE_COPY_BIT, {}, {}, tb, {});
			}
		}

		uint32_t mipmap_offset = 0;
		uint32_t logic_width = texture->width;
		uint32_t logic_height = texture->height;
		for (uint32_t mm_i = 0; mm_i < texture->mipmaps; mm_i++) {
			uint32_t depth = 0;
			uint32_t image_total = get_image_format_required_size(texture->format, texture->width, texture->height, texture->depth, mm_i + 1, &width, &height, &depth);

			const uint8_t *read_ptr_mipmap = read_ptr + mipmap_offset;
			tight_mip_size = image_total - mipmap_offset;

			for (uint32_t z = 0; z < depth; z++) {
				if (required_align > 0) {
					uint32_t align_offset = staging_local_offset % required_align;
					if (align_offset != 0) {
						staging_local_offset += required_align - align_offset;
					}
				}

				uint32_t pitch = (width * pixel_size * block_w) >> pixel_rshift;
				uint32_t pitch_step = driver->api_trait_get(RDD::API_TRAIT_TEXTURE_DATA_ROW_PITCH_STEP);
				pitch = STEPIFY(pitch, pitch_step);
				uint32_t to_allocate = pitch * height;
				to_allocate >>= pixel_rshift;

				if (copy_pass) {
					const uint8_t *read_ptr_mipmap_layer = read_ptr_mipmap + (tight_mip_size / depth) * z;
					uint64_t staging_buffer_offset = staging_worker_offset + staging_local_offset;
					uint8_t *write_ptr_mipmap_layer = write_ptr + staging_buffer_offset;
					_copy_region_block_or_regular(read_ptr_mipmap_layer, write_ptr_mipmap_layer, 0, 0, width, width, height, block_w, block_h, pitch, pixel_size, block_size);

					RDD::BufferTextureCopyRegion copy_region;
					copy_region.buffer_offset = staging_buffer_offset;
					copy_region.row_pitch = pitch;
					copy_region.texture_subresource.aspect = texture->read_aspect_flags.has_flag(RDD::TEXTURE_ASPECT_DEPTH_BIT) ? RDD::TEXTURE_ASPECT_DEPTH : RDD::TEXTURE_ASPECT_COLOR;
					copy_region.texture_subresource.mipmap = mm_i;
					copy_region.texture_subresource.layer = p_layer;
					copy_region.texture_offset = Vector3i(0, 0, z);
					copy_region.texture_region_size = Vector3i(logic_width, logic_height, 1);
					driver->command_copy_buffer_to_texture(transfer_worker->command_buffer, transfer_worker->staging_buffer, texture->driver_id, p_dst_layout, copy_region);
				}

				staging_local_offset += to_allocate;
			}

			mipmap_offset = image_total;
			logic_width = MAX(1u, logic_width >> 1);
			logic_height = MAX(1u, logic_height >> 1);
		}

		if (copy_pass) {
			driver->buffer_unmap(transfer_worker->staging_buffer);

			// If the texture does not have a tracker, it means it must be transitioned to the sampling state.
			if (texture->draw_tracker == nullptr && driver->api_trait_get(RDD::API_TRAIT_HONORS_PIPELINE_BARRIERS) && driver->api_trait_get(RDD::API_TRAIT_TEXTURES_REQUIRE_LAYOUT_TRANSITIONS)) {
				RDD::TextureBarrier tb;
				tb.texture = texture->driver_id;
				tb.src_access = RDD::BARRIER_ACCESS_COPY_WRITE_BIT;
				tb.prev_layout = p_dst_layout;
				tb.next_layout = RDD::TEXTURE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				tb.subresources.aspect = texture->barrier_aspect_flags;
				tb.subresources.mipmap_count = texture->mipmaps;
				tb.subresources.base_layer = p_layer;
				tb.subresources.layer_count = 1;
				transfer_worker->texture_barriers.push_back(tb);
			}

			if (p_immediate_flush) {
				_end_transfer_worker(transfer_worker);
				_submit_transfer_worker(transfer_worker);
				if (!_wait_for_transfer_worker(transfer_worker, true)) {
					_release_transfer_worker(transfer_worker);
					return ERR_BUSY;
				}
			}

			_release_transfer_worker(transfer_worker);
		}
	}

	return OK;
}

Error RenderingDevice::texture_update(RID p_texture, uint32_t p_layer, const Vector<uint8_t> &p_data) {
	if (native_immutable_resource_is_held(p_texture) || native_immutable_resource_is_retiring(p_texture)) {
		return ERR_BUSY;
	}
	ERR_RENDER_THREAD_GUARD_V(ERR_UNAVAILABLE);
	if (_recording_error() != OK) {
		return _recording_error();
	}

	ERR_FAIL_COND_V_MSG(draw_list.active, ERR_INVALID_PARAMETER, "Updating textures is forbidden during creation of a draw list.");
	ERR_FAIL_COND_V_MSG(compute_list.active, ERR_INVALID_PARAMETER, "Updating textures is forbidden during creation of a compute list.");
	ERR_FAIL_COND_V_MSG(raytracing_list.active, ERR_INVALID_PARAMETER, "Updating textures is forbidden during creation of a raytracing list.");

	Texture *texture = texture_owner.get_or_null(p_texture);
	ERR_FAIL_NULL_V(texture, ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(texture->qualification_return, ERR_UNAUTHORIZED);

	if (texture->owner != RID()) {
		p_texture = texture->owner;
		texture = texture_owner.get_or_null(texture->owner);
		ERR_FAIL_NULL_V(texture, ERR_BUG); // This is a bug.
		ERR_FAIL_COND_V(texture->qualification_return, ERR_UNAUTHORIZED);
	}

	ERR_FAIL_COND_V_MSG(texture->bound, ERR_CANT_ACQUIRE_RESOURCE,
			"Texture can't be updated while a draw list that uses it as part of a framebuffer is being created. Ensure the draw list is finalized (and that the color/depth texture using it is not set to `RenderingDevice.FINAL_ACTION_CONTINUE`) to update this texture.");

	ERR_FAIL_COND_V_MSG(!(texture->usage_flags & TEXTURE_USAGE_CAN_UPDATE_BIT), ERR_INVALID_PARAMETER, "Texture requires the `RenderingDevice.TEXTURE_USAGE_CAN_UPDATE_BIT` to be set to be updatable.");

	uint32_t layer_count = _texture_layer_count(texture);
	ERR_FAIL_COND_V(p_layer >= layer_count, ERR_INVALID_PARAMETER);

	uint32_t width, height;
	uint32_t tight_mip_size = get_image_format_required_size(texture->format, texture->width, texture->height, texture->depth, texture->mipmaps, &width, &height);
	uint32_t required_size = tight_mip_size;
	uint32_t required_align = _texture_alignment(texture);

	ERR_FAIL_COND_V_MSG(required_size != (uint32_t)p_data.size(), ERR_INVALID_PARAMETER,
			"Required size for texture update (" + itos(required_size) + ") does not match data supplied size (" + itos(p_data.size()) + ").");

	// Clear the texture if the driver requires it during its first use.
	{
		const Error dependency_error = _texture_check_pending_clear(p_texture, texture);
		if (dependency_error != OK) {
			return dependency_error;
		}
	}

	{
		const Error dependency_error = _check_transfer_worker_texture(texture);
		if (dependency_error != OK) {
			return dependency_error;
		}
	}

	uint32_t block_w, block_h;
	get_compressed_image_format_block_dimensions(texture->format, block_w, block_h);

	uint32_t pixel_size = get_image_format_pixel_size(texture->format);
	uint32_t pixel_rshift = get_compressed_image_format_pixel_rshift(texture->format);
	uint32_t block_size = get_compressed_image_format_block_byte_size(texture->format);

	uint32_t region_size = texture_upload_region_size_px;

	const uint8_t *read_ptr = p_data.ptr();

	thread_local LocalVector<RDG::RecordedBufferToTextureCopy> command_buffer_to_texture_copies_vector;
	command_buffer_to_texture_copies_vector.clear();

	// Indicate the texture will get modified for the shared texture fallback.
	_texture_update_shared_fallback(p_texture, texture, true);

	uint32_t mipmap_offset = 0;

	uint32_t logic_width = texture->width;
	uint32_t logic_height = texture->height;

	for (uint32_t mm_i = 0; mm_i < texture->mipmaps; mm_i++) {
		uint32_t depth = 0;
		uint32_t image_total = get_image_format_required_size(texture->format, texture->width, texture->height, texture->depth, mm_i + 1, &width, &height, &depth);

		const uint8_t *read_ptr_mipmap = read_ptr + mipmap_offset;
		tight_mip_size = image_total - mipmap_offset;

		for (uint32_t z = 0; z < depth; z++) {
			const uint8_t *read_ptr_mipmap_layer = read_ptr_mipmap + (tight_mip_size / depth) * z;
			for (uint32_t y = 0; y < height; y += region_size) {
				for (uint32_t x = 0; x < width; x += region_size) {
					uint32_t region_w = MIN(region_size, width - x);
					uint32_t region_h = MIN(region_size, height - y);

					uint32_t region_logic_w = MIN(region_size, logic_width - x);
					uint32_t region_logic_h = MIN(region_size, logic_height - y);

					uint32_t region_pitch = (region_w * pixel_size * block_w) >> pixel_rshift;
					uint32_t pitch_step = driver->api_trait_get(RDD::API_TRAIT_TEXTURE_DATA_ROW_PITCH_STEP);
					region_pitch = STEPIFY(region_pitch, pitch_step);
					uint32_t to_allocate = region_pitch * region_h;
					uint32_t alloc_offset = 0, alloc_size = 0;
					StagingRequiredAction required_action;
					Error err = _staging_buffer_allocate(upload_staging_buffers, to_allocate, required_align, alloc_offset, alloc_size, required_action, false);
					ERR_FAIL_COND_V(err, ERR_CANT_CREATE);

					if (!command_buffer_to_texture_copies_vector.is_empty() && required_action == STAGING_REQUIRED_ACTION_FLUSH_AND_STALL_ALL) {
						if (_texture_make_mutable(texture, p_texture)) {
							// The texture must be mutable to be used as a copy destination.
							draw_graph.add_synchronization();
						}

						// If the staging buffer requires flushing everything, we submit the command early and clear the current vector.
						draw_graph.add_texture_update(texture->driver_id, texture->draw_tracker, command_buffer_to_texture_copies_vector);
						command_buffer_to_texture_copies_vector.clear();
					}

					err = _staging_buffer_execute_required_action(upload_staging_buffers, required_action);
					if (err != OK) {
						return err;
					}

					uint8_t *write_ptr = upload_staging_buffers.blocks[upload_staging_buffers.current].data_ptr + alloc_offset;

					ERR_FAIL_COND_V(region_w % block_w, ERR_BUG);
					ERR_FAIL_COND_V(region_h % block_h, ERR_BUG);

					_copy_region_block_or_regular(read_ptr_mipmap_layer, write_ptr, x, y, width, region_w, region_h, block_w, block_h, region_pitch, pixel_size, block_size);

					RDD::BufferTextureCopyRegion copy_region;
					copy_region.buffer_offset = alloc_offset;
					copy_region.row_pitch = region_pitch;
					copy_region.texture_subresource.aspect = texture->read_aspect_flags.has_flag(RDD::TEXTURE_ASPECT_DEPTH_BIT) ? RDD::TEXTURE_ASPECT_DEPTH : RDD::TEXTURE_ASPECT_COLOR;
					copy_region.texture_subresource.mipmap = mm_i;
					copy_region.texture_subresource.layer = p_layer;
					copy_region.texture_offset = Vector3i(x, y, z);
					copy_region.texture_region_size = Vector3i(region_logic_w, region_logic_h, 1);

					RDG::RecordedBufferToTextureCopy buffer_to_texture_copy;
					buffer_to_texture_copy.from_buffer = upload_staging_buffers.blocks[upload_staging_buffers.current].driver_id;
					buffer_to_texture_copy.region = copy_region;
					command_buffer_to_texture_copies_vector.push_back(buffer_to_texture_copy);

					upload_staging_buffers.blocks.write[upload_staging_buffers.current].fill_amount = alloc_offset + alloc_size;
				}
			}
		}

		mipmap_offset = image_total;
		logic_width = MAX(1u, logic_width >> 1);
		logic_height = MAX(1u, logic_height >> 1);
	}

	if (_texture_make_mutable(texture, p_texture)) {
		// The texture must be mutable to be used as a copy destination.
		draw_graph.add_synchronization();
	}

	draw_graph.add_texture_update(texture->driver_id, texture->draw_tracker, command_buffer_to_texture_copies_vector);

	return OK;
}

// Original checked image upload: each accepted row is a real graph command.
// The full immutable tight image is supplied on each retry; only its suffix is staged.
Error RenderingDevice::_qualification_texture_upload(RID rid, const uint8_t *bytes, uint32_t size, QualificationUploadProgress &progress) {
	if (!qualification_recording || !bytes || _recording_error() != OK) {
		return ERR_INVALID_PARAMETER;
	}
	auto *texture = texture_owner.get_or_null(rid);
	if (!texture || !texture->qualification_return || !texture->driver_id || texture->owner.is_valid() || texture->shared_fallback || texture->transfer_worker_index >= 0 || texture->mipmaps != 1 || texture->layers != 1 || texture->bound) {
		return ERR_UNAVAILABLE;
	}
	const uint32_t pixel = texture->format == DATA_FORMAT_R32G32B32A32_SFLOAT ? 16 : texture->format == DATA_FORMAT_R8G8B8A8_UNORM ? 4
																																   : 0;
	const uint64_t row64 = uint64_t(texture->width) * pixel, rows = uint64_t(texture->height) * texture->depth;
	if (!pixel || !row64 || !rows || rows > 4096 || row64 * rows != size || progress.consumed > size || progress.consumed % row64 || progress.max_segments != rows) {
		return ERR_INVALID_DATA;
	}
	const uint32_t row_bytes = uint32_t(row64);
	const uint32_t pitch_step = MAX(1U, uint32_t(driver->api_trait_get(RDD::API_TRAIT_TEXTURE_DATA_ROW_PITCH_STEP)));
	const uint64_t pitch64 = ((row64 + pitch_step - 1) / pitch_step) * pitch_step;
	if (pitch64 > UINT32_MAX || pitch64 > upload_staging_buffers.block_size || pitch64 > upload_staging_buffers.max_size) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	while (progress.consumed < size) {
		uint32_t offset = 0, amount = 0;
		StagingRequiredAction action;
		const Error error = _staging_buffer_allocate(upload_staging_buffers, uint32_t(pitch64), _texture_alignment(texture), offset, amount, action, true);
		if (error != OK) {
			return error;
		}
		if (action != STAGING_REQUIRED_ACTION_NONE || amount < pitch64) {
			return ERR_BUSY;
		}
		if (progress.segments.size() >= progress.max_segments) {
			return ERR_INVALID_DATA;
		}
		uint8_t *destination = upload_staging_buffers.blocks[upload_staging_buffers.current].data_ptr + offset;
		memcpy(destination, bytes + progress.consumed, row_bytes);
		if (pitch64 > row_bytes) {
			memset(destination + row_bytes, 0, pitch64 - row_bytes);
		}
		const uint32_t row = progress.consumed / row_bytes;
		RDG::RecordedBufferToTextureCopy copy;
		copy.from_buffer = upload_staging_buffers.blocks[upload_staging_buffers.current].driver_id;
		copy.region.buffer_offset = offset;
		copy.region.row_pitch = uint32_t(pitch64);
		copy.region.texture_subresource.aspect = RDD::TEXTURE_ASPECT_COLOR;
		copy.region.texture_subresource.mipmap = 0;
		copy.region.texture_subresource.layer = 0;
		copy.region.texture_offset = Vector3i(0, row % texture->height, row / texture->height);
		copy.region.texture_region_size = Vector3i(texture->width, 1, 1);
		if (_texture_make_mutable(texture, rid)) {
			draw_graph.add_synchronization();
		}
		const auto handle = draw_graph.add_texture_update(texture->driver_id, texture->draw_tracker, VectorView(&copy, 1));
		upload_staging_buffers.blocks.write[upload_staging_buffers.current].fill_amount = offset + amount;
		progress.segments.push_back({ handle, progress.consumed, progress.consumed + row_bytes });
		progress.consumed += row_bytes;
	}
	// Every pixel now has an original upload command; no implicit clear is needed.
	texture->pending_clear = false;
	texture->has_initial_data = true;
	return OK;
}

void RenderingDevice::_texture_check_shared_fallback(Texture *p_texture) {
	if (p_texture->shared_fallback == nullptr) {
		p_texture->shared_fallback = memnew(Texture::SharedFallback);
	}
}

void RenderingDevice::_texture_update_shared_fallback(RID p_texture_rid, Texture *p_texture, bool p_for_writing) {
	if (p_texture->shared_fallback == nullptr) {
		// This texture does not use any of the shared texture fallbacks.
		return;
	}

	if (p_texture->owner.is_valid()) {
		Texture *owner_texture = texture_owner.get_or_null(p_texture->owner);
		ERR_FAIL_NULL(owner_texture);
		if (p_for_writing) {
			// Only the main texture is used for writing when using the shared fallback.
			owner_texture->shared_fallback->revision++;
		} else if (p_texture->shared_fallback->revision != owner_texture->shared_fallback->revision) {
			// Copy the contents of the main texture into the shared texture fallback slice. Update the revision.
			_texture_copy_shared(p_texture->owner, owner_texture, p_texture_rid, p_texture);
			p_texture->shared_fallback->revision = owner_texture->shared_fallback->revision;
		}
	} else if (p_for_writing) {
		// Increment the revision of the texture so shared texture fallback slices must be updated.
		p_texture->shared_fallback->revision++;
	}
}

void RenderingDevice::_texture_free_shared_fallback(Texture *p_texture) {
	if (p_texture->shared_fallback != nullptr) {
		if (p_texture->shared_fallback->texture_tracker != nullptr) {
			RDG::resource_tracker_free(p_texture->shared_fallback->texture_tracker);
		}

		if (p_texture->shared_fallback->buffer_tracker != nullptr) {
			RDG::resource_tracker_free(p_texture->shared_fallback->buffer_tracker);
		}

		if (p_texture->shared_fallback->texture.id != 0) {
			texture_memory -= driver->texture_get_allocation_size(p_texture->shared_fallback->texture);
			driver->texture_free(p_texture->shared_fallback->texture);
		}

		if (p_texture->shared_fallback->buffer.id != 0) {
			buffer_memory -= driver->buffer_get_allocation_size(p_texture->shared_fallback->buffer);
			driver->buffer_free(p_texture->shared_fallback->buffer);
		}

		memdelete(p_texture->shared_fallback);
		p_texture->shared_fallback = nullptr;
	}
}

void RenderingDevice::_texture_copy_shared(RID p_src_texture_rid, Texture *p_src_texture, RID p_dst_texture_rid, Texture *p_dst_texture) {
	ERR_FAIL_COND_MSG(_recording_error() != OK, "Recording is suspended after a frame fence wait failure.");
	// The only type of copying allowed is from the main texture to the slice texture, as slice textures are not allowed to be used for writing when using this fallback.
	DEV_ASSERT(p_src_texture != nullptr);
	DEV_ASSERT(p_dst_texture != nullptr);
	DEV_ASSERT(p_src_texture->owner.is_null());
	DEV_ASSERT(p_dst_texture->owner == p_src_texture_rid);

	bool src_made_mutable = _texture_make_mutable(p_src_texture, p_src_texture_rid);
	bool dst_made_mutable = _texture_make_mutable(p_dst_texture, p_dst_texture_rid);
	if (src_made_mutable || dst_made_mutable) {
		draw_graph.add_synchronization();
	}

	if (p_dst_texture->shared_fallback->raw_reinterpretation) {
		// If one of the textures is a main texture and they have a reinterpret buffer, we prefer using that as it's guaranteed to be big enough to hold
		// anything and it's how the shared textures that don't use slices are created.
		bool src_has_buffer = p_src_texture->shared_fallback->buffer.id != 0;
		bool dst_has_buffer = p_dst_texture->shared_fallback->buffer.id != 0;
		bool from_src = p_src_texture->owner.is_null() && src_has_buffer;
		bool from_dst = p_dst_texture->owner.is_null() && dst_has_buffer;
		if (!from_src && !from_dst) {
			// If neither texture passed the condition, we just pick whichever texture has a reinterpretation buffer.
			from_src = src_has_buffer;
			from_dst = dst_has_buffer;
		}

		// Pick the buffer and tracker to use from the right texture.
		RDD::BufferID shared_buffer;
		RDG::ResourceTracker *shared_buffer_tracker = nullptr;
		if (from_src) {
			shared_buffer = p_src_texture->shared_fallback->buffer;
			shared_buffer_tracker = p_src_texture->shared_fallback->buffer_tracker;
		} else if (from_dst) {
			shared_buffer = p_dst_texture->shared_fallback->buffer;
			shared_buffer_tracker = p_dst_texture->shared_fallback->buffer_tracker;
		} else {
			DEV_ASSERT(false && "This path should not be reachable.");
		}

		// Copying each mipmap from main texture to a buffer and then to the slice texture.
		thread_local LocalVector<RDD::BufferTextureCopyRegion> get_data_vector;
		thread_local LocalVector<RDG::RecordedBufferToTextureCopy> update_vector;
		get_data_vector.clear();
		update_vector.clear();

		uint32_t buffer_size = 0;
		uint32_t transfer_alignment = driver->api_trait_get(RDD::API_TRAIT_TEXTURE_TRANSFER_ALIGNMENT);

		for (uint32_t i = 0; i < p_dst_texture->layers; i++) {
			for (uint32_t j = 0; j < p_dst_texture->mipmaps; j++) {
				// FIXME: When using reinterpretation buffers, the only texture aspect supported is color. Depth or stencil contents won't get copied.
				RDD::TextureSubresource texture_subresource;
				texture_subresource.aspect = RDD::TEXTURE_ASPECT_COLOR;
				texture_subresource.layer = i;
				texture_subresource.mipmap = j;

				RDD::TextureCopyableLayout copyable_layout;
				driver->texture_get_copyable_layout(p_dst_texture->shared_fallback->texture, texture_subresource, &copyable_layout);

				uint32_t mipmap = p_dst_texture->base_mipmap + j;

				RDD::BufferTextureCopyRegion get_data_region;
				get_data_region.buffer_offset = STEPIFY(buffer_size, transfer_alignment);
				get_data_region.row_pitch = copyable_layout.row_pitch;
				get_data_region.texture_subresource.aspect = RDD::TEXTURE_ASPECT_COLOR;
				get_data_region.texture_subresource.layer = p_dst_texture->base_layer + i;
				get_data_region.texture_subresource.mipmap = mipmap;
				get_data_region.texture_region_size.x = MAX(1U, p_src_texture->width >> mipmap);
				get_data_region.texture_region_size.y = MAX(1U, p_src_texture->height >> mipmap);
				get_data_region.texture_region_size.z = MAX(1U, p_src_texture->depth >> mipmap);
				get_data_vector.push_back(get_data_region);

				RDG::RecordedBufferToTextureCopy update_copy;
				update_copy.from_buffer = shared_buffer;
				update_copy.region.buffer_offset = get_data_region.buffer_offset;
				update_copy.region.row_pitch = get_data_region.row_pitch;
				update_copy.region.texture_subresource.aspect = RDD::TEXTURE_ASPECT_COLOR;
				update_copy.region.texture_subresource.layer = texture_subresource.layer;
				update_copy.region.texture_subresource.mipmap = texture_subresource.mipmap;
				update_copy.region.texture_region_size.x = get_data_region.texture_region_size.x;
				update_copy.region.texture_region_size.y = get_data_region.texture_region_size.y;
				update_copy.region.texture_region_size.z = get_data_region.texture_region_size.z;
				update_vector.push_back(update_copy);

				buffer_size = get_data_region.buffer_offset + copyable_layout.size;
			}
		}

		DEV_ASSERT(buffer_size <= driver->buffer_get_allocation_size(shared_buffer));

		draw_graph.add_texture_get_data(p_src_texture->driver_id, p_src_texture->draw_tracker, shared_buffer, get_data_vector, shared_buffer_tracker);
		draw_graph.add_texture_update(p_dst_texture->shared_fallback->texture, p_dst_texture->shared_fallback->texture_tracker, update_vector, shared_buffer_tracker);
	} else {
		// Raw reinterpretation is not required. Use a regular texture copy.
		RDD::TextureCopyRegion copy_region;
		copy_region.src_subresources.aspect = p_src_texture->read_aspect_flags;
		copy_region.src_subresources.base_layer = p_dst_texture->base_layer;
		copy_region.src_subresources.layer_count = p_dst_texture->layers;
		copy_region.dst_subresources.aspect = p_dst_texture->read_aspect_flags;
		copy_region.dst_subresources.base_layer = 0;
		copy_region.dst_subresources.layer_count = copy_region.src_subresources.layer_count;

		// Copying each mipmap from main texture to to the slice texture.
		thread_local LocalVector<RDD::TextureCopyRegion> region_vector;
		region_vector.clear();
		for (uint32_t i = 0; i < p_dst_texture->mipmaps; i++) {
			uint32_t mipmap = p_dst_texture->base_mipmap + i;
			copy_region.src_subresources.mipmap = mipmap;
			copy_region.dst_subresources.mipmap = i;
			copy_region.size.x = MAX(1U, p_src_texture->width >> mipmap);
			copy_region.size.y = MAX(1U, p_src_texture->height >> mipmap);
			copy_region.size.z = MAX(1U, p_src_texture->depth >> mipmap);
			region_vector.push_back(copy_region);
		}

		draw_graph.add_texture_copy(p_src_texture->driver_id, p_src_texture->draw_tracker, p_dst_texture->shared_fallback->texture, p_dst_texture->shared_fallback->texture_tracker, region_vector);
	}
}

bool RenderingDevice::_texture_create_reinterpret_buffer(Texture *p_texture) {
	ERR_FAIL_COND_V(!p_texture || !p_texture->shared_fallback, false);
	if (p_texture->shared_fallback->buffer) {
		return true;
	}
	uint64_t row_pitch_step = driver->api_trait_get(RDD::API_TRAIT_TEXTURE_DATA_ROW_PITCH_STEP);
	uint64_t transfer_alignment = driver->api_trait_get(RDD::API_TRAIT_TEXTURE_TRANSFER_ALIGNMENT);
	uint32_t pixel_bytes = get_image_format_pixel_size(p_texture->format);
	uint32_t row_pitch = STEPIFY(p_texture->width * pixel_bytes, row_pitch_step);
	uint64_t buffer_size = STEPIFY(pixel_bytes * row_pitch * p_texture->height * p_texture->depth, transfer_alignment);
	const RDD::BufferID buffer = driver->buffer_create(buffer_size, RDD::BUFFER_USAGE_TRANSFER_FROM_BIT | RDD::BUFFER_USAGE_TRANSFER_TO_BIT, RDD::MEMORY_ALLOCATION_TYPE_GPU, frames_drawn);
	if (!buffer) {
		return false;
	}
	p_texture->shared_fallback->buffer = buffer;
	buffer_memory += driver->buffer_get_allocation_size(buffer);

	RDG::ResourceTracker *tracker = RDG::resource_tracker_create();
	tracker->buffer_driver_id = buffer;
	p_texture->shared_fallback->buffer_tracker = tracker;
	return true;
}

uint32_t RenderingDevice::_texture_vrs_method_to_usage_bits() const {
	switch (vrs_method) {
		case VRS_METHOD_FRAGMENT_SHADING_RATE:
			return RDD::TEXTURE_USAGE_VRS_FRAGMENT_SHADING_RATE_BIT;
		case VRS_METHOD_FRAGMENT_DENSITY_MAP:
			return RDD::TEXTURE_USAGE_VRS_FRAGMENT_DENSITY_MAP_BIT;
		default:
			return 0;
	}
}

void RenderingDevice::_texture_ensure_shareable_format(RID p_texture, const DataFormat &p_shareable_format) {
	Texture *texture = texture_owner.get_or_null(p_texture);
	ERR_FAIL_NULL(texture);
	if (!texture->allowed_shared_formats.has(p_shareable_format)) {
		texture->allowed_shared_formats.push_back(p_shareable_format);
	}
}

Error RenderingDevice::_texture_check_pending_clear(RID p_texture_rid, Texture *p_texture) {
	ERR_FAIL_COND_V_MSG(_recording_error() != OK, _recording_error(), "Texture clear recording is suspended during frame fence recovery or readback callbacks.");
	DEV_ASSERT(p_texture != nullptr);

	if (!p_texture->pending_clear) {
		return OK;
	}

	bool clear = true;
	Texture *requested_texture = p_texture;

	if (p_texture->owner.is_valid()) {
		// Check the owner texture instead if it exists.
		p_texture_rid = p_texture->owner;
		p_texture = texture_owner.get_or_null(p_texture_rid);
		ERR_FAIL_NULL_V(p_texture, ERR_INVALID_DATA);
		clear = p_texture->pending_clear;
	}

	if (p_texture != nullptr && clear) {
		const Error admission = _check_transfer_worker_texture(p_texture);
		if (admission != OK) {
			return admission;
		}
		if (p_texture->usage_flags & TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
			{
				const Error dependency_error = _texture_clear_depth_stencil(p_texture_rid, p_texture, 0.0f, 0, 0, p_texture->mipmaps, 0, p_texture->layers);
				if (dependency_error != OK) {
					return dependency_error;
				}
			}
		} else {
			{
				const Error dependency_error = _texture_clear_color(p_texture_rid, p_texture, Color(), 0, p_texture->mipmaps, 0, p_texture->layers);
				if (dependency_error != OK) {
					return dependency_error;
				}
			}
		}
	}
	requested_texture->pending_clear = false;
	return OK;
}

Error RenderingDevice::_texture_clear_color(RID p_texture_rid, Texture *p_texture, const Color &p_color, uint32_t p_base_mipmap, uint32_t p_mipmaps, uint32_t p_base_layer, uint32_t p_layers) {
	ERR_FAIL_COND_V_MSG(_recording_error() != OK, _recording_error(), "Recording is suspended after a frame fence wait failure.");
	{
		const Error dependency_error = _check_transfer_worker_texture(p_texture);
		if (dependency_error != OK) {
			return dependency_error;
		}
	}

	RDD::TextureSubresourceRange range;
	range.aspect = RDD::TEXTURE_ASPECT_COLOR_BIT;
	range.base_mipmap = p_texture->base_mipmap + p_base_mipmap;
	range.mipmap_count = p_mipmaps;
	range.base_layer = p_texture->base_layer + p_base_layer;
	range.layer_count = p_layers;

	// Indicate the texture will get modified for the shared texture fallback.
	_texture_update_shared_fallback(p_texture_rid, p_texture, true);

	if (_texture_make_mutable(p_texture, p_texture_rid)) {
		// The texture must be mutable to be used as a clear destination.
		draw_graph.add_synchronization();
	}

	draw_graph.add_texture_clear_color(p_texture->driver_id, p_texture->draw_tracker, p_color, range);
	return OK;
}

Error RenderingDevice::_texture_clear_depth_stencil(RID p_texture_rid, Texture *p_texture, float p_depth, uint8_t p_stencil, uint32_t p_base_mipmap, uint32_t p_mipmaps, uint32_t p_base_layer, uint32_t p_layers) {
	ERR_FAIL_COND_V_MSG(_recording_error() != OK, _recording_error(), "Recording is suspended after a frame fence wait failure.");
	{
		const Error dependency_error = _check_transfer_worker_texture(p_texture);
		if (dependency_error != OK) {
			return dependency_error;
		}
	}

	RDD::TextureSubresourceRange range;
	if (format_has_depth(p_texture->format)) {
		range.aspect.set_flag(RDD::TEXTURE_ASPECT_DEPTH_BIT);
	}
	if (format_has_stencil(p_texture->format)) {
		range.aspect.set_flag(RDD::TEXTURE_ASPECT_STENCIL_BIT);
	}
	range.base_mipmap = p_texture->base_mipmap + p_base_mipmap;
	range.mipmap_count = p_mipmaps;
	range.base_layer = p_texture->base_layer + p_base_layer;
	range.layer_count = p_layers;

	// Indicate the texture will get modified for the shared texture fallback.
	_texture_update_shared_fallback(p_texture_rid, p_texture, true);

	if (_texture_make_mutable(p_texture, p_texture_rid)) {
		// The texture must be mutable to be used as a clear destination.
		draw_graph.add_synchronization();
	}

	draw_graph.add_texture_clear_depth_stencil(p_texture->driver_id, p_texture->draw_tracker, p_depth, p_stencil, range);
	return OK;
}

Vector<uint8_t> RenderingDevice::texture_get_data(RID p_texture, uint32_t p_layer) {
	ERR_FAIL_COND_V_MSG(_recording_error() != OK, Vector<uint8_t>(), "A frame fence wait failed; readback recording is suspended.");
	ERR_RENDER_THREAD_GUARD_V(Vector<uint8_t>());

	Texture *tex = texture_owner.get_or_null(p_texture);
	ERR_FAIL_NULL_V(tex, Vector<uint8_t>());
	ERR_FAIL_COND_V(tex->qualification_return, Vector<uint8_t>());

	ERR_FAIL_COND_V_MSG(tex->bound, Vector<uint8_t>(),
			"Texture can't be retrieved while a draw list that uses it as part of a framebuffer is being created. Ensure the draw list is finalized (and that the color/depth texture using it is not set to `RenderingDevice.FINAL_ACTION_CONTINUE`) to retrieve this texture.");
	ERR_FAIL_COND_V_MSG(!(tex->usage_flags & TEXTURE_USAGE_CAN_COPY_FROM_BIT), Vector<uint8_t>(),
			"Texture requires the `RenderingDevice.TEXTURE_USAGE_CAN_COPY_FROM_BIT` to be set to be retrieved.");

	ERR_FAIL_COND_V(p_layer >= tex->layers, Vector<uint8_t>());

	// Clear the texture if the driver requires it during its first use.
	{
		const Error dependency_error = _texture_check_pending_clear(p_texture, tex);
		if (dependency_error != OK) {
			return Vector<uint8_t>();
		}
	}

	{
		const Error dependency_error = _check_transfer_worker_texture(tex);
		if (dependency_error != OK) {
			return Vector<uint8_t>();
		}
	}

	if (tex->usage_flags & TEXTURE_USAGE_CPU_READ_BIT) {
		return driver->texture_get_data(tex->driver_id, p_layer);
	} else {
		RDD::TextureAspect aspect = tex->read_aspect_flags.has_flag(RDD::TEXTURE_ASPECT_DEPTH_BIT) ? RDD::TEXTURE_ASPECT_DEPTH : RDD::TEXTURE_ASPECT_COLOR;
		uint32_t mip_alignment = driver->api_trait_get(RDD::API_TRAIT_TEXTURE_TRANSFER_ALIGNMENT);
		uint32_t buffer_size = 0;

		thread_local LocalVector<RDD::TextureCopyableLayout> mip_layouts;
		thread_local LocalVector<RDD::BufferTextureCopyRegion> copy_regions;
		mip_layouts.resize(tex->mipmaps);
		copy_regions.resize(tex->mipmaps);

		for (uint32_t i = 0; i < tex->mipmaps; i++) {
			RDD::TextureSubresource subres;
			subres.aspect = aspect;
			subres.layer = p_layer;
			subres.mipmap = i;

			RDD::TextureCopyableLayout &mip_layout = mip_layouts[i];
			driver->texture_get_copyable_layout(tex->driver_id, subres, &mip_layout);

			uint32_t mip_offset = STEPIFY(buffer_size, mip_alignment);
			buffer_size = mip_offset + mip_layout.size;

			RDD::BufferTextureCopyRegion &copy_region = copy_regions[i];
			copy_region.buffer_offset = mip_offset;
			copy_region.row_pitch = mip_layout.row_pitch;
			copy_region.texture_subresource.aspect = aspect;
			copy_region.texture_subresource.mipmap = i;
			copy_region.texture_subresource.layer = p_layer;
			copy_region.texture_region_size.x = MAX(1u, tex->width >> i);
			copy_region.texture_region_size.y = MAX(1u, tex->height >> i);
			copy_region.texture_region_size.z = MAX(1u, tex->depth >> i);
		}

		RDD::BufferID tmp_buffer = driver->buffer_create(buffer_size, RDD::BUFFER_USAGE_TRANSFER_TO_BIT, RDD::MEMORY_ALLOCATION_TYPE_CPU, frames_drawn);
		ERR_FAIL_COND_V(!tmp_buffer, Vector<uint8_t>());

		if (_texture_make_mutable(tex, p_texture)) {
			// The texture must be mutable to be used as a copy source due to layout transitions.
			draw_graph.add_synchronization();
		}

		draw_graph.add_texture_get_data(tex->driver_id, tex->draw_tracker, tmp_buffer, copy_regions);

		// Flush everything so memory can be safely mapped.
		const Error wait_error = _flush_and_stall_for_all_frames();
		if (wait_error != OK) {
			Buffer retained_buffer;
			retained_buffer.driver_id = tmp_buffer;
			frames[frame].buffers_to_dispose_of.push_back(retained_buffer);
			ERR_FAIL_V_MSG(Vector<uint8_t>(), "Texture readback fence wait failed.");
		}

		const uint8_t *read_ptr = driver->buffer_map(tmp_buffer);
		if (read_ptr == nullptr) {
			// The temporary readback buffer is no longer referenced by the draw
			// graph after the blocking stall, so release it on map failure too.
			driver->buffer_free(tmp_buffer);
			ERR_FAIL_V(Vector<uint8_t>());
		}

		uint32_t block_w = 0;
		uint32_t block_h = 0;
		get_compressed_image_format_block_dimensions(tex->format, block_w, block_h);

		Vector<uint8_t> buffer_data;
		uint32_t tight_buffer_size = get_image_format_required_size(tex->format, tex->width, tex->height, tex->depth, tex->mipmaps);
		buffer_data.resize(tight_buffer_size);

		uint8_t *write_ptr = buffer_data.ptrw();

		for (uint32_t i = 0; i < tex->mipmaps; i++) {
			uint32_t width = 0, height = 0, depth = 0;

			uint32_t tight_mip_size = get_image_format_required_size(
					tex->format,
					MAX(1u, tex->width >> i),
					MAX(1u, tex->height >> i),
					MAX(1u, tex->depth >> i),
					1,
					&width,
					&height,
					&depth);

			uint32_t row_count = (height / block_h) * depth;
			uint32_t tight_row_pitch = tight_mip_size / row_count;

			const uint8_t *rp = read_ptr + copy_regions[i].buffer_offset;
			uint32_t row_pitch = mip_layouts[i].row_pitch;

			if (tight_row_pitch == row_pitch) {
				// Same row pitch, we can copy directly.
				memcpy(write_ptr, rp, tight_mip_size);
				write_ptr += tight_mip_size;
			} else {
				// Copy row-by-row to erase padding.
				for (uint32_t j = 0; j < row_count; j++) {
					memcpy(write_ptr, rp, tight_row_pitch);
					rp += row_pitch;
					write_ptr += tight_row_pitch;
				}
			}
		}

		driver->buffer_unmap(tmp_buffer);
		driver->buffer_free(tmp_buffer);

		return buffer_data;
	}
}

Error RenderingDevice::texture_get_data_async(RID p_texture, uint32_t p_layer, const Callable &p_callback) {
	ERR_RENDER_THREAD_GUARD_V(ERR_UNAVAILABLE);
	if (_recording_error() != OK) {
		return _recording_error();
	}

	Texture *tex = texture_owner.get_or_null(p_texture);
	ERR_FAIL_NULL_V(tex, ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(tex->qualification_return, ERR_UNAUTHORIZED);

	ERR_FAIL_COND_V_MSG(tex->bound, ERR_INVALID_PARAMETER, "Texture can't be retrieved while a draw list that uses it as part of a framebuffer is being created. Ensure the draw list is finalized (and that the color/depth texture using it is not set to `RenderingDevice.FINAL_ACTION_CONTINUE`) to retrieve this texture.");
	ERR_FAIL_COND_V_MSG(!(tex->usage_flags & TEXTURE_USAGE_CAN_COPY_FROM_BIT), ERR_INVALID_PARAMETER, "Texture requires the `RenderingDevice.TEXTURE_USAGE_CAN_COPY_FROM_BIT` to be set to be retrieved.");
	ERR_FAIL_COND_V(p_layer >= tex->layers, ERR_INVALID_PARAMETER);

	// Clear the texture if the driver requires it during its first use.
	{
		const Error dependency_error = _texture_check_pending_clear(p_texture, tex);
		if (dependency_error != OK) {
			return dependency_error;
		}
	}

	{
		const Error dependency_error = _check_transfer_worker_texture(tex);
		if (dependency_error != OK) {
			return dependency_error;
		}
	}

	if (_texture_make_mutable(tex, p_texture)) {
		// The texture must be mutable to be used as a copy source due to layout transitions.
		draw_graph.add_synchronization();
	}

	TextureGetDataRequest get_data_request;
	get_data_request.callback = p_callback;
	get_data_request.frame_local_index = frames[frame].download_buffer_texture_copy_regions.size();
	get_data_request.width = tex->width;
	get_data_request.height = tex->height;
	get_data_request.depth = tex->depth;
	get_data_request.format = tex->format;
	get_data_request.mipmaps = tex->mipmaps;

	uint32_t block_w, block_h;
	get_compressed_image_format_block_dimensions(tex->format, block_w, block_h);

	uint32_t pixel_size = get_image_format_pixel_size(tex->format);
	uint32_t pixel_rshift = get_compressed_image_format_pixel_rshift(tex->format);

	uint32_t w, h, d;
	uint32_t required_align = driver->api_trait_get(RDD::API_TRAIT_TEXTURE_TRANSFER_ALIGNMENT);
	uint32_t pitch_step = driver->api_trait_get(RDD::API_TRAIT_TEXTURE_DATA_ROW_PITCH_STEP);
	uint32_t region_size = texture_download_region_size_px;
	uint32_t logic_w = tex->width;
	uint32_t logic_h = tex->height;
	uint32_t mipmap_offset = 0;
	uint32_t block_write_offset;
	uint32_t block_write_amount;
	StagingRequiredAction required_action;
	for (uint32_t i = 0; i < tex->mipmaps; i++) {
		uint32_t image_total = get_image_format_required_size(tex->format, tex->width, tex->height, tex->depth, i + 1, &w, &h, &d);
		uint32_t tight_mip_size = image_total - mipmap_offset;
		for (uint32_t z = 0; z < d; z++) {
			for (uint32_t y = 0; y < h; y += region_size) {
				for (uint32_t x = 0; x < w; x += region_size) {
					uint32_t region_w = MIN(region_size, w - x);
					uint32_t region_h = MIN(region_size, h - y);
					ERR_FAIL_COND_V(region_w % block_w, ERR_BUG);
					ERR_FAIL_COND_V(region_h % block_h, ERR_BUG);

					uint32_t region_logic_w = MIN(region_size, logic_w - x);
					uint32_t region_logic_h = MIN(region_size, logic_h - y);
					uint32_t region_pitch = (region_w * pixel_size * block_w) >> pixel_rshift;
					region_pitch = STEPIFY(region_pitch, pitch_step);

					uint32_t to_allocate = region_pitch * region_h;
					Error err = _staging_buffer_allocate(download_staging_buffers, to_allocate, required_align, block_write_offset, block_write_amount, required_action, false);
					ERR_FAIL_COND_V(err, ERR_CANT_CREATE);

					const bool flush_frames = (get_data_request.frame_local_count > 0) && required_action == STAGING_REQUIRED_ACTION_FLUSH_AND_STALL_ALL;
					if (flush_frames) {
						for (uint32_t j = 0; j < get_data_request.frame_local_count; j++) {
							uint32_t local_index = get_data_request.frame_local_index + j;
							draw_graph.add_texture_get_data(tex->driver_id, tex->draw_tracker, frames[frame].download_texture_staging_buffers[local_index], frames[frame].download_buffer_texture_copy_regions[local_index]);
						}
					}

					err = _staging_buffer_execute_required_action(download_staging_buffers, required_action);
					if (err != OK) {
						return err;
					}

					if (flush_frames) {
						get_data_request.frame_local_count = 0;
						get_data_request.frame_local_index = frames[frame].download_buffer_texture_copy_regions.size();
					}

					RDD::BufferTextureCopyRegion copy_region;
					copy_region.buffer_offset = block_write_offset;
					copy_region.row_pitch = region_pitch;
					copy_region.texture_subresource.aspect = tex->read_aspect_flags.has_flag(RDD::TEXTURE_ASPECT_DEPTH_BIT) ? RDD::TEXTURE_ASPECT_DEPTH : RDD::TEXTURE_ASPECT_COLOR;
					copy_region.texture_subresource.mipmap = i;
					copy_region.texture_subresource.layer = p_layer;
					copy_region.texture_offset = Vector3i(x, y, z);
					copy_region.texture_region_size = Vector3i(region_logic_w, region_logic_h, 1);
					frames[frame].download_texture_staging_buffers.push_back(download_staging_buffers.blocks[download_staging_buffers.current].driver_id);
					frames[frame].download_buffer_texture_copy_regions.push_back(copy_region);
					frames[frame].download_texture_mipmap_offsets.push_back(mipmap_offset + (tight_mip_size / d) * z);
					get_data_request.frame_local_count++;

					download_staging_buffers.blocks.write[download_staging_buffers.current].fill_amount = block_write_offset + block_write_amount;
				}
			}
		}

		mipmap_offset = image_total;
		logic_w = MAX(1u, logic_w >> 1);
		logic_h = MAX(1u, logic_h >> 1);
	}

	if (get_data_request.frame_local_count > 0) {
		for (uint32_t i = 0; i < get_data_request.frame_local_count; i++) {
			uint32_t local_index = get_data_request.frame_local_index + i;
			draw_graph.add_texture_get_data(tex->driver_id, tex->draw_tracker, frames[frame].download_texture_staging_buffers[local_index], frames[frame].download_buffer_texture_copy_regions[local_index]);
		}

		frames[frame].download_texture_get_data_requests.push_back(get_data_request);
	}

	return OK;
}

bool RenderingDevice::texture_is_shared(RID p_texture) {
	ERR_RENDER_THREAD_GUARD_V(false);

	Texture *tex = texture_owner.get_or_null(p_texture);
	ERR_FAIL_NULL_V(tex, false);
	return tex->owner.is_valid();
}

bool RenderingDevice::texture_is_valid(RID p_texture) {
	ERR_RENDER_THREAD_GUARD_V(false);

	return texture_owner.owns(p_texture);
}

RD::TextureFormat RenderingDevice::texture_get_format(RID p_texture) {
	ERR_RENDER_THREAD_GUARD_V(TextureFormat());

	Texture *tex = texture_owner.get_or_null(p_texture);
	ERR_FAIL_NULL_V(tex, TextureFormat());

	TextureFormat tf;

	tf.format = tex->format;
	tf.width = tex->width;
	tf.height = tex->height;
	tf.depth = tex->depth;
	tf.array_layers = tex->layers;
	tf.mipmaps = tex->mipmaps;
	tf.texture_type = tex->type;
	tf.samples = tex->samples;
	tf.usage_bits = tex->usage_flags;
	tf.shareable_formats = tex->allowed_shared_formats;
	tf.is_resolve_buffer = tex->is_resolve_buffer;
	tf.is_discardable = tex->is_discardable;

	return tf;
}

Size2i RenderingDevice::texture_size(RID p_texture) {
	ERR_RENDER_THREAD_GUARD_V(Size2i());

	Texture *tex = texture_owner.get_or_null(p_texture);
	ERR_FAIL_NULL_V(tex, Size2i());
	return Size2i(tex->width, tex->height);
}

#ifndef DISABLE_DEPRECATED
uint64_t RenderingDevice::texture_get_native_handle(RID p_texture) {
	return get_driver_resource(DRIVER_RESOURCE_TEXTURE, p_texture);
}
#endif

Error RenderingDevice::texture_copy(RID p_from_texture, RID p_to_texture, const Vector3 &p_from, const Vector3 &p_to, const Vector3 &p_size, uint32_t p_src_mipmap, uint32_t p_dst_mipmap, uint32_t p_src_layer, uint32_t p_dst_layer) {
	if (native_immutable_resource_is_held(p_to_texture) || native_immutable_resource_is_retiring(p_to_texture)) {
		return ERR_BUSY;
	}
	ERR_RENDER_THREAD_GUARD_V(ERR_UNAVAILABLE);
	if (_recording_error() != OK) {
		return _recording_error();
	}

	Texture *src_tex = texture_owner.get_or_null(p_from_texture);
	ERR_FAIL_NULL_V(src_tex, ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(src_tex->qualification_return, ERR_UNAUTHORIZED);

	ERR_FAIL_COND_V_MSG(src_tex->bound, ERR_INVALID_PARAMETER,
			"Source texture can't be copied while a draw list that uses it as part of a framebuffer is being created. Ensure the draw list is finalized (and that the color/depth texture using it is not set to `RenderingDevice.FINAL_ACTION_CONTINUE`) to copy this texture.");
	ERR_FAIL_COND_V_MSG(!(src_tex->usage_flags & TEXTURE_USAGE_CAN_COPY_FROM_BIT), ERR_INVALID_PARAMETER,
			"Source texture requires the `RenderingDevice.TEXTURE_USAGE_CAN_COPY_FROM_BIT` to be set to be retrieved.");

	ERR_FAIL_COND_V(p_size.x == 0, ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(p_size.y == 0, ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(p_size.z == 0, ERR_INVALID_PARAMETER);

	uint32_t src_width, src_height, src_depth;
	get_image_format_required_size(src_tex->format, src_tex->width, src_tex->height, src_tex->depth, p_src_mipmap + 1, &src_width, &src_height, &src_depth);

	ERR_FAIL_COND_V(p_from.x < 0 || p_from.x + p_size.x > src_width, ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(p_from.y < 0 || p_from.y + p_size.y > src_height, ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(p_from.z < 0 || p_from.z + p_size.z > src_depth, ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(p_src_mipmap >= src_tex->mipmaps, ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(p_src_layer >= src_tex->layers, ERR_INVALID_PARAMETER);

	Texture *dst_tex = texture_owner.get_or_null(p_to_texture);
	ERR_FAIL_NULL_V(dst_tex, ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(dst_tex->qualification_return, ERR_UNAUTHORIZED);

	ERR_FAIL_COND_V_MSG(dst_tex->bound, ERR_INVALID_PARAMETER,
			"Destination texture can't be copied while a draw list that uses it as part of a framebuffer is being created. Ensure the draw list is finalized (and that the color/depth texture using it is not set to `RenderingDevice.FINAL_ACTION_CONTINUE`) to copy this texture.");
	ERR_FAIL_COND_V_MSG(!(dst_tex->usage_flags & TEXTURE_USAGE_CAN_COPY_TO_BIT), ERR_INVALID_PARAMETER,
			"Destination texture requires the `RenderingDevice.TEXTURE_USAGE_CAN_COPY_TO_BIT` to be set to be retrieved.");

	uint32_t dst_width, dst_height, dst_depth;
	get_image_format_required_size(dst_tex->format, dst_tex->width, dst_tex->height, dst_tex->depth, p_dst_mipmap + 1, &dst_width, &dst_height, &dst_depth);

	ERR_FAIL_COND_V(p_to.x < 0 || p_to.x + p_size.x > dst_width, ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(p_to.y < 0 || p_to.y + p_size.y > dst_height, ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(p_to.z < 0 || p_to.z + p_size.z > dst_depth, ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(p_dst_mipmap >= dst_tex->mipmaps, ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(p_dst_layer >= dst_tex->layers, ERR_INVALID_PARAMETER);

	ERR_FAIL_COND_V_MSG(src_tex->read_aspect_flags != dst_tex->read_aspect_flags, ERR_INVALID_PARAMETER,
			"Source and destination texture must be of the same type (color or depth).");

	// Clear the textures if the driver requires it during its first use.
	{
		const Error dependency_error = _texture_check_pending_clear(p_from_texture, src_tex);
		if (dependency_error != OK) {
			return dependency_error;
		}
	}
	{
		const Error dependency_error = _texture_check_pending_clear(p_to_texture, dst_tex);
		if (dependency_error != OK) {
			return dependency_error;
		}
	}

	{
		const Error dependency_error = _check_transfer_worker_texture(src_tex);
		if (dependency_error != OK) {
			return dependency_error;
		}
	}
	{
		const Error dependency_error = _check_transfer_worker_texture(dst_tex);
		if (dependency_error != OK) {
			return dependency_error;
		}
	}

	RDD::TextureCopyRegion copy_region;
	copy_region.src_subresources.aspect = src_tex->read_aspect_flags;
	copy_region.src_subresources.mipmap = p_src_mipmap;
	copy_region.src_subresources.base_layer = p_src_layer;
	copy_region.src_subresources.layer_count = 1;
	copy_region.src_offset = p_from;

	copy_region.dst_subresources.aspect = dst_tex->read_aspect_flags;
	copy_region.dst_subresources.mipmap = p_dst_mipmap;
	copy_region.dst_subresources.base_layer = p_dst_layer;
	copy_region.dst_subresources.layer_count = 1;
	copy_region.dst_offset = p_to;

	copy_region.size = p_size;

	// Indicate the texture will get modified for the shared texture fallback.
	_texture_update_shared_fallback(p_to_texture, dst_tex, true);

	// The textures must be mutable to be used in the copy operation.
	bool src_made_mutable = _texture_make_mutable(src_tex, p_from_texture);
	bool dst_made_mutable = _texture_make_mutable(dst_tex, p_to_texture);
	if (src_made_mutable || dst_made_mutable) {
		draw_graph.add_synchronization();
	}

	draw_graph.add_texture_copy(src_tex->driver_id, src_tex->draw_tracker, dst_tex->driver_id, dst_tex->draw_tracker, copy_region);

	return OK;
}

Error RenderingDevice::texture_resolve_multisample(RID p_from_texture, RID p_to_texture) {
	if (native_immutable_resource_is_held(p_to_texture) || native_immutable_resource_is_retiring(p_to_texture)) {
		return ERR_BUSY;
	}
	ERR_RENDER_THREAD_GUARD_V(ERR_UNAVAILABLE);
	if (_recording_error() != OK) {
		return _recording_error();
	}

	Texture *src_tex = texture_owner.get_or_null(p_from_texture);
	ERR_FAIL_NULL_V(src_tex, ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(src_tex->qualification_return, ERR_UNAUTHORIZED);

	ERR_FAIL_COND_V_MSG(src_tex->bound, ERR_INVALID_PARAMETER,
			"Source texture can't be copied while a draw list that uses it as part of a framebuffer is being created. Ensure the draw list is finalized (and that the color/depth texture using it is not set to `RenderingDevice.FINAL_ACTION_CONTINUE`) to copy this texture.");
	ERR_FAIL_COND_V_MSG(!(src_tex->usage_flags & TEXTURE_USAGE_CAN_COPY_FROM_BIT), ERR_INVALID_PARAMETER,
			"Source texture requires the `RenderingDevice.TEXTURE_USAGE_CAN_COPY_FROM_BIT` to be set to be retrieved.");

	ERR_FAIL_COND_V_MSG(src_tex->type != TEXTURE_TYPE_2D, ERR_INVALID_PARAMETER, "Source texture must be 2D (or a slice of a 3D/Cube texture)");
	ERR_FAIL_COND_V_MSG(src_tex->samples == TEXTURE_SAMPLES_1, ERR_INVALID_PARAMETER, "Source texture must be multisampled.");

	Texture *dst_tex = texture_owner.get_or_null(p_to_texture);
	ERR_FAIL_NULL_V(dst_tex, ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(dst_tex->qualification_return, ERR_UNAUTHORIZED);

	ERR_FAIL_COND_V_MSG(dst_tex->bound, ERR_INVALID_PARAMETER,
			"Destination texture can't be copied while a draw list that uses it as part of a framebuffer is being created. Ensure the draw list is finalized (and that the color/depth texture using it is not set to `RenderingDevice.FINAL_ACTION_CONTINUE`) to copy this texture.");
	ERR_FAIL_COND_V_MSG(!(dst_tex->usage_flags & TEXTURE_USAGE_CAN_COPY_TO_BIT), ERR_INVALID_PARAMETER,
			"Destination texture requires the `RenderingDevice.TEXTURE_USAGE_CAN_COPY_TO_BIT` to be set to be retrieved.");

	ERR_FAIL_COND_V_MSG(dst_tex->type != TEXTURE_TYPE_2D, ERR_INVALID_PARAMETER, "Destination texture must be 2D (or a slice of a 3D/Cube texture).");
	ERR_FAIL_COND_V_MSG(dst_tex->samples != TEXTURE_SAMPLES_1, ERR_INVALID_PARAMETER, "Destination texture must not be multisampled.");

	ERR_FAIL_COND_V_MSG(src_tex->format != dst_tex->format, ERR_INVALID_PARAMETER, "Source and Destination textures must be the same format.");
	ERR_FAIL_COND_V_MSG(src_tex->width != dst_tex->width && src_tex->height != dst_tex->height && src_tex->depth != dst_tex->depth, ERR_INVALID_PARAMETER, "Source and Destination textures must have the same dimensions.");

	ERR_FAIL_COND_V_MSG(src_tex->read_aspect_flags != dst_tex->read_aspect_flags, ERR_INVALID_PARAMETER,
			"Source and destination texture must be of the same type (color or depth).");

	// Indicate the texture will get modified for the shared texture fallback.
	_texture_update_shared_fallback(p_to_texture, dst_tex, true);

	// Clear the textures if the driver requires it during its first use.
	{
		const Error dependency_error = _texture_check_pending_clear(p_from_texture, src_tex);
		if (dependency_error != OK) {
			return dependency_error;
		}
	}
	{
		const Error dependency_error = _texture_check_pending_clear(p_to_texture, dst_tex);
		if (dependency_error != OK) {
			return dependency_error;
		}
	}

	{
		const Error dependency_error = _check_transfer_worker_texture(src_tex);
		if (dependency_error != OK) {
			return dependency_error;
		}
	}
	{
		const Error dependency_error = _check_transfer_worker_texture(dst_tex);
		if (dependency_error != OK) {
			return dependency_error;
		}
	}

	// The textures must be mutable to be used in the resolve operation.
	bool src_made_mutable = _texture_make_mutable(src_tex, p_from_texture);
	bool dst_made_mutable = _texture_make_mutable(dst_tex, p_to_texture);
	if (src_made_mutable || dst_made_mutable) {
		draw_graph.add_synchronization();
	}

	draw_graph.add_texture_resolve(src_tex->driver_id, src_tex->draw_tracker, dst_tex->driver_id, dst_tex->draw_tracker, src_tex->base_layer, src_tex->base_mipmap, dst_tex->base_layer, dst_tex->base_mipmap);

	return OK;
}

void RenderingDevice::texture_set_discardable(RID p_texture, bool p_discardable) {
	ERR_RENDER_THREAD_GUARD();

	Texture *texture = texture_owner.get_or_null(p_texture);
	ERR_FAIL_NULL(texture);
	ERR_FAIL_COND(texture->qualification_return);

	texture->is_discardable = p_discardable;

	if (texture->draw_tracker != nullptr) {
		texture->draw_tracker->is_discardable = p_discardable;
	}

	if (texture->shared_fallback != nullptr && texture->shared_fallback->texture_tracker != nullptr) {
		texture->shared_fallback->texture_tracker->is_discardable = p_discardable;
	}
}

bool RenderingDevice::texture_is_discardable(RID p_texture) {
	ERR_RENDER_THREAD_GUARD_V(false);

	Texture *texture = texture_owner.get_or_null(p_texture);
	ERR_FAIL_NULL_V(texture, false);

	return texture->is_discardable;
}

Error RenderingDevice::texture_clear(RID p_texture, const Color &p_color, uint32_t p_base_mipmap, uint32_t p_mipmaps, uint32_t p_base_layer, uint32_t p_layers) {
	if (native_immutable_resource_is_held(p_texture) || native_immutable_resource_is_retiring(p_texture)) {
		return ERR_BUSY;
	}
	ERR_RENDER_THREAD_GUARD_V(ERR_UNAVAILABLE);
	if (_recording_error() != OK) {
		return _recording_error();
	}

	Texture *src_tex = texture_owner.get_or_null(p_texture);
	ERR_FAIL_NULL_V(src_tex, ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(src_tex->qualification_return, ERR_UNAUTHORIZED);

	ERR_FAIL_COND_V_MSG(src_tex->bound, ERR_INVALID_PARAMETER,
			"Source texture can't be cleared while a draw list that uses it as part of a framebuffer is being created. Ensure the draw list is finalized (and that the color/depth texture using it is not set to `RenderingDevice.FINAL_ACTION_CONTINUE`) to clear this texture.");

	ERR_FAIL_COND_V(p_layers == 0, ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(p_mipmaps == 0, ERR_INVALID_PARAMETER);

	ERR_FAIL_COND_V_MSG(!(src_tex->usage_flags & TEXTURE_USAGE_CAN_COPY_TO_BIT), ERR_INVALID_PARAMETER,
			"Source texture requires the `RenderingDevice.TEXTURE_USAGE_CAN_COPY_TO_BIT` to be set to be cleared.");

	ERR_FAIL_COND_V(p_base_mipmap + p_mipmaps > src_tex->mipmaps, ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(p_base_layer + p_layers > src_tex->layers, ERR_INVALID_PARAMETER);

	// Clear the texture if the driver requires it during its first use.
	{
		const Error dependency_error = _texture_check_pending_clear(p_texture, src_tex);
		if (dependency_error != OK) {
			return dependency_error;
		}
	}

	{
		const Error dependency_error = _texture_clear_color(p_texture, src_tex, p_color, p_base_mipmap, p_mipmaps, p_base_layer, p_layers);
		if (dependency_error != OK) {
			return dependency_error;
		}
	}

	return OK;
}

bool RenderingDevice::texture_is_format_supported_for_usage(DataFormat p_format, BitField<RenderingDevice::TextureUsageBits> p_usage) const {
	ERR_FAIL_INDEX_V(p_format, DATA_FORMAT_MAX, false);

	bool cpu_readable = (p_usage & RDD::TEXTURE_USAGE_CPU_READ_BIT);
	BitField<TextureUsageBits> supported = driver->texture_get_usages_supported_by_format(p_format, cpu_readable);
	bool any_unsupported = (((int64_t)supported) | ((int64_t)p_usage)) != ((int64_t)supported);
	return !any_unsupported;
}

/*********************/
/**** FRAMEBUFFER ****/
/*********************/

RDD::RenderPassID RenderingDevice::_render_pass_create(RenderingDeviceDriver *p_driver, const Vector<AttachmentFormat> &p_attachments, const Vector<FramebufferPass> &p_passes, VectorView<RDD::AttachmentLoadOp> p_load_ops, VectorView<RDD::AttachmentStoreOp> p_store_ops, uint32_t p_view_count, VRSMethod p_vrs_method, int32_t p_vrs_attachment, Size2i p_vrs_texel_size, Vector<TextureSamples> *r_samples) {
	// NOTE:
	// Before the refactor to RenderingDevice-RenderingDeviceDriver, there was commented out code to
	// specify dependencies to external subpasses. Since it had been unused for a long timel it wasn't ported
	// to the new architecture.

	LocalVector<int32_t> attachment_last_pass;
	attachment_last_pass.resize(p_attachments.size());

	if (p_view_count > 1) {
		const RDD::MultiviewCapabilities &capabilities = p_driver->get_multiview_capabilities();

		// This only works with multiview!
		ERR_FAIL_COND_V_MSG(!capabilities.is_supported, RDD::RenderPassID(), "Multiview not supported");

		// Make sure we limit this to the number of views we support.
		ERR_FAIL_COND_V_MSG(p_view_count > capabilities.max_view_count, RDD::RenderPassID(), "Hardware does not support requested number of views for Multiview render pass");
	}

	LocalVector<RDD::Attachment> attachments;
	LocalVector<uint32_t> attachment_remap;

	for (int i = 0; i < p_attachments.size(); i++) {
		if (p_attachments[i].usage_flags == AttachmentFormat::UNUSED_ATTACHMENT) {
			attachment_remap.push_back(RDD::AttachmentReference::UNUSED);
			continue;
		}

		ERR_FAIL_INDEX_V(p_attachments[i].format, DATA_FORMAT_MAX, RDD::RenderPassID());
		ERR_FAIL_INDEX_V(p_attachments[i].samples, TEXTURE_SAMPLES_MAX, RDD::RenderPassID());
		ERR_FAIL_COND_V_MSG(!(p_attachments[i].usage_flags & (TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | TEXTURE_USAGE_DEPTH_RESOLVE_ATTACHMENT_BIT | TEXTURE_USAGE_INPUT_ATTACHMENT_BIT | TEXTURE_USAGE_VRS_ATTACHMENT_BIT)),
				RDD::RenderPassID(), "Texture format for index (" + itos(i) + ") requires an attachment (color, depth-stencil, input or VRS) bit set.");

		RDD::Attachment description;
		description.format = p_attachments[i].format;
		description.samples = p_attachments[i].samples;

		// We can setup a framebuffer where we write to our VRS texture to set it up.
		// We make the assumption here that if our texture is actually used as our VRS attachment.
		// It is used as such for each subpass. This is fairly certain seeing the restrictions on subpasses.
		bool is_vrs = (p_attachments[i].usage_flags & TEXTURE_USAGE_VRS_ATTACHMENT_BIT) && i == p_vrs_attachment;
		if (is_vrs) {
			description.load_op = RDD::ATTACHMENT_LOAD_OP_LOAD;
			description.store_op = RDD::ATTACHMENT_STORE_OP_DONT_CARE;
			description.stencil_load_op = RDD::ATTACHMENT_LOAD_OP_DONT_CARE;
			description.stencil_store_op = RDD::ATTACHMENT_STORE_OP_DONT_CARE;
			description.initial_layout = _vrs_layout_from_method(p_vrs_method);
			description.final_layout = _vrs_layout_from_method(p_vrs_method);
		} else {
			if (p_attachments[i].usage_flags & TEXTURE_USAGE_COLOR_ATTACHMENT_BIT) {
				description.load_op = p_load_ops[i];
				description.store_op = p_store_ops[i];
				description.stencil_load_op = RDD::ATTACHMENT_LOAD_OP_DONT_CARE;
				description.stencil_store_op = RDD::ATTACHMENT_STORE_OP_DONT_CARE;
				description.initial_layout = RDD::TEXTURE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				description.final_layout = RDD::TEXTURE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			} else if (p_attachments[i].usage_flags & TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
				description.load_op = p_load_ops[i];
				description.store_op = p_store_ops[i];
				description.stencil_load_op = p_load_ops[i];
				description.stencil_store_op = p_store_ops[i];
				description.initial_layout = RDD::TEXTURE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
				description.final_layout = RDD::TEXTURE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			} else if (p_attachments[i].usage_flags & TEXTURE_USAGE_DEPTH_RESOLVE_ATTACHMENT_BIT) {
				description.load_op = p_load_ops[i];
				description.store_op = p_store_ops[i];
				description.stencil_load_op = p_load_ops[i];
				description.stencil_store_op = p_store_ops[i];
				description.initial_layout = RDD::TEXTURE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
				description.final_layout = RDD::TEXTURE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			} else {
				description.load_op = RDD::ATTACHMENT_LOAD_OP_DONT_CARE;
				description.store_op = RDD::ATTACHMENT_STORE_OP_DONT_CARE;
				description.stencil_load_op = RDD::ATTACHMENT_LOAD_OP_DONT_CARE;
				description.stencil_store_op = RDD::ATTACHMENT_STORE_OP_DONT_CARE;
				description.initial_layout = RDD::TEXTURE_LAYOUT_UNDEFINED;
				description.final_layout = RDD::TEXTURE_LAYOUT_UNDEFINED;
			}
		}

		attachment_last_pass[i] = -1;
		attachment_remap.push_back(attachments.size());
		attachments.push_back(description);
	}

	LocalVector<RDD::Subpass> subpasses;
	subpasses.resize(p_passes.size());
	LocalVector<RDD::SubpassDependency> subpass_dependencies;

	for (int i = 0; i < p_passes.size(); i++) {
		const FramebufferPass *pass = &p_passes[i];
		RDD::Subpass &subpass = subpasses[i];

		TextureSamples texture_samples = TEXTURE_SAMPLES_1;
		bool is_multisample_first = true;

		for (int j = 0; j < pass->color_attachments.size(); j++) {
			int32_t attachment = pass->color_attachments[j];
			RDD::AttachmentReference reference;
			if (attachment == ATTACHMENT_UNUSED) {
				reference.attachment = RDD::AttachmentReference::UNUSED;
				reference.layout = RDD::TEXTURE_LAYOUT_UNDEFINED;
			} else {
				ERR_FAIL_INDEX_V_MSG(attachment, p_attachments.size(), RDD::RenderPassID(), "Invalid framebuffer format attachment(" + itos(attachment) + "), in pass (" + itos(i) + "), color attachment (" + itos(j) + ").");
				ERR_FAIL_COND_V_MSG(!(p_attachments[attachment].usage_flags & TEXTURE_USAGE_COLOR_ATTACHMENT_BIT), RDD::RenderPassID(), "Invalid framebuffer format attachment(" + itos(attachment) + "), in pass (" + itos(i) + "), it's marked as depth, but it's not usable as color attachment.");
				ERR_FAIL_COND_V_MSG(attachment_last_pass[attachment] == i, RDD::RenderPassID(), "Invalid framebuffer format attachment(" + itos(attachment) + "), in pass (" + itos(i) + "), it already was used for something else before in this pass.");

				if (is_multisample_first) {
					texture_samples = p_attachments[attachment].samples;
					is_multisample_first = false;
				} else {
					ERR_FAIL_COND_V_MSG(texture_samples != p_attachments[attachment].samples, RDD::RenderPassID(), "Invalid framebuffer format attachment(" + itos(attachment) + "), in pass (" + itos(i) + "), if an attachment is marked as multisample, all of them should be multisample and use the same number of samples.");
				}
				reference.attachment = attachment_remap[attachment];
				reference.layout = RDD::TEXTURE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				attachment_last_pass[attachment] = i;
			}
			reference.aspect = RDD::TEXTURE_ASPECT_COLOR_BIT;
			subpass.color_references.push_back(reference);
		}

		for (int j = 0; j < pass->input_attachments.size(); j++) {
			int32_t attachment = pass->input_attachments[j];
			RDD::AttachmentReference reference;
			if (attachment == ATTACHMENT_UNUSED) {
				reference.attachment = RDD::AttachmentReference::UNUSED;
				reference.layout = RDD::TEXTURE_LAYOUT_UNDEFINED;
			} else {
				ERR_FAIL_INDEX_V_MSG(attachment, p_attachments.size(), RDD::RenderPassID(), "Invalid framebuffer format attachment(" + itos(attachment) + "), in pass (" + itos(i) + "), input attachment (" + itos(j) + ").");
				ERR_FAIL_COND_V_MSG(!(p_attachments[attachment].usage_flags & TEXTURE_USAGE_INPUT_ATTACHMENT_BIT), RDD::RenderPassID(), "Invalid framebuffer format attachment(" + itos(attachment) + "), in pass (" + itos(i) + "), it isn't marked as an input texture.");
				ERR_FAIL_COND_V_MSG(attachment_last_pass[attachment] == i, RDD::RenderPassID(), "Invalid framebuffer format attachment(" + itos(attachment) + "), in pass (" + itos(i) + "), it already was used for something else before in this pass.");
				reference.attachment = attachment_remap[attachment];
				reference.layout = RDD::TEXTURE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				attachment_last_pass[attachment] = i;
			}
			reference.aspect = RDD::TEXTURE_ASPECT_COLOR_BIT;
			subpass.input_references.push_back(reference);
		}

		if (pass->resolve_attachments.size() > 0) {
			ERR_FAIL_COND_V_MSG(pass->resolve_attachments.size() != pass->color_attachments.size(), RDD::RenderPassID(), "The amount of resolve attachments (" + itos(pass->resolve_attachments.size()) + ") must match the number of color attachments (" + itos(pass->color_attachments.size()) + ").");
			ERR_FAIL_COND_V_MSG(texture_samples == TEXTURE_SAMPLES_1, RDD::RenderPassID(), "Resolve attachments specified, but color attachments are not multisample.");
		}
		for (int j = 0; j < pass->resolve_attachments.size(); j++) {
			int32_t attachment = pass->resolve_attachments[j];
			attachments[attachment].load_op = RDD::ATTACHMENT_LOAD_OP_DONT_CARE;

			RDD::AttachmentReference reference;
			if (attachment == ATTACHMENT_UNUSED) {
				reference.attachment = RDD::AttachmentReference::UNUSED;
				reference.layout = RDD::TEXTURE_LAYOUT_UNDEFINED;
			} else {
				ERR_FAIL_INDEX_V_MSG(attachment, p_attachments.size(), RDD::RenderPassID(), "Invalid framebuffer format attachment(" + itos(attachment) + "), in pass (" + itos(i) + "), resolve attachment (" + itos(j) + ").");
				ERR_FAIL_COND_V_MSG(pass->color_attachments[j] == ATTACHMENT_UNUSED, RDD::RenderPassID(), "Invalid framebuffer format attachment(" + itos(attachment) + "), in pass (" + itos(i) + "), resolve attachment (" + itos(j) + "), the respective color attachment is marked as unused.");
				ERR_FAIL_COND_V_MSG(!(p_attachments[attachment].usage_flags & TEXTURE_USAGE_COLOR_ATTACHMENT_BIT), RDD::RenderPassID(), "Invalid framebuffer format attachment(" + itos(attachment) + "), in pass (" + itos(i) + "), resolve attachment, it isn't marked as a color texture.");
				ERR_FAIL_COND_V_MSG(attachment_last_pass[attachment] == i, RDD::RenderPassID(), "Invalid framebuffer format attachment(" + itos(attachment) + "), in pass (" + itos(i) + "), it already was used for something else before in this pass.");
				bool multisample = p_attachments[attachment].samples > TEXTURE_SAMPLES_1;
				ERR_FAIL_COND_V_MSG(multisample, RDD::RenderPassID(), "Invalid framebuffer format attachment(" + itos(attachment) + "), in pass (" + itos(i) + "), resolve attachments can't be multisample.");
				reference.attachment = attachment_remap[attachment];
				reference.layout = RDD::TEXTURE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // RDD::TEXTURE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
				attachment_last_pass[attachment] = i;
			}
			reference.aspect = RDD::TEXTURE_ASPECT_COLOR_BIT;
			subpass.resolve_references.push_back(reference);
		}

		if (pass->depth_attachment != ATTACHMENT_UNUSED) {
			int32_t attachment = pass->depth_attachment;
			ERR_FAIL_INDEX_V_MSG(attachment, p_attachments.size(), RDD::RenderPassID(), "Invalid framebuffer depth format attachment(" + itos(attachment) + "), in pass (" + itos(i) + "), depth attachment.");
			ERR_FAIL_COND_V_MSG(!(p_attachments[attachment].usage_flags & TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT), RDD::RenderPassID(), "Invalid framebuffer depth format attachment(" + itos(attachment) + "), in pass (" + itos(i) + "), it's marked as depth, but it's not a depth attachment.");
			ERR_FAIL_COND_V_MSG(attachment_last_pass[attachment] == i, RDD::RenderPassID(), "Invalid framebuffer depth format attachment(" + itos(attachment) + "), in pass (" + itos(i) + "), it already was used for something else before in this pass.");
			subpass.depth_stencil_reference.attachment = attachment_remap[attachment];
			subpass.depth_stencil_reference.layout = RDD::TEXTURE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			attachment_last_pass[attachment] = i;

			if (is_multisample_first) {
				texture_samples = p_attachments[attachment].samples;
				is_multisample_first = false;
			} else {
				ERR_FAIL_COND_V_MSG(texture_samples != p_attachments[attachment].samples, RDD::RenderPassID(), "Invalid framebuffer depth format attachment(" + itos(attachment) + "), in pass (" + itos(i) + "), if an attachment is marked as multisample, all of them should be multisample and use the same number of samples including the depth.");
			}

			if (pass->depth_resolve_attachment != ATTACHMENT_UNUSED) {
				attachment = pass->depth_resolve_attachment;

				// As our fallbacks are handled outside of our pass, we should never be setting up a render pass with a depth resolve attachment when not supported.
				ERR_FAIL_COND_V_MSG(!p_driver->has_feature(SUPPORTS_FRAMEBUFFER_DEPTH_RESOLVE), RDD::RenderPassID(), "Invalid framebuffer depth format attachment(" + itos(attachment) + "), in pass (" + itos(i) + "), a depth resolve attachment was supplied when driver doesn't support this feature.");

				ERR_FAIL_INDEX_V_MSG(attachment, p_attachments.size(), RDD::RenderPassID(), "Invalid framebuffer depth resolve format attachment(" + itos(attachment) + "), in pass (" + itos(i) + "), depth resolve attachment.");
				ERR_FAIL_COND_V_MSG(!(p_attachments[attachment].usage_flags & TEXTURE_USAGE_DEPTH_RESOLVE_ATTACHMENT_BIT), RDD::RenderPassID(), "Invalid framebuffer depth resolve format attachment(" + itos(attachment) + "), in pass (" + itos(i) + "), it's marked as depth, but it's not a depth resolve attachment.");
				ERR_FAIL_COND_V_MSG(attachment_last_pass[attachment] == i, RDD::RenderPassID(), "Invalid framebuffer depth resolve format attachment(" + itos(attachment) + "), in pass (" + itos(i) + "), it already was used for something else before in this pass.");

				subpass.depth_resolve_reference.attachment = attachment_remap[attachment];
				subpass.depth_resolve_reference.layout = RDD::TEXTURE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
				attachment_last_pass[attachment] = i;
			}

		} else {
			subpass.depth_stencil_reference.attachment = RDD::AttachmentReference::UNUSED;
			subpass.depth_stencil_reference.layout = RDD::TEXTURE_LAYOUT_UNDEFINED;
		}

		if (p_vrs_method == VRS_METHOD_FRAGMENT_SHADING_RATE && p_vrs_attachment >= 0) {
			int32_t attachment = p_vrs_attachment;
			ERR_FAIL_INDEX_V_MSG(attachment, p_attachments.size(), RDD::RenderPassID(), "Invalid framebuffer VRS format attachment(" + itos(attachment) + "), in pass (" + itos(i) + "), VRS attachment.");
			ERR_FAIL_COND_V_MSG(!(p_attachments[attachment].usage_flags & TEXTURE_USAGE_VRS_ATTACHMENT_BIT), RDD::RenderPassID(), "Invalid framebuffer VRS format attachment(" + itos(attachment) + "), in pass (" + itos(i) + "), it's marked as VRS, but it's not a VRS attachment.");
			ERR_FAIL_COND_V_MSG(attachment_last_pass[attachment] == i, RDD::RenderPassID(), "Invalid framebuffer VRS attachment(" + itos(attachment) + "), in pass (" + itos(i) + "), it already was used for something else before in this pass.");

			subpass.fragment_shading_rate_reference.attachment = attachment_remap[attachment];
			subpass.fragment_shading_rate_reference.layout = RDD::TEXTURE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL;
			subpass.fragment_shading_rate_texel_size = p_vrs_texel_size;

			attachment_last_pass[attachment] = i;
		}

		for (int j = 0; j < pass->preserve_attachments.size(); j++) {
			int32_t attachment = pass->preserve_attachments[j];

			ERR_FAIL_COND_V_MSG(attachment == ATTACHMENT_UNUSED, RDD::RenderPassID(), "Invalid framebuffer format attachment(" + itos(attachment) + "), in pass (" + itos(i) + "), preserve attachment (" + itos(j) + "). Preserve attachments can't be unused.");

			ERR_FAIL_INDEX_V_MSG(attachment, p_attachments.size(), RDD::RenderPassID(), "Invalid framebuffer format attachment(" + itos(attachment) + "), in pass (" + itos(i) + "), preserve attachment (" + itos(j) + ").");

			if (attachment_last_pass[attachment] != i) {
				// Preserve can still be used to keep depth or color from being discarded after use.
				attachment_last_pass[attachment] = i;
				subpasses[i].preserve_attachments.push_back(attachment);
			}
		}

		if (r_samples) {
			r_samples->push_back(texture_samples);
		}

		if (i > 0) {
			RDD::SubpassDependency dependency;
			dependency.src_subpass = i - 1;
			dependency.dst_subpass = i;
			dependency.src_stages = (RDD::PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | RDD::PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | RDD::PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
			dependency.dst_stages = (RDD::PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | RDD::PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | RDD::PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | RDD::PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
			dependency.src_access = (RDD::BARRIER_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | RDD::BARRIER_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
			dependency.dst_access = (RDD::BARRIER_ACCESS_COLOR_ATTACHMENT_READ_BIT | RDD::BARRIER_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | RDD::BARRIER_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | RDD::BARRIER_ACCESS_INPUT_ATTACHMENT_READ_BIT);
			subpass_dependencies.push_back(dependency);
		}
	}

	RDD::AttachmentReference fragment_density_map_attachment_reference;
	if (p_vrs_method == VRS_METHOD_FRAGMENT_DENSITY_MAP && p_vrs_attachment >= 0) {
		fragment_density_map_attachment_reference.attachment = p_vrs_attachment;
		fragment_density_map_attachment_reference.layout = RDD::TEXTURE_LAYOUT_FRAGMENT_DENSITY_MAP_ATTACHMENT_OPTIMAL;
	}

	RDD::RenderPassID render_pass = p_driver->render_pass_create(attachments, subpasses, subpass_dependencies, p_view_count, fragment_density_map_attachment_reference);
	ERR_FAIL_COND_V(!render_pass, RDD::RenderPassID());

	return render_pass;
}

RDD::RenderPassID RenderingDevice::_render_pass_create_from_graph(RenderingDeviceDriver *p_driver, VectorView<RDD::AttachmentLoadOp> p_load_ops, VectorView<RDD::AttachmentStoreOp> p_store_ops, void *p_user_data) {
	DEV_ASSERT(p_driver != nullptr);
	DEV_ASSERT(p_user_data != nullptr);

	// The graph delegates the creation of the render pass to the user according to the load and store ops that were determined as necessary after
	// resolving the dependencies between commands. This function creates a render pass for the framebuffer accordingly.
	const FramebufferFormatKey *key = (const FramebufferFormatKey *)(p_user_data);
	return _render_pass_create(p_driver, key->attachments, key->passes, p_load_ops, p_store_ops, key->view_count, key->vrs_method, key->vrs_attachment, key->vrs_texel_size);
}

RDG::ResourceUsage RenderingDevice::_vrs_usage_from_method(VRSMethod p_method) {
	switch (p_method) {
		case VRS_METHOD_FRAGMENT_SHADING_RATE:
			return RDG::RESOURCE_USAGE_ATTACHMENT_FRAGMENT_SHADING_RATE_READ;
		case VRS_METHOD_FRAGMENT_DENSITY_MAP:
			return RDG::RESOURCE_USAGE_ATTACHMENT_FRAGMENT_DENSITY_MAP_READ;
		default:
			return RDG::RESOURCE_USAGE_NONE;
	}
}

RDD::PipelineStageBits RenderingDevice::_vrs_stages_from_method(VRSMethod p_method) {
	switch (p_method) {
		case VRS_METHOD_FRAGMENT_SHADING_RATE:
			return RDD::PIPELINE_STAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT;
		case VRS_METHOD_FRAGMENT_DENSITY_MAP:
			return RDD::PIPELINE_STAGE_FRAGMENT_DENSITY_PROCESS_BIT;
		default:
			return RDD::PipelineStageBits(0);
	}
}

RDD::TextureLayout RenderingDevice::_vrs_layout_from_method(VRSMethod p_method) {
	switch (p_method) {
		case VRS_METHOD_FRAGMENT_SHADING_RATE:
			return RDD::TEXTURE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL;
		case VRS_METHOD_FRAGMENT_DENSITY_MAP:
			return RDD::TEXTURE_LAYOUT_FRAGMENT_DENSITY_MAP_ATTACHMENT_OPTIMAL;
		default:
			return RDD::TEXTURE_LAYOUT_UNDEFINED;
	}
}

void RenderingDevice::_vrs_detect_method() {
	const RDD::FragmentShadingRateCapabilities &fsr_capabilities = driver->get_fragment_shading_rate_capabilities();
	const RDD::FragmentDensityMapCapabilities &fdm_capabilities = driver->get_fragment_density_map_capabilities();
	if (fsr_capabilities.attachment_supported) {
		vrs_method = VRS_METHOD_FRAGMENT_SHADING_RATE;
	} else if (fdm_capabilities.attachment_supported) {
		vrs_method = VRS_METHOD_FRAGMENT_DENSITY_MAP;
	}

	switch (vrs_method) {
		case VRS_METHOD_FRAGMENT_SHADING_RATE:
			vrs_format = DATA_FORMAT_R8_UINT;
			vrs_texel_size = Vector2i(16, 16).clamp(fsr_capabilities.min_texel_size, fsr_capabilities.max_texel_size);
			break;
		case VRS_METHOD_FRAGMENT_DENSITY_MAP:
			vrs_format = DATA_FORMAT_R8G8_UNORM;
			vrs_texel_size = Vector2i(32, 32).clamp(fdm_capabilities.min_texel_size, fdm_capabilities.max_texel_size);
			break;
		default:
			break;
	}
}

RD::VRSMethod RenderingDevice::vrs_get_method() const {
	return vrs_method;
}

RD::DataFormat RenderingDevice::vrs_get_format() const {
	return vrs_format;
}

Size2i RenderingDevice::vrs_get_texel_size() const {
	return vrs_texel_size;
}

RenderingDevice::FramebufferFormatID RenderingDevice::framebuffer_format_create(const Vector<AttachmentFormat> &p_format, uint32_t p_view_count, int32_t p_fragment_density_map_attachment) {
	FramebufferPass pass;
	for (int i = 0; i < p_format.size(); i++) {
		if (p_format[i].usage_flags & TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
			pass.depth_attachment = i;
		} else if (p_format[i].usage_flags & TEXTURE_USAGE_DEPTH_RESOLVE_ATTACHMENT_BIT) {
			pass.depth_resolve_attachment = i;
		} else {
			pass.color_attachments.push_back(i);
		}
	}

	Vector<FramebufferPass> passes;
	passes.push_back(pass);
	return framebuffer_format_create_multipass(p_format, passes, p_view_count, p_fragment_density_map_attachment);
}

RenderingDevice::FramebufferFormatID RenderingDevice::framebuffer_format_create_multipass(const Vector<AttachmentFormat> &p_attachments, const Vector<FramebufferPass> &p_passes, uint32_t p_view_count, int32_t p_vrs_attachment) {
	_THREAD_SAFE_METHOD_

	FramebufferFormatKey key;
	key.attachments = p_attachments;
	key.passes = p_passes;
	key.view_count = p_view_count;
	key.vrs_method = vrs_method;
	key.vrs_attachment = p_vrs_attachment;
	key.vrs_texel_size = vrs_texel_size;

	const RBMap<FramebufferFormatKey, FramebufferFormatID>::Element *E = framebuffer_format_cache.find(key);
	if (E) {
		// Exists, return.
		return E->get();
	}

	Vector<TextureSamples> samples;
	LocalVector<RDD::AttachmentLoadOp> load_ops;
	LocalVector<RDD::AttachmentStoreOp> store_ops;
	for (int64_t i = 0; i < p_attachments.size(); i++) {
		load_ops.push_back(RDD::ATTACHMENT_LOAD_OP_CLEAR);
		store_ops.push_back(RDD::ATTACHMENT_STORE_OP_STORE);
	}

	RDD::RenderPassID render_pass = _render_pass_create(driver, p_attachments, p_passes, load_ops, store_ops, p_view_count, vrs_method, p_vrs_attachment, vrs_texel_size, &samples); // Actions don't matter for this use case.
	if (!render_pass) { // Was likely invalid.
		return INVALID_ID;
	}

	FramebufferFormatID id = FramebufferFormatID(framebuffer_format_cache.size()) | (FramebufferFormatID(ID_TYPE_FRAMEBUFFER_FORMAT) << FramebufferFormatID(ID_BASE_SHIFT));
	E = framebuffer_format_cache.insert(key, id);

	FramebufferFormat fb_format;
	fb_format.E = E;
	fb_format.render_pass = render_pass;
	fb_format.pass_samples = samples;
	fb_format.view_count = p_view_count;
	framebuffer_formats[id] = fb_format;

#if PRINT_FRAMEBUFFER_FORMAT
	print_line("FRAMEBUFFER FORMAT:", id, "ATTACHMENTS:", p_attachments.size(), "PASSES:", p_passes.size());
	for (RD::AttachmentFormat attachment : p_attachments) {
		print_line("FORMAT:", attachment.format, "SAMPLES:", attachment.samples, "USAGE FLAGS:", attachment.usage_flags);
	}
#endif

	return id;
}

RenderingDevice::FramebufferFormatID RenderingDevice::framebuffer_format_create_empty(TextureSamples p_samples) {
	_THREAD_SAFE_METHOD_

	FramebufferFormatKey key;
	key.passes.push_back(FramebufferPass());

	const RBMap<FramebufferFormatKey, FramebufferFormatID>::Element *E = framebuffer_format_cache.find(key);
	if (E) {
		// Exists, return.
		return E->get();
	}

	LocalVector<RDD::Subpass> subpass;
	subpass.resize(1);

	RDD::RenderPassID render_pass = driver->render_pass_create({}, subpass, {}, 1, RDD::AttachmentReference());
	ERR_FAIL_COND_V(!render_pass, FramebufferFormatID());

	FramebufferFormatID id = FramebufferFormatID(framebuffer_format_cache.size()) | (FramebufferFormatID(ID_TYPE_FRAMEBUFFER_FORMAT) << FramebufferFormatID(ID_BASE_SHIFT));

	E = framebuffer_format_cache.insert(key, id);

	FramebufferFormat fb_format;
	fb_format.E = E;
	fb_format.render_pass = render_pass;
	fb_format.pass_samples.push_back(p_samples);
	framebuffer_formats[id] = fb_format;

#if PRINT_FRAMEBUFFER_FORMAT
	print_line("FRAMEBUFFER FORMAT:", id, "ATTACHMENTS: EMPTY");
#endif

	return id;
}

RenderingDevice::TextureSamples RenderingDevice::framebuffer_format_get_texture_samples(FramebufferFormatID p_format, uint32_t p_pass) {
	_THREAD_SAFE_METHOD_

	HashMap<FramebufferFormatID, FramebufferFormat>::Iterator E = framebuffer_formats.find(p_format);
	ERR_FAIL_COND_V(!E, TEXTURE_SAMPLES_1);
	ERR_FAIL_COND_V(p_pass >= uint32_t(E->value.pass_samples.size()), TEXTURE_SAMPLES_1);

	return E->value.pass_samples[p_pass];
}

RID RenderingDevice::framebuffer_create_empty(const Size2i &p_size, TextureSamples p_samples, FramebufferFormatID p_format_check) {
	_THREAD_SAFE_METHOD_

	Framebuffer framebuffer;
	framebuffer.format_id = framebuffer_format_create_empty(p_samples);
	ERR_FAIL_COND_V(p_format_check != INVALID_FORMAT_ID && framebuffer.format_id != p_format_check, RID());
	framebuffer.size = p_size;
	framebuffer.view_count = 1;

	RDG::FramebufferCache *framebuffer_cache = RDG::framebuffer_cache_create();
	framebuffer_cache->width = p_size.width;
	framebuffer_cache->height = p_size.height;
	framebuffer.framebuffer_cache = framebuffer_cache;

	RID id = framebuffer_owner.make_rid(framebuffer);
#ifdef DEV_ENABLED
	set_resource_name(id, "RID:" + itos(id.get_id()));
#endif

	// This relies on the fact that HashMap will not change the address of an object after it's been inserted into the container.
	framebuffer_cache->render_pass_creation_user_data = (void *)(&framebuffer_formats[framebuffer.format_id].E->key());

	return id;
}

RID RenderingDevice::framebuffer_create(const Vector<RID> &p_texture_attachments, FramebufferFormatID p_format_check, uint32_t p_view_count) {
	_THREAD_SAFE_METHOD_

	// Preflight the entire attachment list before touching any prior worker.
	for (RID attachment : p_texture_attachments) {
		const Texture *texture = texture_owner.get_or_null(attachment);
		ERR_FAIL_COND_V(texture && texture->qualification_return, RID());
	}

	FramebufferPass pass;

	for (int i = 0; i < p_texture_attachments.size(); i++) {
		Texture *texture = texture_owner.get_or_null(p_texture_attachments[i]);

		ERR_FAIL_COND_V_MSG(texture && texture->layers != p_view_count, RID(), "Layers of our texture doesn't match view count for this framebuffer");

		if (texture != nullptr) {
			{
				const Error dependency_error = _check_transfer_worker_texture(texture);
				if (dependency_error != OK) {
					return RID();
				}
			}
		}

		if (texture && texture->usage_flags & TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
			pass.depth_attachment = i;
		} else if (texture && texture->usage_flags & TEXTURE_USAGE_DEPTH_RESOLVE_ATTACHMENT_BIT) {
			pass.depth_resolve_attachment = i;
		} else if (texture && texture->usage_flags & TEXTURE_USAGE_VRS_ATTACHMENT_BIT) {
			// Prevent the VRS attachment from being added to the color_attachments.
		} else {
			if (texture && texture->is_resolve_buffer) {
				pass.resolve_attachments.push_back(i);
			} else {
				pass.color_attachments.push_back(texture ? i : ATTACHMENT_UNUSED);
			}
		}
	}

	Vector<FramebufferPass> passes;
	passes.push_back(pass);

	return framebuffer_create_multipass(p_texture_attachments, passes, p_format_check, p_view_count);
}

RID RenderingDevice::framebuffer_create_multipass(const Vector<RID> &p_texture_attachments, const Vector<FramebufferPass> &p_passes, FramebufferFormatID p_format_check, uint32_t p_view_count) {
	return _qualification_framebuffer_create_multipass(p_texture_attachments, p_passes, p_format_check, p_view_count, nullptr, nullptr, RID());
}

RID RenderingDevice::_qualification_framebuffer_create(RID p_target, GDExtensionDeviceExit::QualificationPhysicalReturn *p_cell, GDExtensionDeviceExit::QualificationPhysicalReturn *p_target_cell, RID p_reserved) {
	ERR_FAIL_NULL_V(p_cell, RID());
	ERR_FAIL_NULL_V(p_target_cell, RID());
	Vector<RID> attachments;
	attachments.push_back(p_target);
	FramebufferPass pass;
	pass.depth_attachment = 0;
	Vector<FramebufferPass> passes;
	passes.push_back(pass);
	return _qualification_framebuffer_create_multipass(attachments, passes, INVALID_ID, 1, p_cell, p_target_cell, p_reserved);
}

RID RenderingDevice::_qualification_framebuffer_create_multipass(const Vector<RID> &p_texture_attachments, const Vector<FramebufferPass> &p_passes, FramebufferFormatID p_format_check, uint32_t p_view_count, GDExtensionDeviceExit::QualificationPhysicalReturn *p_cell, GDExtensionDeviceExit::QualificationPhysicalReturn *p_target_cell, RID p_reserved, const NativeDisposalWitness *p_native_disposal) {
	_THREAD_SAFE_METHOD_
	ERR_FAIL_COND_V(p_native_disposal && (p_cell || p_target_cell || p_reserved.is_null()), RID());

	// Authenticate the original target before worker waits or texture mutation.
	if (p_cell) {
		ERR_FAIL_COND_V(!p_reserved.is_valid() || !p_target_cell || p_texture_attachments.size() != 1 || p_passes.size() != 1 || p_view_count != 1, RID());
		const Texture *target = texture_owner.get_or_null(p_texture_attachments[0]);
		ERR_FAIL_COND_V(!target || target->qualification_return != p_target_cell || p_target_cell->bridge != p_cell->bridge || p_target_cell->rid != p_texture_attachments[0].get_id() || p_target_cell->use_closed.load() || p_target_cell->dependent_children.load() == UINT32_MAX, RID());
		ERR_FAIL_COND_V(target->format != DATA_FORMAT_D32_SFLOAT || target->samples != TEXTURE_SAMPLES_1 || target->usage_flags != (TEXTURE_USAGE_SAMPLING_BIT | TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT), RID());
	}
	// Preflight the entire attachment list before touching any prior worker.
	for (RID attachment : p_texture_attachments) {
		const Texture *texture = texture_owner.get_or_null(attachment);
		ERR_FAIL_COND_V(texture && texture->qualification_return && !p_cell, RID());
	}

	Vector<AttachmentFormat> attachments;
	LocalVector<RDD::TextureID> textures;
	LocalVector<RDG::ResourceTracker *> trackers;
	int32_t vrs_attachment = -1;
	attachments.resize(p_texture_attachments.size());
	Size2i size;
	bool size_set = false;
	for (int i = 0; i < p_texture_attachments.size(); i++) {
		AttachmentFormat af;
		Texture *texture = texture_owner.get_or_null(p_texture_attachments[i]);
		if (!texture) {
			af.usage_flags = AttachmentFormat::UNUSED_ATTACHMENT;
			trackers.push_back(nullptr);
		} else {
			ERR_FAIL_COND_V_MSG(texture->layers != p_view_count, RID(), "Layers of our texture doesn't match view count for this framebuffer");

			{
				const Error dependency_error = _check_transfer_worker_texture(texture);
				if (dependency_error != OK) {
					return RID();
				}
			}

			if (i != 0 && texture->usage_flags & TEXTURE_USAGE_VRS_ATTACHMENT_BIT) {
				// Detect if the texture is the fragment density map and it's not the first attachment.
				vrs_attachment = i;
			}

			if (!size_set) {
				size.width = texture->width;
				size.height = texture->height;
				size_set = true;
			} else if (texture->usage_flags & TEXTURE_USAGE_VRS_ATTACHMENT_BIT) {
				// If this is not the first attachment we assume this is used as the VRS attachment.
				// In this case this texture will be 1/16th the size of the color attachment.
				// So we skip the size check.
			} else {
				ERR_FAIL_COND_V_MSG((uint32_t)size.width != texture->width || (uint32_t)size.height != texture->height, RID(),
						"All textures in a framebuffer should be the same size.");
			}

			af.format = texture->format;
			af.samples = texture->samples;
			af.usage_flags = texture->usage_flags;

			_texture_make_mutable(texture, p_texture_attachments[i]);

			textures.push_back(texture->driver_id);
			trackers.push_back(texture->draw_tracker);
		}
		attachments.write[i] = af;
	}

	ERR_FAIL_COND_V_MSG(!size_set, RID(), "All attachments unused.");

	FramebufferFormatID format_id = framebuffer_format_create_multipass(attachments, p_passes, p_view_count, vrs_attachment);
	if (format_id == INVALID_ID) {
		return RID();
	}

	ERR_FAIL_COND_V_MSG(p_format_check != INVALID_ID && format_id != p_format_check, RID(),
			"The format used to check this framebuffer differs from the intended framebuffer format.");

	Framebuffer framebuffer;
	framebuffer.qualification_return = p_cell;
	framebuffer.qualification_dependency = p_target_cell;
	framebuffer.format_id = format_id;
	framebuffer.texture_ids = p_texture_attachments;
	framebuffer.size = size;
	framebuffer.view_count = p_view_count;

	RDG::FramebufferCache *framebuffer_cache = RDG::framebuffer_cache_create();
	framebuffer_cache->width = size.width;
	framebuffer_cache->height = size.height;
	framebuffer_cache->textures = textures;
	framebuffer_cache->trackers = trackers;
	framebuffer.framebuffer_cache = framebuffer_cache;

	RID id;
	if (p_cell || p_native_disposal) {
		id = p_reserved;
		if (p_native_disposal) {
			framebuffer.native_disposal = *p_native_disposal;
			framebuffer.native_disposal_rid = id.get_id();
			framebuffer.native_disposal_serial = p_native_disposal->cell->serial;
			framebuffer.native_disposal_backing = uint64_t(uintptr_t(framebuffer_cache));
			p_native_disposal->cell->rid = id.get_id();
			p_native_disposal->cell->backing = framebuffer.native_disposal_backing;
		}
		framebuffer_owner.initialize_rid(id, framebuffer);
		if (p_target_cell) {
			p_target_cell->dependent_children.fetch_add(1);
		}
	} else {
		id = framebuffer_owner.make_rid(framebuffer);
	}
#ifdef DEV_ENABLED
	set_resource_name(id, "RID:" + itos(id.get_id()));
#endif

	for (int i = 0; i < p_texture_attachments.size(); i++) {
		if (p_texture_attachments[i].is_valid()) {
			_add_dependency(id, p_texture_attachments[i]);
		}
	}

	// This relies on the fact that HashMap will not change the address of an object after it's been inserted into the container.
	framebuffer_cache->render_pass_creation_user_data = (void *)(&framebuffer_formats[framebuffer.format_id].E->key());

	return id;
}

RenderingDevice::FramebufferFormatID RenderingDevice::framebuffer_get_format(RID p_framebuffer) {
	_THREAD_SAFE_METHOD_

	Framebuffer *framebuffer = framebuffer_owner.get_or_null(p_framebuffer);
	ERR_FAIL_NULL_V(framebuffer, INVALID_ID);

	return framebuffer->format_id;
}

Size2 RenderingDevice::framebuffer_get_size(RID p_framebuffer) {
	_THREAD_SAFE_METHOD_

	Framebuffer *framebuffer = framebuffer_owner.get_or_null(p_framebuffer);
	ERR_FAIL_NULL_V(framebuffer, Size2(0, 0));

	return framebuffer->size;
}

bool RenderingDevice::framebuffer_is_valid(RID p_framebuffer) const {
	_THREAD_SAFE_METHOD_

	return framebuffer_owner.owns(p_framebuffer);
}

void RenderingDevice::framebuffer_set_invalidation_callback(RID p_framebuffer, InvalidationCallback p_callback, void *p_userdata) {
	_THREAD_SAFE_METHOD_

	Framebuffer *framebuffer = framebuffer_owner.get_or_null(p_framebuffer);
	ERR_FAIL_NULL(framebuffer);
	ERR_FAIL_COND(framebuffer->qualification_return);

	framebuffer->invalidated_callback = p_callback;
	framebuffer->invalidated_callback_userdata = p_userdata;
}

/*****************/
/**** SAMPLER ****/
/*****************/

RID RenderingDevice::sampler_create(const SamplerState &p_state) {
	return _sampler_create_internal(p_state, nullptr);
}

RID RenderingDevice::_qualification_sampler_create(const SamplerState &p_state, GDExtensionDeviceExit::QualificationPhysicalReturn *p_cell) {
	ERR_FAIL_COND_V(!p_cell || !p_cell->bridge || !p_cell->child || !p_cell->serial || p_cell->rid || p_cell->use_closed || p_cell->enqueued || p_cell->free_event, RID());
	return _sampler_create_internal(p_state, p_cell);
}

RID RenderingDevice::_sampler_create_internal(const SamplerState &p_state, GDExtensionDeviceExit::QualificationPhysicalReturn *p_cell, const NativeDisposalWitness *p_native_disposal) {
	_THREAD_SAFE_METHOD_
	ERR_FAIL_COND_V(p_cell && p_native_disposal, RID());
	ERR_FAIL_COND_V((p_cell || p_native_disposal) && _recording_error() != OK, RID());

	ERR_FAIL_INDEX_V(p_state.repeat_u, SAMPLER_REPEAT_MODE_MAX, RID());
	ERR_FAIL_INDEX_V(p_state.repeat_v, SAMPLER_REPEAT_MODE_MAX, RID());
	ERR_FAIL_INDEX_V(p_state.repeat_w, SAMPLER_REPEAT_MODE_MAX, RID());
	ERR_FAIL_INDEX_V(p_state.compare_op, COMPARE_OP_MAX, RID());
	ERR_FAIL_INDEX_V(p_state.border_color, SAMPLER_BORDER_COLOR_MAX, RID());

	// Validation above cannot leak a pending RID: this shared body owns the
	// reservation and rollback for both retained creation paths.
	RID reserved;
	if (p_cell || p_native_disposal) {
		ERR_FAIL_COND_V(p_cell && !_qualification_reserve_free_event(p_cell), RID());
		reserved = sampler_owner.allocate_rid();
		if (reserved.is_null()) {
			if (p_cell) {
				_qualification_release_free_event(p_cell);
			}
			return RID();
		}
	}
	Sampler sampler;
	sampler.driver_id = driver->sampler_create(p_state);
	if (!sampler.driver_id) {
		if (reserved.is_valid()) {
			sampler_owner.initialize_rid(reserved, Sampler());
			sampler_owner.free(reserved);
			if (p_cell) {
				_qualification_release_free_event(p_cell);
			}
		}
		return RID();
	}

	RID id = reserved;
	if (p_cell) {
		sampler.qualification_return = p_cell;
		p_cell->rid = id.get_id();
	}
	if (p_native_disposal) {
		sampler.native_disposal = *p_native_disposal;
		sampler.native_disposal_rid = id.get_id();
		sampler.native_disposal_serial = p_native_disposal->cell->serial;
		p_native_disposal->cell->rid = id.get_id();
		p_native_disposal->cell->backing = sampler.driver_id.id;
	}
	if (id.is_valid()) {
		sampler_owner.initialize_rid(id, sampler);
	} else {
		id = sampler_owner.make_rid(sampler);
	}
#ifdef DEV_ENABLED
	set_resource_name(id, "RID:" + itos(id.get_id()));
#endif
	return id;
}

bool RenderingDevice::sampler_is_format_supported_for_filter(DataFormat p_format, SamplerFilter p_sampler_filter) const {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_INDEX_V(p_format, DATA_FORMAT_MAX, false);

	return driver->sampler_is_format_supported_for_filter(p_format, p_sampler_filter);
}

/***********************/
/**** VERTEX BUFFER ****/
/***********************/

RID RenderingDevice::vertex_buffer_create(uint32_t p_size_bytes, Span<uint8_t> p_data, BitField<BufferCreationBits> p_creation_bits) {
	ERR_FAIL_COND_V_MSG(_recording_error() != OK, RID(), "Resource recording is suspended during frame fence recovery or readback callbacks.");
	ERR_FAIL_COND_V(p_data.size() && (uint32_t)p_data.size() != p_size_bytes, RID());

	Buffer buffer;
	buffer.size = p_size_bytes;
	buffer.usage = RDD::BUFFER_USAGE_TRANSFER_FROM_BIT | RDD::BUFFER_USAGE_TRANSFER_TO_BIT | RDD::BUFFER_USAGE_VERTEX_BIT;
	if (p_creation_bits.has_flag(BUFFER_CREATION_AS_STORAGE_BIT)) {
		buffer.usage.set_flag(RDD::BUFFER_USAGE_STORAGE_BIT);
	}
	if (p_creation_bits.has_flag(BUFFER_CREATION_DYNAMIC_PERSISTENT_BIT)) {
		buffer.usage.set_flag(RDD::BUFFER_USAGE_DYNAMIC_PERSISTENT_BIT);

		// Persistent buffers expect frequent CPU -> GPU writes, so GPU writes should avoid the same path.
		buffer.usage.clear_flag(RDD::BUFFER_USAGE_TRANSFER_TO_BIT);
	}
	if (p_creation_bits.has_flag(BUFFER_CREATION_DEVICE_ADDRESS_BIT)) {
		buffer.usage.set_flag(RDD::BUFFER_USAGE_DEVICE_ADDRESS_BIT);
	}
	if (p_creation_bits.has_flag(BUFFER_CREATION_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT)) {
		buffer.usage.set_flag(RDD::BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT);
	}
	buffer.driver_id = driver->buffer_create(buffer.size, buffer.usage, RDD::MEMORY_ALLOCATION_TYPE_GPU, frames_drawn);
	ERR_FAIL_COND_V(!buffer.driver_id, RID());

	// Vertex buffers are assumed to be immutable unless they don't have initial data or they've been marked for storage explicitly.
	if (p_data.is_empty() || p_creation_bits.has_flag(BUFFER_CREATION_AS_STORAGE_BIT) || p_creation_bits.has_flag(BUFFER_CREATION_DYNAMIC_PERSISTENT_BIT)) {
		buffer.draw_tracker = RDG::resource_tracker_create();
		buffer.draw_tracker->buffer_driver_id = buffer.driver_id;
	}

	if (p_data.size()) {
		Error initialize_error = _buffer_initialize(&buffer, p_data);
		if (initialize_error != OK) {
			RDG::resource_tracker_free(buffer.draw_tracker);
			driver->buffer_free(buffer.driver_id);
			_production_disposal_returned(buffer.production_disposal_uses);
			return RID();
		}
	}

	_THREAD_SAFE_LOCK_
	buffer_memory += buffer.size;
	_THREAD_SAFE_UNLOCK_

#if defined(TESTS_ENABLED) && (defined(MODULE_SKINNED_MESH_CAPTURE_TESTS_ENABLED) || defined(MODULE_PARTICLES_RUNTIME_TESTS_ENABLED))
	RID id = vertex_buffer_owner.allocate_rid();
	buffer.capture_test_allocation_serial = capture_test_allocation_registry.allocate(buffer.driver_id.id, id.get_id());
	if (buffer.capture_test_allocation_serial == 0) {
		capture_test_coverage_failed.store(true);
	}
	vertex_buffer_owner.initialize_rid(id, buffer);
#else
	RID id = vertex_buffer_owner.make_rid(buffer);
#endif
#if defined(TESTS_ENABLED) && defined(MODULE_SKINNED_MESH_CAPTURE_TESTS_ENABLED)
	RenderCaptureTestHooks::observe_buffer_allocation(id.get_id(), uint64_t(get_instance_id()), buffer.driver_id.id, buffer.capture_test_allocation_serial);
#endif
#ifdef DEV_ENABLED
	set_resource_name(id, "RID:" + itos(id.get_id()));
#endif
	return id;
}

// Internally reference counted, this ID is warranted to be unique for the same description, but needs to be freed as many times as it was allocated.
RenderingDevice::VertexFormatID RenderingDevice::vertex_format_create(const Vector<VertexAttribute> &p_vertex_descriptions) {
	_THREAD_SAFE_METHOD_

	VertexDescriptionKey key;
	key.vertex_formats = p_vertex_descriptions;

	VertexFormatID *idptr = vertex_format_cache.getptr(key);
	if (idptr) {
		return *idptr;
	}

	VertexAttributeBindingsMap bindings;
	bool has_implicit = false;
	bool has_explicit = false;
	Vector<VertexAttribute> vertex_descriptions = p_vertex_descriptions;
	HashSet<int> used_locations;
	for (int i = 0; i < vertex_descriptions.size(); i++) {
		VertexAttribute &attr = vertex_descriptions.write[i];
		ERR_CONTINUE(attr.format >= DATA_FORMAT_MAX);
		ERR_FAIL_COND_V(used_locations.has(attr.location), INVALID_ID);

		ERR_FAIL_COND_V_MSG(get_format_vertex_size(attr.format) == 0, INVALID_ID,
				vformat("Data format for attribute (%d), '%s', is not valid for a vertex array.", attr.location, String(FORMAT_NAMES[attr.format])));

		if (attr.binding == UINT32_MAX) {
			attr.binding = i; // Implicitly assigned binding
			has_implicit = true;
		} else {
			has_explicit = true;
		}
		ERR_FAIL_COND_V_MSG(!(has_implicit ^ has_explicit), INVALID_ID, "Vertex attributes must use either all explicit or all implicit bindings.");

		const VertexAttributeBinding *existing = bindings.getptr(attr.binding);
		if (!existing) {
			bindings.insert(attr.binding, VertexAttributeBinding(attr.stride, attr.frequency));
		} else {
			ERR_FAIL_COND_V_MSG(existing->stride != attr.stride, INVALID_ID,
					vformat("Vertex attributes with binding (%d) have an inconsistent stride.", attr.binding));
			ERR_FAIL_COND_V_MSG(existing->frequency != attr.frequency, INVALID_ID,
					vformat("Vertex attributes with binding (%d) have an inconsistent frequency.", attr.binding));
		}

		used_locations.insert(attr.location);
	}

	RDD::VertexFormatID driver_id = driver->vertex_format_create(vertex_descriptions, bindings);
	ERR_FAIL_COND_V(!driver_id, 0);

	VertexFormatID id = (vertex_format_cache.size() | ((int64_t)ID_TYPE_VERTEX_FORMAT << ID_BASE_SHIFT));
	vertex_format_cache[key] = id;
	VertexDescriptionCache &ce = vertex_formats.insert(id, VertexDescriptionCache())->value;
	ce.vertex_formats = vertex_descriptions;
	ce.bindings = std::move(bindings);
	ce.driver_id = driver_id;
	return id;
}

RID RenderingDevice::vertex_array_create(uint32_t p_vertex_count, VertexFormatID p_vertex_format, const Vector<RID> &p_src_buffers, const Vector<uint64_t> &p_offsets) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND_V(!vertex_formats.has(p_vertex_format), RID());
	const VertexDescriptionCache &vd = vertex_formats[p_vertex_format];

	VertexArray vertex_array;

	if (p_offsets.is_empty()) {
		vertex_array.offsets.resize_initialized(p_src_buffers.size());
	} else {
		ERR_FAIL_COND_V(p_offsets.size() != p_src_buffers.size(), RID());
		vertex_array.offsets = p_offsets;
	}

	vertex_array.vertex_count = p_vertex_count;
	vertex_array.description = p_vertex_format;
	vertex_array.max_instances_allowed = 0xFFFFFFFF; // By default as many as you want.
	vertex_array.buffers.resize(p_src_buffers.size());

	HashSet<RID> unique_buffers;
	unique_buffers.reserve(p_src_buffers.size());

	for (const VertexAttribute &atf : vd.vertex_formats) {
		ERR_FAIL_COND_V_MSG(atf.binding >= p_src_buffers.size(), RID(), vformat("Vertex attribute location (%d) is missing a buffer for binding (%d).", atf.location, atf.binding));
		RID buf = p_src_buffers[atf.binding];
		ERR_FAIL_COND_V(native_immutable_resource_is_retiring(buf), RID());
		ERR_FAIL_COND_V(!vertex_buffer_owner.owns(buf), RID());

		Buffer *buffer = vertex_buffer_owner.get_or_null(buf);

		// Validate the complete fetch range, including the binding offset.
		{
			uint32_t element_size = get_format_vertex_size(atf.format);
			ERR_FAIL_COND_V(element_size == 0, RID()); // Should never happen since this was prevalidated.

			const uint64_t binding_offset = vertex_array.offsets[atf.binding];
			ERR_FAIL_COND_V_MSG(binding_offset > buffer->size || atf.offset > buffer->size - binding_offset, RID(),
					vformat("Vertex attribute (%d) will read past the end of the buffer.", atf.location));
			const uint64_t available = buffer->size - binding_offset - atf.offset;
			ERR_FAIL_COND_V_MSG(element_size > available, RID(),
					vformat("Vertex attribute (%d) will read past the end of the buffer.", atf.location));

			if (atf.frequency == VERTEX_FREQUENCY_VERTEX) {
				// The uint32 stride/count product fits uint64; subtract the
				// element first so adding large binding offsets cannot wrap.
				const uint64_t stride_extent = uint64_t(atf.stride) * (p_vertex_count - 1);
				ERR_FAIL_COND_V_MSG(stride_extent > available - element_size, RID(),
						vformat("Vertex attribute (%d) will read past the end of the buffer.", atf.location));
			} else {
				// Every instance needs one complete element. Zero stride repeats
				// that element, including all-instance point-size emulation.
				const uint32_t instances_allowed = atf.stride == 0 ? UINT32_MAX : uint32_t((available - element_size) / atf.stride + 1);
				vertex_array.max_instances_allowed = MIN(instances_allowed, vertex_array.max_instances_allowed);
			}
		}

		vertex_array.buffers.write[atf.binding] = buffer->driver_id;

		if (unique_buffers.has(buf)) {
			// No need to add dependencies multiple times.
			continue;
		}

		unique_buffers.insert(buf);

		if (buffer->draw_tracker != nullptr) {
			vertex_array.draw_trackers.push_back(buffer->draw_tracker);
		} else {
			vertex_array.untracked_buffers.insert(buf);
		}

		if (buffer->transfer_worker_index >= 0) {
			vertex_array.transfer_worker_indices.push_back(buffer->transfer_worker_index);
			vertex_array.transfer_worker_operations.push_back(buffer->transfer_worker_operation);
		}
	}

	RID id = vertex_array_owner.make_rid(vertex_array);
	for (const RID &buf : unique_buffers) {
		_add_dependency(id, buf);
	}

	return id;
}

bool RenderingDevice::validate_external_indexed_surface_buffers(RID p_current_vertex_buffer, RID p_previous_vertex_buffer, RID p_index_buffer, RID p_index_array, uint32_t p_vertex_capacity, uint32_t p_index_capacity) {
	// Indexed144 uses one tightly packed Float144 record (9 vec4 values) per vertex
	// and a separate tightly packed UINT32 index allocation. Keep this check CPU-only;
	// it is an admission guard for MeshStorage and does not authorize public RID use.
	if (p_vertex_capacity == 0 || p_index_capacity == 0) {
		return false;
	}
	const uint64_t vertex_bytes = uint64_t(p_vertex_capacity) * 144u;
	const uint64_t index_bytes = uint64_t(p_index_capacity) * sizeof(uint32_t);
	if (vertex_bytes > UINT32_MAX || index_bytes > UINT32_MAX) {
		return false;
	}

	const Buffer *current = vertex_buffer_owner.get_or_null(p_current_vertex_buffer);
	const Buffer *previous = vertex_buffer_owner.get_or_null(p_previous_vertex_buffer);
	const IndexBuffer *index = index_buffer_owner.get_or_null(p_index_buffer);
	const IndexArray *array = index_array_owner.get_or_null(p_index_array);
	if (current == nullptr || previous == nullptr || index == nullptr || array == nullptr) {
		return false;
	}
	if (p_current_vertex_buffer == p_previous_vertex_buffer || current->size != vertex_bytes || previous->size != vertex_bytes) {
		return false;
	}
	if (!current->usage.has_flag(RDD::BUFFER_USAGE_VERTEX_BIT) || !previous->usage.has_flag(RDD::BUFFER_USAGE_VERTEX_BIT) ||
			!current->usage.has_flag(RDD::BUFFER_USAGE_STORAGE_BIT) || !previous->usage.has_flag(RDD::BUFFER_USAGE_STORAGE_BIT)) {
		return false;
	}
	if (index->format != INDEX_BUFFER_FORMAT_UINT32 || index->index_count != p_index_capacity || index->size != index_bytes ||
			array->format != INDEX_BUFFER_FORMAT_UINT32 || array->offset != 0 || array->indices != p_index_capacity || array->driver_id != index->driver_id) {
		return false;
	}
	return true;
}

RID RenderingDevice::index_buffer_create(uint32_t p_index_count, IndexBufferFormat p_format, Span<uint8_t> p_data, bool p_use_restart_indices, BitField<BufferCreationBits> p_creation_bits) {
	ERR_FAIL_COND_V_MSG(_recording_error() != OK, RID(), "Resource recording is suspended during frame fence recovery or readback callbacks.");
	ERR_FAIL_COND_V(p_index_count == 0, RID());

	IndexBuffer index_buffer;
	index_buffer.format = p_format;
	index_buffer.supports_restart_indices = p_use_restart_indices;
	index_buffer.index_count = p_index_count;
	uint32_t size_bytes = p_index_count * ((p_format == INDEX_BUFFER_FORMAT_UINT16) ? 2 : 4);
#ifdef DEBUG_ENABLED
	if (p_data.size()) {
		index_buffer.max_index = 0;
		ERR_FAIL_COND_V_MSG((uint32_t)p_data.size() != size_bytes, RID(),
				"Default index buffer initializer array size (" + itos(p_data.size()) + ") does not match format required size (" + itos(size_bytes) + ").");
		const uint8_t *r = p_data.ptr();
		if (p_format == INDEX_BUFFER_FORMAT_UINT16) {
			const uint16_t *index16 = (const uint16_t *)r;
			for (uint32_t i = 0; i < p_index_count; i++) {
				if (p_use_restart_indices && index16[i] == 0xFFFF) {
					continue; // Restart index, ignore.
				}
				index_buffer.max_index = MAX(index16[i], index_buffer.max_index);
			}
		} else {
			const uint32_t *index32 = (const uint32_t *)r;
			for (uint32_t i = 0; i < p_index_count; i++) {
				if (p_use_restart_indices && index32[i] == 0xFFFFFFFF) {
					continue; // Restart index, ignore.
				}
				index_buffer.max_index = MAX(index32[i], index_buffer.max_index);
			}
		}
	} else {
		index_buffer.max_index = 0xFFFFFFFF;
	}
#else
	index_buffer.max_index = 0xFFFFFFFF;
#endif
	index_buffer.size = size_bytes;
	index_buffer.usage = (RDD::BUFFER_USAGE_TRANSFER_FROM_BIT | RDD::BUFFER_USAGE_TRANSFER_TO_BIT | RDD::BUFFER_USAGE_INDEX_BIT);
	if (p_creation_bits.has_flag(BUFFER_CREATION_AS_STORAGE_BIT)) {
		index_buffer.usage.set_flag(RDD::BUFFER_USAGE_STORAGE_BIT);
	}
	if (p_creation_bits.has_flag(BUFFER_CREATION_DEVICE_ADDRESS_BIT)) {
		index_buffer.usage.set_flag(RDD::BUFFER_USAGE_DEVICE_ADDRESS_BIT);
	}
	if (p_creation_bits.has_flag(BUFFER_CREATION_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT)) {
		index_buffer.usage.set_flag(RDD::BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT);
	}
	index_buffer.driver_id = driver->buffer_create(index_buffer.size, index_buffer.usage, RDD::MEMORY_ALLOCATION_TYPE_GPU, frames_drawn);
	ERR_FAIL_COND_V(!index_buffer.driver_id, RID());

	// Index buffers are assumed to be immutable unless they don't have initial data.
	if (p_data.is_empty()) {
		index_buffer.draw_tracker = RDG::resource_tracker_create();
		index_buffer.draw_tracker->buffer_driver_id = index_buffer.driver_id;
	}

	if (p_data.size()) {
		Error initialize_error = _buffer_initialize(&index_buffer, p_data);
		if (initialize_error != OK) {
			RDG::resource_tracker_free(index_buffer.draw_tracker);
			driver->buffer_free(index_buffer.driver_id);
			return RID();
		}
	}

	_THREAD_SAFE_LOCK_
	buffer_memory += index_buffer.size;
	_THREAD_SAFE_UNLOCK_

	RID id = index_buffer_owner.make_rid(index_buffer);
#ifdef DEV_ENABLED
	set_resource_name(id, "RID:" + itos(id.get_id()));
#endif
	return id;
}

RID RenderingDevice::index_array_create(RID p_index_buffer, uint32_t p_index_offset, uint32_t p_index_count) {
	_THREAD_SAFE_METHOD_
	ERR_FAIL_COND_V(native_immutable_resource_is_retiring(p_index_buffer), RID());

	ERR_FAIL_COND_V(!index_buffer_owner.owns(p_index_buffer), RID());

	IndexBuffer *index_buffer = index_buffer_owner.get_or_null(p_index_buffer);

	ERR_FAIL_COND_V(p_index_count == 0, RID());
	ERR_FAIL_COND_V(p_index_offset + p_index_count > index_buffer->index_count, RID());

	IndexArray index_array;
	index_array.max_index = index_buffer->max_index;
	index_array.driver_id = index_buffer->driver_id;
	index_array.draw_tracker = index_buffer->draw_tracker;
	index_array.offset = p_index_offset;
	index_array.indices = p_index_count;
	index_array.format = index_buffer->format;
	index_array.supports_restart_indices = index_buffer->supports_restart_indices;
	index_array.transfer_worker_index = index_buffer->transfer_worker_index;
	index_array.transfer_worker_operation = index_buffer->transfer_worker_operation;

	RID id = index_array_owner.make_rid(index_array);
	_add_dependency(id, p_index_buffer);
	return id;
}

/****************/
/**** SHADER ****/
/****************/

// Keep the values in sync with the `UniformType` enum (file rendering_device_commons.h).
static const char *SHADER_UNIFORM_NAMES[RenderingDevice::UNIFORM_TYPE_MAX] = {
	"Sampler",
	"CombinedSampler", // UNIFORM_TYPE_SAMPLER_WITH_TEXTURE
	"Texture",
	"Image",
	"TextureBuffer",
	"SamplerTextureBuffer",
	"ImageBuffer",
	"UniformBuffer",
	"StorageBuffer",
	"InputAttachment",
	"UniformBufferDynamic",
	"StorageBufferDynamic",
};

String RenderingDevice::_shader_uniform_debug(RID p_shader, int p_set) {
	String ret;
	const Shader *shader = shader_owner.get_or_null(p_shader);
	ERR_FAIL_NULL_V(shader, String());
	for (int i = 0; i < shader->uniform_sets.size(); i++) {
		if (p_set >= 0 && i != p_set) {
			continue;
		}
		for (int j = 0; j < shader->uniform_sets[i].size(); j++) {
			const ShaderUniform &ui = shader->uniform_sets[i][j];
			if (!ret.is_empty()) {
				ret += "\n";
			}
			ret += "Set: " + itos(i) + " Binding: " + itos(ui.binding) + " Type: " + SHADER_UNIFORM_NAMES[ui.type] + " Writable: " + (ui.writable ? "Y" : "N") + " Length: " + itos(ui.length);
		}
	}
	return ret;
}

Vector<uint8_t> RenderingDevice::shader_compile_binary_from_spirv(const Vector<ShaderStageSPIRVData> &p_spirv, const String &p_shader_name) {
	return shader_compile_binary_from_spirv_with_options(p_spirv, p_shader_name, {});
}

Vector<uint8_t> RenderingDevice::shader_compile_binary_from_spirv_with_options(const Vector<ShaderStageSPIRVData> &p_spirv, const String &p_shader_name, BitField<ShaderCompileFlags> p_flags) {
	ERR_FAIL_COND_V(p_spirv.is_empty(), Vector<uint8_t>());
	ERR_FAIL_COND_V(!ShaderCompilePolicy::flags_valid(p_flags, SHADER_COMPILE_FLAG_DISABLE_FAST_MATH), Vector<uint8_t>());
	const RenderingShaderContainerFormat &container_format = driver->get_shader_container_format();
	ERR_FAIL_COND_V(!ShaderCompilePolicy::flags_valid(p_flags, container_format.supported_compile_flags()), Vector<uint8_t>());
	Ref<RenderingShaderContainer> shader_container = container_format.create_container();
	ERR_FAIL_COND_V(shader_container.is_null(), Vector<uint8_t>());

	// Compile shader binary from SPIR-V.
	bool code_compiled = shader_container->set_code_from_spirv(p_shader_name, p_spirv, p_flags);
	ERR_FAIL_COND_V_MSG(!code_compiled, Vector<uint8_t>(), vformat("Failed to compile code to native for SPIR-V."));

	return shader_container->to_bytes();
}

RID RenderingDevice::shader_create_from_bytecode(const Vector<uint8_t> &p_shader_binary, RID p_placeholder) {
	// Immutable samplers :
	// Expanding api when creating shader to allow passing optionally a set of immutable samplers
	// keeping existing api but extending it by sending an empty set.
	Vector<PipelineImmutableSampler> immutable_samplers;
	return shader_create_from_bytecode_with_samplers(p_shader_binary, p_placeholder, immutable_samplers);
}

RID RenderingDevice::shader_create_from_bytecode_with_samplers(const Vector<uint8_t> &p_shader_binary, RID p_placeholder, const Vector<PipelineImmutableSampler> &p_immutable_samplers) {
	return _qualification_shader_create(p_shader_binary, p_placeholder, p_immutable_samplers, nullptr, RID());
}
RID RenderingDevice::_qualification_shader_create(const Vector<uint8_t> &p_shader_binary, RID p_placeholder, const Vector<PipelineImmutableSampler> &p_immutable_samplers, GDExtensionDeviceExit::QualificationPhysicalReturn *p_cell, RID p_reserved) {
	_THREAD_SAFE_METHOD_

	Ref<RenderingShaderContainer> shader_container = driver->get_shader_container_format().create_container();
	ERR_FAIL_COND_V(shader_container.is_null(), RID());

	bool parsed_container = shader_container->from_bytes(p_shader_binary);
	ERR_FAIL_COND_V_MSG(!parsed_container, RID(), "Failed to parse shader container from binary.");

	ERR_FAIL_COND_V(!ShaderCompilePolicy::flags_valid(shader_container->get_compile_flags(), driver->get_shader_container_format().supported_compile_flags()), RID());
	if (p_placeholder.is_valid()) {
		const Shader *placeholder = shader_owner.get_or_null(p_placeholder);
		ERR_FAIL_NULL_V(placeholder, RID());
		ERR_FAIL_COND_V(placeholder->driver_id, RID());
	}

	// Freeze actual sampler owner identities before the first native effect.
	// They join native holds without changing legacy dependency/free behavior.
	uint64_t immutable_sampler_count = 0;
	for (const PipelineImmutableSampler &source_sampler : p_immutable_samplers) {
		immutable_sampler_count += source_sampler.get_id_count();
		ERR_FAIL_COND_V(immutable_sampler_count > INT32_MAX, RID());
	}
	Vector<RID> immutable_sampler_owners;
	ERR_FAIL_COND_V(immutable_sampler_owners.resize(immutable_sampler_count) != OK, RID());
	uint32_t immutable_sampler_index = 0;
	Vector<RDD::ImmutableSampler> driver_immutable_samplers;
	for (const PipelineImmutableSampler &source_sampler : p_immutable_samplers) {
		RDD::ImmutableSampler driver_sampler;
		driver_sampler.type = source_sampler.uniform_type;
		driver_sampler.binding = source_sampler.binding;

		for (uint32_t j = 0; j < source_sampler.get_id_count(); j++) {
			Sampler *sampler = sampler_owner.get_or_null(source_sampler.get_id(j));
			ERR_FAIL_NULL_V(sampler, RID());
			ERR_FAIL_COND_V(sampler->qualification_return, RID());
			ERR_FAIL_COND_V(native_immutable_resource_is_retiring(source_sampler.get_id(j)), RID());
			immutable_sampler_owners.write[immutable_sampler_index++] = source_sampler.get_id(j);
			driver_sampler.ids.push_back(sampler->driver_id);
		}

		driver_immutable_samplers.append(driver_sampler);
	}

	RDD::ShaderID shader_id = driver->shader_create_from_container(shader_container, driver_immutable_samplers);
	ERR_FAIL_COND_V(!shader_id, RID());

	// All good, let's create modules.

	RID id;
	if (p_cell) {
		CRASH_COND(!p_reserved.is_valid() || p_placeholder.is_valid() || !p_immutable_samplers.is_empty());
		id = p_reserved;
		shader_owner.initialize_rid(id, Shader());
	} else if (p_placeholder.is_null()) {
		id = shader_owner.make_rid();
	} else {
		id = p_placeholder;
	}

	Shader *shader = shader_owner.get_or_null(id);
	ERR_FAIL_NULL_V(shader, RID());
	shader->qualification_return = p_cell;
	shader->native_immutable_samplers = static_cast<Vector<RID> &&>(immutable_sampler_owners);

	*((ShaderReflection *)shader) = shader_container->get_shader_reflection();
	shader->name.clear();
	shader->name.append_utf8(shader_container->shader_name);
	shader->driver_id = shader_id;
	shader->compile_flags = shader_container->get_compile_flags();
	shader->layout_hash = driver->shader_get_layout_hash(shader_id);

	for (int i = 0; i < shader->uniform_sets.size(); i++) {
		uint32_t format = 0; // No format, default.

		if (shader->uniform_sets[i].size()) {
			// Sort and hash.

			shader->uniform_sets.write[i].sort();

			UniformSetFormat usformat;
			usformat.uniforms = shader->uniform_sets[i];
			RBMap<UniformSetFormat, uint32_t>::Element *E = uniform_set_format_cache.find(usformat);
			if (E) {
				format = E->get();
			} else {
				format = uniform_set_format_cache.size() + 1;
				uniform_set_format_cache.insert(usformat, format);
			}
		}

		shader->set_formats.push_back(format);
	}

	for (ShaderStage stage : shader->stages_vector) {
		switch (stage) {
			case SHADER_STAGE_VERTEX:
				shader->stage_bits.set_flag(RDD::PIPELINE_STAGE_VERTEX_SHADER_BIT);
				break;
			case SHADER_STAGE_FRAGMENT:
				shader->stage_bits.set_flag(RDD::PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
				break;
			case SHADER_STAGE_TESSELATION_CONTROL:
				shader->stage_bits.set_flag(RDD::PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT);
				break;
			case SHADER_STAGE_TESSELATION_EVALUATION:
				shader->stage_bits.set_flag(RDD::PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT);
				break;
			case SHADER_STAGE_COMPUTE:
				shader->stage_bits.set_flag(RDD::PIPELINE_STAGE_COMPUTE_SHADER_BIT);
				break;
			case SHADER_STAGE_RAYGEN:
			case SHADER_STAGE_ANY_HIT:
			case SHADER_STAGE_CLOSEST_HIT:
			case SHADER_STAGE_MISS:
			case SHADER_STAGE_INTERSECTION:
				shader->stage_bits.set_flag(RDD::PIPELINE_STAGE_RAY_TRACING_SHADER_BIT);
				break;
			default:
				DEV_ASSERT(false && "Unknown shader stage.");
				break;
		}
	}

#ifdef DEV_ENABLED
	set_resource_name(id, "RID:" + itos(id.get_id()));
#endif
	return id;
}

BitField<RenderingDevice::ShaderCompileFlags> RenderingDevice::shader_get_compile_flags(RID p_shader) const {
	_THREAD_SAFE_METHOD_
	const Shader *shader = const_cast<RenderingDevice *>(this)->shader_owner.get_or_null(p_shader);
	ERR_FAIL_NULL_V(shader, {});
	ERR_FAIL_COND_V(!shader->driver_id, {});
	return shader->compile_flags;
}

void RenderingDevice::shader_destroy_modules(RID p_shader) {
	Shader *shader = shader_owner.get_or_null(p_shader);
	ERR_FAIL_NULL(shader);
	ERR_FAIL_COND(shader->qualification_return);
	driver->shader_destroy_modules(shader->driver_id);
}

RID RenderingDevice::shader_create_placeholder() {
	_THREAD_SAFE_METHOD_

	Shader shader;
	return shader_owner.make_rid(shader);
}

uint64_t RenderingDevice::shader_get_vertex_input_attribute_mask(RID p_shader) {
	_THREAD_SAFE_METHOD_

	const Shader *shader = shader_owner.get_or_null(p_shader);
	ERR_FAIL_NULL_V(shader, 0);
	return shader->vertex_input_mask;
}

/******************/
/**** UNIFORMS ****/
/******************/

RID RenderingDevice::uniform_buffer_create(uint32_t p_size_bytes, Span<uint8_t> p_data, BitField<BufferCreationBits> p_creation_bits) {
	ERR_FAIL_COND_V_MSG(_recording_error() != OK, RID(), "Resource recording is suspended during frame fence recovery or readback callbacks.");
	ERR_FAIL_COND_V(p_data.size() && (uint32_t)p_data.size() != p_size_bytes, RID());

	Buffer buffer;
	buffer.size = p_size_bytes;
	buffer.usage = (RDD::BUFFER_USAGE_TRANSFER_TO_BIT | RDD::BUFFER_USAGE_UNIFORM_BIT);
	if (p_creation_bits.has_flag(BUFFER_CREATION_DEVICE_ADDRESS_BIT)) {
		buffer.usage.set_flag(RDD::BUFFER_USAGE_DEVICE_ADDRESS_BIT);
	}
	if (p_creation_bits.has_flag(BUFFER_CREATION_DYNAMIC_PERSISTENT_BIT)) {
		buffer.usage.set_flag(RDD::BUFFER_USAGE_DYNAMIC_PERSISTENT_BIT);

		// This is a precaution: Persistent buffers are meant for frequent CPU -> GPU transfers.
		// Writing to this buffer from GPU might cause sync issues if both CPU & GPU try to write at the
		// same time. It's probably fine (since CPU always advances the pointer before writing) but let's
		// stick to the known/intended use cases and scream if we deviate from it.
		buffer.usage.clear_flag(RDD::BUFFER_USAGE_TRANSFER_TO_BIT);
	}
	buffer.driver_id = driver->buffer_create(buffer.size, buffer.usage, RDD::MEMORY_ALLOCATION_TYPE_GPU, frames_drawn);
	ERR_FAIL_COND_V(!buffer.driver_id, RID());

	// Uniform buffers are assumed to be immutable unless they don't have initial data.
	if (p_data.is_empty()) {
		buffer.draw_tracker = RDG::resource_tracker_create();
		buffer.draw_tracker->buffer_driver_id = buffer.driver_id;
	}

	if (p_data.size()) {
		Error initialize_error = _buffer_initialize(&buffer, p_data);
		if (initialize_error != OK) {
			RDG::resource_tracker_free(buffer.draw_tracker);
			driver->buffer_free(buffer.driver_id);
			_production_disposal_returned(buffer.production_disposal_uses);
			return RID();
		}
	}

	_THREAD_SAFE_LOCK_
	buffer_memory += buffer.size;
	_THREAD_SAFE_UNLOCK_

	RID id = uniform_buffer_owner.make_rid(buffer);
#ifdef DEV_ENABLED
	set_resource_name(id, "RID:" + itos(id.get_id()));
#endif
	return id;
}

void RenderingDevice::_uniform_set_update_shared(UniformSet *p_uniform_set) {
	for (UniformSet::SharedTexture shared : p_uniform_set->shared_textures_to_update) {
		Texture *texture = texture_owner.get_or_null(shared.texture);
		ERR_CONTINUE(texture == nullptr);
		_texture_update_shared_fallback(shared.texture, texture, shared.writing);
	}
}

Error RenderingDevice::_uniform_set_admit_clear_transfers(UniformSet *p_set) {
	for (RID rid : p_set->pending_clear_textures) {
		Texture *texture = texture_owner.get_or_null(rid);
		if (!texture || !texture->pending_clear) {
			continue;
		}
		if (texture->owner.is_valid()) {
			texture = texture_owner.get_or_null(texture->owner);
			ERR_FAIL_NULL_V(texture, ERR_INVALID_DATA);
		}
		const Error error = _check_transfer_worker_texture(texture);
		if (error != OK) {
			return error;
		}
	}
	return OK;
}

Error RenderingDevice::_uniform_set_update_clears(UniformSet *p_uniform_set) {
	if (p_uniform_set->pending_clear_textures.is_empty()) {
		return OK;
	}

	for (RID texture_id : p_uniform_set->pending_clear_textures) {
		Texture *texture = texture_owner.get_or_null(texture_id);
		if (texture != nullptr) {
			{
				const Error dependency_error = _texture_check_pending_clear(texture_id, texture);
				if (dependency_error != OK) {
					return dependency_error;
				}
			}
		}
	}

	p_uniform_set->pending_clear_textures.clear();
	return OK;
}

RID RenderingDevice::uniform_set_create(const VectorView<RD::Uniform> &p_uniforms, RID p_shader, uint32_t p_shader_set, bool p_linear_pool) {
	return _uniform_set_create_qualification(p_uniforms, p_shader, p_shader_set, p_linear_pool, nullptr, nullptr);
}
RID RenderingDevice::_qualification_uniform_set_create(const VectorView<Uniform> &uniforms, RID shader, uint32_t index, GDExtensionDeviceExit::QualificationPhysicalReturn *cell, const LocalVector<GDExtensionDeviceExit::QualificationPhysicalReturn *> &children) {
	if (!cell || !cell->bridge || !cell->child || !cell->serial || cell->rid || cell->use_closed || !_qualification_reserve_free_event(cell)) {
		return RID();
	}
	RID result = _uniform_set_create_qualification(uniforms, shader, index, false, cell, &children);
	if (result.is_null()) {
		_qualification_release_free_event(cell);
	}
	return result;
}
RID RenderingDevice::_uniform_set_create_qualification(const VectorView<RD::Uniform> &p_uniforms, RID p_shader, uint32_t p_shader_set, bool p_linear_pool, GDExtensionDeviceExit::QualificationPhysicalReturn *p_cell, const LocalVector<GDExtensionDeviceExit::QualificationPhysicalReturn *> *p_children, const NativeDisposalWitness *p_native_disposal, const GDExtensionDeviceExit::ProductionRequest *p_production) {
	ERR_FAIL_COND_V_MSG(_recording_error() != OK, RID(), "Recording is suspended after a frame fence wait failure.");
	_THREAD_SAFE_METHOD_
	QualificationSourceAdmissionScope source_admission(*this);
	ERR_FAIL_COND_V(native_immutable_resource_is_retiring(p_shader), RID());

	ERR_FAIL_COND_V(p_uniforms.size() == 0, RID());

	Shader *shader = shader_owner.get_or_null(p_shader);
	ERR_FAIL_NULL_V(shader, RID());

	LocalVector<GDExtensionDeviceExit::QualificationPhysicalReturn *> qualification_dependencies;
	LocalVector<NativeDisposalWitness> output_dependencies;
	if (shader->qualification_return) {
		qualification_dependencies.push_back(shader->qualification_return);
	}
	for (uint32_t u = 0; u < p_uniforms.size(); ++u) {
		for (uint32_t i = 0; i < p_uniforms[u].get_id_count(); ++i) {
			ERR_FAIL_COND_V(native_immutable_resource_is_retiring(p_uniforms[u].get_id(i)), RID());
			const auto &uniform = p_uniforms[u];
			auto *texture = texture_owner.owns(uniform.get_id(i)) ? texture_owner.get_or_null(uniform.get_id(i)) : nullptr;
			ERR_FAIL_COND_V(texture && texture->qualification_return && (!p_children || !p_children->has(texture->qualification_return)), RID());
			if (texture && texture->qualification_return && !qualification_dependencies.has(texture->qualification_return)) {
				qualification_dependencies.push_back(texture->qualification_return);
			}
			auto *sampler = sampler_owner.owns(uniform.get_id(i)) ? sampler_owner.get_or_null(uniform.get_id(i)) : nullptr;
			ERR_FAIL_COND_V(sampler && sampler->qualification_return && (!p_children || !p_children->has(sampler->qualification_return)), RID());
			if (sampler && sampler->qualification_return && !qualification_dependencies.has(sampler->qualification_return)) {
				qualification_dependencies.push_back(sampler->qualification_return);
			}
			auto *buffer = storage_buffer_owner.owns(uniform.get_id(i)) ? storage_buffer_owner.get_or_null(uniform.get_id(i)) : nullptr;
			if (buffer && buffer->qualification_return && !qualification_dependencies.has(buffer->qualification_return)) {
				qualification_dependencies.push_back(buffer->qualification_return);
			}
			if (buffer && buffer->native_disposal.cell.is_valid() && buffer->native_disposal.cell->production_request) {
				const auto &physical = **buffer->native_disposal.cell;
				ERR_FAIL_COND_V(!p_production || physical.production_request != p_production || physical.production_request_serial != p_production->serial || physical.state != NativeDisposalWitness::State::LIVE || physical.production_dependent_sets == UINT32_MAX, RID());
				// The original owned-set factory carries its actual predeclared
				// group/root witness; nonowning E09 sets require exact aliases.
				bool declared = p_native_disposal && p_native_disposal->cell.is_valid() && p_native_disposal->cell->production_request == p_production && p_native_disposal->cell->production_request_serial == p_production->serial && p_native_disposal->cell->production_group_serial == physical.production_group_serial && p_native_disposal->cell->production_pin_serial == physical.production_pin_serial;
				bool duplicate = false;
				for (uint32_t r = 0; r < p_production->resource_count; ++r) {
					const auto &resource = p_production->resources[r];
					declared |= resource.output_alias_prepared && resource.imported && !resource.retiring && resource.output_witness.cell == buffer->native_disposal.cell && resource.primary == uniform.get_id(i) && resource.output_root_serial == physical.production_pin_serial && resource.output_group_serial == physical.production_group_serial;
				}
				ERR_FAIL_COND_V(!declared, RID());
				for (const auto &dependency : output_dependencies) {
					duplicate |= dependency.cell == buffer->native_disposal.cell;
				}
				if (!duplicate) {
					output_dependencies.push_back(buffer->native_disposal);
				}
			}
		}
	}
	if (!qualification_dependencies.is_empty()) {
		ERR_FAIL_COND_V(render_thread_id != Thread::get_caller_id() || p_linear_pool, RID());
		ERR_FAIL_COND_V(!p_production && qualification_dependencies.size() > 128, RID());
		const auto bridge = qualification_dependencies[0]->bridge;
		for (auto *cell : qualification_dependencies) {
			bool owned = !p_production && cell->bridge == bridge;
			if (p_production && p_production->capture_inflight && p_production->serial) {
				for (uint32_t r = 0; r < p_production->resource_count && !owned; ++r) {
					const auto &resource = p_production->resources[r];
					for (uint32_t u = 0; u < resource.use_count; ++u) {
						const auto &use = resource.uses[u];
						owned |= use.physical == cell && use.request == p_production && use.request_serial == p_production->serial && use.same_physical() && use.linked();
					}
				}
			}
			ERR_FAIL_COND_V(!owned || cell->use_closed.load() || cell->dependent_children.load() == UINT32_MAX, RID());
		}
	}

	ERR_FAIL_COND_V_MSG(p_shader_set >= (uint32_t)shader->uniform_sets.size() || shader->uniform_sets[p_shader_set].is_empty(), RID(),
			"Desired set (" + itos(p_shader_set) + ") not used by shader.");
	// See that all sets in shader are satisfied.

	const Vector<ShaderUniform> &set = shader->uniform_sets[p_shader_set];

	uint32_t uniform_count = p_uniforms.size();
	const Uniform *uniforms = p_uniforms.ptr();

	uint32_t set_uniform_count = set.size();
	const ShaderUniform *set_uniforms = set.ptr();

	LocalVector<RDD::BoundUniform> driver_uniforms;
	driver_uniforms.resize(set_uniform_count);

	// Used for verification to make sure a uniform set does not use a framebuffer bound texture.
	LocalVector<UniformSet::AttachableTexture> attachable_textures;
	Vector<RDG::ResourceTracker *> draw_trackers;
	Vector<RDG::ResourceUsage> draw_trackers_usage;
	HashMap<RID, RDG::ResourceUsage> untracked_usage;
	Vector<UniformSet::SharedTexture> shared_textures_to_update;
	LocalVector<RID> pending_clear_textures;
	Vector<RID> acceleration_structures;

	for (uint32_t i = 0; i < set_uniform_count; i++) {
		const ShaderUniform &set_uniform = set_uniforms[i];
		int uniform_idx = -1;
		for (int j = 0; j < (int)uniform_count; j++) {
			if (uniforms[j].binding == set_uniform.binding) {
				uniform_idx = j;
				break;
			}
		}
		ERR_FAIL_COND_V_MSG(uniform_idx == -1, RID(),
				"All the shader bindings for the given set must be covered by the uniforms provided. Binding (" + itos(set_uniform.binding) + "), set (" + itos(p_shader_set) + ") was not provided.");

		const Uniform &uniform = uniforms[uniform_idx];

		ERR_FAIL_INDEX_V(uniform.uniform_type, RD::UNIFORM_TYPE_MAX, RID());
		ERR_FAIL_COND_V_MSG(uniform.uniform_type != set_uniform.type, RID(), "Shader '" + shader->name + "' Mismatch uniform type for binding (" + itos(set_uniform.binding) + "), set (" + itos(p_shader_set) + "). Expected '" + SHADER_UNIFORM_NAMES[set_uniform.type] + "', supplied: '" + SHADER_UNIFORM_NAMES[uniform.uniform_type] + "'.");

		RDD::BoundUniform &driver_uniform = driver_uniforms[i];
		driver_uniform.type = uniform.uniform_type;
		driver_uniform.binding = uniform.binding;

		// Mark immutable samplers to be skipped when creating uniform set.
		driver_uniform.immutable_sampler = uniform.immutable_sampler;

		switch (uniform.uniform_type) {
			case UNIFORM_TYPE_SAMPLER: {
				if (uniform.get_id_count() != (uint32_t)set_uniform.length) {
					if (set_uniform.length > 1) {
						ERR_FAIL_V_MSG(RID(), "Sampler (binding: " + itos(uniform.binding) + ") is an array of (" + itos(set_uniform.length) + ") sampler elements, so it should be provided equal number of sampler IDs to satisfy it (IDs provided: " + itos(uniform.get_id_count()) + ").");
					} else {
						ERR_FAIL_V_MSG(RID(), "Sampler (binding: " + itos(uniform.binding) + ") should provide one ID referencing a sampler (IDs provided: " + itos(uniform.get_id_count()) + ").");
					}
				}

				for (uint32_t j = 0; j < uniform.get_id_count(); j++) {
					Sampler *sampler = sampler_owner.get_or_null(uniform.get_id(j));
					ERR_FAIL_NULL_V_MSG(sampler, RID(), "Sampler (binding: " + itos(uniform.binding) + ", index " + itos(j) + ") is not a valid sampler.");

					driver_uniform.ids.push_back(sampler->driver_id);
				}
			} break;
			case UNIFORM_TYPE_SAMPLER_WITH_TEXTURE: {
				if (uniform.get_id_count() != (uint32_t)set_uniform.length * 2) {
					if (set_uniform.length > 1) {
						ERR_FAIL_V_MSG(RID(), "SamplerTexture (binding: " + itos(uniform.binding) + ") is an array of (" + itos(set_uniform.length) + ") sampler&texture elements, so it should provided twice the amount of IDs (sampler,texture pairs) to satisfy it (IDs provided: " + itos(uniform.get_id_count()) + ").");
					} else {
						ERR_FAIL_V_MSG(RID(), "SamplerTexture (binding: " + itos(uniform.binding) + ") should provide two IDs referencing a sampler and then a texture (IDs provided: " + itos(uniform.get_id_count()) + ").");
					}
				}

				for (uint32_t j = 0; j < uniform.get_id_count(); j += 2) {
					Sampler *sampler = sampler_owner.get_or_null(uniform.get_id(j + 0));
					ERR_FAIL_NULL_V_MSG(sampler, RID(), "SamplerBuffer (binding: " + itos(uniform.binding) + ", index " + itos(j + 1) + ") is not a valid sampler.");

					RID texture_id = uniform.get_id(j + 1);
					Texture *texture = texture_owner.get_or_null(texture_id);
					ERR_FAIL_NULL_V_MSG(texture, RID(), "Texture (binding: " + itos(uniform.binding) + ", index " + itos(j) + ") is not a valid texture.");

					ERR_FAIL_COND_V_MSG(!(texture->usage_flags & TEXTURE_USAGE_SAMPLING_BIT), RID(),
							"Texture (binding: " + itos(uniform.binding) + ", index " + itos(j) + ") needs the TEXTURE_USAGE_SAMPLING_BIT usage flag set in order to be used as uniform.");

					if ((texture->usage_flags & (TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | TEXTURE_USAGE_DEPTH_RESOLVE_ATTACHMENT_BIT | TEXTURE_USAGE_INPUT_ATTACHMENT_BIT))) {
						UniformSet::AttachableTexture attachable_texture;
						attachable_texture.bind = set_uniform.binding;
						attachable_texture.texture = texture->owner.is_valid() ? texture->owner : uniform.get_id(j + 1);
						attachable_textures.push_back(attachable_texture);
					}

					if (texture->pending_clear) {
						pending_clear_textures.push_back(texture_id);
					}

					RDD::TextureID driver_id = texture->driver_id;
					RDG::ResourceTracker *tracker = texture->draw_tracker;
					if (texture->shared_fallback != nullptr && texture->shared_fallback->texture.id != 0) {
						driver_id = texture->shared_fallback->texture;
						tracker = texture->shared_fallback->texture_tracker;
						shared_textures_to_update.push_back({ false, texture_id });
					}

					if (tracker != nullptr) {
						draw_trackers.push_back(tracker);
						draw_trackers_usage.push_back(RDG::RESOURCE_USAGE_TEXTURE_SAMPLE);
					} else {
						untracked_usage[texture_id] = RDG::RESOURCE_USAGE_TEXTURE_SAMPLE;
					}

					DEV_ASSERT(!texture->owner.is_valid() || texture_owner.get_or_null(texture->owner));

					driver_uniform.ids.push_back(sampler->driver_id);
					driver_uniform.ids.push_back(driver_id);
					{
						const Error dependency_error = _check_transfer_worker_texture(texture);
						if (dependency_error != OK) {
							return RID();
						}
					}
				}
			} break;
			case UNIFORM_TYPE_TEXTURE: {
				if (uniform.get_id_count() != (uint32_t)set_uniform.length) {
					if (set_uniform.length > 1) {
						ERR_FAIL_V_MSG(RID(), "Texture (binding: " + itos(uniform.binding) + ") is an array of (" + itos(set_uniform.length) + ") textures, so it should be provided equal number of texture IDs to satisfy it (IDs provided: " + itos(uniform.get_id_count()) + ").");
					} else {
						ERR_FAIL_V_MSG(RID(), "Texture (binding: " + itos(uniform.binding) + ") should provide one ID referencing a texture (IDs provided: " + itos(uniform.get_id_count()) + ").");
					}
				}

				for (uint32_t j = 0; j < uniform.get_id_count(); j++) {
					RID texture_id = uniform.get_id(j);
					Texture *texture = texture_owner.get_or_null(texture_id);
					ERR_FAIL_NULL_V_MSG(texture, RID(), "Texture (binding: " + itos(uniform.binding) + ", index " + itos(j) + ") is not a valid texture.");

					ERR_FAIL_COND_V_MSG(!(texture->usage_flags & TEXTURE_USAGE_SAMPLING_BIT), RID(),
							"Texture (binding: " + itos(uniform.binding) + ", index " + itos(j) + ") needs the TEXTURE_USAGE_SAMPLING_BIT usage flag set in order to be used as uniform.");

					if ((texture->usage_flags & (TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | TEXTURE_USAGE_DEPTH_RESOLVE_ATTACHMENT_BIT | TEXTURE_USAGE_INPUT_ATTACHMENT_BIT))) {
						UniformSet::AttachableTexture attachable_texture;
						attachable_texture.bind = set_uniform.binding;
						attachable_texture.texture = texture->owner.is_valid() ? texture->owner : uniform.get_id(j);
						attachable_textures.push_back(attachable_texture);
					}

					if (texture->pending_clear) {
						pending_clear_textures.push_back(texture_id);
					}

					RDD::TextureID driver_id = texture->driver_id;
					RDG::ResourceTracker *tracker = texture->draw_tracker;
					if (texture->shared_fallback != nullptr && texture->shared_fallback->texture.id != 0) {
						driver_id = texture->shared_fallback->texture;
						tracker = texture->shared_fallback->texture_tracker;
						shared_textures_to_update.push_back({ false, texture_id });
					}

					if (tracker != nullptr) {
						draw_trackers.push_back(tracker);
						draw_trackers_usage.push_back(RDG::RESOURCE_USAGE_TEXTURE_SAMPLE);
					} else {
						untracked_usage[texture_id] = RDG::RESOURCE_USAGE_TEXTURE_SAMPLE;
					}

					DEV_ASSERT(!texture->owner.is_valid() || texture_owner.get_or_null(texture->owner));

					driver_uniform.ids.push_back(driver_id);
					{
						const Error dependency_error = _check_transfer_worker_texture(texture);
						if (dependency_error != OK) {
							return RID();
						}
					}
				}
			} break;
			case UNIFORM_TYPE_IMAGE: {
				if (uniform.get_id_count() != (uint32_t)set_uniform.length) {
					if (set_uniform.length > 1) {
						ERR_FAIL_V_MSG(RID(), "Image (binding: " + itos(uniform.binding) + ") is an array of (" + itos(set_uniform.length) + ") textures, so it should be provided equal number of texture IDs to satisfy it (IDs provided: " + itos(uniform.get_id_count()) + ").");
					} else {
						ERR_FAIL_V_MSG(RID(), "Image (binding: " + itos(uniform.binding) + ") should provide one ID referencing a texture (IDs provided: " + itos(uniform.get_id_count()) + ").");
					}
				}

				for (uint32_t j = 0; j < uniform.get_id_count(); j++) {
					RID texture_id = uniform.get_id(j);
					Texture *texture = texture_owner.get_or_null(texture_id);

					ERR_FAIL_NULL_V_MSG(texture, RID(),
							"Image (binding: " + itos(uniform.binding) + ", index " + itos(j) + ") is not a valid texture.");

					ERR_FAIL_COND_V_MSG(!(texture->usage_flags & TEXTURE_USAGE_STORAGE_BIT), RID(),
							"Image (binding: " + itos(uniform.binding) + ", index " + itos(j) + ") needs the TEXTURE_USAGE_STORAGE_BIT usage flag set in order to be used as uniform.");

					if (texture->owner.is_null() && texture->shared_fallback != nullptr) {
						shared_textures_to_update.push_back({ true, texture_id });
					}

					if (texture->pending_clear) {
						pending_clear_textures.push_back(texture_id);
					}

					if (_texture_make_mutable(texture, texture_id)) {
						// The texture must be mutable as a layout transition will be required.
						draw_graph.add_synchronization();
					}

					if (texture->draw_tracker != nullptr) {
						draw_trackers.push_back(texture->draw_tracker);

						if (set_uniform.writable) {
							draw_trackers_usage.push_back(RDG::RESOURCE_USAGE_STORAGE_IMAGE_READ_WRITE);
						} else {
							draw_trackers_usage.push_back(RDG::RESOURCE_USAGE_STORAGE_IMAGE_READ);
						}
					}

					DEV_ASSERT(!texture->owner.is_valid() || texture_owner.get_or_null(texture->owner));

					driver_uniform.ids.push_back(texture->driver_id);
					{
						const Error dependency_error = _check_transfer_worker_texture(texture);
						if (dependency_error != OK) {
							return RID();
						}
					}
				}
			} break;
			case UNIFORM_TYPE_TEXTURE_BUFFER: {
				if (uniform.get_id_count() != (uint32_t)set_uniform.length) {
					if (set_uniform.length > 1) {
						ERR_FAIL_V_MSG(RID(), "Buffer (binding: " + itos(uniform.binding) + ") is an array of (" + itos(set_uniform.length) + ") texture buffer elements, so it should be provided equal number of texture buffer IDs to satisfy it (IDs provided: " + itos(uniform.get_id_count()) + ").");
					} else {
						ERR_FAIL_V_MSG(RID(), "Buffer (binding: " + itos(uniform.binding) + ") should provide one ID referencing a texture buffer (IDs provided: " + itos(uniform.get_id_count()) + ").");
					}
				}

				for (uint32_t j = 0; j < uniform.get_id_count(); j++) {
					RID buffer_id = uniform.get_id(j);
					Buffer *buffer = texture_buffer_owner.get_or_null(buffer_id);
					ERR_FAIL_NULL_V_MSG(buffer, RID(), "Texture Buffer (binding: " + itos(uniform.binding) + ", index " + itos(j) + ") is not a valid texture buffer.");

					if (set_uniform.writable && _buffer_make_mutable(buffer, buffer_id)) {
						// The buffer must be mutable if it's used for writing.
						draw_graph.add_synchronization();
					}

					if (buffer->draw_tracker != nullptr) {
						draw_trackers.push_back(buffer->draw_tracker);

						if (set_uniform.writable) {
							draw_trackers_usage.push_back(RDG::RESOURCE_USAGE_TEXTURE_BUFFER_READ_WRITE);
						} else {
							draw_trackers_usage.push_back(RDG::RESOURCE_USAGE_TEXTURE_BUFFER_READ);
						}
					} else {
						untracked_usage[buffer_id] = RDG::RESOURCE_USAGE_TEXTURE_BUFFER_READ;
					}

					driver_uniform.ids.push_back(buffer->driver_id);
					{
						const Error dependency_error = _check_transfer_worker_buffer(buffer);
						if (dependency_error != OK) {
							return RID();
						}
					}
				}
			} break;
			case UNIFORM_TYPE_SAMPLER_WITH_TEXTURE_BUFFER: {
				if (uniform.get_id_count() != (uint32_t)set_uniform.length * 2) {
					if (set_uniform.length > 1) {
						ERR_FAIL_V_MSG(RID(), "SamplerBuffer (binding: " + itos(uniform.binding) + ") is an array of (" + itos(set_uniform.length) + ") sampler buffer elements, so it should provided twice the amount of IDs (sampler,buffer pairs) to satisfy it (IDs provided: " + itos(uniform.get_id_count()) + ").");
					} else {
						ERR_FAIL_V_MSG(RID(), "SamplerBuffer (binding: " + itos(uniform.binding) + ") should provide two IDs referencing a sampler and then a texture buffer (IDs provided: " + itos(uniform.get_id_count()) + ").");
					}
				}

				for (uint32_t j = 0; j < uniform.get_id_count(); j += 2) {
					Sampler *sampler = sampler_owner.get_or_null(uniform.get_id(j + 0));
					ERR_FAIL_NULL_V_MSG(sampler, RID(), "SamplerBuffer (binding: " + itos(uniform.binding) + ", index " + itos(j + 1) + ") is not a valid sampler.");

					RID buffer_id = uniform.get_id(j + 1);
					Buffer *buffer = texture_buffer_owner.get_or_null(buffer_id);
					ERR_FAIL_NULL_V_MSG(buffer, RID(), "SamplerBuffer (binding: " + itos(uniform.binding) + ", index " + itos(j + 1) + ") is not a valid texture buffer.");

					if (buffer->draw_tracker != nullptr) {
						draw_trackers.push_back(buffer->draw_tracker);
						draw_trackers_usage.push_back(RDG::RESOURCE_USAGE_TEXTURE_BUFFER_READ);
					} else {
						untracked_usage[buffer_id] = RDG::RESOURCE_USAGE_TEXTURE_BUFFER_READ;
					}

					driver_uniform.ids.push_back(sampler->driver_id);
					driver_uniform.ids.push_back(buffer->driver_id);
					{
						const Error dependency_error = _check_transfer_worker_buffer(buffer);
						if (dependency_error != OK) {
							return RID();
						}
					}
				}
			} break;
			case UNIFORM_TYPE_IMAGE_BUFFER: {
				// Todo.
			} break;
			case UNIFORM_TYPE_UNIFORM_BUFFER:
			case UNIFORM_TYPE_UNIFORM_BUFFER_DYNAMIC: {
				ERR_FAIL_COND_V_MSG(uniform.get_id_count() != 1, RID(),
						"Uniform buffer supplied (binding: " + itos(uniform.binding) + ") must provide one ID (" + itos(uniform.get_id_count()) + " provided).");

				RID buffer_id = uniform.get_id(0);
				Buffer *buffer = uniform_buffer_owner.get_or_null(buffer_id);
				ERR_FAIL_NULL_V_MSG(buffer, RID(), "Uniform buffer supplied (binding: " + itos(uniform.binding) + ") is invalid.");

				ERR_FAIL_COND_V_MSG(buffer->size < (uint32_t)set_uniform.length, RID(),
						"Uniform buffer supplied (binding: " + itos(uniform.binding) + ") size (" + itos(buffer->size) + ") is smaller than size of shader uniform: (" + itos(set_uniform.length) + ").");

				if (buffer->draw_tracker != nullptr) {
					draw_trackers.push_back(buffer->draw_tracker);
					draw_trackers_usage.push_back(RDG::RESOURCE_USAGE_UNIFORM_BUFFER_READ);
				} else {
					untracked_usage[buffer_id] = RDG::RESOURCE_USAGE_UNIFORM_BUFFER_READ;
				}

				driver_uniform.ids.push_back(buffer->driver_id);
				{
					const Error dependency_error = _check_transfer_worker_buffer(buffer);
					if (dependency_error != OK) {
						return RID();
					}
				}
			} break;
			case UNIFORM_TYPE_STORAGE_BUFFER:
			case UNIFORM_TYPE_STORAGE_BUFFER_DYNAMIC: {
				ERR_FAIL_COND_V_MSG(uniform.get_id_count() != 1, RID(),
						"Storage buffer supplied (binding: " + itos(uniform.binding) + ") must provide one ID (" + itos(uniform.get_id_count()) + " provided).");

				Buffer *buffer = nullptr;

				RID buffer_id = uniform.get_id(0);
				if (storage_buffer_owner.owns(buffer_id)) {
					buffer = storage_buffer_owner.get_or_null(buffer_id);
				} else if (vertex_buffer_owner.owns(buffer_id)) {
					buffer = vertex_buffer_owner.get_or_null(buffer_id);

					ERR_FAIL_COND_V_MSG(!(buffer->usage.has_flag(RDD::BUFFER_USAGE_STORAGE_BIT)), RID(), "Vertex buffer supplied (binding: " + itos(uniform.binding) + ") was not created with storage flag.");
				}
				ERR_FAIL_NULL_V_MSG(buffer, RID(), "Storage buffer supplied (binding: " + itos(uniform.binding) + ") is invalid.");

				// If 0, then it's sized on link time.
				ERR_FAIL_COND_V_MSG(set_uniform.length > 0 && buffer->size != (uint32_t)set_uniform.length, RID(),
						"Storage buffer supplied (binding: " + itos(uniform.binding) + ") size (" + itos(buffer->size) + ") does not match size of shader uniform: (" + itos(set_uniform.length) + ").");

				if (set_uniform.writable && _buffer_make_mutable(buffer, buffer_id)) {
					// The buffer must be mutable if it's used for writing.
					draw_graph.add_synchronization();
				}

				if (buffer->draw_tracker != nullptr) {
					draw_trackers.push_back(buffer->draw_tracker);

					if (set_uniform.writable) {
						draw_trackers_usage.push_back(RDG::RESOURCE_USAGE_STORAGE_BUFFER_READ_WRITE);
					} else {
						draw_trackers_usage.push_back(RDG::RESOURCE_USAGE_STORAGE_BUFFER_READ);
					}
				} else {
					untracked_usage[buffer_id] = RDG::RESOURCE_USAGE_STORAGE_BUFFER_READ;
				}

				driver_uniform.ids.push_back(buffer->driver_id);
				{
					const Error dependency_error = _check_transfer_worker_buffer(buffer);
					if (dependency_error != OK) {
						return RID();
					}
				}
			} break;
			case UNIFORM_TYPE_INPUT_ATTACHMENT: {
				ERR_FAIL_COND_V_MSG(shader->pipeline_type != PIPELINE_TYPE_RASTERIZATION, RID(), "InputAttachment (binding: " + itos(uniform.binding) + ") supplied for non-render shader (this is not allowed).");

				if (uniform.get_id_count() != (uint32_t)set_uniform.length) {
					if (set_uniform.length > 1) {
						ERR_FAIL_V_MSG(RID(), "InputAttachment (binding: " + itos(uniform.binding) + ") is an array of (" + itos(set_uniform.length) + ") textures, so it should be provided equal number of texture IDs to satisfy it (IDs provided: " + itos(uniform.get_id_count()) + ").");
					} else {
						ERR_FAIL_V_MSG(RID(), "InputAttachment (binding: " + itos(uniform.binding) + ") should provide one ID referencing a texture (IDs provided: " + itos(uniform.get_id_count()) + ").");
					}
				}

				for (uint32_t j = 0; j < uniform.get_id_count(); j++) {
					RID texture_id = uniform.get_id(j);
					Texture *texture = texture_owner.get_or_null(texture_id);

					ERR_FAIL_NULL_V_MSG(texture, RID(),
							"InputAttachment (binding: " + itos(uniform.binding) + ", index " + itos(j) + ") is not a valid texture.");

					ERR_FAIL_COND_V_MSG(!(texture->usage_flags & TEXTURE_USAGE_SAMPLING_BIT), RID(),
							"InputAttachment (binding: " + itos(uniform.binding) + ", index " + itos(j) + ") needs the TEXTURE_USAGE_SAMPLING_BIT usage flag set in order to be used as uniform.");

					DEV_ASSERT(!texture->owner.is_valid() || texture_owner.get_or_null(texture->owner));

					driver_uniform.ids.push_back(texture->driver_id);
					{
						const Error dependency_error = _check_transfer_worker_texture(texture);
						if (dependency_error != OK) {
							return RID();
						}
					}
				}
			} break;
			case UNIFORM_TYPE_ACCELERATION_STRUCTURE: {
				ERR_FAIL_COND_V_MSG(uniform.get_id_count() != 1, RID(),
						"Acceleration structure supplied (binding: " + itos(uniform.binding) + ") must provide one ID (" + itos(uniform.get_id_count()) + " provided).");

				RID accel_id = uniform.get_id(0);
				AccelerationStructure *accel = acceleration_structure_owner.get_or_null(accel_id);
				ERR_FAIL_NULL_V_MSG(accel, RID(), "Acceleration Structure supplied (binding: " + itos(uniform.binding) + ") is invalid.");

				if (accel->draw_tracker != nullptr) {
					draw_trackers.push_back(accel->draw_tracker);
					// Acceleration structure is never going to be writable from raytracing shaders
					draw_trackers_usage.push_back(RDG::RESOURCE_USAGE_ACCELERATION_STRUCTURE_READ);
				}

				driver_uniform.ids.push_back(accel->driver_id);
				acceleration_structures.push_back(accel_id);
			} break;
			default: {
			}
		}
	}

	RID reserved_set;
	if (p_native_disposal || !qualification_dependencies.is_empty()) {
		reserved_set = uniform_set_owner.allocate_rid();
		ERR_FAIL_COND_V(reserved_set.is_null(), RID());
	}
	RDD::UniformSetID driver_uniform_set = driver->uniform_set_create(driver_uniforms, shader->driver_id, p_shader_set, p_linear_pool ? frame : -1);
	if (!driver_uniform_set) {
		if (reserved_set.is_valid()) {
			uniform_set_owner.initialize_rid(reserved_set, UniformSet());
			uniform_set_owner.free(reserved_set);
		}
		ERR_FAIL_V(RID());
	}

	UniformSet uniform_set;
	for (uint32_t uniform_index = 0; uniform_index < uniform_count; ++uniform_index) {
		const Uniform &uniform = uniforms[uniform_index];
		for (const ShaderUniform &reflected : shader->uniform_sets[p_shader_set]) {
			if (reflected.binding == uniform.binding && reflected.writable) {
				for (uint32_t i = 0; i < uniform.get_id_count(); ++i) {
					const RID rid = uniform.get_id(i);
					if (!uniform_set.writable_resources.has(rid)) {
						uniform_set.writable_resources.push_back(rid);
					}
				}
			}
		}
	}
	uniform_set.production_output_dependencies = output_dependencies;
	for (auto &dependency : uniform_set.production_output_dependencies) {
		++dependency.cell->production_dependent_sets;
	}
	uniform_set.qualification_dependencies = qualification_dependencies;
	uniform_set.qualification_return = p_cell;
	for (auto *cell : qualification_dependencies) {
		cell->dependent_children.fetch_add(1);
	}
	uniform_set.driver_id = driver_uniform_set;
	if (p_native_disposal) {
		uniform_set.native_disposal = *p_native_disposal;
		uniform_set.native_disposal_rid = reserved_set.get_id();
		uniform_set.native_disposal_serial = p_native_disposal->cell->serial;
		p_native_disposal->cell->rid = reserved_set.get_id();
		p_native_disposal->cell->backing = driver_uniform_set.id;
	}
	uniform_set.format = shader->set_formats[p_shader_set];
	uniform_set.attachable_textures = attachable_textures;
	uniform_set.draw_trackers = draw_trackers;
	uniform_set.draw_trackers_usage = draw_trackers_usage;
	uniform_set.untracked_usage = untracked_usage;
	uniform_set.shared_textures_to_update = shared_textures_to_update;
	uniform_set.pending_clear_textures = pending_clear_textures;
	uniform_set.acceleration_structures = acceleration_structures;
	uniform_set.shader_set = p_shader_set;
	uniform_set.shader_id = p_shader;

	RID id = reserved_set;
	if (id.is_valid()) {
		uniform_set_owner.initialize_rid(id, uniform_set);
	} else {
		id = uniform_set_owner.make_rid(uniform_set);
	}
	if (p_cell) {
		p_cell->rid = id.get_id();
	}
#ifdef DEV_ENABLED
	set_resource_name(id, "RID:" + itos(id.get_id()));
#endif
	// Add dependencies.
	_add_dependency(id, p_shader);
	for (uint32_t i = 0; i < uniform_count; i++) {
		const Uniform &uniform = uniforms[i];
		int id_count = uniform.get_id_count();
		for (int j = 0; j < id_count; j++) {
			_add_dependency(id, uniform.get_id(j));
		}
	}

	return id;
}

bool RenderingDevice::uniform_set_is_valid(RID p_uniform_set) {
	_THREAD_SAFE_METHOD_

	return uniform_set_owner.owns(p_uniform_set);
}

void RenderingDevice::uniform_set_set_invalidation_callback(RID p_uniform_set, InvalidationCallback p_callback, void *p_userdata) {
	_THREAD_SAFE_METHOD_

	UniformSet *us = uniform_set_owner.get_or_null(p_uniform_set);
	ERR_FAIL_NULL(us);
	ERR_FAIL_COND(us->qualification_return);
	ERR_FAIL_COND(p_callback && native_immutable_resource_is_retiring(p_uniform_set));
	us->production_material_origin = RID();
	us->production_material_origin_generation = 0;
	us->invalidated_callback = p_callback;
	us->invalidated_callback_userdata = p_userdata;
}

bool RenderingDevice::uniform_sets_have_linear_pools() const {
	return driver->uniform_sets_have_linear_pools();
}

/*******************/
/**** PIPELINES ****/
/*******************/

RID RenderingDevice::render_pipeline_create(RID p_shader, FramebufferFormatID p_framebuffer_format, VertexFormatID p_vertex_format, RenderPrimitive p_render_primitive, const PipelineRasterizationState &p_rasterization_state, const PipelineMultisampleState &p_multisample_state, const PipelineDepthStencilState &p_depth_stencil_state, const PipelineColorBlendState &p_blend_state, BitField<PipelineDynamicStateFlags> p_dynamic_state_flags, uint32_t p_for_render_pass, const Vector<PipelineSpecializationConstant> &p_specialization_constants) {
	return _qualification_render_pipeline_create_internal(p_shader, p_framebuffer_format, p_vertex_format, p_render_primitive, p_rasterization_state, p_multisample_state, p_depth_stencil_state, p_blend_state, p_dynamic_state_flags, p_for_render_pass, p_specialization_constants, nullptr, RID());
}

RID RenderingDevice::_qualification_render_pipeline_create(RID p_shader, FramebufferFormatID p_format, GDExtensionDeviceExit::QualificationPhysicalReturn *p_cell, RID p_reserved) {
	ERR_FAIL_NULL_V(p_cell, RID());
	PipelineDepthStencilState depth;
	depth.enable_depth_test = true;
	depth.enable_depth_write = true;
	depth.depth_compare_operator = COMPARE_OP_ALWAYS;
	return _qualification_render_pipeline_create_internal(p_shader, p_format, INVALID_ID, RENDER_PRIMITIVE_TRIANGLES, PipelineRasterizationState(), PipelineMultisampleState(), depth, PipelineColorBlendState(), 0, 0, Vector<PipelineSpecializationConstant>(), p_cell, p_reserved);
}

RID RenderingDevice::_qualification_render_pipeline_create_internal(RID p_shader, FramebufferFormatID p_framebuffer_format, VertexFormatID p_vertex_format, RenderPrimitive p_render_primitive, const PipelineRasterizationState &p_rasterization_state, const PipelineMultisampleState &p_multisample_state, const PipelineDepthStencilState &p_depth_stencil_state, const PipelineColorBlendState &p_blend_state, BitField<PipelineDynamicStateFlags> p_dynamic_state_flags, uint32_t p_for_render_pass, const Vector<PipelineSpecializationConstant> &p_specialization_constants, GDExtensionDeviceExit::QualificationPhysicalReturn *p_cell, RID p_reserved, GDExtensionDeviceExit::ProductionUseCell *p_output_cell) {
	ERR_FAIL_COND_V(p_cell && p_output_cell, RID());
	ERR_FAIL_COND_V(native_immutable_resource_is_retiring(p_shader), RID());
	// Needs a shader.
	Shader *shader = shader_owner.get_or_null(p_shader);
	ERR_FAIL_NULL_V(shader, RID());
	ERR_FAIL_COND_V(shader->qualification_return && !p_cell, RID());
	ERR_FAIL_COND_V(p_cell && (!p_reserved.is_valid() || !shader->qualification_return || shader->qualification_return->bridge != p_cell->bridge || shader->qualification_return->child != p_cell->child || shader->qualification_return->use_closed.load() || shader->qualification_return->dependent_children.load() == UINT32_MAX), RID());
	ERR_FAIL_COND_V(p_cell && (!shader->set_formats.is_empty() || !shader->stage_bits.has_flag(RDD::PIPELINE_STAGE_FRAGMENT_SHADER_BIT)), RID());
	ERR_FAIL_COND_V_MSG(shader->pipeline_type != PIPELINE_TYPE_RASTERIZATION, RID(),
			"Only render shaders can be used in render pipelines");

	// Validate pre-raster shader. One of stages must be vertex shader or mesh shader (not implemented yet).
	ERR_FAIL_COND_V_MSG(!shader->stage_bits.has_flag(RDD::PIPELINE_STAGE_VERTEX_SHADER_BIT), RID(), "Pre-raster shader (vertex shader) is not provided for pipeline creation.");

	FramebufferFormat fb_format;
	{
		_THREAD_SAFE_METHOD_

		if (p_framebuffer_format == INVALID_ID) {
			// If nothing provided, use an empty one (no attachments).
			p_framebuffer_format = framebuffer_format_create(Vector<AttachmentFormat>());
		}
		ERR_FAIL_COND_V(!framebuffer_formats.has(p_framebuffer_format), RID());
		fb_format = framebuffer_formats[p_framebuffer_format];
	}

	// Validate shader vs. framebuffer.
	{
		ERR_FAIL_COND_V_MSG(p_for_render_pass >= uint32_t(fb_format.E->key().passes.size()), RID(), "Render pass requested for pipeline creation (" + itos(p_for_render_pass) + ") is out of bounds");
		const FramebufferPass &pass = fb_format.E->key().passes[p_for_render_pass];
		uint32_t output_mask = 0;
		for (int i = 0; i < pass.color_attachments.size(); i++) {
			if (pass.color_attachments[i] != ATTACHMENT_UNUSED) {
				output_mask |= 1 << i;
			}
		}
		ERR_FAIL_COND_V_MSG(shader->fragment_output_mask != output_mask, RID(),
				"Mismatch fragment shader output mask (" + itos(shader->fragment_output_mask) + ") and framebuffer color output mask (" + itos(output_mask) + ") when binding both in render pipeline.");
	}

	RDD::VertexFormatID driver_vertex_format;
	if (p_vertex_format != INVALID_ID) {
		// Uses vertices, else it does not.
		const VertexDescriptionCache *vd;
		{
			_THREAD_SAFE_METHOD_
			vd = vertex_formats.getptr(p_vertex_format);
		}
		ERR_FAIL_NULL_V(vd, RID());

		driver_vertex_format = vd->driver_id;

		// Validate with inputs.
		for (uint32_t i = 0; i < 64; i++) {
			if (!(shader->vertex_input_mask & ((uint64_t)1) << i)) {
				continue;
			}
			bool found = false;
			for (int j = 0; j < vd->vertex_formats.size(); j++) {
				if (vd->vertex_formats[j].location == i) {
					found = true;
					break;
				}
			}

			ERR_FAIL_COND_V_MSG(!found, RID(),
					"Shader vertex input location (" + itos(i) + ") not provided in vertex input description for pipeline creation.");
		}

	} else {
		ERR_FAIL_COND_V_MSG(shader->vertex_input_mask != 0, RID(),
				"Shader contains vertex inputs, but no vertex input description was provided for pipeline creation.");
	}

	ERR_FAIL_INDEX_V(p_render_primitive, RENDER_PRIMITIVE_MAX, RID());

	ERR_FAIL_INDEX_V(p_rasterization_state.cull_mode, 3, RID());

	if (p_multisample_state.sample_mask.size()) {
		// Use sample mask.
		ERR_FAIL_COND_V((int)TEXTURE_SAMPLES_COUNT[p_multisample_state.sample_count] != p_multisample_state.sample_mask.size(), RID());
	}

	ERR_FAIL_INDEX_V(p_depth_stencil_state.depth_compare_operator, COMPARE_OP_MAX, RID());

	ERR_FAIL_INDEX_V(p_depth_stencil_state.front_op.fail, STENCIL_OP_MAX, RID());
	ERR_FAIL_INDEX_V(p_depth_stencil_state.front_op.pass, STENCIL_OP_MAX, RID());
	ERR_FAIL_INDEX_V(p_depth_stencil_state.front_op.depth_fail, STENCIL_OP_MAX, RID());
	ERR_FAIL_INDEX_V(p_depth_stencil_state.front_op.compare, COMPARE_OP_MAX, RID());

	ERR_FAIL_INDEX_V(p_depth_stencil_state.back_op.fail, STENCIL_OP_MAX, RID());
	ERR_FAIL_INDEX_V(p_depth_stencil_state.back_op.pass, STENCIL_OP_MAX, RID());
	ERR_FAIL_INDEX_V(p_depth_stencil_state.back_op.depth_fail, STENCIL_OP_MAX, RID());
	ERR_FAIL_INDEX_V(p_depth_stencil_state.back_op.compare, COMPARE_OP_MAX, RID());

	ERR_FAIL_INDEX_V(p_blend_state.logic_op, LOGIC_OP_MAX, RID());

	const FramebufferPass &pass = fb_format.E->key().passes[p_for_render_pass];
	ERR_FAIL_COND_V(p_blend_state.attachments.size() < pass.color_attachments.size(), RID());
	for (int i = 0; i < pass.color_attachments.size(); i++) {
		if (pass.color_attachments[i] != ATTACHMENT_UNUSED) {
			ERR_FAIL_INDEX_V(p_blend_state.attachments[i].src_color_blend_factor, BLEND_FACTOR_MAX, RID());
			ERR_FAIL_INDEX_V(p_blend_state.attachments[i].dst_color_blend_factor, BLEND_FACTOR_MAX, RID());
			ERR_FAIL_INDEX_V(p_blend_state.attachments[i].color_blend_op, BLEND_OP_MAX, RID());

			ERR_FAIL_INDEX_V(p_blend_state.attachments[i].src_alpha_blend_factor, BLEND_FACTOR_MAX, RID());
			ERR_FAIL_INDEX_V(p_blend_state.attachments[i].dst_alpha_blend_factor, BLEND_FACTOR_MAX, RID());
			ERR_FAIL_INDEX_V(p_blend_state.attachments[i].alpha_blend_op, BLEND_OP_MAX, RID());
		}
	}

	for (int i = 0; i < shader->specialization_constants.size(); i++) {
		const ShaderSpecializationConstant &sc = shader->specialization_constants[i];
		for (int j = 0; j < p_specialization_constants.size(); j++) {
			const PipelineSpecializationConstant &psc = p_specialization_constants[j];
			if (psc.constant_id == sc.constant_id) {
				ERR_FAIL_COND_V_MSG(psc.type != sc.type, RID(), "Specialization constant provided for id (" + itos(sc.constant_id) + ") is of the wrong type.");
				break;
			}
		}
	}

	// Ordinary creation must own its RID before the driver can create a native
	// pipeline. A thread-safe RID owner can refuse when its finite arena is full.
	RID ordinary_reserved;
	if (!p_cell) {
		ordinary_reserved = render_pipeline_owner.allocate_rid();
		ERR_FAIL_COND_V(ordinary_reserved.is_null(), RID());
	}

	RenderPipeline pipeline;
	pipeline.production_disposal_uses = p_output_cell ? &p_output_cell->disposal : nullptr;
	pipeline.qualification_return = p_cell;
	pipeline.qualification_dependency = p_cell ? shader->qualification_return : nullptr;
	pipeline.driver_id = driver->render_pipeline_create(
			shader->driver_id,
			driver_vertex_format,
			p_render_primitive,
			p_rasterization_state,
			p_multisample_state,
			p_depth_stencil_state,
			p_blend_state,
			pass.color_attachments,
			p_dynamic_state_flags,
			fb_format.render_pass,
			p_for_render_pass,
			p_specialization_constants);

	if (!pipeline.driver_id && ordinary_reserved.is_valid()) {
		// RID_Owner::free requires an initialized slot, even when no driver
		// object was created. Return this ordinary reservation on both exits.
		render_pipeline_owner.initialize_rid(ordinary_reserved, RenderPipeline());
		render_pipeline_owner.free(ordinary_reserved);
	}

	// Don't print error when it's expected.
	if (unlikely(!pipeline.driver_id && driver->get_driver_workarounds().dont_print_on_render_pipeline_creation_failure)) {
		return RID();
	}

	ERR_FAIL_COND_V(!pipeline.driver_id, RID());

	if (pipeline_cache_enabled) {
		update_pipeline_cache();
	}

	pipeline.shader = p_shader;
	pipeline.shader_driver_id = shader->driver_id;
	pipeline.shader_layout_hash = shader->layout_hash;
	pipeline.set_formats = shader->set_formats;
	pipeline.push_constant_size = shader->push_constant_size;
	pipeline.stage_bits = shader->stage_bits;

	pipeline.validation.framebuffer_format = p_framebuffer_format;
	pipeline.validation.render_pass = p_for_render_pass;
	pipeline.validation.vertex_format = p_vertex_format;
	pipeline.validation.primitive = p_render_primitive;
#ifdef DEBUG_ENABLED
	pipeline.validation.dynamic_state = p_dynamic_state_flags;
	pipeline.validation.uses_restart_indices = p_render_primitive == RENDER_PRIMITIVE_TRIANGLE_STRIPS_WITH_RESTART_INDEX;

	static const uint32_t primitive_divisor[RENDER_PRIMITIVE_MAX] = {
		1, 2, 1, 1, 1, 3, 1, 1, 1, 1, 1
	};
	pipeline.validation.primitive_divisor = primitive_divisor[p_render_primitive];
	static const uint32_t primitive_minimum[RENDER_PRIMITIVE_MAX] = {
		1,
		2,
		2,
		2,
		2,
		3,
		3,
		3,
		3,
		3,
		1,
	};
	pipeline.validation.primitive_minimum = primitive_minimum[p_render_primitive];
#endif

	// Create ID to associate with this pipeline.
	RID id;
	if (p_cell) {
		id = p_reserved;
		render_pipeline_owner.initialize_rid(id, pipeline);
		pipeline.qualification_dependency->dependent_children.fetch_add(1);
	} else {
		id = ordinary_reserved;
		render_pipeline_owner.initialize_rid(id, pipeline);
	}
	if (p_output_cell) {
		// The exact original reserved cell exists before the driver allocation.
		// Publish its identity in the factory before any created RID escapes.
		auto &node = p_output_cell->disposal;
		node.rid = id.get_id();
		node.serial = id.get_id();
		node.native_driver_id = pipeline.driver_id.id;
		node.kind = 0;
		node.next = nullptr;
		node.state.store(1, std::memory_order_release);
		p_output_cell->output_role = GDExtensionDeviceExit::ProductionUseCell::OutputRole::DISPOSAL_MEMBER;
	}
	{
		_THREAD_SAFE_METHOD_

#ifdef DEV_ENABLED
		set_resource_name(id, "RID:" + itos(id.get_id()));
#endif
		// Now add all the dependencies.
		_add_dependency(id, p_shader);
	}

	return id;
}

bool RenderingDevice::render_pipeline_is_valid(RID p_pipeline) {
	_THREAD_SAFE_METHOD_

	return render_pipeline_owner.owns(p_pipeline);
}

RID RenderingDevice::compute_pipeline_create(RID p_shader, const Vector<PipelineSpecializationConstant> &p_specialization_constants) {
	return _qualification_pipeline_create(p_shader, p_specialization_constants, nullptr, RID());
}
RID RenderingDevice::_qualification_pipeline_create(RID p_shader, const Vector<PipelineSpecializationConstant> &p_specialization_constants, GDExtensionDeviceExit::QualificationPhysicalReturn *p_cell, RID p_reserved, GDExtensionDeviceExit::ProductionUseCell *p_output_cell) {
	ERR_FAIL_COND_V(p_cell && p_output_cell, RID());
	ERR_FAIL_COND_V(native_immutable_resource_is_retiring(p_shader), RID());
	Shader *shader;

	{
		_THREAD_SAFE_METHOD_

		// Needs a shader.
		shader = shader_owner.get_or_null(p_shader);
		ERR_FAIL_NULL_V(shader, RID());
		ERR_FAIL_COND_V(shader->qualification_return && !p_cell, RID());
		ERR_FAIL_COND_V(p_cell && (!shader->qualification_return || shader->qualification_return->bridge != p_cell->bridge || shader->qualification_return->child != p_cell->child), RID());

		ERR_FAIL_COND_V_MSG(shader->pipeline_type != PIPELINE_TYPE_COMPUTE, RID(),
				"Non-compute shaders can't be used in compute pipelines");
	}

	for (int i = 0; i < shader->specialization_constants.size(); i++) {
		const ShaderSpecializationConstant &sc = shader->specialization_constants[i];
		for (int j = 0; j < p_specialization_constants.size(); j++) {
			const PipelineSpecializationConstant &psc = p_specialization_constants[j];
			if (psc.constant_id == sc.constant_id) {
				ERR_FAIL_COND_V_MSG(psc.type != sc.type, RID(), "Specialization constant provided for id (" + itos(sc.constant_id) + ") is of the wrong type.");
				break;
			}
		}
	}

	ComputePipeline pipeline;
	pipeline.qualification_return = p_cell;
	pipeline.driver_id = driver->compute_pipeline_create(shader->driver_id, p_specialization_constants);
	ERR_FAIL_COND_V(!pipeline.driver_id, RID());

	if (pipeline_cache_enabled) {
		update_pipeline_cache();
	}

	pipeline.shader = p_shader;
	pipeline.shader_driver_id = shader->driver_id;
	pipeline.shader_layout_hash = shader->layout_hash;
	pipeline.set_formats = shader->set_formats;
	pipeline.push_constant_size = shader->push_constant_size;
	pipeline.local_group_size[0] = shader->compute_local_size[0];
	pipeline.local_group_size[1] = shader->compute_local_size[1];
	pipeline.local_group_size[2] = shader->compute_local_size[2];

	// Create ID to associate with this pipeline.
	RID id;
	if (p_cell) {
		CRASH_COND(!p_reserved.is_valid());
		id = p_reserved;
		compute_pipeline_owner.initialize_rid(id, pipeline);
	} else {
		id = compute_pipeline_owner.make_rid(pipeline);
	}
	{
		_THREAD_SAFE_METHOD_

#ifdef DEV_ENABLED
		set_resource_name(id, "RID:" + itos(id.get_id()));
#endif
		// Now add all the dependencies.
		_add_dependency(id, p_shader);
	}

	return id;
}

bool RenderingDevice::compute_pipeline_is_valid(RID p_pipeline) {
	_THREAD_SAFE_METHOD_

	return compute_pipeline_owner.owns(p_pipeline);
}

Error RenderingDevice::_raytracing_pipeline_create_sbt_buffer(RDD::RaytracingPipelineID p_raytracing_pipeline, uint32_t p_raygen_shader_count, uint32_t p_miss_shader_count, Buffer &r_sbt_buffer) {
	// Ray generation and miss shader group handles are placed next to each other in the same buffer.

	uint32_t shader_group_handle_size = driver->api_trait_get(RDD::API_TRAIT_SHADER_GROUP_HANDLE_SIZE);
	uint32_t shader_group_handle_alignment = driver->api_trait_get(RDD::API_TRAIT_SHADER_GROUP_HANDLE_ALIGNMENT);
	uint32_t shader_group_base_alignment = driver->api_trait_get(RDD::API_TRAIT_SHADER_GROUP_BASE_ALIGNMENT);

	uint32_t shader_group_stride = STEPIFY(shader_group_handle_size, shader_group_handle_alignment);
	uint32_t raygen_shader_group_stride = STEPIFY(shader_group_stride, shader_group_base_alignment);

	// Make sure to reserve a full base-aligned slot for every raygen shader.
	uint32_t raygen_sbt_size = p_raygen_shader_count * raygen_shader_group_stride;
	uint32_t miss_sbt_offset = STEPIFY(raygen_sbt_size, shader_group_base_alignment);
	// Reserve handle-aligned slots for miss shaders.
	uint32_t miss_sbt_size = p_miss_shader_count * shader_group_stride;

	r_sbt_buffer.size = miss_sbt_offset + miss_sbt_size;
	r_sbt_buffer.driver_id = driver->buffer_create(r_sbt_buffer.size, RDD::BUFFER_USAGE_TRANSFER_TO_BIT | RDD::BUFFER_USAGE_SHADER_BINDING_TABLE_BIT | RDD::BUFFER_USAGE_DEVICE_ADDRESS_BIT, RDD::MEMORY_ALLOCATION_TYPE_GPU, frames_drawn);
	ERR_FAIL_COND_V(!r_sbt_buffer.driver_id, ERR_CANT_CREATE);

	thread_local LocalVector<uint8_t> sbt_data;
	sbt_data.resize(r_sbt_buffer.size);

	thread_local LocalVector<uint32_t> raygen_shader_group_indices;
	raygen_shader_group_indices.resize(p_raygen_shader_count);
	for (uint32_t i = 0; i < p_raygen_shader_count; i++) {
		raygen_shader_group_indices[i] = i;
	}
	bool success = driver->raytracing_pipeline_get_shader_group_handles(p_raytracing_pipeline, 0, VectorView<uint32_t>(raygen_shader_group_indices), sbt_data.ptr(), raygen_shader_group_stride);
	if (unlikely(!success)) {
		driver->buffer_free(r_sbt_buffer.driver_id);
		ERR_FAIL_V(ERR_CANT_CREATE);
	}

	thread_local LocalVector<uint32_t> miss_shader_group_indices;
	miss_shader_group_indices.resize(p_miss_shader_count);
	for (uint32_t i = 0; i < p_miss_shader_count; i++) {
		miss_shader_group_indices[i] = i;
	}
	success = driver->raytracing_pipeline_get_shader_group_handles(p_raytracing_pipeline, p_raygen_shader_count, VectorView<uint32_t>(miss_shader_group_indices), sbt_data.ptr() + miss_sbt_offset, shader_group_stride);
	if (unlikely(!success)) {
		driver->buffer_free(r_sbt_buffer.driver_id);
		ERR_FAIL_V(ERR_CANT_CREATE);
	}

	Error err = _buffer_initialize(&r_sbt_buffer, sbt_data);
	if (unlikely(err != OK)) {
		driver->buffer_free(r_sbt_buffer.driver_id);
		ERR_FAIL_V(err);
	}

	_THREAD_SAFE_LOCK_
	buffer_memory += r_sbt_buffer.size;
	_THREAD_SAFE_UNLOCK_

	return OK;
}

RID RenderingDevice::raytracing_pipeline_create(Span<PipelineShader> p_raygen_shaders, Span<PipelineShader> p_miss_shaders, Span<HitGroup> p_hit_groups, uint32_t p_max_trace_recursion_depth) {
	ERR_FAIL_COND_V_MSG(!has_feature(SUPPORTS_RAYTRACING_PIPELINE) && !has_feature(SUPPORTS_RAY_QUERY), RID(), "The current rendering device has neither raytracing pipeline nor ray query support.");

	struct PipelineShaderKey : PipelineShader {
		ShaderStage shader_stage = {};

		uint32_t hash() const {
			uint32_t h = hash_murmur3_one_64(shader.get_id());
			h = hash_murmur3_one_32(shader_stage, h);

			for (uint32_t i = 0; i < specialization_constants.size(); i++) {
				const PipelineSpecializationConstant &specialization_constant = specialization_constants[i];
				h = hash_murmur3_one_32(specialization_constant.type, h);
				h = hash_murmur3_one_32(specialization_constant.constant_id, h);
				h = hash_murmur3_one_32(specialization_constant.int_value, h);
			}

			return hash_fmix32(h);
		}

		bool operator==(const PipelineShaderKey &p_rhs) const {
			if (shader != p_rhs.shader) {
				return false;
			}
			if (shader_stage != p_rhs.shader_stage) {
				return false;
			}
			if (specialization_constants != p_rhs.specialization_constants) {
				return false;
			}
			return true;
		}
	};

	thread_local LocalVector<RDD::PipelineShader> shaders;
	thread_local AHashMap<PipelineShaderKey, uint32_t> shader_indices;
	thread_local LocalVector<uint32_t> raygen_and_miss_shader_indices;
	thread_local LocalVector<RDD::HitGroup> hit_groups;
	shaders.clear();
	shader_indices.clear();
	raygen_and_miss_shader_indices.clear();
	hit_groups.clear();

	RID layout_defining_shader_rid;
	Shader *layout_defining_shader = nullptr;

	auto _get_shader_index = [&](const PipelineShader &p_shader, ShaderStage p_shader_stage) -> uint32_t {
		PipelineShaderKey shader_key;
		shader_key.shader = p_shader.shader;
		shader_key.specialization_constants = p_shader.specialization_constants;
		shader_key.shader_stage = p_shader_stage;

		uint32_t &index = shader_indices[shader_key];
		if (!index) {
			Shader *shader = shader_owner.get_or_null(p_shader.shader);
			ERR_FAIL_NULL_V(shader, UINT32_MAX);
			ERR_FAIL_COND_V_MSG(shader->pipeline_type != PIPELINE_TYPE_RAYTRACING, UINT32_MAX, "Only raytracing shaders can be used in raytracing pipelines.");
			ERR_FAIL_COND_V_MSG(!(shader->stages_bits & (1 << p_shader_stage)), UINT32_MAX, "Shader does not contain the required stage.");

			for (int i = 0; i < shader->specialization_constants.size(); i++) {
				const ShaderSpecializationConstant &sc = shader->specialization_constants[i];
				for (int j = 0; j < p_shader.specialization_constants.size(); j++) {
					const PipelineSpecializationConstant &psc = p_shader.specialization_constants[j];
					if (psc.constant_id == sc.constant_id) {
						ERR_FAIL_COND_V_MSG(psc.type != sc.type, UINT32_MAX, "Specialization constant provided for id (" + itos(sc.constant_id) + ") is of the wrong type.");
						break;
					}
				}
			}

			if (layout_defining_shader) {
				bool compatible_layout = true;

				for (uint32_t i = 0; i < MIN(layout_defining_shader->set_formats.size(), shader->set_formats.size()); i++) {
					if (layout_defining_shader->set_formats[i] != shader->set_formats[i]) {
						compatible_layout = false;
						break;
					}
				}

				if (layout_defining_shader->push_constant_size != shader->push_constant_size || layout_defining_shader->push_constant_stages != shader->push_constant_stages) {
					compatible_layout = false;
				}

				ERR_FAIL_COND_V_MSG(!compatible_layout, UINT32_MAX, "All shaders must have compatible layouts.");
			}

			if (!layout_defining_shader || layout_defining_shader->set_formats.size() < shader->set_formats.size()) {
				layout_defining_shader_rid = p_shader.shader;
				layout_defining_shader = shader;
			}

			index = shaders.size() + 1;

			RDD::PipelineShader rdd_shader;
			rdd_shader.shader = shader->driver_id;
			rdd_shader.shader_stage = p_shader_stage;
			rdd_shader.specialization_constants = p_shader.specialization_constants;
			shaders.push_back(rdd_shader);
		}

		return index - 1;
	};

	for (uint32_t i = 0; i < p_raygen_shaders.size(); i++) {
		uint32_t raygen_shader_index = _get_shader_index(p_raygen_shaders[i], SHADER_STAGE_RAYGEN);
		ERR_FAIL_COND_V(raygen_shader_index == UINT32_MAX, RID());

		raygen_and_miss_shader_indices.push_back(raygen_shader_index);
	}

	for (uint32_t i = 0; i < p_miss_shaders.size(); i++) {
		uint32_t miss_shader_index = _get_shader_index(p_miss_shaders[i], SHADER_STAGE_MISS);
		ERR_FAIL_COND_V(miss_shader_index == UINT32_MAX, RID());

		raygen_and_miss_shader_indices.push_back(miss_shader_index);
	}

	for (uint32_t i = 0; i < p_hit_groups.size(); i++) {
		const HitGroup &hit_group = p_hit_groups[i];

		RDD::HitGroup rdd_hit_group;

		if (hit_group.closest_hit_shader.is_valid()) {
			rdd_hit_group.closest_hit_shader_index = _get_shader_index(hit_group.closest_hit_shader, SHADER_STAGE_CLOSEST_HIT);
			ERR_FAIL_COND_V(rdd_hit_group.closest_hit_shader_index == UINT32_MAX, RID());
		}

		if (hit_group.any_hit_shader.is_valid()) {
			rdd_hit_group.any_hit_shader_index = _get_shader_index(hit_group.any_hit_shader, SHADER_STAGE_ANY_HIT);
			ERR_FAIL_COND_V(rdd_hit_group.any_hit_shader_index == UINT32_MAX, RID());
		}

		if (hit_group.intersection_shader.is_valid()) {
			rdd_hit_group.intersection_shader_index = _get_shader_index(hit_group.intersection_shader, SHADER_STAGE_INTERSECTION);
			ERR_FAIL_COND_V(rdd_hit_group.intersection_shader_index == UINT32_MAX, RID());
		}

		hit_groups.push_back(rdd_hit_group);
	}

	ERR_FAIL_NULL_V(layout_defining_shader, RID());

	RaytracingPipeline pipeline;
	pipeline.driver_id = driver->raytracing_pipeline_create(
			shaders,
			VectorView(raygen_and_miss_shader_indices.ptr(), p_raygen_shaders.size()),
			VectorView(raygen_and_miss_shader_indices.ptr() + p_raygen_shaders.size(), p_miss_shaders.size()),
			hit_groups,
			p_max_trace_recursion_depth,
			layout_defining_shader->driver_id);

	ERR_FAIL_COND_V(!pipeline.driver_id, RID());

	if (pipeline_cache_enabled) {
		update_pipeline_cache();
	}

	pipeline.layout_defining_shader = layout_defining_shader_rid;
	pipeline.layout_defining_shader_driver_id = layout_defining_shader->driver_id;
	pipeline.layout_defining_shader_layout_hash = layout_defining_shader->layout_hash;
	pipeline.set_formats = layout_defining_shader->set_formats;
	pipeline.push_constant_size = layout_defining_shader->push_constant_size;

	Error err = _raytracing_pipeline_create_sbt_buffer(pipeline.driver_id, p_raygen_shaders.size(), p_miss_shaders.size(), pipeline.sbt_buffer);
	if (unlikely(err != OK)) {
		driver->raytracing_pipeline_free(pipeline.driver_id);
		ERR_FAIL_V(RID());
	}

	pipeline.raygen_shader_count = p_raygen_shaders.size();
	pipeline.miss_shader_count = p_miss_shaders.size();
	pipeline.hit_group_count = p_hit_groups.size();

	// Create ID to associate with this pipeline.
	RID id = raytracing_pipeline_owner.make_rid(pipeline);
#ifdef DEV_ENABLED
	set_resource_name(id, "RID:" + itos(id.get_id()));
#endif

	// Now add all the dependencies.
	for (uint32_t i = 0; i < p_raygen_shaders.size(); i++) {
		_add_dependency(id, p_raygen_shaders[i].shader);
	}
	for (uint32_t i = 0; i < p_miss_shaders.size(); i++) {
		_add_dependency(id, p_miss_shaders[i].shader);
	}
	for (uint32_t i = 0; i < p_hit_groups.size(); i++) {
		const HitGroup &hit_group = p_hit_groups[i];

		if (hit_group.closest_hit_shader.is_valid()) {
			_add_dependency(id, hit_group.closest_hit_shader.shader);
		}

		if (hit_group.any_hit_shader.is_valid()) {
			_add_dependency(id, hit_group.any_hit_shader.shader);
		}

		if (hit_group.intersection_shader.is_valid()) {
			_add_dependency(id, hit_group.intersection_shader.shader);
		}
	}

	return id;
}

bool RenderingDevice::raytracing_pipeline_is_valid(RID p_pipeline) {
	return raytracing_pipeline_owner.owns(p_pipeline);
}

/****************/
/**** SCREEN ****/
/****************/

uint32_t RenderingDevice::_get_swap_chain_desired_count() const {
	return MAX(2U, uint32_t(GLOBAL_GET_CACHED(uint32_t, "rendering/rendering_device/vsync/swapchain_image_count")));
}

Error RenderingDevice::screen_create(DisplayServerEnums::WindowID p_screen) {
	_THREAD_SAFE_METHOD_

	RenderingContextDriver::SurfaceID surface = context->surface_get_from_window(p_screen);
	ERR_FAIL_COND_V_MSG(surface == 0, ERR_CANT_CREATE, "A surface was not created for the screen.");

	HashMap<DisplayServerEnums::WindowID, RDD::SwapChainID>::ConstIterator it = screen_swap_chains.find(p_screen);
	ERR_FAIL_COND_V_MSG(it != screen_swap_chains.end(), ERR_CANT_CREATE, "A swap chain was already created for the screen.");

	RDD::SwapChainID swap_chain = driver->swap_chain_create(surface);
	ERR_FAIL_COND_V_MSG(swap_chain.id == 0, ERR_CANT_CREATE, "Unable to create swap chain.");

	screen_swap_chains[p_screen] = swap_chain;

	return OK;
}

Error RenderingDevice::screen_prepare_for_drawing(DisplayServerEnums::WindowID p_screen) {
	_THREAD_SAFE_METHOD_
	const Error recording_error = _recording_error();
	if (recording_error != OK) {
		return recording_error; // Preserve the suspended frame before acquire or presentation.
	}

	// After submitting work, acquire the swapchain image(s).
	HashMap<DisplayServerEnums::WindowID, RDD::SwapChainID>::ConstIterator it = screen_swap_chains.find(p_screen);
	ERR_FAIL_COND_V_MSG(it == screen_swap_chains.end(), ERR_CANT_CREATE, "A swap chain was not created for the screen.");

	// Erase the framebuffer corresponding to this screen from the map in case any of the operations fail.
	screen_framebuffers.erase(p_screen);

	// If this frame has already queued this swap chain for presentation, we present it and remove it from the pending list.
	uint32_t to_present_index = 0;
	while (to_present_index < frames[frame].swap_chains_to_present.size()) {
		if (frames[frame].swap_chains_to_present[to_present_index] == it->value) {
#if defined(TESTS_ENABLED) && defined(MODULE_SKINNED_MESH_CAPTURE_TESTS_ENABLED)
			const Error skinned_mesh_capture_test_driver_result =
#endif
					driver->command_queue_execute_and_present(present_queue, {}, {}, {}, {}, it->value);
#if defined(TESTS_ENABLED) && defined(MODULE_SKINNED_MESH_CAPTURE_TESTS_ENABLED)
			RenderCaptureTestHooks::observe_driver_result(uint64_t(get_instance_id()), 1, 0, skinned_mesh_capture_test_driver_result);
			if (skinned_mesh_capture_test_driver_result != OK) {
				capture_test_coverage_failed.store(true);
			}
#endif
			frames[frame].swap_chains_to_present.remove_at(to_present_index);
		} else {
			to_present_index++;
		}
	}

	bool resize_required = false;
	RDD::FramebufferID framebuffer = driver->swap_chain_acquire_framebuffer(main_queue, it->value, resize_required);
	if (resize_required) {
		// Flush everything so nothing can be using the swap chain before resizing it.
		const Error wait_error = _flush_and_stall_for_all_frames();
		if (wait_error != OK) {
			return wait_error;
		}

		Error err = driver->swap_chain_resize(main_queue, it->value, _get_swap_chain_desired_count());
		if (err != OK) {
			// Resize is allowed to fail silently because the window can be minimized.
			return err;
		}

		framebuffer = driver->swap_chain_acquire_framebuffer(main_queue, it->value, resize_required);
	}

	if (framebuffer.id == 0) {
		// Some drivers like NVIDIA are fast enough to invalidate the swap chain between resizing and acquisition (GH-94104).
		// This typically occurs during continuous window resizing operations, especially if done quickly.
		// Allow this to fail silently since it has no visual consequences.
		return ERR_CANT_CREATE;
	}

	// Store the framebuffer that will be used next to draw to this screen.
	screen_framebuffers[p_screen] = framebuffer;
	frames[frame].swap_chains_to_present.push_back(it->value);

	return OK;
}

int RenderingDevice::screen_get_width(DisplayServerEnums::WindowID p_screen) const {
	_THREAD_SAFE_METHOD_

	RenderingContextDriver::SurfaceID surface = context->surface_get_from_window(p_screen);
	ERR_FAIL_COND_V_MSG(surface == 0, 0, "A surface was not created for the screen.");
	return context->surface_get_width(surface);
}

int RenderingDevice::screen_get_height(DisplayServerEnums::WindowID p_screen) const {
	_THREAD_SAFE_METHOD_

	RenderingContextDriver::SurfaceID surface = context->surface_get_from_window(p_screen);
	ERR_FAIL_COND_V_MSG(surface == 0, 0, "A surface was not created for the screen.");
	return context->surface_get_height(surface);
}

int RenderingDevice::screen_get_pre_rotation_degrees(DisplayServerEnums::WindowID p_screen) const {
	_THREAD_SAFE_METHOD_

	HashMap<DisplayServerEnums::WindowID, RDD::SwapChainID>::ConstIterator it = screen_swap_chains.find(p_screen);
	ERR_FAIL_COND_V_MSG(it == screen_swap_chains.end(), ERR_CANT_CREATE, "A swap chain was not created for the screen.");

	return driver->swap_chain_get_pre_rotation_degrees(it->value);
}

RenderingDevice::FramebufferFormatID RenderingDevice::screen_get_framebuffer_format(DisplayServerEnums::WindowID p_screen) const {
	_THREAD_SAFE_METHOD_

	HashMap<DisplayServerEnums::WindowID, RDD::SwapChainID>::ConstIterator it = screen_swap_chains.find(p_screen);
	ERR_FAIL_COND_V_MSG(it == screen_swap_chains.end(), INVALID_ID, "Screen was never prepared.");

	DataFormat format = driver->swap_chain_get_format(it->value);
	ERR_FAIL_COND_V(format == DATA_FORMAT_MAX, INVALID_ID);

	AttachmentFormat attachment;
	attachment.format = format;
	attachment.samples = TEXTURE_SAMPLES_1;
	attachment.usage_flags = TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;
	Vector<AttachmentFormat> screen_attachment;
	screen_attachment.push_back(attachment);
	return const_cast<RenderingDevice *>(this)->framebuffer_format_create(screen_attachment);
}

RenderingDevice::ColorSpace RenderingDevice::screen_get_color_space(DisplayServerEnums::WindowID p_screen) const {
	_THREAD_SAFE_METHOD_

	HashMap<DisplayServerEnums::WindowID, RDD::SwapChainID>::ConstIterator it = screen_swap_chains.find(p_screen);
	ERR_FAIL_COND_V_MSG(it == screen_swap_chains.end(), COLOR_SPACE_MAX, "Screen was never prepared.");

	ColorSpace color_space = driver->swap_chain_get_color_space(it->value);
	ERR_FAIL_COND_V_MSG(color_space == COLOR_SPACE_MAX, COLOR_SPACE_MAX, "Unknown color space.");
	return color_space;
}

bool RenderingDevice::screen_get_hdr_output_supported(DisplayServerEnums::WindowID p_screen) const {
	_THREAD_SAFE_METHOD_

	HashMap<DisplayServerEnums::WindowID, RDD::SwapChainID>::ConstIterator it = screen_swap_chains.find(p_screen);
	ERR_FAIL_COND_V_MSG(it == screen_swap_chains.end(), false, "Screen was never prepared.");

	return driver->swap_chain_get_hdr_output_supported(it->value);
}

Error RenderingDevice::screen_free(DisplayServerEnums::WindowID p_screen) {
	_THREAD_SAFE_METHOD_

	HashMap<DisplayServerEnums::WindowID, RDD::SwapChainID>::ConstIterator it = screen_swap_chains.find(p_screen);
	ERR_FAIL_COND_V_MSG(it == screen_swap_chains.end(), FAILED, "Screen was never created.");
	if (native_finalization_returned) {
		// The actual RT finalizer already returned from this exact swapchain's
		// driver free. Consume the retained original screen owner acknowledgment.
		ERR_FAIL_COND_V_MSG(it->value, ERR_INVALID_DATA, "Finalized screen still has an unresolved native swapchain.");
		screen_framebuffers.erase(p_screen);
		screen_swap_chains.erase(p_screen);
		return OK;
	}

	// Flush everything so nothing can be using the swap chain before erasing it.
	const Error wait_error = _flush_and_stall_for_all_frames();
	if (wait_error != OK) {
		return wait_error;
	}

	const DisplayServerEnums::WindowID screen = it->key;
	const RDD::SwapChainID swap_chain = it->value;
	driver->swap_chain_free(swap_chain);
	screen_framebuffers.erase(screen);
	screen_swap_chains.erase(screen);

	return OK;
}

/*******************/
/**** DRAW LIST ****/
/*******************/

RenderingDevice::DrawListID RenderingDevice::draw_list_begin_for_screen(DisplayServerEnums::WindowID p_screen, const Color &p_clear_color) {
	ERR_RENDER_THREAD_GUARD_V(INVALID_ID);
	ERR_FAIL_COND_V_MSG(_recording_error() != OK, INVALID_ID, "Recording is suspended after a frame fence wait failure.");

	ERR_FAIL_COND_V_MSG(draw_list.active, INVALID_ID, "Only one draw list can be active at the same time.");
	ERR_FAIL_COND_V_MSG(compute_list.active, INVALID_ID, "Only one draw/compute list can be active at the same time.");
	ERR_FAIL_COND_V_MSG(raytracing_list.active, INVALID_ID, "Only one draw/raytracing list can be active at the same time.");

	if (split_swapchain_into_its_own_cmd_buffer) {
		const Error admission = _qualification_admit_extra_main(1);
		if (admission != OK) {
			return INVALID_ID;
		}
	}
	RenderingContextDriver::SurfaceID surface = context->surface_get_from_window(p_screen);
	HashMap<DisplayServerEnums::WindowID, RDD::SwapChainID>::ConstIterator sc_it = screen_swap_chains.find(p_screen);
	HashMap<DisplayServerEnums::WindowID, RDD::FramebufferID>::ConstIterator fb_it = screen_framebuffers.find(p_screen);
	ERR_FAIL_COND_V_MSG(surface == 0, 0, "A surface was not created for the screen.");
	ERR_FAIL_COND_V_MSG(sc_it == screen_swap_chains.end(), INVALID_ID, "Screen was never prepared.");
	ERR_FAIL_COND_V_MSG(fb_it == screen_framebuffers.end(), INVALID_ID, "Framebuffer was never prepared.");
#ifdef TESTS_ENABLED
	if (test_main_screen_prepared && test_main_screen_epoch && p_screen == DisplayServerEnums::MAIN_WINDOW_ID) {
		bool pending_original_swapchain = false;
		for (auto pending : frames[frame].swap_chains_to_present) {
			pending_original_swapchain = pending_original_swapchain || pending == sc_it->value;
		}
		ERR_FAIL_COND_V_MSG(!pending_original_swapchain || frames[frame].qualification_epoch != test_main_screen_epoch, INVALID_ID, "TESTS-only acquired screen ownership changed before fixture draw.");
		print_line(vformat("K03_TEST_MAIN_SCREEN_OWNERSHIP_VALIDATED frame_epoch=%d pending_original_swapchain=1", test_main_screen_epoch));
		// This assertion belongs to the first fixture draw only. Subsequent
		// ordinary renderer frames retain their own original acquire path.
		test_main_screen_epoch = 0;
	}
#endif

	Rect2i viewport = Rect2i(0, 0, context->surface_get_width(surface), context->surface_get_height(surface));

	_draw_list_start(viewport);
	draw_list_framebuffer_format = screen_get_framebuffer_format(p_screen);
	draw_list_subpass_count = 1;

	RDD::RenderPassClearValue clear_value;
	clear_value.color = p_clear_color;

	RDD::RenderPassID render_pass = driver->swap_chain_get_render_pass(sc_it->value);
	draw_list.begin_handle = draw_graph.add_draw_list_begin(render_pass, fb_it->value, viewport, RDG::ATTACHMENT_OPERATION_CLEAR, clear_value, RDD::PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, RDD::BreadcrumbMarker::BLIT_PASS, split_swapchain_into_its_own_cmd_buffer);

	draw_graph.add_draw_list_set_viewport(viewport);
	draw_graph.add_draw_list_set_scissor(viewport);

	return int64_t(ID_TYPE_DRAW_LIST) << ID_BASE_SHIFT;
}

RenderingDevice::DrawListID RenderingDevice::_draw_list_begin_bind(RID p_framebuffer, BitField<DrawFlags> p_draw_flags, const Vector<Color> &p_clear_color_values, float p_clear_depth_value, uint32_t p_clear_stencil_value, const Rect2 &p_region, uint32_t p_breadcrumb) {
	return draw_list_begin(p_framebuffer, p_draw_flags, p_clear_color_values, p_clear_depth_value, p_clear_stencil_value, p_region, p_breadcrumb);
}

RenderingDevice::DrawListID RenderingDevice::draw_list_begin(RID p_framebuffer, BitField<DrawFlags> p_draw_flags, VectorView<Color> p_clear_color_values, float p_clear_depth_value, uint32_t p_clear_stencil_value, const Rect2 &p_region, uint32_t p_breadcrumb) {
	return _draw_list_begin_recorded(p_framebuffer, p_draw_flags, p_clear_color_values, p_clear_depth_value, p_clear_stencil_value, p_region, p_breadcrumb, false).list;
}

RenderingDevice::DrawListBeginResult RenderingDevice::_draw_list_begin_recorded(RID p_framebuffer, uint32_t p_draw_flags_bits, VectorView<Color> p_clear_color_values, float p_clear_depth_value, uint32_t p_clear_stencil_value, const Rect2 &p_region, uint32_t p_breadcrumb, bool p_capture) {
	DrawListBeginResult result;
	const BitField<DrawFlags> p_draw_flags = p_draw_flags_bits;
	ERR_RENDER_THREAD_GUARD_V(result.failure(ERR_UNAVAILABLE));
	ERR_FAIL_COND_V_MSG(_recording_error() != OK, result.failure(_recording_error()), "Recording is suspended after a frame fence wait failure.");

	ERR_FAIL_COND_V_MSG(draw_list.active, result.failure(ERR_INVALID_PARAMETER), "Only one draw list can be active at the same time.");

	Framebuffer *framebuffer = framebuffer_owner.get_or_null(p_framebuffer);
	ERR_FAIL_NULL_V(framebuffer, result.failure(ERR_INVALID_PARAMETER));
	ERR_FAIL_COND_V(framebuffer->qualification_return && ((!qualification_recording && !_production_resource_permitted(p_framebuffer)) || framebuffer->qualification_return->use_closed.load()), result.failure(ERR_UNAUTHORIZED));

	if (p_capture) {
		ERR_FAIL_COND_V(framebuffer->texture_ids.size() != 1 || !p_draw_flags.has_flag(DRAW_CLEAR_DEPTH), result.failure(ERR_UNAVAILABLE));
		const Texture *target = texture_owner.get_or_null(framebuffer->texture_ids[0]);
		ERR_FAIL_NULL_V(target, result.failure(ERR_INVALID_PARAMETER));
		ERR_FAIL_COND_V(target->format != DATA_FORMAT_D32_SFLOAT || target->owner.is_valid() || target->shared_fallback || target->transfer_worker_index >= 0, result.failure(ERR_UNAVAILABLE));
	}
	const uint32_t preparation_begin = draw_graph.qualification_command_count();

	const FramebufferFormatKey *framebuffer_key;
	{
		_THREAD_SAFE_METHOD_
		framebuffer_key = &framebuffer_formats[framebuffer->format_id].E->key();
	}

	Point2i viewport_offset;
	Point2i viewport_size = framebuffer->size;

	if (p_region != Rect2() && p_region != Rect2(Vector2(), viewport_size)) { // Check custom region.
		Rect2i viewport(viewport_offset, viewport_size);
		Rect2i regioni = p_region;
		if (!((regioni.position.x >= viewport.position.x) && (regioni.position.y >= viewport.position.y) &&
					((regioni.position.x + regioni.size.x) <= (viewport.position.x + viewport.size.x)) &&
					((regioni.position.y + regioni.size.y) <= (viewport.position.y + viewport.size.y)))) {
			ERR_FAIL_V_MSG(result.failure(ERR_INVALID_PARAMETER), "When supplying a custom region, it must be contained within the framebuffer rectangle");
		}

		viewport_offset = regioni.position;
		viewport_size = regioni.size;
	}

	thread_local LocalVector<RDG::AttachmentOperation> operations;
	thread_local LocalVector<RDD::RenderPassClearValue> clear_values;
	thread_local LocalVector<RDG::ResourceTracker *> resource_trackers;
	thread_local LocalVector<RDG::ResourceUsage> resource_usages;
	BitField<RDD::PipelineStageBits> stages = {};
	operations.resize(framebuffer->texture_ids.size());
	clear_values.resize(framebuffer->texture_ids.size());
	resource_trackers.clear();
	resource_usages.clear();
	stages.clear();

	uint32_t color_index = 0;
	for (int i = 0; i < framebuffer->texture_ids.size(); i++) {
		RID texture_rid = framebuffer->texture_ids[i];
		Texture *texture = texture_owner.get_or_null(texture_rid);
		if (texture == nullptr) {
			operations[i] = RDG::ATTACHMENT_OPERATION_DEFAULT;
			clear_values[i] = RDD::RenderPassClearValue();
			continue;
		}

		// Clear the texture if the driver requires it during its first use.
		{
			const Error dependency_error = _texture_check_pending_clear(texture_rid, texture);
			if (dependency_error != OK) {
				return result.failure(dependency_error);
			}
		}

		// Indicate the texture will get modified for the shared texture fallback.
		_texture_update_shared_fallback(texture_rid, texture, true);

		RDG::AttachmentOperation operation = RDG::ATTACHMENT_OPERATION_DEFAULT;
		RDD::RenderPassClearValue clear_value;
		if (framebuffer_key->vrs_attachment == i && (texture->usage_flags & TEXTURE_USAGE_VRS_ATTACHMENT_BIT)) {
			resource_trackers.push_back(texture->draw_tracker);
			resource_usages.push_back(_vrs_usage_from_method(framebuffer_key->vrs_method));
			stages.set_flag(_vrs_stages_from_method(framebuffer_key->vrs_method));
		} else if (texture->usage_flags & TEXTURE_USAGE_COLOR_ATTACHMENT_BIT) {
			if (p_draw_flags.has_flag(DrawFlags(DRAW_CLEAR_COLOR_0 << color_index))) {
				ERR_FAIL_COND_V_MSG(color_index >= p_clear_color_values.size(), result.failure(ERR_INVALID_PARAMETER), vformat("Color texture (%d) was specified to be cleared but no color value was provided.", color_index));
				operation = RDG::ATTACHMENT_OPERATION_CLEAR;
				clear_value.color = p_clear_color_values[color_index];
			} else if (p_draw_flags.has_flag(DrawFlags(DRAW_IGNORE_COLOR_0 << color_index))) {
				operation = RDG::ATTACHMENT_OPERATION_IGNORE;
			}

			resource_trackers.push_back(texture->draw_tracker);
			resource_usages.push_back(RDG::RESOURCE_USAGE_ATTACHMENT_COLOR_READ_WRITE);
			stages.set_flag(RDD::PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
			color_index++;
		} else if (texture->usage_flags & (TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | TEXTURE_USAGE_DEPTH_RESOLVE_ATTACHMENT_BIT)) {
			if (p_draw_flags.has_flag(DRAW_CLEAR_DEPTH) || p_draw_flags.has_flag(DRAW_CLEAR_STENCIL)) {
				operation = RDG::ATTACHMENT_OPERATION_CLEAR;
				clear_value.depth = p_clear_depth_value;
				clear_value.stencil = p_clear_stencil_value;
			} else if (p_draw_flags.has_flag(DRAW_IGNORE_DEPTH) || p_draw_flags.has_flag(DRAW_IGNORE_STENCIL)) {
				operation = RDG::ATTACHMENT_OPERATION_IGNORE;
			}

			resource_trackers.push_back(texture->draw_tracker);
			resource_usages.push_back(RDG::RESOURCE_USAGE_ATTACHMENT_DEPTH_STENCIL_READ_WRITE);
			stages.set_flag(RDD::PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);
			stages.set_flag(RDD::PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
		}

		operations[i] = operation;
		clear_values[i] = clear_value;
	}

	result.begin_handle = draw_graph.add_draw_list_begin(framebuffer->framebuffer_cache, Rect2i(viewport_offset, viewport_size), operations, clear_values, stages, p_breadcrumb);
	result.effect = NativeAppendEffect::NativePrefixOnly;
	if (p_capture) {
		const uint32_t preparation_end = draw_graph.qualification_command_count();
		for (uint32_t i = preparation_begin; i < preparation_end; ++i) {
			const auto handle = draw_graph.capture_command_record(i);
			if (result.preparation_count == 1 || !handle.is_valid()) {
				return result.failure(ERR_INVALID_DATA);
			}
			result.preparation_handles[result.preparation_count++] = handle;
		}
		result.clear_handle = draw_graph.capture_draw_list_clear(0);
	}
	draw_graph.add_draw_list_usages(resource_trackers, resource_usages);

	// Mark textures as bound.
	draw_list_bound_textures.clear();

	for (int i = 0; i < framebuffer->texture_ids.size(); i++) {
		Texture *texture = texture_owner.get_or_null(framebuffer->texture_ids[i]);
		if (texture == nullptr) {
			continue;
		}

		texture->bound = true;
		draw_list_bound_textures.push_back(framebuffer->texture_ids[i]);
	}

	_draw_list_start(Rect2i(viewport_offset, viewport_size));
	draw_list.begin_handle = result.begin_handle;
	draw_list_framebuffer_format = framebuffer->format_id;
	draw_list_current_subpass = 0;
	draw_list_subpass_count = framebuffer_key->passes.size();

	Rect2i viewport_rect(viewport_offset, viewport_size);
	result.prefix_handles[result.prefix_count++] = draw_graph.add_draw_list_set_viewport(viewport_rect);
	result.prefix_handles[result.prefix_count++] = draw_graph.add_draw_list_set_scissor(viewport_rect);

	result.list = int64_t(ID_TYPE_DRAW_LIST) << ID_BASE_SHIFT;
	result.effect = NativeAppendEffect::Recorded;
	result.capture_complete = p_capture;
	return result;
}

#ifndef DISABLE_DEPRECATED
Error RenderingDevice::draw_list_begin_split(RID p_framebuffer, uint32_t p_splits, DrawListID *r_split_ids, InitialAction p_initial_color_action, FinalAction p_final_color_action, InitialAction p_initial_depth_action, FinalAction p_final_depth_action, const Vector<Color> &p_clear_color_values, float p_clear_depth, uint32_t p_clear_stencil, const Rect2 &p_region, const Vector<RID> &p_storage_textures) {
	if (_recording_error() != OK) {
		return _recording_error();
	}
	ERR_FAIL_V_MSG(ERR_UNAVAILABLE, "Deprecated. Split draw lists are used automatically by RenderingDevice.");
}
#endif

void RenderingDevice::draw_list_set_blend_constants(DrawListID p_list, const Color &p_color) {
	ERR_RENDER_THREAD_GUARD();
	ERR_FAIL_COND_MSG(_recording_error() != OK, "Recording is suspended after a frame fence wait failure.");

	ERR_FAIL_COND(!draw_list.active);

	draw_graph.add_draw_list_set_blend_constants(p_color);
}

void RenderingDevice::draw_list_bind_render_pipeline(DrawListID p_list, RID p_render_pipeline) {
	_draw_list_bind_render_pipeline_recorded(p_list, p_render_pipeline, false);
}

RenderingDevice::DrawAppendResult RenderingDevice::_draw_list_bind_render_pipeline_recorded(DrawListID p_list, RID p_render_pipeline, bool p_capture) {
	DrawAppendResult result;
	ERR_RENDER_THREAD_GUARD_V(result.failure(ERR_UNAVAILABLE));
	ERR_FAIL_COND_V_MSG(_recording_error() != OK, result.failure(_recording_error()), "Recording is suspended after a frame fence wait failure.");
	if (p_capture) {
		ERR_FAIL_COND_V(p_list != (int64_t(ID_TYPE_DRAW_LIST) << ID_BASE_SHIFT), result.failure(ERR_INVALID_PARAMETER));
	}

	ERR_FAIL_COND_V(!draw_list.active, result.failure(ERR_INVALID_PARAMETER));

	const RenderPipeline *pipeline = render_pipeline_owner.get_or_null(p_render_pipeline);
	ERR_FAIL_NULL_V(pipeline, result.failure(ERR_INVALID_PARAMETER));
	ERR_FAIL_COND_V(pipeline->qualification_return && (!qualification_recording || pipeline->qualification_return->use_closed.load()), result.failure(ERR_UNAUTHORIZED));
	if (p_capture) {
		ERR_FAIL_COND_V(pipeline->validation.framebuffer_format != draw_list_framebuffer_format || pipeline->validation.render_pass != draw_list_current_subpass, result.failure(ERR_INVALID_PARAMETER));
	}
#ifdef DEBUG_ENABLED
	ERR_FAIL_COND_V(pipeline->validation.framebuffer_format != draw_list_framebuffer_format && pipeline->validation.render_pass != draw_list_current_subpass, result.failure(ERR_INVALID_PARAMETER));
#endif

	if (p_render_pipeline == draw_list.state.pipeline) {
		return result; // Redundant state, return.
	}

	draw_list.state.pipeline = p_render_pipeline;
	draw_list.state.checked_push_constant_supplied = false;

	result.draw_handle = draw_graph.add_draw_list_bind_pipeline(pipeline->driver_id, pipeline->stage_bits);

	if (draw_list.state.pipeline_shader != pipeline->shader) {
		// Shader changed, so descriptor sets may become incompatible.

		uint32_t pcount = pipeline->set_formats.size(); // Formats count in this pipeline.
		draw_list.state.set_count = MAX(draw_list.state.set_count, pcount);
		const uint32_t *pformats = pipeline->set_formats.ptr(); // Pipeline set formats.

		uint32_t first_invalid_set = UINT32_MAX; // All valid by default.
		if (pipeline->push_constant_size != draw_list.state.pipeline_push_constant_size) {
			// All sets must be invalidated as the pipeline layout is not compatible if the push constant range is different.
			draw_list.state.pipeline_push_constant_size = pipeline->push_constant_size;
			first_invalid_set = 0;
		} else {
			switch (driver->api_trait_get(RDD::API_TRAIT_SHADER_CHANGE_INVALIDATION)) {
				case RDD::SHADER_CHANGE_INVALIDATION_ALL_BOUND_UNIFORM_SETS: {
					first_invalid_set = 0;
				} break;
				case RDD::SHADER_CHANGE_INVALIDATION_INCOMPATIBLE_SETS_PLUS_CASCADE: {
					for (uint32_t i = 0; i < pcount; i++) {
						if (draw_list.state.sets[i].pipeline_expected_format != pformats[i]) {
							first_invalid_set = i;
							break;
						}
					}
				} break;
				case RDD::SHADER_CHANGE_INVALIDATION_ALL_OR_NONE_ACCORDING_TO_LAYOUT_HASH: {
					if (draw_list.state.pipeline_shader_layout_hash != pipeline->shader_layout_hash) {
						first_invalid_set = 0;
					}
				} break;
			}
		}

		if (pipeline->push_constant_size) {
#ifdef DEBUG_ENABLED
			draw_list.validation.pipeline_push_constant_supplied = false;
#endif
		}

		for (uint32_t i = 0; i < pcount; i++) {
			draw_list.state.sets[i].bound = draw_list.state.sets[i].bound && i < first_invalid_set;
			draw_list.state.sets[i].pipeline_expected_format = pformats[i];
		}

		for (uint32_t i = pcount; i < draw_list.state.set_count; i++) {
			// Unbind the ones above (not used) if exist.
			draw_list.state.sets[i].bound = false;
		}

		draw_list.state.set_count = pcount; // Update set count.

		draw_list.state.pipeline_shader = pipeline->shader;
		draw_list.state.pipeline_shader_driver_id = pipeline->shader_driver_id;
		draw_list.state.pipeline_shader_layout_hash = pipeline->shader_layout_hash;
	}

#ifdef DEBUG_ENABLED
	// Update render pass pipeline info.
	draw_list.validation.pipeline_active = true;
	draw_list.validation.pipeline_dynamic_state = pipeline->validation.dynamic_state;
	draw_list.validation.pipeline_vertex_format = pipeline->validation.vertex_format;
	draw_list.validation.pipeline_uses_restart_indices = pipeline->validation.uses_restart_indices;
	draw_list.validation.pipeline_primitive_divisor = pipeline->validation.primitive_divisor;
	draw_list.validation.pipeline_primitive_minimum = pipeline->validation.primitive_minimum;
	draw_list.validation.pipeline_push_constant_size = pipeline->push_constant_size;
#endif

	result.effect = NativeAppendEffect::Recorded;
	result.capture_complete = p_capture;
	return result;
}

void RenderingDevice::draw_list_bind_uniform_set(DrawListID p_list, RID p_uniform_set, uint32_t p_index) {
	_draw_list_bind_uniform_set_recorded(p_list, p_uniform_set, p_index, false);
}

RenderingDevice::BindAppendResult RenderingDevice::_draw_list_bind_uniform_set_recorded(DrawListID p_list, RID p_uniform_set, uint32_t p_index, bool p_capture) {
	BindAppendResult result;
	ERR_RENDER_THREAD_GUARD_V(result.failure(ERR_UNAVAILABLE));
	ERR_FAIL_COND_V_MSG(_recording_error() != OK, result.failure(_recording_error()), "Recording is suspended after a frame fence wait failure.");
	if (p_capture) {
		ERR_FAIL_COND_V(p_list != (int64_t(ID_TYPE_DRAW_LIST) << ID_BASE_SHIFT), result.failure(ERR_INVALID_PARAMETER));
		ERR_FAIL_COND_V(p_index >= MAX_UNIFORM_SETS || p_index >= driver->limit_get(LIMIT_MAX_BOUND_UNIFORM_SETS), result.failure(ERR_INVALID_PARAMETER));
	}

#ifdef DEBUG_ENABLED
	ERR_FAIL_COND_V_MSG(p_index >= driver->limit_get(LIMIT_MAX_BOUND_UNIFORM_SETS) || p_index >= MAX_UNIFORM_SETS, result.failure(ERR_INVALID_PARAMETER),
			"Attempting to bind a descriptor set (" + itos(p_index) + ") greater than what the hardware supports (" + itos(driver->limit_get(LIMIT_MAX_BOUND_UNIFORM_SETS)) + ").");
#endif

	ERR_FAIL_COND_V(!draw_list.active, result.failure(ERR_INVALID_PARAMETER));

	const UniformSet *uniform_set = uniform_set_owner.get_or_null(p_uniform_set);
	ERR_FAIL_NULL_V(uniform_set, result.failure(ERR_INVALID_PARAMETER));
	ERR_FAIL_COND_V_MSG(_native_immutable_uniform_set_writes(*uniform_set), result.failure(ERR_UNAUTHORIZED), "An immutable native backing cannot be bound for shader writes.");
	ERR_FAIL_COND_V((!uniform_set->qualification_dependencies.is_empty() || uniform_set->qualification_return) && !qualification_recording, result.failure(ERR_UNAUTHORIZED));

	const auto &previous = draw_list.state.sets[p_index];
	result.state_changed = p_index > draw_list.state.set_count || previous.uniform_set_driver_id != uniform_set->driver_id || previous.bound || previous.uniform_set_format != uniform_set->format || previous.uniform_set != p_uniform_set;
	if (p_index > draw_list.state.set_count) {
		draw_list.state.set_count = p_index;
	}

	draw_list.state.sets[p_index].uniform_set_driver_id = uniform_set->driver_id; // Update set pointer.
	draw_list.state.sets[p_index].bound = false; // Needs rebind.
	draw_list.state.sets[p_index].uniform_set_format = uniform_set->format;
	draw_list.state.sets[p_index].uniform_set = p_uniform_set;

	bool validate_attachments = p_capture;
#ifdef DEBUG_ENABLED
	validate_attachments = true;
#endif
	if (validate_attachments) { // Preserve the original post-state-mutation validation.
		uint32_t attachable_count = uniform_set->attachable_textures.size();
		const UniformSet::AttachableTexture *attachable_ptr = uniform_set->attachable_textures.ptr();
		uint32_t bound_count = draw_list_bound_textures.size();
		const RID *bound_ptr = draw_list_bound_textures.ptr();
		for (uint32_t i = 0; i < attachable_count; i++) {
			for (uint32_t j = 0; j < bound_count; j++) {
				ERR_FAIL_COND_V_MSG(attachable_ptr[i].texture == bound_ptr[j], result.failure(ERR_INVALID_PARAMETER),
						"Attempted to use the same texture in framebuffer attachment and a uniform (set: " + itos(p_index) + ", binding: " + itos(attachable_ptr[i].bind) + "), this is not allowed.");
			}
		}
	}
	result.state_accepted = true;
	result.capture_complete = p_capture;
	return result;
}

void RenderingDevice::draw_list_bind_vertex_array(DrawListID p_list, RID p_vertex_array) {
	_draw_list_bind_vertex_array_recorded(p_list, p_vertex_array, nullptr, 0, false);
}

bool RenderingDevice::_draw_list_usage_conflicts(const RDG::ResourceTracker *p_tracker, RDG::ResourceUsage p_usage) const {
	// Match add_draw_list_usage's reset-if-outdated and same-list usage rule.
	// The retained original begin identifies this active graph list only.
	return p_tracker->command_frame == int64_t(draw_list.begin_handle.graph_frame) && p_tracker->draw_list_index == int32_t(draw_list.begin_handle.index) && p_tracker->draw_list_usage != p_usage;
}

RenderingDevice::BindAppendResult RenderingDevice::_draw_list_bind_vertex_array_recorded(DrawListID p_list, RID p_vertex_array, NativeTransferDependency *p_transfers, uint32_t p_transfer_capacity, bool p_capture) {
	BindAppendResult result;
	ERR_RENDER_THREAD_GUARD_V(result.failure(ERR_UNAVAILABLE));
	ERR_FAIL_COND_V_MSG(_recording_error() != OK, result.failure(_recording_error()), "Recording is suspended after a frame fence wait failure.");
	if (p_capture) {
		ERR_FAIL_COND_V(p_list != (int64_t(ID_TYPE_DRAW_LIST) << ID_BASE_SHIFT), result.failure(ERR_INVALID_PARAMETER));
	}

	ERR_FAIL_COND_V(!draw_list.active, result.failure(ERR_INVALID_PARAMETER));
	if (p_capture) {
		ERR_FAIL_COND_V(!draw_graph.contains_record(draw_list.begin_handle), result.failure(ERR_INVALID_DATA));
	}

	VertexArray *vertex_array = vertex_array_owner.get_or_null(p_vertex_array);
	ERR_FAIL_NULL_V(vertex_array, result.failure(ERR_INVALID_PARAMETER));

	if (draw_list.state.vertex_array == p_vertex_array) {
		result.state_accepted = true;
		result.capture_complete = p_capture;
		return result; // Already set; no new transfer or instruction.
	}

	if (p_capture) {
		const uint32_t count = vertex_array->transfer_worker_indices.size();
		ERR_FAIL_COND_V(count != uint32_t(vertex_array->transfer_worker_operations.size()) || vertex_array->buffers.size() != vertex_array->offsets.size(), result.failure(ERR_INVALID_DATA));
		ERR_FAIL_COND_V(count > p_transfer_capacity || (count && !p_transfers), result.failure(ERR_PARAMETER_RANGE_ERROR));
		for (uint32_t i = 0; i < count; ++i) {
			const int32_t index = vertex_array->transfer_worker_indices[i];
			ERR_FAIL_COND_V(index < 0 || uint32_t(index) >= transfer_worker_pool.size() || !transfer_worker_pool[index] || transfer_worker_pool[index]->index != uint32_t(index) || transfer_worker_pool[index]->index >= transfer_worker_operation_used_by_draw.size(), result.failure(ERR_INVALID_DATA));
		}
		for (RDG::ResourceTracker *tracker : vertex_array->draw_trackers) {
			ERR_FAIL_NULL_V(tracker, result.failure(ERR_INVALID_DATA));
			ERR_FAIL_COND_V(_draw_list_usage_conflicts(tracker, RDG::RESOURCE_USAGE_VERTEX_BUFFER_READ), result.failure(ERR_INVALID_PARAMETER));
		}
		for (uint32_t i = 0; i < count; ++i) {
			p_transfers[i] = { uint32_t(vertex_array->transfer_worker_indices[i]), vertex_array->transfer_worker_operations[i] };
		}
		result.transfer_count = count;
	}
	{
		const Error dependency_error = _check_transfer_worker_vertex_array(vertex_array);
		if (dependency_error != OK) {
			return result.failure(dependency_error);
		}
	}
	result.state_changed = true;

	draw_list.state.vertex_array = p_vertex_array;

#ifdef DEBUG_ENABLED
	draw_list.validation.vertex_format = vertex_array->description;
	draw_list.validation.vertex_max_instances_allowed = vertex_array->max_instances_allowed;
#endif
	draw_list.validation.vertex_array_size = vertex_array->vertex_count;
	result.state_accepted = true;

	result.bind_handle = draw_graph.add_draw_list_bind_vertex_buffers(vertex_array->buffers, vertex_array->offsets);
	result.effect = NativeAppendEffect::Recorded;

	for (int i = 0; i < vertex_array->draw_trackers.size(); i++) {
		draw_graph.add_draw_list_usage(vertex_array->draw_trackers[i], RDG::RESOURCE_USAGE_VERTEX_BUFFER_READ);
	}
	if (p_capture) {
		ERR_FAIL_COND_V(!draw_graph.contains_record(result.bind_handle), result.failure(ERR_INVALID_DATA));
	}
	result.state_accepted = true;
	result.capture_complete = p_capture;
	return result;
}

void RenderingDevice::draw_list_bind_vertex_buffers_format(DrawListID p_list, VertexFormatID p_vertex_format, uint32_t p_vertex_count, const Span<RID> &p_vertex_buffers, const Span<uint64_t> &p_offsets) {
	ERR_RENDER_THREAD_GUARD();
	ERR_FAIL_COND_MSG(_recording_error() != OK, "Recording is suspended after a frame fence wait failure.");

	ERR_FAIL_COND(!draw_list.active);

	const VertexDescriptionCache *vertex_description;
	{
		_THREAD_SAFE_METHOD_
		vertex_description = vertex_formats.getptr(p_vertex_format);
	}
	ERR_FAIL_NULL_MSG(vertex_description, "Supplied vertex format does not exist.");

	Span<uint64_t> offsets_span = p_offsets;
	FixedVector<uint64_t, 32> offsets;
	if (offsets_span.is_empty()) {
		offsets.resize_initialized(p_vertex_buffers.size());
		offsets_span = offsets;
	} else {
		ERR_FAIL_COND_MSG(offsets_span.size() != p_vertex_buffers.size(),
				"Number of vertex buffer offsets (" + itos(offsets_span.size()) + ") does not match number of vertex buffers (" + itos(p_vertex_buffers.size()) + ").");
	}

	FixedVector<RDD::BufferID, 32> driver_buffers;
	driver_buffers.resize_initialized(p_vertex_buffers.size());

	FixedVector<RDG::ResourceTracker *, 32> draw_trackers;

#if DEBUG_ENABLED
	uint32_t max_instances_allowed = 0xFFFFFFFF;
#endif

	for (uint32_t i = 0; i < p_vertex_buffers.size(); i++) {
		RID buffer_rid = p_vertex_buffers[i];
		if (buffer_rid.is_null()) {
			// The buffer array can be sparse.
			continue;
		}
		ERR_FAIL_COND_MSG(!vertex_buffer_owner.owns(buffer_rid), "Vertex buffer at index " + itos(i) + " is invalid.");

		Buffer *buffer = vertex_buffer_owner.get_or_null(buffer_rid);
		ERR_FAIL_NULL(buffer);

		{
			const Error dependency_error = _check_transfer_worker_buffer(buffer);
			if (dependency_error != OK) {
				return;
			}
		}

#if DEBUG_ENABLED
		uint64_t binding_offset = offsets_span[i];
		ERR_FAIL_COND_MSG(binding_offset > buffer->size, "Vertex buffer offset for attachment (" + itos(i) + ") exceeds buffer size.");

		const VertexAttribute &attribute = vertex_description->vertex_formats[i];
		uint32_t element_size = get_format_vertex_size(attribute.format);
		ERR_FAIL_COND_MSG(element_size == 0, "Vertex attribute format for attachment (" + itos(i) + ") is invalid.");

		uint64_t attribute_offset = binding_offset + attribute.offset;
		ERR_FAIL_COND_MSG(attribute_offset > buffer->size, "Vertex attribute offset for attachment (" + itos(i) + ") exceeds buffer size.");
		ERR_FAIL_COND_MSG(attribute_offset + element_size > buffer->size,
				"Vertex buffer (" + itos(i) + ") will read past the end of the buffer.");

		if (attribute.frequency == VERTEX_FREQUENCY_VERTEX) {
			ERR_FAIL_COND_MSG(p_vertex_count == 0, "Vertex count must be greater than 0 when binding vertex buffers.");

			uint64_t required_size = attribute_offset + element_size;
			if (p_vertex_count > 1) {
				required_size += uint64_t(attribute.stride) * (uint64_t(p_vertex_count) - 1);
			}

			ERR_FAIL_COND_MSG(required_size > buffer->size,
					"Vertex buffer (" + itos(i) + ") will read past the end of the buffer.");
		} else {
			uint64_t available = buffer->size - attribute_offset;
			ERR_FAIL_COND_MSG(available < element_size,
					"Vertex buffer (" + itos(i) + ") uses instancing, but it's just too small.");

			uint32_t instances_allowed = attribute.stride == 0 ? 0 : uint32_t(buffer->size / attribute.stride);
			max_instances_allowed = MIN(instances_allowed, max_instances_allowed);
		}
#endif

		driver_buffers[i] = buffer->driver_id;

		if (buffer->draw_tracker != nullptr) {
			draw_trackers.push_back(buffer->draw_tracker);
		}
	}

	draw_list.state.vertex_array = RID();

	draw_graph.add_draw_list_bind_vertex_buffers(driver_buffers, offsets_span);

	for (RDG::ResourceTracker *tracker : draw_trackers) {
		draw_graph.add_draw_list_usage(tracker, RDG::RESOURCE_USAGE_VERTEX_BUFFER_READ);
	}

	draw_list.validation.vertex_array_size = p_vertex_count;

#ifdef DEBUG_ENABLED
	draw_list.validation.vertex_format = p_vertex_format;
	draw_list.validation.vertex_max_instances_allowed = max_instances_allowed;
#endif
}

void RenderingDevice::draw_list_bind_index_array(DrawListID p_list, RID p_index_array) {
	_draw_list_bind_index_array_recorded(p_list, p_index_array, nullptr, 0, false);
}

RenderingDevice::BindAppendResult RenderingDevice::_draw_list_bind_index_array_recorded(DrawListID p_list, RID p_index_array, NativeTransferDependency *p_transfers, uint32_t p_transfer_capacity, bool p_capture) {
	BindAppendResult result;
	ERR_RENDER_THREAD_GUARD_V(result.failure(ERR_UNAVAILABLE));
	ERR_FAIL_COND_V_MSG(_recording_error() != OK, result.failure(_recording_error()), "Recording is suspended after a frame fence wait failure.");
	if (p_capture) {
		ERR_FAIL_COND_V(p_list != (int64_t(ID_TYPE_DRAW_LIST) << ID_BASE_SHIFT), result.failure(ERR_INVALID_PARAMETER));
	}

	ERR_FAIL_COND_V(!draw_list.active, result.failure(ERR_INVALID_PARAMETER));
	if (p_capture) {
		ERR_FAIL_COND_V(!draw_graph.contains_record(draw_list.begin_handle), result.failure(ERR_INVALID_DATA));
	}

	IndexArray *index_array = index_array_owner.get_or_null(p_index_array);
	ERR_FAIL_NULL_V(index_array, result.failure(ERR_INVALID_PARAMETER));

	if (draw_list.state.index_array == p_index_array) {
		result.state_accepted = true;
		result.capture_complete = p_capture;
		return result; // Already set; no new transfer or instruction.
	}

	if (p_capture) {
		const uint64_t checked_offset = uint64_t(index_array->offset) * (index_array->format == INDEX_BUFFER_FORMAT_UINT16 ? sizeof(uint16_t) : sizeof(uint32_t));
		ERR_FAIL_COND_V(checked_offset > UINT32_MAX, result.failure(ERR_PARAMETER_RANGE_ERROR));
		ERR_FAIL_COND_V(index_array->draw_tracker && _draw_list_usage_conflicts(index_array->draw_tracker, RDG::RESOURCE_USAGE_INDEX_BUFFER_READ), result.failure(ERR_INVALID_PARAMETER));
		if (index_array->transfer_worker_index >= 0) {
			const uint32_t index = index_array->transfer_worker_index;
			ERR_FAIL_COND_V(!p_transfer_capacity || !p_transfers, result.failure(ERR_PARAMETER_RANGE_ERROR));
			ERR_FAIL_COND_V(index >= transfer_worker_pool.size() || !transfer_worker_pool[index] || transfer_worker_pool[index]->index != uint32_t(index) || transfer_worker_pool[index]->index >= transfer_worker_operation_used_by_draw.size(), result.failure(ERR_INVALID_DATA));
			p_transfers[0] = { index, index_array->transfer_worker_operation };
			result.transfer_count = 1;
		}
	}
	{
		const Error dependency_error = _check_transfer_worker_index_array(index_array);
		if (dependency_error != OK) {
			return result.failure(dependency_error);
		}
	}
	result.state_changed = true;

	draw_list.state.index_array = p_index_array;
#ifdef DEBUG_ENABLED
	draw_list.validation.index_array_max_index = index_array->max_index;
#endif
	draw_list.validation.index_array_count = index_array->indices;
	result.state_accepted = true;

	const uint64_t offset_bytes = index_array->offset * (index_array->format == INDEX_BUFFER_FORMAT_UINT16 ? sizeof(uint16_t) : sizeof(uint32_t));
	result.bind_handle = draw_graph.add_draw_list_bind_index_buffer(index_array->driver_id, index_array->format, offset_bytes);
	result.effect = NativeAppendEffect::Recorded;

	if (index_array->draw_tracker != nullptr) {
		draw_graph.add_draw_list_usage(index_array->draw_tracker, RDG::RESOURCE_USAGE_INDEX_BUFFER_READ);
	}
	if (p_capture) {
		ERR_FAIL_COND_V(!draw_graph.contains_record(result.bind_handle), result.failure(ERR_INVALID_DATA));
	}
	result.state_accepted = true;
	result.capture_complete = p_capture;
	return result;
}

void RenderingDevice::draw_list_set_line_width(DrawListID p_list, float p_width) {
	ERR_RENDER_THREAD_GUARD();
	ERR_FAIL_COND_MSG(_recording_error() != OK, "Recording is suspended after a frame fence wait failure.");

	ERR_FAIL_COND(!draw_list.active);

	draw_graph.add_draw_list_set_line_width(p_width);
}

void RenderingDevice::draw_list_set_push_constant(DrawListID p_list, const void *p_data, uint32_t p_data_size) {
	_draw_list_set_push_constant_recorded(p_list, p_data, p_data_size, false);
}

RenderingDevice::DrawAppendResult RenderingDevice::_draw_list_set_push_constant_recorded(DrawListID p_list, const void *p_data, uint32_t p_data_size, bool p_capture) {
	DrawAppendResult result;
	ERR_RENDER_THREAD_GUARD_V(result.failure(ERR_UNAVAILABLE));
	ERR_FAIL_COND_V_MSG(_recording_error() != OK, result.failure(_recording_error()), "Recording is suspended after a frame fence wait failure.");

	ERR_FAIL_COND_V(!draw_list.active, result.failure(ERR_INVALID_PARAMETER));

	if (p_capture) {
		ERR_FAIL_COND_V(p_list != (int64_t(ID_TYPE_DRAW_LIST) << ID_BASE_SHIFT), result.failure(ERR_INVALID_PARAMETER));
		ERR_FAIL_COND_V(!draw_list.state.pipeline.is_valid() || p_data_size != draw_list.state.pipeline_push_constant_size || (p_data_size && !p_data), result.failure(ERR_INVALID_PARAMETER));
	}
#ifdef DEBUG_ENABLED
	ERR_FAIL_COND_V_MSG(p_data_size != draw_list.validation.pipeline_push_constant_size, result.failure(ERR_INVALID_PARAMETER),
			"This render pipeline requires (" + itos(draw_list.validation.pipeline_push_constant_size) + ") bytes of push constant data, supplied: (" + itos(p_data_size) + ")");
#endif

	result.draw_handle = draw_graph.add_draw_list_set_push_constant(draw_list.state.pipeline_shader_driver_id, p_data, p_data_size);

#ifdef DEBUG_ENABLED
	draw_list.validation.pipeline_push_constant_supplied = true;
#endif

	draw_list.state.checked_push_constant_supplied = true;

	result.effect = NativeAppendEffect::Recorded;
	result.capture_complete = p_capture;
	return result;
}

RenderingDevice::DrawAppendResult RenderingDevice::_draw_list_draw_geometry_direct_recorded(DrawListID p_list, bool p_use_indices, uint32_t p_instances) {
	return _draw_list_draw_recorded(p_list, p_use_indices, p_instances, 0, true, DrawCaptureProfile::GeometryDirect);
}

RenderingDevice::DrawAppendResult RenderingDevice::_draw_list_draw_geometry_prepared_recorded(DrawListID p_list, bool p_use_indices, uint32_t p_instances) {
	return _draw_list_draw_recorded(p_list, p_use_indices, p_instances, 0, true, DrawCaptureProfile::GeometryPrepared);
}

void RenderingDevice::draw_list_draw(DrawListID p_list, bool p_use_indices, uint32_t p_instances, uint32_t p_procedural_vertices) {
	_draw_list_draw_recorded(p_list, p_use_indices, p_instances, p_procedural_vertices, false);
}

RenderingDevice::DrawAppendResult RenderingDevice::_draw_list_draw_recorded(DrawListID p_list, bool p_use_indices, uint32_t p_instances, uint32_t p_procedural_vertices, bool p_capture, DrawCaptureProfile p_profile) {
	DrawAppendResult result;
	const bool geometry_capture = p_capture && (p_profile == DrawCaptureProfile::GeometryDirect || p_profile == DrawCaptureProfile::GeometryPrepared);
	const bool prepared_capture = p_capture && p_profile == DrawCaptureProfile::GeometryPrepared;
	uint32_t expected_prefix_count = 0;
	ERR_RENDER_THREAD_GUARD_V(result.failure(ERR_UNAVAILABLE));
	ERR_FAIL_COND_V_MSG(_recording_error() != OK, result.failure(_recording_error()), "Recording is suspended after a frame fence wait failure.");

	ERR_FAIL_COND_V(!draw_list.active, result.failure(ERR_INVALID_PARAMETER));

	if (p_capture && p_profile == DrawCaptureProfile::ProceduralTriangle) {
		ERR_FAIL_COND_V(p_list != (int64_t(ID_TYPE_DRAW_LIST) << ID_BASE_SHIFT), result.failure(ERR_INVALID_PARAMETER));
		ERR_FAIL_COND_V(!draw_list.state.pipeline.is_valid() || p_use_indices || p_instances != 1 || p_procedural_vertices != 3 || draw_list.state.vertex_array.is_valid() || draw_list.state.index_array.is_valid() || draw_list.state.set_count || (draw_list.state.pipeline_push_constant_size && !draw_list.state.checked_push_constant_supplied), result.failure(ERR_UNAVAILABLE));
	}

	if (geometry_capture) {
		ERR_FAIL_COND_V(p_list != (int64_t(ID_TYPE_DRAW_LIST) << ID_BASE_SHIFT), result.failure(ERR_INVALID_PARAMETER));
		ERR_FAIL_COND_V(!draw_graph.contains_record(draw_list.begin_handle), result.failure(ERR_INVALID_DATA));
		ERR_FAIL_COND_V(!draw_list.state.pipeline.is_valid() || p_instances != 1 || p_procedural_vertices != 0, result.failure(ERR_UNAVAILABLE));
		if (!prepared_capture) {
			ERR_FAIL_COND_V(draw_list.state.set_count != 0, result.failure(ERR_UNAVAILABLE));
			for (const DrawList::SetState &set : draw_list.state.sets) {
				ERR_FAIL_COND_V(set.uniform_set.is_valid(), result.failure(ERR_UNAVAILABLE));
			}
		}
		ERR_FAIL_COND_V(draw_list.state.pipeline_push_constant_size && !draw_list.state.checked_push_constant_supplied, result.failure(ERR_UNAVAILABLE));
		ERR_FAIL_COND_V(!render_pipeline_owner.owns(draw_list.state.pipeline) || native_immutable_resource_is_retiring(draw_list.state.pipeline), result.failure(ERR_INVALID_PARAMETER));
		const RenderPipeline *pipeline = render_pipeline_owner.get_or_null(draw_list.state.pipeline);
		ERR_FAIL_NULL_V(pipeline, result.failure(ERR_INVALID_PARAMETER));
		ERR_FAIL_COND_V(!pipeline->driver_id || pipeline->qualification_return, result.failure(ERR_UNAUTHORIZED));
		ERR_FAIL_COND_V(pipeline->validation.framebuffer_format != draw_list_framebuffer_format || pipeline->validation.render_pass != draw_list_current_subpass, result.failure(ERR_INVALID_PARAMETER));
		ERR_FAIL_COND_V(!shader_owner.owns(pipeline->shader) || native_immutable_resource_is_retiring(pipeline->shader), result.failure(ERR_INVALID_PARAMETER));
		const Shader *shader = shader_owner.get_or_null(pipeline->shader);
		ERR_FAIL_NULL_V(shader, result.failure(ERR_INVALID_PARAMETER));
		ERR_FAIL_COND_V(shader->qualification_return, result.failure(ERR_UNAUTHORIZED));
		ERR_FAIL_COND_V(!shader->driver_id || shader->driver_id != pipeline->shader_driver_id || draw_list.state.pipeline_shader != pipeline->shader || draw_list.state.pipeline_shader_driver_id != pipeline->shader_driver_id || draw_list.state.pipeline_shader_layout_hash != pipeline->shader_layout_hash || draw_list.state.pipeline_push_constant_size != pipeline->push_constant_size, result.failure(ERR_INVALID_DATA));
		ERR_FAIL_COND_V(pipeline->validation.primitive != RENDER_PRIMITIVE_TRIANGLES || pipeline->validation.vertex_format == INVALID_ID, result.failure(ERR_UNAVAILABLE));
		if (!prepared_capture) {
			for (uint32_t i = 0; i < pipeline->set_formats.size(); ++i) {
				ERR_FAIL_COND_V(pipeline->set_formats[i] != 0, result.failure(ERR_UNAVAILABLE));
			}
		}
		ERR_FAIL_COND_V(!draw_list.state.vertex_array.is_valid() || !vertex_array_owner.owns(draw_list.state.vertex_array), result.failure(ERR_INVALID_PARAMETER));
		const VertexArray *vertex_array = vertex_array_owner.get_or_null(draw_list.state.vertex_array);
		ERR_FAIL_NULL_V(vertex_array, result.failure(ERR_INVALID_PARAMETER));
		ERR_FAIL_COND_V(native_immutable_resource_is_retiring(draw_list.state.vertex_array) || vertex_array->description != pipeline->validation.vertex_format || vertex_array->vertex_count <= 0 || vertex_array->max_instances_allowed < 1, result.failure(ERR_INVALID_PARAMETER));
		ERR_FAIL_COND_V(uint32_t(vertex_array->vertex_count) != draw_list.validation.vertex_array_size, result.failure(ERR_INVALID_DATA));
		if (p_use_indices) {
			ERR_FAIL_COND_V(!draw_list.state.index_array.is_valid() || !index_array_owner.owns(draw_list.state.index_array), result.failure(ERR_INVALID_PARAMETER));
			const IndexArray *index_array = index_array_owner.get_or_null(draw_list.state.index_array);
			ERR_FAIL_NULL_V(index_array, result.failure(ERR_INVALID_PARAMETER));
			ERR_FAIL_COND_V(native_immutable_resource_is_retiring(draw_list.state.index_array) || index_array->indices < 3 || index_array->indices % 3 != 0 || index_array->supports_restart_indices || (index_array->format != INDEX_BUFFER_FORMAT_UINT16 && index_array->format != INDEX_BUFFER_FORMAT_UINT32), result.failure(ERR_INVALID_PARAMETER));
			ERR_FAIL_COND_V(index_array->indices != draw_list.validation.index_array_count, result.failure(ERR_INVALID_DATA));
			const uint64_t index_width = index_array->format == INDEX_BUFFER_FORMAT_UINT16 ? sizeof(uint16_t) : sizeof(uint32_t);
			ERR_FAIL_COND_V(uint64_t(index_array->offset) * index_width > UINT32_MAX, result.failure(ERR_PARAMETER_RANGE_ERROR));
		} else {
			ERR_FAIL_COND_V(draw_list.state.index_array.is_valid(), result.failure(ERR_UNAVAILABLE));
			ERR_FAIL_COND_V(vertex_array->vertex_count < 3 || vertex_array->vertex_count % 3 != 0, result.failure(ERR_INVALID_PARAMETER));
		}
	}

	if (prepared_capture) {
		static_assert(MAX_UNIFORM_SETS * 2 <= DrawAppendResult::PREFIX_CAPACITY);
		ERR_FAIL_COND_V(draw_list.state.set_count > MAX_UNIFORM_SETS || draw_list.state.set_count > driver->limit_get(LIMIT_MAX_BOUND_UNIFORM_SETS), result.failure(ERR_INVALID_PARAMETER));
		const RenderPipeline *pipeline = render_pipeline_owner.get_or_null(draw_list.state.pipeline);
		const Shader *shader = shader_owner.get_or_null(pipeline->shader);
		ERR_FAIL_COND_V(uint32_t(pipeline->set_formats.size()) != draw_list.state.set_count || shader->set_formats != pipeline->set_formats, result.failure(ERR_INVALID_DATA));
		// This profile captures read-only sets requiring no hidden copy, clear,
		// transfer or revision mutation. Original public drawing is unchanged.
		HashMap<RDG::ResourceTracker *, RDG::ResourceUsage> pending_usages;
		uint32_t batch_count = 0;
		uint32_t last_index = 0;
		for (uint32_t i = 0; i < draw_list.state.set_count; ++i) {
			const DrawList::SetState &binding = draw_list.state.sets[i];
			ERR_FAIL_COND_V(binding.pipeline_expected_format != pipeline->set_formats[i], result.failure(ERR_INVALID_DATA));
			if (!binding.pipeline_expected_format) {
				continue;
			}
			ERR_FAIL_COND_V(!uniform_set_owner.owns(binding.uniform_set) || native_immutable_resource_is_retiring(binding.uniform_set), result.failure(ERR_INVALID_PARAMETER));
			const UniformSet *set = uniform_set_owner.get_or_null(binding.uniform_set);
			ERR_FAIL_NULL_V(set, result.failure(ERR_INVALID_PARAMETER));
			ERR_FAIL_COND_V(!set->driver_id || set->driver_id != binding.uniform_set_driver_id || set->format != binding.uniform_set_format || set->format != binding.pipeline_expected_format, result.failure(ERR_INVALID_DATA));
			ERR_FAIL_COND_V(set->qualification_return || !set->qualification_dependencies.is_empty() || _native_immutable_uniform_set_writes(*set), result.failure(ERR_UNAUTHORIZED));
			ERR_FAIL_COND_V(!set->writable_resources.is_empty() || !set->pending_clear_textures.is_empty(), result.failure(ERR_UNAVAILABLE));
			ERR_FAIL_COND_V(!shader_owner.owns(set->shader_id) || native_immutable_resource_is_retiring(set->shader_id), result.failure(ERR_INVALID_PARAMETER));
			const Shader *origin = shader_owner.get_or_null(set->shader_id);
			ERR_FAIL_COND_V(!origin || !origin->driver_id || origin->qualification_return || set->shader_set >= uint32_t(origin->set_formats.size()) || origin->set_formats[set->shader_set] != set->format, result.failure(ERR_INVALID_DATA));
			for (const UniformSet::AttachableTexture &attachment : set->attachable_textures) {
				ERR_FAIL_COND_V(draw_list_bound_textures.has(attachment.texture), result.failure(ERR_INVALID_PARAMETER));
			}
			for (const UniformSet::SharedTexture &shared : set->shared_textures_to_update) {
				ERR_FAIL_COND_V(!texture_owner.owns(shared.texture) || native_immutable_resource_is_retiring(shared.texture), result.failure(ERR_INVALID_PARAMETER));
				const Texture *texture = texture_owner.get_or_null(shared.texture);
				ERR_FAIL_NULL_V(texture, result.failure(ERR_INVALID_PARAMETER));
				if (!texture->shared_fallback) {
					continue;
				}
				ERR_FAIL_COND_V(shared.writing, result.failure(ERR_UNAVAILABLE));
				if (texture->owner.is_valid()) {
					ERR_FAIL_COND_V(!texture_owner.owns(texture->owner) || native_immutable_resource_is_retiring(texture->owner), result.failure(ERR_INVALID_PARAMETER));
					const Texture *owner = texture_owner.get_or_null(texture->owner);
					ERR_FAIL_COND_V(!owner || !owner->shared_fallback, result.failure(ERR_INVALID_PARAMETER));
					ERR_FAIL_COND_V(texture->shared_fallback->revision != owner->shared_fallback->revision, result.failure(ERR_UNAVAILABLE));
				}
			}
			ERR_FAIL_COND_V(set->draw_trackers.size() != set->draw_trackers_usage.size(), result.failure(ERR_INVALID_DATA));
			for (int j = 0; j < set->draw_trackers.size(); ++j) {
				RDG::ResourceTracker *tracker = set->draw_trackers[j];
				const RDG::ResourceUsage usage = set->draw_trackers_usage[j];
				ERR_FAIL_COND_V(!tracker || usage <= RDG::RESOURCE_USAGE_NONE || usage >= RDG::RESOURCE_USAGE_MAX, result.failure(ERR_INVALID_DATA));
				if (descriptor_set_batching && !binding.bound) {
					ERR_FAIL_COND_V(_draw_list_usage_conflicts(tracker, usage), result.failure(ERR_INVALID_PARAMETER));
					const RDG::ResourceUsage *previous = pending_usages.getptr(tracker);
					ERR_FAIL_COND_V(previous && *previous != usage, result.failure(ERR_INVALID_PARAMETER));
					pending_usages.insert(tracker, usage);
				}
			}
			// Immutable resources legitimately have no tracker. Check the
			// original owner and transfer premise without creating one here.
			for (const auto &entry : set->untracked_usage) {
				ERR_FAIL_COND_V(native_immutable_resource_is_retiring(entry.key), result.failure(ERR_INVALID_PARAMETER));
				const Buffer *buffer = nullptr;
				switch (entry.value) {
					case RDG::RESOURCE_USAGE_UNIFORM_BUFFER_READ:
						ERR_FAIL_COND_V(!uniform_buffer_owner.owns(entry.key), result.failure(ERR_INVALID_PARAMETER));
						buffer = uniform_buffer_owner.get_or_null(entry.key);
						break;
					case RDG::RESOURCE_USAGE_TEXTURE_BUFFER_READ:
						ERR_FAIL_COND_V(!texture_buffer_owner.owns(entry.key), result.failure(ERR_INVALID_PARAMETER));
						buffer = texture_buffer_owner.get_or_null(entry.key);
						break;
					case RDG::RESOURCE_USAGE_STORAGE_BUFFER_READ:
						if (storage_buffer_owner.owns(entry.key)) {
							buffer = storage_buffer_owner.get_or_null(entry.key);
						} else if (vertex_buffer_owner.owns(entry.key)) {
							buffer = vertex_buffer_owner.get_or_null(entry.key);
						}
						ERR_FAIL_NULL_V(buffer, result.failure(ERR_INVALID_PARAMETER));
						break;
					case RDG::RESOURCE_USAGE_TEXTURE_SAMPLE: {
						ERR_FAIL_COND_V(!texture_owner.owns(entry.key), result.failure(ERR_INVALID_PARAMETER));
						const Texture *texture = texture_owner.get_or_null(entry.key);
						ERR_FAIL_COND_V(!texture || !texture->driver_id || texture->qualification_return || texture->transfer_worker_index >= 0, result.failure(ERR_INVALID_DATA));
					} break;
					default:
						return result.failure(ERR_UNAVAILABLE);
				}
				ERR_FAIL_COND_V(buffer && (!buffer->driver_id || buffer->qualification_return || buffer->transfer_worker_index >= 0), result.failure(ERR_UNAVAILABLE));
			}
			expected_prefix_count += !driver->api_trait_get(RDD::API_TRAIT_HONORS_PIPELINE_BARRIERS);
			if (!binding.bound) {
				if (descriptor_set_batching) {
					// Mirror the original loop, including its empty first batch
					// when the first expected set has index greater than one.
					if (i - last_index > 1) {
						++expected_prefix_count;
						batch_count = 1;
					} else {
						++batch_count;
					}
					last_index = i;
				} else {
					++expected_prefix_count;
				}
			}
		}
		expected_prefix_count += descriptor_set_batching && batch_count != 0;
		ERR_FAIL_COND_V(expected_prefix_count > DrawAppendResult::PREFIX_CAPACITY, result.failure(ERR_PARAMETER_RANGE_ERROR));
	}

#ifdef DEBUG_ENABLED
	ERR_FAIL_COND_V_MSG(!draw_list.validation.pipeline_active, result.failure(ERR_INVALID_PARAMETER),
			"No render pipeline was set before attempting to draw.");
	if (draw_list.validation.pipeline_vertex_format != INVALID_ID) {
		// Pipeline uses vertices, validate format.
		ERR_FAIL_COND_V_MSG(draw_list.validation.vertex_format == INVALID_ID, result.failure(ERR_INVALID_PARAMETER),
				"No vertex array was bound, and render pipeline expects vertices.");
		// Make sure format is right.
		ERR_FAIL_COND_V_MSG(draw_list.validation.pipeline_vertex_format != draw_list.validation.vertex_format, result.failure(ERR_INVALID_PARAMETER),
				"The vertex format used to create the pipeline does not match the vertex format bound.");
		// Make sure number of instances is valid.
		ERR_FAIL_COND_V_MSG(p_instances > draw_list.validation.vertex_max_instances_allowed, result.failure(ERR_INVALID_PARAMETER),
				"Number of instances requested (" + itos(p_instances) + " is larger than the maximum number supported by the bound vertex array (" + itos(draw_list.validation.vertex_max_instances_allowed) + ").");
	}

	if (draw_list.validation.pipeline_push_constant_size > 0) {
		// Using push constants, check that they were supplied.
		ERR_FAIL_COND_V_MSG(!draw_list.validation.pipeline_push_constant_supplied, result.failure(ERR_INVALID_PARAMETER),
				"The shader in this pipeline requires a push constant to be set before drawing, but it's not present.");
	}

#endif

#ifdef DEBUG_ENABLED
	for (uint32_t i = 0; i < draw_list.state.set_count; i++) {
		if (draw_list.state.sets[i].pipeline_expected_format == 0) {
			// Nothing expected by this pipeline.
			continue;
		}

		if (draw_list.state.sets[i].pipeline_expected_format != draw_list.state.sets[i].uniform_set_format) {
			if (draw_list.state.sets[i].uniform_set_format == 0) {
				ERR_FAIL_V_MSG(result.failure(ERR_INVALID_PARAMETER), vformat("Uniforms were never supplied for set (%d) at the time of drawing, which are required by the pipeline.", i));
			} else if (uniform_set_owner.owns(draw_list.state.sets[i].uniform_set)) {
				UniformSet *us = uniform_set_owner.get_or_null(draw_list.state.sets[i].uniform_set);
				const String us_info = us ? vformat("(%d):\n%s\n", i, _shader_uniform_debug(us->shader_id, us->shader_set)) : vformat("(%d, which was just freed) ", i);
				ERR_FAIL_V_MSG(result.failure(ERR_INVALID_PARAMETER), vformat("Uniforms supplied for set %sare not the same format as required by the pipeline shader. Pipeline shader requires the following bindings:\n%s", us_info, _shader_uniform_debug(draw_list.state.pipeline_shader)));
			} else {
				ERR_FAIL_V_MSG(result.failure(ERR_INVALID_PARAMETER), vformat("Uniforms supplied for set (%d, which was just freed) are not the same format as required by the pipeline shader. Pipeline shader requires the following bindings:\n%s", i, _shader_uniform_debug(draw_list.state.pipeline_shader)));
			}
		}
	}
#endif
	// Reserve pending-clear worker dependencies before descriptor graph prefixes.
	for (uint32_t i = 0; i < draw_list.state.set_count; ++i) {
		if (!draw_list.state.sets[i].pipeline_expected_format || draw_list.state.sets[i].bound) {
			continue;
		}
		UniformSet *set = uniform_set_owner.get_or_null(draw_list.state.sets[i].uniform_set);
		if (set) {
			const Error error = _uniform_set_admit_clear_transfers(set);
			if (error != OK) {
				return result.failure(error);
			}
		}
	}
	thread_local LocalVector<RDD::UniformSetID> valid_descriptor_ids;
	valid_descriptor_ids.clear();
	valid_descriptor_ids.resize(draw_list.state.set_count);
	uint32_t valid_set_count = 0;
	uint32_t first_set_index = 0;
	uint32_t last_set_index = 0;
	bool found_first_set = false;

	for (uint32_t i = 0; i < draw_list.state.set_count; i++) {
		if (draw_list.state.sets[i].pipeline_expected_format == 0) {
			continue; // Nothing expected by this pipeline.
		}

		if (!draw_list.state.sets[i].bound && !found_first_set) {
			first_set_index = i;
			found_first_set = true;
		}
		// Prepare descriptor sets if the API doesn't use pipeline barriers.
		if (!driver->api_trait_get(RDD::API_TRAIT_HONORS_PIPELINE_BARRIERS)) {
			{
				result.effect = NativeAppendEffect::NativePrefixOnly;
				ERR_FAIL_COND_V(p_capture && result.prefix_count >= DrawAppendResult::PREFIX_CAPACITY, result.failure(ERR_PARAMETER_RANGE_ERROR));
				const auto prefix = draw_graph.add_draw_list_uniform_set_prepare_for_use(draw_list.state.pipeline_shader_driver_id, draw_list.state.sets[i].uniform_set_driver_id, i);
				if (p_capture) {
					result.prefix_handles[result.prefix_count++] = prefix;
				}
			}
		}
	}

	// Bind descriptor sets.
	for (uint32_t i = first_set_index; i < draw_list.state.set_count; i++) {
		if (draw_list.state.sets[i].pipeline_expected_format == 0) {
			continue; // Nothing expected by this pipeline.
		}

		if (!draw_list.state.sets[i].bound) {
			// Batch contiguous descriptor sets in a single call.
			if (descriptor_set_batching) {
				// All good, see if this requires re-binding.
				if (i - last_set_index > 1) {
					// If the descriptor sets are not contiguous, bind the previous ones and start a new batch.
					{
						result.effect = NativeAppendEffect::NativePrefixOnly;
						ERR_FAIL_COND_V(p_capture && result.prefix_count >= DrawAppendResult::PREFIX_CAPACITY, result.failure(ERR_PARAMETER_RANGE_ERROR));
						const auto prefix = draw_graph.add_draw_list_bind_uniform_sets(draw_list.state.pipeline_shader_driver_id, valid_descriptor_ids, first_set_index, valid_set_count);
						if (p_capture) {
							result.prefix_handles[result.prefix_count++] = prefix;
						}
					}

					first_set_index = i;
					valid_set_count = 1;
					valid_descriptor_ids[0] = draw_list.state.sets[i].uniform_set_driver_id;
				} else {
					// Otherwise, keep storing in the current batch.
					valid_descriptor_ids[valid_set_count] = draw_list.state.sets[i].uniform_set_driver_id;
					valid_set_count++;
				}

				UniformSet *uniform_set = uniform_set_owner.get_or_null(draw_list.state.sets[i].uniform_set);
				ERR_FAIL_NULL_V(uniform_set, result.failure(ERR_INVALID_PARAMETER));
				_uniform_set_update_shared(uniform_set);
				{
					const Error dependency_error = _uniform_set_update_clears(uniform_set);
					if (dependency_error != OK) {
						return result.failure(dependency_error);
					}
				}

				draw_graph.add_draw_list_usages(uniform_set->draw_trackers, uniform_set->draw_trackers_usage);
				draw_list.state.sets[i].bound = true;

				last_set_index = i;
			} else {
				{
					result.effect = NativeAppendEffect::NativePrefixOnly;
					ERR_FAIL_COND_V(p_capture && result.prefix_count >= DrawAppendResult::PREFIX_CAPACITY, result.failure(ERR_PARAMETER_RANGE_ERROR));
					const auto prefix = draw_graph.add_draw_list_bind_uniform_set(draw_list.state.pipeline_shader_driver_id, draw_list.state.sets[i].uniform_set_driver_id, i);
					if (p_capture) {
						result.prefix_handles[result.prefix_count++] = prefix;
					}
				}
			}
		}
	}

	// Bind the remaining batch.
	if (descriptor_set_batching && valid_set_count > 0) {
		{
			result.effect = NativeAppendEffect::NativePrefixOnly;
			ERR_FAIL_COND_V(p_capture && result.prefix_count >= DrawAppendResult::PREFIX_CAPACITY, result.failure(ERR_PARAMETER_RANGE_ERROR));
			const auto prefix = draw_graph.add_draw_list_bind_uniform_sets(draw_list.state.pipeline_shader_driver_id, valid_descriptor_ids, first_set_index, valid_set_count);
			if (p_capture) {
				result.prefix_handles[result.prefix_count++] = prefix;
			}
		}
	}

	if (p_use_indices) {
#ifdef DEBUG_ENABLED
		ERR_FAIL_COND_V_MSG(p_procedural_vertices > 0, result.failure(ERR_INVALID_PARAMETER),
				"Procedural vertices can't be used together with indices.");

		ERR_FAIL_COND_V_MSG(!draw_list.validation.index_array_count, result.failure(ERR_INVALID_PARAMETER),
				"Draw command requested indices, but no index buffer was set.");

		ERR_FAIL_COND_V_MSG(draw_list.validation.pipeline_uses_restart_indices != draw_list.validation.index_buffer_uses_restart_indices, result.failure(ERR_INVALID_PARAMETER),
				"The usage of restart indices in index buffer does not match the render primitive in the pipeline.");
#endif
		uint32_t to_draw = draw_list.validation.index_array_count;

#ifdef DEBUG_ENABLED
		ERR_FAIL_COND_V_MSG(to_draw < draw_list.validation.pipeline_primitive_minimum, result.failure(ERR_INVALID_PARAMETER),
				"Too few indices (" + itos(to_draw) + ") for the render primitive set in the render pipeline (" + itos(draw_list.validation.pipeline_primitive_minimum) + ").");

		ERR_FAIL_COND_V_MSG((to_draw % draw_list.validation.pipeline_primitive_divisor) != 0, result.failure(ERR_INVALID_PARAMETER),
				"Index amount (" + itos(to_draw) + ") must be a multiple of the amount of indices required by the render primitive (" + itos(draw_list.validation.pipeline_primitive_divisor) + ").");
#endif

		result.draw_handle = draw_graph.add_draw_list_draw_indexed(to_draw, p_instances, 0);
	} else {
		uint32_t to_draw;

		if (p_procedural_vertices > 0) {
			to_draw = p_procedural_vertices;
		} else {
#ifdef DEBUG_ENABLED
			ERR_FAIL_COND_V_MSG(draw_list.validation.pipeline_vertex_format == INVALID_ID, result.failure(ERR_INVALID_PARAMETER),
					"Draw command lacks indices, but pipeline format does not use vertices.");
#endif
			to_draw = draw_list.validation.vertex_array_size;
		}

#ifdef DEBUG_ENABLED
		ERR_FAIL_COND_V_MSG(to_draw < draw_list.validation.pipeline_primitive_minimum, result.failure(ERR_INVALID_PARAMETER),
				"Too few vertices (" + itos(to_draw) + ") for the render primitive set in the render pipeline (" + itos(draw_list.validation.pipeline_primitive_minimum) + ").");

		ERR_FAIL_COND_V_MSG((to_draw % draw_list.validation.pipeline_primitive_divisor) != 0, result.failure(ERR_INVALID_PARAMETER),
				"Vertex amount (" + itos(to_draw) + ") must be a multiple of the amount of vertices required by the render primitive (" + itos(draw_list.validation.pipeline_primitive_divisor) + ").");
#endif

		result.draw_handle = draw_graph.add_draw_list_draw(to_draw, p_instances);
	}

#if defined(TESTS_ENABLED) && defined(MODULE_SKINNED_MESH_CAPTURE_TESTS_ENABLED)
	if (RenderCaptureTestHooks::write_census_active()) {
		for (uint32_t i = 0; i < draw_list.state.set_count; i++) {
			if (draw_list.state.sets[i].pipeline_expected_format != 0) {
				_capture_test_record_writable_set(draw_list.state.sets[i].uniform_set, RenderCaptureTestHooks::DRAW);
			}
		}
	}
#endif
	draw_list.state.draw_count++;

	result.effect = NativeAppendEffect::Recorded;
	result.capture_complete = p_capture;
	if (geometry_capture && !draw_graph.contains_record(result.draw_handle)) {
		// The native draw was already appended. Preserve that fact while
		// reporting that its capture handle could not be retained.
		result.error = ERR_INVALID_DATA;
		result.capture_complete = false;
	}
	if (prepared_capture) {
		const auto &begin = draw_list.begin_handle;
		uint32_t previous_offset = 0;
		bool have_previous = false;
		auto exact_instruction = [&](const RDG::RecordHandle &handle) {
			return handle.kind == 3 && handle.graph_instance == begin.graph_instance && handle.graph_frame == begin.graph_frame && handle.index == begin.index && draw_graph.contains_record(handle);
		};
		bool complete = result.prefix_count == expected_prefix_count;
		for (uint32_t i = 0; i < result.prefix_count; ++i) {
			const auto &handle = result.prefix_handles[i];
			complete = complete && exact_instruction(handle) && (!have_previous || handle.offset > previous_offset);
			previous_offset = handle.offset;
			have_previous = true;
		}
		complete = complete && exact_instruction(result.draw_handle) && (!have_previous || result.draw_handle.offset > previous_offset);
		result.capture_complete = complete;
		if (!complete) {
			result.error = ERR_INVALID_DATA; // Preserve the already recorded draw.
		}
	}
	return result;
}

void RenderingDevice::draw_list_draw_indirect(DrawListID p_list, bool p_use_indices, RID p_buffer, uint32_t p_offset, uint32_t p_draw_count, uint32_t p_stride) {
	_draw_list_draw_indirect_recorded(p_list, p_use_indices, p_buffer, p_offset, p_draw_count, p_stride, false);
}

RenderingDevice::DrawAppendResult RenderingDevice::_draw_list_draw_indirect_recorded(DrawListID p_list, bool p_use_indices, RID p_buffer, uint32_t p_offset, uint32_t p_draw_count, uint32_t p_stride, bool p_capture) {
	DrawAppendResult result;
	ERR_RENDER_THREAD_GUARD_V(result.failure(ERR_UNAVAILABLE));
	ERR_FAIL_COND_V_MSG(_recording_error() != OK, result.failure(_recording_error()), "Recording is suspended after a frame fence wait failure.");
	if (p_capture) {
		ERR_FAIL_COND_V(p_list != (int64_t(ID_TYPE_DRAW_LIST) << ID_BASE_SHIFT), result.failure(ERR_INVALID_PARAMETER));
	}

	ERR_FAIL_COND_V(!draw_list.active, result.failure(ERR_INVALID_PARAMETER));

	Buffer *buffer = storage_buffer_owner.get_or_null(p_buffer);
	ERR_FAIL_NULL_V(buffer, result.failure(ERR_INVALID_PARAMETER));
	ERR_FAIL_COND_V(buffer->qualification_return, result.failure(ERR_UNAUTHORIZED));

	ERR_FAIL_COND_V_MSG(!buffer->usage.has_flag(RDD::BUFFER_USAGE_INDIRECT_BIT), result.failure(ERR_INVALID_PARAMETER), "Buffer provided was not created to do indirect dispatch.");

	// Validate the complete indirect command span before touching descriptor
	// state or recording graph instructions. A zero stride retains the existing
	// tightly packed single-command meaning; multiple commands must provide an
	// explicit stride at least as large as the command itself.
	const uint64_t command_size = p_use_indices ? 20u : 16u;
	ERR_FAIL_COND_V_MSG((p_offset & 3u) != 0, result.failure(ERR_INVALID_PARAMETER), "Indirect command offset must be 4-byte aligned.");
	ERR_FAIL_COND_V_MSG((p_stride & 3u) != 0, result.failure(ERR_INVALID_PARAMETER), "Indirect command stride must be 4-byte aligned.");
	if (p_draw_count > 1) {
		ERR_FAIL_COND_V_MSG(p_stride == 0 || uint64_t(p_stride) < command_size, result.failure(ERR_INVALID_PARAMETER), "Indirect command stride is too small for multiple draws.");
	} else if (p_draw_count == 1 && p_stride != 0) {
		ERR_FAIL_COND_V_MSG(uint64_t(p_stride) < command_size, result.failure(ERR_INVALID_PARAMETER), "Indirect command stride is too small for one draw.");
	}
	if (p_draw_count != 0) {
		const uint64_t effective_stride = p_stride == 0 ? command_size : p_stride;
		const uint64_t command_end = uint64_t(p_offset) + effective_stride * uint64_t(p_draw_count - 1) + command_size;
		ERR_FAIL_COND_V_MSG(command_end < uint64_t(p_offset) || command_end > buffer->size, result.failure(ERR_INVALID_PARAMETER), "Indirect command span is past the end of the buffer.");
	}

#ifdef DEBUG_ENABLED
	ERR_FAIL_COND_V_MSG(!draw_list.validation.pipeline_active, result.failure(ERR_INVALID_PARAMETER),
			"No render pipeline was set before attempting to draw.");
	if (draw_list.validation.pipeline_vertex_format != INVALID_ID) {
		// Pipeline uses vertices, validate format.
		ERR_FAIL_COND_V_MSG(draw_list.validation.vertex_format == INVALID_ID, result.failure(ERR_INVALID_PARAMETER),
				"No vertex array was bound, and render pipeline expects vertices.");
		// Make sure format is right.
		ERR_FAIL_COND_V_MSG(draw_list.validation.pipeline_vertex_format != draw_list.validation.vertex_format, result.failure(ERR_INVALID_PARAMETER),
				"The vertex format used to create the pipeline does not match the vertex format bound.");
	}

	if (draw_list.validation.pipeline_push_constant_size > 0) {
		// Using push constants, check that they were supplied.
		ERR_FAIL_COND_V_MSG(!draw_list.validation.pipeline_push_constant_supplied, result.failure(ERR_INVALID_PARAMETER),
				"The shader in this pipeline requires a push constant to be set before drawing, but it's not present.");
	}
#endif

#ifdef DEBUG_ENABLED
	for (uint32_t i = 0; i < draw_list.state.set_count; i++) {
		if (draw_list.state.sets[i].pipeline_expected_format == 0) {
			// Nothing expected by this pipeline.
			continue;
		}

		if (draw_list.state.sets[i].pipeline_expected_format != draw_list.state.sets[i].uniform_set_format) {
			if (draw_list.state.sets[i].uniform_set_format == 0) {
				ERR_FAIL_V_MSG(result.failure(ERR_INVALID_PARAMETER), vformat("Uniforms were never supplied for set (%d) at the time of drawing, which are required by the pipeline.", i));
			} else if (uniform_set_owner.owns(draw_list.state.sets[i].uniform_set)) {
				UniformSet *us = uniform_set_owner.get_or_null(draw_list.state.sets[i].uniform_set);
				const String us_info = us ? vformat("(%d):\n%s\n", i, _shader_uniform_debug(us->shader_id, us->shader_set)) : vformat("(%d, which was just freed) ", i);
				ERR_FAIL_V_MSG(result.failure(ERR_INVALID_PARAMETER), vformat("Uniforms supplied for set %sare not the same format as required by the pipeline shader. Pipeline shader requires the following bindings:\n%s", us_info, _shader_uniform_debug(draw_list.state.pipeline_shader)));
			} else {
				ERR_FAIL_V_MSG(result.failure(ERR_INVALID_PARAMETER), vformat("Uniforms supplied for set (%d, which was just freed) are not the same format as required by the pipeline shader. Pipeline shader requires the following bindings:\n%s", i, _shader_uniform_debug(draw_list.state.pipeline_shader)));
			}
		}
	}
#endif

	if (p_capture) {
		static_assert(MAX_UNIFORM_SETS * 2 == 32);
		ERR_FAIL_COND_V(draw_list.state.set_count > MAX_UNIFORM_SETS, result.failure(ERR_INVALID_PARAMETER));
		// Capture permits only already-prepared immutable bindings. Ordinary
		// public RD drawing keeps its original shared-copy/clear behavior below.
		// Those original preparation operations need their own native capture
		// before an output packet may borrow the resulting material generation.
		for (uint32_t i = 0; i < draw_list.state.set_count; i++) {
			if (draw_list.state.sets[i].pipeline_expected_format == 0) {
				continue;
			}
			UniformSet *set = uniform_set_owner.get_or_null(draw_list.state.sets[i].uniform_set);
			ERR_FAIL_NULL_V(set, result.failure(ERR_INVALID_PARAMETER));
			for (const UniformSet::SharedTexture &shared : set->shared_textures_to_update) {
				Texture *texture = texture_owner.get_or_null(shared.texture);
				ERR_FAIL_NULL_V(texture, result.failure(ERR_INVALID_PARAMETER));
				if (texture->shared_fallback == nullptr) {
					continue;
				}
				ERR_FAIL_COND_V(shared.writing, result.failure(ERR_UNAVAILABLE));
				if (texture->owner.is_valid()) {
					Texture *owner = texture_owner.get_or_null(texture->owner);
					ERR_FAIL_COND_V(!owner || !owner->shared_fallback, result.failure(ERR_INVALID_PARAMETER));
					ERR_FAIL_COND_V(texture->shared_fallback->revision != owner->shared_fallback->revision, result.failure(ERR_UNAVAILABLE));
				}
			}
			for (RID rid : set->pending_clear_textures) {
				Texture *texture = texture_owner.get_or_null(rid);
				ERR_FAIL_NULL_V(texture, result.failure(ERR_INVALID_PARAMETER));
				ERR_FAIL_COND_V(texture->pending_clear, result.failure(ERR_UNAVAILABLE));
			}
		}
		result.capture_complete = true;
	}

	if (buffer->transfer_worker_index >= 0) {
		result.has_transfer = true;
		result.transfer = { uint32_t(buffer->transfer_worker_index), buffer->transfer_worker_operation };
	}
	{
		const Error dependency_error = _check_transfer_worker_buffer(buffer);
		if (dependency_error != OK) {
			return result.failure(dependency_error);
		}
	}
	for (uint32_t i = 0; i < draw_list.state.set_count; ++i) {
		if (!draw_list.state.sets[i].pipeline_expected_format || draw_list.state.sets[i].bound) {
			continue;
		}
		UniformSet *set = uniform_set_owner.get_or_null(draw_list.state.sets[i].uniform_set);
		if (set) {
			const Error error = _uniform_set_admit_clear_transfers(set);
			if (error != OK) {
				return result.failure(error);
			}
		}
	}

	// Prepare descriptor sets if the API doesn't use pipeline barriers.
	if (!driver->api_trait_get(RDD::API_TRAIT_HONORS_PIPELINE_BARRIERS)) {
		for (uint32_t i = 0; i < draw_list.state.set_count; i++) {
			if (draw_list.state.sets[i].pipeline_expected_format == 0) {
				// Nothing expected by this pipeline.
				continue;
			}

			result.effect = NativeAppendEffect::NativePrefixOnly;
			const auto prepare = draw_graph.add_draw_list_uniform_set_prepare_for_use(draw_list.state.pipeline_shader_driver_id, draw_list.state.sets[i].uniform_set_driver_id, i);
			if (p_capture) {
				result.prefix_handles[result.prefix_count++] = prepare;
			}
		}
	}

	// Bind descriptor sets.
	for (uint32_t i = 0; i < draw_list.state.set_count; i++) {
		if (draw_list.state.sets[i].pipeline_expected_format == 0) {
			continue; // Nothing expected by this pipeline.
		}
		if (!draw_list.state.sets[i].bound) {
			// All good, see if this requires re-binding.
			result.effect = NativeAppendEffect::NativePrefixOnly;
			const auto bind = draw_graph.add_draw_list_bind_uniform_set(draw_list.state.pipeline_shader_driver_id, draw_list.state.sets[i].uniform_set_driver_id, i);
			if (p_capture) {
				result.prefix_handles[result.prefix_count++] = bind;
			}

			UniformSet *uniform_set = uniform_set_owner.get_or_null(draw_list.state.sets[i].uniform_set);
			ERR_FAIL_NULL_V(uniform_set, result.failure(ERR_INVALID_PARAMETER));
			_uniform_set_update_shared(uniform_set);
			{
				const Error dependency_error = _uniform_set_update_clears(uniform_set);
				if (dependency_error != OK) {
					return result.failure(dependency_error);
				}
			}

			draw_graph.add_draw_list_usages(uniform_set->draw_trackers, uniform_set->draw_trackers_usage);

			draw_list.state.sets[i].bound = true;
		}
	}

	if (p_use_indices) {
		ERR_FAIL_COND_V_MSG(!draw_list.validation.index_array_count, result.failure(ERR_INVALID_PARAMETER),
				"Draw command requested indices, but no index buffer was set.");

#ifdef DEBUG_ENABLED
		ERR_FAIL_COND_V_MSG(draw_list.validation.pipeline_uses_restart_indices != draw_list.validation.index_buffer_uses_restart_indices, result.failure(ERR_INVALID_PARAMETER),
				"The usage of restart indices in index buffer does not match the render primitive in the pipeline.");
#endif

		result.draw_handle = draw_graph.add_draw_list_draw_indexed_indirect(buffer->driver_id, p_offset, p_draw_count, p_stride);
	} else {
		result.draw_handle = draw_graph.add_draw_list_draw_indirect(buffer->driver_id, p_offset, p_draw_count, p_stride);
	}

#if defined(TESTS_ENABLED) && defined(MODULE_SKINNED_MESH_CAPTURE_TESTS_ENABLED)
	if (RenderCaptureTestHooks::write_census_active()) {
		for (uint32_t i = 0; i < draw_list.state.set_count; i++) {
			if (draw_list.state.sets[i].pipeline_expected_format != 0) {
				_capture_test_record_writable_set(draw_list.state.sets[i].uniform_set, RenderCaptureTestHooks::DRAW_INDIRECT);
			}
		}
	}
#endif
	draw_list.state.draw_count++;

	if (buffer->draw_tracker != nullptr) {
		draw_graph.add_draw_list_usage(buffer->draw_tracker, RDG::RESOURCE_USAGE_INDIRECT_BUFFER_READ);
	}

	result.effect = NativeAppendEffect::Recorded;
	return result;
}

void RenderingDevice::draw_list_set_viewport(DrawListID p_list, const Rect2 &p_rect) {
	ERR_FAIL_COND_MSG(_recording_error() != OK, "Recording is suspended after a frame fence wait failure.");
	ERR_FAIL_COND(!draw_list.active);

	if (p_rect.get_area() == 0) {
		return;
	}

	draw_list.viewport = p_rect;
	draw_graph.add_draw_list_set_viewport(p_rect);
}

void RenderingDevice::draw_list_enable_scissor(DrawListID p_list, const Rect2 &p_rect) {
	_draw_list_enable_scissor_recorded(p_list, p_rect, false);
}

RenderingDevice::DrawAppendResult RenderingDevice::_draw_list_enable_scissor_recorded(DrawListID p_list, const Rect2 &p_rect, bool p_capture) {
	DrawAppendResult result;
	ERR_RENDER_THREAD_GUARD_V(result.failure(ERR_UNAVAILABLE));
	ERR_FAIL_COND_V_MSG(_recording_error() != OK, result.failure(_recording_error()), "Recording is suspended after a frame fence wait failure.");
	if (p_capture) {
		ERR_FAIL_COND_V(p_list != (int64_t(ID_TYPE_DRAW_LIST) << ID_BASE_SHIFT), result.failure(ERR_INVALID_PARAMETER));
	}

	ERR_FAIL_COND_V(!draw_list.active, result.failure(ERR_INVALID_PARAMETER));

	Rect2i rect = p_rect;
	rect.position += draw_list.viewport.position;

	rect = draw_list.viewport.intersection(rect);

	if (rect.get_area() == 0) {
		return result;
	}

	result.draw_handle = draw_graph.add_draw_list_set_scissor(rect);

	result.effect = NativeAppendEffect::Recorded;
	result.capture_complete = p_capture;
	return result;
}

void RenderingDevice::draw_list_disable_scissor(DrawListID p_list) {
	ERR_RENDER_THREAD_GUARD();
	ERR_FAIL_COND_MSG(_recording_error() != OK, "Recording is suspended after a frame fence wait failure.");

	ERR_FAIL_COND(!draw_list.active);

	draw_graph.add_draw_list_set_scissor(draw_list.viewport);
}

uint32_t RenderingDevice::draw_list_get_current_pass() {
	ERR_RENDER_THREAD_GUARD_V(0);

	return draw_list_current_subpass;
}

RenderingDevice::DrawListID RenderingDevice::draw_list_switch_to_next_pass() {
	ERR_RENDER_THREAD_GUARD_V(INVALID_ID);
	ERR_FAIL_COND_V_MSG(_recording_error() != OK, INVALID_ID, "Recording is suspended after a frame fence wait failure.");

	ERR_FAIL_COND_V(!draw_list.active, INVALID_FORMAT_ID);
	ERR_FAIL_COND_V(draw_list_current_subpass >= draw_list_subpass_count - 1, INVALID_FORMAT_ID);

	draw_list_current_subpass++;

	Rect2i viewport;
	_draw_list_end(&viewport);

	draw_graph.add_draw_list_next_subpass(RDD::COMMAND_BUFFER_TYPE_PRIMARY);

	_draw_list_start(viewport);

	return int64_t(ID_TYPE_DRAW_LIST) << ID_BASE_SHIFT;
}

#ifndef DISABLE_DEPRECATED
Error RenderingDevice::draw_list_switch_to_next_pass_split(uint32_t p_splits, DrawListID *r_split_ids) {
	if (_recording_error() != OK) {
		return _recording_error();
	}
	ERR_FAIL_V_MSG(ERR_UNAVAILABLE, "Deprecated. Split draw lists are used automatically by RenderingDevice.");
}
#endif

void RenderingDevice::_draw_list_start(const Rect2i &p_viewport) {
	draw_list.viewport = p_viewport;
	draw_list.active = true;
}

void RenderingDevice::_draw_list_end(Rect2i *r_last_viewport) {
	if (r_last_viewport) {
		*r_last_viewport = draw_list.viewport;
	}

	draw_list = DrawList();
}

void RenderingDevice::draw_list_end() {
	_draw_list_end_recorded(false);
}

RenderingDevice::DrawAppendResult RenderingDevice::_draw_list_end_recorded(bool p_capture) {
	DrawAppendResult result;
	ERR_RENDER_THREAD_GUARD_V(result.failure(ERR_UNAVAILABLE));
	ERR_FAIL_COND_V_MSG(_recording_error() != OK, result.failure(_recording_error()), "Recording is suspended after a frame fence wait failure.");

	ERR_FAIL_COND_V_MSG(!draw_list.active, result.failure(ERR_INVALID_PARAMETER), "Immediate draw list is already inactive.");

	result.draw_handle = draw_graph.add_draw_list_end();

	_draw_list_end();

	for (uint32_t i = 0; i < draw_list_bound_textures.size(); i++) {
		Texture *texture = texture_owner.get_or_null(draw_list_bound_textures[i]);
		ERR_CONTINUE(!texture); // Wtf.
		if (texture->usage_flags & TEXTURE_USAGE_COLOR_ATTACHMENT_BIT) {
			texture->bound = false;
		}
		if (texture->usage_flags & TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
			texture->bound = false;
		}
		if (texture->usage_flags & TEXTURE_USAGE_DEPTH_RESOLVE_ATTACHMENT_BIT) {
			texture->bound = false;
		}
	}

	draw_list_bound_textures.clear();

	result.effect = NativeAppendEffect::Recorded;
	result.capture_complete = p_capture;
	return result;
}

/***************************/
/**** RAYTRACING LISTS ****/
/**************************/

RenderingDevice::RaytracingListID RenderingDevice::raytracing_list_begin() {
	ERR_RENDER_THREAD_GUARD_V(INVALID_ID);
	ERR_FAIL_COND_V_MSG(_recording_error() != OK, INVALID_ID, "Recording is suspended after a frame fence wait failure.");

	ERR_FAIL_COND_V_MSG(!has_feature(SUPPORTS_RAYTRACING_PIPELINE), INVALID_ID, "The current rendering device has no raytracing pipeline support.");

	ERR_FAIL_COND_V_MSG(draw_list.active, INVALID_ID, "Only one draw/raytracing list can be active at the same time.");
	ERR_FAIL_COND_V_MSG(compute_list.active, INVALID_ID, "Only one compute/raytracing list can be active at the same time.");
	ERR_FAIL_COND_V_MSG(raytracing_list.active, INVALID_ID, "Only one raytracing list can be active at the same time.");

	raytracing_list.active = true;

	draw_graph.add_raytracing_list_begin();

	return ID_TYPE_RAYTRACING_LIST;
}

void RenderingDevice::raytracing_list_bind_raytracing_pipeline(RaytracingListID p_list, RID p_raytracing_pipeline) {
	ERR_RENDER_THREAD_GUARD();
	ERR_FAIL_COND_MSG(_recording_error() != OK, "Recording is suspended after a frame fence wait failure.");

	ERR_FAIL_COND(p_list != ID_TYPE_RAYTRACING_LIST);
	ERR_FAIL_COND(!raytracing_list.active);

	RaytracingPipeline *pipeline = raytracing_pipeline_owner.get_or_null(p_raytracing_pipeline);
	ERR_FAIL_NULL(pipeline);

	if (p_raytracing_pipeline == raytracing_list.state.pipeline) {
		return; // Redundant state, return.
	}

	{
		const Error dependency_error = _check_transfer_worker_buffer(&pipeline->sbt_buffer);
		if (dependency_error != OK) {
			return;
		}
	}

	raytracing_list.state.pipeline = p_raytracing_pipeline;
	raytracing_list.state.pipeline_driver_id = pipeline->driver_id;
	raytracing_list.state.sbt_buffer = pipeline->sbt_buffer.driver_id;
	raytracing_list.state.raygen_shader_count = pipeline->raygen_shader_count;
	raytracing_list.state.miss_shader_count = pipeline->miss_shader_count;

	draw_graph.add_raytracing_list_bind_pipeline(pipeline->driver_id);

	if (raytracing_list.state.layout_defining_shader != pipeline->layout_defining_shader) {
		// Shader changed, so descriptor sets may become incompatible.

		uint32_t pcount = pipeline->set_formats.size(); // Formats count in this pipeline.
		raytracing_list.state.set_count = MAX(raytracing_list.state.set_count, pcount);
		const uint32_t *pformats = pipeline->set_formats.ptr(); // Pipeline set formats.

		uint32_t first_invalid_set = UINT32_MAX; // All valid by default.
		switch (driver->api_trait_get(RDD::API_TRAIT_SHADER_CHANGE_INVALIDATION)) {
			case RDD::SHADER_CHANGE_INVALIDATION_ALL_BOUND_UNIFORM_SETS: {
				first_invalid_set = 0;
			} break;
			case RDD::SHADER_CHANGE_INVALIDATION_INCOMPATIBLE_SETS_PLUS_CASCADE: {
				for (uint32_t i = 0; i < pcount; i++) {
					if (raytracing_list.state.sets[i].pipeline_expected_format != pformats[i]) {
						first_invalid_set = i;
						break;
					}
				}
			} break;
			case RDD::SHADER_CHANGE_INVALIDATION_ALL_OR_NONE_ACCORDING_TO_LAYOUT_HASH: {
				if (raytracing_list.state.layout_defining_shader_layout_hash != pipeline->layout_defining_shader_layout_hash) {
					first_invalid_set = 0;
				}
			} break;
		}

		for (uint32_t i = 0; i < pcount; i++) {
			raytracing_list.state.sets[i].bound = raytracing_list.state.sets[i].bound && i < first_invalid_set;
			raytracing_list.state.sets[i].pipeline_expected_format = pformats[i];
		}

		for (uint32_t i = pcount; i < raytracing_list.state.set_count; i++) {
			// Unbind the ones above (not used) if exist.
			raytracing_list.state.sets[i].bound = false;
		}

		raytracing_list.state.set_count = pcount; // Update set count.

		if (pipeline->push_constant_size) {
#ifdef DEBUG_ENABLED
			raytracing_list.validation.pipeline_push_constant_supplied = false;
#endif
		}

		raytracing_list.state.layout_defining_shader = pipeline->layout_defining_shader;
		raytracing_list.state.layout_defining_shader_driver_id = pipeline->layout_defining_shader_driver_id;
		raytracing_list.state.layout_defining_shader_layout_hash = pipeline->layout_defining_shader_layout_hash;
	}

#ifdef DEBUG_ENABLED
	// Update raytracing pass pipeline info.
	raytracing_list.validation.pipeline_active = true;
	raytracing_list.validation.pipeline_push_constant_size = pipeline->push_constant_size;
#endif
}

void RenderingDevice::raytracing_list_bind_uniform_set(RaytracingListID p_list, RID p_uniform_set, uint32_t p_index) {
	ERR_RENDER_THREAD_GUARD();
	ERR_FAIL_COND_MSG(_recording_error() != OK, "Recording is suspended after a frame fence wait failure.");

	ERR_FAIL_COND(p_list != ID_TYPE_RAYTRACING_LIST);
	ERR_FAIL_COND(!raytracing_list.active);

#ifdef DEBUG_ENABLED
	ERR_FAIL_COND_MSG(p_index >= driver->limit_get(LIMIT_MAX_BOUND_UNIFORM_SETS) || p_index >= MAX_UNIFORM_SETS,
			"Attempting to bind a descriptor set (" + itos(p_index) + ") greater than what the hardware supports (" + itos(driver->limit_get(LIMIT_MAX_BOUND_UNIFORM_SETS)) + ").");
#endif

	UniformSet *uniform_set = uniform_set_owner.get_or_null(p_uniform_set);
	ERR_FAIL_NULL(uniform_set);
	ERR_FAIL_COND_MSG(_native_immutable_uniform_set_writes(*uniform_set), "An immutable native backing cannot be bound for shader writes.");
	ERR_FAIL_COND((!uniform_set->qualification_dependencies.is_empty() || uniform_set->qualification_return) && !qualification_recording);

#ifdef DEBUG_ENABLED
	// Validate the acceleration structures.
	for (RID acceleration_structure_id : uniform_set->acceleration_structures) {
		AccelerationStructure *acceleration_structure = acceleration_structure_owner.get_or_null(acceleration_structure_id);
		ERR_FAIL_NULL(acceleration_structure);
		ERR_FAIL_COND_MSG(acceleration_structure->invalidated, "Acceleration structure either has not been built yet, or has been invalidated by an operation and needs to be rebuilt.");
	}
#endif

	if (p_index > raytracing_list.state.set_count) {
		raytracing_list.state.set_count = p_index;
	}

	raytracing_list.state.sets[p_index].uniform_set_driver_id = uniform_set->driver_id; // Update set pointer.
	raytracing_list.state.sets[p_index].bound = false; // Needs rebind.
	raytracing_list.state.sets[p_index].uniform_set_format = uniform_set->format;
	raytracing_list.state.sets[p_index].uniform_set = p_uniform_set;
}

void RenderingDevice::raytracing_list_set_push_constant(RaytracingListID p_list, const void *p_data, uint32_t p_data_size) {
	ERR_RENDER_THREAD_GUARD();
	ERR_FAIL_COND_MSG(_recording_error() != OK, "Recording is suspended after a frame fence wait failure.");

	ERR_FAIL_COND(p_list != ID_TYPE_RAYTRACING_LIST);
	ERR_FAIL_COND(!raytracing_list.active);

	ERR_FAIL_COND_MSG(p_data_size > MAX_PUSH_CONSTANT_SIZE, "Push constants can't be bigger than 128 bytes to maintain compatibility.");

#ifdef DEBUG_ENABLED
	ERR_FAIL_COND_MSG(p_data_size != raytracing_list.validation.pipeline_push_constant_size,
			"This raytracing pipeline requires (" + itos(raytracing_list.validation.pipeline_push_constant_size) + ") bytes of push constant data, supplied: (" + itos(p_data_size) + ")");
#endif

	draw_graph.add_raytracing_list_set_push_constant(raytracing_list.state.layout_defining_shader_driver_id, p_data, p_data_size);

	// Store it in the state in case we need to restart the raytracing list.
	memcpy(raytracing_list.state.push_constant_data, p_data, p_data_size);
	raytracing_list.state.push_constant_size = p_data_size;

#ifdef DEBUG_ENABLED
	raytracing_list.validation.pipeline_push_constant_supplied = true;
#endif
}

void RenderingDevice::raytracing_list_trace_rays(RaytracingListID p_list, uint32_t p_raygen_shader_index, RID p_hit_sbt, uint32_t p_width, uint32_t p_height, uint32_t p_depth) {
	ERR_RENDER_THREAD_GUARD();
	ERR_FAIL_COND_MSG(_recording_error() != OK, "Recording is suspended after a frame fence wait failure.");

	ERR_FAIL_COND(p_list != ID_TYPE_RAYTRACING_LIST);
	ERR_FAIL_COND(!raytracing_list.active);

#ifdef DEBUG_ENABLED
	ERR_FAIL_NULL_MSG(shader_owner.get_or_null(raytracing_list.state.layout_defining_shader), "No shader was set before attempting to trace rays.");
	ERR_FAIL_NULL_MSG(raytracing_pipeline_owner.get_or_null(raytracing_list.state.pipeline), "No raytracing pipeline was set before attempting to trace rays.");
#endif

#ifdef DEBUG_ENABLED

	ERR_FAIL_COND_MSG(!raytracing_list.validation.pipeline_active, "No raytracing pipeline was set before attempting to draw.");

	if (raytracing_list.validation.pipeline_push_constant_size > 0) {
		// Using push constants, check that they were supplied.
		ERR_FAIL_COND_MSG(!raytracing_list.validation.pipeline_push_constant_supplied,
				"The shader in this pipeline requires a push constant to be set before drawing, but it's not present.");
	}

#endif

#ifdef DEBUG_ENABLED
	for (uint32_t i = 0; i < raytracing_list.state.set_count; i++) {
		if (raytracing_list.state.sets[i].pipeline_expected_format == 0) {
			// Nothing expected by this pipeline.
			continue;
		}

		if (raytracing_list.state.sets[i].pipeline_expected_format != raytracing_list.state.sets[i].uniform_set_format) {
			if (raytracing_list.state.sets[i].uniform_set_format == 0) {
				ERR_FAIL_MSG("Uniforms were never supplied for set (" + itos(i) + ") at the time of drawing, which are required by the pipeline.");
			} else if (uniform_set_owner.owns(raytracing_list.state.sets[i].uniform_set)) {
				UniformSet *us = uniform_set_owner.get_or_null(raytracing_list.state.sets[i].uniform_set);
				ERR_FAIL_MSG("Uniforms supplied for set (" + itos(i) + "):\n" + _shader_uniform_debug(us->shader_id, us->shader_set) + "\nare not the same format as required by the pipeline shader. Pipeline shader requires the following bindings:\n" + _shader_uniform_debug(raytracing_list.state.layout_defining_shader));
			} else {
				ERR_FAIL_MSG("Uniforms supplied for set (" + itos(i) + ", which was just freed) are not the same format as required by the pipeline shader. Pipeline shader requires the following bindings:\n" + _shader_uniform_debug(raytracing_list.state.layout_defining_shader));
			}
		}
	}
#endif

	// Prepare descriptor sets if the API doesn't use pipeline barriers.
	if (!driver->api_trait_get(RDD::API_TRAIT_HONORS_PIPELINE_BARRIERS)) {
		for (uint32_t i = 0; i < raytracing_list.state.set_count; i++) {
			if (raytracing_list.state.sets[i].pipeline_expected_format == 0) {
				// Nothing expected by this pipeline.
				continue;
			}

			draw_graph.add_raytracing_list_uniform_set_prepare_for_use(raytracing_list.state.layout_defining_shader_driver_id, raytracing_list.state.sets[i].uniform_set_driver_id, i);
		}
	}

	// Bind descriptor sets.
	for (uint32_t i = 0; i < raytracing_list.state.set_count; i++) {
		if (raytracing_list.state.sets[i].pipeline_expected_format == 0) {
			continue; // Nothing expected by this pipeline.
		}
		if (!raytracing_list.state.sets[i].bound) {
			// All good, see if this requires re-binding.
			draw_graph.add_raytracing_list_bind_uniform_set(raytracing_list.state.layout_defining_shader_driver_id, raytracing_list.state.sets[i].uniform_set_driver_id, i);

			UniformSet *uniform_set = uniform_set_owner.get_or_null(raytracing_list.state.sets[i].uniform_set);
			_uniform_set_update_shared(uniform_set);

			draw_graph.add_raytracing_list_usages(uniform_set->draw_trackers, uniform_set->draw_trackers_usage);

			raytracing_list.state.sets[i].bound = true;
		}
	}

	uint32_t shader_group_handle_size = driver->api_trait_get(RDD::API_TRAIT_SHADER_GROUP_HANDLE_SIZE);
	uint32_t shader_group_handle_alignment = driver->api_trait_get(RDD::API_TRAIT_SHADER_GROUP_HANDLE_ALIGNMENT);
	uint32_t shader_group_base_alignment = driver->api_trait_get(RDD::API_TRAIT_SHADER_GROUP_BASE_ALIGNMENT);

	uint32_t shader_group_stride = STEPIFY(shader_group_handle_size, shader_group_handle_alignment);
	uint32_t raygen_shader_group_stride = STEPIFY(shader_group_stride, shader_group_base_alignment);

	ERR_FAIL_COND(p_raygen_shader_index >= raytracing_list.state.raygen_shader_count);

	RDD::ShaderBindingTable raygen_sbt;
	raygen_sbt.buffer = raytracing_list.state.sbt_buffer;
	raygen_sbt.offset = p_raygen_shader_index * raygen_shader_group_stride;
	raygen_sbt.stride = raygen_shader_group_stride;
	raygen_sbt.size = raygen_shader_group_stride;

	RDD::ShaderBindingTable miss_sbt;
	miss_sbt.buffer = raytracing_list.state.sbt_buffer;
	miss_sbt.offset = STEPIFY(raytracing_list.state.raygen_shader_count * raygen_shader_group_stride, shader_group_base_alignment);
	miss_sbt.stride = shader_group_stride;
	miss_sbt.size = raytracing_list.state.miss_shader_count * shader_group_stride;

	HitShaderBindingTable *hit_sbt = hit_sbt_owner.get_or_null(p_hit_sbt);
	ERR_FAIL_NULL(hit_sbt);
	ERR_FAIL_COND(hit_sbt->raytracing_pipeline != raytracing_list.state.pipeline_driver_id);

	RDD::ShaderBindingTable rdd_hit_sbt;
	Error err = _hit_sbt_buffer_update(hit_sbt, p_hit_sbt, rdd_hit_sbt);
	ERR_FAIL_COND(err != OK);

	if (hit_sbt->draw_tracker != nullptr) {
		draw_graph.add_raytracing_list_usage(hit_sbt->draw_tracker, RDG::RESOURCE_USAGE_STORAGE_BUFFER_READ);
	}

	draw_graph.add_raytracing_list_trace_rays(raygen_sbt, miss_sbt, rdd_hit_sbt, p_width, p_height, p_depth);
#if defined(TESTS_ENABLED) && defined(MODULE_SKINNED_MESH_CAPTURE_TESTS_ENABLED)
	if (RenderCaptureTestHooks::write_census_active()) {
		for (uint32_t i = 0; i < raytracing_list.state.set_count; i++) {
			if (raytracing_list.state.sets[i].pipeline_expected_format != 0) {
				_capture_test_record_writable_set(raytracing_list.state.sets[i].uniform_set, RenderCaptureTestHooks::RAYTRACE);
			}
		}
	}
#endif
	raytracing_list.state.trace_count++;
}

void RenderingDevice::raytracing_list_end() {
	ERR_RENDER_THREAD_GUARD();
	ERR_FAIL_COND_MSG(_recording_error() != OK, "Recording is suspended after a frame fence wait failure.");

	ERR_FAIL_COND(!raytracing_list.active);

	draw_graph.add_raytracing_list_end();

	raytracing_list = RaytracingList();
}

/***********************/
/**** COMPUTE LISTS ****/
/***********************/

RenderingDevice::ComputeListID RenderingDevice::compute_list_begin() {
	ERR_RENDER_THREAD_GUARD_V(INVALID_ID);
	ERR_FAIL_COND_V_MSG(_recording_error() != OK, INVALID_ID, "Recording is suspended after a frame fence wait failure.");

	ERR_FAIL_COND_V_MSG(compute_list.active, INVALID_ID, "Only one compute list can be active at the same time.");
	ERR_FAIL_COND_V_MSG(raytracing_list.active, INVALID_ID, "Only one raytracing list can be active at the same time.");

	compute_list.active = true;

	draw_graph.add_compute_list_begin();

	return ID_TYPE_COMPUTE_LIST;
}

void RenderingDevice::compute_list_bind_compute_pipeline(ComputeListID p_list, RID p_compute_pipeline) {
	ERR_RENDER_THREAD_GUARD();
	ERR_FAIL_COND_MSG(_recording_error() != OK, "Recording is suspended after a frame fence wait failure.");

	ERR_FAIL_COND(p_list != ID_TYPE_COMPUTE_LIST);
	ERR_FAIL_COND(!compute_list.active);

	const ComputePipeline *pipeline = compute_pipeline_owner.get_or_null(p_compute_pipeline);
	ERR_FAIL_NULL(pipeline);
	ERR_FAIL_COND(pipeline->qualification_return && ((!qualification_recording && !_production_resource_permitted(p_compute_pipeline)) || pipeline->qualification_return->use_closed.load()));

	if (p_compute_pipeline == compute_list.state.pipeline) {
		return; // Redundant state, return.
	}

	compute_list.state.pipeline = p_compute_pipeline;

	draw_graph.add_compute_list_bind_pipeline(pipeline->driver_id);

	if (compute_list.state.pipeline_shader != pipeline->shader) {
		// Shader changed, so descriptor sets may become incompatible.

		uint32_t pcount = pipeline->set_formats.size(); // Formats count in this pipeline.
		compute_list.state.set_count = MAX(compute_list.state.set_count, pcount);
		const uint32_t *pformats = pipeline->set_formats.ptr(); // Pipeline set formats.

		uint32_t first_invalid_set = UINT32_MAX; // All valid by default.
		switch (driver->api_trait_get(RDD::API_TRAIT_SHADER_CHANGE_INVALIDATION)) {
			case RDD::SHADER_CHANGE_INVALIDATION_ALL_BOUND_UNIFORM_SETS: {
				first_invalid_set = 0;
			} break;
			case RDD::SHADER_CHANGE_INVALIDATION_INCOMPATIBLE_SETS_PLUS_CASCADE: {
				for (uint32_t i = 0; i < pcount; i++) {
					if (compute_list.state.sets[i].pipeline_expected_format != pformats[i]) {
						first_invalid_set = i;
						break;
					}
				}
			} break;
			case RDD::SHADER_CHANGE_INVALIDATION_ALL_OR_NONE_ACCORDING_TO_LAYOUT_HASH: {
				if (compute_list.state.pipeline_shader_layout_hash != pipeline->shader_layout_hash) {
					first_invalid_set = 0;
				}
			} break;
		}

		for (uint32_t i = 0; i < pcount; i++) {
			compute_list.state.sets[i].bound = compute_list.state.sets[i].bound && i < first_invalid_set;
			compute_list.state.sets[i].pipeline_expected_format = pformats[i];
		}

		for (uint32_t i = pcount; i < compute_list.state.set_count; i++) {
			// Unbind the ones above (not used) if exist.
			compute_list.state.sets[i].bound = false;
		}

		compute_list.state.set_count = pcount; // Update set count.

		if (pipeline->push_constant_size) {
#ifdef DEBUG_ENABLED
			compute_list.validation.pipeline_push_constant_supplied = false;
#endif
		}

		compute_list.state.pipeline_shader = pipeline->shader;
		compute_list.state.pipeline_shader_driver_id = pipeline->shader_driver_id;
		compute_list.state.pipeline_shader_layout_hash = pipeline->shader_layout_hash;
		compute_list.state.local_group_size[0] = pipeline->local_group_size[0];
		compute_list.state.local_group_size[1] = pipeline->local_group_size[1];
		compute_list.state.local_group_size[2] = pipeline->local_group_size[2];
	}

#ifdef DEBUG_ENABLED
	// Update compute pass pipeline info.
	compute_list.validation.pipeline_active = true;
	compute_list.validation.pipeline_push_constant_size = pipeline->push_constant_size;
#endif
}

void RenderingDevice::compute_list_bind_uniform_set(ComputeListID p_list, RID p_uniform_set, uint32_t p_index) {
	ERR_RENDER_THREAD_GUARD();
	ERR_FAIL_COND_MSG(_recording_error() != OK, "Recording is suspended after a frame fence wait failure.");

	ERR_FAIL_COND(p_list != ID_TYPE_COMPUTE_LIST);
	ERR_FAIL_COND(!compute_list.active);

#ifdef DEBUG_ENABLED
	ERR_FAIL_COND_MSG(p_index >= driver->limit_get(LIMIT_MAX_BOUND_UNIFORM_SETS) || p_index >= MAX_UNIFORM_SETS,
			"Attempting to bind a descriptor set (" + itos(p_index) + ") greater than what the hardware supports (" + itos(driver->limit_get(LIMIT_MAX_BOUND_UNIFORM_SETS)) + ").");
#endif

	UniformSet *uniform_set = uniform_set_owner.get_or_null(p_uniform_set);
	ERR_FAIL_NULL(uniform_set);
	ERR_FAIL_COND_MSG(_native_immutable_uniform_set_writes(*uniform_set), "An immutable native backing cannot be bound for shader writes.");
	ERR_FAIL_COND((!uniform_set->qualification_dependencies.is_empty() || uniform_set->qualification_return) && !qualification_recording && !_production_resource_permitted(p_uniform_set));

	if (p_index > compute_list.state.set_count) {
		compute_list.state.set_count = p_index;
	}

	compute_list.state.sets[p_index].uniform_set_driver_id = uniform_set->driver_id; // Update set pointer.
	compute_list.state.sets[p_index].bound = false; // Needs rebind.
	compute_list.state.sets[p_index].uniform_set_format = uniform_set->format;
	compute_list.state.sets[p_index].uniform_set = p_uniform_set;

#if 0
	{ // Validate that textures bound are not attached as framebuffer bindings.
		uint32_t attachable_count = uniform_set->attachable_textures.size();
		const RID *attachable_ptr = uniform_set->attachable_textures.ptr();
		uint32_t bound_count = draw_list_bound_textures.size();
		const RID *bound_ptr = draw_list_bound_textures.ptr();
		for (uint32_t i = 0; i < attachable_count; i++) {
			for (uint32_t j = 0; j < bound_count; j++) {
				ERR_FAIL_COND_MSG(attachable_ptr[i] == bound_ptr[j],
						"Attempted to use the same texture in framebuffer attachment and a uniform set, this is not allowed.");
			}
		}
	}
#endif
}

void RenderingDevice::compute_list_set_push_constant(ComputeListID p_list, const void *p_data, uint32_t p_data_size) {
	ERR_RENDER_THREAD_GUARD();
	ERR_FAIL_COND_MSG(_recording_error() != OK, "Recording is suspended after a frame fence wait failure.");

	ERR_FAIL_COND(p_list != ID_TYPE_COMPUTE_LIST);
	ERR_FAIL_COND(!compute_list.active);
	ERR_FAIL_COND_MSG(p_data_size > MAX_PUSH_CONSTANT_SIZE, "Push constants can't be bigger than 128 bytes to maintain compatibility.");

#ifdef DEBUG_ENABLED
	ERR_FAIL_COND_MSG(p_data_size != compute_list.validation.pipeline_push_constant_size,
			"This compute pipeline requires (" + itos(compute_list.validation.pipeline_push_constant_size) + ") bytes of push constant data, supplied: (" + itos(p_data_size) + ")");
#endif

	draw_graph.add_compute_list_set_push_constant(compute_list.state.pipeline_shader_driver_id, p_data, p_data_size);

	// Store it in the state in case we need to restart the compute list.
	memcpy(compute_list.state.push_constant_data, p_data, p_data_size);
	compute_list.state.push_constant_size = p_data_size;

#ifdef DEBUG_ENABLED
	compute_list.validation.pipeline_push_constant_supplied = true;
#endif
}

void RenderingDevice::compute_list_dispatch(ComputeListID list, uint32_t x, uint32_t y, uint32_t z) {
	_compute_list_dispatch_recorded(list, x, y, z);
}

RDG::RecordHandle RenderingDevice::_compute_list_dispatch_recorded(ComputeListID p_list, uint32_t p_x_groups, uint32_t p_y_groups, uint32_t p_z_groups) {
	ERR_RENDER_THREAD_GUARD_V(RDG::RecordHandle());
	ERR_FAIL_COND_V_MSG(_recording_error() != OK, RDG::RecordHandle(), "Recording is suspended after a frame fence wait failure.");

	ERR_FAIL_COND_V(p_list != ID_TYPE_COMPUTE_LIST, RDG::RecordHandle());
	ERR_FAIL_COND_V(!compute_list.active, RDG::RecordHandle());

#ifdef DEBUG_ENABLED
	ERR_FAIL_COND_V_MSG(p_x_groups == 0, RDG::RecordHandle(), "Dispatch amount of X compute groups (" + itos(p_x_groups) + ") is zero.");
	ERR_FAIL_COND_V_MSG(p_z_groups == 0, RDG::RecordHandle(), "Dispatch amount of Z compute groups (" + itos(p_z_groups) + ") is zero.");
	ERR_FAIL_COND_V_MSG(p_y_groups == 0, RDG::RecordHandle(), "Dispatch amount of Y compute groups (" + itos(p_y_groups) + ") is zero.");
	ERR_FAIL_COND_V_MSG(p_x_groups > driver->limit_get(LIMIT_MAX_COMPUTE_WORKGROUP_COUNT_X), RDG::RecordHandle(),
			"Dispatch amount of X compute groups (" + itos(p_x_groups) + ") is larger than device limit (" + itos(driver->limit_get(LIMIT_MAX_COMPUTE_WORKGROUP_COUNT_X)) + ")");
	ERR_FAIL_COND_V_MSG(p_y_groups > driver->limit_get(LIMIT_MAX_COMPUTE_WORKGROUP_COUNT_Y), RDG::RecordHandle(),
			"Dispatch amount of Y compute groups (" + itos(p_y_groups) + ") is larger than device limit (" + itos(driver->limit_get(LIMIT_MAX_COMPUTE_WORKGROUP_COUNT_Y)) + ")");
	ERR_FAIL_COND_V_MSG(p_z_groups > driver->limit_get(LIMIT_MAX_COMPUTE_WORKGROUP_COUNT_Z), RDG::RecordHandle(),
			"Dispatch amount of Z compute groups (" + itos(p_z_groups) + ") is larger than device limit (" + itos(driver->limit_get(LIMIT_MAX_COMPUTE_WORKGROUP_COUNT_Z)) + ")");
#endif

#ifdef DEBUG_ENABLED

	ERR_FAIL_COND_V_MSG(!compute_list.validation.pipeline_active, RDG::RecordHandle(), "No compute pipeline was set before attempting to draw.");

	if (compute_list.validation.pipeline_push_constant_size > 0) {
		// Using push constants, check that they were supplied.
		ERR_FAIL_COND_V_MSG(!compute_list.validation.pipeline_push_constant_supplied, RDG::RecordHandle(),
				"The shader in this pipeline requires a push constant to be set before drawing, but it's not present.");
	}

#endif

#ifdef DEBUG_ENABLED
	for (uint32_t i = 0; i < compute_list.state.set_count; i++) {
		if (compute_list.state.sets[i].pipeline_expected_format == 0) {
			// Nothing expected by this pipeline.
			continue;
		}

		if (compute_list.state.sets[i].pipeline_expected_format != compute_list.state.sets[i].uniform_set_format) {
			if (compute_list.state.sets[i].uniform_set_format == 0) {
				ERR_FAIL_V_MSG(RDG::RecordHandle(), "Uniforms were never supplied for set (" + itos(i) + ") at the time of drawing, which are required by the pipeline.");
			} else if (uniform_set_owner.owns(compute_list.state.sets[i].uniform_set)) {
				UniformSet *us = uniform_set_owner.get_or_null(compute_list.state.sets[i].uniform_set);
				ERR_FAIL_V_MSG(RDG::RecordHandle(), "Uniforms supplied for set (" + itos(i) + "):\n" + _shader_uniform_debug(us->shader_id, us->shader_set) + "\nare not the same format as required by the pipeline shader. Pipeline shader requires the following bindings:\n" + _shader_uniform_debug(compute_list.state.pipeline_shader));
			} else {
				ERR_FAIL_V_MSG(RDG::RecordHandle(), "Uniforms supplied for set (" + itos(i) + ", which was just freed) are not the same format as required by the pipeline shader. Pipeline shader requires the following bindings:\n" + _shader_uniform_debug(compute_list.state.pipeline_shader));
			}
		}
	}
#endif
	// Reserve pending-clear worker dependencies before descriptor graph prefixes.
	for (uint32_t i = 0; i < compute_list.state.set_count; ++i) {
		if (!compute_list.state.sets[i].pipeline_expected_format || compute_list.state.sets[i].bound) {
			continue;
		}
		UniformSet *set = uniform_set_owner.get_or_null(compute_list.state.sets[i].uniform_set);
		if (set) {
			const Error error = _uniform_set_admit_clear_transfers(set);
			if (error != OK) {
				return RDG::RecordHandle();
			}
		}
	}
	thread_local LocalVector<RDD::UniformSetID> valid_descriptor_ids;
	valid_descriptor_ids.clear();
	valid_descriptor_ids.resize(compute_list.state.set_count);

	uint32_t valid_set_count = 0;
	uint32_t first_set_index = 0;
	uint32_t last_set_index = 0;
	bool found_first_set = false;

	for (uint32_t i = 0; i < compute_list.state.set_count; i++) {
		if (compute_list.state.sets[i].pipeline_expected_format == 0) {
			// Nothing expected by this pipeline.
			continue;
		}

		if (!compute_list.state.sets[i].bound && !found_first_set) {
			first_set_index = i;
			found_first_set = true;
		}
		// Prepare descriptor sets if the API doesn't use pipeline barriers.
		if (!driver->api_trait_get(RDD::API_TRAIT_HONORS_PIPELINE_BARRIERS)) {
			draw_graph.add_compute_list_uniform_set_prepare_for_use(compute_list.state.pipeline_shader_driver_id, compute_list.state.sets[i].uniform_set_driver_id, i);
		}
	}

	// Bind descriptor sets.
	for (uint32_t i = first_set_index; i < compute_list.state.set_count; i++) {
		if (compute_list.state.sets[i].pipeline_expected_format == 0) {
			continue; // Nothing expected by this pipeline.
		}

		if (!compute_list.state.sets[i].bound) {
			// Descriptor set batching
			if (descriptor_set_batching) {
				// All good, see if this requires re-binding.
				if (i - last_set_index > 1) {
					// If the descriptor sets are not contiguous, bind the previous ones and start a new batch.
					draw_graph.add_compute_list_bind_uniform_sets(compute_list.state.pipeline_shader_driver_id, valid_descriptor_ids, first_set_index, valid_set_count);

					first_set_index = i;
					valid_set_count = 1;
					valid_descriptor_ids[0] = compute_list.state.sets[i].uniform_set_driver_id;
				} else {
					// Otherwise, keep storing in the current batch.
					valid_descriptor_ids[valid_set_count] = compute_list.state.sets[i].uniform_set_driver_id;
					valid_set_count++;
				}

				last_set_index = i;
			} else {
				draw_graph.add_compute_list_bind_uniform_set(compute_list.state.pipeline_shader_driver_id, compute_list.state.sets[i].uniform_set_driver_id, i);
			}
			UniformSet *uniform_set = uniform_set_owner.get_or_null(compute_list.state.sets[i].uniform_set);
			_uniform_set_update_shared(uniform_set);
			{
				const Error dependency_error = _uniform_set_update_clears(uniform_set);
				if (dependency_error != OK) {
					return RDG::RecordHandle();
				}
			}

			draw_graph.add_compute_list_usages(uniform_set->draw_trackers, uniform_set->draw_trackers_usage);
			compute_list.state.sets[i].bound = true;
		}
	}

	// Bind the remaining batch.
	if (valid_set_count > 0) {
		draw_graph.add_compute_list_bind_uniform_sets(compute_list.state.pipeline_shader_driver_id, valid_descriptor_ids, first_set_index, valid_set_count);
	}
	const RDG::RecordHandle recorded = draw_graph.add_compute_list_dispatch(p_x_groups, p_y_groups, p_z_groups);
#ifdef TESTS_ENABLED
	if (recorded.is_valid()) {
		test_compute_dispatch_count++;
	}
#endif
#if defined(TESTS_ENABLED) && defined(MODULE_SKINNED_MESH_CAPTURE_TESTS_ENABLED)
	if (RenderCaptureTestHooks::write_census_active()) {
		for (uint32_t i = 0; i < compute_list.state.set_count; i++) {
			if (compute_list.state.sets[i].pipeline_expected_format != 0) {
				_capture_test_record_writable_set(compute_list.state.sets[i].uniform_set, RenderCaptureTestHooks::COMPUTE);
			}
		}
	}
#endif
	compute_list.state.dispatch_count++;
	return recorded;
}

void RenderingDevice::compute_list_dispatch_threads(ComputeListID p_list, uint32_t p_x_threads, uint32_t p_y_threads, uint32_t p_z_threads) {
	ERR_RENDER_THREAD_GUARD();
	ERR_FAIL_COND_MSG(_recording_error() != OK, "Recording is suspended after a frame fence wait failure.");

	ERR_FAIL_COND(p_list != ID_TYPE_COMPUTE_LIST);
	ERR_FAIL_COND(!compute_list.active);

#ifdef DEBUG_ENABLED
	ERR_FAIL_COND_MSG(p_x_threads == 0, "Dispatch amount of X compute threads (" + itos(p_x_threads) + ") is zero.");
	ERR_FAIL_COND_MSG(p_y_threads == 0, "Dispatch amount of Y compute threads (" + itos(p_y_threads) + ") is zero.");
	ERR_FAIL_COND_MSG(p_z_threads == 0, "Dispatch amount of Z compute threads (" + itos(p_z_threads) + ") is zero.");
#endif

#ifdef DEBUG_ENABLED

	ERR_FAIL_COND_MSG(!compute_list.validation.pipeline_active, "No compute pipeline was set before attempting to draw.");

	if (compute_list.validation.pipeline_push_constant_size > 0) {
		// Using push constants, check that they were supplied.
		ERR_FAIL_COND_MSG(!compute_list.validation.pipeline_push_constant_supplied,
				"The shader in this pipeline requires a push constant to be set before drawing, but it's not present.");
	}

#endif

	compute_list_dispatch(p_list, Math::division_round_up(p_x_threads, compute_list.state.local_group_size[0]), Math::division_round_up(p_y_threads, compute_list.state.local_group_size[1]), Math::division_round_up(p_z_threads, compute_list.state.local_group_size[2]));
}

void RenderingDevice::compute_list_dispatch_indirect(ComputeListID p_list, RID p_buffer, uint32_t p_offset) {
	ERR_RENDER_THREAD_GUARD();
	ERR_FAIL_COND_MSG(_recording_error() != OK, "Recording is suspended after a frame fence wait failure.");

	ERR_FAIL_COND(p_list != ID_TYPE_COMPUTE_LIST);
	ERR_FAIL_COND(!compute_list.active);

	Buffer *buffer = storage_buffer_owner.get_or_null(p_buffer);
	ERR_FAIL_NULL(buffer);
	ERR_FAIL_COND(buffer->qualification_return);

	ERR_FAIL_COND_MSG(!buffer->usage.has_flag(RDD::BUFFER_USAGE_INDIRECT_BIT), "Buffer provided was not created to do indirect dispatch.");

	ERR_FAIL_COND_MSG(p_offset % 4 != 0, "Offset must be a multiple of 4 bytes.");
	ERR_FAIL_COND_MSG(uint64_t(p_offset) + 12 > buffer->size, "Offset provided (+12) is past the end of buffer.");

#ifdef DEBUG_ENABLED

	ERR_FAIL_COND_MSG(!compute_list.validation.pipeline_active, "No compute pipeline was set before attempting to draw.");

	if (compute_list.validation.pipeline_push_constant_size > 0) {
		// Using push constants, check that they were supplied.
		ERR_FAIL_COND_MSG(!compute_list.validation.pipeline_push_constant_supplied,
				"The shader in this pipeline requires a push constant to be set before drawing, but it's not present.");
	}

#endif

#ifdef DEBUG_ENABLED
	for (uint32_t i = 0; i < compute_list.state.set_count; i++) {
		if (compute_list.state.sets[i].pipeline_expected_format == 0) {
			// Nothing expected by this pipeline.
			continue;
		}

		if (compute_list.state.sets[i].pipeline_expected_format != compute_list.state.sets[i].uniform_set_format) {
			if (compute_list.state.sets[i].uniform_set_format == 0) {
				ERR_FAIL_MSG("Uniforms were never supplied for set (" + itos(i) + ") at the time of drawing, which are required by the pipeline.");
			} else if (uniform_set_owner.owns(compute_list.state.sets[i].uniform_set)) {
				UniformSet *us = uniform_set_owner.get_or_null(compute_list.state.sets[i].uniform_set);
				ERR_FAIL_MSG("Uniforms supplied for set (" + itos(i) + "):\n" + _shader_uniform_debug(us->shader_id, us->shader_set) + "\nare not the same format as required by the pipeline shader. Pipeline shader requires the following bindings:\n" + _shader_uniform_debug(compute_list.state.pipeline_shader));
			} else {
				ERR_FAIL_MSG("Uniforms supplied for set (" + itos(i) + ", which was just freed) are not the same format as required by the pipeline shader. Pipeline shader requires the following bindings:\n" + _shader_uniform_debug(compute_list.state.pipeline_shader));
			}
		}
	}
#endif
	// Reserve pending-clear worker dependencies before descriptor graph prefixes.
	for (uint32_t i = 0; i < compute_list.state.set_count; ++i) {
		if (!compute_list.state.sets[i].pipeline_expected_format || compute_list.state.sets[i].bound) {
			continue;
		}
		UniformSet *set = uniform_set_owner.get_or_null(compute_list.state.sets[i].uniform_set);
		if (set) {
			const Error error = _uniform_set_admit_clear_transfers(set);
			if (error != OK) {
				return;
			}
		}
	}
	thread_local LocalVector<RDD::UniformSetID> valid_descriptor_ids;
	valid_descriptor_ids.clear();
	valid_descriptor_ids.resize(compute_list.state.set_count);

	uint32_t valid_set_count = 0;
	uint32_t first_set_index = 0;
	uint32_t last_set_index = 0;
	bool found_first_set = false;

	for (uint32_t i = 0; i < compute_list.state.set_count; i++) {
		if (compute_list.state.sets[i].pipeline_expected_format == 0) {
			// Nothing expected by this pipeline.
			continue;
		}

		if (!compute_list.state.sets[i].bound && !found_first_set) {
			first_set_index = i;
			found_first_set = true;
		}

		// Prepare descriptor sets if the API doesn't use pipeline barriers.
		if (!driver->api_trait_get(RDD::API_TRAIT_HONORS_PIPELINE_BARRIERS)) {
			draw_graph.add_compute_list_uniform_set_prepare_for_use(compute_list.state.pipeline_shader_driver_id, compute_list.state.sets[i].uniform_set_driver_id, i);
		}
	}

	// Bind descriptor sets.
	for (uint32_t i = first_set_index; i < compute_list.state.set_count; i++) {
		if (compute_list.state.sets[i].pipeline_expected_format == 0) {
			continue; // Nothing expected by this pipeline.
		}

		if (!compute_list.state.sets[i].bound) {
			// All good, see if this requires re-binding.
			if (i - last_set_index > 1) {
				// If the descriptor sets are not contiguous, bind the previous ones and start a new batch.
				draw_graph.add_compute_list_bind_uniform_sets(compute_list.state.pipeline_shader_driver_id, valid_descriptor_ids, first_set_index, valid_set_count);

				first_set_index = i;
				valid_set_count = 1;
				valid_descriptor_ids[0] = compute_list.state.sets[i].uniform_set_driver_id;
			} else {
				// Otherwise, keep storing in the current batch.
				valid_descriptor_ids[valid_set_count] = compute_list.state.sets[i].uniform_set_driver_id;
				valid_set_count++;
			}

			last_set_index = i;

			UniformSet *uniform_set = uniform_set_owner.get_or_null(compute_list.state.sets[i].uniform_set);
			_uniform_set_update_shared(uniform_set);
			{
				const Error dependency_error = _uniform_set_update_clears(uniform_set);
				if (dependency_error != OK) {
					return;
				}
			}

			draw_graph.add_compute_list_usages(uniform_set->draw_trackers, uniform_set->draw_trackers_usage);
			compute_list.state.sets[i].bound = true;
		}
	}

	// Bind the remaining batch.
	if (valid_set_count > 0) {
		draw_graph.add_compute_list_bind_uniform_sets(compute_list.state.pipeline_shader_driver_id, valid_descriptor_ids, first_set_index, valid_set_count);
	}

	draw_graph.add_compute_list_dispatch_indirect(buffer->driver_id, p_offset);
#if defined(TESTS_ENABLED) && defined(MODULE_SKINNED_MESH_CAPTURE_TESTS_ENABLED)
	if (RenderCaptureTestHooks::write_census_active()) {
		for (uint32_t i = 0; i < compute_list.state.set_count; i++) {
			if (compute_list.state.sets[i].pipeline_expected_format != 0) {
				_capture_test_record_writable_set(compute_list.state.sets[i].uniform_set, RenderCaptureTestHooks::COMPUTE_INDIRECT);
			}
		}
	}
#endif
	compute_list.state.dispatch_count++;

	if (buffer->draw_tracker != nullptr) {
		draw_graph.add_compute_list_usage(buffer->draw_tracker, RDG::RESOURCE_USAGE_INDIRECT_BUFFER_READ);
	}

	{
		const Error dependency_error = _check_transfer_worker_buffer(buffer);
		if (dependency_error != OK) {
			return;
		}
	}
}

void RenderingDevice::compute_list_add_barrier(ComputeListID p_list) {
	ERR_RENDER_THREAD_GUARD();
	ERR_FAIL_COND_MSG(_recording_error() != OK, "Recording is suspended after a frame fence wait failure.");

	ERR_FAIL_COND(p_list != ID_TYPE_COMPUTE_LIST);
	ERR_FAIL_COND(!compute_list.active);

	compute_list_barrier_state = compute_list.state;
	compute_list_end();
	compute_list_begin();

	if (compute_list_barrier_state.pipeline.is_valid()) {
		compute_list_bind_compute_pipeline(p_list, compute_list_barrier_state.pipeline);
	}

	for (uint32_t i = 0; i < compute_list_barrier_state.set_count; i++) {
		if (compute_list_barrier_state.sets[i].uniform_set.is_valid()) {
			compute_list_bind_uniform_set(p_list, compute_list_barrier_state.sets[i].uniform_set, i);
		}
	}

	if (compute_list_barrier_state.push_constant_size > 0) {
		compute_list_set_push_constant(p_list, compute_list_barrier_state.push_constant_data, compute_list_barrier_state.push_constant_size);
	}
}

void RenderingDevice::compute_list_end() {
	_compute_list_end_recorded();
}

RDG::RecordHandle RenderingDevice::_compute_list_end_recorded() {
	ERR_RENDER_THREAD_GUARD_V(RDG::RecordHandle());
	ERR_FAIL_COND_V_MSG(_recording_error() != OK, RDG::RecordHandle(), "Recording is suspended after a frame fence wait failure.");
	ERR_FAIL_COND_V(!compute_list.active, RDG::RecordHandle());
	const auto recorded = draw_graph.add_compute_list_end();
	compute_list = ComputeList();
	return recorded;
}

#ifndef DISABLE_DEPRECATED
void RenderingDevice::barrier(BitField<BarrierMask> p_from, BitField<BarrierMask> p_to) {
	WARN_PRINT("Deprecated. Barriers are automatically inserted by RenderingDevice.");
}

void RenderingDevice::full_barrier() {
	WARN_PRINT("Deprecated. Barriers are automatically inserted by RenderingDevice.");
}
#endif

/*************************/
/**** TRANSFER WORKER ****/
/*************************/

static uint32_t _get_alignment_offset(uint32_t p_offset, uint32_t p_required_align) {
	uint32_t alignment_offset = (p_required_align > 0) ? (p_offset % p_required_align) : 0;
	if (alignment_offset != 0) {
		// If a particular alignment is required, add the offset as part of the required size.
		alignment_offset = p_required_align - alignment_offset;
	}

	return alignment_offset;
}

RenderingDevice::TransferWorker *RenderingDevice::_acquire_transfer_worker(uint32_t p_transfer_size, uint32_t p_required_align, uint32_t &r_staging_offset) {
	// Find the first worker that is not currently executing anything and has enough size for the transfer.
	// If no workers are available, we make a new one. If we're not allowed to make new ones, we wait until one of them is available.
	TransferWorker *transfer_worker = nullptr;
	uint32_t available_list_index = 0;
	bool transfer_worker_busy = true;
	bool transfer_worker_full = true;
	{
		MutexLock pool_lock(transfer_worker_pool_mutex);

		// If no workers are available and we've reached the max pool capacity, wait until one of them becomes available.
		bool transfer_worker_pool_full = transfer_worker_pool_size >= transfer_worker_pool_max_size;
		while (transfer_worker_pool_available_list.is_empty() && transfer_worker_pool_full) {
			bool all_workers_poisoned = transfer_worker_pool_size > 0;
			for (uint32_t worker_index = 0; worker_index < transfer_worker_pool_size; worker_index++) {
				TransferWorker *worker = transfer_worker_pool[worker_index];
				MutexLock worker_lock(worker->thread_mutex);
				if (!worker->poisoned && !worker->qualification_batch_sealed && !worker->height_observer_pending) {
					all_workers_poisoned = false;
					break;
				}
			}
			if (all_workers_poisoned) {
				return nullptr;
			}
			transfer_worker_pool_condition.wait(pool_lock);
		}

		// Look at all available workers first.
		for (uint32_t i = 0; i < transfer_worker_pool_available_list.size(); i++) {
			uint32_t worker_index = transfer_worker_pool_available_list[i];
			TransferWorker *candidate_worker = transfer_worker_pool[worker_index];
			candidate_worker->thread_mutex.lock();

			// Figure out if the worker can fit the transfer.
			uint32_t alignment_offset = _get_alignment_offset(candidate_worker->staging_buffer_size_in_use, p_required_align);
			uint32_t required_size = candidate_worker->staging_buffer_size_in_use + p_transfer_size + alignment_offset;
			bool candidate_worker_busy = candidate_worker->submitted;
			bool candidate_worker_poisoned = candidate_worker->poisoned || candidate_worker->qualification_batch_sealed || candidate_worker->height_observer_pending;
			bool candidate_worker_full = required_size > candidate_worker->staging_buffer_size_allocated;
			bool pick_candidate = false;
			if (!candidate_worker_poisoned && !candidate_worker_busy && !candidate_worker_full) {
				// A worker that can fit the transfer and is not waiting for a previous execution is the best possible candidate.
				pick_candidate = true;
			} else if (!candidate_worker_poisoned && !candidate_worker_busy) {
				// The worker can't fit the transfer but it's not currently doing anything.
				// We pick it as a possible candidate if the current one is busy.
				pick_candidate = transfer_worker_busy;
			} else if (!candidate_worker_poisoned && !candidate_worker_full) {
				// The worker can fit the transfer but it's currently executing previous work.
				// We pick it as a possible candidate if the current one is both busy and full.
				pick_candidate = transfer_worker_busy && transfer_worker_full;
			} else if (!candidate_worker_poisoned && transfer_worker == nullptr) {
				// The worker can't fit the transfer and it's currently executing work, so it's the worst candidate.
				// We only pick if no candidate has been picked yet.
				pick_candidate = true;
			}

			if (pick_candidate) {
				if (transfer_worker != nullptr) {
					// Release the lock for the worker that was picked previously.
					transfer_worker->thread_mutex.unlock();
				}

				// Keep the lock active for this worker.
				transfer_worker = candidate_worker;
				transfer_worker_busy = candidate_worker_busy;
				transfer_worker_full = candidate_worker_full;
				available_list_index = i;

				if (!transfer_worker_busy && !transfer_worker_full) {
					// Best possible candidate, stop searching early.
					break;
				}
			} else {
				// Release the lock for the candidate.
				candidate_worker->thread_mutex.unlock();
			}
		}

		if (transfer_worker != nullptr) {
			// A worker was picked, remove it from the available list.
			transfer_worker_pool_available_list.remove_at(available_list_index);
		} else {
			// Available entries can have been poisoned by an aggregate wait.
			// They retain ownership but cannot be lent or replaced past capacity.
			if (transfer_worker_pool_full) {
				return nullptr;
			}

			// No existing worker was picked, we create a new one.
			uint32_t transfer_worker_index = transfer_worker_pool_size;

			transfer_worker = memnew(TransferWorker);
			transfer_worker->command_fence = driver->fence_create();
			if (!transfer_worker->command_fence) {
				memdelete(transfer_worker);
				return nullptr;
			}
			transfer_worker->command_pool = driver->command_pool_create(transfer_queue_family, RDD::COMMAND_BUFFER_TYPE_PRIMARY);
			if (!transfer_worker->command_pool) {
				driver->fence_free(transfer_worker->command_fence);
				memdelete(transfer_worker);
				return nullptr;
			}
			transfer_worker->command_buffer = driver->command_buffer_create(transfer_worker->command_pool);
			if (!transfer_worker->command_buffer) {
				driver->command_pool_free(transfer_worker->command_pool);
				driver->fence_free(transfer_worker->command_fence);
				memdelete(transfer_worker);
				return nullptr;
			}
			++transfer_worker_pool_size;
			transfer_worker->index = transfer_worker_index;
			transfer_worker_pool[transfer_worker_index] = transfer_worker;
			transfer_worker_operation_used_by_draw[transfer_worker_index] = 0;
			transfer_worker->thread_mutex.lock();
		}
	}

	if (transfer_worker->submitted) {
		// Wait for the worker if the command buffer was submitted but it hasn't finished processing yet.
		if (!_wait_for_transfer_worker(transfer_worker, true)) {
			_release_transfer_worker(transfer_worker);
			return nullptr;
		}
	}

	uint32_t alignment_offset = _get_alignment_offset(transfer_worker->staging_buffer_size_in_use, p_required_align);
	transfer_worker->max_transfer_size = MAX(transfer_worker->max_transfer_size, p_transfer_size);

	uint32_t required_size = transfer_worker->staging_buffer_size_in_use + p_transfer_size + alignment_offset;
	if (required_size > transfer_worker->staging_buffer_size_allocated) {
		// If there's not enough bytes to use on the staging buffer, we submit everything pending from the worker and wait for the work to be finished.
		if (transfer_worker->recording || transfer_worker->submission_pending) {
			if (transfer_worker->recording) {
				_end_transfer_worker(transfer_worker);
			}
			_submit_transfer_worker(transfer_worker);
			if (transfer_worker->poisoned) {
				_release_transfer_worker(transfer_worker);
				return nullptr;
			}
		}

		if (transfer_worker->submitted) {
			if (!_wait_for_transfer_worker(transfer_worker, true)) {
				_release_transfer_worker(transfer_worker);
				return nullptr;
			}
		}

		alignment_offset = 0;

		// If the staging buffer can't fit the transfer, we recreate the buffer.
		const uint32_t expected_buffer_size_minimum = 16 * 1024;
		uint32_t expected_buffer_size = MAX(transfer_worker->max_transfer_size, expected_buffer_size_minimum);
		if (expected_buffer_size > transfer_worker->staging_buffer_size_allocated) {
			if (transfer_worker->staging_buffer.id != 0) {
				driver->buffer_free(transfer_worker->staging_buffer);
			}

			uint32_t new_staging_buffer_size = Math::next_power_of_2(expected_buffer_size);
			transfer_worker->staging_buffer_size_allocated = 0;
			transfer_worker->staging_buffer = driver->buffer_create(new_staging_buffer_size, RDD::BUFFER_USAGE_TRANSFER_FROM_BIT, RDD::MEMORY_ALLOCATION_TYPE_CPU, frames_drawn);
			if (!transfer_worker->staging_buffer) {
				transfer_worker->poisoned = true;
				transfer_worker->completion_error = ERR_OUT_OF_MEMORY;
				_release_transfer_worker(transfer_worker);
				return nullptr;
			}
			transfer_worker->staging_buffer_size_allocated = new_staging_buffer_size;
		}
	}

	// Add the alignment before storing the offset that will be returned.
	transfer_worker->staging_buffer_size_in_use += alignment_offset;

	// Store the offset to return and increment the current size.
	r_staging_offset = transfer_worker->staging_buffer_size_in_use;
	transfer_worker->staging_buffer_size_in_use += p_transfer_size;

	if (!transfer_worker->recording) {
		// Begin the command buffer if the worker wasn't recording yet.
		if (!driver->command_buffer_begin(transfer_worker->command_buffer)) {
			transfer_worker->poisoned = true;
			transfer_worker->completion_error = ERR_CANT_CREATE;
			_release_transfer_worker(transfer_worker);
			return nullptr;
		}
		transfer_worker->recording = true;
	}

	return transfer_worker;
}

void RenderingDevice::_release_transfer_worker(TransferWorker *p_transfer_worker) {
	const bool poisoned = p_transfer_worker->poisoned;
	p_transfer_worker->thread_mutex.unlock();

	{
		// Drop the worker lock before taking the pool lock: acquisition and
		// aggregate waits use pool -> worker. Even retirement must cross this
		// mutex so it cannot notify between the pool predicate and wait entry.
		MutexLock pool_lock(transfer_worker_pool_mutex);
		if (!poisoned) {
			transfer_worker_pool_available_list.push_back(p_transfer_worker->index);
		}
	}
	if (poisoned) {
		// Retirement can make the exhausted pool terminal for every waiter.
		transfer_worker_pool_condition.notify_all();
	} else {
		transfer_worker_pool_condition.notify_one();
	}
}

void RenderingDevice::_end_transfer_worker(TransferWorker *p_transfer_worker) {
	driver->command_buffer_end(p_transfer_worker->command_buffer);
	p_transfer_worker->recording = false;
	p_transfer_worker->submission_pending = true;
}

Error RenderingDevice::_native_height_join_worker(TransferWorker *worker, uint64_t first, uint64_t last, const RDD::QueueCallReport &report) {
	if (!worker->height_watcher_count) {
		return OK;
	}
	// Watchers freeze acquisition until original RT has captured and retained
	// this batch. A background waiter cannot mutate the range or recycle it.
	if (render_thread_id != Thread::get_caller_id()) {
		return ERR_BUSY;
	}
	Error result = OK;
	for (uint32_t w = 0; w < worker->height_watcher_count; ++w) {
		auto &watcher = worker->height_watchers[w];
		const Ref<NativeHeightRange> &range = watcher.range;
		auto &dependency = range->workers[watcher.dependency];
		if (!dependency.attached && !dependency.issue_attempted) {
			if (dependency.worker != worker->index || dependency.operation < first || dependency.operation > last) {
				if (result == OK) {
					result = ERR_BUSY;
				}
				continue;
			}
			// Zero-prefix retry leaves the exact watcher unresolved, with no
			// invented source and no sticky failure for a later actual issue.
			if (report.error == ERR_BUSY && report.calls.is_empty()) {
				if (result == OK) {
					result = ERR_BUSY;
				}
				continue;
			}
			bool valid = report.error == OK && report.issued && report.submission_serial && report.native_issue_count &&
					report.native_issue_count <= range->limits.worker_sources;
			uint32_t count = 0;
			for (const auto &fact : report.calls) {
				if (fact.presentation_only || fact.issue == RDD::IssueFact::NotIssued) {
					continue;
				}
				++count;
				if (!fact.serial || !fact.completion_serial || fact.completion == RDD::CompletionCapability::Disabled) {
					valid = false;
				}
			}
			const bool capacity = count <= range->limits.worker_sources && range->source_count + count <= range->limits.native_sources && range->worker_edge_count + count <= range->worker_edges.size();
			if (!count || count > range->limits.worker_sources || range->source_count + count > range->limits.native_sources ||
					range->worker_edge_count + count > range->worker_edges.size()) {
				valid = false;
			}
			// Failed reports can contain a real prefix. Preserve its observable
			// facts and quarantine the original worker; never call it Complete.
			if (!valid) {
				range->issue_error = report.error != OK ? report.error : ERR_INVALID_DATA;
				range->uncaptured_prefix = true;
				if (result == OK || result == ERR_BUSY) {
					result = range->issue_error;
				}
			}
			dependency.issue_attempted = true;
			dependency.first_operation = first;
			dependency.last_operation = last;
			for (const auto &fact : report.calls) {
				if (!capacity || fact.presentation_only || fact.issue == RDD::IssueFact::NotIssued || !fact.serial || !fact.completion_serial || fact.completion == RDD::CompletionCapability::Disabled) {
					continue;
				}
				if (fact.error != OK) {
					range->issue_error = fact.error;
				}
				uint32_t source = range->source_count;
				for (uint32_t i = 0; i < range->source_count; ++i) {
					const auto &old = range->sources[i].original;
					if (old.serial == fact.serial) {
						source = i;
						if (old.completion_serial != fact.completion_serial || old.queue_index != fact.queue_index || old.issue != fact.issue ||
								old.error != fact.error || old.native_code != fact.native_code || old.completion != fact.completion) {
							range->issue_error = ERR_INVALID_DATA;
							range->uncaptured_prefix = true;
						}
						break;
					}
				}
				if (source == range->source_count) {
					range->sources[range->source_count++].original = fact;
				}
				range->worker_edges[range->worker_edge_count++] = { watcher.dependency, source };
			}
			dependency.attached = valid && range->issue_error == OK && !range->uncaptured_prefix;
		}
		const Error retained = _native_height_retain_sources(range);
		if (range->issue_error != OK && (result == OK || result == ERR_BUSY)) {
			result = range->issue_error;
		}
		if (retained != OK && (result == OK || result == ERR_BUSY)) {
			result = retained;
		}
	}
	worker->height_observer_pending = result != OK;
	return result;
}

void RenderingDevice::_submit_transfer_worker(TransferWorker *p_transfer_worker, VectorView<RDD::SemaphoreID> p_signal_semaphores, bool p_operations_locked) {
	QualificationSourceAdmissionScope source_admission(*this, p_transfer_worker->index);
	RenderingDeviceDriver::QueueCallReport report;
#if defined(TESTS_ENABLED) && defined(MODULE_SKINNED_MESH_CAPTURE_TESTS_ENABLED)
	const Error skinned_mesh_capture_test_driver_result =
#endif
			driver->command_queue_execute_and_present_observed(transfer_queue, {}, p_transfer_worker->command_buffer, p_signal_semaphores, p_transfer_worker->command_fence, {}, report);
#if defined(TESTS_ENABLED) && defined(MODULE_SKINNED_MESH_CAPTURE_TESTS_ENABLED)
	RenderCaptureTestHooks::observe_driver_result(uint64_t(get_instance_id()), 1, p_transfer_worker->command_fence.id, skinned_mesh_capture_test_driver_result);
	if (skinned_mesh_capture_test_driver_result != OK) {
		capture_test_coverage_failed.store(true);
	}
#endif

	// Zero-prefix contention did not submit this ended recording. Keep its
	// original pending batch and all reservations so retry can issue it once.
	if (report.error == ERR_BUSY && report.calls.is_empty()) {
		return;
	}
	// Only an original production attachment enrolls this device in capture.
	// Ordinary transfers keep their original issue/error path without a frame
	// dependency. Never erase an already pending production observation here.
	if (auto *registry = production_entity_registry.load(std::memory_order_acquire)) {
		// Join the actual covered worker operation range while its original lock
		// still excludes recycle. Never use an unrelated later last_submission.
		if (!p_operations_locked) {
			p_transfer_worker->operations_mutex.lock();
		}
		const uint64_t first_operation = p_transfer_worker->operations_processed + 1;
		const uint64_t last_operation = p_transfer_worker->operations_counter;
		if (!p_operations_locked) {
			p_transfer_worker->operations_mutex.unlock();
		}
		GDExtensionDeviceExit::Registry::Lock lock(*registry);
		registry->production_capture_issue_locked(*this, UINT32_MAX, { report }, report.error, p_transfer_worker->index, first_operation, last_operation);
		const GDExtensionDeviceExit::NativeSourceIssueView view{ report };
		p_transfer_worker->production_observer_pending = registry->production_retain_sources_locked(*this, frame, frames[frame].qualification_epoch, &view) != OK;
	}
	p_transfer_worker->submission_pending = false;
	p_transfer_worker->last_submission = report;
	p_transfer_worker->submission_serial = report.submission_serial;
	p_transfer_worker->submitted = report.issued;
	if (report.error != OK || !report.issued || report.submission_serial == 0) {
		// A rejected submission leaves an ended recording that cannot be reused
		// or reported as a successfully completed upload.
		p_transfer_worker->poisoned = true;
		p_transfer_worker->completion_error = report.error != OK ? report.error : ERR_CANT_CREATE;
		if (!p_operations_locked) {
			p_transfer_worker->operations_mutex.lock();
		}
		_native_height_join_worker(p_transfer_worker, p_transfer_worker->operations_processed + 1, p_transfer_worker->operations_counter, report);
		if (!p_operations_locked) {
			p_transfer_worker->operations_mutex.unlock();
		}
		return;
	}

	for (uint32_t i = 0; i < p_signal_semaphores.size(); i++) {
		// Indicate the frame should wait on these semaphores before executing the main command buffer.
		frames[frame].semaphores_to_wait_on.push_back(p_signal_semaphores[i]);
	}

	if (!p_operations_locked) {
		p_transfer_worker->operations_mutex.lock();
	}
	p_transfer_worker->operations_submitted = p_transfer_worker->operations_counter;
	_native_height_join_worker(p_transfer_worker, p_transfer_worker->operations_processed + 1, p_transfer_worker->operations_submitted, report);
	if (!p_operations_locked) {
		p_transfer_worker->operations_mutex.unlock();
	}
}

Error RenderingDevice::_production_join_transfer_worker(TransferWorker *worker, bool operations_locked) {
	auto *registry = production_entity_registry.load(std::memory_order_acquire);
	if (!registry) {
		// Missing enrollment is a no-op only for an ordinary worker. An existing
		// production observation must still block its original fence recycle.
		return worker->production_observer_pending ? ERR_BUSY : OK;
	}
	if (!operations_locked) {
		worker->operations_mutex.lock();
	}
	const uint64_t first = worker->operations_processed + 1;
	const uint64_t last = worker->operations_submitted;
	if (!operations_locked) {
		worker->operations_mutex.unlock();
	}
	if (first > last || worker->last_submission.calls.is_empty()) {
		return worker->production_observer_pending ? ERR_BUSY : OK;
	}
	GDExtensionDeviceExit::Registry::Lock lock(*registry);
	registry->production_capture_issue_locked(*this, UINT32_MAX, { worker->last_submission }, worker->last_submission.error, worker->index, first, last);
	const GDExtensionDeviceExit::NativeSourceIssueView view{ worker->last_submission };
	const Error retained = registry->production_retain_sources_locked(*this, frame, frames[frame].qualification_epoch, &view);
	worker->production_observer_pending = retained != OK;
	return retained;
}

Error RenderingDevice::_production_retain_frame_sources(uint32_t index) {
	auto *registry = production_entity_registry.load(std::memory_order_acquire);
	if (!registry) {
		return OK;
	}
	GDExtensionDeviceExit::Registry::Lock lock(*registry);
	return registry->production_retain_sources_locked(*this, index, frames[index].qualification_epoch);
}

bool RenderingDevice::_wait_for_transfer_worker(TransferWorker *p_transfer_worker, bool p_blocking) {
	// A frame joining this exact operation must retain its original completion
	// sources before the worker fence can recycle them or reuse its staging.
	if (p_transfer_worker->qualification_observer_pending || p_transfer_worker->submission_pending) {
		return false;
	}
	if (p_transfer_worker->poisoned) {
		return false;
	}
	if (!p_transfer_worker->submitted) {
		return true;
	}
	if (p_transfer_worker->height_watcher_count) {
		MutexLock operations_lock(p_transfer_worker->operations_mutex);
		if (_native_height_join_worker(p_transfer_worker, p_transfer_worker->operations_processed + 1, p_transfer_worker->operations_submitted, p_transfer_worker->last_submission) != OK) {
			return false;
		}
	}
	const uint64_t serial = p_transfer_worker->submission_serial;
	RDD::FencePollResult poll = p_transfer_worker->last_completion;
	if (!poll.matches(serial)) {
		if (!_native_disposal_admit(NativeDisposalWork::POLL)) {
			return false;
		}
		poll = p_blocking ? driver->fence_wait_observed(p_transfer_worker->command_fence, serial) : driver->fence_poll(p_transfer_worker->command_fence, serial);
		p_transfer_worker->last_completion = poll;
	}
	if (!poll.matches(serial)) {
		if (serial == 0 || poll.native_error || poll.completed || (poll.completed_serial != 0 && poll.completed_serial != serial)) {
			p_transfer_worker->poisoned = true;
			p_transfer_worker->completion_error = FAILED;
		}
		return false;
	}
	if (_production_join_transfer_worker(p_transfer_worker, false) != OK) {
		return false;
	}
	if (!_native_disposal_admit(NativeDisposalWork::RECYCLE)) {
		return false;
	}
	Error recycle_error = p_blocking ? driver->fence_recycle_wait(p_transfer_worker->command_fence, serial) : driver->fence_recycle(p_transfer_worker->command_fence, serial);
	if (recycle_error == ERR_BUSY) {
		// Nonblocking source contention retains this exact submitted operation.
		// It is still owned and can be observed/recycled on a later attempt.
		return false;
	}
	if (recycle_error != OK) {
		p_transfer_worker->poisoned = true;
		p_transfer_worker->completion_error = recycle_error;
		return false;
	}
	if (!driver->command_pool_reset(p_transfer_worker->command_pool)) {
		p_transfer_worker->poisoned = true;
		p_transfer_worker->completion_error = ERR_CANT_CREATE;
		return false;
	}
	p_transfer_worker->staging_buffer_size_in_use = 0;
	for (uint32_t i = 0; i < p_transfer_worker->height_watcher_count; ++i) {
		p_transfer_worker->height_watchers[i].range.unref();
	}
	p_transfer_worker->height_watcher_count = 0;
	p_transfer_worker->height_observer_pending = false;
	p_transfer_worker->submitted = false;
	p_transfer_worker->submission_serial = 0;

	{
		MutexLock lock(p_transfer_worker->operations_mutex);
		p_transfer_worker->operations_processed = p_transfer_worker->operations_submitted;
	}

	_flush_barriers_for_transfer_worker(p_transfer_worker);
	return true;
}

void RenderingDevice::_flush_barriers_for_transfer_worker(TransferWorker *p_transfer_worker, bool p_barriers_locked) {
	// Caller must have already acquired the mutex for the worker.
	if (!p_transfer_worker->texture_barriers.is_empty()) {
		if (!p_barriers_locked) {
			transfer_worker_pool_texture_barriers_mutex.lock();
		}
		for (uint32_t i = 0; i < p_transfer_worker->texture_barriers.size(); i++) {
			transfer_worker_pool_texture_barriers.push_back(p_transfer_worker->texture_barriers[i]);
		}

		p_transfer_worker->texture_barriers.clear();
		if (!p_barriers_locked) {
			transfer_worker_pool_texture_barriers_mutex.unlock();
		}
	}
}

Error RenderingDevice::_check_transfer_worker_operation(uint32_t index, uint64_t operation, bool *r_processed) {
	auto *worker = transfer_worker_pool[index];
	// Never hold operations while waiting for thread: aggregate submit uses the
	// original thread -> operations order. A retry publishes no new dependency.
	if (!worker->thread_mutex.try_lock()) {
		return ERR_BUSY;
	}
	struct Unlock {
		BinaryMutex &mutex;
		~Unlock() { mutex.unlock(); }
	} thread_unlock{ worker->thread_mutex };
	if (!worker->operations_mutex.try_lock()) {
		return ERR_BUSY;
	}
	Unlock operations_unlock{ worker->operations_mutex };
	// Capacity checks precede qualification quotes and all watcher publication.
	Ref<NativeHeightRange> height_range;
	uint32_t height_dependency = UINT32_MAX;
	if (render_thread_id == Thread::get_caller_id() && native_height_active_checked_range.is_valid() && worker->operations_processed < operation) {
		if (operation > worker->operations_counter) {
			return ERR_INVALID_DATA;
		}
		height_range = native_height_active_checked_range;
		if (height_range->recording_closed || height_range->capture_frame != uint32_t(frame) || height_range->capture_epoch != frames[frame].qualification_epoch) {
			return ERR_INVALID_DATA;
		}
		for (uint32_t i = 0; i < worker->height_watcher_count; ++i) {
			if (worker->height_watchers[i].range == height_range) {
				height_dependency = worker->height_watchers[i].dependency;
			}
		}
		if (height_dependency == UINT32_MAX && (!height_range->limits.worker_sources || height_range->worker_count == height_range->limits.worker_dependencies || worker->height_watcher_count == NATIVE_HEIGHT_RANGE_CAPACITY)) {
			return ERR_PARAMETER_RANGE_ERROR;
		}
	}

	if (height_range.is_valid() && height_dependency != UINT32_MAX) {
		const auto &dependency = height_range->workers[height_dependency];
		if (dependency.issue_attempted && (operation < dependency.first_operation || operation > dependency.last_operation)) {
			return ERR_INVALID_DATA;
		}
		if (height_range->issue_error != OK) {
			return height_range->issue_error;
		}
	}
	// Quote the current prefix plus future submit under both original locks.
	// This bounded recipe refuses excess prefix before publishing any watcher.
	if (height_range.is_valid()) {
		RDD::QueueCallReport prefix;
		uint32_t future = 0;
		if (worker->recording || worker->submission_pending) {
			const Error quoted = driver->qualification_worker_source_quote(worker->command_buffer, prefix, future);
			if (quoted != OK) {
				return quoted;
			}
		} else if (worker->submitted && operation <= worker->operations_submitted) {
			prefix = worker->last_submission;
		} else {
			return ERR_UNAVAILABLE;
		}
		if (future > height_range->limits.worker_sources || prefix.calls.size() > height_range->limits.worker_sources - future ||
				prefix.native_issue_count > height_range->limits.worker_sources - future || (!future && prefix.calls.is_empty())) {
			return ERR_PARAMETER_RANGE_ERROR;
		}
	}
	if (worker->operations_processed < operation) {
		bool exact = false;
		if (!frames.is_empty() && uint32_t(frame) < frames.size()) {
			for (auto *ticket : frames[frame].qualification_tickets) {
				exact = exact || ticket->credits.capacity;
			}
		}
		if (exact) {
			RDD::QueueCallReport prefix;
			uint32_t future = 0;
			if (worker->recording || worker->submission_pending) {
				const Error error = driver->qualification_worker_source_quote(worker->command_buffer, prefix, future);
				if (error != OK) {
					return error;
				}
			} else {
				prefix = worker->last_submission;
			}
			const Error admission = _qualification_admit_worker(index, worker->operations_counter, prefix, future);
			if (admission != OK) {
				return admission;
			}
			worker->qualification_batch_sealed = true;
		}
	}
	if (height_range.is_valid()) {
		if (height_dependency == UINT32_MAX) {
			height_dependency = height_range->worker_count++;
			height_range->workers[height_dependency] = { index, operation, false, false };
			worker->height_watchers[worker->height_watcher_count++] = { height_range, height_dependency };
			worker->height_observer_pending = true;
		} else {
			auto &dependency = height_range->workers[height_dependency];
			if (operation > dependency.operation) {
				dependency.operation = operation;
				// A second resource in the same authenticated batch shares the
				// existing edges and observer; it does not reopen its issue.
				if (!dependency.issue_attempted) {
					worker->height_observer_pending = true;
				}
			}
		}
		// The original worker locks still authenticate this historical batch.
		if (worker->submitted && operation <= worker->operations_submitted) {
			const Error joined = _native_height_join_worker(worker, worker->operations_processed + 1, worker->operations_submitted, worker->last_submission);
			if (joined != OK && joined != ERR_BUSY) {
				return joined;
			}
		}
	}
	if (r_processed != nullptr) {
		*r_processed = worker->operations_processed >= operation;
	}
	auto &used = transfer_worker_operation_used_by_draw[index];
	used = MAX(used, operation);
	return OK;
}
Error RenderingDevice::_check_transfer_worker_buffer(Buffer *buffer) {
	if (buffer->transfer_worker_index < 0) {
		return OK;
	}
	bool processed = false;
	const Error error = _check_transfer_worker_operation(buffer->transfer_worker_index, buffer->transfer_worker_operation, &processed);
	if (error == OK && processed) {
		buffer->transfer_worker_index = -1;
	}
	return error;
}
Error RenderingDevice::_check_transfer_worker_texture(Texture *texture) {
	if (texture->transfer_worker_index < 0) {
		return OK;
	}
	bool processed = false;
	const Error error = _check_transfer_worker_operation(texture->transfer_worker_index, texture->transfer_worker_operation, &processed);
	if (error == OK && processed) {
		texture->transfer_worker_index = -1;
	}
	return error;
}
Error RenderingDevice::_check_transfer_worker_vertex_array(VertexArray *array) {
	for (uint32_t i = 0; i < array->transfer_worker_indices.size();) {
		bool processed = false;
		const Error error = _check_transfer_worker_operation(array->transfer_worker_indices[i], array->transfer_worker_operations[i], &processed);
		if (error != OK) {
			return error;
		}
		if (processed) {
			array->transfer_worker_indices.remove_at(i);
			array->transfer_worker_operations.remove_at(i);
		} else {
			i++;
		}
	}
	return OK;
}
Error RenderingDevice::_check_transfer_worker_index_array(IndexArray *array) {
	if (array->transfer_worker_index < 0) {
		return OK;
	}
	bool processed = false;
	const Error error = _check_transfer_worker_operation(array->transfer_worker_index, array->transfer_worker_operation, &processed);
	if (error == OK && processed) {
		array->transfer_worker_index = -1;
	}
	return error;
}

Error RenderingDevice::_submit_transfer_workers(RDD::CommandBufferID p_draw_command_buffer, bool p_nonblocking) {
	if (p_nonblocking) {
		// Acquire every original lock before ending or issuing any worker. Refusal
		// has no native effect; no check-then-lock window can become a blocking wait.
		struct Locks {
			LocalVector<BinaryMutex *> held;
			bool take(BinaryMutex &mutex) {
				if (!mutex.try_lock()) {
					return false;
				}
				held.push_back(&mutex);
				return true;
			}
			~Locks() {
				for (uint32_t i = held.size(); i > 0; --i) {
					held[i - 1]->unlock();
				}
			}
		} locks;
		if (!locks.take(transfer_worker_pool_mutex) || !locks.take(transfer_worker_pool_texture_barriers_mutex)) {
			return ERR_BUSY;
		}
		for (uint32_t i = 0; i < transfer_worker_pool_size; ++i) {
			auto *worker = transfer_worker_pool[i];
			if (!locks.take(worker->thread_mutex) || !locks.take(worker->operations_mutex)) {
				return ERR_BUSY;
			}
		}
		for (uint32_t i = 0; i < transfer_worker_pool_size; ++i) {
			auto *worker = transfer_worker_pool[i];
			if (worker->operations_processed >= transfer_worker_operation_used_by_draw[worker->index]) {
				continue;
			}
			const bool qualified = !frames[frame].qualification_tickets.is_empty();
			if (qualified) {
				worker->qualification_observer_pending = true;
			}
			if (worker->recording) {
				if (!_native_disposal_admit(NativeDisposalWork::WORK)) {
					return ERR_BUSY;
				}
				_end_transfer_worker(worker);
			}
			if (worker->submission_pending) {
				uint32_t bound = 0;
				if (native_disposal_budget && (!driver->native_issue_bound(RDD::NativeIssueKind::TRANSFER_WORKER, 1, bound) || !_native_disposal_admit(NativeDisposalWork::ISSUE, bound))) {
					return ERR_BUSY;
				}
				_submit_transfer_worker(worker, frames[frame].transfer_worker_semaphores[worker->index], true);
				if (native_disposal_usage) {
					if (worker->last_submission.native_issue_count > bound) {
						native_disposal_usage->issues += worker->last_submission.native_issue_count - bound;
						return ERR_INVALID_DATA;
					}
					native_disposal_usage->issues -= bound - worker->last_submission.native_issue_count;
				}
			}
			if (worker->submission_pending) {
				return ERR_BUSY;
			}
			const Error production_retained = _production_join_transfer_worker(worker, true);
			if (production_retained != OK) {
				return production_retained;
			}
			if (qualified) {
				// Both original worker locks still cover the operation -> submission join.
				// Never read a later reused worker report as proof of this dependency.
				// A failed worker may still have an actual native prefix. Join that exact
				// report before retaining the worker as failed; never lose its callbacks.
				const bool operation_covered = worker->operations_submitted >= transfer_worker_operation_used_by_draw[worker->index];
				bool known = false;
				for (const auto &report : frames[frame].qualification_hidden_reports) {
					if (report.submission_serial == worker->last_submission.submission_serial && !report.calls.is_empty() && report.calls[0].serial == worker->last_submission.calls[0].serial) {
						known = true;
						break;
					}
				}
				if (!known) {
					frames[frame].qualification_hidden_reports.push_back(worker->last_submission);
				}
				{
					auto &registry = GDExtensionDeviceExit::Registry::get_singleton();
					GDExtensionDeviceExit::Registry::Lock lock(registry);
					// Production already joined the exact worker operation above.
					_qualification_issue_tickets_locked(UINT32_MAX, worker->last_submission, worker->last_submission.error);
				}
				const Error retained = _qualification_retain_tickets(frame);
				if (retained != OK) {
					return retained;
				}
				{
					auto &registry = GDExtensionDeviceExit::Registry::get_singleton();
					GDExtensionDeviceExit::Registry::Lock lock(registry);
					for (auto *ticket : frames[frame].qualification_tickets) {
						if (ticket->credits.capacity) {
							for (auto &quote : ticket->worker_credits) {
								if (quote.worker == worker->index && quote.frame == uint32_t(frame) && quote.epoch == frames[frame].qualification_epoch && quote.operation <= worker->operations_counter) {
									const Error released = ticket->credits.release_worker_quote(quote.remaining);
									CRASH_COND(released != OK);
									quote.remaining = 0;
								}
							}
						}
					}
					worker->qualification_batch_sealed = false;
				}
				if (!operation_covered || worker->last_submission.calls.is_empty()) {
					return FAILED;
				}
				worker->qualification_observer_pending = false;
			}
			_flush_barriers_for_transfer_worker(worker, true);
			if (worker->poisoned) {
				return worker->completion_error;
			}
		}
		if (!transfer_worker_pool_texture_barriers.is_empty()) {
			if (!_native_disposal_admit(NativeDisposalWork::WORK)) {
				return ERR_BUSY;
			}
			driver->command_pipeline_barrier(p_draw_command_buffer, RDD::PIPELINE_STAGE_COPY_BIT, RDD::PIPELINE_STAGE_ALL_COMMANDS_BIT, {}, {}, transfer_worker_pool_texture_barriers, {});
			transfer_worker_pool_texture_barriers.clear();
		}
		return OK;
	}
	MutexLock transfer_worker_lock(transfer_worker_pool_mutex);
	for (uint32_t i = 0; i < transfer_worker_pool_size; i++) {
		TransferWorker *worker = transfer_worker_pool[i];
		if (p_draw_command_buffer) {
			MutexLock lock(worker->operations_mutex);
			if (worker->operations_processed >= transfer_worker_operation_used_by_draw[worker->index]) {
				// The operation used by the draw has already been processed, we don't need to wait on the worker.
				continue;
			}
		}

		{
			MutexLock lock(worker->thread_mutex);
			if (worker->recording || worker->submission_pending) {
				VectorView<RDD::SemaphoreID> semaphores = p_draw_command_buffer ? frames[frame].transfer_worker_semaphores[i] : VectorView<RDD::SemaphoreID>();
				if (worker->recording) {
					_end_transfer_worker(worker);
				}
				_submit_transfer_worker(worker, semaphores);
			}

			_production_join_transfer_worker(worker, false);
			if (p_draw_command_buffer) {
				_flush_barriers_for_transfer_worker(worker);
			}
		}
	}
	return OK;
}

void RenderingDevice::_submit_transfer_barriers(RDD::CommandBufferID p_draw_command_buffer) {
	MutexLock transfer_worker_lock(transfer_worker_pool_texture_barriers_mutex);
	if (!transfer_worker_pool_texture_barriers.is_empty()) {
		driver->command_pipeline_barrier(p_draw_command_buffer, RDD::PIPELINE_STAGE_COPY_BIT, RDD::PIPELINE_STAGE_ALL_COMMANDS_BIT, {}, {}, transfer_worker_pool_texture_barriers, {});
		transfer_worker_pool_texture_barriers.clear();
	}
}

bool RenderingDevice::_wait_for_transfer_workers() {
	MutexLock transfer_worker_lock(transfer_worker_pool_mutex);
	bool all_complete = true;
	for (uint32_t i = 0; i < transfer_worker_pool_size; i++) {
		TransferWorker *worker = transfer_worker_pool[i];
		MutexLock lock(worker->thread_mutex);
		if (worker->submitted) {
			if (!_wait_for_transfer_worker(worker, true)) {
				all_complete = false;
			}
		}
		if (worker->poisoned) {
			all_complete = false;
		}
	}
	return all_complete;
}

void RenderingDevice::_free_transfer_workers() {
	MutexLock transfer_worker_lock(transfer_worker_pool_mutex);
	for (uint32_t i = 0; i < transfer_worker_pool_size; i++) {
		TransferWorker *worker = transfer_worker_pool[i];
		if (worker->submitted || worker->poisoned) {
			// Do not partially free the pool: a later pending worker keeps every
			// pool index and its staging ownership live for a future poll.
			return;
		}
	}
	for (uint32_t i = 0; i < transfer_worker_pool_size; i++) {
		TransferWorker *worker = transfer_worker_pool[i];
		driver->fence_free(worker->command_fence);
		driver->buffer_free(worker->staging_buffer);
		driver->command_pool_free(worker->command_pool);
		memdelete(worker);
	}

	transfer_worker_pool_size = 0;
}

/***********************/
/**** COMMAND GRAPH ****/
/***********************/

bool RenderingDevice::_texture_make_mutable(Texture *p_texture, RID p_texture_id) {
	if (p_texture->draw_tracker != nullptr) {
		// Texture already has a tracker.
		return false;
	} else {
		if (p_texture->owner.is_valid()) {
			// Texture has an owner.
			Texture *owner_texture = texture_owner.get_or_null(p_texture->owner);
			ERR_FAIL_NULL_V(owner_texture, false);

			if (owner_texture->draw_tracker != nullptr) {
				// Create a tracker for this dependency in particular.
				if (p_texture->slice_type == TEXTURE_SLICE_MAX) {
					// Shared texture.
					p_texture->draw_tracker = owner_texture->draw_tracker;
					p_texture->draw_tracker->reference_count++;
				} else {
					// Slice texture.
					if (owner_texture->slice_trackers == nullptr) {
						owner_texture->slice_trackers = memnew((HashMap<Rect2i, RDG::ResourceTracker *>));
					}
					HashMap<Rect2i, RDG::ResourceTracker *>::ConstIterator draw_tracker_iterator = owner_texture->slice_trackers->find(p_texture->slice_rect);
					RDG::ResourceTracker *draw_tracker = nullptr;
					if (draw_tracker_iterator != owner_texture->slice_trackers->end()) {
						// Reuse the tracker at the matching rectangle.
						draw_tracker = draw_tracker_iterator->value;
					} else {
						// Create a new tracker and store it on the map.
						draw_tracker = RDG::resource_tracker_create();
						draw_tracker->parent = owner_texture->draw_tracker;
						draw_tracker->texture_driver_id = p_texture->driver_id;
						draw_tracker->texture_size = Size2i(p_texture->width, p_texture->height);
						draw_tracker->texture_subresources = p_texture->barrier_range();
						draw_tracker->texture_usage = p_texture->usage_flags;
						draw_tracker->texture_slice_or_dirty_rect = p_texture->slice_rect;
						(*owner_texture->slice_trackers)[p_texture->slice_rect] = draw_tracker;
					}

					p_texture->draw_tracker = draw_tracker;
					p_texture->draw_tracker->reference_count++;
				}

				if (p_texture_id.is_valid()) {
					_dependencies_make_mutable(p_texture_id, p_texture->draw_tracker);
				}
			} else {
				// Delegate this to the owner instead, as it'll make all its dependencies mutable.
				_texture_make_mutable(owner_texture, p_texture->owner);
			}
		} else {
			// Regular texture.
			p_texture->draw_tracker = RDG::resource_tracker_create();
			p_texture->draw_tracker->texture_driver_id = p_texture->driver_id;
			p_texture->draw_tracker->texture_size = Size2i(p_texture->width, p_texture->height);
			p_texture->draw_tracker->texture_subresources = p_texture->barrier_range();
			p_texture->draw_tracker->texture_usage = p_texture->usage_flags;
			p_texture->draw_tracker->is_discardable = p_texture->is_discardable;
			p_texture->draw_tracker->reference_count = 1;

			if (p_texture_id.is_valid()) {
				if (p_texture->has_initial_data) {
					// If the texture was initialized with initial data but wasn't made mutable from the start, assume the texture sampling usage.
					p_texture->draw_tracker->usage = RDG::RESOURCE_USAGE_TEXTURE_SAMPLE;
				}

				_dependencies_make_mutable(p_texture_id, p_texture->draw_tracker);
			}
		}

		return true;
	}
}

#if defined(TESTS_ENABLED) && defined(MODULE_SKINNED_MESH_CAPTURE_TESTS_ENABLED)
void RenderingDevice::_capture_test_record_buffer_usage(uint64_t p_driver, RDG::ResourceUsage p_usage, uint32_t p_operation) {
	if (!RenderCaptureTestHooks::write_census_active()) {
		return;
	}
	switch (p_usage) {
		case RDG::RESOURCE_USAGE_COPY_FROM:
		case RDG::RESOURCE_USAGE_UNIFORM_BUFFER_READ:
		case RDG::RESOURCE_USAGE_INDIRECT_BUFFER_READ:
		case RDG::RESOURCE_USAGE_TEXTURE_BUFFER_READ:
		case RDG::RESOURCE_USAGE_STORAGE_BUFFER_READ:
		case RDG::RESOURCE_USAGE_VERTEX_BUFFER_READ:
		case RDG::RESOURCE_USAGE_INDEX_BUFFER_READ:
			return;
		case RDG::RESOURCE_USAGE_COPY_TO:
		case RDG::RESOURCE_USAGE_STORAGE_BUFFER_READ_WRITE:
		case RDG::RESOURCE_USAGE_TEXTURE_BUFFER_READ_WRITE:
		case RDG::RESOURCE_USAGE_GENERAL:
			break;
		default:
			RenderCaptureTestHooks::write_census_poison();
			return;
	}
	if (capture_test_coverage_failed) {
		RenderCaptureTestHooks::write_census_poison();
		return;
	}
	RenderCaptureTestHooks::write_census_record(p_driver, uint64_t(get_instance_id()), capture_test_allocation_registry.resolve_driver(p_driver), uint32_t(p_usage), p_operation);
}
void RenderingDevice::_capture_test_record_writable_uses(const Vector<RDG::ResourceTracker *> &p_trackers, const Vector<RDG::ResourceUsage> &p_usages, uint32_t p_operation) {
	if (!RenderCaptureTestHooks::write_census_active()) {
		return;
	}
	if (p_trackers.size() != p_usages.size()) {
		RenderCaptureTestHooks::write_census_poison();
		return;
	}
	for (int i = 0; i < p_trackers.size(); i++) {
		const auto *tracker = p_trackers[i];
		if (!tracker) {
			RenderCaptureTestHooks::write_census_poison();
			continue;
		}
		// Fixed P01 buffers do not alias texture or acceleration-structure allocations.
		if (!tracker->buffer_driver_id) {
			if (tracker->texture_driver_id && (p_usages[i] == RDG::RESOURCE_USAGE_STORAGE_IMAGE_READ_WRITE || p_usages[i] == RDG::RESOURCE_USAGE_GENERAL)) {
				RenderCaptureTestHooks::observe_texture_write(uint64_t(get_instance_id()), tracker->texture_driver_id.id, p_usages[i], p_operation);
			}
			if (!tracker->texture_driver_id && !tracker->acceleration_structure_driver_id) {
				RenderCaptureTestHooks::write_census_poison();
			}
			continue;
		}
		_capture_test_record_buffer_usage(tracker->buffer_driver_id.id, p_usages[i], p_operation);
	}
}
void RenderingDevice::_capture_test_record_writable_set(RID p_set, uint32_t p_operation) {
	if (!RenderCaptureTestHooks::write_census_active()) {
		return;
	}
	const UniformSet *set = uniform_set_owner.get_or_null(p_set);
	if (!set) {
		RenderCaptureTestHooks::write_census_poison();
		return;
	}
	_capture_test_record_writable_uses(set->draw_trackers, set->draw_trackers_usage, p_operation);
	// Writable buffers must already have trackers at reflected binding creation.
	for (const auto &entry : set->untracked_usage) {
		if (entry.value == RDG::RESOURCE_USAGE_STORAGE_BUFFER_READ_WRITE || entry.value == RDG::RESOURCE_USAGE_TEXTURE_BUFFER_READ_WRITE || entry.value == RDG::RESOURCE_USAGE_GENERAL) {
			RenderCaptureTestHooks::write_census_poison();
		}
	}
}
#endif

bool RenderingDevice::_buffer_make_mutable(Buffer *p_buffer, RID p_buffer_id) {
	if (p_buffer->draw_tracker != nullptr) {
		// Buffer already has a tracker.
		return false;
	} else {
		// Create a tracker for the buffer and make all its dependencies mutable.
		p_buffer->draw_tracker = RDG::resource_tracker_create();
		p_buffer->draw_tracker->buffer_driver_id = p_buffer->driver_id;
		if (p_buffer_id.is_valid()) {
			_dependencies_make_mutable(p_buffer_id, p_buffer->draw_tracker);
		}

		return true;
	}
}

bool RenderingDevice::_vertex_array_make_mutable(VertexArray *p_vertex_array, RID p_resource_id, RDG::ResourceTracker *p_resource_tracker) {
	if (!p_vertex_array->untracked_buffers.has(p_resource_id)) {
		// Vertex array thinks the buffer is already tracked or does not use it.
		return false;
	} else {
		// Vertex array is aware of the buffer but it isn't being tracked.
		p_vertex_array->draw_trackers.push_back(p_resource_tracker);
		p_vertex_array->untracked_buffers.erase(p_resource_id);
		return true;
	}
}

bool RenderingDevice::_index_array_make_mutable(IndexArray *p_index_array, RDG::ResourceTracker *p_resource_tracker) {
	if (p_index_array->draw_tracker != nullptr) {
		// Index array already has a tracker.
		return false;
	} else {
		// Index array should assign the tracker from the buffer.
		p_index_array->draw_tracker = p_resource_tracker;
		return true;
	}
}

bool RenderingDevice::_uniform_set_make_mutable(UniformSet *p_uniform_set, RID p_resource_id, RDG::ResourceTracker *p_resource_tracker) {
	HashMap<RID, RDG::ResourceUsage>::Iterator E = p_uniform_set->untracked_usage.find(p_resource_id);
	if (!E) {
		// Uniform set thinks the resource is already tracked or does not use it.
		return false;
	} else {
		// Uniform set has seen the resource but hasn't added its tracker yet.
		p_uniform_set->draw_trackers.push_back(p_resource_tracker);
		p_uniform_set->draw_trackers_usage.push_back(E->value);
		p_uniform_set->untracked_usage.remove(E);
		return true;
	}
}

bool RenderingDevice::_acceleration_structure_make_mutable(AccelerationStructure *p_acceleration_structure, RID p_resource_id, RDG::ResourceTracker *p_resource_tracker) {
	if (!p_acceleration_structure->untracked_buffers.has(p_resource_id)) {
		// Acceleration structure thinks the buffer is already tracked or does not use it.
		return false;
	} else {
		// Acceleration structure is aware of the buffer but it isn't being tracked.
		p_acceleration_structure->draw_trackers.push_back(p_resource_tracker);
		p_acceleration_structure->untracked_buffers.erase(p_resource_id);
		return true;
	}
}

bool RenderingDevice::_dependency_make_mutable(RID p_id, RID p_resource_id, RDG::ResourceTracker *p_resource_tracker) {
	if (texture_owner.owns(p_id)) {
		Texture *texture = texture_owner.get_or_null(p_id);
		return _texture_make_mutable(texture, p_id);
	} else if (vertex_array_owner.owns(p_id)) {
		VertexArray *vertex_array = vertex_array_owner.get_or_null(p_id);
		return _vertex_array_make_mutable(vertex_array, p_resource_id, p_resource_tracker);
	} else if (index_array_owner.owns(p_id)) {
		IndexArray *index_array = index_array_owner.get_or_null(p_id);
		return _index_array_make_mutable(index_array, p_resource_tracker);
	} else if (uniform_set_owner.owns(p_id)) {
		UniformSet *uniform_set = uniform_set_owner.get_or_null(p_id);
		return _uniform_set_make_mutable(uniform_set, p_resource_id, p_resource_tracker);
	} else if (acceleration_structure_owner.owns(p_id)) {
		AccelerationStructure *acceleration_structure = acceleration_structure_owner.get_or_null(p_id);
		return _acceleration_structure_make_mutable(acceleration_structure, p_resource_id, p_resource_tracker);
	} else {
		DEV_ASSERT(false && "Unknown resource type to make mutable.");
		return false;
	}
}

bool RenderingDevice::_dependencies_make_mutable_recursive(RID p_id, RDG::ResourceTracker *p_resource_tracker) {
	bool made_mutable = false;
	HashMap<RID, HashSet<RID>>::Iterator E = dependency_map.find(p_id);
	if (E) {
		for (RID rid : E->value) {
			made_mutable = _dependency_make_mutable(rid, p_id, p_resource_tracker) || made_mutable;
		}
	}

	return made_mutable;
}

bool RenderingDevice::_dependencies_make_mutable(RID p_id, RDG::ResourceTracker *p_resource_tracker) {
	_THREAD_SAFE_METHOD_
	return _dependencies_make_mutable_recursive(p_id, p_resource_tracker);
}

/**************************/
/**** FRAME MANAGEMENT ****/
/**************************/

Error RenderingDevice::_native_height_range_validate(const Ref<NativeHeightRange> &p_range) const {
	if (!is_main_instance || render_thread_id != Thread::get_caller_id() || !driver || native_finalization_returned) {
		return ERR_UNAUTHORIZED;
	}
	if (p_range.is_null() || p_range->device != uint64_t(get_instance_id()) || !p_range->serial) {
		return ERR_INVALID_PARAMETER;
	}
	return OK;
}

Error RenderingDevice::_native_height_range_prepare_checked_capture(RendererRD::HeightCaptureCheckedSink &r_sink, Ref<NativeHeightRange> &r_range) {
	if (&r_sink.rd != this || r_sink.native_range.is_valid() || r_sink.begin_attempted || r_sink.business_count != 0 || r_sink.cleanup_attempted || r_sink.terminal_requested) {
		return ERR_INVALID_PARAMETER;
	}
	const Error error = _native_height_range_prepare(r_range);
	if (error != OK) {
		return error;
	}
	frames[frame].native_height_ranges.push_back(r_range);
	r_range->recording_started = true;
	r_range->capture_sink = &r_sink;
	r_range->capture_frame = frame;
	r_range->capture_epoch = frames[frame].qualification_epoch;
	return OK;
}

Error RenderingDevice::_native_height_range_enter_checked_call(const Ref<NativeHeightRange> &p_range, RendererRD::HeightCaptureCheckedSink &r_sink) {
	if (&r_sink.rd != this || r_sink.native_range != p_range) {
		return ERR_INVALID_PARAMETER;
	}
	const Error valid = _native_height_range_validate(p_range);
	if (valid != OK) {
		return valid;
	}
	if (p_range->capture_sink != &r_sink || p_range->capture_frame != frame || p_range->capture_epoch != frames[frame].qualification_epoch) {
		p_range->uncaptured_prefix = true;
		return ERR_INVALID_DATA;
	}
	if (!p_range->recording_started || p_range->recording_closed || p_range->observers_returned) {
		return ERR_BUSY;
	}
	// An already captured business failure must still permit the original
	// independent cleanup End. It never makes another business append legal.
	const Error result = r_sink.cleanup_attempted ? OK : p_range->append_error;
	if (result == OK) {
		if (native_height_active_checked_range.is_valid()) {
			return ERR_BUSY;
		}
		native_height_active_checked_range = p_range;
	}
	return result;
}

Error RenderingDevice::_native_height_range_enroll_original_record(const Ref<NativeHeightRange> &p_range, RendererRD::HeightCaptureCheckedSink &p_sink, uint32_t p_business_index, bool p_cleanup) {
	if (native_height_active_checked_range == p_range) {
		native_height_active_checked_range.unref();
	}
	if (&p_sink.rd != this || p_sink.native_range != p_range) {
		return ERR_INVALID_PARAMETER;
	}
	const Error valid = _native_height_range_validate(p_range);
	if (valid != OK) {
		return valid;
	}
	if (p_range->recording_closed || p_range->observers_returned) {
		return ERR_BUSY;
	}
	auto fail = [&](Error p_error, bool p_unknown = false) {
		if (p_range->append_error == OK && p_error != OK) {
			p_range->append_error = p_error;
		}
		p_range->uncaptured_prefix |= p_unknown;
	};
	if (p_range->capture_sink != &p_sink || p_range->capture_frame != frame || p_range->capture_epoch != frames[frame].qualification_epoch ||
			(p_cleanup ? (!p_sink.cleanup_attempted || p_range->capture_cleanup) : (p_business_index != p_range->capture_records || p_business_index >= p_sink.business_count))) {
		fail(ERR_INVALID_DATA, true);
		return p_range->append_error;
	}
	if (p_cleanup) {
		p_range->capture_cleanup = true;
	} else {
		++p_range->capture_records;
	}
	auto save = [&](const RDG::RecordHandle &p_handle) {
		if (!p_handle.is_valid()) {
			return;
		}
		p_range->has_recorded_effect = true;
		if (!draw_graph.contains_record(p_handle)) {
			fail(ERR_INVALID_DATA, true);
			return;
		}
		for (uint32_t i = 0; i < p_range->graph_count; ++i) {
			const auto &old = p_range->graphs[i].handle;
			if (old.graph_instance == p_handle.graph_instance && old.graph_frame == p_handle.graph_frame && old.kind == p_handle.kind && old.index == p_handle.index && old.offset == p_handle.offset) {
				return;
			}
		}
		if (p_range->graph_count == p_range->limits.graph_members) {
			fail(ERR_PARAMETER_RANGE_ERROR, true);
			return;
		}
		auto &member = p_range->graphs[p_range->graph_count++];
		member.handle = p_handle;
		member.frame = frame;
		member.frame_epoch = frames[frame].qualification_epoch;
	};
	auto result = [&](Error p_error, NativeAppendEffect p_effect, bool p_complete) {
		fail(p_error);
		if (p_effect != NativeAppendEffect::NoNewEffect) {
			p_range->has_recorded_effect = true;
		}
		if (p_effect == NativeAppendEffect::Unknown || (p_effect != NativeAppendEffect::NoNewEffect && !p_complete)) {
			fail(ERR_INVALID_DATA, true);
		}
	};
	const auto &record = p_cleanup ? p_sink.cleanup : p_sink.business[p_business_index];
	switch (record.operation) {
		case RendererRD::HeightCaptureCheckedSink::Operation::Begin: {
			result(record.begin.error, record.begin.effect, record.begin.capture_complete);
			save(record.begin.begin_handle);
			save(record.begin.clear_handle);
			if (record.begin.preparation_count > std::size(record.begin.preparation_handles) || record.begin.prefix_count > std::size(record.begin.prefix_handles)) {
				fail(ERR_INVALID_DATA, true);
				break;
			}
			for (uint32_t i = 0; i < record.begin.preparation_count; ++i) {
				save(record.begin.preparation_handles[i]);
			}
			for (uint32_t i = 0; i < record.begin.prefix_count; ++i) {
				save(record.begin.prefix_handles[i]);
			}
		} break;
		case RendererRD::HeightCaptureCheckedSink::Operation::UniformSet:
		case RendererRD::HeightCaptureCheckedSink::Operation::VertexArray:
		case RendererRD::HeightCaptureCheckedSink::Operation::IndexArray:
			result(record.bind.error, record.bind.effect, record.bind.capture_complete);
			save(record.bind.bind_handle);
			if (record.transfer_offset != p_range->capture_transfers || record.transfer_count > p_sink.transfer_count - MIN(p_sink.transfer_count, record.transfer_offset)) {
				fail(ERR_INVALID_DATA, true);
			} else {
				p_range->capture_transfers += record.transfer_count;
			}
			// A raw transfer copy is not an authenticated original worker join.
			// Until that path is installed, never retire it as an empty prefix.
			if (record.bind.transfer_count || record.transfer_count) {
				fail(ERR_UNAVAILABLE, true);
			}
			break;
		default:
			result(record.draw.error, record.draw.effect, record.draw.capture_complete);
			save(record.draw.draw_handle);
			if (record.draw.prefix_count > std::size(record.draw.prefix_handles)) {
				fail(ERR_INVALID_DATA, true);
				break;
			}
			for (uint32_t i = 0; i < record.draw.prefix_count; ++i) {
				save(record.draw.prefix_handles[i]);
			}
			if (record.draw.has_transfer) {
				fail(ERR_UNAVAILABLE, true);
			}
			break;
	}
	return p_range->append_error;
}

Error RenderingDevice::_native_height_range_close_checked_capture(const Ref<NativeHeightRange> &p_range, RendererRD::HeightCaptureCheckedSink &r_sink) {
	if (&r_sink.rd != this || r_sink.native_range != p_range) {
		return ERR_INVALID_PARAMETER;
	}
	const Error valid = _native_height_range_validate(p_range);
	if (valid != OK) {
		return valid;
	}
	if (p_range->capture_sink != &r_sink || !r_sink.terminal_requested) {
		return ERR_INVALID_PARAMETER;
	}
	if (p_range->append_error == OK && r_sink.first_error != OK) {
		p_range->append_error = r_sink.first_error;
	}
	if (p_range->capture_frame != frame || p_range->capture_epoch != frames[frame].qualification_epoch ||
			r_sink.owns_list || r_sink.restoration_unknown || p_range->capture_records != r_sink.business_count ||
			p_range->capture_transfers != r_sink.transfer_count || p_range->capture_cleanup != r_sink.cleanup_attempted ||
			(p_range->has_recorded_effect && !r_sink.restored)) {
		p_range->uncaptured_prefix = true;
		if (p_range->append_error == OK) {
			p_range->append_error = ERR_INVALID_DATA;
		}
	}
	return _native_height_range_close(p_range); // Freezes recording only.
}

Error RenderingDevice::_native_height_range_prepare(Ref<NativeHeightRange> &r_range) {
	const Error ready = _native_disposal_preflight();
	if (ready != OK) {
		return ready;
	}
	if (r_range.is_valid()) {
		return ERR_INVALID_PARAMETER;
	}
	if (native_height_finalization_started || production_finalization_started.load() || frame_wait_draining || native_disposal_active ||
			!frames[frame].qualification_epoch || frames[frame].qualification_graph_closed || !frames[frame].primary_command_buffer_begun || draw_graph.has_driver_callbacks()) {
		return ERR_BUSY;
	}
	if (native_height_ranges.size() >= NATIVE_HEIGHT_RANGE_CAPACITY) {
		return ERR_BUSY;
	}
	uint32_t source_bound = 0;
	if (!driver->native_call_observation_supported() || !driver->native_issue_bound(RDD::NativeIssueKind::MAIN_SLOT, 1, source_bound) || !source_bound) {
		return ERR_UNAVAILABLE;
	}
	// Derive storage from the actual checked-result arrays, not a graph-counter
	// interval. A single closed pass maps these members to one actual slot.
	constexpr uint32_t graph_bound = 2 + sizeof(DrawListBeginResult::preparation_handles) / sizeof(RDG::RecordHandle) +
			sizeof(DrawListBeginResult::prefix_handles) / sizeof(RDG::RecordHandle) + 1 + DrawAppendResult::PREFIX_CAPACITY;
	if (source_bound > UINT32_MAX / graph_bound || !next_native_height_range_serial) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	uint32_t worker_bound = 0;
	// Unsupported transfer observation preserves the standalone clear recipe.
	if (!driver->native_issue_bound(RDD::NativeIssueKind::TRANSFER_WORKER, 1, worker_bound)) {
		worker_bound = 0;
	} else if (worker_bound) {
		// Finite per-dependency admission allowance, not a claim that the
		// current-submit bound includes a backend's historical prefix.
		worker_bound = MAX(worker_bound, 16u);
	}
	if (worker_bound > (UINT32_MAX - source_bound) / graph_bound) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	const uint32_t worker_sources = graph_bound * worker_bound;
	const NativeHeightRange::Limits limits{ graph_bound, source_bound + worker_sources, graph_bound * source_bound, graph_bound, worker_bound };
	Ref<NativeHeightRange> range = memnew(NativeHeightRange(uint64_t(get_instance_id()), next_native_height_range_serial++, limits));
	range->graphs.resize(limits.graph_members);
	range->sources.resize(limits.native_sources);
	range->edges.resize(limits.source_edges);
	range->workers.resize(limits.worker_dependencies);
	range->worker_edges.resize(worker_sources);
	native_height_ranges.reserve(NATIVE_HEIGHT_RANGE_CAPACITY);
	native_height_ranges.push_back(range);
	r_range = range;
	return OK;
}

Error RenderingDevice::_native_height_range_record_depth_clear(const Ref<NativeHeightRange> &p_range, const NativeDisposalWitness &p_depth, const NativeDisposalWitness &p_framebuffer, float p_clear_depth) {
	const Error valid = _native_height_range_validate(p_range);
	if (valid != OK) {
		return valid;
	}
	const Error ready = _native_disposal_preflight();
	if (ready != OK) {
		return ready;
	}
	if (native_height_finalization_started || production_finalization_started.load() || p_range->recording_started || p_range->recording_closed || p_range->observers_returned ||
			frame_wait_draining || native_disposal_active || !frames[frame].qualification_epoch || frames[frame].qualification_graph_closed ||
			!frames[frame].primary_command_buffer_begun || draw_graph.has_driver_callbacks()) {
		return ERR_BUSY;
	}
	if (!Math::is_finite(p_clear_depth) || p_clear_depth < 0.0f || p_clear_depth > 1.0f || p_depth.cell.is_null() || p_framebuffer.cell.is_null()) {
		return ERR_INVALID_PARAMETER;
	}
	_THREAD_SAFE_METHOD_
	const RID depth_rid = RID::from_uint64(p_depth.cell->rid);
	const RID framebuffer_rid = RID::from_uint64(p_framebuffer.cell->rid);
	const Texture *texture = texture_owner.owns(depth_rid) ? texture_owner.get_or_null(depth_rid) : nullptr;
	const Framebuffer *fb = framebuffer_owner.owns(framebuffer_rid) ? framebuffer_owner.get_or_null(framebuffer_rid) : nullptr;
	if (!texture || !fb || p_depth.cell->device != uint64_t(get_instance_id()) || p_framebuffer.cell->device != uint64_t(get_instance_id()) ||
			p_depth.cell->state != NativeDisposalWitness::State::LIVE || p_framebuffer.cell->state != NativeDisposalWitness::State::LIVE ||
			texture->native_disposal.cell != p_depth.cell || fb->native_disposal.cell != p_framebuffer.cell ||
			!p_depth.cell->matches(depth_rid.get_id(), texture->native_disposal_serial, texture->driver_id.id) ||
			!p_framebuffer.cell->matches(framebuffer_rid.get_id(), fb->native_disposal_serial, fb->native_disposal_backing)) {
		return ERR_INVALID_DATA;
	}
	// First recipe deliberately excludes every path that could prepare hidden
	// copies or consume a transfer operation. No unrelated Frame report is joined.
	if (texture->format != DATA_FORMAT_D32_SFLOAT || texture->type != TEXTURE_TYPE_2D || texture->samples != TEXTURE_SAMPLES_1 ||
			texture->depth != 1 || texture->layers != 1 || texture->mipmaps != 1 || texture->owner.is_valid() || texture->shared_fallback ||
			texture->transfer_worker_index >= 0 || texture->pending_clear || texture->native_external_access || texture->native_immutable_retire_requested ||
			texture->qualification_return || texture->qualification_users || texture->production_disposal_uses || p_depth.cell->production_request ||
			p_framebuffer.cell->production_request || fb->qualification_return || fb->qualification_dependency || fb->qualification_users || fb->invalidated_callback ||
			texture->usage_flags != (TEXTURE_USAGE_SAMPLING_BIT | TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)) {
		return ERR_UNAVAILABLE;
	}
	if (!fb->framebuffer_cache || uint64_t(uintptr_t(fb->framebuffer_cache)) != fb->native_disposal_backing ||
			fb->texture_ids.size() != 1 || fb->texture_ids[0] != depth_rid || fb->framebuffer_cache->textures.size() != 1 ||
			fb->framebuffer_cache->textures[0] != texture->driver_id || fb->view_count != 1 || !texture->draw_tracker ||
			!framebuffer_formats.has(fb->format_id)) {
		return ERR_INVALID_DATA;
	}
	const FramebufferFormatKey &key = framebuffer_formats[fb->format_id].E->key();
	if (key.passes.size() != 1 || key.passes[0].depth_attachment != 0 || !key.passes[0].color_attachments.is_empty() || key.vrs_attachment != -1) {
		return ERR_UNAVAILABLE;
	}
	// Finite custody is installed before the first actual checked append.
	frames[frame].native_height_ranges.reserve(NATIVE_HEIGHT_RANGE_CAPACITY);
	frames[frame].native_height_ranges.push_back(p_range);
	p_range->recording_started = true;
	auto save = [&](const RDG::RecordHandle &handle) {
		if (!handle.is_valid()) {
			return;
		}
		p_range->has_recorded_effect = true;
		for (uint32_t i = 0; i < p_range->graph_count; ++i) {
			const auto &old = p_range->graphs[i].handle;
			if (old.graph_instance == handle.graph_instance && old.graph_frame == handle.graph_frame && old.kind == handle.kind && old.index == handle.index && old.offset == handle.offset) {
				return;
			}
		}
		if (p_range->graph_count == p_range->limits.graph_members) {
			p_range->uncaptured_prefix = true;
			p_range->append_error = ERR_PARAMETER_RANGE_ERROR;
			return;
		}
		auto &member = p_range->graphs[p_range->graph_count++];
		member.handle = handle;
		member.frame = frame;
		member.frame_epoch = frames[frame].qualification_epoch;
	};
	const auto begin = _draw_list_begin_recorded(framebuffer_rid, DRAW_CLEAR_DEPTH, {}, p_clear_depth, 0, Rect2(), 0);
	save(begin.begin_handle);
	save(begin.clear_handle);
	for (uint32_t i = 0; i < begin.preparation_count; ++i) {
		save(begin.preparation_handles[i]);
	}
	for (uint32_t i = 0; i < begin.prefix_count; ++i) {
		save(begin.prefix_handles[i]);
	}
	if (begin.effect != NativeAppendEffect::NoNewEffect) {
		p_range->has_recorded_effect = true;
	}
	if (begin.error != OK || !begin.capture_complete || !begin.begin_handle.is_valid() || !begin.clear_handle.is_valid()) {
		p_range->append_error = begin.error != OK ? begin.error : ERR_INVALID_DATA;
	}
	// Only this exact successful begin may own an active list. Never end a
	// caller list, reset global state, or replay a failed checked prefix.
	if (draw_list.active && begin.begin_handle.is_valid() && draw_list.begin_handle.graph_instance == begin.begin_handle.graph_instance &&
			draw_list.begin_handle.graph_frame == begin.begin_handle.graph_frame && draw_list.begin_handle.kind == begin.begin_handle.kind &&
			draw_list.begin_handle.index == begin.begin_handle.index && draw_list.begin_handle.offset == begin.begin_handle.offset) {
		const auto end = _draw_list_end_recorded();
		save(end.draw_handle);
		for (uint32_t i = 0; i < end.prefix_count; ++i) {
			save(end.prefix_handles[i]);
		}
		if (end.error != OK || !end.capture_complete || !end.draw_handle.is_valid()) {
			p_range->append_error = end.error != OK ? end.error : ERR_INVALID_DATA;
			p_range->uncaptured_prefix = true;
		}
	} else if (p_range->has_recorded_effect) {
		p_range->uncaptured_prefix = true;
		p_range->append_error = ERR_INVALID_DATA;
	}
	return p_range->append_error;
}

Error RenderingDevice::_native_height_range_close(const Ref<NativeHeightRange> &p_range) {
	const Error valid = _native_height_range_validate(p_range);
	if (valid != OK) {
		return valid;
	}
	p_range->recording_closed = true;
	return p_range->append_error;
}

void RenderingDevice::_native_height_seal_frame(uint32_t p_frame) {
	for (const Ref<NativeHeightRange> &range : frames[p_frame].native_height_ranges) {
		for (uint32_t i = 0; i < range->graph_count; ++i) {
			auto &member = range->graphs[i];
			if (member.frame != p_frame || member.frame_epoch != frames[p_frame].qualification_epoch || member.state != NativeHeightRange::MemberState::Recorded) {
				continue;
			}
			if (draw_graph.record_mapping_state(member.handle) != RDG::RecordMappingState::Recorded ||
					!draw_graph.resolve_record_command_buffer_slot(member.handle, member.slot) ||
					!draw_graph.resolve_record_command_buffer(member.handle, member.command_buffer) || !member.command_buffer) {
				member.state = NativeHeightRange::MemberState::MappingFailed;
				range->mapping_failed = true;
				range->mapping_error = ERR_INVALID_DATA;
				range->uncaptured_prefix = true;
				continue;
			}
			member.state = NativeHeightRange::MemberState::Sealed;
			// One exclusive closed pass must be mapped consistently for every
			// actual header/clear/instruction/end handle, not just its tail.
			if (i && (member.slot != range->graphs[0].slot || member.command_buffer != range->graphs[0].command_buffer)) {
				range->mapping_failed = true;
				range->mapping_error = ERR_INVALID_DATA;
				range->uncaptured_prefix = true;
			}
		}
	}
}

Error RenderingDevice::_native_height_issue_slot(uint32_t p_frame, uint64_t p_epoch, uint32_t p_slot, RDD::CommandBufferID p_command, Error p_error, const RDD::QueueCallReport &p_report) {
	if (p_error == ERR_BUSY && p_report.calls.is_empty()) {
		return ERR_BUSY; // The original slot has not attempted a native issue.
	}
	Error result = OK;
	for (const Ref<NativeHeightRange> &range : frames[p_frame].native_height_ranges) {
		for (uint32_t i = 0; i < range->graph_count; ++i) {
			auto &member = range->graphs[i];
			if (member.frame != p_frame || member.frame_epoch != p_epoch || member.state != NativeHeightRange::MemberState::Sealed ||
					member.issue_attempted || member.slot != p_slot || member.command_buffer != p_command) {
				continue;
			}
			member.issue_attempted = true;
			member.state = NativeHeightRange::MemberState::IssueAttached;
			if (p_report.native_issue_count > range->limits.native_sources) {
				range->uncaptured_prefix = true;
				range->issue_error = ERR_PARAMETER_RANGE_ERROR;
			}
			if (p_error != OK || p_report.error != OK || !p_report.issued) {
				range->issue_error = p_error != OK ? p_error : (p_report.error != OK ? p_report.error : FAILED);
			}
			uint32_t member_sources = 0;
			for (const auto &fact : p_report.calls) {
				if (fact.presentation_only || fact.issue == RDD::IssueFact::NotIssued) {
					continue;
				}
				if (!p_report.native_issue_count || !fact.serial || !fact.completion_serial || fact.completion == RDD::CompletionCapability::Disabled) {
					range->uncaptured_prefix = true;
					range->issue_error = ERR_INVALID_DATA;
					continue;
				}
				uint32_t source = range->source_count;
				for (uint32_t j = 0; j < range->source_count; ++j) {
					const auto &old = range->sources[j].original;
					if (old.serial == fact.serial) {
						source = j;
						if (old.completion_serial != fact.completion_serial || old.queue_index != fact.queue_index || old.issue != fact.issue ||
								old.error != fact.error || old.native_code != fact.native_code || old.completion != fact.completion) {
							range->uncaptured_prefix = true;
							range->issue_error = ERR_INVALID_DATA;
						}
						break;
					}
				}
				if (source == range->source_count) {
					if (range->source_count == range->limits.native_sources) {
						range->uncaptured_prefix = true;
						range->issue_error = ERR_PARAMETER_RANGE_ERROR;
						continue;
					}
					range->sources[range->source_count++].original = fact;
				}
				if (range->edge_count == range->limits.source_edges) {
					range->uncaptured_prefix = true;
					range->issue_error = ERR_PARAMETER_RANGE_ERROR;
					continue;
				}
				range->edges[range->edge_count++] = { i, source };
				++member_sources;
			}
			if (!member_sources) {
				range->uncaptured_prefix = true;
				range->issue_error = ERR_INVALID_DATA;
			}
			if (range->issue_error != OK) {
				result = range->issue_error;
			}
		}
	}
	// A busy retain never replays this already attempted native slot. The Frame
	// remains its original owner until the independent recycle barrier succeeds.
	const Error retained = _native_height_retain_frame_sources(p_frame);
	return result != OK ? result : retained;
}

Error RenderingDevice::_native_height_retain_sources(const Ref<NativeHeightRange> &range) {
	while (range->retain_cursor < range->source_count) {
		auto &source = range->sources[range->retain_cursor];
		if (!source.retained) {
			if (source.retention_error != OK) {
				return source.retention_error;
			}
			if (!_native_disposal_admit(NativeDisposalWork::RETAIN)) {
				return ERR_BUSY;
			}
			const Error error = driver->native_call_retain(source.original.serial);
			if (error != OK) {
				if (error != ERR_BUSY) {
					source.retention_error = error;
					range->retention_error = error;
				}
				return error;
			}
			source.retained = true;
		}
		++range->retain_cursor;
	}
	if (range->uncaptured_prefix) {
		return ERR_INVALID_DATA; // The original Frame must not retire a lost prefix.
	}
	return OK;
}

Error RenderingDevice::_native_height_retain_frame_sources(uint32_t p_frame) {
	for (const Ref<NativeHeightRange> &range : frames[p_frame].native_height_ranges) {
		const Error retained = _native_height_retain_sources(range);
		if (retained != OK) {
			return retained;
		}
	}
	return OK;
}

Error RenderingDevice::_native_height_range_observe(const Ref<NativeHeightRange> &p_range, const NativeDisposalBudget &p_budget, NativeDisposalUsage &r_usage, NativeHeightRange::Snapshot &r_snapshot) {
	r_snapshot = {};
	const Error valid = _native_height_range_validate(p_range);
	if (valid != OK) {
		return valid;
	}
	Error result = OK;
	if (!p_range->native_complete && !p_range->observers_returned) {
		while (p_range->poll_cursor < p_range->source_count) {
			auto &source = p_range->sources[p_range->poll_cursor];
			if (!source.retained) {
				result = ERR_BUSY;
				break;
			}
			if (source.completion_error != OK) {
				result = source.completion_error;
				break;
			}
			if (!source.complete) {
				if (!_native_disposal_charge(p_budget, r_usage, NativeDisposalWork::POLL)) {
					result = ERR_BUSY;
					break;
				}
				const auto observed = driver->native_call_poll(source.original.serial);
				if (observed.native_error) {
					source.completion_error = FAILED;
					source.completion_native_code = observed.native_code;
					p_range->completion_error = FAILED;
					result = FAILED;
					break;
				}
				const bool accepted = source.original.issue == RDD::IssueFact::KnownAccepted ||
						(source.original.issue == RDD::IssueFact::IssuedUnknown && observed.resolved_issue == RDD::IssueFact::KnownAccepted);
				if (!accepted || source.original.error != OK || !observed.matches(source.original.completion_serial)) {
					result = ERR_BUSY;
					break;
				}
				source.complete = true;
			}
			++p_range->poll_cursor;
		}
	}
	uint32_t sealed = 0, attempted = 0, retained = 0, completed = 0;
	for (uint32_t i = 0; i < p_range->graph_count; ++i) {
		const auto &member = p_range->graphs[i];
		sealed += member.state == NativeHeightRange::MemberState::Sealed || member.state == NativeHeightRange::MemberState::IssueAttached;
		attempted += member.issue_attempted;
	}
	for (uint32_t i = 0; i < p_range->source_count; ++i) {
		retained += p_range->sources[i].retained;
		completed += p_range->sources[i].complete;
	}
	bool workers_attached = true;
	for (uint32_t i = 0; i < p_range->worker_count; ++i) {
		workers_attached = workers_attached && p_range->workers[i].attached;
	}
	const bool clean = workers_attached && p_range->append_error == OK && p_range->mapping_error == OK && p_range->issue_error == OK &&
			p_range->retention_error == OK && p_range->completion_error == OK && !p_range->uncaptured_prefix;
	const bool prefix_clean = workers_attached && p_range->recording_closed && p_range->has_recorded_effect && p_range->graph_count &&
			sealed == p_range->graph_count && attempted == p_range->graph_count && p_range->source_count &&
			retained == p_range->source_count && completed == p_range->source_count &&
			p_range->mapping_error == OK && p_range->issue_error == OK && p_range->retention_error == OK &&
			p_range->completion_error == OK && !p_range->uncaptured_prefix;
	if (prefix_clean) {
		p_range->prefix_settled = true;
	}
	if (clean && p_range->recording_closed && p_range->has_recorded_effect && p_range->graph_count && sealed == p_range->graph_count &&
			attempted == p_range->graph_count && p_range->source_count && retained == p_range->source_count && completed == p_range->source_count) {
		p_range->native_complete = true;
	}
	r_snapshot.recording_closed = p_range->recording_closed;
	r_snapshot.has_recorded_effect = p_range->has_recorded_effect;
	r_snapshot.mapping_closed = sealed == p_range->graph_count;
	r_snapshot.issue_closed = workers_attached && attempted == p_range->graph_count;
	r_snapshot.native_complete = p_range->native_complete;
	r_snapshot.prefix_settled = p_range->prefix_settled;
	r_snapshot.observers_returned = p_range->observers_returned;
	r_snapshot.graph_members = p_range->graph_count;
	r_snapshot.sealed_members = sealed;
	r_snapshot.issued_sources = p_range->source_count;
	r_snapshot.retained_sources = retained;
	r_snapshot.completed_sources = completed;
	r_snapshot.append_error = p_range->append_error;
	r_snapshot.mapping_error = p_range->mapping_error;
	r_snapshot.issue_error = p_range->issue_error;
	r_snapshot.retention_error = p_range->retention_error;
	r_snapshot.completion_error = p_range->completion_error;
	r_snapshot.return_error = p_range->return_error;
	for (Error error : { p_range->append_error, p_range->mapping_error, p_range->issue_error, p_range->retention_error, p_range->completion_error, p_range->return_error }) {
		if (error != OK) {
			return error;
		}
	}
	if (result != OK) {
		return result;
	}
	return p_range->has_recorded_effect && !p_range->native_complete && !p_range->prefix_settled ? ERR_BUSY : OK;
}

Error RenderingDevice::_native_height_range_return(const Ref<NativeHeightRange> &p_range, const NativeDisposalBudget &p_budget, NativeDisposalUsage &r_usage) {
	const Error valid = _native_height_range_validate(p_range);
	if (valid != OK) {
		return valid;
	}
	if (p_range->observers_returned) {
		return OK;
	}
	if (p_range->return_error != OK) {
		return p_range->return_error;
	}
	const bool known_no_effect = !p_range->has_recorded_effect && !p_range->uncaptured_prefix && !p_range->graph_count && !p_range->source_count && !p_range->edge_count && !p_range->worker_count;
	if (!p_range->recording_closed || (!p_range->native_complete && !p_range->prefix_settled && !known_no_effect)) {
		return ERR_BUSY;
	}
	while (p_range->return_cursor < p_range->source_count) {
		auto &source = p_range->sources[p_range->return_cursor];
		if (source.retained && !source.observer_returned) {
			if (!_native_disposal_charge(p_budget, r_usage, NativeDisposalWork::RELEASE)) {
				return ERR_BUSY;
			}
			const Error error = driver->native_call_release(source.original.serial);
			if (error != OK) {
				if (error != ERR_BUSY) {
					p_range->return_error = error;
				}
				return error;
			}
			source.observer_returned = true;
		}
		++p_range->return_cursor;
	}
	p_range->observers_returned = true;
	for (uint32_t i = 0; i < native_height_ranges.size(); ++i) {
		if (native_height_ranges[i] == p_range) {
			native_height_ranges.remove_at(i);
			break;
		}
	}
	// No later seal/issue may retarget a returned cell in a recycled Frame.
	for (auto &f : frames) {
		for (uint32_t i = 0; i < f.native_height_ranges.size(); ++i) {
			if (f.native_height_ranges[i] == p_range) {
				f.native_height_ranges.remove_at(i);
				break;
			}
		}
	}
	return OK;
}

Error RenderingDevice::_native_height_ranges_prepare_finalization() {
	const NativeDisposalBudget budget{ UINT32_MAX, UINT32_MAX, UINT64_MAX };
	NativeDisposalUsage usage;
	while (!native_height_ranges.is_empty()) {
		// Keep a local Ref: explicit return removes the original RD/Frame refs.
		const Ref<NativeHeightRange> range = native_height_ranges[0];
		// Only the original sink's explicit finish/unwind can close its capture:
		// there may be an actual checked result not yet enrolled when finalization
		// is entered. Empty reducer arrays do not establish a no-effect prefix.
		if (range->capture_sink && !range->recording_closed) {
			return ERR_BUSY;
		}
		range->recording_closed = true;
		if (!range->has_recorded_effect && !range->uncaptured_prefix && !range->graph_count && !range->source_count && !range->edge_count && !range->worker_count) {
			const Error returned = _native_height_range_return(range, budget, usage);
			if (returned != OK) {
				return returned;
			}
			continue;
		}
		NativeHeightRange::Snapshot snapshot;
		const Error observed = _native_height_range_observe(range, budget, usage, snapshot);
		// A checked capture can leave a sticky business error after its complete
		// native prefix has settled. Preserve that error in the snapshot, but do
		// not strand already-observed native sources or block finalization.
		const bool sticky_business_error = snapshot.prefix_settled && snapshot.append_error != OK &&
				snapshot.mapping_error == OK && snapshot.issue_error == OK && snapshot.retention_error == OK &&
				snapshot.completion_error == OK && snapshot.return_error == OK;
		if (observed != OK && !sticky_business_error) {
			return observed;
		}
		const Error returned = _native_height_range_return(range, budget, usage);
		if (returned != OK) {
			return returned;
		}
	}
	return OK;
}

Error RenderingDevice::_native_disposal_preflight() const {
	if (!is_main_instance || render_thread_id != Thread::get_caller_id() || !driver || native_finalization_returned) {
		return ERR_UNAUTHORIZED;
	}
	if (frames.is_empty() || uint32_t(frame) >= frames.size() || draw_list.active || compute_list.active || raytracing_list.active) {
		return ERR_BUSY;
	}
	return _recording_error();
}

Error RenderingDevice::_native_disposal_sampler_create(const SamplerState &p_state, RID &r_rid, NativeDisposalWitness &r_witness) {
	const Error ready = _native_disposal_preflight();
	if (ready != OK) {
		return ready;
	}
	_THREAD_SAFE_METHOD_
	if (r_rid.is_valid() || r_witness.cell.is_valid()) {
		return ERR_INVALID_PARAMETER;
	}
	const uint64_t serial = _reserve_backing_serial();
	if (!serial) {
		return ERR_OUT_OF_MEMORY;
	}
	NativeDisposalWitness reserved;
	reserved.cell.instantiate();
	reserved.cell->device = uint64_t(get_instance_id());
	reserved.cell->serial = serial;
	const RID created = _sampler_create_internal(p_state, nullptr, &reserved);
	if (created.is_null()) {
		return ERR_CANT_CREATE;
	}
	r_rid = created;
	r_witness = reserved;
	return OK;
}

Error RenderingDevice::_native_disposal_depth_texture_create(uint32_t p_width, uint32_t p_height, RID &r_rid, NativeDisposalWitness &r_witness) {
	const Error ready = _native_disposal_preflight();
	if (ready != OK) {
		return ready;
	}
	_THREAD_SAFE_METHOD_
	if (!p_width || !p_height || r_rid.is_valid() || r_witness.cell.is_valid()) {
		return ERR_INVALID_PARAMETER;
	}
	const uint64_t serial = _reserve_backing_serial();
	if (!serial) {
		return ERR_OUT_OF_MEMORY;
	}
	NativeDisposalWitness reserved;
	reserved.cell.instantiate();
	reserved.cell->device = uint64_t(get_instance_id());
	reserved.cell->serial = serial;
	const RID id = texture_owner.allocate_rid();
	if (id.is_null()) {
		return ERR_OUT_OF_MEMORY;
	}
	TextureFormat format;
	format.texture_type = TEXTURE_TYPE_2D;
	format.format = DATA_FORMAT_D32_SFLOAT;
	format.width = p_width;
	format.height = p_height;
	format.usage_bits = TEXTURE_USAGE_SAMPLING_BIT | TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	// No upload/clear is issued here. A failed allocation has no native prefix.
	const RID created = _texture_create_internal(format, TextureView(), {}, nullptr, id, &reserved);
	if (created.is_null()) {
		texture_owner.initialize_rid(id, Texture());
		texture_owner.free(id);
		return ERR_CANT_CREATE;
	}
	r_rid = created;
	r_witness = reserved;
	return OK;
}

Error RenderingDevice::_native_disposal_depth_framebuffer_create(const NativeDisposalWitness &p_texture, RID &r_rid, NativeDisposalWitness &r_witness) {
	const Error ready = _native_disposal_preflight();
	if (ready != OK) {
		return ready;
	}
	_THREAD_SAFE_METHOD_
	if (r_rid.is_valid() || r_witness.cell.is_valid()) {
		return ERR_INVALID_PARAMETER;
	}
	NativeDisposalWitness::Report target_report;
	const Error observed = _native_disposal_observe(p_texture, target_report);
	if (observed != OK) {
		return observed;
	}
	const RID target = RID::from_uint64(target_report.rid);
	const Texture *texture = texture_owner.owns(target) ? texture_owner.get_or_null(target) : nullptr;
	if (!texture || target_report.state != NativeDisposalWitness::State::LIVE || texture->native_disposal.cell != p_texture.cell ||
			!p_texture.cell->matches(target.get_id(), texture->native_disposal_serial, texture->driver_id.id)) {
		return ERR_INVALID_DATA;
	}
	if (texture->qualification_return || texture->native_immutable_retire_requested || texture->owner.is_valid() || texture->shared_fallback || texture->transfer_worker_index >= 0 || texture->format != DATA_FORMAT_D32_SFLOAT ||
			texture->samples != TEXTURE_SAMPLES_1 || texture->usage_flags != (TEXTURE_USAGE_SAMPLING_BIT | TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)) {
		return ERR_UNAVAILABLE;
	}
	const uint64_t serial = _reserve_backing_serial();
	if (!serial) {
		return ERR_OUT_OF_MEMORY;
	}
	NativeDisposalWitness reserved;
	reserved.cell.instantiate();
	reserved.cell->device = uint64_t(get_instance_id());
	reserved.cell->serial = serial;
	const RID id = framebuffer_owner.allocate_rid();
	if (id.is_null()) {
		return ERR_OUT_OF_MEMORY;
	}
	Vector<RID> attachments;
	attachments.push_back(target);
	FramebufferPass pass;
	pass.depth_attachment = 0;
	Vector<FramebufferPass> passes;
	passes.push_back(pass);
	const RID created = _qualification_framebuffer_create_multipass(attachments, passes, INVALID_ID, 1, nullptr, nullptr, id, &reserved);
	if (created.is_null()) {
		framebuffer_owner.initialize_rid(id, Framebuffer());
		framebuffer_owner.free(id);
		// The texture was already published by its own factory. Its original
		// caller still owns its LIVE witness and performs any rollback.
		return ERR_CANT_CREATE;
	}
	r_rid = created;
	r_witness = reserved;
	return OK;
}

Error RenderingDevice::_native_disposal_depth_sample_set_create(const NativeDisposalWitness &p_sampler, const NativeDisposalWitness &p_texture, RID p_shader, uint32_t p_set, uint32_t p_binding, RID &r_rid, NativeDisposalWitness &r_witness) {
	const Error ready = _native_disposal_preflight();
	if (ready != OK) {
		return ready;
	}
	_THREAD_SAFE_METHOD_
	if (r_rid.is_valid() || r_witness.cell.is_valid()) {
		return ERR_INVALID_PARAMETER;
	}
	NativeDisposalWitness::Report sampler_report, texture_report;
	Error observed = _native_disposal_observe(p_sampler, sampler_report);
	if (observed != OK) {
		return observed;
	}
	observed = _native_disposal_observe(p_texture, texture_report);
	if (observed != OK) {
		return observed;
	}
	const RID sampler_rid = RID::from_uint64(sampler_report.rid);
	const RID texture_rid = RID::from_uint64(texture_report.rid);
	// Reject foreign validators before inspecting possibly uninitialized slots.
	const Sampler *sampler = sampler_owner.owns(sampler_rid) ? sampler_owner.get_or_null(sampler_rid) : nullptr;
	const Texture *texture = texture_owner.owns(texture_rid) ? texture_owner.get_or_null(texture_rid) : nullptr;
	if (!sampler || !sampler->driver_id || sampler_report.state != NativeDisposalWitness::State::LIVE || sampler->native_disposal.cell != p_sampler.cell || sampler->native_disposal_rid != sampler_rid.get_id() ||
			!p_sampler.cell->matches(sampler_rid.get_id(), sampler->native_disposal_serial, sampler->driver_id.id) ||
			!texture || !texture->driver_id || texture_report.state != NativeDisposalWitness::State::LIVE || texture->native_disposal.cell != p_texture.cell || texture->native_disposal_rid != texture_rid.get_id() ||
			!p_texture.cell->matches(texture_rid.get_id(), texture->native_disposal_serial, texture->driver_id.id)) {
		return ERR_INVALID_DATA;
	}
	// Dependency invalidation callbacks run unlocked while the original root
	// free is active. They cannot add a new descriptor child to that root.
	if (resource_frees_in_progress.has(sampler_rid) || resource_frees_in_progress.has(texture_rid) || resource_frees_in_progress.has(p_shader)) {
		return ERR_BUSY;
	}
	const Shader *shader = shader_owner.owns(p_shader) ? shader_owner.get_or_null(p_shader) : nullptr;
	if (!shader || !shader->driver_id || shader->qualification_return || shader->native_immutable_retire_requested || !shader->native_immutable_samplers.is_empty() ||
			sampler->qualification_return || sampler->native_immutable_retire_requested || texture->qualification_return || texture->native_immutable_retire_requested) {
		return ERR_UNAVAILABLE;
	}
	if (texture->type != TEXTURE_TYPE_2D || texture->format != DATA_FORMAT_D32_SFLOAT || texture->samples != TEXTURE_SAMPLES_1 ||
			!texture->width || !texture->height || texture->depth != 1 || texture->layers != 1 || texture->mipmaps != 1 || texture->base_layer || texture->base_mipmap ||
			texture->usage_flags != (TEXTURE_USAGE_SAMPLING_BIT | TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) || texture->owner.is_valid() || texture->shared_fallback ||
			texture->transfer_worker_index >= 0 || !texture->draw_tracker) {
		return ERR_UNAVAILABLE;
	}
	if (p_set >= uint32_t(shader->uniform_sets.size()) || p_set >= uint32_t(shader->set_formats.size()) || shader->uniform_sets[p_set].size() != 1) {
		return ERR_UNAVAILABLE;
	}
	const ShaderUniform &binding = shader->uniform_sets[p_set][0];
	if (binding.binding != p_binding || binding.type != UNIFORM_TYPE_SAMPLER_WITH_TEXTURE || binding.length != 1 || binding.writable) {
		return ERR_UNAVAILABLE;
	}
	Vector<RID> ids;
	const Error allocated = ids.resize(2);
	if (allocated != OK) {
		return allocated;
	}
	ids.write[0] = sampler_rid;
	ids.write[1] = texture_rid;
	Uniform uniform(UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, ids);
	uniform.binding = p_binding;
	uniform.immutable_sampler = false;
	const uint64_t serial = _reserve_backing_serial();
	if (!serial) {
		return ERR_OUT_OF_MEMORY;
	}
	NativeDisposalWitness reserved;
	reserved.cell.instantiate();
	reserved.cell->device = uint64_t(get_instance_id());
	reserved.cell->serial = serial;
	// The original factory reserves its RID before native creation and owns
	// all dependency edges. It records pending_clear metadata without consuming
	// the clear. This disposal witness grants no read/source/bind authority.
	const RID created = _uniform_set_create_qualification(VectorView<Uniform>(&uniform, 1), p_shader, p_set, false, nullptr, nullptr, &reserved);
	if (created.is_null()) {
		return ERR_CANT_CREATE; // Earlier published parents remain caller-owned.
	}
	r_rid = created;
	r_witness = reserved;
	return OK;
}

Error RenderingDevice::_native_disposal_storage_create(uint32_t p_bytes, BitField<StorageBufferUsage> p_usage, RID &r_rid, NativeDisposalWitness &r_witness) {
	const Error ready = _native_disposal_preflight();
	if (ready != OK) {
		return ready;
	}
	if (!p_bytes || r_rid.is_valid() || r_witness.cell.is_valid()) {
		return ERR_INVALID_PARAMETER;
	}
	if (uint64_t(p_usage) & ~uint64_t(STORAGE_BUFFER_USAGE_DISPATCH_INDIRECT)) {
		return ERR_UNAVAILABLE;
	}
	NativeDisposalWitness reserved;
	reserved.cell.instantiate(); // Entire observation storage exists before native creation.
	reserved.cell->device = uint64_t(get_instance_id());
	// No initial upload: a failed factory cannot hide a recorded upload prefix.
	const RID rid = _storage_buffer_create_internal(p_bytes, Span<uint8_t>(), p_usage, {}, &reserved);
	if (rid.is_null()) {
		return ERR_CANT_CREATE;
	}
	r_rid = rid;
	r_witness = reserved;
	return OK;
}

Error RenderingDevice::_native_disposal_uniform_set_create(const VectorView<Uniform> &p_uniforms, RID p_shader, uint32_t p_set, RID &r_rid, NativeDisposalWitness &r_witness, const GDExtensionDeviceExit::ProductionRequest *p_owner, uint64_t p_group, uint64_t p_root, uint32_t p_ordinal) {
	const Error ready = _native_disposal_preflight();
	if (ready != OK) {
		return ready;
	}
	if (r_rid.is_valid() || r_witness.cell.is_valid() || p_uniforms.size() == 0) {
		return ERR_INVALID_PARAMETER;
	}
	const Shader *shader = shader_owner.get_or_null(p_shader);
	if (!shader || !shader->driver_id || shader->qualification_return) {
		return ERR_UNAVAILABLE;
	}
	// This batch supports only mutable, Core-created storage buffers. Texture,
	// VA/IA, shader/pipeline disposal and arbitrary existing resources have no
	// production observation contract here. Reject before synchronization work.
	for (uint32_t uniform_index = 0; uniform_index < p_uniforms.size(); ++uniform_index) {
		const Uniform &uniform = p_uniforms[uniform_index];
		if (uniform.uniform_type != UNIFORM_TYPE_STORAGE_BUFFER || uniform.get_id_count() != 1) {
			return ERR_UNAVAILABLE;
		}
		const Buffer *buffer = storage_buffer_owner.get_or_null(uniform.get_id(0));
		if (!buffer || buffer->qualification_return || !buffer->draw_tracker || buffer->transfer_worker_index >= 0 || buffer->native_disposal.cell.is_null()) {
			return ERR_UNAVAILABLE;
		}
		const auto &cell = buffer->native_disposal.cell;
		if (cell->device != uint64_t(get_instance_id()) || cell->state != NativeDisposalWitness::State::LIVE || !cell->matches(uniform.get_id(0).get_id(), buffer->backing_serial, buffer->driver_id.id)) {
			return ERR_INVALID_DATA;
		}
	}
	const uint64_t serial = _reserve_backing_serial();
	if (!serial) {
		return ERR_OUT_OF_MEMORY;
	}
	NativeDisposalWitness reserved;
	reserved.cell.instantiate();
	reserved.cell->device = uint64_t(get_instance_id());
	reserved.cell->serial = serial;
	if (p_owner) {
		if (!p_owner->serial || !p_group || !p_root || p_ordinal == UINT32_MAX) {
			return ERR_UNAUTHORIZED;
		}
		reserved.cell->production_request = const_cast<GDExtensionDeviceExit::ProductionRequest *>(p_owner);
		reserved.cell->production_request_serial = p_owner->serial;
		reserved.cell->production_group_serial = p_group;
		reserved.cell->production_pin_serial = p_root;
		reserved.cell->production_ordinal = p_ordinal;
	}
	const RID rid = _uniform_set_create_qualification(p_uniforms, p_shader, p_set, false, nullptr, nullptr, &reserved, p_owner);
	if (rid.is_null()) {
		return ERR_CANT_CREATE;
	}
	r_rid = rid;
	r_witness = reserved;
	return OK;
}

Error RenderingDevice::_native_disposal_observe(const NativeDisposalWitness &p_witness, NativeDisposalWitness::Report &r_report) const {
	// Observation is possible during recovery, but never from a foreign RD/RT.
	if (!is_main_instance || render_thread_id != Thread::get_caller_id() || p_witness.cell.is_null() || p_witness.cell->device != uint64_t(get_instance_id())) {
		return ERR_UNAUTHORIZED;
	}
	const auto &cell = p_witness.cell;
	r_report = { cell->state, cell->rid, cell->serial };
	return cell->state == NativeDisposalWitness::State::UNKNOWN ? ERR_INVALID_DATA : OK;
}

Error RenderingDevice::_native_disposal_depth_allocation_quote(uint32_t p_width, uint32_t p_height, NativeDepthAllocationQuote &r_quote) const {
	const Error ready = _native_disposal_preflight();
	if (ready != OK) {
		return ready;
	}
	if (!p_width || !p_height || r_quote.device) {
		return ERR_INVALID_PARAMETER;
	}
	_THREAD_SAFE_METHOD_
	RDD::DepthAllocationQuote native;
	const Error error = driver->depth_allocation_quote(p_width, p_height, native);
	if (error != OK) {
		return error;
	}
	if ((native.policy != RDD::DepthAllocationQuote::Policy::MetalHeapPool && native.policy != RDD::DepthAllocationQuote::Policy::MetalHeapDedicated) || !native.native_requirement || !native.owner_reservation ||
			native.native_requirement > native.owner_reservation || !native.alignment || (native.alignment & (native.alignment - 1))) {
		return ERR_INVALID_DATA;
	}
	NativeDepthAllocationQuote quote;
	quote.device = uint64_t(get_instance_id());
	quote.report.width = p_width;
	quote.report.height = p_height;
	quote.report.policy = native.policy == RDD::DepthAllocationQuote::Policy::MetalHeapPool ? NativeDepthAllocationQuote::Policy::MetalHeapPool : NativeDepthAllocationQuote::Policy::MetalHeapDedicated;
	quote.report.texture_native_requirement = { NativeDepthAllocationProjection::ByteKnowledge::Known, native.native_requirement };
	quote.report.texture_owner_reservation = { NativeDepthAllocationProjection::ByteKnowledge::Known, native.owner_reservation };
	quote.report.framebuffer_owner_reservation = { NativeDepthAllocationProjection::ByteKnowledge::Known, 0 };
	quote.report.alignment = native.alignment;
	quote.report.allocations = 2;
	quote.report.parts = 1;
	quote.report.shared_attachment_references = 1;
	r_quote = quote;
	return OK;
}

Error RenderingDevice::_native_disposal_depth_allocation_quote_observe(const NativeDepthAllocationQuote &p_quote, NativeDepthAllocationQuote::Report &r_report) const {
	r_report = {};
	if (!is_main_instance || render_thread_id != Thread::get_caller_id() || !p_quote.device || p_quote.device != uint64_t(get_instance_id())) {
		return ERR_UNAUTHORIZED;
	}
	r_report = p_quote.report;
	return OK;
}

Error RenderingDevice::_native_disposal_depth_texture_create_quoted(const NativeDepthAllocationQuote &p_quote, RID &r_rid, NativeDisposalWitness &r_witness, NativeDepthAllocationProjection &r_projection, NativeDepthAllocationQuote::CreateOutcome &r_outcome) {
	r_outcome = NativeDepthAllocationQuote::CreateOutcome::NotCreated;
	NativeDepthAllocationQuote::Report quote;
	const Error observed = _native_disposal_depth_allocation_quote_observe(p_quote, quote);
	if (observed != OK) {
		return observed;
	}
	if (r_projection.device || r_rid.is_valid() || r_witness.cell.is_valid()) {
		return ERR_INVALID_PARAMETER;
	}
	// This recheck binds the original quote to the current original allocator
	// policy before native effect. C still owns the retained E05 reservation.
	NativeDepthAllocationQuote current;
	const Error requoted = _native_disposal_depth_allocation_quote(quote.width, quote.height, current);
	if (requoted != OK) {
		return requoted;
	}
	if (current.report.policy != quote.policy || current.report.alignment != quote.alignment || current.report.texture_owner_reservation.bytes > quote.texture_owner_reservation.bytes) {
		return ERR_OUT_OF_MEMORY;
	}
	const Error created = _native_disposal_depth_texture_create(quote.width, quote.height, r_rid, r_witness);
	if (created != OK) {
		return created;
	}
	r_outcome = NativeDepthAllocationQuote::CreateOutcome::CreatedActualUnknown;
	const Error captured = _native_disposal_depth_allocation_capture(r_witness, NativeDisposalWitness(), r_projection);
	if (captured != OK) {
		return captured;
	}
	if (r_projection.depth_driver_backing.knowledge != NativeDepthAllocationProjection::ByteKnowledge::Known) {
		return ERR_UNAVAILABLE;
	}
	if (r_projection.depth_driver_backing.bytes > quote.texture_owner_reservation.bytes) {
		r_outcome = NativeDepthAllocationQuote::CreateOutcome::CreatedOverReservation;
		return ERR_OUT_OF_MEMORY;
	}
	r_outcome = NativeDepthAllocationQuote::CreateOutcome::CreatedVerified;
	return OK;
}

Error RenderingDevice::_native_disposal_depth_allocation_capture(const NativeDisposalWitness &p_depth, const NativeDisposalWitness &p_framebuffer, NativeDepthAllocationProjection &r_projection) {
	if (r_projection.device) {
		return ERR_INVALID_PARAMETER;
	}
	NativeDisposalWitness::Report depth_report;
	const Error observed = _native_disposal_observe(p_depth, depth_report);
	if (observed != OK) {
		return observed;
	}
	_THREAD_SAFE_METHOD_
	const RID depth_rid = RID::from_uint64(depth_report.rid);
	const Texture *texture = texture_owner.owns(depth_rid) ? texture_owner.get_or_null(depth_rid) : nullptr;
	if (!texture || depth_report.state != NativeDisposalWitness::State::LIVE || texture->native_disposal.cell != p_depth.cell ||
			!p_depth.cell->matches(depth_report.rid, texture->native_disposal_serial, texture->driver_id.id)) {
		return ERR_INVALID_DATA;
	}
	if (texture->owner.is_valid() || texture->shared_fallback || texture->transfer_worker_index >= 0 || texture->format != DATA_FORMAT_D32_SFLOAT ||
			texture->type != TEXTURE_TYPE_2D || texture->depth != 1 || texture->layers != 1 || texture->mipmaps != 1 || texture->samples != TEXTURE_SAMPLES_1 || texture->usage_flags != (TEXTURE_USAGE_SAMPLING_BIT | TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)) {
		return ERR_UNAVAILABLE;
	}
	if (p_framebuffer.cell.is_valid()) {
		NativeDisposalWitness::Report framebuffer_report;
		const Error framebuffer_observed = _native_disposal_observe(p_framebuffer, framebuffer_report);
		if (framebuffer_observed != OK) {
			return framebuffer_observed;
		}
		const RID framebuffer_rid = RID::from_uint64(framebuffer_report.rid);
		const Framebuffer *fb = framebuffer_owner.owns(framebuffer_rid) ? framebuffer_owner.get_or_null(framebuffer_rid) : nullptr;
		if (!fb || framebuffer_report.state != NativeDisposalWitness::State::LIVE || fb->native_disposal.cell != p_framebuffer.cell ||
				!p_framebuffer.cell->matches(framebuffer_report.rid, fb->native_disposal_serial, fb->native_disposal_backing) ||
				!fb->framebuffer_cache || uint64_t(uintptr_t(fb->framebuffer_cache)) != fb->native_disposal_backing ||
				fb->texture_ids.size() != 1 || fb->texture_ids[0] != depth_rid || fb->framebuffer_cache->textures.size() != 1 || fb->framebuffer_cache->textures[0] != texture->driver_id) {
			return ERR_INVALID_DATA;
		}
	}
	if (!driver) {
		return ERR_UNAVAILABLE;
	}
	NativeDepthAllocationProjection projection;
	projection.device = uint64_t(get_instance_id());
	projection.depth = p_depth;
	projection.framebuffer = p_framebuffer;
	// This standalone original texture has no shared/view backing. The driver
	// exposes its allocation only, never opaque driver or framebuffer overhead.
	const uint64_t actual = driver->texture_get_allocation_size(texture->driver_id);
	if (actual) {
		projection.depth_driver_backing = { NativeDepthAllocationProjection::ByteKnowledge::Known, actual };
	}
	r_projection = projection;
	return OK;
}

Error RenderingDevice::_native_disposal_depth_allocation_observe(const NativeDepthAllocationProjection &p_projection, NativeDepthAllocationProjection::Report &r_report) const {
	r_report = {};
	if (!is_main_instance || render_thread_id != Thread::get_caller_id() || !p_projection.device || p_projection.device != uint64_t(get_instance_id())) {
		return ERR_UNAUTHORIZED;
	}
	r_report.depth_driver_backing = p_projection.depth_driver_backing;
	r_report.framebuffer_created = p_projection.framebuffer.cell.is_valid();
	const Error depth_error = _native_disposal_observe(p_projection.depth, r_report.depth);
	Error framebuffer_error = OK;
	if (r_report.framebuffer_created) {
		framebuffer_error = _native_disposal_observe(p_projection.framebuffer, r_report.framebuffer);
		r_report.shared_attachment_depth = r_report.depth;
	}
	r_report.all_created_children_returned = depth_error == OK && framebuffer_error == OK &&
			r_report.depth.state == NativeDisposalWitness::State::RETURNED &&
			(!r_report.framebuffer_created || r_report.framebuffer.state == NativeDisposalWitness::State::RETURNED);
	return depth_error != OK ? depth_error : framebuffer_error;
}

Error RenderingDevice::_native_disposal_free(const NativeDisposalWitness &p_witness) {
	const Error ready = _native_disposal_preflight();
	if (ready != OK) {
		return ready;
	}
	NativeDisposalWitness::Report report;
	const Error observed = _native_disposal_observe(p_witness, report);
	if (observed != OK) {
		return observed;
	}
	if (report.state == NativeDisposalWitness::State::ENQUEUED || report.state == NativeDisposalWitness::State::RETURNED) {
		return OK; // Idempotent request; OK does not mean driver-free returned.
	}
	const RID rid = RID::from_uint64(report.rid);
	const Buffer *buffer = storage_buffer_owner.owns(rid) ? storage_buffer_owner.get_or_null(rid) : nullptr;
	const UniformSet *set = uniform_set_owner.owns(rid) ? uniform_set_owner.get_or_null(rid) : nullptr;
	const Texture *texture = texture_owner.owns(rid) ? texture_owner.get_or_null(rid) : nullptr;
	const Framebuffer *framebuffer = framebuffer_owner.owns(rid) ? framebuffer_owner.get_or_null(rid) : nullptr;
	const Sampler *sampler = sampler_owner.owns(rid) ? sampler_owner.get_or_null(rid) : nullptr;
	const auto &cell = p_witness.cell;
	if ((!buffer || buffer->native_disposal.cell != cell || !cell->matches(rid.get_id(), buffer->backing_serial, buffer->driver_id.id)) &&
			(!set || set->native_disposal.cell != cell || !cell->matches(rid.get_id(), set->native_disposal_serial, set->driver_id.id)) &&
			(!texture || texture->native_disposal.cell != cell || !cell->matches(rid.get_id(), texture->native_disposal_serial, texture->driver_id.id)) &&
			(!sampler || sampler->native_disposal.cell != cell || !cell->matches(rid.get_id(), sampler->native_disposal_serial, sampler->driver_id.id)) &&
			(!framebuffer || framebuffer->native_disposal.cell != cell || !cell->matches(rid.get_id(), framebuffer->native_disposal_serial, uint64_t(uintptr_t(framebuffer->framebuffer_cache))))) {
		cell->state = NativeDisposalWitness::State::UNKNOWN;
		return ERR_INVALID_DATA;
	}
	if (cell->production_dependent_sets) {
		return ERR_BUSY;
	}
	// Original dependency recursion, invalidation and pending-free queue own
	// disposal. No logical free, elapsed frame count or sync writes RETURNED.
	free_rid(rid);
	return cell->state == NativeDisposalWitness::State::ENQUEUED || cell->state == NativeDisposalWitness::State::RETURNED ? OK : ERR_BUSY;
}

bool RenderingDevice::_native_disposal_enqueue(NativeDisposalWitness &p_witness, uint64_t p_rid, uint64_t p_serial, uint64_t p_backing) {
	if (p_witness.cell.is_null()) {
		return true;
	}
	if (p_witness.cell->production_dependent_sets) {
		return false;
	}
	if (p_witness.cell->device != uint64_t(get_instance_id())) {
		p_witness.cell->state = NativeDisposalWitness::State::UNKNOWN;
		return false;
	}
	return p_witness.cell->enqueue(p_rid, p_serial, p_backing);
}

void RenderingDevice::_native_disposal_returned(NativeDisposalWitness &p_witness, uint64_t p_rid, uint64_t p_serial, uint64_t p_backing) {
	if (p_witness.cell.is_valid()) {
		if (p_witness.cell->device != uint64_t(get_instance_id())) {
			p_witness.cell->state = NativeDisposalWitness::State::UNKNOWN;
			return;
		}
		p_witness.cell->returned(p_rid, p_serial, p_backing);
	}
}

Error RenderingDevice::_qualification_maintenance() {
	return _native_disposal_maintenance();
}

Error RenderingDevice::_native_disposal_maintenance_step(const NativeDisposalBudget &p_budget, NativeDisposalUsage &r_usage, const GDExtensionDeviceExit::ProductionRequest *p_production_owner) {
	QualificationSourceAdmissionScope source_admission(*this);
	NativeDrainDiagnostic::Span drain_trace("rd.maintenance", uintptr_t(this), uintptr_t(p_production_owner));
	if (!p_budget.receipts || !p_budget.passes || !p_budget.deadline_usec || OS::get_singleton()->get_ticks_usec() >= p_budget.deadline_usec) {
		return drain_trace.result(ERR_BUSY, "rd.return.1");
	}
	if (render_thread_id != Thread::get_caller_id() || !is_main_instance || !driver || native_finalization_returned) {
		return drain_trace.result(ERR_UNAUTHORIZED, "rd.return.2");
	}
	if (frames.is_empty() || uint32_t(frame) >= frames.size()) {
		return drain_trace.result(ERR_UNCONFIGURED, "rd.return.3");
	}
	if (frame_issue_error != OK) {
		return drain_trace.result(frame_issue_error.load(), "rd.return.4");
	}
	if (draw_list.active || compute_list.active || raytracing_list.active || frame_wait_draining || native_disposal_budget) {
		return drain_trace.result(ERR_BUSY, "rd.return.5");
	}
	if (!_thread_safe_.try_lock()) {
		return drain_trace.result(ERR_BUSY, "rd.return.6");
	}
	struct Unlock {
		RenderingDevice &rd;
		~Unlock() {
			rd.native_disposal_budget = nullptr;
			rd.native_disposal_usage = nullptr;
			rd._thread_safe_.unlock();
		}
	} unlock{ *this };
	drain_trace.state(uintptr_t(this), uintptr_t(p_production_owner), frame, native_disposal_continuation.serial, native_disposal_continuation.phase, native_disposal_continuation.end_phase);
	if (native_disposal_active && native_disposal_continuation.production_owner != p_production_owner) {
		return drain_trace.result(ERR_BUSY, "rd.return.7"); // A foreign lane cannot resume the original owner's cursor.
	}
	// Only terminal/cancelled tickets may spend their reserved cleanup slice.
	const Error cleanup_credits = _qualification_activate_cleanup_credits();
	if (cleanup_credits != OK) {
		return drain_trace.result(cleanup_credits, "rd.cleanup.credit.lock");
	}
	native_disposal_budget = &p_budget;
	native_disposal_usage = &r_usage;
	if (!_native_disposal_available(NativeDisposalWork::WORK)) {
		return drain_trace.result(ERR_BUSY, "rd.return.8");
	}
	auto &cursor = native_disposal_continuation;
	if (!native_disposal_active) {
		if (draw_graph.has_driver_callbacks()) {
			return drain_trace.result(ERR_BUSY, "rd.return.9");
		}
		if (!next_native_disposal_serial) {
			return drain_trace.result(ERR_PARAMETER_RANGE_ERROR, "rd.return.10");
		}
		const uint32_t target = qualification_frame_advance_pending ? uint32_t(frame) : (frame + 1) % frames.size();
		if (p_production_owner && frames[target].disposal.active) {
			return drain_trace.result(ERR_BUSY, "rd.return.11"); // A pre-existing ordinary drain keeps its original cutoff.
		}
		cursor = {};
		cursor.production_owner = p_production_owner;
		cursor.serial = next_native_disposal_serial;
		next_native_disposal_serial = cursor.serial == UINT64_MAX ? 0 : cursor.serial + 1;
		cursor.target = target;
		if (p_production_owner) {
			// The original production preflight covered this exact prefix. Late
			// logical frees remain appended for a later ordinary safe-frame drain.
			_capture_disposal_cutoff(target);
		}
		// An ordinary advance already issued its old frame. Resume only this
		// exact target, never execute the preceding graph again.
		cursor.phase = qualification_frame_advance_pending ? 3 : 0;
		native_disposal_active = true;
	}
	if (cursor.phase == 0) {
		if (failed_wait_frame >= 0 && uint32_t(failed_wait_frame) != cursor.target) {
			const Error previous = _native_disposal_stall_step(failed_wait_frame);
			if (previous != OK) {
				return drain_trace.result(previous, "rd.return.12");
			}
		}
		const Error error = _native_disposal_stall_step(cursor.target);
		if (error != OK) {
			return drain_trace.result(error, "rd.return.13");
		}
		cursor.phase = 1;
		cursor.command_buffer = frames[frame].command_buffer;
	}
	if (cursor.phase == 1) {
		auto &f = frames[frame];
		if (f.chain_prefix_poisoned) {
			return drain_trace.result(_fail_frame_issue(FAILED), "rd.return.14");
		}
		if (f.qualification_graph_closed) {
			cursor.end_phase = f.qualification_segment_closed ? 5 : 4;
		}
		if (!f.fence_signaled && f.primary_command_buffer_begun) {
			if (cursor.end_phase == 0) {
				uint32_t bound = 0;
				if (!driver->native_issue_bound(RDD::NativeIssueKind::PREPARE_SEGMENT, 0, bound)) {
					return drain_trace.result(ERR_UNAVAILABLE, "rd.return.15");
				}
				if (!_native_disposal_admit(NativeDisposalWork::ISSUE, bound)) {
					return drain_trace.result(ERR_BUSY, "rd.return.16");
				}
				RDD::QueueCallReport hidden;
				const Error error = driver->qualification_prepare_segment(hidden, true);
				if (!hidden.calls.is_empty()) {
					f.qualification_hidden_reports.push_back(hidden);
					_qualification_issue_tickets(UINT32_MAX, hidden, error);
				}
				if (hidden.native_issue_count > bound) {
					r_usage.issues = uint32_t(MIN(uint64_t(UINT32_MAX), uint64_t(r_usage.issues) + hidden.native_issue_count - bound));
					// An invalid backend bound cannot erase its actual source prefix.
					return drain_trace.result(_fail_frame_issue(ERR_INVALID_DATA), "rd.return.17");
				}
				r_usage.issues -= bound - hidden.native_issue_count;
				if (error != OK) {
					return drain_trace.result(error, "rd.return.18");
				}
				cursor.end_phase = 1;
			}
			if (cursor.end_phase == 1) {
				const Error error = _submit_transfer_workers(cursor.command_buffer, true);
				if (error != OK) {
					return drain_trace.result(error == ERR_BUSY ? error : _fail_frame_issue(error), "rd.return.19");
				}
				cursor.end_phase = 2;
			}
			if (cursor.end_phase == 2) {
				if (!_native_disposal_admit(NativeDisposalWork::WORK)) {
					return drain_trace.result(ERR_BUSY, "rd.return.20");
				}
				// The caller validated every closed graph record against its
				// accepted capture/quote. Recording this graph is one atomic pass;
				// its native issues above/below are individually charged receipts.
				cursor.end_phase = 3; // Never replay an uncertain graph prefix.
				{
					NativeDrainDiagnostic::Span graph_trace("graph.maintenance", uintptr_t(this), cursor.command_buffer.id, cursor.serial, frame, cursor.phase, cursor.end_phase);
					draw_graph.end(RENDER_GRAPH_REORDER == 1, RENDER_GRAPH_FULL_BARRIERS == 1, cursor.command_buffer, f.command_buffer_pool);
				}
				_seal_buffer_readback_records();
				_native_height_seal_frame(frame);
				_qualification_seal_tickets();
				if (draw_graph.recording_failed()) {
					return drain_trace.result(_fail_frame_issue(FAILED), "rd.return.21");
				}
			}
			if (cursor.end_phase == 3) {
				if (!_native_disposal_admit(NativeDisposalWork::WORK)) {
					return drain_trace.result(ERR_BUSY, "rd.return.22");
				}
				driver->command_buffer_end(cursor.command_buffer);
				f.qualification_graph_closed = true;
				cursor.end_phase = 4;
			}
			if (cursor.end_phase == 4) {
				// Graph compilation may itself produce hidden copy work. Never
				// enter the legacy end_segment wait from a bounded owner turn.
				uint32_t bound = 0;
				if (!driver->native_issue_bound(RDD::NativeIssueKind::PREPARE_SEGMENT, 0, bound)) {
					return drain_trace.result(ERR_UNAVAILABLE, "rd.return.23");
				}
				if (!_native_disposal_admit(NativeDisposalWork::ISSUE, bound)) {
					return drain_trace.result(ERR_BUSY, "rd.return.24");
				}
				RDD::QueueCallReport hidden;
				const Error error = driver->qualification_prepare_segment(hidden, true);
				if (!hidden.calls.is_empty()) {
					f.qualification_hidden_reports.push_back(hidden);
					_qualification_issue_tickets(UINT32_MAX, hidden, error);
				}
				if (hidden.native_issue_count > bound) {
					r_usage.issues = uint32_t(MIN(uint64_t(UINT32_MAX), uint64_t(r_usage.issues) + hidden.native_issue_count - bound));
					// An invalid backend bound cannot erase its actual source prefix.
					return drain_trace.result(_fail_frame_issue(ERR_INVALID_DATA), "rd.return.25");
				}
				r_usage.issues -= bound - hidden.native_issue_count;
				if (error != OK) {
					return drain_trace.result(error, "rd.return.26");
				}
				cursor.end_phase = 5;
				f.qualification_graph_closed = true;
				f.qualification_segment_closed = true;
			}
		}
		cursor.phase = 2;
		if (!f.fence_signaled && f.primary_command_buffer_begun) {
			cursor.slot_count = 1 + f.command_buffer_pool.buffers_used;
			cursor.slot = f.qualification_issue_slot;
			if (!cursor.slot) {
				f.chain_reports.clear();
			}
			f.chain_reports.reserve(cursor.slot_count);
		}
	}
	if (cursor.phase == 2) {
		auto &f = frames[frame];
		auto &pool = f.command_buffer_pool;
		while (cursor.slot < cursor.slot_count) {
			uint32_t bound = 0;
			if (!driver->native_issue_bound(RDD::NativeIssueKind::MAIN_SLOT, 1, bound)) {
				return drain_trace.result(ERR_UNAVAILABLE, "rd.return.27");
			}
			if (!_native_disposal_admit(NativeDisposalWork::ISSUE, bound)) {
				return drain_trace.result(ERR_BUSY, "rd.return.28");
			}
			const uint32_t slot = cursor.slot;
			const auto command = slot ? pool.buffers[slot - 1] : f.command_buffer;
			const bool last = slot + 1 == cursor.slot_count;
			const auto signal = last ? RDD::SemaphoreID() : pool.semaphores[slot];
			const auto fence = last ? f.fence : RDD::FenceID();
			const VectorView<RDD::SemaphoreID> waits = slot ? VectorView<RDD::SemaphoreID>(pool.semaphores[slot - 1]) : VectorView<RDD::SemaphoreID>(f.semaphores_to_wait_on);
			RDD::QueueCallReport report;
			const Error error = driver->command_queue_execute_and_present_observed(main_queue, waits, command, signal ? VectorView<RDD::SemaphoreID>(signal) : VectorView<RDD::SemaphoreID>(), fence, {}, report);
			if (error == ERR_BUSY && report.calls.is_empty()) {
				return drain_trace.result(ERR_BUSY, "rd.qualification.source_busy");
			}
			f.chain_reports.push_back(report);
			++cursor.slot; // This exact slot was attempted; never submit it again.
			f.qualification_issue_slot = cursor.slot;
			_observe_buffer_readback_issue(slot, command, error, report);
			const Error height_issue = _native_height_issue_slot(frame, f.qualification_epoch, slot, command, error, report);
			if (height_issue != OK && height_issue != ERR_BUSY) {
				return drain_trace.result(_fail_frame_issue(height_issue), "rd.height.issue_failed");
			}
			_qualification_issue_tickets(slot, report, error);
			if (report.native_issue_count > bound) {
				r_usage.issues = uint32_t(MIN(uint64_t(UINT32_MAX), uint64_t(r_usage.issues) + report.native_issue_count - bound));
				return drain_trace.result(_fail_frame_issue(ERR_INVALID_DATA), "rd.return.29");
			}
			r_usage.issues -= bound - report.native_issue_count;
			if (error != OK || !report.issued || report.error != OK) {
				return drain_trace.result(_fail_frame_issue(error != OK ? error : (report.error != OK ? report.error : FAILED)), "rd.return.30");
			}
		}
		if (cursor.slot_count) {
			f.fence_signaled = true;
			f.semaphores_to_wait_on.clear();
			pool.buffers_used = 0;
			// A frame that was idle and pre-observed can now have new work.
			f.maintenance_wait = {};
		}
		frame = cursor.target;
		qualification_frame_advance_pending = true;
		cursor.phase = 3;
	}
	if (cursor.phase == 3) {
		const Error error = _native_disposal_stall_step(frame);
		if (error != OK) {
			return drain_trace.result(error, "rd.return.31");
		}
		cursor.phase = 4;
	}
	if (cursor.phase == 4) {
		const Error error = _free_pending_resources_step(frame, p_budget, r_usage);
		if (error != OK) {
			return drain_trace.result(error, "rd.return.32");
		}
		cursor.phase = 5;
	}
	if (cursor.phase == 5) {
		if (!next_qualification_frame_epoch) {
			return drain_trace.result(ERR_PARAMETER_RANGE_ERROR, "rd.return.33");
		}
		auto &f = frames[frame];
		if (cursor.begin_phase == 0) {
			if (command_pool_reset_enabled) {
				if (!_native_disposal_admit(NativeDisposalWork::WORK)) {
					return drain_trace.result(ERR_BUSY, "rd.return.34");
				}
				if (!driver->command_pool_reset(f.command_pool)) {
					return drain_trace.result(_fail_frame_issue(FAILED), "rd.return.35");
				}
			}
			f.primary_command_buffer_begun = false;
			// A bounded return opens the same fresh recording epoch as _begin_frame.
			// Previous graph/segment/slot progress belongs to the returned epoch.
			f.qualification_graph_closed = false;
			f.qualification_segment_closed = false;
			f.qualification_issue_busy = false;
			f.qualification_issue_slot = 0;
			f.present_attempted = false;
			f.present_report = {};
			cursor.begin_phase = 1;
		}
		if (cursor.begin_phase == 1) {
			if (!_native_disposal_admit(NativeDisposalWork::WORK)) {
				return drain_trace.result(ERR_BUSY, "rd.return.36");
			}
			driver->begin_segment(frame, frames_drawn++);
			cursor.begin_phase = 2;
		}
		if (cursor.begin_phase == 2) {
			if (!_native_disposal_admit(NativeDisposalWork::WORK)) {
				return drain_trace.result(ERR_BUSY, "rd.return.37");
			}
			f.primary_command_buffer_begun = driver->command_buffer_begin(f.command_buffer);
			if (!f.primary_command_buffer_begun) {
				return drain_trace.result(_fail_frame_issue(FAILED), "rd.return.38");
			}
			cursor.begin_phase = 3;
		}
		if (cursor.begin_phase == 3) {
			if (!_native_disposal_admit(NativeDisposalWork::WORK)) {
				return drain_trace.result(ERR_BUSY, "rd.return.39");
			}
			draw_graph.begin(true);
			f.native_height_ranges.clear();
			f.qualification_epoch = next_qualification_frame_epoch++;
			if (upload_staging_buffers.used) {
				upload_staging_buffers.current = (upload_staging_buffers.current + 1) % upload_staging_buffers.blocks.size();
				upload_staging_buffers.used = false;
			}
			if (download_staging_buffers.used) {
				download_staging_buffers.current = (download_staging_buffers.current + 1) % download_staging_buffers.blocks.size();
				download_staging_buffers.used = false;
			}
			cursor.begin_phase = 4;
		}
		if (cursor.begin_phase == 4) {
			if (f.timestamp_count) {
				if (!_native_disposal_admit(NativeDisposalWork::WORK)) {
					return drain_trace.result(ERR_BUSY, "rd.return.40");
				}
				driver->timestamp_query_pool_get_results(f.timestamp_pool, f.timestamp_count, f.timestamp_result_values.ptr());
			}
			cursor.begin_phase = 5;
		}
		if (cursor.begin_phase == 5) {
			if (f.timestamp_count) {
				if (!_native_disposal_admit(NativeDisposalWork::WORK)) {
					return drain_trace.result(ERR_BUSY, "rd.return.41");
				}
				driver->command_timestamp_query_pool_reset(f.command_buffer, f.timestamp_pool, f.timestamp_count);
				SWAP(f.timestamp_names, f.timestamp_result_names);
				SWAP(f.timestamp_cpu_values, f.timestamp_cpu_result_values);
			}
			f.timestamp_result_count = f.timestamp_count;
			f.timestamp_count = 0;
			f.index = Engine::get_singleton()->get_frames_drawn();
			f.maintenance_wait = {};
			frame_wait_error = OK;
			failed_wait_frame = -1;
			qualification_frame_advance_pending = false;
			native_disposal_active = false;
			cursor = {};
		}
	}
	return drain_trace.result(OK, "rd.return.42");
}

Error RenderingDevice::_native_disposal_stall_step(uint32_t p_frame) {
	const Error height_retain = _native_height_retain_frame_sources(p_frame);
	if (height_retain != OK) {
		return height_retain;
	}
	if (frame_issue_error != OK) {
		return frame_issue_error;
	}
	if (frame_wait_draining) {
		return ERR_BUSY;
	}
	auto &f = frames[p_frame];
	auto &cursor = f.maintenance_wait;
	const Error retain_error = _production_retain_frame_sources(p_frame);
	if (retain_error != OK) {
		return retain_error;
	}
	// Qualification owns its own publication/return path. The production caller
	// proves that no such ticket or foreign callback shares this frozen frame.
	if (!f.qualification_tickets.is_empty() || !f.download_texture_get_data_requests.is_empty()) {
		return ERR_BUSY;
	}
	for (const auto &request : f.download_buffer_get_data_requests) {
		if (!request.capture && (request.result.is_null() || !request.result->qualification_owned)) {
			return ERR_BUSY;
		}
	}
	auto observed = [&](const RDD::FencePollResult &p_result, uint64_t p_serial) {
		if (p_result.native_error) {
			frame_wait_native_code = p_result.native_code;
			frame_wait_native_code_valid = true;
			return FAILED;
		}
		return p_result.matches(p_serial) ? OK : ERR_BUSY;
	};
	if (cursor.phase == 0) {
		while (cursor.report < f.chain_reports.size()) {
			auto &report = f.chain_reports[cursor.report];
			while (cursor.call < report.calls.size()) {
				auto &call = report.calls.write[cursor.call];
				if (!call.presentation_only && !call.source_owner_returned) {
					if (!_native_disposal_admit(NativeDisposalWork::POLL)) {
						return ERR_BUSY;
					}
					const Error error = observed(driver->native_call_poll(call.serial), call.completion_serial);
					if (error != OK) {
						return error;
					}
				}
				++cursor.call;
			}
			cursor.call = 0;
			++cursor.report;
		}
		cursor.phase = 1;
		cursor.report = 0;
	}
	if (cursor.phase == 1) {
		while (cursor.report < f.chain_reports.size()) {
			auto &report = f.chain_reports[cursor.report];
			while (cursor.call < report.calls.size()) {
				auto &call = report.calls.write[cursor.call];
				if (!report.submission_serial && !call.presentation_only && !call.source_owner_returned) {
					if (!_native_disposal_admit(NativeDisposalWork::RECYCLE)) {
						return ERR_BUSY;
					}
					const Error error = driver->native_call_recycle(call.serial);
					if (error != OK) {
						return error;
					}
					call.source_owner_returned = true;
				}
				++cursor.call;
			}
			cursor.call = 0;
			++cursor.report;
		}
		cursor.phase = 2;
		cursor.report = 0;
	}
	if (cursor.phase == 2) {
		while (cursor.report < f.qualification_hidden_reports.size()) {
			auto &report = f.qualification_hidden_reports[cursor.report];
			while (cursor.call < report.calls.size()) {
				auto &call = report.calls.write[cursor.call];
				if (!call.presentation_only && !call.source_owner_returned) {
					if (!cursor.ready) {
						if (!_native_disposal_admit(NativeDisposalWork::POLL)) {
							return ERR_BUSY;
						}
						const Error error = observed(driver->native_call_poll(call.serial), call.completion_serial);
						if (error != OK) {
							return error;
						}
						cursor.ready = true;
					}
					if (call.frame_observer_owned) {
						if (!_native_disposal_admit(NativeDisposalWork::RELEASE)) {
							return ERR_BUSY;
						}
						const Error error = driver->native_call_release(call.serial);
						if (error != OK) {
							return error;
						}
					}
					call.source_owner_returned = true;
				}
				cursor.ready = false;
				++cursor.call;
			}
			cursor.call = 0;
			++cursor.report;
		}
		f.qualification_hidden_reports.clear();
		cursor.phase = 3;
		cursor.report = 0;
	}
	if (cursor.phase == 3) {
		if (f.fence_signaled) {
			if (f.chain_reports.is_empty() || !f.chain_reports[f.chain_reports.size() - 1].submission_serial) {
				return ERR_UNAVAILABLE;
			}
			const uint64_t serial = f.chain_reports[f.chain_reports.size() - 1].submission_serial;
			if (!cursor.ready) {
				if (!_native_disposal_admit(NativeDisposalWork::POLL)) {
					return ERR_BUSY;
				}
				const Error error = observed(driver->fence_poll(f.fence, serial), serial);
				if (error != OK) {
					return error;
				}
				cursor.ready = true;
			}
			if (!_native_disposal_admit(NativeDisposalWork::RECYCLE)) {
				return ERR_BUSY;
			}
			const Error error = driver->fence_recycle(f.fence, serial);
			if (error != OK) {
				return error;
			}
			f.fence_signaled = false;
			cursor.fence_returned = true;
			f.primary_command_buffer_begun = false;
		}
		cursor.ready = false;
		cursor.phase = 4;
	}
	// Readback mapping stays on the original frame's requests and staging. A
	// deadline between map/unmap preserves the mapping and blocks frame reuse.
	if (cursor.phase == 4) {
		if (!cursor.fence_returned) {
			// No fence return means these requests can still be recorded work in
			// this slot (in particular a one-frame RD). Do not deliver or clear it.
			cursor.phase = 5;
			return OK;
		}
		while (cursor.request < f.download_buffer_get_data_requests.size()) {
			const auto &request = f.download_buffer_get_data_requests[cursor.request];
			const auto result = request.result;
			auto *capture = request.capture;
			if (capture && (!capture->registry || capture->registry->production_readback_delivery(*capture, true) != OK)) {
				return ERR_BUSY;
			}
			struct DeliveryReturn {
				GDExtensionDeviceExit::BufferReadbackSourceCapture *capture;
				~DeliveryReturn() {
					if (capture) {
						capture->registry->production_readback_delivery(*capture, false);
					}
				}
			} delivery_return{ capture };
			// A callback reentering frame advancement cannot disturb these cursors.
			frame_wait_draining = true;
			struct DrainReturn {
				std::atomic<bool> &draining;
				~DrainReturn() { draining = false; }
			} drain_return{ frame_wait_draining };
			Error &error = capture ? capture->state->error : result->error;
			bool &delivered = capture ? capture->state->delivered : result->delivered;
			const uint64_t capacity = capture ? capture->byte_capacity : uint64_t(result->bytes.size());
			uint8_t *bytes = capture ? capture->bytes : result->bytes.ptrw();
			const uint64_t completed = capture ? capture->state->completed : result->completed_size;
			if (!cursor.request_started) {
				if (capture) {
					if (capture->state->returned > capture->state->issued || request.result_offset != capture->state->returned || request.size > capture->state->issued - capture->state->returned) {
						error = ERR_INVALID_DATA;
					}
				}
				if (request.result_offset != completed || request.result_offset > capacity || request.size > capacity - request.result_offset) {
					error = ERR_INVALID_DATA;
				}
				cursor.request_started = true;
			}
			while ((error == OK && !delivered && cursor.region < request.frame_local_count) || cursor.mapped) {
				const uint32_t local = request.frame_local_index + cursor.region;
				const auto &region = f.download_buffer_copy_regions[local];
				if (!cursor.mapped) {
					if (cursor.offset > request.size || region.size > request.size - cursor.offset) {
						error = ERR_INVALID_DATA;
						break;
					}
					if (!_native_disposal_admit(NativeDisposalWork::WORK)) {
						return ERR_BUSY;
					}
					int64_t native_code = 0;
					cursor.mapped = driver->buffer_map_observed(f.download_buffer_staging_buffers[local], native_code);
					if (!cursor.mapped) {
						error = ERR_CANT_ACQUIRE_RESOURCE;
						if (capture) {
							capture->native_code = native_code;
						} else {
							result->native_code = native_code;
						}
						break;
					}
					memcpy(bytes + request.result_offset + cursor.offset, cursor.mapped + region.dst_offset, region.size);
				}
				if (!_native_disposal_admit(NativeDisposalWork::WORK)) {
					return ERR_BUSY;
				}
				driver->buffer_unmap(f.download_buffer_staging_buffers[local]);
				cursor.mapped = nullptr;
				cursor.offset += region.size;
				++cursor.region;
			}
			if (error == OK && !delivered) {
				if (cursor.offset != request.size) {
					error = ERR_INVALID_DATA;
				} else {
					const uint64_t total = completed + cursor.offset;
					if (capture) {
						capture->state->completed = total;
					} else {
						result->completed_size = uint32_t(total);
					}
					if (request.final_segment) {
						if (total != capacity) {
							error = ERR_INVALID_DATA;
						} else {
							delivered = true;
						}
					}
				}
			}
			cursor.region = 0;
			cursor.offset = 0;
			cursor.request_started = false;
			if (capture) {
				// The original frame no longer owns a mapped region or any
				// resumable work for this raw capture entry, including errors.
				if (capture->state->returned <= capture->state->issued && request.size <= capture->state->issued - capture->state->returned) {
					capture->state->returned += request.size;
				}
			}
			++cursor.request;
		}
		f.download_buffer_get_data_requests.clear();
		f.download_buffer_copy_regions.clear();
		f.download_buffer_staging_buffers.clear();
		for (auto &report : f.chain_reports) {
			for (auto &call : report.calls) {
				if (!call.presentation_only) {
					call.source_owner_returned = true;
				}
			}
		}
		cursor.phase = 5;
	}
	return OK;
}

Error RenderingDevice::_native_disposal_maintenance() {
	if (native_disposal_active) {
		const NativeDisposalBudget budget{ UINT32_MAX, UINT32_MAX, UINT64_MAX };
		NativeDisposalUsage usage;
		return _native_disposal_maintenance_step(budget, usage);
	}
	if (render_thread_id != Thread::get_caller_id() || !is_main_instance || !driver || native_finalization_returned) {
		return ERR_UNAUTHORIZED;
	}
	if (frames.is_empty() || uint32_t(frame) >= frames.size()) {
		return ERR_UNCONFIGURED;
	}
	if (draw_list.active || compute_list.active || raytracing_list.active || frame_wait_draining) {
		return ERR_BUSY;
	}
	if (!_thread_safe_.try_lock()) {
		return ERR_BUSY;
	}
	struct Unlock {
		Mutex &mutex;
		~Unlock() { mutex.unlock(); }
	} unlock{ _thread_safe_ };
	const Error cleanup_credits = _qualification_activate_cleanup_credits();
	if (cleanup_credits != OK) {
		return cleanup_credits;
	}
	if (qualification_frame_advance_pending) {
		return _begin_frame(false, true);
	}
	if (draw_graph.has_driver_callbacks()) {
		return ERR_BUSY;
	}
	const uint32_t next = (frame + 1) % frames.size();
	Error error = _stall_for_frame(next, true);
	if (error != OK) {
		return error;
	}
	for (const auto &shader : frames[next].shaders_to_dispose_of) {
		if (!driver->shader_retire_ready_nonblocking(shader.driver_id)) {
			return ERR_BUSY;
		}
	}
	error = _end_frame(true);
	if (error != OK) {
		return error;
	}
	error = _execute_frame(false);
	if (error != OK) {
		return error;
	}
	frame = (frame + 1) % frames.size();
	qualification_frame_advance_pending = true;
	return _begin_frame(false, true);
}

bool RenderingDevice::_qualification_free_event_available(bool p_reserved) const {
	if (!next_qualification_free_event) {
		return false;
	}
	return p_reserved ? qualification_free_event_reservations != 0 : qualification_free_event_reservations <= UINT64_MAX - next_qualification_free_event;
}

bool RenderingDevice::_qualification_reserve_free_event(GDExtensionDeviceExit::QualificationPhysicalReturn *cell) {
	if (!cell || cell->event_capacity_reserved || !_qualification_free_event_available(false)) {
		return false;
	}
	++qualification_free_event_reservations;
	cell->event_capacity_reserved = true;
	return true;
}

void RenderingDevice::_qualification_release_free_event(GDExtensionDeviceExit::QualificationPhysicalReturn *cell) {
	CRASH_COND(!cell || !cell->event_capacity_reserved || !qualification_free_event_reservations);
	--qualification_free_event_reservations;
	cell->event_capacity_reserved = false;
}

Error RenderingDevice::_qualification_enqueue(GDExtensionDeviceExit::QualificationPhysicalReturn *cell) {
	if (!cell) {
		return OK;
	}
	if (!qualification_retiring || !frames[frame].qualification_epoch) {
		return ERR_BUSY;
	}
	MutexLock lock(cell->mutex);
	if (!cell->use_closed || cell->enqueued || cell->free_event) {
		return ERR_BUSY;
	}
	if (!_qualification_free_event_available(cell->event_capacity_reserved)) {
		return ERR_BUSY;
	}
	if (cell->event_capacity_reserved) {
		--qualification_free_event_reservations;
		cell->event_capacity_reserved = false;
	}
	cell->reserved_event = next_qualification_free_event++;
	cell->frame_slot = frame;
	cell->frame_epoch = frames[frame].qualification_epoch;
	cell->enqueued = true;
	return OK;
}
void RenderingDevice::_qualification_freed(GDExtensionDeviceExit::QualificationPhysicalReturn *cell) {
	if (!cell) {
		return;
	}
	MutexLock lock(cell->mutex);
	CRASH_COND(!cell->enqueued || !cell->reserved_event || cell->free_event);
	cell->free_event = cell->reserved_event;
}
RID RenderingDevice::_qualification_storage_create(uint32_t bytes, GDExtensionDeviceExit::QualificationPhysicalReturn *cell) {
	ERR_FAIL_COND_V(!cell || cell->event_capacity_reserved || !bytes || _recording_error() != OK, RID());
#ifdef TESTS_ENABLED
	if (is_main_instance && !test_main_screen_prepared && OS::get_singleton()->has_environment("GODOT_K03_TEST_PREPARE_MAIN_SCREEN")) {
		ERR_FAIL_COND_V_MSG(OS::get_singleton()->get_environment("GODOT_K03_TEST_PREPARE_MAIN_SCREEN") != "1", RID(), "Invalid TESTS-only main screen preparation opt-in.");
		for (const auto &entry : frames) {
			ERR_FAIL_COND_V(!entry.qualification_tickets.is_empty(), RID());
		}
		ERR_FAIL_COND_V(!frames[frame].swap_chains_to_present.is_empty(), RID());
		// Ticket-preparation fixture only. Boot presentation has finished; reacquire
		// through the original path before any backing or ticket effect. Resize,
		// flush and acquire here are not nonwaiting qualification maintenance.
		ERR_FAIL_COND_V(screen_prepare_for_drawing(DisplayServerEnums::MAIN_WINDOW_ID) != OK, RID());
		test_main_screen_epoch = frames[frame].qualification_epoch;
		test_main_screen_prepared = true;
		print_line(vformat("K03_TEST_MAIN_SCREEN_PREPARED phase=before_first_ticket frame_epoch=%d real_acquire=1 present_called=0", test_main_screen_epoch));
	}
#endif
	const uint64_t serial = _reserve_backing_serial();
	ERR_FAIL_COND_V(!serial, RID());
	const RID rid = storage_buffer_owner.allocate_rid();
	ERR_FAIL_COND_V(rid.is_null(), RID());
	if (!_qualification_reserve_free_event(cell)) {
		storage_buffer_owner.initialize_rid(rid, Buffer());
		storage_buffer_owner.free(rid);
		return RID();
	}
	Buffer buffer;
	buffer.backing_serial = serial;
	buffer.size = bytes;
	buffer.usage = RDD::BUFFER_USAGE_TRANSFER_FROM_BIT | RDD::BUFFER_USAGE_TRANSFER_TO_BIT | RDD::BUFFER_USAGE_STORAGE_BIT;
	buffer.qualification_return = cell;
	buffer.driver_id = driver->buffer_create(bytes, buffer.usage, RDD::MEMORY_ALLOCATION_TYPE_GPU, frames_drawn);
	if (!buffer.driver_id) {
		_qualification_release_free_event(cell);
		// RID_Alloc cannot free an uninitialized reservation.
		storage_buffer_owner.initialize_rid(rid, Buffer());
		storage_buffer_owner.free(rid);
		return RID();
	}
	buffer.draw_tracker = RDG::resource_tracker_create();
	buffer.draw_tracker->buffer_driver_id = buffer.driver_id;
	cell->rid = rid.get_id();
	cell->serial = buffer.backing_serial;
	storage_buffer_owner.initialize_rid(rid, buffer);
	buffer_memory += bytes;
	return rid;
}

Error RenderingDevice::_qualification_prepare_free(RID rid) {
	// Keep the original RID and dependency edges until every transfer on the
	// recursive free can join safely. No disposal cell or driver object moves
	// to a returned/enqueued state on a refused reservation.
	LocalVector<RID> pending, seen;
	pending.push_back(rid);
	while (!pending.is_empty()) {
		const RID current = pending[pending.size() - 1];
		pending.remove_at(pending.size() - 1);
		bool known = false;
		for (RID old : seen) {
			if (old == current) {
				known = true;
				break;
			}
		}
		if (known) {
			continue;
		}
		seen.push_back(current);
		if (auto edge = dependency_map.find(current)) {
			for (RID dependent : edge->value) {
				pending.push_back(dependent);
			}
		}
		Error error = OK;
		if (texture_owner.owns(current)) {
			auto *texture = texture_owner.get_or_null(current);
			if (texture) {
				error = _check_transfer_worker_texture(texture);
			}
		} else {
			Buffer *buffer = nullptr;
			if (storage_buffer_owner.owns(current)) {
				buffer = storage_buffer_owner.get_or_null(current);
			} else if (vertex_buffer_owner.owns(current)) {
				buffer = vertex_buffer_owner.get_or_null(current);
			} else if (index_buffer_owner.owns(current)) {
				buffer = index_buffer_owner.get_or_null(current);
			} else if (uniform_buffer_owner.owns(current)) {
				buffer = uniform_buffer_owner.get_or_null(current);
			} else if (texture_buffer_owner.owns(current)) {
				buffer = texture_buffer_owner.get_or_null(current);
			}
			if (buffer) {
				error = _check_transfer_worker_buffer(buffer);
			}
		}
		if (error != OK) {
			return error;
		}
	}
	return OK;
}
void RenderingDevice::_qualification_defer_free(RID rid) {
	for (RID pending : qualification_deferred_frees) {
		if (pending == rid) {
			return;
		}
	}
	qualification_deferred_frees.push_back(rid);
}

void RenderingDevice::_qualification_retry_deferred_frees() {
	// Snapshot the finite roots for this pass, but leave custody in the original
	// queue across every void free_rid early return. Only actual owner removal
	// in free_rid may erase a root; callbacks can append work for the next pass.
	const LocalVector<RID> pending(qualification_deferred_frees);
	for (RID rid : pending) {
		bool still_pending = false;
		for (RID queued : qualification_deferred_frees) {
			if (queued == rid) {
				still_pending = true;
				break;
			}
		}
		if (still_pending) {
			free_rid(rid);
		}
	}
}

void RenderingDevice::free_rid(RID p_rid) {
	ERR_RENDER_THREAD_GUARD();
	MutexLock immutable_lock(_thread_safe_);
	if (production_disposal_drop_owner) {
		// Keep the current callback proof under the same lock that closes
		// retirement; an ordinary setter must not replace it in between.
		if (!_production_disposal_belongs(p_rid, production_disposal_drop_owner) || !_production_output_callback_supported(p_rid)) {
			return;
		}
	}

	if (resource_frees_in_progress.has(p_rid)) {
		return;
	}
	resource_frees_in_progress.insert(p_rid);
	struct ResourceFreeScope {
		RenderingDevice &device;
		const MutexLock<Mutex> &lock;
		NativeImmutableFreeFrame frame;
		RID rid;
		ResourceFreeScope(RenderingDevice &p_device, RID p_rid, const MutexLock<Mutex> &p_lock) : device(p_device), lock(p_lock), rid(p_rid) {
			frame.previous = device.native_immutable_free_frame;
			device.native_immutable_free_frame = &frame;
		}
		~ResourceFreeScope() {
			device.resource_frees_in_progress.erase(rid);
			if (frame.previous) {
				device.native_immutable_free_frame = frame.previous;
				for (RID pending : frame.returned) {
					if (!frame.previous->returned.has(pending)) {
						frame.previous->returned.push_back(pending);
					}
				}
				return;
			}
			// Callback returns belong to this original synchronous free frame.
			// Once its recursion guard closes, only original pending frees resume.
			// Keep the actual outer stack frame visible until all synchronous
			// retries return, including intervals with no RID recursion guard.
			for (uint32_t i = 0; i < frame.returned.size(); ++i) {
				const RID pending = frame.returned[i];
				bool *retiring = nullptr;
				const auto *holds = device._native_immutable_counter(pending, &retiring);
				if (holds && !*holds && *retiring) {
					lock.temp_unlock();
					device.free_rid(pending);
					lock.temp_relock();
				}
			}
			device.native_immutable_free_frame = nullptr;
		}
	} free_scope{ *this, p_rid, immutable_lock };

	// Cross-owner probes must reject foreign validators before inspecting a
	// possibly uninitialized slot reserved by asynchronous native creation.
	auto *buffer = storage_buffer_owner.owns(p_rid) ? storage_buffer_owner.get_or_null(p_rid) : nullptr;
	auto *shader = shader_owner.owns(p_rid) ? shader_owner.get_or_null(p_rid) : nullptr;
	auto *pipeline = compute_pipeline_owner.owns(p_rid) ? compute_pipeline_owner.get_or_null(p_rid) : nullptr;
	auto *texture = texture_owner.owns(p_rid) ? texture_owner.get_or_null(p_rid) : nullptr;
	auto *sampler = sampler_owner.owns(p_rid) ? sampler_owner.get_or_null(p_rid) : nullptr;
	auto *raster_pipeline = render_pipeline_owner.owns(p_rid) ? render_pipeline_owner.get_or_null(p_rid) : nullptr;
	auto *framebuffer = framebuffer_owner.owns(p_rid) ? framebuffer_owner.get_or_null(p_rid) : nullptr;
	auto *uniform_set = uniform_set_owner.owns(p_rid) ? uniform_set_owner.get_or_null(p_rid) : nullptr;
	ERR_FAIL_COND_MSG(!qualification_retiring && ((raster_pipeline && raster_pipeline->qualification_return) || (framebuffer && framebuffer->qualification_return) || (buffer && buffer->qualification_return) || (shader && shader->qualification_return) || (pipeline && pipeline->qualification_return) || (texture && texture->qualification_return) || (sampler && sampler->qualification_return) || (uniform_set && uniform_set->qualification_return)), "Qualification resources require their original owner retirement acknowledgment.");
	ERR_FAIL_COND_MSG(texture && texture->qualification_return && texture->transfer_worker_index >= 0, "Qualification texture has an unsettled transfer worker.");
	ERR_FAIL_COND_MSG(_qualification_resource_busy(p_rid), "Qualification ticket still owns this resource or one of its dependents.");
	bool *retiring = nullptr;
	if (auto *holds = _native_immutable_counter(p_rid, &retiring)) {
		// Close future physical acquisition before dependency callbacks can run.
		*retiring = true;
		if (*holds) {
			immutable_lock.temp_unlock();
			_native_immutable_retire_dependents(p_rid);
			immutable_lock.temp_relock();
			// A callback may return the last hold or delete a dependent. Re-resolve
			// the actual owner; never dereference its pre-callback pointer again.
			const auto *remaining = _native_immutable_counter(p_rid);
			if (!remaining || *remaining) {
				return;
			}
		}
	}

	const Error dependency_admission = _qualification_prepare_free(p_rid);
	if (dependency_admission != OK) {
		_qualification_defer_free(p_rid);
		return;
	}
	if (!_free_dependencies(p_rid, immutable_lock)) { // Keep the original guard through dependency removal and RID free.
		// A callback can return the last pin, then destroy an owner whose
		// dependency is this outer free. Resume that original zero-pin free
		// after the outer recursion guard closes.
		_qualification_defer_free(p_rid);
		const auto *remaining = _native_immutable_counter(p_rid, &retiring);
		if (free_scope.frame.previous && remaining && !*remaining && *retiring && !free_scope.frame.returned.has(p_rid)) {
			free_scope.frame.returned.push_back(p_rid);
		}
		return;
	}
	_free_internal(p_rid, immutable_lock);
	// Remove a deferred root only after its original RID owner actually freed it.
	if (!uniform_buffer_owner.owns(p_rid) &&
			!storage_buffer_owner.owns(p_rid) &&
			!texture_buffer_owner.owns(p_rid) &&
			!texture_owner.owns(p_rid) &&
			!framebuffer_owner.owns(p_rid) &&
			!sampler_owner.owns(p_rid) &&
			!vertex_buffer_owner.owns(p_rid) &&
			!vertex_array_owner.owns(p_rid) &&
			!index_buffer_owner.owns(p_rid) &&
			!index_array_owner.owns(p_rid) &&
			!shader_owner.owns(p_rid) &&
			!uniform_set_owner.owns(p_rid) &&
			!render_pipeline_owner.owns(p_rid) &&
			!compute_pipeline_owner.owns(p_rid) &&
			!raytracing_pipeline_owner.owns(p_rid) &&
			!acceleration_structure_owner.owns(p_rid) &&
			!hit_sbt_owner.owns(p_rid)) {
		for (uint32_t i = 0; i < qualification_deferred_frees.size(); ++i) {
			if (qualification_deferred_frees[i] == p_rid) {
				qualification_deferred_frees.remove_at(i);
				break;
			}
		}
	}
}

#ifdef TESTS_ENABLED
thread_local void (*test_uniform_set_after_rid_free)(RenderingDevice *) = nullptr;
#endif

void RenderingDevice::_free_internal(RID p_id, const MutexLock<Mutex> &p_lock) {
	ERR_FAIL_COND(_qualification_resource_busy(p_id));
	auto *qualified_texture = texture_owner.owns(p_id) ? texture_owner.get_or_null(p_id) : nullptr;
	ERR_FAIL_COND(qualified_texture && qualified_texture->qualification_return && qualified_texture->transfer_worker_index >= 0);
#ifdef DEV_ENABLED
	String resource_name;
	if (resource_names.has(p_id)) {
		resource_name = resource_names[p_id];
		resource_names.erase(p_id);
	}
#endif

	// Push everything so it's disposed of next time this frame index is processed (means, it's safe to do it).
	if (texture_owner.owns(p_id)) {
		Texture *texture = texture_owner.get_or_null(p_id);
		{
			const Error dependency_error = _check_transfer_worker_texture(texture);
			if (dependency_error != OK) {
				return;
			}
		}
		ERR_FAIL_COND(_qualification_enqueue(texture->qualification_return) != OK);
		ERR_FAIL_COND(!_native_disposal_enqueue(texture->native_disposal, p_id.get_id(), texture->native_disposal_serial, texture->driver_id.id));

		RDG::ResourceTracker *draw_tracker = texture->draw_tracker;
		if (draw_tracker != nullptr) {
			draw_tracker->reference_count--;
			if (draw_tracker->reference_count == 0) {
				RDG::resource_tracker_free(draw_tracker);

				if (texture->owner.is_valid() && (texture->slice_type != TEXTURE_SLICE_MAX)) {
					// If this was a texture slice, erase the tracker from the map.
					Texture *owner_texture = texture_owner.get_or_null(texture->owner);
					if (owner_texture != nullptr && owner_texture->slice_trackers != nullptr) {
						owner_texture->slice_trackers->erase(texture->slice_rect);

						if (owner_texture->slice_trackers->is_empty()) {
							memdelete(owner_texture->slice_trackers);
							owner_texture->slice_trackers = nullptr;
						}
					}
				}
			}
		}

		_production_disposal_enqueued(texture->production_disposal_uses);
		frames[frame].textures_to_dispose_of.push_back(*texture);
		texture_owner.free(p_id);
	} else if (framebuffer_owner.owns(p_id)) {
		Framebuffer *framebuffer = framebuffer_owner.get_or_null(p_id);
		ERR_FAIL_COND(_qualification_enqueue(framebuffer->qualification_return) != OK);
		ERR_FAIL_COND(!_native_disposal_enqueue(framebuffer->native_disposal, p_id.get_id(), framebuffer->native_disposal_serial, uint64_t(uintptr_t(framebuffer->framebuffer_cache))));
		InvalidationCallback invalidated_callback = framebuffer->invalidated_callback;
		void *invalidated_callback_userdata = framebuffer->invalidated_callback_userdata;
		frames[frame].framebuffers_to_dispose_of.push_back(*framebuffer);
		framebuffer_owner.free(p_id);

		if (invalidated_callback != nullptr) {
			p_lock.temp_unlock();
			invalidated_callback(invalidated_callback_userdata);
			p_lock.temp_relock();
		}
	} else if (sampler_owner.owns(p_id)) {
		Sampler *sampler = sampler_owner.get_or_null(p_id);
		ERR_FAIL_COND(_qualification_enqueue(sampler->qualification_return) != OK);
		ERR_FAIL_COND(!_native_disposal_enqueue(sampler->native_disposal, p_id.get_id(), sampler->native_disposal_serial, sampler->driver_id.id));
		_production_disposal_enqueued(sampler->production_disposal_uses);
		frames[frame].samplers_to_dispose_of.push_back(*sampler);
		sampler_owner.free(p_id);
	} else if (vertex_buffer_owner.owns(p_id)) {
		Buffer *vertex_buffer = vertex_buffer_owner.get_or_null(p_id);
		{
			const Error dependency_error = _check_transfer_worker_buffer(vertex_buffer);
			if (dependency_error != OK) {
				return;
			}
		}

		RDG::resource_tracker_free(vertex_buffer->draw_tracker);
		_production_disposal_enqueued(vertex_buffer->production_disposal_uses);
		frames[frame].buffers_to_dispose_of.push_back(*vertex_buffer);
#if defined(TESTS_ENABLED) && (defined(MODULE_SKINNED_MESH_CAPTURE_TESTS_ENABLED) || defined(MODULE_PARTICLES_RUNTIME_TESTS_ENABLED))
		if (vertex_buffer->capture_test_allocation_serial) {
			if (!capture_test_allocation_registry.release_owner(vertex_buffer->driver_id.id, vertex_buffer->capture_test_allocation_serial, p_id.get_id())) {
				capture_test_coverage_failed.store(true);
			}
		} else {
			capture_test_coverage_failed.store(true);
		}
#if defined(TESTS_ENABLED) && defined(MODULE_SKINNED_MESH_CAPTURE_TESTS_ENABLED)
		RenderCaptureTestHooks::observe_buffer_free(p_id.get_id(), uint64_t(get_instance_id()), vertex_buffer->driver_id.id, vertex_buffer->capture_test_allocation_serial);
#endif
#endif
		vertex_buffer_owner.free(p_id);
	} else if (vertex_array_owner.owns(p_id)) {
		auto *array = vertex_array_owner.get_or_null(p_id);
		_production_disposal_enqueued(array->production_disposal_uses);
		_production_disposal_returned(array->production_disposal_uses);
		vertex_array_owner.free(p_id);
	} else if (index_buffer_owner.owns(p_id)) {
		IndexBuffer *index_buffer = index_buffer_owner.get_or_null(p_id);
		{
			const Error dependency_error = _check_transfer_worker_buffer(index_buffer);
			if (dependency_error != OK) {
				return;
			}
		}

		RDG::resource_tracker_free(index_buffer->draw_tracker);
		_production_disposal_enqueued(index_buffer->production_disposal_uses);
		frames[frame].buffers_to_dispose_of.push_back(*index_buffer);
		index_buffer_owner.free(p_id);
	} else if (index_array_owner.owns(p_id)) {
		auto *array = index_array_owner.get_or_null(p_id);
		_production_disposal_enqueued(array->production_disposal_uses);
		_production_disposal_returned(array->production_disposal_uses);
		index_array_owner.free(p_id);
	} else if (shader_owner.owns(p_id)) {
		Shader *shader = shader_owner.get_or_null(p_id);
		if (shader->qualification_return && native_disposal_budget && !_native_disposal_admit(NativeDisposalWork::POLL)) {
			return;
		}
		ERR_FAIL_COND(shader->qualification_return && !driver->shader_retire_ready_nonblocking(shader->driver_id));
		ERR_FAIL_COND(_qualification_enqueue(shader->qualification_return) != OK);
		if (shader->driver_id) { // Not placeholder?
			_production_disposal_enqueued(shader->production_disposal_uses);
			frames[frame].shaders_to_dispose_of.push_back(*shader);
		}
		shader_owner.free(p_id);
	} else if (uniform_buffer_owner.owns(p_id)) {
		Buffer *uniform_buffer = uniform_buffer_owner.get_or_null(p_id);
		{
			const Error dependency_error = _check_transfer_worker_buffer(uniform_buffer);
			if (dependency_error != OK) {
				return;
			}
		}

		RDG::resource_tracker_free(uniform_buffer->draw_tracker);
		_production_disposal_enqueued(uniform_buffer->production_disposal_uses);
		frames[frame].buffers_to_dispose_of.push_back(*uniform_buffer);
		uniform_buffer_owner.free(p_id);
	} else if (texture_buffer_owner.owns(p_id)) {
		Buffer *texture_buffer = texture_buffer_owner.get_or_null(p_id);
		{
			const Error dependency_error = _check_transfer_worker_buffer(texture_buffer);
			if (dependency_error != OK) {
				return;
			}
		}

		RDG::resource_tracker_free(texture_buffer->draw_tracker);
		_production_disposal_enqueued(texture_buffer->production_disposal_uses);
		frames[frame].buffers_to_dispose_of.push_back(*texture_buffer);
		texture_buffer_owner.free(p_id);
	} else if (storage_buffer_owner.owns(p_id)) {
		Buffer *storage_buffer = storage_buffer_owner.get_or_null(p_id);
		ERR_FAIL_COND(_qualification_enqueue(storage_buffer->qualification_return) != OK);
		ERR_FAIL_COND(!_native_disposal_enqueue(storage_buffer->native_disposal, p_id.get_id(), storage_buffer->backing_serial, storage_buffer->driver_id.id));
		{
			const Error dependency_error = _check_transfer_worker_buffer(storage_buffer);
			if (dependency_error != OK) {
				return;
			}
		}

		RDG::resource_tracker_free(storage_buffer->draw_tracker);
		_production_disposal_enqueued(storage_buffer->production_disposal_uses);
		frames[frame].buffers_to_dispose_of.push_back(*storage_buffer);
#if defined(TESTS_ENABLED) && (defined(MODULE_SKINNED_MESH_CAPTURE_TESTS_ENABLED) || defined(MODULE_PARTICLES_RUNTIME_TESTS_ENABLED))
		if (storage_buffer->capture_test_allocation_serial) {
			if (!capture_test_allocation_registry.release_owner(storage_buffer->driver_id.id, storage_buffer->capture_test_allocation_serial, p_id.get_id())) {
				capture_test_coverage_failed.store(true);
			}
		} else {
			capture_test_coverage_failed.store(true);
		}
#if defined(TESTS_ENABLED) && defined(MODULE_SKINNED_MESH_CAPTURE_TESTS_ENABLED)
		RenderCaptureTestHooks::observe_buffer_free(p_id.get_id(), uint64_t(get_instance_id()), storage_buffer->driver_id.id, storage_buffer->capture_test_allocation_serial);
#endif
#endif
		storage_buffer_owner.free(p_id);
	} else if (uniform_set_owner.owns(p_id)) {
		UniformSet *uniform_set = uniform_set_owner.get_or_null(p_id);
		ERR_FAIL_COND(_qualification_enqueue(uniform_set->qualification_return) != OK);
		ERR_FAIL_COND(!_native_disposal_enqueue(uniform_set->native_disposal, p_id.get_id(), uniform_set->native_disposal_serial, uniform_set->driver_id.id));
		// The RID owner destroys the set before the synchronous callback.
		InvalidationCallback invalidated_callback = uniform_set->invalidated_callback;
		void *invalidated_callback_userdata = uniform_set->invalidated_callback_userdata;
		_production_disposal_enqueued(uniform_set->production_disposal_uses);
		frames[frame].uniform_sets_to_dispose_of.push_back(*uniform_set);
		uniform_set_owner.free(p_id);
#ifdef TESTS_ENABLED
		if (test_uniform_set_after_rid_free) {
			test_uniform_set_after_rid_free(this);
		}
#endif

		if (invalidated_callback != nullptr) {
			p_lock.temp_unlock();
			invalidated_callback(invalidated_callback_userdata);
			p_lock.temp_relock();
		}
	} else if (render_pipeline_owner.owns(p_id)) {
		RenderPipeline *pipeline = render_pipeline_owner.get_or_null(p_id);
		ERR_FAIL_COND(_qualification_enqueue(pipeline->qualification_return) != OK);
		_production_disposal_enqueued(pipeline->production_disposal_uses);
		frames[frame].render_pipelines_to_dispose_of.push_back(*pipeline);
		render_pipeline_owner.free(p_id);
	} else if (compute_pipeline_owner.owns(p_id)) {
		ComputePipeline *pipeline = compute_pipeline_owner.get_or_null(p_id);
		ERR_FAIL_COND(_qualification_enqueue(pipeline->qualification_return) != OK);
		_production_disposal_enqueued(pipeline->production_disposal_uses);
		frames[frame].compute_pipelines_to_dispose_of.push_back(*pipeline);
		compute_pipeline_owner.free(p_id);
	} else if (acceleration_structure_owner.owns(p_id)) {
		AccelerationStructure *acceleration_structure = acceleration_structure_owner.get_or_null(p_id);
		if (acceleration_structure->type == ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL) {
			_blas_remove_tlas_dependencies(acceleration_structure, p_id);
		} else if (acceleration_structure->type == ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL) {
			_tlas_remove_blas_dependencies(acceleration_structure, p_id);
		}
		RDG::resource_tracker_free(acceleration_structure->draw_tracker);
		frames[frame].acceleration_structures_to_dispose_of.push_back(*acceleration_structure);
		acceleration_structure_owner.free(p_id);
	} else if (raytracing_pipeline_owner.owns(p_id)) {
		RaytracingPipeline *pipeline = raytracing_pipeline_owner.get_or_null(p_id);
		frames[frame].raytracing_pipelines_to_dispose_of.push_back(*pipeline);
		raytracing_pipeline_owner.free(p_id);
	} else if (hit_sbt_owner.owns(p_id)) {
		HitShaderBindingTable *hit_sbt = hit_sbt_owner.get_or_null(p_id);
		RDG::resource_tracker_free(hit_sbt->draw_tracker);
		_production_disposal_enqueued(hit_sbt->production_disposal_uses);
		frames[frame].buffers_to_dispose_of.push_back(*hit_sbt);
		hit_sbt_owner.free(p_id);
	} else {
#ifdef DEV_ENABLED
		ERR_PRINT("Attempted to free invalid ID: " + itos(p_id.get_id()) + " " + resource_name);
#else
		ERR_PRINT("Attempted to free invalid ID: " + itos(p_id.get_id()));
#endif
	}

	frames_pending_resources_for_processing = uint32_t(frames.size());
}

// The full list of resources that can be named is in the VkObjectType enum.
// We just expose the resources that are owned and can be accessed easily.
void RenderingDevice::set_resource_name(RID p_id, const String &p_name) {
	_THREAD_SAFE_METHOD_

	if (texture_owner.owns(p_id)) {
		Texture *texture = texture_owner.get_or_null(p_id);
		driver->set_object_name(RDD::OBJECT_TYPE_TEXTURE, texture->driver_id, p_name);
	} else if (framebuffer_owner.owns(p_id)) {
		//Framebuffer *framebuffer = framebuffer_owner.get_or_null(p_id);
		// Not implemented for now as the relationship between Framebuffer and RenderPass is very complex.
	} else if (sampler_owner.owns(p_id)) {
		Sampler *sampler = sampler_owner.get_or_null(p_id);
		driver->set_object_name(RDD::OBJECT_TYPE_SAMPLER, sampler->driver_id, p_name);
	} else if (vertex_buffer_owner.owns(p_id)) {
		Buffer *vertex_buffer = vertex_buffer_owner.get_or_null(p_id);
		driver->set_object_name(RDD::OBJECT_TYPE_BUFFER, vertex_buffer->driver_id, p_name);
	} else if (index_buffer_owner.owns(p_id)) {
		IndexBuffer *index_buffer = index_buffer_owner.get_or_null(p_id);
		driver->set_object_name(RDD::OBJECT_TYPE_BUFFER, index_buffer->driver_id, p_name);
	} else if (shader_owner.owns(p_id)) {
		Shader *shader = shader_owner.get_or_null(p_id);
		driver->set_object_name(RDD::OBJECT_TYPE_SHADER, shader->driver_id, p_name);
	} else if (uniform_buffer_owner.owns(p_id)) {
		Buffer *uniform_buffer = uniform_buffer_owner.get_or_null(p_id);
		driver->set_object_name(RDD::OBJECT_TYPE_BUFFER, uniform_buffer->driver_id, p_name);
	} else if (texture_buffer_owner.owns(p_id)) {
		Buffer *texture_buffer = texture_buffer_owner.get_or_null(p_id);
		driver->set_object_name(RDD::OBJECT_TYPE_BUFFER, texture_buffer->driver_id, p_name);
	} else if (storage_buffer_owner.owns(p_id)) {
		Buffer *storage_buffer = storage_buffer_owner.get_or_null(p_id);
		driver->set_object_name(RDD::OBJECT_TYPE_BUFFER, storage_buffer->driver_id, p_name);
	} else if (uniform_set_owner.owns(p_id)) {
		UniformSet *uniform_set = uniform_set_owner.get_or_null(p_id);
		driver->set_object_name(RDD::OBJECT_TYPE_UNIFORM_SET, uniform_set->driver_id, p_name);
	} else if (render_pipeline_owner.owns(p_id)) {
		RenderPipeline *pipeline = render_pipeline_owner.get_or_null(p_id);
		driver->set_object_name(RDD::OBJECT_TYPE_PIPELINE, pipeline->driver_id, p_name);
	} else if (compute_pipeline_owner.owns(p_id)) {
		ComputePipeline *pipeline = compute_pipeline_owner.get_or_null(p_id);
		driver->set_object_name(RDD::OBJECT_TYPE_PIPELINE, pipeline->driver_id, p_name);
	} else if (acceleration_structure_owner.owns(p_id)) {
		AccelerationStructure *acceleration_structure = acceleration_structure_owner.get_or_null(p_id);
		driver->set_object_name(RDD::OBJECT_TYPE_ACCELERATION_STRUCTURE, acceleration_structure->driver_id, p_name);
	} else if (raytracing_pipeline_owner.owns(p_id)) {
		RaytracingPipeline *pipeline = raytracing_pipeline_owner.get_or_null(p_id);
		driver->set_object_name(RDD::OBJECT_TYPE_RAYTRACING_PIPELINE, pipeline->driver_id, p_name);
	} else if (hit_sbt_owner.owns(p_id)) {
		HitShaderBindingTable *hit_sbt = hit_sbt_owner.get_or_null(p_id);
		driver->set_object_name(RDD::OBJECT_TYPE_BUFFER, hit_sbt->driver_id, p_name);
	} else {
		ERR_PRINT("Attempted to name invalid ID: " + itos(p_id.get_id()));
		return;
	}
#ifdef DEV_ENABLED
	resource_names[p_id] = p_name;
#endif
}

void RenderingDevice::_draw_command_begin_label(String p_label_name, const Color &p_color) {
	draw_command_begin_label(p_label_name.utf8().span(), p_color);
}

void RenderingDevice::draw_command_begin_label(const Span<char> p_label_name, const Color &p_color) {
	ERR_RENDER_THREAD_GUARD();
	ERR_FAIL_COND_MSG(_recording_error() != OK, "Recording is suspended during frame fence recovery or readback callbacks.");

	if (!context->is_debug_utils_enabled()) {
		return;
	}

	draw_graph.begin_label(p_label_name, p_color);
}

#ifndef DISABLE_DEPRECATED
void RenderingDevice::draw_command_insert_label(String p_label_name, const Color &p_color) {
	WARN_PRINT("Deprecated. Inserting labels no longer applies due to command reordering.");
}
#endif

void RenderingDevice::draw_command_end_label() {
	ERR_RENDER_THREAD_GUARD();
	ERR_FAIL_COND_MSG(_recording_error() != OK, "Recording is suspended during frame fence recovery or readback callbacks.");

	draw_graph.end_label();
}

String RenderingDevice::get_device_vendor_name() const {
	return _get_device_vendor_name(device);
}

String RenderingDevice::get_device_name() const {
	return device.name;
}

RenderingDeviceEnums::DeviceType RenderingDevice::get_device_type() const {
	return RenderingDeviceEnums::DeviceType(device.type);
}

String RenderingDevice::get_device_api_name() const {
	return driver->get_api_name();
}

bool RenderingDevice::is_composite_alpha_supported() const {
	return driver->is_composite_alpha_supported(main_queue);
}

String RenderingDevice::get_device_api_version() const {
	return driver->get_api_version();
}

String RenderingDevice::get_device_pipeline_cache_uuid() const {
	return driver->get_pipeline_cache_uuid();
}

RenderingDevice::DriverWorkarounds RenderingDevice::get_driver_workarounds() const {
	return driver->get_driver_workarounds();
}

void RenderingDevice::swap_buffers(bool p_present) {
	ERR_RENDER_THREAD_GUARD();
	if (native_disposal_active) {
		const Error error = _native_disposal_maintenance();
		ERR_FAIL_COND_MSG(error != OK, "Failed to resume native disposal frame advance.");
		return;
	}
	if (qualification_frame_advance_pending) {
		const Error error = _begin_frame(true);
		ERR_FAIL_COND_MSG(error != OK, "Failed to finish qualification frame advance.");
		return;
	}
	ERR_FAIL_COND_MSG(frame_issue_error != OK, "Cannot advance a frame after a non-replayable rendering issue failure.");
	ERR_FAIL_COND_MSG(frame_wait_draining, "Cannot advance a frame from its readback callback.");
	if (frame_wait_error != OK) {
		// Retry this recording slot before advancing again. Its outstanding
		// native fence still owns the requests and staging allocations.
		if (failed_wait_frame == frame) {
			const Error begin_error = _begin_frame(true);
			ERR_FAIL_COND_MSG(begin_error != OK, "Failed to recover the rendering frame fence.");
			return;
		}
		const Error wait_error = _stall_for_frame(failed_wait_frame);
		ERR_FAIL_COND_MSG(wait_error != OK, "Failed to recover a previous rendering frame fence.");
		frame_wait_error = OK;
		failed_wait_frame = -1;
	}

	GodotProfileZoneGroupedFirst(_profile_zone, "_end_frame");
	const Error end_error = _end_frame();
	ERR_FAIL_COND_MSG(end_error != OK, "Failed to seal the rendering frame.");

	GodotProfileZoneGrouped(_profile_zone, "_execute_frame");
	const Error issue_error = _execute_frame(p_present);
	ERR_FAIL_COND_MSG(issue_error != OK, "Failed to issue the rendering frame.");

	// Advance to the next frame and begin recording again.
	frame = (frame + 1) % frames.size();
	// Early native/ticket polling can refuse before fence recovery records an
	// error. Retain this exact slot and suspend recording until begin succeeds.
	qualification_frame_advance_pending = true;

	GodotProfileZoneGrouped(_profile_zone, "_begin_frame");
	const Error begin_error = _begin_frame(true);
	ERR_FAIL_COND_MSG(begin_error != OK, "Failed to wait for the next rendering frame.");
}

void RenderingDevice::submit() {
	ERR_RENDER_THREAD_GUARD();
	ERR_FAIL_COND_MSG(_recording_error() != OK, "Recording is suspended after a frame fence wait failure.");
	ERR_FAIL_COND_MSG(is_main_instance, "Only local devices can submit and sync.");
	ERR_FAIL_COND_MSG(local_device_processing, "device already submitted, call sync to wait until done.");

	const Error end_error = _end_frame();
	ERR_FAIL_COND_MSG(end_error != OK, "Failed to seal the local rendering frame.");
	const Error issue_error = _execute_frame(false);
	ERR_FAIL_COND_MSG(issue_error != OK, "Failed to issue the local rendering frame.");
	local_device_processing = true;
}

void RenderingDevice::sync() {
	ERR_RENDER_THREAD_GUARD();
	ERR_FAIL_COND_MSG(frame_wait_draining, "Cannot synchronize a frame from its readback callback.");
	ERR_FAIL_COND_MSG(is_main_instance, "Only local devices can submit and sync.");
	ERR_FAIL_COND_MSG(!local_device_processing, "sync can only be called after a submit");

	const Error begin_error = _begin_frame(true);
	ERR_FAIL_COND_MSG(begin_error != OK, "Failed to synchronize the rendering frame.");
	local_device_processing = false;
}

bool RenderingDevice::_native_disposal_available(NativeDisposalWork p_work, uint32_t p_count) const {
	if (!native_disposal_budget) {
		return true;
	}
	const auto &budget = *native_disposal_budget;
	const auto &usage = *native_disposal_usage;
	const uint64_t used = uint64_t(usage.polls) + usage.frees + usage.issues + usage.retains + usage.releases + usage.recycles;
	return budget.receipts && budget.passes && used <= budget.receipts && (p_work == NativeDisposalWork::WORK || p_count <= budget.receipts - used) && usage.passes < budget.passes && budget.deadline_usec && OS::get_singleton()->get_ticks_usec() < budget.deadline_usec;
}

bool RenderingDevice::_native_disposal_admit(NativeDisposalWork p_work, uint32_t p_count) {
	if (!_native_disposal_available(p_work, p_count)) {
		return false;
	}
	if (!native_disposal_budget) {
		return true;
	}
	auto &usage = *native_disposal_usage;
	++usage.passes;
	switch (p_work) {
		case NativeDisposalWork::POLL:
			usage.polls += p_count;
			break;
		case NativeDisposalWork::FREE:
			usage.frees += p_count;
			break;
		case NativeDisposalWork::ISSUE:
			usage.issues += p_count;
			break;
		case NativeDisposalWork::RETAIN:
			usage.retains += p_count;
			break;
		case NativeDisposalWork::RELEASE:
			usage.releases += p_count;
			break;
		case NativeDisposalWork::RECYCLE:
			usage.recycles += p_count;
			break;
		case NativeDisposalWork::WORK:
			break;
	}
	return true;
}

bool RenderingDevice::_native_disposal_charge(const NativeDisposalBudget &p_budget, NativeDisposalUsage &r_usage, NativeDisposalWork p_work) {
	const NativeDisposalBudget *previous_budget = native_disposal_budget;
	NativeDisposalUsage *previous_usage = native_disposal_usage;
	native_disposal_budget = &p_budget;
	native_disposal_usage = &r_usage;
	const bool admitted = _native_disposal_admit(p_work);
	native_disposal_budget = previous_budget;
	native_disposal_usage = previous_usage;
	return admitted;
}

void RenderingDevice::_capture_disposal_cutoff(uint32_t p_frame) {
	auto &f = frames[p_frame];
	auto &cursor = f.disposal;
	cursor = {};
	cursor.active = true;
	cursor.remaining[0] = f.render_pipelines_to_dispose_of.size();
	cursor.remaining[1] = f.compute_pipelines_to_dispose_of.size();
	cursor.remaining[2] = f.raytracing_pipelines_to_dispose_of.size();
	cursor.remaining[3] = f.acceleration_structures_to_dispose_of.size();
	cursor.remaining[4] = f.uniform_sets_to_dispose_of.size();
	cursor.remaining[5] = f.shaders_to_dispose_of.size();
	cursor.remaining[6] = f.samplers_to_dispose_of.size();
	cursor.remaining[7] = f.framebuffers_to_dispose_of.size();
	cursor.remaining[8] = f.textures_to_dispose_of.size();
	cursor.remaining[9] = f.buffers_to_dispose_of.size();
}

Error RenderingDevice::_free_pending_resources_step(int p_frame, const NativeDisposalBudget &p_budget, NativeDisposalUsage &r_usage) {
	// Admission precedes even the cutoff capture: a zero query cannot advance it.
	if (!p_budget.receipts || !p_budget.passes || !p_budget.deadline_usec || OS::get_singleton()->get_ticks_usec() >= p_budget.deadline_usec) {
		return ERR_BUSY;
	}
	auto &f = frames[p_frame];
	auto &cursor = f.disposal;
	const uint64_t used = uint64_t(r_usage.polls) + r_usage.frees + r_usage.issues + r_usage.retains + r_usage.releases + r_usage.recycles;
	if (used >= p_budget.receipts || r_usage.passes >= p_budget.passes) {
		return ERR_BUSY;
	}
	if (!cursor.active) {
		_capture_disposal_cutoff(p_frame);
	}
	auto admit = [&](NativeDisposalWork p_work) { return _native_disposal_charge(p_budget, r_usage, p_work); };
	while (cursor.kind < 10) {
		if (!cursor.remaining[cursor.kind]) {
			++cursor.kind;
			continue;
		}
		switch (cursor.kind) {
			case 0: {
				if (!admit(NativeDisposalWork::FREE)) {
					return ERR_BUSY;
				}
				auto &pipeline = f.render_pipelines_to_dispose_of.front()->get();
				driver->pipeline_free(pipeline.driver_id);
				_production_disposal_returned(pipeline.production_disposal_uses);
				if (pipeline.qualification_dependency) {
					CRASH_COND(!pipeline.qualification_dependency->dependent_children.load());
					pipeline.qualification_dependency->dependent_children.fetch_sub(1);
				}
				_qualification_freed(pipeline.qualification_return);
				f.render_pipelines_to_dispose_of.pop_front();
			} break;
			case 1: {
				if (!admit(NativeDisposalWork::FREE)) {
					return ERR_BUSY;
				}
				ComputePipeline *pipeline = &f.compute_pipelines_to_dispose_of.front()->get();
				driver->pipeline_free(pipeline->driver_id);
				_production_disposal_returned(pipeline->production_disposal_uses);
				_qualification_freed(pipeline->qualification_return);
#if defined(TESTS_ENABLED) && (defined(MODULE_SKINNED_MESH_CAPTURE_TESTS_ENABLED) || defined(MODULE_PARTICLES_RUNTIME_TESTS_ENABLED))
				capture_test_resource_registry.observe_free(RenderCaptureTestResourceKind::ComputePipeline, pipeline->driver_id.id);
#endif

				f.compute_pipelines_to_dispose_of.pop_front();
			} break;
			case 2: {
				auto &pipeline = f.raytracing_pipelines_to_dispose_of.front()->get();
				if (!cursor.phase) {
					if (!admit(NativeDisposalWork::FREE)) {
						return ERR_BUSY;
					}
					driver->buffer_free(pipeline.sbt_buffer.driver_id);
					buffer_memory -= pipeline.sbt_buffer.size;
					cursor.phase = 1;
				}
				if (!admit(NativeDisposalWork::FREE)) {
					return ERR_BUSY;
				}
				driver->raytracing_pipeline_free(pipeline.driver_id);
				f.raytracing_pipelines_to_dispose_of.pop_front();
			} break;
			case 3: {
				auto &acceleration = f.acceleration_structures_to_dispose_of.front()->get();
				if (!cursor.phase) {
					if (!admit(NativeDisposalWork::FREE)) {
						return ERR_BUSY;
					}
					driver->acceleration_structure_free(acceleration.driver_id);
					cursor.phase = 1;
				}
				if (cursor.phase == 1) {
					if (acceleration.scratch_buffer) {
						if (!admit(NativeDisposalWork::WORK)) {
							return ERR_BUSY;
						}
						cursor.allocation_size = driver->buffer_get_allocation_size(acceleration.scratch_buffer);
					}
					cursor.phase = 2;
				}
				if (cursor.phase == 2) {
					if (acceleration.scratch_buffer) {
						if (!admit(NativeDisposalWork::FREE)) {
							return ERR_BUSY;
						}
						driver->buffer_free(acceleration.scratch_buffer);
						buffer_memory -= cursor.allocation_size;
					}
					cursor.phase = 3;
				}
				if (cursor.phase == 3) {
					if (!acceleration.instance_buffers.is_empty()) {
						if (!admit(NativeDisposalWork::WORK)) {
							return ERR_BUSY;
						}
						cursor.allocation_size = uint64_t(driver->api_trait_get(RDD::API_TRAIT_ACCELERATION_STRUCTURE_INSTANCE_SIZE)) * acceleration.max_instance_count;
					}
					cursor.phase = 4;
				}
				while (cursor.item < acceleration.instance_buffers.size()) {
					const auto id = acceleration.instance_buffers[cursor.item].driver_id;
					if (cursor.phase == 4) {
						if (!admit(NativeDisposalWork::WORK)) {
							return ERR_BUSY;
						}
						driver->buffer_unmap(id);
						cursor.phase = 5;
					}
					if (!admit(NativeDisposalWork::FREE)) {
						return ERR_BUSY;
					}
					driver->buffer_free(id);
					buffer_memory -= cursor.allocation_size;
					++cursor.item;
					cursor.phase = 4;
				}
				f.acceleration_structures_to_dispose_of.pop_front();
			} break;
			case 4: {
				if (!admit(NativeDisposalWork::FREE)) {
					return ERR_BUSY;
				}
				UniformSet *uniform_set = &f.uniform_sets_to_dispose_of.front()->get();
				driver->uniform_set_free(uniform_set->driver_id);
				_production_disposal_returned(uniform_set->production_disposal_uses);
				_native_disposal_returned(uniform_set->native_disposal, uniform_set->native_disposal_rid, uniform_set->native_disposal_serial, uniform_set->driver_id.id);
				for (auto &dependency : uniform_set->production_output_dependencies) {
					CRASH_COND(dependency.cell.is_null() || !dependency.cell->production_dependent_sets);
					--dependency.cell->production_dependent_sets;
				}
				for (auto *cell : uniform_set->qualification_dependencies) {
					CRASH_COND(!cell->dependent_children.load());
					cell->dependent_children.fetch_sub(1);
				}
				_qualification_freed(uniform_set->qualification_return);
#if defined(TESTS_ENABLED) && (defined(MODULE_SKINNED_MESH_CAPTURE_TESTS_ENABLED) || defined(MODULE_PARTICLES_RUNTIME_TESTS_ENABLED))
				capture_test_resource_registry.observe_free(RenderCaptureTestResourceKind::UniformSet, uniform_set->driver_id.id);
#endif

				f.uniform_sets_to_dispose_of.pop_front();
			} break;
			case 5: {
				Shader *shader = &f.shaders_to_dispose_of.front()->get();
				if (!cursor.phase) {
					if (!admit(NativeDisposalWork::POLL)) {
						return ERR_BUSY;
					}
					if (!driver->shader_retire_ready_nonblocking(shader->driver_id)) {
						return ERR_BUSY;
					}
					cursor.phase = 1;
				}
				if (!admit(NativeDisposalWork::FREE)) {
					return ERR_BUSY;
				}
				driver->shader_free(shader->driver_id);
				_production_disposal_returned(shader->production_disposal_uses);
				_qualification_freed(shader->qualification_return);
#if defined(TESTS_ENABLED) && (defined(MODULE_SKINNED_MESH_CAPTURE_TESTS_ENABLED) || defined(MODULE_PARTICLES_RUNTIME_TESTS_ENABLED))
				capture_test_resource_registry.observe_free(RenderCaptureTestResourceKind::Shader, shader->driver_id.id);
#endif

				f.shaders_to_dispose_of.pop_front();
			} break;
			case 6: {
				if (!admit(NativeDisposalWork::FREE)) {
					return ERR_BUSY;
				}
				auto &sampler = f.samplers_to_dispose_of.front()->get();
				driver->sampler_free(sampler.driver_id);
				_native_disposal_returned(sampler.native_disposal, sampler.native_disposal_rid, sampler.native_disposal_serial, sampler.driver_id.id);
				_production_disposal_returned(sampler.production_disposal_uses);
				_qualification_freed(sampler.qualification_return);
				f.samplers_to_dispose_of.pop_front();
			} break;
			case 7: {
				auto &framebuffer = f.framebuffers_to_dispose_of.front()->get();
				auto *cache = framebuffer.framebuffer_cache;
				if (cache) {
					while (!cache->storage_map.is_empty()) {
						auto entry = cache->storage_map.begin();
						if (!cursor.phase) {
							if (!admit(NativeDisposalWork::FREE)) {
								return ERR_BUSY;
							}
							driver->framebuffer_free(entry->value.framebuffer);
							cursor.phase = 1;
						}
						if (!admit(NativeDisposalWork::FREE)) {
							return ERR_BUSY;
						}
						driver->render_pass_free(entry->value.render_pass);
						cache->storage_map.remove(entry);
						cursor.phase = 0;
					}
					memdelete(cache);
				}
				if (framebuffer.qualification_dependency) {
					CRASH_COND(!framebuffer.qualification_dependency->dependent_children.load());
					framebuffer.qualification_dependency->dependent_children.fetch_sub(1);
				}
				_native_disposal_returned(framebuffer.native_disposal, framebuffer.native_disposal_rid, framebuffer.native_disposal_serial, framebuffer.native_disposal_backing);
				_qualification_freed(framebuffer.qualification_return);
				f.framebuffers_to_dispose_of.pop_front();
			} break;
			case 8: {
				Texture *texture = &f.textures_to_dispose_of.front()->get();
				auto *fallback = texture->shared_fallback;
				if (cursor.phase == 0) {
					if (fallback && fallback->texture.id) {
						if (!admit(NativeDisposalWork::WORK)) {
							return ERR_BUSY;
						}
						cursor.allocation_size = driver->texture_get_allocation_size(fallback->texture);
					}
					cursor.phase = 1;
				}
				if (cursor.phase == 1) {
					if (fallback && fallback->texture.id) {
						if (!admit(NativeDisposalWork::FREE)) {
							return ERR_BUSY;
						}
						driver->texture_free(fallback->texture);
						texture_memory -= cursor.allocation_size;
					}
					cursor.phase = 2;
				}
				if (cursor.phase == 2) {
					if (fallback && fallback->buffer.id) {
						if (!admit(NativeDisposalWork::WORK)) {
							return ERR_BUSY;
						}
						cursor.allocation_size = driver->buffer_get_allocation_size(fallback->buffer);
					}
					cursor.phase = 3;
				}
				if (cursor.phase == 3) {
					if (fallback && fallback->buffer.id) {
						if (!admit(NativeDisposalWork::FREE)) {
							return ERR_BUSY;
						}
						driver->buffer_free(fallback->buffer);
						buffer_memory -= cursor.allocation_size;
					}
					if (fallback) {
						if (fallback->texture_tracker) {
							RDG::resource_tracker_free(fallback->texture_tracker);
						}
						if (fallback->buffer_tracker) {
							RDG::resource_tracker_free(fallback->buffer_tracker);
						}
						memdelete(fallback);
						texture->shared_fallback = nullptr;
					}
					cursor.phase = 4;
				}
				if (cursor.phase == 4) {
					if (!admit(NativeDisposalWork::WORK)) {
						return ERR_BUSY;
					}
					cursor.allocation_size = driver->texture_get_allocation_size(texture->driver_id);
					cursor.phase = 5;
				}
				if (!admit(NativeDisposalWork::FREE)) {
					return ERR_BUSY;
				}
				driver->texture_free(texture->driver_id);
				_production_disposal_returned(texture->production_disposal_uses);
				_native_disposal_returned(texture->native_disposal, texture->native_disposal_rid, texture->native_disposal_serial, texture->driver_id.id);
				texture_memory -= cursor.allocation_size;
				_qualification_freed(texture->qualification_return);
#if defined(TESTS_ENABLED) && (defined(MODULE_SKINNED_MESH_CAPTURE_TESTS_ENABLED) || defined(MODULE_PARTICLES_RUNTIME_TESTS_ENABLED))
				capture_test_resource_registry.observe_free(RenderCaptureTestResourceKind::Texture, texture->driver_id.id);
#endif

				f.textures_to_dispose_of.pop_front();
			} break;
			case 9: {
				if (!admit(NativeDisposalWork::FREE)) {
					return ERR_BUSY;
				}
				Buffer &buffer = f.buffers_to_dispose_of.front()->get();
				driver->buffer_free(buffer.driver_id);
				_production_disposal_returned(buffer.production_disposal_uses);
				_native_disposal_returned(buffer.native_disposal, buffer.native_disposal_rid, buffer.backing_serial, buffer.driver_id.id);
				_qualification_freed(buffer.qualification_return);
#if defined(TESTS_ENABLED) && (defined(MODULE_SKINNED_MESH_CAPTURE_TESTS_ENABLED) || defined(MODULE_PARTICLES_RUNTIME_TESTS_ENABLED))
				if (buffer.capture_test_allocation_serial) {
					if (!capture_test_allocation_registry.dispose(buffer.driver_id.id, buffer.capture_test_allocation_serial)) {
						capture_test_coverage_failed.store(true);
					} else {
#if defined(TESTS_ENABLED) && defined(MODULE_SKINNED_MESH_CAPTURE_TESTS_ENABLED)
						RenderCaptureTestHooks::observe_buffer_dispose(uint64_t(get_instance_id()), buffer.driver_id.id, buffer.capture_test_allocation_serial);
#endif
					}
				}
#endif
				buffer_memory -= buffer.size;

				f.buffers_to_dispose_of.pop_front();
			} break;
		}
		--cursor.remaining[cursor.kind];
		cursor.phase = 0;
		cursor.item = 0;
		cursor.allocation_size = 0;
	}
	cursor = {};
	if (!f.render_pipelines_to_dispose_of.is_empty() || !f.compute_pipelines_to_dispose_of.is_empty() || !f.raytracing_pipelines_to_dispose_of.is_empty() || !f.acceleration_structures_to_dispose_of.is_empty() || !f.uniform_sets_to_dispose_of.is_empty() || !f.shaders_to_dispose_of.is_empty() || !f.samplers_to_dispose_of.is_empty() || !f.framebuffers_to_dispose_of.is_empty() || !f.textures_to_dispose_of.is_empty() || !f.buffers_to_dispose_of.is_empty()) {
		// Appended rows were outside this original cutoff. Keep their ordinary
		// safe-frame maintenance obligation visible after this prefix returns.
		frames_pending_resources_for_processing = uint32_t(frames.size());
	} else if (frames_pending_resources_for_processing > 0u) {
		--frames_pending_resources_for_processing;
	}
	return OK;
}

void RenderingDevice::_free_pending_resources(int p_frame) {
	const bool resumed_cursor = frames[p_frame].disposal.active;
	if (resumed_cursor) {
		const NativeDisposalBudget budget{ UINT32_MAX, UINT32_MAX, UINT64_MAX };
		NativeDisposalUsage usage;
		const Error error = _free_pending_resources_step(p_frame, budget, usage);
		ERR_FAIL_COND_MSG(error != OK, "Cannot recycle an unfinished native disposal cursor.");
	}
	// Free in dependency usage order, so nothing weird happens.
	// Pipelines.
	while (frames[p_frame].render_pipelines_to_dispose_of.front()) {
		RenderPipeline *pipeline = &frames[p_frame].render_pipelines_to_dispose_of.front()->get();

		driver->pipeline_free(pipeline->driver_id);
		_production_disposal_returned(pipeline->production_disposal_uses);
		if (pipeline->qualification_dependency) {
			CRASH_COND(!pipeline->qualification_dependency->dependent_children.load());
			pipeline->qualification_dependency->dependent_children.fetch_sub(1);
		}
		_qualification_freed(pipeline->qualification_return);

		frames[p_frame].render_pipelines_to_dispose_of.pop_front();
	}

	while (frames[p_frame].compute_pipelines_to_dispose_of.front()) {
		ComputePipeline *pipeline = &frames[p_frame].compute_pipelines_to_dispose_of.front()->get();

		driver->pipeline_free(pipeline->driver_id);
		_production_disposal_returned(pipeline->production_disposal_uses);
		_qualification_freed(pipeline->qualification_return);
#if defined(TESTS_ENABLED) && (defined(MODULE_SKINNED_MESH_CAPTURE_TESTS_ENABLED) || defined(MODULE_PARTICLES_RUNTIME_TESTS_ENABLED))
		capture_test_resource_registry.observe_free(RenderCaptureTestResourceKind::ComputePipeline, pipeline->driver_id.id);
#endif

		frames[p_frame].compute_pipelines_to_dispose_of.pop_front();
	}

	while (frames[p_frame].raytracing_pipelines_to_dispose_of.front()) {
		RaytracingPipeline *pipeline = &frames[p_frame].raytracing_pipelines_to_dispose_of.front()->get();

		driver->buffer_free(pipeline->sbt_buffer.driver_id);
		buffer_memory -= pipeline->sbt_buffer.size;

		driver->raytracing_pipeline_free(pipeline->driver_id);

		frames[p_frame].raytracing_pipelines_to_dispose_of.pop_front();
	}

	// Acceleration structures.
	while (frames[p_frame].acceleration_structures_to_dispose_of.front()) {
		AccelerationStructure &acceleration_structure = frames[p_frame].acceleration_structures_to_dispose_of.front()->get();

		driver->acceleration_structure_free(acceleration_structure.driver_id);

		if (acceleration_structure.scratch_buffer) {
			size_t scratch_size = driver->buffer_get_allocation_size(acceleration_structure.scratch_buffer);
			_THREAD_SAFE_LOCK_
			buffer_memory -= scratch_size;
			_THREAD_SAFE_UNLOCK_

			driver->buffer_free(acceleration_structure.scratch_buffer);
		}

		if (!acceleration_structure.instance_buffers.is_empty()) {
			uint32_t instance_buffer_size = driver->api_trait_get(RDD::API_TRAIT_ACCELERATION_STRUCTURE_INSTANCE_SIZE) * acceleration_structure.max_instance_count;
			for (AccelerationStructure::InstanceBuffer &instance_buffer : acceleration_structure.instance_buffers) {
				driver->buffer_unmap(instance_buffer.driver_id);
				driver->buffer_free(instance_buffer.driver_id);
			}

			_THREAD_SAFE_LOCK_
			buffer_memory -= instance_buffer_size * acceleration_structure.instance_buffers.size();
			_THREAD_SAFE_UNLOCK_
		}

		frames[p_frame].acceleration_structures_to_dispose_of.pop_front();
	}

	// Uniform sets.
	while (frames[p_frame].uniform_sets_to_dispose_of.front()) {
		UniformSet *uniform_set = &frames[p_frame].uniform_sets_to_dispose_of.front()->get();

		driver->uniform_set_free(uniform_set->driver_id);
		_production_disposal_returned(uniform_set->production_disposal_uses);
		_native_disposal_returned(uniform_set->native_disposal, uniform_set->native_disposal_rid, uniform_set->native_disposal_serial, uniform_set->driver_id.id);
		for (auto &dependency : uniform_set->production_output_dependencies) {
			CRASH_COND(dependency.cell.is_null() || !dependency.cell->production_dependent_sets);
			--dependency.cell->production_dependent_sets;
		}
		for (auto *cell : uniform_set->qualification_dependencies) {
			CRASH_COND(!cell->dependent_children.load());
			cell->dependent_children.fetch_sub(1);
		}
		_qualification_freed(uniform_set->qualification_return);
#if defined(TESTS_ENABLED) && (defined(MODULE_SKINNED_MESH_CAPTURE_TESTS_ENABLED) || defined(MODULE_PARTICLES_RUNTIME_TESTS_ENABLED))
		capture_test_resource_registry.observe_free(RenderCaptureTestResourceKind::UniformSet, uniform_set->driver_id.id);
#endif

		frames[p_frame].uniform_sets_to_dispose_of.pop_front();
	}

	// Shaders.
	while (frames[p_frame].shaders_to_dispose_of.front()) {
		Shader *shader = &frames[p_frame].shaders_to_dispose_of.front()->get();

		driver->shader_free(shader->driver_id);
		_production_disposal_returned(shader->production_disposal_uses);
		_qualification_freed(shader->qualification_return);
#if defined(TESTS_ENABLED) && (defined(MODULE_SKINNED_MESH_CAPTURE_TESTS_ENABLED) || defined(MODULE_PARTICLES_RUNTIME_TESTS_ENABLED))
		capture_test_resource_registry.observe_free(RenderCaptureTestResourceKind::Shader, shader->driver_id.id);
#endif

		frames[p_frame].shaders_to_dispose_of.pop_front();
	}

	// Samplers.
	while (frames[p_frame].samplers_to_dispose_of.front()) {
		Sampler &sampler = frames[p_frame].samplers_to_dispose_of.front()->get();

		driver->sampler_free(sampler.driver_id);
		_native_disposal_returned(sampler.native_disposal, sampler.native_disposal_rid, sampler.native_disposal_serial, sampler.driver_id.id);
		_production_disposal_returned(sampler.production_disposal_uses);
		_qualification_freed(sampler.qualification_return);

		frames[p_frame].samplers_to_dispose_of.pop_front();
	}

	// Framebuffers.
	while (frames[p_frame].framebuffers_to_dispose_of.front()) {
		Framebuffer *framebuffer = &frames[p_frame].framebuffers_to_dispose_of.front()->get();
		draw_graph.framebuffer_cache_free(driver, framebuffer->framebuffer_cache);
		if (framebuffer->qualification_dependency) {
			CRASH_COND(!framebuffer->qualification_dependency->dependent_children.load());
			framebuffer->qualification_dependency->dependent_children.fetch_sub(1);
		}
		_native_disposal_returned(framebuffer->native_disposal, framebuffer->native_disposal_rid, framebuffer->native_disposal_serial, framebuffer->native_disposal_backing);
		_qualification_freed(framebuffer->qualification_return);
		frames[p_frame].framebuffers_to_dispose_of.pop_front();
	}

	// Textures.
	while (frames[p_frame].textures_to_dispose_of.front()) {
		Texture *texture = &frames[p_frame].textures_to_dispose_of.front()->get();
		if (texture->bound) {
			WARN_PRINT("Deleted a texture while it was bound.");
		}

		_texture_free_shared_fallback(texture);

		texture_memory -= driver->texture_get_allocation_size(texture->driver_id);
		driver->texture_free(texture->driver_id);
		_production_disposal_returned(texture->production_disposal_uses);
		_native_disposal_returned(texture->native_disposal, texture->native_disposal_rid, texture->native_disposal_serial, texture->driver_id.id);
		_qualification_freed(texture->qualification_return);
#if defined(TESTS_ENABLED) && (defined(MODULE_SKINNED_MESH_CAPTURE_TESTS_ENABLED) || defined(MODULE_PARTICLES_RUNTIME_TESTS_ENABLED))
		capture_test_resource_registry.observe_free(RenderCaptureTestResourceKind::Texture, texture->driver_id.id);
#endif

		frames[p_frame].textures_to_dispose_of.pop_front();
	}

	// Buffers.
	while (frames[p_frame].buffers_to_dispose_of.front()) {
		Buffer &buffer = frames[p_frame].buffers_to_dispose_of.front()->get();
		driver->buffer_free(buffer.driver_id);
		_production_disposal_returned(buffer.production_disposal_uses);
		_native_disposal_returned(buffer.native_disposal, buffer.native_disposal_rid, buffer.backing_serial, buffer.driver_id.id);
		_qualification_freed(buffer.qualification_return);
#if defined(TESTS_ENABLED) && (defined(MODULE_SKINNED_MESH_CAPTURE_TESTS_ENABLED) || defined(MODULE_PARTICLES_RUNTIME_TESTS_ENABLED))
		if (buffer.capture_test_allocation_serial) {
			if (!capture_test_allocation_registry.dispose(buffer.driver_id.id, buffer.capture_test_allocation_serial)) {
				capture_test_coverage_failed.store(true);
			} else {
#if defined(TESTS_ENABLED) && defined(MODULE_SKINNED_MESH_CAPTURE_TESTS_ENABLED)
				RenderCaptureTestHooks::observe_buffer_dispose(uint64_t(get_instance_id()), buffer.driver_id.id, buffer.capture_test_allocation_serial);
#endif
			}
		}
#endif
		buffer_memory -= buffer.size;

		frames[p_frame].buffers_to_dispose_of.pop_front();
	}

	if (!resumed_cursor && frames_pending_resources_for_processing > 0u) {
		--frames_pending_resources_for_processing;
	}
}

uint32_t RenderingDevice::get_frame_delay() const {
	return frames.size();
}

uint64_t RenderingDevice::get_memory_usage(MemoryType p_type) const {
	switch (p_type) {
		case MEMORY_BUFFERS: {
			return buffer_memory;
		}
		case MEMORY_TEXTURES: {
			return texture_memory;
		}
		case MEMORY_TOTAL: {
			return driver->get_total_memory_used();
		}
		default: {
			DEV_ASSERT(false);
			return 0;
		}
	}
}

Error RenderingDevice::_begin_frame(bool p_presented, bool p_nonblocking) {
	if (native_disposal_active) {
		return ERR_BUSY;
	}
	frames[frame].maintenance_wait = {};
	if (!next_qualification_frame_epoch) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	if (frame_issue_error != OK) {
		return frame_issue_error;
	}
	if (frame_wait_draining) {
		return ERR_BUSY;
	}
	GodotProfileZoneGroupedFirst(_profile_zone, "_stall_for_frame");
	if (failed_wait_frame >= 0 && failed_wait_frame != frame) {
		const Error previous_error = _stall_for_frame(failed_wait_frame, p_nonblocking);
		if (previous_error != OK) {
			return previous_error;
		}
	}
	// Before writing to this frame, wait for it to be finished.
	const Error wait_error = _stall_for_frame(frame, p_nonblocking);
	if (wait_error != OK) {
#ifdef TESTS_ENABLED
		static thread_local uint32_t begin_wait_traces = 0;
		if (begin_wait_traces < 16 && OS::get_singleton()->has_environment("GODOT_QUALIFICATION_CAPACITY_TRACE")) {
			++begin_wait_traces;
			print_line(vformat("QFRAME begin_wait frame=%d error=%d nonblocking=%d pending=%d wait_error=%d failed_frame=%d native_code_valid=%d native_code=%d", frame, int(wait_error), int(p_nonblocking), int(qualification_frame_advance_pending.load()), int(frame_wait_error.load()), failed_wait_frame, int(frame_wait_native_code_valid.load()), frame_wait_native_code.load()));
		}
#endif
		return wait_error;
	}
	if (p_nonblocking) {
		for (const Shader &shader : frames[frame].shaders_to_dispose_of) {
			if (!driver->shader_retire_ready_nonblocking(shader.driver_id)) {
				return ERR_BUSY;
			}
		}
	}
	frames[frame].primary_command_buffer_begun = false;
	frames[frame].qualification_graph_closed = false;
	frames[frame].qualification_segment_closed = false;
	frames[frame].qualification_issue_busy = false;
	frames[frame].qualification_issue_slot = 0;
	frames[frame].present_attempted = false;
	frames[frame].present_report = RDD::QueueCallReport();

	if (command_pool_reset_enabled) {
		GodotProfileZoneGrouped(_profile_zone, "driver->command_pool_reset");
		bool reset = driver->command_pool_reset(frames[frame].command_pool);
		ERR_FAIL_COND_V(!reset, _fail_frame_issue(FAILED));
	}

	if (p_presented) {
		GodotProfileZoneGrouped(_profile_zone, "update_perf_report");
		update_perf_report();
		driver->linear_uniform_set_pools_reset(frame);
	}

	// Begin recording on the frame's command buffers.
	GodotProfileZoneGrouped(_profile_zone, "driver->begin_segment");
	driver->begin_segment(frame, frames_drawn++);
	GodotProfileZoneGrouped(_profile_zone, "driver->command_buffer_begin");
	frames[frame].primary_command_buffer_begun = driver->command_buffer_begin(frames[frame].command_buffer);
	if (!frames[frame].primary_command_buffer_begun) {
		frames[frame].chain_prefix_poisoned = true;
	}

	// Reset the graph.
	GodotProfileZoneGrouped(_profile_zone, "draw_graph.begin");
	draw_graph.begin(frames[frame].primary_command_buffer_begun);
	if (!frames[frame].primary_command_buffer_begun) {
		return _fail_frame_issue(FAILED);
	}

	frames[frame].native_height_ranges.clear();
	frames[frame].qualification_epoch = next_qualification_frame_epoch++;
	// A refused free retained its exact original owner and edges. Retry only
	// after a real frame return, in a fresh recording epoch with no old ticket
	// membership; this never waits for a worker sealed by the frame being ended.
	if (frames[frame].qualification_tickets.is_empty()) {
		_qualification_retry_deferred_frees();
	}

	// Erase pending resources.
	GodotProfileZoneGrouped(_profile_zone, "_free_pending_resources");
	_free_pending_resources(frame);

	// Advance staging buffers if used.
	if (upload_staging_buffers.used) {
		upload_staging_buffers.current = (upload_staging_buffers.current + 1) % upload_staging_buffers.blocks.size();
		upload_staging_buffers.used = false;
	}

	if (download_staging_buffers.used) {
		download_staging_buffers.current = (download_staging_buffers.current + 1) % download_staging_buffers.blocks.size();
		download_staging_buffers.used = false;
	}

	if (frames[frame].timestamp_count) {
		driver->timestamp_query_pool_get_results(frames[frame].timestamp_pool, frames[frame].timestamp_count, frames[frame].timestamp_result_values.ptr());
		driver->command_timestamp_query_pool_reset(frames[frame].command_buffer, frames[frame].timestamp_pool, frames[frame].timestamp_count);
		SWAP(frames[frame].timestamp_names, frames[frame].timestamp_result_names);
		SWAP(frames[frame].timestamp_cpu_values, frames[frame].timestamp_cpu_result_values);
	}

	frames[frame].timestamp_result_count = frames[frame].timestamp_count;
	frames[frame].timestamp_count = 0;
	frames[frame].index = Engine::get_singleton()->get_frames_drawn();
	frame_wait_error = OK;
	failed_wait_frame = -1;
	qualification_frame_advance_pending = false;
	return OK;
}

Error RenderingDevice::_fail_frame_issue(Error p_error) {
	CRASH_COND(p_error == OK);
	Error expected = OK;
	frame_issue_error.compare_exchange_strong(expected, p_error);
	const Error error = frame_issue_error.load();
	frames[frame].chain_prefix_poisoned = true;
	for (const BufferGetDataRequest &request : frames[frame].download_buffer_get_data_requests) {
		if (request.capture && request.capture->registry && request.capture->registry->production_readback_delivery(*request.capture, true) == OK) {
			if (request.capture->state->error == OK) {
				request.capture->state->error = error;
			}
			request.capture->registry->production_readback_delivery(*request.capture, false);
		}
		if (request.result.is_valid() && request.result->error == OK) {
			request.result->error = error;
		}
	}
	return error;
}

void RenderingDevice::_seal_buffer_readback_records() {
	for (const BufferGetDataRequest &request : frames[frame].download_buffer_get_data_requests) {
		if (request.result.is_null()) {
			continue;
		}
		for (BufferGetDataResult::CopyRecord &copy : request.result->copies) {
			// A result can span several graph epochs. Earlier sealed copies must
			// retain their original mapping when the staging graph is reset.
			if (copy.mapping_sealed || !draw_graph.contains_record(copy.handle)) {
				continue;
			}
			copy.mapping = draw_graph.record_mapping_state(copy.handle);
			if (copy.mapping == RDG::RecordMappingState::Recorded) {
				const bool slot_found = draw_graph.resolve_record_command_buffer_slot(copy.handle, copy.command_slot);
				const bool buffer_found = draw_graph.resolve_record_command_buffer(copy.handle, copy.command_buffer);
				CRASH_COND(!slot_found || !buffer_found);
			}
			copy.mapping_sealed = true;
		}
	}
}

void RenderingDevice::_observe_buffer_readback_issue(uint32_t p_slot, RDD::CommandBufferID p_command_buffer, Error p_error, const RDD::QueueCallReport &p_report) {
	for (const BufferGetDataRequest &request : frames[frame].download_buffer_get_data_requests) {
		if (request.result.is_null()) {
			continue;
		}
		for (BufferGetDataResult::CopyRecord &copy : request.result->copies) {
			if (!copy.mapping_sealed || copy.issue_attempted || copy.mapping != RDG::RecordMappingState::Recorded || !draw_graph.contains_record(copy.handle) || copy.command_slot != p_slot) {
				continue;
			}
			CRASH_COND(copy.command_buffer != p_command_buffer);
			copy.issue_attempted = true;
			copy.queue_error = p_error;
			copy.issue_report = p_report;
		}
	}
}

Error RenderingDevice::_end_frame(bool p_nonblocking) {
	QualificationSourceAdmissionScope source_admission(*this);
	// The original RD mutex excludes a concurrent producer adding copy-queue work
	// between the bounded preparation and end_segment. The maintenance caller
	// already owns this recursive mutex; an ordinary frame must acquire it too.
	const bool qualification_frame = p_nonblocking || !frames[frame].qualification_tickets.is_empty();
	struct QualificationUnlock {
		Mutex *mutex = nullptr;
		~QualificationUnlock() {
			if (mutex) {
				mutex->unlock();
			}
		}
	} qualification_unlock;
	if (qualification_frame) {
		if (!_thread_safe_.try_lock()) {
			return ERR_BUSY;
		}
		qualification_unlock.mutex = &_thread_safe_;
	}
	if (frame_issue_error != OK) {
		return frame_issue_error;
	}
	if (frames[frame].chain_prefix_poisoned) {
		return _fail_frame_issue(FAILED);
	}
	if (frames[frame].qualification_graph_closed) {
		if (frames[frame].qualification_segment_closed) {
			return OK;
		}
		const Error error = driver->end_segment_checked(qualification_frame);
		if (error == OK) {
			frames[frame].qualification_segment_closed = true;
		}
		return error;
	}
	if (frames[frame].fence_signaled) {
		return OK;
	}
	if (!frames[frame].primary_command_buffer_begun) {
		return OK;
	}
	if (draw_list.active) {
		ERR_PRINT("Found open draw list at the end of the frame, this should never happen (further drawing will likely not work).");
	}

	if (compute_list.active) {
		ERR_PRINT("Found open compute list at the end of the frame, this should never happen (further compute will likely not work).");
	}

	if (raytracing_list.active) {
		ERR_PRINT("Found open raytracing list at the end of the frame, this should never happen (further raytracing will likely not work).");
	}

	if (qualification_frame) {
		if (draw_list.active || compute_list.active || raytracing_list.active || frame_wait_draining || draw_graph.has_driver_callbacks()) {
			return ERR_BUSY;
		}
		RDD::QueueCallReport hidden;
		const Error error = driver->qualification_prepare_segment(hidden, p_nonblocking);
		if (!hidden.calls.is_empty()) {
			frames[frame].qualification_hidden_reports.push_back(hidden);
			_qualification_issue_tickets(UINT32_MAX, hidden, error);
		}
		if (error != OK) {
			return error;
		}
	}

	// The command buffer must be copied into a stack variable as the driver workarounds can change the command buffer in use.
	RDD::CommandBufferID command_buffer = frames[frame].command_buffer;
	GodotProfileZoneGroupedFirst(_profile_zone, "_submit_transfer_workers");
	const Error transfer_error = _submit_transfer_workers(command_buffer, qualification_frame);
	if (transfer_error != OK) {
		return transfer_error == ERR_BUSY ? transfer_error : _fail_frame_issue(transfer_error);
	}
	GodotProfileZoneGrouped(_profile_zone, "_submit_transfer_barriers");
	if (!qualification_frame) {
		_submit_transfer_barriers(command_buffer);
	}

	GodotProfileZoneGrouped(_profile_zone, "draw_graph.end");
	{
		NativeDrainDiagnostic::Span graph_trace("graph.ordinary", uintptr_t(this), command_buffer.id, frame, frames[frame].swap_chains_to_present.size(), frames[frame].fence_signaled, qualification_frame);
		draw_graph.end(RENDER_GRAPH_REORDER == 1, RENDER_GRAPH_FULL_BARRIERS == 1, command_buffer, frames[frame].command_buffer_pool);
	}
	_seal_buffer_readback_records();
	_native_height_seal_frame(frame);
	_qualification_seal_tickets();
	if (draw_graph.recording_failed()) {
		return _fail_frame_issue(FAILED);
	}
	GodotProfileZoneGrouped(_profile_zone, "driver->command_buffer_end");
	driver->command_buffer_end(command_buffer);
	GodotProfileZoneGrouped(_profile_zone, "driver->end_segment");
	frames[frame].qualification_graph_closed = true;
	const Error segment_error = driver->end_segment_checked(qualification_frame);
	if (segment_error == OK) {
		frames[frame].qualification_segment_closed = true;
	}
	return segment_error;
}

bool RenderingDevice::execute_chained_cmds(bool p_present_swap_chain, RenderingDeviceDriver::FenceID p_draw_fence,
		RenderingDeviceDriver::SemaphoreID p_dst_draw_semaphore_to_signal) {
	QualificationSourceAdmissionScope source_admission(*this);
	if (frame_issue_error != OK) {
		return false;
	}
	if (frames[frame].chain_prefix_poisoned || !frames[frame].primary_command_buffer_begun) {
		return false;
	}
	// Execute command buffers and use semaphores to wait on the execution of the previous one.
	// Normally there's only one command buffer, but driver workarounds can force situations where
	// there'll be more.
	uint32_t command_buffer_count = 1;
	RDG::CommandBufferPool &buffer_pool = frames[frame].command_buffer_pool;
	if (buffer_pool.buffers_used > 0) {
		command_buffer_count += buffer_pool.buffers_used;
	}
	if (!frames[frame].qualification_issue_slot) {
		frames[frame].chain_reports.clear();
	}
	frames[frame].qualification_issue_busy = false;
	frames[frame].chain_reports.reserve(command_buffer_count);
	frames[frame].chain_prefix_poisoned = false;

	thread_local LocalVector<RDD::SwapChainID> swap_chains;
	swap_chains.clear();

	// Instead of having just one command; we have potentially many (which had to be split due to an
	// Adreno workaround on mobile, only if the workaround is active). Thus we must execute all of them
	// and chain them together via semaphores as dependent executions.
	thread_local LocalVector<RDD::SemaphoreID> wait_semaphores;
	wait_semaphores = frames[frame].semaphores_to_wait_on;

	bool all_issued = true;
	if (frames[frame].qualification_issue_slot) {
		wait_semaphores.resize(1);
		wait_semaphores[0] = buffer_pool.semaphores[frames[frame].qualification_issue_slot - 1];
	}
	for (uint32_t i = frames[frame].qualification_issue_slot; i < command_buffer_count; i++) {
		RDD::CommandBufferID command_buffer;
		RDD::SemaphoreID signal_semaphore;
		RDD::FenceID signal_fence;
		if (i > 0) {
			command_buffer = buffer_pool.buffers[i - 1];
		} else {
			command_buffer = frames[frame].command_buffer;
		}

		if (i == (command_buffer_count - 1)) {
			// This is the last command buffer, it should signal the semaphore & fence.
			signal_semaphore = p_dst_draw_semaphore_to_signal;
			signal_fence = p_draw_fence;

			if (p_present_swap_chain) {
				// Just present the swap chains as part of the last command execution.
				swap_chains = frames[frame].swap_chains_to_present;
			}
		} else {
			signal_semaphore = buffer_pool.semaphores[i];
			// Semaphores always need to be signaled if it's not the last command buffer.
		}

		RenderingDeviceDriver::QueueCallReport report;
		const Error queue_error = driver->command_queue_execute_and_present_observed(main_queue, wait_semaphores, command_buffer,
				signal_semaphore ? signal_semaphore : VectorView<RDD::SemaphoreID>(), signal_fence,
				swap_chains, report);
		if (queue_error == ERR_BUSY && report.calls.is_empty()) {
			frames[frame].qualification_issue_busy = true;
			return false;
		}
		_observe_buffer_readback_issue(i, command_buffer, queue_error, report);
		const Error height_issue = _native_height_issue_slot(frame, frames[frame].qualification_epoch, i, command_buffer, queue_error, report);
		if (height_issue != OK && height_issue != ERR_BUSY) {
			_fail_frame_issue(height_issue);
			all_issued = false;
		}
		_qualification_issue_tickets(i, report, queue_error);
		frames[frame].chain_reports.push_back(report);
		frames[frame].qualification_issue_slot = i + 1;
		if (queue_error != OK || !report.issued || report.error != OK) {
			_fail_frame_issue(queue_error != OK ? queue_error : (report.error != OK ? report.error : FAILED));
			all_issued = false;
		}
#if defined(TESTS_ENABLED) && defined(MODULE_SKINNED_MESH_CAPTURE_TESTS_ENABLED)
		RenderCaptureTestHooks::observe_driver_result(uint64_t(get_instance_id()), 1, signal_fence.id, queue_error);
		if (queue_error != OK) {
			capture_test_coverage_failed.store(true);
		}
#endif

		// Make the next command buffer wait on the semaphore signaled by this one.
		if (!all_issued) {
			break;
		}
		wait_semaphores.resize(1);
		wait_semaphores[0] = signal_semaphore;
	}

	if (all_issued) {
		frames[frame].semaphores_to_wait_on.clear();
		buffer_pool.buffers_used = 0;
	} else {
		frames[frame].chain_prefix_poisoned = !frames[frame].chain_reports.is_empty();
	}
	return all_issued;
}

Error RenderingDevice::_execute_frame(bool p_present) {
	if (frame_issue_error != OK) {
		return frame_issue_error;
	}
	if (frames[frame].fence_signaled) {
		return OK;
	}
	if (frames[frame].chain_prefix_poisoned) {
		return _fail_frame_issue(FAILED);
	}
	if (!frames[frame].primary_command_buffer_begun) {
		return OK;
	}
	// Check whether this frame should present the swap chains and in which queue.
	const bool frame_can_present = p_present && !frames[frame].swap_chains_to_present.is_empty();
	const bool separate_present_queue = main_queue != present_queue;

	// The semaphore is required if the frame can be presented and a separate present queue is used;
	// since the separate queue will wait for that semaphore before presenting.
	const RDD::SemaphoreID semaphore = (frame_can_present && separate_present_queue)
			? frames[frame].semaphore
			: RDD::SemaphoreID(nullptr);
	const bool present_swap_chain = frame_can_present && !separate_present_queue;

	const bool frame_issued = execute_chained_cmds(present_swap_chain, frames[frame].fence, semaphore);
	// Indicate the fence has been signaled so the next time the frame's contents need to be
	// used, the CPU needs to wait on the work to be completed.
	frames[frame].fence_signaled = frame_issued;
	if (!frame_issued) {
		if (frames[frame].qualification_issue_busy && frame_issue_error == OK) {
			return ERR_BUSY;
		}
		return _fail_frame_issue(frame_issue_error != OK ? frame_issue_error.load() : FAILED);
	}

	if (frame_can_present) {
		if (separate_present_queue && frame_issued) {
			// Issue the presentation separately if the presentation queue is different from the main queue.

			// Keep presentation separate from each command-buffer slot's report.
			frames[frame].present_attempted = true;
			const Error present_error = driver->command_queue_execute_and_present_observed(present_queue, frames[frame].semaphore, {}, {}, {}, frames[frame].swap_chains_to_present, frames[frame].present_report);
#if defined(TESTS_ENABLED) && defined(MODULE_SKINNED_MESH_CAPTURE_TESTS_ENABLED)
			RenderCaptureTestHooks::observe_driver_result(uint64_t(get_instance_id()), 1, 0, present_error);
			if (present_error != OK) {
				capture_test_coverage_failed.store(true);
			}
#endif
			if (present_error != OK) {
				return _fail_frame_issue(present_error);
			}
		}

		frames[frame].swap_chains_to_present.clear();
	}
	return OK;
}

Error RenderingDevice::_stall_for_frame(uint32_t p_frame, bool p_nonblocking) {
	if (native_disposal_active) {
		return _native_disposal_stall_step(p_frame);
	}
	if (frame_issue_error != OK) {
		return frame_issue_error;
	}
	if (frame_wait_draining) {
		return ERR_BUSY;
	}
	const Error height_retain = _native_height_retain_frame_sources(p_frame);
	if (height_retain != OK) {
		return height_retain;
	}
	frame_wait_native_code = 0;
	frame_wait_native_code_valid = false;
	thread_local PackedByteArray packed_byte_array;
	// Original Frame ownership covers every actual slot call, including slots
	// without the final fence. Qualification only polls; ordinary frame waits may
	// use the original native wait, then recycle the same sources independently.
	if (driver->native_call_observation_supported()) {
		for (auto &report : frames[p_frame].chain_reports) {
			for (auto &call : report.calls) {
				if (call.presentation_only || call.source_owner_returned) {
					continue;
				}
				const auto completion = p_nonblocking ? driver->native_call_poll(call.serial) : driver->native_call_wait(call.serial);
				if (completion.native_error) {
					frame_wait_native_code = completion.native_code;
					frame_wait_native_code_valid = true;
					return FAILED;
				}
				if (!completion.matches(call.completion_serial)) {
					return ERR_BUSY;
				}
			}
		}
	}
	const Error ticket_poll = _qualification_poll_tickets(p_frame);
	if (ticket_poll != OK) {
		return ticket_poll;
	}
	if (driver->native_call_observation_supported()) {
		for (auto &report : frames[p_frame].chain_reports) {
			if (!report.submission_serial) {
				for (auto &call : report.calls) {
					if (call.presentation_only || call.source_owner_returned) {
						continue;
					}
					const Error recycled = p_nonblocking ? driver->native_call_recycle(call.serial) : driver->native_call_recycle_wait(call.serial);
					if (recycled != OK) {
						return recycled;
					}
					call.source_owner_returned = true;
				}
			}
		}
	}

	for (auto &report : frames[p_frame].qualification_hidden_reports) {
		for (auto &call : report.calls) {
			if (call.presentation_only || call.source_owner_returned) {
				continue;
			}
			const auto completion = p_nonblocking ? driver->native_call_poll(call.serial) : driver->native_call_wait(call.serial);
			if (completion.native_error) {
				frame_wait_native_code = completion.native_code;
				frame_wait_native_code_valid = true;
				return FAILED;
			}
			if (!completion.matches(call.completion_serial)) {
				return ERR_BUSY;
			}
			if (call.frame_observer_owned) {
				const Error released = driver->native_call_release(call.serial);
				if (released != OK) {
					return released;
				}
			}
			call.source_owner_returned = true; // Return this Frame's observer exactly once, preserving actual source identity.
		}
	}
	frames[p_frame].qualification_hidden_reports.clear();

	if (frames[p_frame].fence_signaled) {
		GodotProfileZoneGroupedFirst(_profile_zone, "driver->fence_wait");

		Error wait_error = OK;
		if (p_nonblocking) {
			// This continuation owns no extension Callable. An unrelated legacy
			// callback remains for the ordinary frame path to deliver.
			if (!frames[p_frame].download_texture_get_data_requests.is_empty()) {
				return ERR_BUSY;
			}
			for (const auto &request : frames[p_frame].download_buffer_get_data_requests) {
				if (!request.capture && (request.result.is_null() || !request.result->qualification_owned)) {
					return ERR_BUSY;
				}
			}
			const auto &reports = frames[p_frame].chain_reports;
			if (reports.is_empty() || !reports[reports.size() - 1].submission_serial) {
				frame_wait_native_code_valid = true;
				return ERR_UNAVAILABLE;
			}
			const uint64_t serial = reports[reports.size() - 1].submission_serial;
			const auto completion = driver->fence_poll(frames[p_frame].fence, serial);
			if (completion.native_error) {
				frame_wait_native_code = completion.native_code;
				frame_wait_native_code_valid = true;
				wait_error = FAILED;
			} else if (!completion.matches(serial)) {
				return ERR_BUSY;
			} else {
				wait_error = driver->fence_recycle(frames[p_frame].fence, serial);
			}
		} else {
			wait_error = driver->fence_wait(frames[p_frame].fence);
		}
#if defined(TESTS_ENABLED) && defined(MODULE_SKINNED_MESH_CAPTURE_TESTS_ENABLED)
		RenderCaptureTestHooks::observe_driver_result(uint64_t(get_instance_id()), 2, frames[p_frame].fence.id, wait_error);
		if (wait_error != OK) {
			capture_test_coverage_failed.store(true);
		}
#endif
		if (wait_error != OK) {
			frames[p_frame].primary_command_buffer_begun = false;
			frame_wait_error = wait_error;
			if (!frame_wait_native_code_valid.load()) {
				frame_wait_native_code_valid = true;
			}
			failed_wait_frame = p_frame;
			return wait_error;
		}
		frames[p_frame].fence_signaled = false;
		frames[p_frame].primary_command_buffer_begun = false;
		frame_wait_draining = true;

		// Flush any pending requests for asynchronous buffer downloads.
		if (!frames[p_frame].download_buffer_get_data_requests.is_empty()) {
			GodotProfileZoneGrouped(_profile_zone, "flush asynchronous buffer downloads");
			const LocalVector<BufferGetDataRequest> requests = std::move(frames[p_frame].download_buffer_get_data_requests);
			for (uint32_t i = 0; i < requests.size(); i++) {
				const BufferGetDataRequest &request = requests[i];
				const Ref<BufferGetDataResult> result = request.result;
				auto *capture = request.capture;
				if (capture && (!capture->registry || capture->registry->production_readback_delivery(*capture, true) != OK)) {
					continue;
				}
				auto finish_delivery = [&]() {
					if (capture) {
						if (capture->state->returned <= capture->state->issued && request.size <= capture->state->issued - capture->state->returned) {
							capture->state->returned += request.size;
						}
						capture->registry->production_readback_delivery(*capture, false);
					}
				};
				struct DeliveryReturn {
					decltype(finish_delivery) &finish;
					~DeliveryReturn() { finish(); }
				} delivery_return{ finish_delivery };
				if (!capture && result.is_null()) {
					continue;
				}
				if (capture) {
					if (request.result_offset != capture->state->returned || request.size > capture->state->issued - capture->state->returned) {
						capture->state->error = ERR_INVALID_DATA;
						continue;
					}
				}
				Error &result_error = capture ? capture->state->error : result->error;
				bool &delivered = capture ? capture->state->delivered : result->delivered;
				const uint64_t byte_size = capture ? capture->byte_capacity : uint64_t(result->bytes.size());
				uint8_t *byte_data = capture ? capture->bytes : result->bytes.ptrw();
				uint64_t completed = capture ? capture->state->completed : result->completed_size;
				if (result_error != OK || delivered) {
					continue;
				}
				if (request.result_offset != completed || request.result_offset > byte_size || request.size > byte_size - request.result_offset) {
					result_error = ERR_INVALID_DATA;
					continue;
				}

				uint32_t array_offset = 0;
				for (uint32_t j = 0; j < request.frame_local_count; j++) {
					uint32_t local_index = request.frame_local_index + j;
					const RDD::BufferCopyRegion &region = frames[p_frame].download_buffer_copy_regions[local_index];
					if (region.size > request.size - array_offset) {
						result_error = ERR_INVALID_DATA;
						break;
					}
					int64_t native_code = 0;
					uint8_t *buffer_data = driver->buffer_map_observed(frames[p_frame].download_buffer_staging_buffers[local_index], native_code);
					if (buffer_data == nullptr) {
						result_error = ERR_CANT_ACQUIRE_RESOURCE;
						if (capture) {
							capture->native_code = native_code;
						} else {
							result->native_code = native_code;
						}
						ERR_PRINT("Failed to map an asynchronous readback staging buffer.");
						break;
					}
					memcpy(byte_data + request.result_offset + array_offset, buffer_data + region.dst_offset, region.size);
					driver->buffer_unmap(frames[p_frame].download_buffer_staging_buffers[local_index]);
					array_offset += region.size;
				}
				if (result_error != OK) {
					continue;
				}
				if (array_offset != request.size) {
					result_error = ERR_INVALID_DATA;
					continue;
				}
				completed += array_offset;
				if (capture) {
					capture->state->completed = completed;
				} else {
					result->completed_size = uint32_t(completed);
				}
				if (!request.final_segment) {
					continue;
				}
				if (completed != byte_size) {
					result_error = ERR_INVALID_DATA;
					continue;
				}
				delivered = true;

#if defined(TESTS_ENABLED) && defined(MODULE_SKINNED_MESH_CAPTURE_TESTS_ENABLED)
				RenderCaptureTestHooks::observe_readback_completed(request.buffer.get_id(), byte_size);
#endif
				if (!capture && !result->qualification_owned) {
					request.callback.call(result->bytes);
				}
			}

			frames[p_frame].download_buffer_staging_buffers.clear();
			frames[p_frame].download_buffer_copy_regions.clear();
			frames[p_frame].download_buffer_get_data_requests.clear();
		}

		// Flush any pending requests for asynchronous texture downloads.
		if (!frames[p_frame].download_texture_get_data_requests.is_empty()) {
			GodotProfileZoneGrouped(_profile_zone, "flush asynchronous texture downloads");
			const LocalVector<TextureGetDataRequest> requests = std::move(frames[p_frame].download_texture_get_data_requests);
			for (uint32_t i = 0; i < requests.size(); i++) {
				const TextureGetDataRequest &request = requests[i];
				uint32_t texture_size = get_image_format_required_size(request.format, request.width, request.height, request.depth, request.mipmaps);
				packed_byte_array.resize(texture_size);

				// Find the block size of the texture's format.
				uint32_t block_w = 0;
				uint32_t block_h = 0;
				get_compressed_image_format_block_dimensions(request.format, block_w, block_h);

				uint32_t block_size = get_compressed_image_format_block_byte_size(request.format);
				uint32_t pixel_size = get_image_format_pixel_size(request.format);
				uint32_t region_size = texture_download_region_size_px;
				bool map_failed = false;

				for (uint32_t j = 0; j < request.frame_local_count; j++) {
					uint32_t local_index = request.frame_local_index + j;
					const RDD::BufferTextureCopyRegion &region = frames[p_frame].download_buffer_texture_copy_regions[local_index];
					uint32_t w = STEPIFY(request.width >> region.texture_subresource.mipmap, block_w);
					uint32_t h = STEPIFY(request.height >> region.texture_subresource.mipmap, block_h);
					uint32_t region_w = MIN(region_size, w - region.texture_offset.x);
					uint32_t region_h = MIN(region_size, h - region.texture_offset.y);

					uint8_t *buffer_data = driver->buffer_map(frames[p_frame].download_texture_staging_buffers[local_index]);
					if (buffer_data == nullptr) {
						map_failed = true;
						ERR_PRINT("Failed to map an asynchronous texture readback staging buffer.");
						break;
					}
					const uint8_t *read_ptr = buffer_data + region.buffer_offset;
					uint8_t *write_ptr = packed_byte_array.ptrw() + frames[p_frame].download_texture_mipmap_offsets[local_index];
					uint32_t unit_size = pixel_size;
					if (block_w != 1 || block_h != 1) {
						unit_size = block_size;
					}

					write_ptr += ((region.texture_offset.y / block_h) * (w / block_w) + (region.texture_offset.x / block_w)) * unit_size;
					for (uint32_t y = region_h / block_h; y > 0; y--) {
						memcpy(write_ptr, read_ptr, (region_w / block_w) * unit_size);
						write_ptr += (w / block_w) * unit_size;
						read_ptr += region.row_pitch;
					}

					driver->buffer_unmap(frames[p_frame].download_texture_staging_buffers[local_index]);
				}

				// A map failure has no result payload. Keep the callback success-only,
				// matching async buffer readback, and never expose partially copied data.
				if (!map_failed) {
					request.callback.call(packed_byte_array);
				}
			}

			GodotProfileZoneGrouped(_profile_zone, "clear buffers");
			frames[p_frame].download_texture_staging_buffers.clear();
			frames[p_frame].download_buffer_texture_copy_regions.clear();
			frames[p_frame].download_texture_mipmap_offsets.clear();
			frames[p_frame].download_texture_get_data_requests.clear();
		}
		_qualification_return_frame_tickets(p_frame);
		for (auto &report : frames[p_frame].chain_reports) {
			for (auto &call : report.calls) {
				if (!call.presentation_only) {
					call.source_owner_returned = true;
				}
			}
		}
		frame_wait_draining = false;
	}
	return OK;
}

Error RenderingDevice::_stall_for_previous_frames() {
	if (failed_wait_frame >= 0) {
		const Error wait_error = _stall_for_frame(failed_wait_frame);
		if (wait_error != OK) {
			return wait_error;
		}
	}
	for (uint32_t i = 0; i < frames.size(); i++) {
		const Error wait_error = _stall_for_frame(i);
		if (wait_error != OK) {
			return wait_error;
		}
	}
	return OK;
}

Error RenderingDevice::_flush_and_stall_for_all_frames(bool p_begin_frame) {
	if (frame_issue_error != OK) {
		return frame_issue_error;
	}
	if (frame_wait_draining) {
		return ERR_BUSY;
	}
	const Error wait_error = _stall_for_previous_frames();
	if (wait_error != OK) {
		return wait_error;
	}
	const Error end_error = _end_frame();
	if (end_error != OK) {
		return end_error;
	}
	const Error issue_error = _execute_frame(false);
	if (issue_error != OK) {
		return issue_error;
	}

	if (p_begin_frame) {
		return _begin_frame();
	} else {
		return _stall_for_frame(frame);
	}
}

Error RenderingDevice::initialize(RenderingContextDriver *p_context, DisplayServerEnums::WindowID p_main_window) {
	ERR_RENDER_THREAD_GUARD_V(ERR_UNAVAILABLE);

	Error err;
	RenderingContextDriver::SurfaceID main_surface = 0;
	is_main_instance = (singleton == this) && (p_main_window != DisplayServerEnums::INVALID_WINDOW_ID);
#ifdef TESTS_ENABLED
	uint32_t test_staging_block_bytes = 0;
	if (is_main_instance && OS::get_singleton()->has_environment("GODOT_K03_TEST_MAIN_RD_STAGING_BLOCK_BYTES")) {
		const String value = OS::get_singleton()->get_environment("GODOT_K03_TEST_MAIN_RD_STAGING_BLOCK_BYTES");
		ERR_FAIL_COND_V_MSG(!value.is_valid_int(), ERR_INVALID_PARAMETER, "Invalid TESTS-only main RD staging block bytes.");
		const int64_t block = value.to_int();
		ERR_FAIL_COND_V_MSG(block < 32 || block > 4096 || (block & (block - 1)), ERR_INVALID_PARAMETER, "Invalid TESTS-only main RD staging block bytes.");
		test_staging_block_bytes = uint32_t(block);
	}
#endif

	if (p_main_window != DisplayServerEnums::INVALID_WINDOW_ID) {
		// Retrieve the surface from the main window if it was specified.
		main_surface = p_context->surface_get_from_window(p_main_window);
		ERR_FAIL_COND_V(main_surface == 0, FAILED);
	}

	context = p_context;
	driver = context->driver_create();
	driver->session_owner = this;

	print_verbose("Devices:");
	int32_t device_index = Engine::get_singleton()->get_gpu_index();
	const uint32_t device_count = context->device_get_count();
	const bool device_index_out_of_range = (device_index >= int32_t(device_count));
	const bool detect_device = (device_index < 0) || device_index_out_of_range;

	if (device_index_out_of_range) {
		WARN_PRINT(vformat("The specified GPU index %d is out of range on this system (0-%d). Falling back to automatic device selection.", device_index, device_count - 1));
	}

	uint32_t device_type_score = 0;
	for (uint32_t i = 0; i < device_count; i++) {
		RenderingContextDriver::Device device_option = context->device_get(i);
		String name = device_option.name;
		String vendor = _get_device_vendor_name(device_option);
		String type = _get_device_type_name(device_option);
		bool present_supported = main_surface != 0 ? context->device_supports_present(i, main_surface) : false;
		print_verbose("  #" + itos(i) + ": " + vendor + " " + name + " - " + (present_supported ? "Supported" : "Unsupported") + ", " + type);
		if (detect_device && (present_supported || main_surface == 0)) {
			// If a window was specified, present must be supported by the device to be available as an option.
			// Assign a score for each type of device and prefer the device with the higher score.
			uint32_t option_score = _get_device_type_score(device_option);
			if (option_score > device_type_score) {
				device_index = i;
				device_type_score = option_score;
			}
		}
	}

	ERR_FAIL_COND_V_MSG((device_index < 0) || (device_index >= int32_t(device_count)), ERR_CANT_CREATE, "None of the devices supports both graphics and present queues.");

	uint32_t frame_count = 1;
	if (main_surface != 0) {
		frame_count = MAX(2U, uint32_t(GLOBAL_GET("rendering/rendering_device/vsync/frame_queue_size")));
	}

	frame = 0;
	max_timestamp_query_elements = GLOBAL_GET("debug/settings/profiler/max_timestamp_query_elements");

	device = context->device_get(device_index);
	err = driver->initialize(device_index, frame_count);
	ERR_FAIL_COND_V_MSG(err != OK, FAILED, "Failed to initialize driver for device.");

	if (is_main_instance) {
		// Only the singleton instance with a display should print this information.
		String rendering_method;
		if (OS::get_singleton()->get_current_rendering_method() == "mobile") {
			rendering_method = "Forward Mobile";
		} else {
			rendering_method = "Forward+";
		}

		// Output our device version.
		Engine::get_singleton()->print_header(vformat("%s %s - %s - Using Device #%d: %s - %s", get_device_api_name(), get_device_api_version(), rendering_method, device_index, _get_device_vendor_name(device), device.name));
	}

	// Pick the main queue family. It is worth noting we explicitly do not request the transfer bit, as apparently the specification defines
	// that the existence of either the graphics or compute bit implies that the queue can also do transfer operations, but it is optional
	// to indicate whether it supports them or not with the dedicated transfer bit if either is set.
	BitField<RDD::CommandQueueFamilyBits> main_queue_bits = {};
	main_queue_bits.set_flag(RDD::COMMAND_QUEUE_FAMILY_GRAPHICS_BIT);
	main_queue_bits.set_flag(RDD::COMMAND_QUEUE_FAMILY_COMPUTE_BIT);

#if !FORCE_SEPARATE_PRESENT_QUEUE
	// Needing to use a separate queue for presentation is an edge case that remains to be seen what hardware triggers it at all.
	main_queue_family = driver->command_queue_family_get(main_queue_bits, main_surface);
	if (!main_queue_family && (main_surface != 0))
#endif
	{
		// If it was not possible to find a main queue that supports the surface, we attempt to get two different queues instead.
		main_queue_family = driver->command_queue_family_get(main_queue_bits);
		present_queue_family = driver->command_queue_family_get(BitField<RDD::CommandQueueFamilyBits>(), main_surface);
		ERR_FAIL_COND_V(!present_queue_family, FAILED);
	}

	ERR_FAIL_COND_V(!main_queue_family, FAILED);

	// Create the main queue.
	main_queue = driver->command_queue_create(main_queue_family, true);
	ERR_FAIL_COND_V(!main_queue, FAILED);

	transfer_queue_family = driver->command_queue_family_get(RDD::COMMAND_QUEUE_FAMILY_TRANSFER_BIT);
	if (!transfer_queue_family) {
		// Use main queue family if transfer queue family is not supported.
		transfer_queue_family = main_queue_family;
	}

	// Create the transfer queue.
	transfer_queue = driver->command_queue_create(transfer_queue_family);
	ERR_FAIL_COND_V(!transfer_queue, FAILED);

	if (present_queue_family) {
		// Create the present queue.
		present_queue = driver->command_queue_create(present_queue_family);
		ERR_FAIL_COND_V(!present_queue, FAILED);
	} else {
		// Use main queue as the present queue.
		present_queue = main_queue;
		present_queue_family = main_queue_family;
	}

	// Use the processor count as the max amount of transfer workers that can be created.
	transfer_worker_pool_max_size = OS::get_singleton()->get_processor_count();

	// Pre-allocate to avoid locking a mutex when indexing into them.
	transfer_worker_pool.resize(transfer_worker_pool_max_size);
	transfer_worker_operation_used_by_draw.resize(transfer_worker_pool_max_size);

	frames.resize(frame_count);

	// Create data for all the frames.
	bool frame_failed = false;
	for (uint32_t i = 0; i < frames.size(); i++) {
		frames[i].index = 0;

		// Create command pool, command buffers, semaphores and fences.
		frames[i].command_pool = driver->command_pool_create(main_queue_family, RDD::COMMAND_BUFFER_TYPE_PRIMARY);
		if (!frames[i].command_pool) {
			frame_failed = true;
			break;
		}
		frames[i].command_buffer = driver->command_buffer_create(frames[i].command_pool);
		if (!frames[i].command_buffer) {
			frame_failed = true;
			break;
		}
		frames[i].semaphore = driver->semaphore_create();
		if (!frames[i].semaphore) {
			frame_failed = true;
			break;
		}
		frames[i].fence = driver->fence_create();
		if (!frames[i].fence) {
			frame_failed = true;
			break;
		}
		frames[i].fence_signaled = false;

		// Create query pool.
		frames[i].timestamp_pool = driver->timestamp_query_pool_create(max_timestamp_query_elements);
		frames[i].timestamp_names.resize(max_timestamp_query_elements);
		frames[i].timestamp_cpu_values.resize(max_timestamp_query_elements);
		frames[i].timestamp_count = 0;
		frames[i].timestamp_result_names.resize(max_timestamp_query_elements);
		frames[i].timestamp_cpu_result_values.resize(max_timestamp_query_elements);
		frames[i].timestamp_result_values.resize(max_timestamp_query_elements);
		frames[i].timestamp_result_count = 0;

		// Assign the main queue family and command pool to the command buffer pool.
		frames[i].command_buffer_pool.pool = frames[i].command_pool;

		// Create the semaphores for the transfer workers.
		frames[i].transfer_worker_semaphores.resize(transfer_worker_pool_max_size);
		for (uint32_t j = 0; j < transfer_worker_pool_max_size; j++) {
			frames[i].transfer_worker_semaphores[j] = driver->semaphore_create();
			if (!frames[i].transfer_worker_semaphores[j]) {
				frame_failed = true;
				break;
			}
		}
	}
	if (frame_failed) {
		// Clean up created data.
		for (uint32_t i = 0; i < frames.size(); i++) {
			if (frames[i].command_pool) {
				driver->command_pool_free(frames[i].command_pool);
			}
			if (frames[i].semaphore) {
				driver->semaphore_free(frames[i].semaphore);
			}
			if (frames[i].fence) {
				driver->fence_free(frames[i].fence);
			}
			if (frames[i].timestamp_pool) {
				driver->timestamp_query_pool_free(frames[i].timestamp_pool);
			}
			for (uint32_t j = 0; j < frames[i].transfer_worker_semaphores.size(); j++) {
				if (frames[i].transfer_worker_semaphores[j]) {
					driver->semaphore_free(frames[i].transfer_worker_semaphores[j]);
				}
			}
		}
		frames.clear();
		ERR_FAIL_V_MSG(FAILED, "Failed to create frame data.");
	}

	// Start from frame count, so everything else is immediately old.
	frames_drawn = frames.size();

	// Initialize recording on the first frame.
	driver->begin_segment(frame, frames_drawn++);
	frames[0].primary_command_buffer_begun = driver->command_buffer_begin(frames[0].command_buffer);
	ERR_FAIL_COND_V_MSG(!frames[0].primary_command_buffer_begun, FAILED, "Failed to begin the initial command buffer.");

	// Create draw graph and start it initialized as well.
	draw_graph.initialize(driver, &_render_pass_create_from_graph, frames.size(), main_queue_family, SECONDARY_COMMAND_BUFFERS_PER_FRAME);
	draw_graph.begin(frames[0].primary_command_buffer_begun);

	for (uint32_t i = 0; i < frames.size(); i++) {
		// Reset all queries in a query pool before doing any operations with them..
		driver->command_timestamp_query_pool_reset(frames[0].command_buffer, frames[i].timestamp_pool, max_timestamp_query_elements);
	}

	// Convert block size from KB.
	upload_staging_buffers.block_size = GLOBAL_GET("rendering/rendering_device/staging_buffer/block_size_kb");
	upload_staging_buffers.block_size = MAX(4u, upload_staging_buffers.block_size);
	upload_staging_buffers.block_size *= 1024;

	// Convert staging buffer size from MB.
	upload_staging_buffers.max_size = GLOBAL_GET("rendering/rendering_device/staging_buffer/max_size_mb");
	upload_staging_buffers.max_size = MAX(1u, upload_staging_buffers.max_size);
	upload_staging_buffers.max_size *= 1024 * 1024;
	upload_staging_buffers.max_size = MAX(upload_staging_buffers.max_size, upload_staging_buffers.block_size * 4);

	// Copy the sizes to the download staging buffers.
	download_staging_buffers.block_size = upload_staging_buffers.block_size;
	download_staging_buffers.max_size = upload_staging_buffers.max_size;
#ifdef TESTS_ENABLED
	if (test_staging_block_bytes) {
		// Explicit resource-pressure control only. Preserve frame age, progress,
		// completion and Registry authority; apply before the first allocation.
		ERR_FAIL_COND_V(!upload_staging_buffers.blocks.is_empty() || !download_staging_buffers.blocks.is_empty(), ERR_ALREADY_IN_USE);
		const uint32_t blocks = MAX(4U, uint32_t(frames.size()));
		ERR_FAIL_COND_V(blocks > UINT32_MAX / test_staging_block_bytes, ERR_PARAMETER_RANGE_ERROR);
		upload_staging_buffers.block_size = download_staging_buffers.block_size = test_staging_block_bytes;
		upload_staging_buffers.max_size = download_staging_buffers.max_size = blocks * test_staging_block_bytes;
		print_line(vformat("K03_TEST_MAIN_RD_STAGING block=%d max=%d initial_upload_blocks=%d initial_download_blocks=%d frame_count=%d", test_staging_block_bytes, upload_staging_buffers.max_size, upload_staging_buffers.blocks.size(), download_staging_buffers.blocks.size(), frames.size()));
	}
#endif

	texture_upload_region_size_px = _normalize_staging_region_size(int64_t(GLOBAL_GET("rendering/rendering_device/staging_buffer/texture_upload_region_size_px")));

	texture_download_region_size_px = _normalize_staging_region_size(int64_t(GLOBAL_GET("rendering/rendering_device/staging_buffer/texture_download_region_size_px")));

	// Ensure current staging block is valid and at least one per frame exists.
	upload_staging_buffers.current = 0;
	upload_staging_buffers.used = false;
	upload_staging_buffers.usage_bits = RDD::BUFFER_USAGE_TRANSFER_FROM_BIT;

	download_staging_buffers.current = 0;
	download_staging_buffers.used = false;
	download_staging_buffers.usage_bits = RDD::BUFFER_USAGE_TRANSFER_TO_BIT;

	for (uint32_t i = 0; i < frames.size(); i++) {
		// Staging was never used, create the blocks.
		err = _insert_staging_block(upload_staging_buffers);
		ERR_FAIL_COND_V(err, FAILED);

		err = _insert_staging_block(download_staging_buffers);
		ERR_FAIL_COND_V(err, FAILED);
	}

	draw_list = DrawList();
	compute_list = ComputeList();
	raytracing_list = RaytracingList();

	bool project_pipeline_cache_enable = GLOBAL_GET("rendering/rendering_device/pipeline_cache/enable");
	if (is_main_instance && project_pipeline_cache_enable) {
		// Only the instance that is not a local device and is also the singleton is allowed to manage a pipeline cache.
		pipeline_cache_file_path = vformat("user://vulkan/pipelines.%s.%s",
				OS::get_singleton()->get_current_rendering_method(),
				device.name.validate_filename().replace_char(' ', '_').to_lower());
		if (Engine::get_singleton()->is_editor_hint()) {
			pipeline_cache_file_path += ".editor";
		}
		pipeline_cache_file_path += ".cache";

		Vector<uint8_t> cache_data = _load_pipeline_cache();
		pipeline_cache_enabled = driver->pipeline_cache_create(cache_data);
		if (pipeline_cache_enabled) {
			pipeline_cache_size = driver->pipeline_cache_query_size();
			print_verbose(vformat("Startup PSO cache (%.1f MiB)", pipeline_cache_size / (1024.0f * 1024.0f)));
		}
	}

	// Find the best method available for VRS on the current hardware.
	_vrs_detect_method();

	return GDExtensionDeviceExit::Registry::get_singleton().device_created(this);
}

Vector<uint8_t> RenderingDevice::_load_pipeline_cache() {
	DirAccess::make_dir_recursive_absolute(pipeline_cache_file_path.get_base_dir());

	if (FileAccess::exists(pipeline_cache_file_path)) {
		Error file_error;
		Vector<uint8_t> file_data = FileAccess::get_file_as_bytes(pipeline_cache_file_path, &file_error);
		return file_data;
	} else {
		return Vector<uint8_t>();
	}
}

void RenderingDevice::update_pipeline_cache(bool p_closing) {
	_THREAD_SAFE_METHOD_

	{
		bool still_saving = pipeline_cache_save_task != WorkerThreadPool::INVALID_TASK_ID && !WorkerThreadPool::get_singleton()->is_task_completed(pipeline_cache_save_task);
		if (still_saving) {
			if (p_closing) {
				WorkerThreadPool::get_singleton()->wait_for_task_completion(pipeline_cache_save_task);
				pipeline_cache_save_task = WorkerThreadPool::INVALID_TASK_ID;
			} else {
				// We can't save until the currently running save is done. We'll retry next time; worst case, we'll save when exiting.
				return;
			}
		}
	}

	{
		size_t new_pipelines_cache_size = driver->pipeline_cache_query_size();
		ERR_FAIL_COND(!new_pipelines_cache_size);
		size_t difference = new_pipelines_cache_size - pipeline_cache_size;

		bool must_save = false;

		if (p_closing) {
			must_save = difference > 0;
		} else {
			float save_interval = GLOBAL_GET("rendering/rendering_device/pipeline_cache/save_chunk_size_mb");
			must_save = difference > 0 && difference / (1024.0f * 1024.0f) >= save_interval;
		}

		if (must_save) {
			pipeline_cache_size = new_pipelines_cache_size;
		} else {
			return;
		}
	}

	if (p_closing) {
		_save_pipeline_cache(this);
	} else {
		pipeline_cache_save_task = WorkerThreadPool::get_singleton()->add_native_task(&_save_pipeline_cache, this, false, "PipelineCacheSave");
	}
}

void RenderingDevice::_save_pipeline_cache(void *p_data) {
	RenderingDevice *self = static_cast<RenderingDevice *>(p_data);

	self->_thread_safe_.lock();
	Vector<uint8_t> cache_blob = self->driver->pipeline_cache_serialize();
	self->_thread_safe_.unlock();

	if (cache_blob.is_empty()) {
		return;
	}
	print_verbose(vformat("Updated PSO cache (%.1f MiB)", cache_blob.size() / (1024.0f * 1024.0f)));

	Ref<FileAccess> f = FileAccess::open(self->pipeline_cache_file_path, FileAccess::WRITE, nullptr);
	if (f.is_valid()) {
		f->store_buffer(cache_blob);
	}
}

template <typename T>
void RenderingDevice::_free_rids(T &p_owner, const char *p_type) {
	LocalVector<RID> owned = p_owner.get_owned_list();
	if (owned.size()) {
		if (owned.size() == 1) {
			WARN_PRINT(vformat("1 RID of type \"%s\" was leaked.", p_type));
		} else {
			WARN_PRINT(vformat("%d RIDs of type \"%s\" were leaked.", owned.size(), p_type));
		}
		for (const RID &rid : owned) {
#ifdef DEV_ENABLED
			if (resource_names.has(rid)) {
				print_line(String(" - ") + resource_names[rid]);
			}
#endif
			free_rid(rid);
		}
	}
}

void RenderingDevice::capture_timestamp(const String &p_name) {
	ERR_RENDER_THREAD_GUARD();
	ERR_FAIL_COND_MSG(_recording_error() != OK, "Recording is suspended after a frame fence wait failure.");

	ERR_FAIL_COND_MSG(draw_list.active && draw_list.state.draw_count > 0, "Capturing timestamps during draw list creation is not allowed. Offending timestamp was: " + p_name);
	ERR_FAIL_COND_MSG(compute_list.active && compute_list.state.dispatch_count > 0, "Capturing timestamps during compute list creation is not allowed. Offending timestamp was: " + p_name);
	ERR_FAIL_COND_MSG(raytracing_list.active && raytracing_list.state.trace_count > 0, "Capturing timestamps during raytracing list creation is not allowed. Offending timestamp was: " + p_name);
	ERR_FAIL_COND_MSG(frames[frame].timestamp_count >= max_timestamp_query_elements, vformat("Tried capturing more timestamps than the configured maximum (%d). You can increase this limit in the project settings under 'Debug/Settings' called 'Max Timestamp Query Elements'.", max_timestamp_query_elements));

	draw_graph.add_capture_timestamp(frames[frame].timestamp_pool, frames[frame].timestamp_count);

	frames[frame].timestamp_names[frames[frame].timestamp_count] = p_name;
	frames[frame].timestamp_cpu_values[frames[frame].timestamp_count] = OS::get_singleton()->get_ticks_usec();
	frames[frame].timestamp_count++;
}

uint64_t RenderingDevice::get_driver_resource(DriverResource p_resource, RID p_rid, uint64_t p_index) {
	ERR_RENDER_THREAD_GUARD_V(0);

#if defined(TESTS_ENABLED) && defined(MODULE_SKINNED_MESH_CAPTURE_TESTS_ENABLED)
	// A native device, queue or descriptor export bypasses buffer-local hooks.
	// Conservatively retain this loss of coverage for the whole RD lifetime.
	if (p_resource == DRIVER_RESOURCE_LOGICAL_DEVICE || p_resource == DRIVER_RESOURCE_COMMAND_QUEUE || p_resource == DRIVER_RESOURCE_TOPMOST_OBJECT || p_resource == DRIVER_RESOURCE_UNIFORM_SET) {
		capture_test_allocation_registry.mark_device_access_escaped();
	}
#endif

	MutexLock immutable_lock(_thread_safe_);
	const bool device_export = p_resource == DRIVER_RESOURCE_LOGICAL_DEVICE || p_resource == DRIVER_RESOURCE_COMMAND_QUEUE || p_resource == DRIVER_RESOURCE_TOPMOST_OBJECT || p_resource == DRIVER_RESOURCE_UNIFORM_SET;
	ERR_FAIL_COND_V(device_export && native_immutable_hold_count, 0);
	const bool native_export = p_resource != DRIVER_RESOURCE_TEXTURE_DATA_FORMAT && p_resource != DRIVER_RESOURCE_QUEUE_FAMILY && p_resource != DRIVER_RESOURCE_PHYSICAL_DEVICE;
	ERR_FAIL_COND_V(native_export && (native_immutable_resource_is_held(p_rid) || native_immutable_resource_is_retiring(p_rid)), 0);

	uint64_t driver_id = 0;
	switch (p_resource) {
		case DRIVER_RESOURCE_LOGICAL_DEVICE:
		case DRIVER_RESOURCE_PHYSICAL_DEVICE:
		case DRIVER_RESOURCE_TOPMOST_OBJECT:
			break;
		case DRIVER_RESOURCE_COMMAND_QUEUE:
			driver_id = main_queue.id;
			break;
		case DRIVER_RESOURCE_QUEUE_FAMILY:
			driver_id = main_queue_family.id;
			break;
		case DRIVER_RESOURCE_TEXTURE:
		case DRIVER_RESOURCE_TEXTURE_VIEW:
		case DRIVER_RESOURCE_TEXTURE_DATA_FORMAT: {
			Texture *tex = texture_owner.get_or_null(p_rid);
			ERR_FAIL_NULL_V(tex, 0);
			ERR_FAIL_COND_V(tex->qualification_return, 0);

			driver_id = tex->driver_id.id;
		} break;
		case DRIVER_RESOURCE_SAMPLER: {
			Sampler *sampler = sampler_owner.get_or_null(p_rid);
			ERR_FAIL_NULL_V(sampler, 0);
			ERR_FAIL_COND_V(sampler->qualification_return, 0);

			driver_id = sampler->driver_id.id;
		} break;
		case DRIVER_RESOURCE_UNIFORM_SET: {
			UniformSet *uniform_set = uniform_set_owner.get_or_null(p_rid);
			ERR_FAIL_NULL_V(uniform_set, 0);
			ERR_FAIL_COND_V(uniform_set->qualification_return || !uniform_set->qualification_dependencies.is_empty(), 0);

			driver_id = uniform_set->driver_id.id;
		} break;
		case DRIVER_RESOURCE_BUFFER: {
			Buffer *buffer = nullptr;
			if (vertex_buffer_owner.owns(p_rid)) {
				buffer = vertex_buffer_owner.get_or_null(p_rid);
			} else if (index_buffer_owner.owns(p_rid)) {
				buffer = index_buffer_owner.get_or_null(p_rid);
			} else if (uniform_buffer_owner.owns(p_rid)) {
				buffer = uniform_buffer_owner.get_or_null(p_rid);
			} else if (texture_buffer_owner.owns(p_rid)) {
				buffer = texture_buffer_owner.get_or_null(p_rid);
			} else if (storage_buffer_owner.owns(p_rid)) {
				buffer = storage_buffer_owner.get_or_null(p_rid);
			}
			ERR_FAIL_NULL_V(buffer, 0);
			ERR_FAIL_COND_V(buffer->qualification_return, 0);

#if defined(TESTS_ENABLED) && (defined(MODULE_SKINNED_MESH_CAPTURE_TESTS_ENABLED) || defined(MODULE_PARTICLES_RUNTIME_TESTS_ENABLED))
			if (buffer->capture_test_allocation_serial && !capture_test_allocation_registry.mark_access_escaped(buffer->driver_id.id, buffer->capture_test_allocation_serial)) {
				capture_test_coverage_failed.store(true);
			}
#endif
			driver_id = buffer->driver_id.id;
		} break;
		case DRIVER_RESOURCE_COMPUTE_PIPELINE: {
			ComputePipeline *compute_pipeline = compute_pipeline_owner.get_or_null(p_rid);
			ERR_FAIL_NULL_V(compute_pipeline, 0);
			ERR_FAIL_COND_V(compute_pipeline->qualification_return, 0);

			driver_id = compute_pipeline->driver_id.id;
		} break;
		case DRIVER_RESOURCE_RENDER_PIPELINE: {
			RenderPipeline *render_pipeline = render_pipeline_owner.get_or_null(p_rid);
			ERR_FAIL_NULL_V(render_pipeline, 0);

			driver_id = render_pipeline->driver_id.id;
		} break;
		default: {
			ERR_FAIL_V(0);
		} break;
	}

	const uint64_t handle = driver->get_resource_native_handle(p_resource, driver_id);
	if (handle && native_export) {
		if (device_export) {
			native_immutable_device_escaped = true;
		}
		RID escaped_rid = p_rid;
		while (escaped_rid.is_valid()) {
			bool *escaped = nullptr;
			if (_native_immutable_counter(escaped_rid, nullptr, &escaped)) {
				*escaped = true;
			}
			// A texture view exports the same mutable physical image as its
			// original parent. Lost coverage belongs to that backing, including
			// sibling views reached later through their actual owner closure.
			const auto *texture = texture_owner.owns(escaped_rid) ? texture_owner.get_or_null(escaped_rid) : nullptr;
			escaped_rid = texture ? texture->owner : RID();
		}
	}
	return handle;
}

String RenderingDevice::get_driver_and_device_memory_report() const {
	return context->get_driver_and_device_memory_report();
}

String RenderingDevice::get_tracked_object_name(uint32_t p_type_index) const {
	return context->get_tracked_object_name(p_type_index);
}

uint64_t RenderingDevice::get_tracked_object_type_count() const {
	return context->get_tracked_object_type_count();
}

uint64_t RenderingDevice::get_driver_total_memory() const {
	return context->get_driver_total_memory();
}

uint64_t RenderingDevice::get_driver_allocation_count() const {
	return context->get_driver_allocation_count();
}

uint64_t RenderingDevice::get_driver_memory_by_object_type(uint32_t p_type) const {
	return context->get_driver_memory_by_object_type(p_type);
}

uint64_t RenderingDevice::get_driver_allocs_by_object_type(uint32_t p_type) const {
	return context->get_driver_allocs_by_object_type(p_type);
}

uint64_t RenderingDevice::get_device_total_memory() const {
	return context->get_device_total_memory();
}

uint64_t RenderingDevice::get_device_allocation_count() const {
	return context->get_device_allocation_count();
}

uint64_t RenderingDevice::get_device_memory_by_object_type(uint32_t type) const {
	return context->get_device_memory_by_object_type(type);
}

uint64_t RenderingDevice::get_device_allocs_by_object_type(uint32_t type) const {
	return context->get_device_allocs_by_object_type(type);
}

uint32_t RenderingDevice::get_captured_timestamps_count() const {
	ERR_RENDER_THREAD_GUARD_V(0);
	return frames[frame].timestamp_result_count;
}

uint64_t RenderingDevice::get_captured_timestamps_frame() const {
	ERR_RENDER_THREAD_GUARD_V(0);
	return frames[frame].index;
}

uint64_t RenderingDevice::get_captured_timestamp_gpu_time(uint32_t p_index) const {
	ERR_RENDER_THREAD_GUARD_V(0);
	ERR_FAIL_UNSIGNED_INDEX_V(p_index, frames[frame].timestamp_result_count, 0);
	return driver->timestamp_query_result_to_time(frames[frame].timestamp_result_values[p_index]);
}

uint64_t RenderingDevice::get_captured_timestamp_cpu_time(uint32_t p_index) const {
	ERR_RENDER_THREAD_GUARD_V(0);
	ERR_FAIL_UNSIGNED_INDEX_V(p_index, frames[frame].timestamp_result_count, 0);
	return frames[frame].timestamp_cpu_result_values[p_index];
}

String RenderingDevice::get_captured_timestamp_name(uint32_t p_index) const {
	ERR_FAIL_UNSIGNED_INDEX_V(p_index, frames[frame].timestamp_result_count, String());
	return frames[frame].timestamp_result_names[p_index];
}

uint64_t RenderingDevice::limit_get(Limit p_limit) const {
	return driver->limit_get(p_limit);
}

void RenderingDevice::_finalize_on_rendering_server_thread() {
	finalize();
	// A void finalizer can return early after a failed invariant. Do not let the
	// actual RT exit and hand an unresolved/native-destroyed object to main.
	GDExtensionDeviceExit::require_close_success(native_finalization_returned ? OK : ERR_BUSY, GDExtensionDeviceExit::CloseRoute::LAST_REF);
}

Error RenderingDevice::_begin_production_finalization() {
	{
		MutexLock immutable_lock(_thread_safe_);
		production_finalization_started.store(true, std::memory_order_release);
		if (native_immutable_hold_count || native_immutable_free_frame || !resource_frees_in_progress.is_empty()) {
			return ERR_BUSY;
		}
	}
	auto *registry = production_entity_registry.load(std::memory_order_acquire);
	return (registry ? *registry : GDExtensionDeviceExit::Registry::get_singleton()).device_begin_finalization(this);
}

void RenderingDevice::finalize() {
	ERR_RENDER_THREAD_GUARD();
	native_height_finalization_started = true;
	const Error entity_return = _begin_production_finalization();
	ERR_FAIL_COND_MSG(entity_return != OK, "RenderingDevice native finalization is blocked by an original production entity obligation.");

	if (!native_height_finalization_drained) {
		if (!frames.is_empty()) {
			// Wait for all frames to have finished rendering.
			if (_flush_and_stall_for_all_frames(false) != OK) {
				CRASH_NOW_MSG("Frame fence wait failed during destructive RenderingDevice finalization; dependent resources were not released.");
			}
		}

		// Wait for transfer workers to finish.
		_submit_transfer_workers();
		if (!_wait_for_transfer_workers()) {
			CRASH_NOW_MSG("Transfer worker completion failed during destructive RenderingDevice finalization; dependent resources were not released.");
		}
		native_height_finalization_drained = true;
	}
	// Returning an observer is distinct from successful frame/worker drain.
	// BUSY preserves this post-drain phase, all cells, and the live driver.
	const Error height_return = _native_height_ranges_prepare_finalization();
	ERR_FAIL_COND_MSG(height_return != OK, "RenderingDevice finalization is blocked by an original native height range obligation.");

	// The original owners remain authoritative while deferred roots are pending.
	// All transfer workers are complete here, so retry through the ordinary free
	// path before the final owner sweep, retaining any original refusal.
	{
		_qualification_retry_deferred_frees();
		ERR_FAIL_COND_MSG(!qualification_deferred_frees.is_empty(), "Deferred resource return is still blocked during RenderingDevice finalization.");
	}

	// Delete everything the graph has created.
	draw_graph.finalize();

	// Free all resources.
	_free_rids(render_pipeline_owner, "Pipeline");
	_free_rids(compute_pipeline_owner, "Compute");
	_free_rids(raytracing_pipeline_owner, "RaytracingPipeline");
	_free_rids(acceleration_structure_owner, "AccelerationStructure");
	_free_rids(hit_sbt_owner, "HitShaderBindingTable");
	_free_rids(uniform_set_owner, "UniformSet");
	_free_rids(texture_buffer_owner, "TextureBuffer");
	_free_rids(storage_buffer_owner, "StorageBuffer");
	_free_rids(uniform_buffer_owner, "UniformBuffer");
	_free_rids(shader_owner, "Shader");
	_free_rids(index_array_owner, "IndexArray");
	_free_rids(index_buffer_owner, "IndexBuffer");
	_free_rids(vertex_array_owner, "VertexArray");
	_free_rids(vertex_buffer_owner, "VertexBuffer");
	_free_rids(framebuffer_owner, "Framebuffer");
	_free_rids(sampler_owner, "Sampler");
	{
		// For textures it's a bit more difficult because they may be shared.
		LocalVector<RID> owned = texture_owner.get_owned_list();
		if (owned.size()) {
			if (owned.size() == 1) {
				WARN_PRINT("1 RID of type \"Texture\" was leaked.");
			} else {
				WARN_PRINT(vformat("%d RIDs of type \"Texture\" were leaked.", owned.size()));
			}
			LocalVector<RID> owned_non_shared;
			// Free shared first.
			for (const RID &texture_rid : owned) {
				if (texture_is_shared(texture_rid)) {
#ifdef DEV_ENABLED
					if (resource_names.has(texture_rid)) {
						print_line(String(" - ") + resource_names[texture_rid]);
					}
#endif
					free_rid(texture_rid);
				} else {
					owned_non_shared.push_back(texture_rid);
				}
			}
			// Free non shared second, this will avoid an error trying to free unexisting textures due to dependencies.
			for (const RID &texture_rid : owned_non_shared) {
#ifdef DEV_ENABLED
				if (resource_names.has(texture_rid)) {
					print_line(String(" - ") + resource_names[texture_rid]);
				}
#endif
				free_rid(texture_rid);
			}
		}
	}

	// Erase the transfer workers after all resources have been freed.
	_free_transfer_workers();

	// Free everything pending.
	for (uint32_t i = 0; i < frames.size(); i++) {
		int f = (frame + i) % frames.size();
		_free_pending_resources(f);
		driver->command_pool_free(frames[i].command_pool);
		driver->timestamp_query_pool_free(frames[i].timestamp_pool);
		driver->semaphore_free(frames[i].semaphore);
		driver->fence_free(frames[i].fence);

		RDG::CommandBufferPool &buffer_pool = frames[i].command_buffer_pool;
		for (uint32_t j = 0; j < buffer_pool.buffers.size(); j++) {
			driver->semaphore_free(buffer_pool.semaphores[j]);
		}

		for (uint32_t j = 0; j < frames[i].transfer_worker_semaphores.size(); j++) {
			driver->semaphore_free(frames[i].transfer_worker_semaphores[j]);
		}
	}

	if (pipeline_cache_enabled) {
		update_pipeline_cache(true);
		driver->pipeline_cache_free();
	}

	frames.clear();

	for (int i = 0; i < upload_staging_buffers.blocks.size(); i++) {
		driver->buffer_unmap(upload_staging_buffers.blocks[i].driver_id);
		driver->buffer_free(upload_staging_buffers.blocks[i].driver_id);
	}

	for (int i = 0; i < download_staging_buffers.blocks.size(); i++) {
		driver->buffer_unmap(download_staging_buffers.blocks[i].driver_id);
		driver->buffer_free(download_staging_buffers.blocks[i].driver_id);
	}

	while (vertex_formats.size()) {
		HashMap<VertexFormatID, VertexDescriptionCache>::Iterator temp = vertex_formats.begin();
		driver->vertex_format_free(temp->value.driver_id);
		vertex_formats.remove(temp);
	}

	for (KeyValue<FramebufferFormatID, FramebufferFormat> &E : framebuffer_formats) {
		driver->render_pass_free(E.value.render_pass);
	}
	framebuffer_formats.clear();

	// Delete the swap chains created for the screens.
	for (KeyValue<DisplayServerEnums::WindowID, RDD::SwapChainID> &it : screen_swap_chains) {
		driver->swap_chain_free(it.value);
		// Keep the original window key through DisplayServer's later close. Zero
		// is written only after this driver's free returned, never on RID erasure.
		it.value = RDD::SwapChainID();
	}

	// Delete the command queues.
	if (present_queue) {
		if (main_queue != present_queue) {
			// Only delete the present queue if it's unique.
			driver->command_queue_free(present_queue);
		}

		present_queue = RDD::CommandQueueID();
	}

	if (transfer_queue) {
		if (main_queue != transfer_queue) {
			// Only delete the transfer queue if it's unique.
			driver->command_queue_free(transfer_queue);
		}

		transfer_queue = RDD::CommandQueueID();
	}

	if (main_queue) {
		driver->command_queue_free(main_queue);
		main_queue = RDD::CommandQueueID();
	}

	// Delete the driver once everything else has been deleted.
	if (driver != nullptr) {
		context->driver_free(driver);
		driver = nullptr;
	}

	// All these should be clear at this point.
	ERR_FAIL_COND(dependency_map.size());
	ERR_FAIL_COND(reverse_dependency_map.size());
	native_finalization_returned = true;
}

void RenderingDevice::_set_max_fps(int p_max_fps) {
	for (const KeyValue<DisplayServerEnums::WindowID, RDD::SwapChainID> &it : screen_swap_chains) {
		driver->swap_chain_set_max_fps(it.value, p_max_fps);
	}
}

RenderingDevice *RenderingDevice::create_local_device() {
	RenderingDevice *rd = memnew(RenderingDevice);
	if (rd->initialize(context) != OK) {
		memdelete(rd);
		return nullptr;
	}
	return rd;
}

bool RenderingDevice::has_feature(const Features p_feature) const {
	// Some features can be deduced from the capabilities without querying the driver and looking at the capabilities.
	switch (p_feature) {
		case SUPPORTS_MULTIVIEW: {
			const RDD::MultiviewCapabilities &multiview_capabilities = driver->get_multiview_capabilities();
			return multiview_capabilities.is_supported && multiview_capabilities.max_view_count > 1;
		}
		case SUPPORTS_ATTACHMENT_VRS: {
			const RDD::FragmentShadingRateCapabilities &fsr_capabilities = driver->get_fragment_shading_rate_capabilities();
			const RDD::FragmentDensityMapCapabilities &fdm_capabilities = driver->get_fragment_density_map_capabilities();
			return fsr_capabilities.attachment_supported || fdm_capabilities.attachment_supported;
		}
		default:
			return driver->has_feature(p_feature);
	}
}

void RenderingDevice::_bind_methods() {
	ClassDB::bind_method(D_METHOD("texture_create", "format", "view", "data"), &RenderingDevice::_texture_create, DEFVAL(Array()));
	ClassDB::bind_method(D_METHOD("texture_create_shared", "view", "with_texture"), &RenderingDevice::_texture_create_shared);
	ClassDB::bind_method(D_METHOD("texture_create_shared_from_slice", "view", "with_texture", "layer", "mipmap", "mipmaps", "slice_type"), &RenderingDevice::_texture_create_shared_from_slice, DEFVAL(1), DEFVAL(TEXTURE_SLICE_2D));
	ClassDB::bind_method(D_METHOD("texture_create_from_extension", "type", "format", "samples", "usage_flags", "image", "width", "height", "depth", "layers", "mipmaps"), &RenderingDevice::texture_create_from_extension, DEFVAL(1));

	ClassDB::bind_method(D_METHOD("texture_update", "texture", "layer", "data"), &RenderingDevice::texture_update);
	ClassDB::bind_method(D_METHOD("texture_get_data", "texture", "layer"), &RenderingDevice::texture_get_data);
	ClassDB::bind_method(D_METHOD("texture_get_data_async", "texture", "layer", "callback"), &RenderingDevice::texture_get_data_async);

	ClassDB::bind_method(D_METHOD("texture_is_format_supported_for_usage", "format", "usage_flags"), &RenderingDevice::texture_is_format_supported_for_usage);

	ClassDB::bind_method(D_METHOD("texture_is_shared", "texture"), &RenderingDevice::texture_is_shared);
	ClassDB::bind_method(D_METHOD("texture_is_valid", "texture"), &RenderingDevice::texture_is_valid);

	ClassDB::bind_method(D_METHOD("texture_set_discardable", "texture", "discardable"), &RenderingDevice::texture_set_discardable);
	ClassDB::bind_method(D_METHOD("texture_is_discardable", "texture"), &RenderingDevice::texture_is_discardable);

	ClassDB::bind_method(D_METHOD("texture_copy", "from_texture", "to_texture", "from_pos", "to_pos", "size", "src_mipmap", "dst_mipmap", "src_layer", "dst_layer"), &RenderingDevice::texture_copy);
	ClassDB::bind_method(D_METHOD("texture_clear", "texture", "color", "base_mipmap", "mipmap_count", "base_layer", "layer_count"), &RenderingDevice::texture_clear);
	ClassDB::bind_method(D_METHOD("texture_resolve_multisample", "from_texture", "to_texture"), &RenderingDevice::texture_resolve_multisample);

	ClassDB::bind_method(D_METHOD("texture_get_format", "texture"), &RenderingDevice::_texture_get_format);
#ifndef DISABLE_DEPRECATED
	ClassDB::bind_method(D_METHOD("texture_get_native_handle", "texture"), &RenderingDevice::texture_get_native_handle);
#endif

	ClassDB::bind_method(D_METHOD("framebuffer_format_create", "attachments", "view_count"), &RenderingDevice::_framebuffer_format_create, DEFVAL(1));
	ClassDB::bind_method(D_METHOD("framebuffer_format_create_multipass", "attachments", "passes", "view_count"), &RenderingDevice::_framebuffer_format_create_multipass, DEFVAL(1));
	ClassDB::bind_method(D_METHOD("framebuffer_format_create_empty", "samples"), &RenderingDevice::framebuffer_format_create_empty, DEFVAL(TEXTURE_SAMPLES_1));
	ClassDB::bind_method(D_METHOD("framebuffer_format_get_texture_samples", "format", "render_pass"), &RenderingDevice::framebuffer_format_get_texture_samples, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("framebuffer_create", "textures", "validate_with_format", "view_count"), &RenderingDevice::_framebuffer_create, DEFVAL(INVALID_FORMAT_ID), DEFVAL(1));
	ClassDB::bind_method(D_METHOD("framebuffer_create_multipass", "textures", "passes", "validate_with_format", "view_count"), &RenderingDevice::_framebuffer_create_multipass, DEFVAL(INVALID_FORMAT_ID), DEFVAL(1));
	ClassDB::bind_method(D_METHOD("framebuffer_create_empty", "size", "samples", "validate_with_format"), &RenderingDevice::framebuffer_create_empty, DEFVAL(TEXTURE_SAMPLES_1), DEFVAL(INVALID_FORMAT_ID));
	ClassDB::bind_method(D_METHOD("framebuffer_get_format", "framebuffer"), &RenderingDevice::framebuffer_get_format);
	ClassDB::bind_method(D_METHOD("framebuffer_is_valid", "framebuffer"), &RenderingDevice::framebuffer_is_valid);

	ClassDB::bind_method(D_METHOD("sampler_create", "state"), &RenderingDevice::_sampler_create);
	ClassDB::bind_method(D_METHOD("sampler_is_format_supported_for_filter", "format", "sampler_filter"), &RenderingDevice::sampler_is_format_supported_for_filter);

	ClassDB::bind_method(D_METHOD("vertex_buffer_create", "size_bytes", "data", "creation_bits"), &RenderingDevice::_vertex_buffer_create, DEFVAL(Vector<uint8_t>()), DEFVAL(0));
	ClassDB::bind_method(D_METHOD("vertex_format_create", "vertex_descriptions"), &RenderingDevice::_vertex_format_create);
	ClassDB::bind_method(D_METHOD("vertex_array_create", "vertex_count", "vertex_format", "src_buffers", "offsets"), &RenderingDevice::_vertex_array_create, DEFVAL(Vector<int64_t>()));

	ClassDB::bind_method(D_METHOD("index_buffer_create", "size_indices", "format", "data", "use_restart_indices", "creation_bits"), &RenderingDevice::_index_buffer_create, DEFVAL(Vector<uint8_t>()), DEFVAL(false), DEFVAL(0));
	ClassDB::bind_method(D_METHOD("index_array_create", "index_buffer", "index_offset", "index_count"), &RenderingDevice::index_array_create);

	ClassDB::bind_method(D_METHOD("shader_compile_spirv_from_source", "shader_source", "allow_cache"), &RenderingDevice::_shader_compile_spirv_from_source, DEFVAL(true));
	BIND_BITFIELD_FLAG(SHADER_COMPILE_FLAG_DISABLE_FAST_MATH);
	ClassDB::bind_method(D_METHOD("shader_compile_binary_from_spirv_with_options", "spirv", "shader_name", "flags"), &RenderingDevice::_shader_compile_binary_from_spirv_with_options);
	ClassDB::bind_method(D_METHOD("shader_create_from_spirv_with_options", "spirv", "shader_name", "flags"), &RenderingDevice::_shader_create_from_spirv_with_options);
	ClassDB::bind_method(D_METHOD("shader_get_compile_flags", "shader"), &RenderingDevice::shader_get_compile_flags);
	ClassDB::bind_method(D_METHOD("shader_compile_binary_from_spirv", "spirv_data", "name"), &RenderingDevice::_shader_compile_binary_from_spirv, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("shader_create_from_spirv", "spirv_data", "name"), &RenderingDevice::_shader_create_from_spirv, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("shader_create_from_bytecode", "binary_data", "placeholder_rid"), &RenderingDevice::shader_create_from_bytecode, DEFVAL(RID()));
	ClassDB::bind_method(D_METHOD("shader_create_placeholder"), &RenderingDevice::shader_create_placeholder);

	ClassDB::bind_method(D_METHOD("shader_get_vertex_input_attribute_mask", "shader"), &RenderingDevice::shader_get_vertex_input_attribute_mask);

	ClassDB::bind_method(D_METHOD("uniform_buffer_create", "size_bytes", "data", "creation_bits"), &RenderingDevice::_uniform_buffer_create, DEFVAL(Vector<uint8_t>()), DEFVAL(0));
	ClassDB::bind_method(D_METHOD("storage_buffer_create", "size_bytes", "data", "usage", "creation_bits"), &RenderingDevice::_storage_buffer_create, DEFVAL(Vector<uint8_t>()), DEFVAL(0), DEFVAL(0));
	ClassDB::bind_method(D_METHOD("texture_buffer_create", "size_bytes", "format", "data"), &RenderingDevice::_texture_buffer_create, DEFVAL(Vector<uint8_t>()));

	ClassDB::bind_method(D_METHOD("uniform_set_create", "uniforms", "shader", "shader_set"), &RenderingDevice::_uniform_set_create);
	ClassDB::bind_method(D_METHOD("uniform_set_is_valid", "uniform_set"), &RenderingDevice::uniform_set_is_valid);

	ClassDB::bind_method(D_METHOD("buffer_copy", "src_buffer", "dst_buffer", "src_offset", "dst_offset", "size"), &RenderingDevice::buffer_copy);
	ClassDB::bind_method(D_METHOD("buffer_update", "buffer", "offset", "size_bytes", "data"), &RenderingDevice::_buffer_update_bind);
	ClassDB::bind_method(D_METHOD("buffer_clear", "buffer", "offset", "size_bytes"), &RenderingDevice::buffer_clear);
	ClassDB::bind_method(D_METHOD("buffer_get_data", "buffer", "offset_bytes", "size_bytes"), &RenderingDevice::buffer_get_data, DEFVAL(0), DEFVAL(0));
	ClassDB::bind_method(D_METHOD("buffer_get_data_async", "buffer", "callback", "offset_bytes", "size_bytes"), &RenderingDevice::buffer_get_data_async, DEFVAL(0), DEFVAL(0));
	ClassDB::bind_method(D_METHOD("buffer_get_device_address", "buffer"), &RenderingDevice::buffer_get_device_address);

	ClassDB::bind_method(D_METHOD("render_pipeline_create", "shader", "framebuffer_format", "vertex_format", "primitive", "rasterization_state", "multisample_state", "stencil_state", "color_blend_state", "dynamic_state_flags", "for_render_pass", "specialization_constants"), &RenderingDevice::_render_pipeline_create, DEFVAL(0), DEFVAL(0), DEFVAL(TypedArray<RDPipelineSpecializationConstant>()));
	ClassDB::bind_method(D_METHOD("render_pipeline_is_valid", "render_pipeline"), &RenderingDevice::render_pipeline_is_valid);

	ClassDB::bind_method(D_METHOD("compute_pipeline_create", "shader", "specialization_constants"), &RenderingDevice::_compute_pipeline_create, DEFVAL(TypedArray<RDPipelineSpecializationConstant>()));
	ClassDB::bind_method(D_METHOD("compute_pipeline_is_valid", "compute_pipeline"), &RenderingDevice::compute_pipeline_is_valid);

	ClassDB::bind_method(D_METHOD("raytracing_pipeline_create", "raygen_shaders", "miss_shaders", "hit_groups", "max_trace_recursion_depth"), &RenderingDevice::_raytracing_pipeline_create);
	ClassDB::bind_method(D_METHOD("raytracing_pipeline_is_valid", "raytracing_pipeline"), &RenderingDevice::raytracing_pipeline_is_valid);

	ClassDB::bind_method(D_METHOD("blas_create", "geometries", "flags"), &RenderingDevice::_blas_create);
	ClassDB::bind_method(D_METHOD("tlas_create", "max_instance_count", "flags"), &RenderingDevice::tlas_create);
	ClassDB::bind_method(D_METHOD("blas_build", "blas"), &RenderingDevice::blas_build);
	ClassDB::bind_method(D_METHOD("tlas_build", "tlas", "instances"), &RenderingDevice::_tlas_build);

	ClassDB::bind_method(D_METHOD("hit_sbt_create", "raytracing_pipeline", "initial_hit_group_capacity"), &RenderingDevice::hit_sbt_create);
	ClassDB::bind_method(D_METHOD("hit_sbt_set_pipeline", "hit_sbt", "raytracing_pipeline"), &RenderingDevice::hit_sbt_set_pipeline);

	ClassDB::bind_method(D_METHOD("hit_sbt_range_alloc", "hit_sbt", "hit_group_count"), &RenderingDevice::hit_sbt_range_alloc);
	ClassDB::bind_method(D_METHOD("hit_sbt_range_free", "hit_sbt", "range"), &RenderingDevice::hit_sbt_range_free);
	ClassDB::bind_method(D_METHOD("hit_sbt_range_update", "hit_sbt", "range", "offset", "hit_group_indices"), &RenderingDevice::_hit_sbt_range_update);

	ClassDB::bind_method(D_METHOD("screen_get_width", "screen"), &RenderingDevice::screen_get_width, DEFVAL(DisplayServerEnums::MAIN_WINDOW_ID));
	ClassDB::bind_method(D_METHOD("screen_get_height", "screen"), &RenderingDevice::screen_get_height, DEFVAL(DisplayServerEnums::MAIN_WINDOW_ID));
	ClassDB::bind_method(D_METHOD("screen_get_framebuffer_format", "screen"), &RenderingDevice::screen_get_framebuffer_format, DEFVAL(DisplayServerEnums::MAIN_WINDOW_ID));

	ClassDB::bind_method(D_METHOD("draw_list_begin_for_screen", "screen", "clear_color"), &RenderingDevice::draw_list_begin_for_screen, DEFVAL(DisplayServerEnums::MAIN_WINDOW_ID), DEFVAL(Color()));

	ClassDB::bind_method(D_METHOD("draw_list_begin", "framebuffer", "draw_flags", "clear_color_values", "clear_depth_value", "clear_stencil_value", "region", "breadcrumb"), &RenderingDevice::_draw_list_begin_bind, DEFVAL(DRAW_DEFAULT_ALL), DEFVAL(Vector<Color>()), DEFVAL(1.0), DEFVAL(0), DEFVAL(Rect2()), DEFVAL(0));
#ifndef DISABLE_DEPRECATED
	ClassDB::bind_method(D_METHOD("draw_list_begin_split", "framebuffer", "splits", "initial_color_action", "final_color_action", "initial_depth_action", "final_depth_action", "clear_color_values", "clear_depth", "clear_stencil", "region", "storage_textures"), &RenderingDevice::_draw_list_begin_split, DEFVAL(Vector<Color>()), DEFVAL(1.0), DEFVAL(0), DEFVAL(Rect2()), DEFVAL(TypedArray<RID>()));
#endif

	ClassDB::bind_method(D_METHOD("draw_list_set_blend_constants", "draw_list", "color"), &RenderingDevice::draw_list_set_blend_constants);
	ClassDB::bind_method(D_METHOD("draw_list_bind_render_pipeline", "draw_list", "render_pipeline"), &RenderingDevice::draw_list_bind_render_pipeline);
	ClassDB::bind_method(D_METHOD("draw_list_bind_uniform_set", "draw_list", "uniform_set", "set_index"), &RenderingDevice::draw_list_bind_uniform_set);
	ClassDB::bind_method(D_METHOD("draw_list_bind_vertex_array", "draw_list", "vertex_array"), &RenderingDevice::draw_list_bind_vertex_array);
	ClassDB::bind_method(D_METHOD("draw_list_bind_vertex_buffers_format", "draw_list", "vertex_format", "vertex_count", "vertex_buffers", "offsets"), &RenderingDevice::_draw_list_bind_vertex_buffers_format, DEFVAL(Vector<int64_t>()));
	ClassDB::bind_method(D_METHOD("draw_list_bind_index_array", "draw_list", "index_array"), &RenderingDevice::draw_list_bind_index_array);
	ClassDB::bind_method(D_METHOD("draw_list_set_push_constant", "draw_list", "buffer", "size_bytes"), &RenderingDevice::_draw_list_set_push_constant);

	ClassDB::bind_method(D_METHOD("draw_list_draw", "draw_list", "use_indices", "instances", "procedural_vertex_count"), &RenderingDevice::draw_list_draw, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("draw_list_draw_indirect", "draw_list", "use_indices", "buffer", "offset", "draw_count", "stride"), &RenderingDevice::draw_list_draw_indirect, DEFVAL(0), DEFVAL(1), DEFVAL(0));

	ClassDB::bind_method(D_METHOD("draw_list_enable_scissor", "draw_list", "rect"), &RenderingDevice::draw_list_enable_scissor, DEFVAL(Rect2()));
	ClassDB::bind_method(D_METHOD("draw_list_disable_scissor", "draw_list"), &RenderingDevice::draw_list_disable_scissor);

	ClassDB::bind_method(D_METHOD("draw_list_switch_to_next_pass"), &RenderingDevice::draw_list_switch_to_next_pass);
#ifndef DISABLE_DEPRECATED
	ClassDB::bind_method(D_METHOD("draw_list_switch_to_next_pass_split", "splits"), &RenderingDevice::_draw_list_switch_to_next_pass_split);
#endif

	ClassDB::bind_method(D_METHOD("draw_list_end"), &RenderingDevice::draw_list_end);

	ClassDB::bind_method(D_METHOD("compute_list_begin"), &RenderingDevice::compute_list_begin);
	ClassDB::bind_method(D_METHOD("compute_list_bind_compute_pipeline", "compute_list", "compute_pipeline"), &RenderingDevice::compute_list_bind_compute_pipeline);
	ClassDB::bind_method(D_METHOD("compute_list_set_push_constant", "compute_list", "buffer", "size_bytes"), &RenderingDevice::_compute_list_set_push_constant);
	ClassDB::bind_method(D_METHOD("compute_list_bind_uniform_set", "compute_list", "uniform_set", "set_index"), &RenderingDevice::compute_list_bind_uniform_set);
	ClassDB::bind_method(D_METHOD("compute_list_dispatch", "compute_list", "x_groups", "y_groups", "z_groups"), &RenderingDevice::compute_list_dispatch);
	ClassDB::bind_method(D_METHOD("compute_list_dispatch_indirect", "compute_list", "buffer", "offset"), &RenderingDevice::compute_list_dispatch_indirect);
	ClassDB::bind_method(D_METHOD("compute_list_add_barrier", "compute_list"), &RenderingDevice::compute_list_add_barrier);
	ClassDB::bind_method(D_METHOD("compute_list_end"), &RenderingDevice::compute_list_end);

	ClassDB::bind_method(D_METHOD("raytracing_list_begin"), &RenderingDevice::raytracing_list_begin);
	ClassDB::bind_method(D_METHOD("raytracing_list_bind_raytracing_pipeline", "raytracing_list", "raytracing_pipeline"), &RenderingDevice::raytracing_list_bind_raytracing_pipeline);
	ClassDB::bind_method(D_METHOD("raytracing_list_set_push_constant", "raytracing_list", "buffer", "size_bytes"), &RenderingDevice::_raytracing_list_set_push_constant);
	ClassDB::bind_method(D_METHOD("raytracing_list_bind_uniform_set", "raytracing_list", "uniform_set", "set_index"), &RenderingDevice::raytracing_list_bind_uniform_set);
	ClassDB::bind_method(D_METHOD("raytracing_list_trace_rays", "raytracing_list", "raygen_shader_index", "hit_sbt", "width", "height", "depth"), &RenderingDevice::raytracing_list_trace_rays);
	ClassDB::bind_method(D_METHOD("raytracing_list_end"), &RenderingDevice::raytracing_list_end);

	ClassDB::bind_method(D_METHOD("free_rid", "rid"), &RenderingDevice::free_rid);

	ClassDB::bind_method(D_METHOD("capture_timestamp", "name"), &RenderingDevice::capture_timestamp);
	ClassDB::bind_method(D_METHOD("get_captured_timestamps_count"), &RenderingDevice::get_captured_timestamps_count);
	ClassDB::bind_method(D_METHOD("get_captured_timestamps_frame"), &RenderingDevice::get_captured_timestamps_frame);
	ClassDB::bind_method(D_METHOD("get_captured_timestamp_gpu_time", "index"), &RenderingDevice::get_captured_timestamp_gpu_time);
	ClassDB::bind_method(D_METHOD("get_captured_timestamp_cpu_time", "index"), &RenderingDevice::get_captured_timestamp_cpu_time);
	ClassDB::bind_method(D_METHOD("get_captured_timestamp_name", "index"), &RenderingDevice::get_captured_timestamp_name);

	ClassDB::bind_method(D_METHOD("has_feature", "feature"), &RenderingDevice::has_feature);
	ClassDB::bind_method(D_METHOD("limit_get", "limit"), &RenderingDevice::limit_get);
	ClassDB::bind_method(D_METHOD("get_frame_delay"), &RenderingDevice::get_frame_delay);
	ClassDB::bind_method(D_METHOD("submit"), &RenderingDevice::submit);
	ClassDB::bind_method(D_METHOD("sync"), &RenderingDevice::sync);

#ifndef DISABLE_DEPRECATED
	ClassDB::bind_method(D_METHOD("barrier", "from", "to"), &RenderingDevice::barrier, DEFVAL(BARRIER_MASK_ALL_BARRIERS), DEFVAL(BARRIER_MASK_ALL_BARRIERS));
	ClassDB::bind_method(D_METHOD("full_barrier"), &RenderingDevice::full_barrier);
#endif

	ClassDB::bind_method(D_METHOD("create_local_device"), &RenderingDevice::create_local_device);

	ClassDB::bind_method(D_METHOD("set_resource_name", "id", "name"), &RenderingDevice::set_resource_name);

	ClassDB::bind_method(D_METHOD("draw_command_begin_label", "name", "color"), &RenderingDevice::_draw_command_begin_label);
#ifndef DISABLE_DEPRECATED
	ClassDB::bind_method(D_METHOD("draw_command_insert_label", "name", "color"), &RenderingDevice::draw_command_insert_label);
#endif
	ClassDB::bind_method(D_METHOD("draw_command_end_label"), &RenderingDevice::draw_command_end_label);

	ClassDB::bind_method(D_METHOD("get_device_vendor_name"), &RenderingDevice::get_device_vendor_name);
	ClassDB::bind_method(D_METHOD("get_device_name"), &RenderingDevice::get_device_name);
	ClassDB::bind_method(D_METHOD("get_device_pipeline_cache_uuid"), &RenderingDevice::get_device_pipeline_cache_uuid);

	ClassDB::bind_method(D_METHOD("get_memory_usage", "type"), &RenderingDevice::get_memory_usage);

	ClassDB::bind_method(D_METHOD("get_driver_resource", "resource", "rid", "index"), &RenderingDevice::get_driver_resource);

	ClassDB::bind_method(D_METHOD("get_perf_report"), &RenderingDevice::get_perf_report);

	ClassDB::bind_method(D_METHOD("get_driver_and_device_memory_report"), &RenderingDevice::get_driver_and_device_memory_report);
	ClassDB::bind_method(D_METHOD("get_tracked_object_name", "type_index"), &RenderingDevice::get_tracked_object_name);
	ClassDB::bind_method(D_METHOD("get_tracked_object_type_count"), &RenderingDevice::get_tracked_object_type_count);
	ClassDB::bind_method(D_METHOD("get_driver_total_memory"), &RenderingDevice::get_driver_total_memory);
	ClassDB::bind_method(D_METHOD("get_driver_allocation_count"), &RenderingDevice::get_driver_allocation_count);
	ClassDB::bind_method(D_METHOD("get_driver_memory_by_object_type", "type"), &RenderingDevice::get_driver_memory_by_object_type);
	ClassDB::bind_method(D_METHOD("get_driver_allocs_by_object_type", "type"), &RenderingDevice::get_driver_allocs_by_object_type);
	ClassDB::bind_method(D_METHOD("get_device_total_memory"), &RenderingDevice::get_device_total_memory);
	ClassDB::bind_method(D_METHOD("get_device_allocation_count"), &RenderingDevice::get_device_allocation_count);
	ClassDB::bind_method(D_METHOD("get_device_memory_by_object_type", "type"), &RenderingDevice::get_device_memory_by_object_type);
	ClassDB::bind_method(D_METHOD("get_device_allocs_by_object_type", "type"), &RenderingDevice::get_device_allocs_by_object_type);

	BIND_ENUM_CONSTANT(RenderingDeviceEnums::DEVICE_TYPE_OTHER);
	BIND_ENUM_CONSTANT(RenderingDeviceEnums::DEVICE_TYPE_INTEGRATED_GPU);
	BIND_ENUM_CONSTANT(RenderingDeviceEnums::DEVICE_TYPE_DISCRETE_GPU);
	BIND_ENUM_CONSTANT(RenderingDeviceEnums::DEVICE_TYPE_VIRTUAL_GPU);
	BIND_ENUM_CONSTANT(RenderingDeviceEnums::DEVICE_TYPE_CPU);
	BIND_ENUM_CONSTANT(RenderingDeviceEnums::DEVICE_TYPE_MAX);

	BIND_ENUM_CONSTANT(DRIVER_RESOURCE_LOGICAL_DEVICE);
	BIND_ENUM_CONSTANT(DRIVER_RESOURCE_PHYSICAL_DEVICE);
	BIND_ENUM_CONSTANT(DRIVER_RESOURCE_TOPMOST_OBJECT);
	BIND_ENUM_CONSTANT(DRIVER_RESOURCE_COMMAND_QUEUE);
	BIND_ENUM_CONSTANT(DRIVER_RESOURCE_QUEUE_FAMILY);
	BIND_ENUM_CONSTANT(DRIVER_RESOURCE_TEXTURE);
	BIND_ENUM_CONSTANT(DRIVER_RESOURCE_TEXTURE_VIEW);
	BIND_ENUM_CONSTANT(DRIVER_RESOURCE_TEXTURE_DATA_FORMAT);
	BIND_ENUM_CONSTANT(DRIVER_RESOURCE_SAMPLER);
	BIND_ENUM_CONSTANT(DRIVER_RESOURCE_UNIFORM_SET);
	BIND_ENUM_CONSTANT(DRIVER_RESOURCE_BUFFER);
	BIND_ENUM_CONSTANT(DRIVER_RESOURCE_COMPUTE_PIPELINE);
	BIND_ENUM_CONSTANT(DRIVER_RESOURCE_RENDER_PIPELINE);
#ifndef DISABLE_DEPRECATED
	BIND_ENUM_CONSTANT(DRIVER_RESOURCE_VULKAN_DEVICE);
	BIND_ENUM_CONSTANT(DRIVER_RESOURCE_VULKAN_PHYSICAL_DEVICE);
	BIND_ENUM_CONSTANT(DRIVER_RESOURCE_VULKAN_INSTANCE);
	BIND_ENUM_CONSTANT(DRIVER_RESOURCE_VULKAN_QUEUE);
	BIND_ENUM_CONSTANT(DRIVER_RESOURCE_VULKAN_QUEUE_FAMILY_INDEX);
	BIND_ENUM_CONSTANT(DRIVER_RESOURCE_VULKAN_IMAGE);
	BIND_ENUM_CONSTANT(DRIVER_RESOURCE_VULKAN_IMAGE_VIEW);
	BIND_ENUM_CONSTANT(DRIVER_RESOURCE_VULKAN_IMAGE_NATIVE_TEXTURE_FORMAT);
	BIND_ENUM_CONSTANT(DRIVER_RESOURCE_VULKAN_SAMPLER);
	BIND_ENUM_CONSTANT(DRIVER_RESOURCE_VULKAN_DESCRIPTOR_SET);
	BIND_ENUM_CONSTANT(DRIVER_RESOURCE_VULKAN_BUFFER);
	BIND_ENUM_CONSTANT(DRIVER_RESOURCE_VULKAN_COMPUTE_PIPELINE);
	BIND_ENUM_CONSTANT(DRIVER_RESOURCE_VULKAN_RENDER_PIPELINE);
#endif

	BIND_ENUM_CONSTANT(DATA_FORMAT_R4G4_UNORM_PACK8);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R4G4B4A4_UNORM_PACK16);
	BIND_ENUM_CONSTANT(DATA_FORMAT_B4G4R4A4_UNORM_PACK16);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R5G6B5_UNORM_PACK16);
	BIND_ENUM_CONSTANT(DATA_FORMAT_B5G6R5_UNORM_PACK16);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R5G5B5A1_UNORM_PACK16);
	BIND_ENUM_CONSTANT(DATA_FORMAT_B5G5R5A1_UNORM_PACK16);
	BIND_ENUM_CONSTANT(DATA_FORMAT_A1R5G5B5_UNORM_PACK16);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R8_UNORM);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R8_SNORM);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R8_USCALED);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R8_SSCALED);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R8_UINT);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R8_SINT);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R8_SRGB);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R8G8_UNORM);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R8G8_SNORM);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R8G8_USCALED);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R8G8_SSCALED);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R8G8_UINT);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R8G8_SINT);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R8G8_SRGB);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R8G8B8_UNORM);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R8G8B8_SNORM);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R8G8B8_USCALED);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R8G8B8_SSCALED);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R8G8B8_UINT);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R8G8B8_SINT);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R8G8B8_SRGB);
	BIND_ENUM_CONSTANT(DATA_FORMAT_B8G8R8_UNORM);
	BIND_ENUM_CONSTANT(DATA_FORMAT_B8G8R8_SNORM);
	BIND_ENUM_CONSTANT(DATA_FORMAT_B8G8R8_USCALED);
	BIND_ENUM_CONSTANT(DATA_FORMAT_B8G8R8_SSCALED);
	BIND_ENUM_CONSTANT(DATA_FORMAT_B8G8R8_UINT);
	BIND_ENUM_CONSTANT(DATA_FORMAT_B8G8R8_SINT);
	BIND_ENUM_CONSTANT(DATA_FORMAT_B8G8R8_SRGB);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R8G8B8A8_UNORM);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R8G8B8A8_SNORM);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R8G8B8A8_USCALED);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R8G8B8A8_SSCALED);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R8G8B8A8_UINT);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R8G8B8A8_SINT);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R8G8B8A8_SRGB);
	BIND_ENUM_CONSTANT(DATA_FORMAT_B8G8R8A8_UNORM);
	BIND_ENUM_CONSTANT(DATA_FORMAT_B8G8R8A8_SNORM);
	BIND_ENUM_CONSTANT(DATA_FORMAT_B8G8R8A8_USCALED);
	BIND_ENUM_CONSTANT(DATA_FORMAT_B8G8R8A8_SSCALED);
	BIND_ENUM_CONSTANT(DATA_FORMAT_B8G8R8A8_UINT);
	BIND_ENUM_CONSTANT(DATA_FORMAT_B8G8R8A8_SINT);
	BIND_ENUM_CONSTANT(DATA_FORMAT_B8G8R8A8_SRGB);
	BIND_ENUM_CONSTANT(DATA_FORMAT_A8B8G8R8_UNORM_PACK32);
	BIND_ENUM_CONSTANT(DATA_FORMAT_A8B8G8R8_SNORM_PACK32);
	BIND_ENUM_CONSTANT(DATA_FORMAT_A8B8G8R8_USCALED_PACK32);
	BIND_ENUM_CONSTANT(DATA_FORMAT_A8B8G8R8_SSCALED_PACK32);
	BIND_ENUM_CONSTANT(DATA_FORMAT_A8B8G8R8_UINT_PACK32);
	BIND_ENUM_CONSTANT(DATA_FORMAT_A8B8G8R8_SINT_PACK32);
	BIND_ENUM_CONSTANT(DATA_FORMAT_A8B8G8R8_SRGB_PACK32);
	BIND_ENUM_CONSTANT(DATA_FORMAT_A2R10G10B10_UNORM_PACK32);
	BIND_ENUM_CONSTANT(DATA_FORMAT_A2R10G10B10_SNORM_PACK32);
	BIND_ENUM_CONSTANT(DATA_FORMAT_A2R10G10B10_USCALED_PACK32);
	BIND_ENUM_CONSTANT(DATA_FORMAT_A2R10G10B10_SSCALED_PACK32);
	BIND_ENUM_CONSTANT(DATA_FORMAT_A2R10G10B10_UINT_PACK32);
	BIND_ENUM_CONSTANT(DATA_FORMAT_A2R10G10B10_SINT_PACK32);
	BIND_ENUM_CONSTANT(DATA_FORMAT_A2B10G10R10_UNORM_PACK32);
	BIND_ENUM_CONSTANT(DATA_FORMAT_A2B10G10R10_SNORM_PACK32);
	BIND_ENUM_CONSTANT(DATA_FORMAT_A2B10G10R10_USCALED_PACK32);
	BIND_ENUM_CONSTANT(DATA_FORMAT_A2B10G10R10_SSCALED_PACK32);
	BIND_ENUM_CONSTANT(DATA_FORMAT_A2B10G10R10_UINT_PACK32);
	BIND_ENUM_CONSTANT(DATA_FORMAT_A2B10G10R10_SINT_PACK32);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R16_UNORM);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R16_SNORM);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R16_USCALED);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R16_SSCALED);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R16_UINT);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R16_SINT);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R16_SFLOAT);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R16G16_UNORM);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R16G16_SNORM);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R16G16_USCALED);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R16G16_SSCALED);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R16G16_UINT);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R16G16_SINT);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R16G16_SFLOAT);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R16G16B16_UNORM);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R16G16B16_SNORM);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R16G16B16_USCALED);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R16G16B16_SSCALED);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R16G16B16_UINT);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R16G16B16_SINT);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R16G16B16_SFLOAT);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R16G16B16A16_UNORM);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R16G16B16A16_SNORM);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R16G16B16A16_USCALED);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R16G16B16A16_SSCALED);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R16G16B16A16_UINT);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R16G16B16A16_SINT);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R16G16B16A16_SFLOAT);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R32_UINT);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R32_SINT);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R32_SFLOAT);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R32G32_UINT);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R32G32_SINT);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R32G32_SFLOAT);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R32G32B32_UINT);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R32G32B32_SINT);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R32G32B32_SFLOAT);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R32G32B32A32_UINT);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R32G32B32A32_SINT);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R32G32B32A32_SFLOAT);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R64_UINT);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R64_SINT);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R64_SFLOAT);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R64G64_UINT);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R64G64_SINT);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R64G64_SFLOAT);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R64G64B64_UINT);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R64G64B64_SINT);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R64G64B64_SFLOAT);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R64G64B64A64_UINT);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R64G64B64A64_SINT);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R64G64B64A64_SFLOAT);
	BIND_ENUM_CONSTANT(DATA_FORMAT_B10G11R11_UFLOAT_PACK32);
	BIND_ENUM_CONSTANT(DATA_FORMAT_E5B9G9R9_UFLOAT_PACK32);
	BIND_ENUM_CONSTANT(DATA_FORMAT_D16_UNORM);
	BIND_ENUM_CONSTANT(DATA_FORMAT_X8_D24_UNORM_PACK32);
	BIND_ENUM_CONSTANT(DATA_FORMAT_D32_SFLOAT);
	BIND_ENUM_CONSTANT(DATA_FORMAT_S8_UINT);
	BIND_ENUM_CONSTANT(DATA_FORMAT_D16_UNORM_S8_UINT);
	BIND_ENUM_CONSTANT(DATA_FORMAT_D24_UNORM_S8_UINT);
	BIND_ENUM_CONSTANT(DATA_FORMAT_D32_SFLOAT_S8_UINT);
	BIND_ENUM_CONSTANT(DATA_FORMAT_BC1_RGB_UNORM_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_BC1_RGB_SRGB_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_BC1_RGBA_UNORM_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_BC1_RGBA_SRGB_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_BC2_UNORM_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_BC2_SRGB_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_BC3_UNORM_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_BC3_SRGB_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_BC4_UNORM_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_BC4_SNORM_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_BC5_UNORM_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_BC5_SNORM_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_BC6H_UFLOAT_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_BC6H_SFLOAT_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_BC7_UNORM_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_BC7_SRGB_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_ETC2_R8G8B8_UNORM_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_ETC2_R8G8B8_SRGB_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_EAC_R11_UNORM_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_EAC_R11_SNORM_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_EAC_R11G11_UNORM_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_EAC_R11G11_SNORM_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_ASTC_4x4_UNORM_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_ASTC_4x4_SRGB_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_ASTC_5x4_UNORM_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_ASTC_5x4_SRGB_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_ASTC_5x5_UNORM_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_ASTC_5x5_SRGB_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_ASTC_6x5_UNORM_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_ASTC_6x5_SRGB_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_ASTC_6x6_UNORM_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_ASTC_6x6_SRGB_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_ASTC_8x5_UNORM_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_ASTC_8x5_SRGB_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_ASTC_8x6_UNORM_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_ASTC_8x6_SRGB_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_ASTC_8x8_UNORM_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_ASTC_8x8_SRGB_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_ASTC_10x5_UNORM_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_ASTC_10x5_SRGB_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_ASTC_10x6_UNORM_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_ASTC_10x6_SRGB_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_ASTC_10x8_UNORM_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_ASTC_10x8_SRGB_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_ASTC_10x10_UNORM_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_ASTC_10x10_SRGB_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_ASTC_12x10_UNORM_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_ASTC_12x10_SRGB_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_ASTC_12x12_UNORM_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_ASTC_12x12_SRGB_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_G8B8G8R8_422_UNORM);
	BIND_ENUM_CONSTANT(DATA_FORMAT_B8G8R8G8_422_UNORM);
	BIND_ENUM_CONSTANT(DATA_FORMAT_G8_B8_R8_3PLANE_420_UNORM);
	BIND_ENUM_CONSTANT(DATA_FORMAT_G8_B8R8_2PLANE_420_UNORM);
	BIND_ENUM_CONSTANT(DATA_FORMAT_G8_B8_R8_3PLANE_422_UNORM);
	BIND_ENUM_CONSTANT(DATA_FORMAT_G8_B8R8_2PLANE_422_UNORM);
	BIND_ENUM_CONSTANT(DATA_FORMAT_G8_B8_R8_3PLANE_444_UNORM);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R10X6_UNORM_PACK16);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R10X6G10X6_UNORM_2PACK16);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16);
	BIND_ENUM_CONSTANT(DATA_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16);
	BIND_ENUM_CONSTANT(DATA_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16);
	BIND_ENUM_CONSTANT(DATA_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16);
	BIND_ENUM_CONSTANT(DATA_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16);
	BIND_ENUM_CONSTANT(DATA_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16);
	BIND_ENUM_CONSTANT(DATA_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16);
	BIND_ENUM_CONSTANT(DATA_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R12X4_UNORM_PACK16);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R12X4G12X4_UNORM_2PACK16);
	BIND_ENUM_CONSTANT(DATA_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16);
	BIND_ENUM_CONSTANT(DATA_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16);
	BIND_ENUM_CONSTANT(DATA_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16);
	BIND_ENUM_CONSTANT(DATA_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16);
	BIND_ENUM_CONSTANT(DATA_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16);
	BIND_ENUM_CONSTANT(DATA_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16);
	BIND_ENUM_CONSTANT(DATA_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16);
	BIND_ENUM_CONSTANT(DATA_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16);
	BIND_ENUM_CONSTANT(DATA_FORMAT_G16B16G16R16_422_UNORM);
	BIND_ENUM_CONSTANT(DATA_FORMAT_B16G16R16G16_422_UNORM);
	BIND_ENUM_CONSTANT(DATA_FORMAT_G16_B16_R16_3PLANE_420_UNORM);
	BIND_ENUM_CONSTANT(DATA_FORMAT_G16_B16R16_2PLANE_420_UNORM);
	BIND_ENUM_CONSTANT(DATA_FORMAT_G16_B16_R16_3PLANE_422_UNORM);
	BIND_ENUM_CONSTANT(DATA_FORMAT_G16_B16R16_2PLANE_422_UNORM);
	BIND_ENUM_CONSTANT(DATA_FORMAT_G16_B16_R16_3PLANE_444_UNORM);
	BIND_ENUM_CONSTANT(DATA_FORMAT_ASTC_4x4_SFLOAT_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_ASTC_5x4_SFLOAT_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_ASTC_5x5_SFLOAT_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_ASTC_6x5_SFLOAT_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_ASTC_6x6_SFLOAT_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_ASTC_8x5_SFLOAT_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_ASTC_8x6_SFLOAT_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_ASTC_8x8_SFLOAT_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_ASTC_10x5_SFLOAT_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_ASTC_10x6_SFLOAT_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_ASTC_10x8_SFLOAT_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_ASTC_10x10_SFLOAT_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_ASTC_12x10_SFLOAT_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_ASTC_12x12_SFLOAT_BLOCK);
	BIND_ENUM_CONSTANT(DATA_FORMAT_MAX);

#ifndef DISABLE_DEPRECATED
	BIND_BITFIELD_FLAG(BARRIER_MASK_VERTEX);
	BIND_BITFIELD_FLAG(BARRIER_MASK_FRAGMENT);
	BIND_BITFIELD_FLAG(BARRIER_MASK_COMPUTE);
	BIND_BITFIELD_FLAG(BARRIER_MASK_TRANSFER);
	BIND_BITFIELD_FLAG(BARRIER_MASK_RASTER);
	BIND_BITFIELD_FLAG(BARRIER_MASK_ALL_BARRIERS);
	BIND_BITFIELD_FLAG(BARRIER_MASK_NO_BARRIER);
#endif

	BIND_ENUM_CONSTANT(TEXTURE_TYPE_1D);
	BIND_ENUM_CONSTANT(TEXTURE_TYPE_2D);
	BIND_ENUM_CONSTANT(TEXTURE_TYPE_3D);
	BIND_ENUM_CONSTANT(TEXTURE_TYPE_CUBE);
	BIND_ENUM_CONSTANT(TEXTURE_TYPE_1D_ARRAY);
	BIND_ENUM_CONSTANT(TEXTURE_TYPE_2D_ARRAY);
	BIND_ENUM_CONSTANT(TEXTURE_TYPE_CUBE_ARRAY);
	BIND_ENUM_CONSTANT(TEXTURE_TYPE_MAX);

	BIND_ENUM_CONSTANT(TEXTURE_SAMPLES_1);
	BIND_ENUM_CONSTANT(TEXTURE_SAMPLES_2);
	BIND_ENUM_CONSTANT(TEXTURE_SAMPLES_4);
	BIND_ENUM_CONSTANT(TEXTURE_SAMPLES_8);
	BIND_ENUM_CONSTANT(TEXTURE_SAMPLES_16);
	BIND_ENUM_CONSTANT(TEXTURE_SAMPLES_32);
	BIND_ENUM_CONSTANT(TEXTURE_SAMPLES_64);
	BIND_ENUM_CONSTANT(TEXTURE_SAMPLES_MAX);

	BIND_BITFIELD_FLAG(TEXTURE_USAGE_SAMPLING_BIT);
	BIND_BITFIELD_FLAG(TEXTURE_USAGE_COLOR_ATTACHMENT_BIT);
	BIND_BITFIELD_FLAG(TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
	BIND_BITFIELD_FLAG(TEXTURE_USAGE_DEPTH_RESOLVE_ATTACHMENT_BIT);
	BIND_BITFIELD_FLAG(TEXTURE_USAGE_STORAGE_BIT);
	BIND_BITFIELD_FLAG(TEXTURE_USAGE_STORAGE_ATOMIC_BIT);
	BIND_BITFIELD_FLAG(TEXTURE_USAGE_CPU_READ_BIT);
	BIND_BITFIELD_FLAG(TEXTURE_USAGE_CAN_UPDATE_BIT);
	BIND_BITFIELD_FLAG(TEXTURE_USAGE_CAN_COPY_FROM_BIT);
	BIND_BITFIELD_FLAG(TEXTURE_USAGE_CAN_COPY_TO_BIT);
	BIND_BITFIELD_FLAG(TEXTURE_USAGE_INPUT_ATTACHMENT_BIT);

	BIND_ENUM_CONSTANT(TEXTURE_SWIZZLE_IDENTITY);
	BIND_ENUM_CONSTANT(TEXTURE_SWIZZLE_ZERO);
	BIND_ENUM_CONSTANT(TEXTURE_SWIZZLE_ONE);
	BIND_ENUM_CONSTANT(TEXTURE_SWIZZLE_R);
	BIND_ENUM_CONSTANT(TEXTURE_SWIZZLE_G);
	BIND_ENUM_CONSTANT(TEXTURE_SWIZZLE_B);
	BIND_ENUM_CONSTANT(TEXTURE_SWIZZLE_A);
	BIND_ENUM_CONSTANT(TEXTURE_SWIZZLE_MAX);

	BIND_ENUM_CONSTANT(TEXTURE_SLICE_2D);
	BIND_ENUM_CONSTANT(TEXTURE_SLICE_CUBEMAP);
	BIND_ENUM_CONSTANT(TEXTURE_SLICE_3D);

	BIND_ENUM_CONSTANT(SAMPLER_FILTER_NEAREST);
	BIND_ENUM_CONSTANT(SAMPLER_FILTER_LINEAR);
	BIND_ENUM_CONSTANT(SAMPLER_REPEAT_MODE_REPEAT);
	BIND_ENUM_CONSTANT(SAMPLER_REPEAT_MODE_MIRRORED_REPEAT);
	BIND_ENUM_CONSTANT(SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE);
	BIND_ENUM_CONSTANT(SAMPLER_REPEAT_MODE_CLAMP_TO_BORDER);
	BIND_ENUM_CONSTANT(SAMPLER_REPEAT_MODE_MIRROR_CLAMP_TO_EDGE);
	BIND_ENUM_CONSTANT(SAMPLER_REPEAT_MODE_MAX);

	BIND_ENUM_CONSTANT(SAMPLER_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK);
	BIND_ENUM_CONSTANT(SAMPLER_BORDER_COLOR_INT_TRANSPARENT_BLACK);
	BIND_ENUM_CONSTANT(SAMPLER_BORDER_COLOR_FLOAT_OPAQUE_BLACK);
	BIND_ENUM_CONSTANT(SAMPLER_BORDER_COLOR_INT_OPAQUE_BLACK);
	BIND_ENUM_CONSTANT(SAMPLER_BORDER_COLOR_FLOAT_OPAQUE_WHITE);
	BIND_ENUM_CONSTANT(SAMPLER_BORDER_COLOR_INT_OPAQUE_WHITE);
	BIND_ENUM_CONSTANT(SAMPLER_BORDER_COLOR_MAX);

	BIND_ENUM_CONSTANT(VERTEX_FREQUENCY_VERTEX);
	BIND_ENUM_CONSTANT(VERTEX_FREQUENCY_INSTANCE);

	BIND_ENUM_CONSTANT(INDEX_BUFFER_FORMAT_UINT16);
	BIND_ENUM_CONSTANT(INDEX_BUFFER_FORMAT_UINT32);

	BIND_BITFIELD_FLAG(STORAGE_BUFFER_USAGE_DISPATCH_INDIRECT);

	BIND_BITFIELD_FLAG(BUFFER_CREATION_DEVICE_ADDRESS_BIT);
	BIND_BITFIELD_FLAG(BUFFER_CREATION_AS_STORAGE_BIT);
	// Not exposed on purpose. This flag is too dangerous to be exposed to regular GD users.
	//BIND_BITFIELD_FLAG(BUFFER_CREATION_DYNAMIC_PERSISTENT_BIT);
	BIND_BITFIELD_FLAG(BUFFER_CREATION_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT);

	BIND_BITFIELD_FLAG(ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT);
	BIND_BITFIELD_FLAG(ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT);
	BIND_BITFIELD_FLAG(ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT);
	BIND_BITFIELD_FLAG(ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT);
	BIND_BITFIELD_FLAG(ACCELERATION_STRUCTURE_LOW_MEMORY_BIT);

	BIND_BITFIELD_FLAG(ACCELERATION_STRUCTURE_GEOMETRY_OPAQUE_BIT);
	BIND_BITFIELD_FLAG(ACCELERATION_STRUCTURE_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT);

	BIND_BITFIELD_FLAG(ACCELERATION_STRUCTURE_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT);
	BIND_BITFIELD_FLAG(ACCELERATION_STRUCTURE_INSTANCE_TRIANGLE_FLIP_FACING_BIT);
	BIND_BITFIELD_FLAG(ACCELERATION_STRUCTURE_INSTANCE_FORCE_OPAQUE_BIT);
	BIND_BITFIELD_FLAG(ACCELERATION_STRUCTURE_INSTANCE_FORCE_NO_OPAQUE_BIT);

	BIND_ENUM_CONSTANT(UNIFORM_TYPE_SAMPLER); //for sampling only (sampler GLSL type)
	BIND_ENUM_CONSTANT(UNIFORM_TYPE_SAMPLER_WITH_TEXTURE); // for sampling only); but includes a texture); (samplerXX GLSL type)); first a sampler then a texture
	BIND_ENUM_CONSTANT(UNIFORM_TYPE_TEXTURE); //only texture); (textureXX GLSL type)
	BIND_ENUM_CONSTANT(UNIFORM_TYPE_IMAGE); // storage image (imageXX GLSL type)); for compute mostly
	BIND_ENUM_CONSTANT(UNIFORM_TYPE_TEXTURE_BUFFER); // buffer texture (or TBO); textureBuffer type)
	BIND_ENUM_CONSTANT(UNIFORM_TYPE_SAMPLER_WITH_TEXTURE_BUFFER); // buffer texture with a sampler(or TBO); samplerBuffer type)
	BIND_ENUM_CONSTANT(UNIFORM_TYPE_IMAGE_BUFFER); //texel buffer); (imageBuffer type)); for compute mostly
	BIND_ENUM_CONSTANT(UNIFORM_TYPE_UNIFORM_BUFFER); //regular uniform buffer (or UBO).
	BIND_ENUM_CONSTANT(UNIFORM_TYPE_STORAGE_BUFFER); //storage buffer ("buffer" qualifier) like UBO); but supports storage); for compute mostly
	BIND_ENUM_CONSTANT(UNIFORM_TYPE_INPUT_ATTACHMENT); //used for sub-pass read/write); for mobile mostly
	BIND_ENUM_CONSTANT(UNIFORM_TYPE_UNIFORM_BUFFER_DYNAMIC); // Exposed in case a BUFFER_CREATION_DYNAMIC_PERSISTENT_BIT buffer created by C++ makes it into GD users.
	BIND_ENUM_CONSTANT(UNIFORM_TYPE_STORAGE_BUFFER_DYNAMIC); // Exposed in case a BUFFER_CREATION_DYNAMIC_PERSISTENT_BIT buffer created by C++ makes it into GD users.
	BIND_ENUM_CONSTANT(UNIFORM_TYPE_ACCELERATION_STRUCTURE); //acceleration structure (TLAS)); for raytracing
	BIND_ENUM_CONSTANT(UNIFORM_TYPE_MAX);

	BIND_ENUM_CONSTANT(RENDER_PRIMITIVE_POINTS);
	BIND_ENUM_CONSTANT(RENDER_PRIMITIVE_LINES);
	BIND_ENUM_CONSTANT(RENDER_PRIMITIVE_LINES_WITH_ADJACENCY);
	BIND_ENUM_CONSTANT(RENDER_PRIMITIVE_LINESTRIPS);
	BIND_ENUM_CONSTANT(RENDER_PRIMITIVE_LINESTRIPS_WITH_ADJACENCY);
	BIND_ENUM_CONSTANT(RENDER_PRIMITIVE_TRIANGLES);
	BIND_ENUM_CONSTANT(RENDER_PRIMITIVE_TRIANGLES_WITH_ADJACENCY);
	BIND_ENUM_CONSTANT(RENDER_PRIMITIVE_TRIANGLE_STRIPS);
	BIND_ENUM_CONSTANT(RENDER_PRIMITIVE_TRIANGLE_STRIPS_WITH_AJACENCY);
	BIND_ENUM_CONSTANT(RENDER_PRIMITIVE_TRIANGLE_STRIPS_WITH_RESTART_INDEX);
	BIND_ENUM_CONSTANT(RENDER_PRIMITIVE_TESSELATION_PATCH);
	BIND_ENUM_CONSTANT(RENDER_PRIMITIVE_MAX);

	BIND_ENUM_CONSTANT(POLYGON_CULL_DISABLED);
	BIND_ENUM_CONSTANT(POLYGON_CULL_FRONT);
	BIND_ENUM_CONSTANT(POLYGON_CULL_BACK);

	BIND_ENUM_CONSTANT(POLYGON_FRONT_FACE_CLOCKWISE);
	BIND_ENUM_CONSTANT(POLYGON_FRONT_FACE_COUNTER_CLOCKWISE);

	BIND_ENUM_CONSTANT(STENCIL_OP_KEEP);
	BIND_ENUM_CONSTANT(STENCIL_OP_ZERO);
	BIND_ENUM_CONSTANT(STENCIL_OP_REPLACE);
	BIND_ENUM_CONSTANT(STENCIL_OP_INCREMENT_AND_CLAMP);
	BIND_ENUM_CONSTANT(STENCIL_OP_DECREMENT_AND_CLAMP);
	BIND_ENUM_CONSTANT(STENCIL_OP_INVERT);
	BIND_ENUM_CONSTANT(STENCIL_OP_INCREMENT_AND_WRAP);
	BIND_ENUM_CONSTANT(STENCIL_OP_DECREMENT_AND_WRAP);
	BIND_ENUM_CONSTANT(STENCIL_OP_MAX); //not an actual operator); just the amount of operators :D

	BIND_ENUM_CONSTANT(COMPARE_OP_NEVER);
	BIND_ENUM_CONSTANT(COMPARE_OP_LESS);
	BIND_ENUM_CONSTANT(COMPARE_OP_EQUAL);
	BIND_ENUM_CONSTANT(COMPARE_OP_LESS_OR_EQUAL);
	BIND_ENUM_CONSTANT(COMPARE_OP_GREATER);
	BIND_ENUM_CONSTANT(COMPARE_OP_NOT_EQUAL);
	BIND_ENUM_CONSTANT(COMPARE_OP_GREATER_OR_EQUAL);
	BIND_ENUM_CONSTANT(COMPARE_OP_ALWAYS);
	BIND_ENUM_CONSTANT(COMPARE_OP_MAX);

	BIND_ENUM_CONSTANT(LOGIC_OP_CLEAR);
	BIND_ENUM_CONSTANT(LOGIC_OP_AND);
	BIND_ENUM_CONSTANT(LOGIC_OP_AND_REVERSE);
	BIND_ENUM_CONSTANT(LOGIC_OP_COPY);
	BIND_ENUM_CONSTANT(LOGIC_OP_AND_INVERTED);
	BIND_ENUM_CONSTANT(LOGIC_OP_NO_OP);
	BIND_ENUM_CONSTANT(LOGIC_OP_XOR);
	BIND_ENUM_CONSTANT(LOGIC_OP_OR);
	BIND_ENUM_CONSTANT(LOGIC_OP_NOR);
	BIND_ENUM_CONSTANT(LOGIC_OP_EQUIVALENT);
	BIND_ENUM_CONSTANT(LOGIC_OP_INVERT);
	BIND_ENUM_CONSTANT(LOGIC_OP_OR_REVERSE);
	BIND_ENUM_CONSTANT(LOGIC_OP_COPY_INVERTED);
	BIND_ENUM_CONSTANT(LOGIC_OP_OR_INVERTED);
	BIND_ENUM_CONSTANT(LOGIC_OP_NAND);
	BIND_ENUM_CONSTANT(LOGIC_OP_SET);
	BIND_ENUM_CONSTANT(LOGIC_OP_MAX); //not an actual operator); just the amount of operators :D

	BIND_ENUM_CONSTANT(BLEND_FACTOR_ZERO);
	BIND_ENUM_CONSTANT(BLEND_FACTOR_ONE);
	BIND_ENUM_CONSTANT(BLEND_FACTOR_SRC_COLOR);
	BIND_ENUM_CONSTANT(BLEND_FACTOR_ONE_MINUS_SRC_COLOR);
	BIND_ENUM_CONSTANT(BLEND_FACTOR_DST_COLOR);
	BIND_ENUM_CONSTANT(BLEND_FACTOR_ONE_MINUS_DST_COLOR);
	BIND_ENUM_CONSTANT(BLEND_FACTOR_SRC_ALPHA);
	BIND_ENUM_CONSTANT(BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
	BIND_ENUM_CONSTANT(BLEND_FACTOR_DST_ALPHA);
	BIND_ENUM_CONSTANT(BLEND_FACTOR_ONE_MINUS_DST_ALPHA);
	BIND_ENUM_CONSTANT(BLEND_FACTOR_CONSTANT_COLOR);
	BIND_ENUM_CONSTANT(BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR);
	BIND_ENUM_CONSTANT(BLEND_FACTOR_CONSTANT_ALPHA);
	BIND_ENUM_CONSTANT(BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA);
	BIND_ENUM_CONSTANT(BLEND_FACTOR_SRC_ALPHA_SATURATE);
	BIND_ENUM_CONSTANT(BLEND_FACTOR_SRC1_COLOR);
	BIND_ENUM_CONSTANT(BLEND_FACTOR_ONE_MINUS_SRC1_COLOR);
	BIND_ENUM_CONSTANT(BLEND_FACTOR_SRC1_ALPHA);
	BIND_ENUM_CONSTANT(BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA);
	BIND_ENUM_CONSTANT(BLEND_FACTOR_MAX);

	BIND_ENUM_CONSTANT(BLEND_OP_ADD);
	BIND_ENUM_CONSTANT(BLEND_OP_SUBTRACT);
	BIND_ENUM_CONSTANT(BLEND_OP_REVERSE_SUBTRACT);
	BIND_ENUM_CONSTANT(BLEND_OP_MINIMUM);
	BIND_ENUM_CONSTANT(BLEND_OP_MAXIMUM);
	BIND_ENUM_CONSTANT(BLEND_OP_MAX);

	BIND_BITFIELD_FLAG(DYNAMIC_STATE_LINE_WIDTH);
	BIND_BITFIELD_FLAG(DYNAMIC_STATE_DEPTH_BIAS);
	BIND_BITFIELD_FLAG(DYNAMIC_STATE_BLEND_CONSTANTS);
	BIND_BITFIELD_FLAG(DYNAMIC_STATE_DEPTH_BOUNDS);
	BIND_BITFIELD_FLAG(DYNAMIC_STATE_STENCIL_COMPARE_MASK);
	BIND_BITFIELD_FLAG(DYNAMIC_STATE_STENCIL_WRITE_MASK);
	BIND_BITFIELD_FLAG(DYNAMIC_STATE_STENCIL_REFERENCE);

#ifndef DISABLE_DEPRECATED
	BIND_ENUM_CONSTANT(INITIAL_ACTION_LOAD);
	BIND_ENUM_CONSTANT(INITIAL_ACTION_CLEAR);
	BIND_ENUM_CONSTANT(INITIAL_ACTION_DISCARD);
	BIND_ENUM_CONSTANT(INITIAL_ACTION_MAX);
	BIND_ENUM_CONSTANT(INITIAL_ACTION_CLEAR_REGION);
	BIND_ENUM_CONSTANT(INITIAL_ACTION_CLEAR_REGION_CONTINUE);
	BIND_ENUM_CONSTANT(INITIAL_ACTION_KEEP);
	BIND_ENUM_CONSTANT(INITIAL_ACTION_DROP);
	BIND_ENUM_CONSTANT(INITIAL_ACTION_CONTINUE);

	BIND_ENUM_CONSTANT(FINAL_ACTION_STORE);
	BIND_ENUM_CONSTANT(FINAL_ACTION_DISCARD);
	BIND_ENUM_CONSTANT(FINAL_ACTION_MAX);
	BIND_ENUM_CONSTANT(FINAL_ACTION_READ);
	BIND_ENUM_CONSTANT(FINAL_ACTION_CONTINUE);
#endif

	BIND_ENUM_CONSTANT(SHADER_STAGE_VERTEX);
	BIND_ENUM_CONSTANT(SHADER_STAGE_FRAGMENT);
	BIND_ENUM_CONSTANT(SHADER_STAGE_TESSELATION_CONTROL);
	BIND_ENUM_CONSTANT(SHADER_STAGE_TESSELATION_EVALUATION);
	BIND_ENUM_CONSTANT(SHADER_STAGE_COMPUTE);
	BIND_ENUM_CONSTANT(SHADER_STAGE_RAYGEN);
	BIND_ENUM_CONSTANT(SHADER_STAGE_ANY_HIT);
	BIND_ENUM_CONSTANT(SHADER_STAGE_CLOSEST_HIT);
	BIND_ENUM_CONSTANT(SHADER_STAGE_MISS);
	BIND_ENUM_CONSTANT(SHADER_STAGE_INTERSECTION);
	BIND_ENUM_CONSTANT(SHADER_STAGE_MAX);
	BIND_ENUM_CONSTANT(SHADER_STAGE_VERTEX_BIT);
	BIND_ENUM_CONSTANT(SHADER_STAGE_FRAGMENT_BIT);
	BIND_ENUM_CONSTANT(SHADER_STAGE_TESSELATION_CONTROL_BIT);
	BIND_ENUM_CONSTANT(SHADER_STAGE_TESSELATION_EVALUATION_BIT);
	BIND_ENUM_CONSTANT(SHADER_STAGE_COMPUTE_BIT);
	BIND_ENUM_CONSTANT(SHADER_STAGE_RAYGEN_BIT);
	BIND_ENUM_CONSTANT(SHADER_STAGE_ANY_HIT_BIT);
	BIND_ENUM_CONSTANT(SHADER_STAGE_CLOSEST_HIT_BIT);
	BIND_ENUM_CONSTANT(SHADER_STAGE_MISS_BIT);
	BIND_ENUM_CONSTANT(SHADER_STAGE_INTERSECTION_BIT);

	BIND_ENUM_CONSTANT(SHADER_LANGUAGE_GLSL);
	BIND_ENUM_CONSTANT(SHADER_LANGUAGE_HLSL);

	BIND_ENUM_CONSTANT(PIPELINE_SPECIALIZATION_CONSTANT_TYPE_BOOL);
	BIND_ENUM_CONSTANT(PIPELINE_SPECIALIZATION_CONSTANT_TYPE_INT);
	BIND_ENUM_CONSTANT(PIPELINE_SPECIALIZATION_CONSTANT_TYPE_FLOAT);

	BIND_ENUM_CONSTANT(SUPPORTS_METALFX_SPATIAL);
	BIND_ENUM_CONSTANT(SUPPORTS_METALFX_TEMPORAL);
	BIND_ENUM_CONSTANT(SUPPORTS_BUFFER_DEVICE_ADDRESS);
	BIND_ENUM_CONSTANT(SUPPORTS_IMAGE_ATOMIC_32_BIT);
	BIND_ENUM_CONSTANT(SUPPORTS_RAY_QUERY);
	BIND_ENUM_CONSTANT(SUPPORTS_RAYTRACING_PIPELINE);
	BIND_ENUM_CONSTANT(SUPPORTS_HDR_OUTPUT);

	BIND_ENUM_CONSTANT(LIMIT_MAX_BOUND_UNIFORM_SETS);
	BIND_ENUM_CONSTANT(LIMIT_MAX_FRAMEBUFFER_COLOR_ATTACHMENTS);
	BIND_ENUM_CONSTANT(LIMIT_MAX_TEXTURES_PER_UNIFORM_SET);
	BIND_ENUM_CONSTANT(LIMIT_MAX_SAMPLERS_PER_UNIFORM_SET);
	BIND_ENUM_CONSTANT(LIMIT_MAX_STORAGE_BUFFERS_PER_UNIFORM_SET);
	BIND_ENUM_CONSTANT(LIMIT_MAX_STORAGE_IMAGES_PER_UNIFORM_SET);
	BIND_ENUM_CONSTANT(LIMIT_MAX_UNIFORM_BUFFERS_PER_UNIFORM_SET);
	BIND_ENUM_CONSTANT(LIMIT_MAX_DRAW_INDEXED_INDEX);
	BIND_ENUM_CONSTANT(LIMIT_MAX_FRAMEBUFFER_HEIGHT);
	BIND_ENUM_CONSTANT(LIMIT_MAX_FRAMEBUFFER_WIDTH);
	BIND_ENUM_CONSTANT(LIMIT_MAX_TEXTURE_ARRAY_LAYERS);
	BIND_ENUM_CONSTANT(LIMIT_MAX_TEXTURE_SIZE_1D);
	BIND_ENUM_CONSTANT(LIMIT_MAX_TEXTURE_SIZE_2D);
	BIND_ENUM_CONSTANT(LIMIT_MAX_TEXTURE_SIZE_3D);
	BIND_ENUM_CONSTANT(LIMIT_MAX_TEXTURE_SIZE_CUBE);
	BIND_ENUM_CONSTANT(LIMIT_MAX_TEXTURES_PER_SHADER_STAGE);
	BIND_ENUM_CONSTANT(LIMIT_MAX_SAMPLERS_PER_SHADER_STAGE);
	BIND_ENUM_CONSTANT(LIMIT_MAX_STORAGE_BUFFERS_PER_SHADER_STAGE);
	BIND_ENUM_CONSTANT(LIMIT_MAX_STORAGE_IMAGES_PER_SHADER_STAGE);
	BIND_ENUM_CONSTANT(LIMIT_MAX_UNIFORM_BUFFERS_PER_SHADER_STAGE);
	BIND_ENUM_CONSTANT(LIMIT_MAX_PUSH_CONSTANT_SIZE);
	BIND_ENUM_CONSTANT(LIMIT_MAX_UNIFORM_BUFFER_SIZE);
	BIND_ENUM_CONSTANT(LIMIT_MAX_VERTEX_INPUT_ATTRIBUTE_OFFSET);
	BIND_ENUM_CONSTANT(LIMIT_MAX_VERTEX_INPUT_ATTRIBUTES);
	BIND_ENUM_CONSTANT(LIMIT_MAX_VERTEX_INPUT_BINDINGS);
	BIND_ENUM_CONSTANT(LIMIT_MAX_VERTEX_INPUT_BINDING_STRIDE);
	BIND_ENUM_CONSTANT(LIMIT_MIN_UNIFORM_BUFFER_OFFSET_ALIGNMENT);
	BIND_ENUM_CONSTANT(LIMIT_MAX_COMPUTE_SHARED_MEMORY_SIZE);
	BIND_ENUM_CONSTANT(LIMIT_MAX_COMPUTE_WORKGROUP_COUNT_X);
	BIND_ENUM_CONSTANT(LIMIT_MAX_COMPUTE_WORKGROUP_COUNT_Y);
	BIND_ENUM_CONSTANT(LIMIT_MAX_COMPUTE_WORKGROUP_COUNT_Z);
	BIND_ENUM_CONSTANT(LIMIT_MAX_COMPUTE_WORKGROUP_INVOCATIONS);
	BIND_ENUM_CONSTANT(LIMIT_MAX_COMPUTE_WORKGROUP_SIZE_X);
	BIND_ENUM_CONSTANT(LIMIT_MAX_COMPUTE_WORKGROUP_SIZE_Y);
	BIND_ENUM_CONSTANT(LIMIT_MAX_COMPUTE_WORKGROUP_SIZE_Z);
	BIND_ENUM_CONSTANT(LIMIT_MAX_VIEWPORT_DIMENSIONS_X);
	BIND_ENUM_CONSTANT(LIMIT_MAX_VIEWPORT_DIMENSIONS_Y);
	BIND_ENUM_CONSTANT(LIMIT_METALFX_TEMPORAL_SCALER_MIN_SCALE);
	BIND_ENUM_CONSTANT(LIMIT_METALFX_TEMPORAL_SCALER_MAX_SCALE);

	BIND_ENUM_CONSTANT(MEMORY_TEXTURES);
	BIND_ENUM_CONSTANT(MEMORY_BUFFERS);
	BIND_ENUM_CONSTANT(MEMORY_TOTAL);

	BIND_CONSTANT(INVALID_ID);
	BIND_CONSTANT(INVALID_FORMAT_ID);

	BIND_ENUM_CONSTANT(NONE);
	BIND_ENUM_CONSTANT(REFLECTION_PROBES);
	BIND_ENUM_CONSTANT(SKY_PASS);
	BIND_ENUM_CONSTANT(LIGHTMAPPER_PASS);
	BIND_ENUM_CONSTANT(SHADOW_PASS_DIRECTIONAL);
	BIND_ENUM_CONSTANT(SHADOW_PASS_CUBE);
	BIND_ENUM_CONSTANT(OPAQUE_PASS);
	BIND_ENUM_CONSTANT(ALPHA_PASS);
	BIND_ENUM_CONSTANT(TRANSPARENT_PASS);
	BIND_ENUM_CONSTANT(POST_PROCESSING_PASS);
	BIND_ENUM_CONSTANT(BLIT_PASS);
	BIND_ENUM_CONSTANT(UI_PASS);
	BIND_ENUM_CONSTANT(DEBUG_PASS);

	BIND_BITFIELD_FLAG(DRAW_DEFAULT_ALL);
	BIND_BITFIELD_FLAG(DRAW_CLEAR_COLOR_0);
	BIND_BITFIELD_FLAG(DRAW_CLEAR_COLOR_1);
	BIND_BITFIELD_FLAG(DRAW_CLEAR_COLOR_2);
	BIND_BITFIELD_FLAG(DRAW_CLEAR_COLOR_3);
	BIND_BITFIELD_FLAG(DRAW_CLEAR_COLOR_4);
	BIND_BITFIELD_FLAG(DRAW_CLEAR_COLOR_5);
	BIND_BITFIELD_FLAG(DRAW_CLEAR_COLOR_6);
	BIND_BITFIELD_FLAG(DRAW_CLEAR_COLOR_7);
	BIND_BITFIELD_FLAG(DRAW_CLEAR_COLOR_MASK);
	BIND_BITFIELD_FLAG(DRAW_CLEAR_COLOR_ALL);
	BIND_BITFIELD_FLAG(DRAW_IGNORE_COLOR_0);
	BIND_BITFIELD_FLAG(DRAW_IGNORE_COLOR_1);
	BIND_BITFIELD_FLAG(DRAW_IGNORE_COLOR_2);
	BIND_BITFIELD_FLAG(DRAW_IGNORE_COLOR_3);
	BIND_BITFIELD_FLAG(DRAW_IGNORE_COLOR_4);
	BIND_BITFIELD_FLAG(DRAW_IGNORE_COLOR_5);
	BIND_BITFIELD_FLAG(DRAW_IGNORE_COLOR_6);
	BIND_BITFIELD_FLAG(DRAW_IGNORE_COLOR_7);
	BIND_BITFIELD_FLAG(DRAW_IGNORE_COLOR_MASK);
	BIND_BITFIELD_FLAG(DRAW_IGNORE_COLOR_ALL);
	BIND_BITFIELD_FLAG(DRAW_CLEAR_DEPTH);
	BIND_BITFIELD_FLAG(DRAW_IGNORE_DEPTH);
	BIND_BITFIELD_FLAG(DRAW_CLEAR_STENCIL);
	BIND_BITFIELD_FLAG(DRAW_IGNORE_STENCIL);
	BIND_BITFIELD_FLAG(DRAW_CLEAR_ALL);
	BIND_BITFIELD_FLAG(DRAW_IGNORE_ALL);
}

void RenderingDevice::make_current() {
	const Thread::ID next = Thread::get_caller_id();
	const Error error = GDExtensionDeviceExit::Registry::get_singleton().device_execution_moved(this, render_thread_id, next);
	ERR_FAIL_COND_MSG(error != OK, "RenderingDevice execution-thread migration was refused by its original registry.");
	render_thread_id = next;
}

RenderingDevice::~RenderingDevice() {
	// A void finalize refusal must never fall through to C++ member destruction.
	GDExtensionDeviceExit::require_close_success(_begin_production_finalization(), GDExtensionDeviceExit::CloseRoute::LAST_REF);
	// The original RS owns the execution thread. Its completed native teardown
	// is distinct from the later DisplayServer/Object owner destruction.
	if (!native_finalization_returned) {
		finalize();
	}
	GDExtensionDeviceExit::require_close_success(native_finalization_returned ? OK : ERR_BUSY, GDExtensionDeviceExit::CloseRoute::LAST_REF);
	GDExtensionDeviceExit::Registry::get_singleton().device_destroyed(this);

	if (singleton == this) {
		singleton = nullptr;
	}
}

RenderingDevice::RenderingDevice() {
	if (singleton == nullptr) {
		singleton = this;
	}

	render_thread_id = Thread::get_caller_id();
}

/*****************/
/**** BINDERS ****/
/*****************/

RID RenderingDevice::_texture_create(const Ref<RDTextureFormat> &p_format, const Ref<RDTextureView> &p_view, const TypedArray<PackedByteArray> &p_data) {
	ERR_FAIL_COND_V(p_format.is_null(), RID());
	ERR_FAIL_COND_V(p_view.is_null(), RID());
	Vector<Vector<uint8_t>> data;
	for (int i = 0; i < p_data.size(); i++) {
		Vector<uint8_t> byte_slice = p_data[i];
		ERR_FAIL_COND_V(byte_slice.is_empty(), RID());
		data.push_back(byte_slice);
	}
	return texture_create(p_format->base, p_view->base, data);
}

RID RenderingDevice::_texture_create_shared(const Ref<RDTextureView> &p_view, RID p_with_texture) {
	ERR_FAIL_COND_V(p_view.is_null(), RID());

	return texture_create_shared(p_view->base, p_with_texture);
}

RID RenderingDevice::_texture_create_shared_from_slice(const Ref<RDTextureView> &p_view, RID p_with_texture, uint32_t p_layer, uint32_t p_mipmap, uint32_t p_mipmaps, TextureSliceType p_slice_type) {
	ERR_FAIL_COND_V(p_view.is_null(), RID());

	return texture_create_shared_from_slice(p_view->base, p_with_texture, p_layer, p_mipmap, p_mipmaps, p_slice_type);
}

Ref<RDTextureFormat> RenderingDevice::_texture_get_format(RID p_rd_texture) {
	Ref<RDTextureFormat> rtf;
	rtf.instantiate();
	rtf->base = texture_get_format(p_rd_texture);

	return rtf;
}

RenderingDevice::FramebufferFormatID RenderingDevice::_framebuffer_format_create(const TypedArray<RDAttachmentFormat> &p_attachments, uint32_t p_view_count) {
	Vector<AttachmentFormat> attachments;
	attachments.resize(p_attachments.size());

	for (int i = 0; i < p_attachments.size(); i++) {
		Ref<RDAttachmentFormat> af = p_attachments[i];
		ERR_FAIL_COND_V(af.is_null(), INVALID_FORMAT_ID);
		attachments.write[i] = af->base;
	}
	return framebuffer_format_create(attachments, p_view_count);
}

RenderingDevice::FramebufferFormatID RenderingDevice::_framebuffer_format_create_multipass(const TypedArray<RDAttachmentFormat> &p_attachments, const TypedArray<RDFramebufferPass> &p_passes, uint32_t p_view_count) {
	Vector<AttachmentFormat> attachments;
	attachments.resize(p_attachments.size());

	for (int i = 0; i < p_attachments.size(); i++) {
		Ref<RDAttachmentFormat> af = p_attachments[i];
		ERR_FAIL_COND_V(af.is_null(), INVALID_FORMAT_ID);
		attachments.write[i] = af->base;
	}

	Vector<FramebufferPass> passes;
	for (int i = 0; i < p_passes.size(); i++) {
		Ref<RDFramebufferPass> pass = p_passes[i];
		ERR_CONTINUE(pass.is_null());
		passes.push_back(pass->base);
	}

	return framebuffer_format_create_multipass(attachments, passes, p_view_count);
}

RID RenderingDevice::_framebuffer_create(const TypedArray<RID> &p_textures, FramebufferFormatID p_format_check, uint32_t p_view_count) {
	Vector<RID> textures = Variant(p_textures);
	return framebuffer_create(textures, p_format_check, p_view_count);
}

RID RenderingDevice::_framebuffer_create_multipass(const TypedArray<RID> &p_textures, const TypedArray<RDFramebufferPass> &p_passes, FramebufferFormatID p_format_check, uint32_t p_view_count) {
	Vector<RID> textures = Variant(p_textures);
	Vector<FramebufferPass> passes;
	for (int i = 0; i < p_passes.size(); i++) {
		Ref<RDFramebufferPass> pass = p_passes[i];
		ERR_CONTINUE(pass.is_null());
		passes.push_back(pass->base);
	}
	return framebuffer_create_multipass(textures, passes, p_format_check, p_view_count);
}

RID RenderingDevice::_sampler_create(const Ref<RDSamplerState> &p_state) {
	ERR_FAIL_COND_V(p_state.is_null(), RID());

	return sampler_create(p_state->base);
}

RenderingDevice::VertexFormatID RenderingDevice::_vertex_format_create(const TypedArray<RDVertexAttribute> &p_vertex_formats) {
	Vector<VertexAttribute> descriptions;
	descriptions.resize(p_vertex_formats.size());

	for (int i = 0; i < p_vertex_formats.size(); i++) {
		Ref<RDVertexAttribute> af = p_vertex_formats[i];
		ERR_FAIL_COND_V(af.is_null(), INVALID_FORMAT_ID);
		descriptions.write[i] = af->base;
	}
	return vertex_format_create(descriptions);
}

RID RenderingDevice::_vertex_array_create(uint32_t p_vertex_count, VertexFormatID p_vertex_format, const TypedArray<RID> &p_src_buffers, const Vector<int64_t> &p_offsets) {
	Vector<RID> buffers = Variant(p_src_buffers);

	Vector<uint64_t> offsets;
	offsets.resize(p_offsets.size());
	for (int i = 0; i < p_offsets.size(); i++) {
		offsets.write[i] = p_offsets[i];
	}

	return vertex_array_create(p_vertex_count, p_vertex_format, buffers, offsets);
}

void RenderingDevice::_draw_list_bind_vertex_buffers_format(DrawListID p_list, VertexFormatID p_vertex_format, uint32_t p_vertex_count, const TypedArray<RID> &p_vertex_buffers, const Vector<int64_t> &p_offsets) {
	Vector<RID> buffers = Variant(p_vertex_buffers);

	Vector<uint64_t> offsets;
	offsets.resize(p_offsets.size());
	for (int i = 0; i < p_offsets.size(); i++) {
		offsets.write[i] = p_offsets[i];
	}

	draw_list_bind_vertex_buffers_format(p_list, p_vertex_format, p_vertex_count, buffers, offsets);
}

Ref<RDShaderSPIRV> RenderingDevice::_shader_compile_spirv_from_source(const Ref<RDShaderSource> &p_source, bool p_allow_cache) {
	ERR_FAIL_COND_V(p_source.is_null(), Ref<RDShaderSPIRV>());

	Ref<RDShaderSPIRV> bytecode;
	bytecode.instantiate();
	for (int i = 0; i < RD::SHADER_STAGE_MAX; i++) {
		String error;

		ShaderStage stage = ShaderStage(i);
		String source = p_source->get_stage_source(stage);

		if (!source.is_empty()) {
			Vector<uint8_t> spirv = shader_compile_spirv_from_source(stage, source, p_source->get_language(), &error, p_allow_cache);
			bytecode->set_stage_bytecode(stage, spirv);
			bytecode->set_stage_compile_error(stage, error);
		}
	}
	return bytecode;
}

Vector<uint8_t> RenderingDevice::_shader_compile_binary_from_spirv(const Ref<RDShaderSPIRV> &p_spirv, const String &p_shader_name) {
	return _shader_compile_binary_from_spirv_with_options(p_spirv, p_shader_name, {});
}

Vector<uint8_t> RenderingDevice::_shader_compile_binary_from_spirv_with_options(const Ref<RDShaderSPIRV> &p_spirv, const String &p_shader_name, BitField<ShaderCompileFlags> p_flags) {
	ERR_FAIL_COND_V(p_spirv.is_null(), Vector<uint8_t>());

	Vector<ShaderStageSPIRVData> stage_data;
	for (int i = 0; i < RD::SHADER_STAGE_MAX; i++) {
		ShaderStage stage = ShaderStage(i);
		ShaderStageSPIRVData sd;
		sd.shader_stage = stage;
		String error = p_spirv->get_stage_compile_error(stage);
		ERR_FAIL_COND_V_MSG(!error.is_empty(), Vector<uint8_t>(), "Can't create a shader from an errored bytecode. Check errors in source bytecode.");
		sd.spirv = p_spirv->get_stage_bytecode(stage);
		if (sd.spirv.is_empty()) {
			continue;
		}
		stage_data.push_back(sd);
	}

	return shader_compile_binary_from_spirv_with_options(stage_data, p_shader_name, p_flags);
}

RID RenderingDevice::_shader_create_from_spirv(const Ref<RDShaderSPIRV> &p_spirv, const String &p_shader_name) {
	return _shader_create_from_spirv_with_options(p_spirv, p_shader_name, {});
}

RID RenderingDevice::_shader_create_from_spirv_with_options(const Ref<RDShaderSPIRV> &p_spirv, const String &p_shader_name, BitField<ShaderCompileFlags> p_flags) {
	ERR_FAIL_COND_V(p_spirv.is_null(), RID());

	Vector<ShaderStageSPIRVData> stage_data;
	for (int i = 0; i < RD::SHADER_STAGE_MAX; i++) {
		ShaderStage stage = ShaderStage(i);
		ShaderStageSPIRVData sd;
		sd.shader_stage = stage;
		String error = p_spirv->get_stage_compile_error(stage);
		ERR_FAIL_COND_V_MSG(!error.is_empty(), RID(), "Can't create a shader from an errored bytecode. Check errors in source bytecode.");
		sd.spirv = p_spirv->get_stage_bytecode(stage);
		if (sd.spirv.is_empty()) {
			continue;
		}
		stage_data.push_back(sd);
	}
	return shader_create_from_spirv_with_options(stage_data, p_shader_name, p_flags);
}

RID RenderingDevice::_uniform_set_create(const TypedArray<RDUniform> &p_uniforms, RID p_shader, uint32_t p_shader_set) {
	LocalVector<Uniform> uniforms;
	uniforms.resize(p_uniforms.size());
	for (int i = 0; i < p_uniforms.size(); i++) {
		Ref<RDUniform> uniform = p_uniforms[i];
		ERR_FAIL_COND_V(uniform.is_null(), RID());
		uniforms[i] = uniform->base;
	}
	return uniform_set_create(uniforms, p_shader, p_shader_set);
}

Error RenderingDevice::_buffer_update_bind(RID p_buffer, uint32_t p_offset, uint32_t p_size, const Vector<uint8_t> &p_data) {
	return buffer_update(p_buffer, p_offset, p_size, p_data.ptr());
}

RID RenderingDevice::_blas_create(const TypedArray<RDAccelerationStructureGeometry> &p_geometries, BitField<AccelerationStructureFlagBits> p_flags) {
	thread_local LocalVector<AccelerationStructureGeometry> geometries;
	geometries.resize(p_geometries.size());

	for (int i = 0; i < p_geometries.size(); i++) {
		Ref<RDAccelerationStructureGeometry> geometry = p_geometries[i];
		ERR_FAIL_COND_V(geometry.is_null(), RID());
		geometries[i] = geometry->base;
	}

	return blas_create(geometries, p_flags);
}

Error RenderingDevice::_tlas_build(RID p_tlas, const TypedArray<RDAccelerationStructureInstance> &p_instances) {
	thread_local LocalVector<AccelerationStructureInstance> instances;
	instances.resize(p_instances.size());

	for (int i = 0; i < p_instances.size(); i++) {
		Ref<RDAccelerationStructureInstance> instance = p_instances[i];
		ERR_FAIL_COND_V(instance.is_null(), ERR_INVALID_PARAMETER);
		instances[i] = instance->base;
	}

	return tlas_build(p_tlas, instances);
}

Error RenderingDevice::_hit_sbt_range_update(RID p_hit_sbt, HitShaderBindingTableRange p_range, uint32_t p_offset, const Vector<int32_t> &p_hit_group_indices) {
	return hit_sbt_range_update(p_hit_sbt, p_range, p_offset, Span<int32_t>(p_hit_group_indices).reinterpret<uint32_t>());
}

static Vector<RenderingDevice::PipelineSpecializationConstant> _get_spec_constants(const TypedArray<RDPipelineSpecializationConstant> &p_constants) {
	Vector<RenderingDevice::PipelineSpecializationConstant> ret;
	ret.resize(p_constants.size());
	for (int i = 0; i < p_constants.size(); i++) {
		Ref<RDPipelineSpecializationConstant> c = p_constants[i];
		ERR_CONTINUE(c.is_null());
		RenderingDevice::PipelineSpecializationConstant &sc = ret.write[i];
		Variant value = c->get_value();
		switch (value.get_type()) {
			case Variant::BOOL: {
				sc.type = RD::PIPELINE_SPECIALIZATION_CONSTANT_TYPE_BOOL;
				sc.bool_value = value;
			} break;
			case Variant::INT: {
				sc.type = RD::PIPELINE_SPECIALIZATION_CONSTANT_TYPE_INT;
				sc.int_value = value;
			} break;
			case Variant::FLOAT: {
				sc.type = RD::PIPELINE_SPECIALIZATION_CONSTANT_TYPE_FLOAT;
				sc.float_value = value;
			} break;
			default: {
			}
		}

		sc.constant_id = c->get_constant_id();
	}
	return ret;
}

RID RenderingDevice::_render_pipeline_create(RID p_shader, FramebufferFormatID p_framebuffer_format, VertexFormatID p_vertex_format, RenderPrimitive p_render_primitive, const Ref<RDPipelineRasterizationState> &p_rasterization_state, const Ref<RDPipelineMultisampleState> &p_multisample_state, const Ref<RDPipelineDepthStencilState> &p_depth_stencil_state, const Ref<RDPipelineColorBlendState> &p_blend_state, BitField<PipelineDynamicStateFlags> p_dynamic_state_flags, uint32_t p_for_render_pass, const TypedArray<RDPipelineSpecializationConstant> &p_specialization_constants) {
	PipelineRasterizationState rasterization_state;
	if (p_rasterization_state.is_valid()) {
		rasterization_state = p_rasterization_state->base;
	}

	PipelineMultisampleState multisample_state;
	if (p_multisample_state.is_valid()) {
		multisample_state = p_multisample_state->base;
		for (int i = 0; i < p_multisample_state->sample_masks.size(); i++) {
			int64_t mask = p_multisample_state->sample_masks[i];
			multisample_state.sample_mask.push_back(mask);
		}
	}

	PipelineDepthStencilState depth_stencil_state;
	if (p_depth_stencil_state.is_valid()) {
		depth_stencil_state = p_depth_stencil_state->base;
	}

	PipelineColorBlendState color_blend_state;
	if (p_blend_state.is_valid()) {
		color_blend_state = p_blend_state->base;
		for (int i = 0; i < p_blend_state->attachments.size(); i++) {
			Ref<RDPipelineColorBlendStateAttachment> attachment = p_blend_state->attachments[i];
			if (attachment.is_valid()) {
				color_blend_state.attachments.push_back(attachment->base);
			}
		}
	}

	return render_pipeline_create(p_shader, p_framebuffer_format, p_vertex_format, p_render_primitive, rasterization_state, multisample_state, depth_stencil_state, color_blend_state, p_dynamic_state_flags, p_for_render_pass, _get_spec_constants(p_specialization_constants));
}

RID RenderingDevice::_compute_pipeline_create(RID p_shader, const TypedArray<RDPipelineSpecializationConstant> &p_specialization_constants = TypedArray<RDPipelineSpecializationConstant>()) {
	return compute_pipeline_create(p_shader, _get_spec_constants(p_specialization_constants));
}

RID RenderingDevice::_raytracing_pipeline_create(const TypedArray<RDPipelineShader> &p_raygen_shaders, const TypedArray<RDPipelineShader> &p_miss_shaders, const TypedArray<RDHitGroup> &p_hit_groups, uint32_t p_max_trace_recursion_depth) {
	thread_local LocalVector<PipelineShader> raygen_and_miss_shaders;
	thread_local LocalVector<HitGroup> hit_groups;
	raygen_and_miss_shaders.clear();
	hit_groups.clear();

	auto _get_shader = [&](const RDPipelineShader *p_shader) -> PipelineShader {
		PipelineShader shader = p_shader->base;
		shader.specialization_constants = _get_spec_constants(p_shader->specialization_constants);
		return shader;
	};

	for (int i = 0; i < p_raygen_shaders.size(); i++) {
		Ref<RDPipelineShader> raygen_shader = p_raygen_shaders[i];
		ERR_FAIL_COND_V(raygen_shader.is_null(), RID());

		raygen_and_miss_shaders.push_back(_get_shader(*raygen_shader));
	}

	for (int i = 0; i < p_miss_shaders.size(); i++) {
		Ref<RDPipelineShader> miss_shader = p_miss_shaders[i];
		ERR_FAIL_COND_V(miss_shader.is_null(), RID());

		raygen_and_miss_shaders.push_back(_get_shader(*miss_shader));
	}

	for (int i = 0; i < p_hit_groups.size(); i++) {
		Ref<RDHitGroup> rd_hit_group = p_hit_groups[i];
		ERR_FAIL_COND_V(rd_hit_group.is_null(), RID());

		HitGroup hit_group;
		if (rd_hit_group->closest_hit_shader.is_valid()) {
			hit_group.closest_hit_shader = _get_shader(*rd_hit_group->closest_hit_shader);
		}
		if (rd_hit_group->any_hit_shader.is_valid()) {
			hit_group.any_hit_shader = _get_shader(*rd_hit_group->any_hit_shader);
		}
		if (rd_hit_group->intersection_shader.is_valid()) {
			hit_group.intersection_shader = _get_shader(*rd_hit_group->intersection_shader);
		}

		hit_groups.push_back(hit_group);
	}

	return raytracing_pipeline_create(
			Span<PipelineShader>(raygen_and_miss_shaders.ptr(), p_raygen_shaders.size()),
			Span<PipelineShader>(raygen_and_miss_shaders.ptr() + p_raygen_shaders.size(), p_miss_shaders.size()),
			hit_groups,
			p_max_trace_recursion_depth);
}

#ifndef DISABLE_DEPRECATED
Vector<int64_t> RenderingDevice::_draw_list_begin_split(RID p_framebuffer, uint32_t p_splits, InitialAction p_initial_color_action, FinalAction p_final_color_action, InitialAction p_initial_depth_action, FinalAction p_final_depth_action, const Vector<Color> &p_clear_color_values, float p_clear_depth, uint32_t p_clear_stencil, const Rect2 &p_region, const TypedArray<RID> &p_storage_textures) {
	ERR_FAIL_V_MSG(Vector<int64_t>(), "Deprecated. Split draw lists are used automatically by RenderingDevice.");
}

Vector<int64_t> RenderingDevice::_draw_list_switch_to_next_pass_split(uint32_t p_splits) {
	ERR_FAIL_V_MSG(Vector<int64_t>(), "Deprecated. Split draw lists are used automatically by RenderingDevice.");
}
#endif

void RenderingDevice::_draw_list_set_push_constant(DrawListID p_list, const Vector<uint8_t> &p_data, uint32_t p_data_size) {
	ERR_FAIL_COND(p_data_size > (uint32_t)p_data.size());
	draw_list_set_push_constant(p_list, p_data.ptr(), p_data_size);
}

void RenderingDevice::_compute_list_set_push_constant(ComputeListID p_list, const Vector<uint8_t> &p_data, uint32_t p_data_size) {
	ERR_FAIL_COND(p_data_size > (uint32_t)p_data.size());
	compute_list_set_push_constant(p_list, p_data.ptr(), p_data_size);
}

void RenderingDevice::_raytracing_list_set_push_constant(RaytracingListID p_list, const Vector<uint8_t> &p_data, uint32_t p_data_size) {
	ERR_FAIL_COND(p_data_size > (uint32_t)p_data.size());
	raytracing_list_set_push_constant(p_list, p_data.ptr(), p_data_size);
}

static_assert(ENUM_MEMBERS_EQUAL(RD::CALLBACK_RESOURCE_USAGE_NONE, RDG::RESOURCE_USAGE_NONE));
static_assert(ENUM_MEMBERS_EQUAL(RD::CALLBACK_RESOURCE_USAGE_COPY_FROM, RDG::RESOURCE_USAGE_COPY_FROM));
static_assert(ENUM_MEMBERS_EQUAL(RD::CALLBACK_RESOURCE_USAGE_COPY_TO, RDG::RESOURCE_USAGE_COPY_TO));
static_assert(ENUM_MEMBERS_EQUAL(RD::CALLBACK_RESOURCE_USAGE_RESOLVE_FROM, RDG::RESOURCE_USAGE_RESOLVE_FROM));
static_assert(ENUM_MEMBERS_EQUAL(RD::CALLBACK_RESOURCE_USAGE_RESOLVE_TO, RDG::RESOURCE_USAGE_RESOLVE_TO));
static_assert(ENUM_MEMBERS_EQUAL(RD::CALLBACK_RESOURCE_USAGE_UNIFORM_BUFFER_READ, RDG::RESOURCE_USAGE_UNIFORM_BUFFER_READ));
static_assert(ENUM_MEMBERS_EQUAL(RD::CALLBACK_RESOURCE_USAGE_INDIRECT_BUFFER_READ, RDG::RESOURCE_USAGE_INDIRECT_BUFFER_READ));
static_assert(ENUM_MEMBERS_EQUAL(RD::CALLBACK_RESOURCE_USAGE_TEXTURE_BUFFER_READ, RDG::RESOURCE_USAGE_TEXTURE_BUFFER_READ));
static_assert(ENUM_MEMBERS_EQUAL(RD::CALLBACK_RESOURCE_USAGE_TEXTURE_BUFFER_READ_WRITE, RDG::RESOURCE_USAGE_TEXTURE_BUFFER_READ_WRITE));
static_assert(ENUM_MEMBERS_EQUAL(RD::CALLBACK_RESOURCE_USAGE_STORAGE_BUFFER_READ, RDG::RESOURCE_USAGE_STORAGE_BUFFER_READ));
static_assert(ENUM_MEMBERS_EQUAL(RD::CALLBACK_RESOURCE_USAGE_STORAGE_BUFFER_READ_WRITE, RDG::RESOURCE_USAGE_STORAGE_BUFFER_READ_WRITE));
static_assert(ENUM_MEMBERS_EQUAL(RD::CALLBACK_RESOURCE_USAGE_VERTEX_BUFFER_READ, RDG::RESOURCE_USAGE_VERTEX_BUFFER_READ));
static_assert(ENUM_MEMBERS_EQUAL(RD::CALLBACK_RESOURCE_USAGE_INDEX_BUFFER_READ, RDG::RESOURCE_USAGE_INDEX_BUFFER_READ));
static_assert(ENUM_MEMBERS_EQUAL(RD::CALLBACK_RESOURCE_USAGE_TEXTURE_SAMPLE, RDG::RESOURCE_USAGE_TEXTURE_SAMPLE));
static_assert(ENUM_MEMBERS_EQUAL(RD::CALLBACK_RESOURCE_USAGE_STORAGE_IMAGE_READ, RDG::RESOURCE_USAGE_STORAGE_IMAGE_READ));
static_assert(ENUM_MEMBERS_EQUAL(RD::CALLBACK_RESOURCE_USAGE_STORAGE_IMAGE_READ_WRITE, RDG::RESOURCE_USAGE_STORAGE_IMAGE_READ_WRITE));
static_assert(ENUM_MEMBERS_EQUAL(RD::CALLBACK_RESOURCE_USAGE_ATTACHMENT_COLOR_READ_WRITE, RDG::RESOURCE_USAGE_ATTACHMENT_COLOR_READ_WRITE));
static_assert(ENUM_MEMBERS_EQUAL(RD::CALLBACK_RESOURCE_USAGE_ATTACHMENT_DEPTH_STENCIL_READ_WRITE, RDG::RESOURCE_USAGE_ATTACHMENT_DEPTH_STENCIL_READ_WRITE));
static_assert(ENUM_MEMBERS_EQUAL(RD::CALLBACK_RESOURCE_USAGE_GENERAL, RDG::RESOURCE_USAGE_GENERAL));
static_assert(ENUM_MEMBERS_EQUAL(RD::CALLBACK_RESOURCE_USAGE_MAX, RDG::RESOURCE_USAGE_MAX));

// Capture original producer outcomes after the caller has admitted the operation.
// The copied identity does not itself grant operation or resource permission.
GDExtensionDeviceExit::CheckedRecordAppend RenderingDevice::_record_source_dispatch(const GDExtensionDeviceExit::OriginalOperationIdentity &identity, ComputeListID list, uint32_t x, uint32_t y, uint32_t z, const void *push_data, uint32_t push_size) {
	using Observation = GDExtensionDeviceExit::CheckedRecordAppend;
	Observation observed;
	observed.error = GDExtensionDeviceExit::bind_checked_record_append(observed, identity);
	if (observed.error != OK) {
		return observed;
	}
	if (render_thread_id != Thread::get_caller_id()) {
		observed.error = ERR_UNAUTHORIZED;
		return observed;
	}
	if (_recording_error() != OK || list != ID_TYPE_COMPUTE_LIST || !compute_list.active || !x || !y || !z || x > driver->limit_get(LIMIT_MAX_COMPUTE_WORKGROUP_COUNT_X) || y > driver->limit_get(LIMIT_MAX_COMPUTE_WORKGROUP_COUNT_Y) || z > driver->limit_get(LIMIT_MAX_COMPUTE_WORKGROUP_COUNT_Z)) {
		observed.error = ERR_INVALID_PARAMETER;
		return observed;
	}
	auto *pipeline = compute_pipeline_owner.get_or_null(compute_list.state.pipeline);
	if (!pipeline || !pipeline->driver_id || !pipeline->shader_driver_id || !shader_owner.owns(pipeline->shader) || compute_list.state.set_count > MAX_UNIFORM_SETS || pipeline->set_formats.size() != compute_list.state.set_count) {
		observed.error = ERR_INVALID_DATA;
		return observed;
	}
	const auto *shader = shader_owner.get_or_null(pipeline->shader);
	if (!shader || shader->driver_id != pipeline->shader_driver_id || compute_list.state.pipeline_shader != pipeline->shader || compute_list.state.pipeline_shader_driver_id != pipeline->shader_driver_id) {
		observed.error = ERR_INVALID_DATA;
		return observed;
	}
	// The identity joins the original operation; it never authorizes another
	// family's resources. Preserve the existing qualification ownership boundary.
	if ((pipeline->qualification_return && ((!qualification_recording && !_production_resource_permitted(compute_list.state.pipeline)) || pipeline->qualification_return->use_closed.load())) ||
			(shader->qualification_return && ((!qualification_recording && !_production_resource_permitted(pipeline->shader)) || shader->qualification_return->use_closed.load()))) {
		observed.error = ERR_UNAUTHORIZED;
		return observed;
	}
#ifdef DEBUG_ENABLED
	if (!compute_list.validation.pipeline_active || compute_list.validation.pipeline_push_constant_size != pipeline->push_constant_size) {
		observed.error = ERR_INVALID_DATA;
		return observed;
	}
#endif
	if (push_size != pipeline->push_constant_size || (push_size && (!push_data || push_size > MAX_PUSH_CONSTANT_SIZE))) {
		observed.error = ERR_UNAVAILABLE;
		return observed;
	}
	for (uint32_t i = 0; i < compute_list.state.set_count; ++i) {
		const auto &binding = compute_list.state.sets[i];
		if (!binding.pipeline_expected_format) {
			continue;
		}
		auto *set = uniform_set_owner.get_or_null(binding.uniform_set);
		if (set && !qualification_recording && !_production_resource_permitted(binding.uniform_set) && (set->qualification_return || !set->qualification_dependencies.is_empty())) {
			observed.error = ERR_UNAUTHORIZED;
			return observed;
		}
		if (!set || !set->driver_id || set->driver_id != binding.uniform_set_driver_id || set->shader_set != i || set->format != pipeline->set_formats[i] || pipeline->set_formats[i] != binding.pipeline_expected_format || binding.pipeline_expected_format != binding.uniform_set_format || !set->shared_textures_to_update.is_empty() || !set->pending_clear_textures.is_empty()) {
			observed.error = ERR_UNAVAILABLE;
			return observed;
		}
	}
	// Every checked/public dispatch refusal is above the first push effect.
	// The shared mechanical tail has no failing return after descriptor prepare;
	// empty shared/clear lists above also exclude hidden preparation work.
	if (push_size) {
		compute_list_set_push_constant(list, push_data, push_size);
	}
	observed.handle = _compute_list_dispatch_recorded(list, x, y, z);
	observed.state = observed.handle.is_valid() ? Observation::State::Appended : Observation::State::Unknown;
	observed.error = observed.handle.is_valid() ? OK : FAILED;
	return observed;
}
GDExtensionDeviceExit::CheckedRecordAppend RenderingDevice::_record_source_buffer_copy(const GDExtensionDeviceExit::OriginalOperationIdentity &identity, RID from, RID to, uint32_t from_offset, uint32_t to_offset, uint32_t size) {
	using Observation = GDExtensionDeviceExit::CheckedRecordAppend;
	Observation observed;
	observed.error = GDExtensionDeviceExit::bind_checked_record_append(observed, identity);
	if (observed.error != OK) {
		return observed;
	}
	if (render_thread_id != Thread::get_caller_id()) {
		observed.error = ERR_UNAUTHORIZED;
		return observed;
	}
	auto *src = _get_buffer_from_owner(from);
	auto *dst = _get_buffer_from_owner(to);
	if (!src || !dst || !src->driver_id || !dst->driver_id) {
		observed.error = ERR_UNAVAILABLE;
		return observed;
	}
	const auto actual = _buffer_copy_recorded(from, to, from_offset, to_offset, size);
	observed.transfer_count = actual.transfer_count;
	for (uint32_t i = 0; i < actual.transfer_count; ++i) {
		observed.transfers[i] = { actual.transfers[i].worker, actual.transfers[i].operation };
	}
	observed.became_mutable = actual.destination_became_mutable;
	observed.handle = actual.copy_handle;
	observed.error = actual.error;
	observed.state = actual.effect == NativeAppendEffect::Recorded ? Observation::State::Appended : actual.effect == NativeAppendEffect::NoNewEffect ? Observation::State::Rejected
																																					 : Observation::State::Unknown;
	return observed;
}

GDExtensionDeviceExit::CheckedRecordAppend RenderingDevice::_record_source_buffer_upload(const GDExtensionDeviceExit::OriginalOperationIdentity &identity, RID destination, uint32_t offset, uint32_t size, const void *data, GDExtensionDeviceExit::BufferUploadSourceCapture &capture) {
	using Observation = GDExtensionDeviceExit::CheckedRecordAppend;
	Observation observed;
	observed.error = GDExtensionDeviceExit::bind_checked_record_append(observed, identity);
	if (observed.error != OK) {
		return observed;
	}
	if (render_thread_id != Thread::get_caller_id()) {
		observed.error = ERR_UNAUTHORIZED;
		return observed;
	}
	auto *buffer = _get_buffer_from_owner(destination);
	if (_recording_error() != OK || draw_list.active || compute_list.active || raytracing_list.active || !buffer || !buffer->driver_id || !size || !data || offset > buffer->size || size > buffer->size - offset) {
		observed.error = ERR_INVALID_PARAMETER;
		return observed;
	}
	if (buffer->qualification_return && !_production_resource_permitted(destination)) {
		observed.error = ERR_UNAUTHORIZED;
		return observed;
	}
	const uint32_t first = capture.count;
	const uint32_t consumed = capture.consumed;
	observed.error = _buffer_update(buffer, destination, offset, size, data, nullptr, &capture);
	if (capture.transfer_worker != UINT32_MAX) {
		observed.transfers[0] = { capture.transfer_worker, capture.transfer_operation };
		observed.transfer_count = 1;
	}
	observed.became_mutable = capture.became_mutable;
	if (capture.count > first) {
		observed.handle = capture.segment(capture.count - 1).handle;
		bool exact = true;
		uint32_t next = consumed;
		for (uint32_t i = first; i < capture.count; ++i) {
			const auto &segment = capture.segment(i);
			exact &= segment.handle.is_valid() && draw_graph.contains_record(segment.handle) && segment.begin == next && segment.end > segment.begin && segment.end <= size;
			next = segment.end;
		}
		exact &= next == capture.consumed;
		observed.state = !exact ? Observation::State::Unknown : capture.consumed == size ? Observation::State::Appended
																						 : Observation::State::KnownPrefix;
	} else if (capture.prefix_possible) {
		observed.state = Observation::State::KnownPrefix;
	}
	return observed;
}

RenderingDevice::SourceDrawAppendResult RenderingDevice::_record_source_draw_indirect(const GDExtensionDeviceExit::OriginalOperationIdentity &identity, DrawListID list, bool use_indices, RID buffer, uint32_t offset, uint32_t draw_count, uint32_t stride) {
	using Observation = GDExtensionDeviceExit::CheckedRecordAppend;
	SourceDrawAppendResult result;
	result.observed.error = GDExtensionDeviceExit::bind_checked_record_append(result.observed, identity);
	if (result.observed.error != OK) {
		result.native.error = result.observed.error;
		return result;
	}
	if (render_thread_id != Thread::get_caller_id()) {
		result.observed.error = ERR_UNAUTHORIZED;
		result.native.error = result.observed.error;
		return result;
	}
	if (list != (int64_t(ID_TYPE_DRAW_LIST) << ID_BASE_SHIFT)) {
		result.observed.error = ERR_INVALID_PARAMETER;
		result.native.error = result.observed.error;
		return result;
	}
	// Keep the original checked draw as the only span/preparation/append path.
	// Unknown includes a refused draw with real descriptor-prefix obligations.
	result.native = _draw_list_draw_indirect_recorded(list, use_indices, buffer, offset, draw_count, stride, true);
	result.observed.handle = result.native.draw_handle;
	result.observed.error = result.native.error;
	result.observed.state = result.native.effect == NativeAppendEffect::Recorded ? Observation::State::Appended : result.native.effect == NativeAppendEffect::NoNewEffect ? Observation::State::Rejected
																																										  : Observation::State::Unknown;
	return result;
}

// An existing request's exact reverse pins permit these mechanical resources
// only during its core-owned record borrow. This is not an admission issuer.
bool RenderingDevice::_production_resource_permitted(RID rid) const {
	const auto *request = production_recording_request;
	if (!request || !request->serial || !request->frozen || !request->capture_inflight) {
		return false;
	}
	for (uint32_t i = 0; i < request->resource_count; ++i) {
		const auto &slot = request->resources[i];
		if (!slot.imported || (slot.primary != rid && slot.secondary != rid)) {
			continue;
		}
		// The original capture_inflight pin and RD recording lock stabilize
		// these rows on RT while Registry is unlocked for the native call.
		// Recheck owner closure as well as the exact actual factory witness.
		if (slot.output_alias_prepared && GDExtensionDeviceExit::Registry::get_singleton().production_output_alias_matches_locked(*request, slot, *const_cast<RenderingDevice *>(this))) {
			return true;
		}
		for (uint32_t u = 0; u < slot.use_count; ++u) {
			const auto &use = slot.uses[u];
			if (use.rid == rid.get_id() && use.request == request && use.request_serial == request->serial && use.same_physical() && use.linked() && !use.physical->use_closed.load()) {
				return true;
			}
		}
	}
	return false;
}

// Keep the original Core-to-native service edge in this already rooted server
// object, so static archive extraction does not depend on Core link order.
bool RenderingDevice::_production_output_owner_matches(
		GDExtensionDeviceExit::ProductionOutputOwnerKind p_kind, RID p_particles, uint64_t p_generation, uint32_t p_slot, uint64_t p_lease,
		const GDExtensionDeviceExit::ProductionOutputPin &p_pin,
		GDExtensionProductionRequestRefV1 p_request) {
	return RendererRD::ParticlesStorage::_production_output_owner_matches(
			p_kind, p_particles, p_generation, p_slot, p_lease, p_pin, *this, p_request);
}
Error RenderingDevice::_production_output_generation_ready(RID p_particles, uint64_t p_generation,
		const GDExtensionDeviceExit::ProductionOutputPin &p_pin, GDExtensionProductionRequestRefV1 p_request,
		uint32_t &r_unused, uint32_t &r_returned) {
	return RendererRD::ParticlesStorage::_production_output_generation_ready(p_particles, p_generation, p_pin, *this, p_request, r_unused, r_returned);
}
Error RenderingDevice::_production_output_owner_service(
		GDExtensionDeviceExit::ProductionOutputOwnerKind p_kind, RID p_particles, uint64_t p_generation, uint32_t p_slot, uint64_t p_lease,
		GDExtensionDeviceExit::ProductionOutputPin &p_pin,
		const GDExtensionProductionServiceBudgetV1 &p_budget,
		GDExtensionDeviceExit::ProductionServiceUsage &r_usage) {
	return RendererRD::ParticlesStorage::_production_output_service(
			p_kind, p_particles, p_generation, p_slot, p_lease, p_pin, p_budget, r_usage);
}

// The four original output leaves live in this already rooted server object,
// keeping Core registration independent of static server archive link order.
namespace GDExtensionDeviceExit {
namespace {
struct OutputMemoryRange {
	uintptr_t begin = 0, end = 0;
};
bool output_range(const void *p_data, uint64_t p_count, size_t p_stride, size_t p_alignment, OutputMemoryRange &r_range) {
	r_range = {};
	if (!p_count) {
		return true;
	}
	const uintptr_t begin = reinterpret_cast<uintptr_t>(p_data);
	if (!begin || begin % p_alignment || p_count > (UINTPTR_MAX - begin) / p_stride) {
		return false;
	}
	r_range = { begin, begin + uintptr_t(p_count * p_stride) };
	return true;
}
bool output_overlaps(const OutputMemoryRange &p_a, const OutputMemoryRange &p_b) {
	return p_a.begin < p_b.end && p_b.begin < p_a.end;
}
template <typename T>
bool output_object(const T *p_value, OutputMemoryRange &r_range) {
	return output_range(p_value, 1, sizeof(T), alignof(T), r_range);
}
template <size_t N>
GDExtensionOutputStatusV1 output_interface_status(Error p_error, const char (&p_detail)[N]) {
	return { int32_t(p_error), uint32_t(N - 1), p_detail };
}
template <typename T>
void output_interface_failure(T &r_result, Error p_error, const char *p_detail, uint32_t p_length) {
	r_result = {};
	r_result.abi = { 1, sizeof(T), 0 };
	r_result.status = { int32_t(p_error), p_length, p_detail };
}
template <typename T>
bool output_create_spans(const OutputMemoryRange &p_input, const OutputMemoryRange &p_output, const GDExtensionOutputCreateInputV1 &p_config, T &r_result) {
	OutputMemoryRange surfaces, materials;
	if (!output_range(p_config.surfaces.data, p_config.surfaces.count, sizeof(uint64_t), alignof(uint64_t), surfaces) ||
			!output_range(p_config.surface_materials.data, p_config.surface_materials.count, sizeof(uint64_t), alignof(uint64_t), materials)) {
		// An invalid span may designate the output: no write until all extents
		// have been proven disjoint. This is a caller contract failure.
		return false;
	}
	if (output_overlaps(p_output, surfaces) || output_overlaps(p_output, materials)) {
		return false;
	}
	if (output_overlaps(p_input, surfaces) || output_overlaps(p_input, materials) || output_overlaps(surfaces, materials)) {
		output_interface_failure(r_result, ERR_INVALID_PARAMETER, "CONTRACT", 8);
		return false;
	}
	return true;
}
} // namespace

void device_session_output_create_v1(GDExtensionProductionPhysicalContextRefV1 p_context, GDExtensionProductionRequestRefV1 p_request, const GDExtensionOutputCreateInputV1 *p_input, GDExtensionOutputCreateResultV1 *r_result) {
	OutputMemoryRange input, output;
	if (!output_object(r_result, output)) {
		return;
	}
	if (!output_object(p_input, input)) {
		return; // Invalid input extent cannot establish nonaliasing output.
	}
	if (output_overlaps(input, output)) {
		return; // Do not overwrite borrowed input through an aliased output.
	}
	if (!output_create_spans(input, output, *p_input, *r_result)) {
		return;
	}
	auto *storage = RendererRD::ParticlesStorage::get_singleton();
	if (!storage) {
		output_interface_failure(*r_result, ERR_UNAVAILABLE, "renderer_unavailable", 20);
		return;
	}
	storage->particles_external_create_typed(p_context, p_request, *p_input, *r_result);
}
void device_session_output_metadata_query_v1(const GDExtensionOutputMetadataQueryInputV1 *p_input, GDExtensionOutputMetadataQueryResultV1 *r_result) {
	OutputMemoryRange input, output;
	if (!output_object(r_result, output) || !output_object(p_input, input) || output_overlaps(input, output)) {
		return;
	}
	if (!output_create_spans(input, output, p_input->config, *r_result)) {
		return;
	}
	auto *storage = RendererRD::ParticlesStorage::get_singleton();
	if (!storage) {
		output_interface_failure(*r_result, ERR_UNAVAILABLE, "renderer_unavailable", 20);
		return;
	}
	storage->particles_external_metadata_query(*p_input, *r_result);
}
void device_session_output_create_v3(GDExtensionProductionPhysicalContextRefV1 p_context, GDExtensionProductionRequestRefV1 p_request, const GDExtensionOutputCreateInputV3 *p_input, GDExtensionOutputCreateResultV1 *r_result) {
	OutputMemoryRange input, output;
	if (!output_object(r_result, output) || !output_object(p_input, input) || output_overlaps(input, output)) {
		return;
	}
	if (!output_create_spans(input, output, p_input->config, *r_result)) {
		return;
	}
	auto *storage = RendererRD::ParticlesStorage::get_singleton();
	if (!storage) {
		output_interface_failure(*r_result, ERR_UNAVAILABLE, "renderer_unavailable", 20);
		return;
	}
	storage->particles_external_create_typed_v3(p_context, p_request, *p_input, *r_result);
}
void device_session_output_create_v2(GDExtensionProductionPhysicalContextRefV1 p_context, GDExtensionProductionRequestRefV1 p_request, const GDExtensionOutputCreateInputV2 *p_input, GDExtensionOutputCreateResultV1 *r_result) {
	OutputMemoryRange input, output;
	if (!output_object(r_result, output) || !output_object(p_input, input) || output_overlaps(input, output)) {
		return;
	}
	// Include the outer header and resource selector in the forbidden input
	// range. Checking only the nested config would allow a span to alias them.
	if (!output_create_spans(input, output, p_input->config, *r_result)) {
		return;
	}
	if (p_input->abi.abi_version != 2 || p_input->abi.struct_size != sizeof(*p_input) || p_input->abi.reserved ||
			p_input->reserved || p_input->metadata_resource_slot == UINT32_MAX) {
		output_interface_failure(*r_result, ERR_INVALID_PARAMETER, "CONTRACT", 8);
		return;
	}
	auto *storage = RendererRD::ParticlesStorage::get_singleton();
	if (!storage) {
		output_interface_failure(*r_result, ERR_UNAVAILABLE, "renderer_unavailable", 20);
		return;
	}
	storage->particles_external_create_typed_v2(p_context, p_request, p_input->config, p_input->metadata_resource_slot, *r_result);
}
void device_session_output_acquire_v1(GDExtensionProductionPhysicalContextRefV1 p_context, GDExtensionProductionRequestRefV1 p_request, uint64_t p_action_ordinal, const GDExtensionOutputAcquireInputV1 *p_input, GDExtensionOutputAcquireResultV1 *r_result) {
	OutputMemoryRange input, output;
	if (!output_object(r_result, output) || !output_object(p_input, input) || output_overlaps(input, output)) {
		return;
	}
	Registry::get_singleton().production_output_acquire(p_context, p_request, p_action_ordinal, p_input, r_result);
}
void device_session_output_poll_v1(GDExtensionProductionPhysicalContextRefV1 p_context, GDExtensionProductionRequestRefV1 p_request, const GDExtensionOutputGenerationInputV1 *p_input, GDExtensionOutputPollResultV1 *r_result) {
	OutputMemoryRange input, output;
	if (!output_object(r_result, output) || !output_object(p_input, input) || output_overlaps(input, output)) {
		return;
	}
	Registry::get_singleton().production_output_poll(p_context, p_request, p_input, r_result);
}
void device_session_output_seal_v1(GDExtensionProductionPhysicalContextRefV1 p_context, GDExtensionProductionRequestRefV1 p_request, const GDExtensionOutputSealInputV1 *p_input, GDExtensionOutputSealResultV1 *r_result) {
	OutputMemoryRange input, output;
	if (!output_object(r_result, output)) {
		return;
	}
	if (!output_object(p_input, input)) {
		return; // Invalid input extent cannot establish nonaliasing output.
	}
	if (output_overlaps(input, output)) {
		return;
	}
	auto *storage = RendererRD::ParticlesStorage::get_singleton();
	if (!storage) {
		output_interface_failure(*r_result, ERR_UNAVAILABLE, "renderer_unavailable", 20);
		return;
	}
	storage->particles_external_seal_typed(p_context, p_request, *p_input, *r_result);
}
GDExtensionOutputStatusV1 device_session_output_cancel_v1(GDExtensionProductionPhysicalContextRefV1 p_context, GDExtensionProductionRequestRefV1 p_request, const GDExtensionOutputCancelInputV1 *p_input) {
	OutputMemoryRange input;
	if (!output_object(p_input, input)) {
		return output_interface_status(ERR_INVALID_PARAMETER, "CONTRACT");
	}
	auto *storage = RendererRD::ParticlesStorage::get_singleton();
	return storage ? storage->particles_external_cancel_typed(p_context, p_request, *p_input) : output_interface_status(ERR_UNAVAILABLE, "renderer_unavailable");
}
GDExtensionOutputStatusV1 device_session_output_withdraw_v1(GDExtensionProductionPhysicalContextRefV1 p_context, GDExtensionProductionRequestRefV1 p_request, const GDExtensionOutputGenerationInputV1 *p_input) {
	OutputMemoryRange input;
	if (!output_object(p_input, input)) {
		return output_interface_status(ERR_INVALID_PARAMETER, "CONTRACT");
	}
	auto *storage = RendererRD::ParticlesStorage::get_singleton();
	return storage ? storage->particles_external_withdraw_typed(p_context, p_request, *p_input) : output_interface_status(ERR_UNAVAILABLE, "renderer_unavailable");
}
} // namespace GDExtensionDeviceExit
