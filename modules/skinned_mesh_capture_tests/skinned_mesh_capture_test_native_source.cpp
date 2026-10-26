/**************************************************************************/
/*  skinned_mesh_capture_test_native_source.cpp                           */
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

#include "skinned_mesh_capture_test_native_source.h"

#include "skinned_mesh_capture_test_observer.h"
#include "skinned_mesh_capture_test_phase_observer.h"
#include "skinned_mesh_capture_test_write_census.h"

#include "servers/rendering/render_capture_test_hooks.h"
#include "servers/rendering/renderer_rd/storage_rd/mesh_storage.h"
#include "servers/rendering/renderer_scene_cull.h"
#include "servers/rendering/rendering_server.h"
#include "servers/rendering/rendering_server_globals.h"

#include "modules/skinned_mesh_capture_tests/render_capture_test_access.h"

#include <mutex>

Error SkinnedMeshCaptureTestNativeSource::capture(RID p_scene, RID p_mesh, RID p_skeleton, SkinnedMeshCaptureTestRDPool &p_pool, uint32_t p_slot, SkinnedMeshCaptureTestNativeSourceObservation &r_observation, const SkinnedMeshCaptureTestDrawObservation *p_draw, bool p_previous) {
	RenderingDevice *rd = RenderingDevice::get_singleton();
	RenderingServer *server = RenderingServer::get_singleton();
	RendererSceneCull *scene = RenderCaptureTestAccess::scene_singleton();
	RendererRD::MeshStorage *storage = RendererRD::MeshStorage::get_singleton();
	if (!rd || !server || !server->is_on_render_thread() || !scene || !storage) {
		return ERR_UNAVAILABLE;
	}
	uint64_t capture_frame = 0;
	bool after_views = false;
	if (!skinned_mesh_capture_test_phase_capture_window(capture_frame, after_views)) {
		return ERR_UNAVAILABLE;
	}
	if (RenderCaptureTestAccess::draw_list(rd).active || RenderCaptureTestAccess::compute_list(rd).active || RenderCaptureTestAccess::raytracing_list(rd).active) {
		return ERR_BUSY;
	}
	if (RenderCaptureTestAccess::capture_test_coverage_failed(rd)) {
		return ERR_INVALID_DATA;
	}
	const auto *instance = RenderCaptureTestAccess::instance(scene, p_scene);
	if (!instance || instance->base_type != RSE::INSTANCE_MESH || instance->base != p_mesh || instance->skeleton != p_skeleton || instance->mesh_instance.is_null()) {
		return ERR_INVALID_DATA;
	}
	const auto *mesh = RenderCaptureTestAccess::mesh(storage, p_mesh);
	const auto *mi = RenderCaptureTestAccess::mesh_instance(storage, instance->mesh_instance);
	const auto *skeleton = RenderCaptureTestAccess::skeleton(storage, p_skeleton);
	if (!mesh || !mi || !skeleton || mi->mesh != mesh || mi->skeleton != p_skeleton || mesh->surface_count != 1 || mi->surfaces.size() != 1 || mesh->blend_shape_count != 0 || skeleton->size != 2 || skeleton->use_2d) {
		return ERR_INVALID_DATA;
	}
	const auto *surface = mesh->surfaces[0];
	constexpr uint64_t attributes = (1ULL << RSE::ARRAY_MAX) - 1;
	constexpr uint64_t required = RSE::ARRAY_FORMAT_VERTEX | RSE::ARRAY_FORMAT_BONES | RSE::ARRAY_FORMAT_WEIGHTS;
	constexpr uint64_t forbidden = RSE::ARRAY_FLAG_USE_2D_VERTICES | RSE::ARRAY_FLAG_USE_8_BONE_WEIGHTS | RSE::ARRAY_FLAG_COMPRESS_ATTRIBUTES | RSE::ARRAY_FLAG_USES_EMPTY_VERTEX_ARRAY;
	if (!surface || surface->primitive != RSE::PRIMITIVE_TRIANGLES || surface->vertex_count != 3 || surface->vertex_buffer_size != 36 || surface->skin_buffer_size != 48 || surface->index_count != 0 || (surface->format & attributes) != required || (surface->format & forbidden)) {
		return ERR_INVALID_DATA;
	}
	const auto &target = mi->surfaces[0];
	if (target.current_buffer > 1 || target.previous_buffer > 1) {
		return ERR_INVALID_DATA;
	}
	uint32_t selected_index = target.current_buffer;
	if (p_draw) {
		if (!after_views || p_draw->frame != capture_frame || p_draw->mesh_instance != instance->mesh_instance.get_id() || p_draw->surface != 0 || p_draw->device != uint64_t(rd->get_instance_id())) {
			return ERR_INVALID_DATA;
		}
		selected_index = p_draw->indices[p_previous ? 1 : 0];
		if (selected_index > 1 || target.vertex_buffer[selected_index].get_id() != p_draw->buffers[p_previous ? 1 : 0]) {
			return ERR_INVALID_DATA;
		}
	}
	// No dispatch call means certainly uninitialized. A nonzero count alone
	// cannot qualify initialization: the caller still needs actual t0 raw proof.
	if (target.capture_test_dispatch_count[selected_index] == 0 || target.capture_test_dispatch_count[selected_index] == UINT64_MAX) {
		return ERR_UNAVAILABLE;
	}
	const RID buffer_rid = target.vertex_buffer[selected_index];
	const auto *buffer = RenderCaptureTestAccess::buffer(rd, buffer_rid);
	if (!buffer || buffer->size < 36 || !buffer->capture_test_allocation_serial || RenderCaptureTestAccess::capture_test_allocation_registry(rd).resolve(buffer->driver_id.id, buffer_rid.get_id()) != buffer->capture_test_allocation_serial) {
		return ERR_INVALID_DATA;
	}
	if (RenderCaptureTestAccess::capture_test_allocation_registry(rd).access_escaped(buffer->driver_id.id, buffer->capture_test_allocation_serial)) {
		return ERR_UNAVAILABLE;
	}
	if (p_draw && (buffer->capture_test_allocation_serial != p_draw->allocations[p_previous ? 1 : 0] || !skinned_mesh_capture_test_hook_buffer_unchanged(p_draw->device, buffer->capture_test_allocation_serial, p_draw->sequence, p_draw->epoch))) {
		return ERR_INVALID_DATA;
	}
	SkinnedMeshCaptureTestNativeSourceObservation result;
	result.draw_sequence = p_draw ? p_draw->sequence : 0;
	result.viewport = p_draw ? p_draw->viewport : 0;
	result.selected_previous = p_draw && p_previous;
	result.frame = capture_frame;
	result.after_views = after_views;
	result.scene = p_scene.get_id();
	result.mesh = p_mesh.get_id();
	result.mesh_instance = instance->mesh_instance.get_id();
	result.skeleton = p_skeleton.get_id();
	result.device = uint64_t(rd->get_instance_id());
	result.allocation = buffer->capture_test_allocation_serial;
	result.buffer = buffer_rid.get_id();
	result.dispatch_call_count = target.capture_test_dispatch_count[selected_index];
	result.skeleton_version = skeleton->version;
	result.mesh_skeleton_version = mi->skeleton_version;
	result.last_change = target.last_change;
	result.current = target.current_buffer;
	result.previous = target.previous_buffer;
	result.queued = mi->array_update_list.in_list();
	const Error error = p_pool.record_native(p_slot, buffer_rid, 0);
	if (error != OK) {
		return error;
	}
	r_observation = result;
	return OK;
}

static thread_local uint64_t current_viewport = 0;
RenderCaptureTestHooks::ViewportDrawScope::ViewportDrawScope(uint64_t p_viewport) : previous(current_viewport) {
	current_viewport = p_viewport;
}
RenderCaptureTestHooks::ViewportDrawScope::~ViewportDrawScope() {
	current_viewport = previous;
}
static std::mutex draw_observer_mutex;
static std::vector<SkinnedMeshCaptureTestDrawObservation> draw_observer_events;
static uint64_t draw_observer_epoch = 0;
uint64_t SkinnedMeshCaptureTestNativeSource::draw_record_count() {
	RenderingDevice *rd = RenderingDevice::get_singleton();
	RenderingServer *server = RenderingServer::get_singleton();
	if (!rd || !server || !server->is_on_render_thread() || !RenderCaptureTestAccess::draw_list(rd).active) {
		return 0;
	}
	return RenderCaptureTestAccess::draw_list(rd).state.draw_count;
}
bool SkinnedMeshCaptureTestNativeSource::observe_draw(RID p_mesh_instance, uint32_t p_surface, RID p_vertex_array, uint64_t p_before_count, uint32_t p_renderer, uint32_t p_pass) {
	if (!RenderCaptureTestHooks::write_census_active()) {
		return false;
	}
	RenderingDevice *rd = RenderingDevice::get_singleton();
	RenderingServer *server = RenderingServer::get_singleton();
	auto *storage = RendererRD::MeshStorage::get_singleton();
	if (!rd || !server || !server->is_on_render_thread() || !storage || !RenderCaptureTestAccess::draw_list(rd).active || RenderCaptureTestAccess::draw_list(rd).state.draw_count != p_before_count + 1 || p_before_count == UINT64_MAX) {
		return false;
	}
	const auto *mi = RenderCaptureTestAccess::mesh_instance(storage, p_mesh_instance);
	if (!mi || p_surface >= mi->surfaces.size()) {
		return false;
	}
	const auto &surface = mi->surfaces[p_surface];
	SkinnedMeshCaptureTestDrawObservation result;
	bool found = false;
	for (uint32_t i = 0; i < surface.version_count; i++) {
		const auto &version = surface.versions[i];
		if (version.vertex_array != p_vertex_array) {
			continue;
		}
		result.indices[0] = version.current_buffer;
		result.indices[1] = version.previous_buffer;
		result.previous_bound = version.input_motion_vectors && (version.input_mask & RSE::ARRAY_FORMAT_VERTEX);
		found = true;
		break;
	}
	if (!found || result.indices[0] > 1 || result.indices[1] > 1 || RenderCaptureTestAccess::capture_test_coverage_failed(rd)) {
		RenderCaptureTestHooks::write_census_poison();
		return false;
	}
	for (uint32_t role = 0; role < 2; role++) {
		const RID rid = surface.vertex_buffer[result.indices[role]];
		const auto *buffer = RenderCaptureTestAccess::buffer(rd, rid);
		if (!buffer || !buffer->capture_test_allocation_serial || RenderCaptureTestAccess::capture_test_allocation_registry(rd).resolve(buffer->driver_id.id, rid.get_id()) != buffer->capture_test_allocation_serial || RenderCaptureTestAccess::capture_test_allocation_registry(rd).access_escaped(buffer->driver_id.id, buffer->capture_test_allocation_serial)) {
			RenderCaptureTestHooks::write_census_poison();
			return false;
		}
		result.buffers[role] = rid.get_id();
		result.allocations[role] = buffer->capture_test_allocation_serial;
	}
	// Original operation marker shares the hazard publication sequence.
	const auto marker = skinned_mesh_capture_test_observe_draw_record(uint64_t(rd->get_instance_id()), p_mesh_instance.get_id(), p_vertex_array.get_id());
	if (!marker.sequence || skinned_mesh_capture_test_hook_is_poisoned()) {
		return false;
	}
	result.epoch = marker.epoch;
	result.sequence = marker.sequence;
	result.frame = RSG::rasterizer->get_frame_number();
	result.mesh_instance = p_mesh_instance.get_id();
	result.vertex_array = p_vertex_array.get_id();
	result.device = uint64_t(rd->get_instance_id());
	result.surface = p_surface;
	result.renderer = p_renderer;
	result.pass = p_pass;
	result.viewport = current_viewport;
	std::lock_guard<std::mutex> lock(draw_observer_mutex);
	if (draw_observer_epoch != result.epoch) {
		draw_observer_epoch = result.epoch;
		draw_observer_events.clear();
	}
	if (draw_observer_events.size() >= 256) {
		RenderCaptureTestHooks::write_census_poison();
		return false;
	}
	draw_observer_events.push_back(result);
	return true;
}
std::vector<SkinnedMeshCaptureTestDrawObservation> SkinnedMeshCaptureTestNativeSource::draw_observations() {
	std::lock_guard<std::mutex> lock(draw_observer_mutex);
	return draw_observer_events;
}

Error SkinnedMeshCaptureTestNativeSource::capture_current(RID p_scene, RID p_mesh, RID p_skeleton, SkinnedMeshCaptureTestRDPool &p_pool, uint32_t p_slot, SkinnedMeshCaptureTestNativeSourceObservation &r_observation) {
	return capture(p_scene, p_mesh, p_skeleton, p_pool, p_slot, r_observation, nullptr, false);
}
Error SkinnedMeshCaptureTestNativeSource::capture_selected(RID p_scene, RID p_mesh, RID p_skeleton, uint64_t p_draw_sequence, bool p_previous, SkinnedMeshCaptureTestRDPool &p_pool, uint32_t p_slot, SkinnedMeshCaptureTestNativeSourceObservation &r_observation) {
	SkinnedMeshCaptureTestDrawObservation selected;
	bool found = false;
	{
		std::lock_guard<std::mutex> lock(draw_observer_mutex);
		for (const auto &event : draw_observer_events) {
			if (event.sequence == p_draw_sequence) {
				selected = event;
				found = true;
				break;
			}
		}
	}
	if (!found || !selected.viewport || (p_previous && !selected.previous_bound)) {
		return ERR_INVALID_DATA;
	}
	return capture(p_scene, p_mesh, p_skeleton, p_pool, p_slot, r_observation, &selected, p_previous);
}

Error SkinnedMeshCaptureTestNativeSource::select_matching_draw(const std::vector<SkinnedMeshCaptureTestDrawObservation> &p_draws, uint64_t p_mesh_instance, uint64_t p_epoch, uint64_t p_frame, uint64_t p_viewport, uint64_t p_sequence, uint64_t &r_sequence) {
	if (!p_mesh_instance || !p_epoch || !p_frame || !p_viewport) {
		return ERR_INVALID_DATA;
	}
	uint64_t selected = 0;
	for (const auto &draw : p_draws) {
		if (draw.mesh_instance != p_mesh_instance || draw.surface != 0 || draw.epoch != p_epoch || draw.frame != p_frame || draw.viewport != p_viewport || (p_sequence && draw.sequence != p_sequence)) {
			continue;
		}
		if (selected || !draw.sequence) {
			return ERR_INVALID_DATA;
		}
		selected = draw.sequence;
	}
	if (!selected) {
		return ERR_INVALID_DATA;
	}
	r_sequence = selected;
	return OK;
}
Error SkinnedMeshCaptureTestNativeSource::select_draw(RID p_scene, RID p_mesh, RID p_skeleton, uint64_t p_epoch, uint64_t p_frame, RID p_viewport, uint64_t p_sequence, uint64_t &r_sequence) {
	auto *server = RenderingServer::get_singleton();
	auto *scene = RenderCaptureTestAccess::scene_singleton();
	if (!server || !server->is_on_render_thread() || !scene) {
		return ERR_UNAVAILABLE;
	}
	const auto *instance = RenderCaptureTestAccess::instance(scene, p_scene);
	if (!instance || instance->base_type != RSE::INSTANCE_MESH || instance->base != p_mesh || instance->skeleton != p_skeleton || instance->mesh_instance.is_null()) {
		return ERR_INVALID_DATA;
	}
	return select_matching_draw(draw_observations(), instance->mesh_instance.get_id(), p_epoch, p_frame, p_viewport.get_id(), p_sequence, r_sequence);
}
