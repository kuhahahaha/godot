#include "p01_native_source.h"
#include "p01_phase_observer.h"
#include "p01_observer.h"
#include "p01_write_census.h"
#include "servers/rendering/rendering_server_globals.h"
#include <mutex>
#include "servers/rendering/renderer_scene_cull.h"
#include "servers/rendering/renderer_rd/storage_rd/mesh_storage.h"
#include "servers/rendering/rendering_server.h"

Error P01NativeSource::capture(RID p_scene, RID p_mesh, RID p_skeleton, P01RDPool &p_pool, uint32_t p_slot, P01NativeSourceObservation &r_observation, const P01DrawObservation *p_draw, bool p_previous) {
	RenderingDevice *rd = RenderingDevice::get_singleton();
	RenderingServer *server = RenderingServer::get_singleton();
	RendererSceneCull *scene = RendererSceneCull::singleton;
	RendererRD::MeshStorage *storage = RendererRD::MeshStorage::get_singleton();
	if (!rd || !server || !server->is_on_render_thread() || !scene || !storage) { return ERR_UNAVAILABLE; }
	uint64_t capture_frame = 0;
	bool after_views = false;
	if (!p01_phase_capture_window(capture_frame, after_views)) { return ERR_UNAVAILABLE; }
	if (rd->draw_list.active || rd->compute_list.active || rd->raytracing_list.active) { return ERR_BUSY; }
	if (rd->p01_physical_coverage_failed) { return ERR_INVALID_DATA; }
	const auto *instance = scene->instance_owner.get_or_null(p_scene);
	if (!instance || instance->base_type != RSE::INSTANCE_MESH || instance->base != p_mesh || instance->skeleton != p_skeleton || instance->mesh_instance.is_null()) { return ERR_INVALID_DATA; }
	const auto *mesh = storage->mesh_owner.get_or_null(p_mesh);
	const auto *mi = storage->mesh_instance_owner.get_or_null(instance->mesh_instance);
	const auto *skeleton = storage->skeleton_owner.get_or_null(p_skeleton);
	if (!mesh || !mi || !skeleton || mi->mesh != mesh || mi->skeleton != p_skeleton || mesh->surface_count != 1 || mi->surfaces.size() != 1 || mesh->blend_shape_count != 0 || skeleton->size != 2 || skeleton->use_2d) { return ERR_INVALID_DATA; }
	const auto *surface = mesh->surfaces[0];
	constexpr uint64_t attributes = (1ULL << RSE::ARRAY_MAX) - 1;
	constexpr uint64_t required = RSE::ARRAY_FORMAT_VERTEX | RSE::ARRAY_FORMAT_BONES | RSE::ARRAY_FORMAT_WEIGHTS;
	constexpr uint64_t forbidden = RSE::ARRAY_FLAG_USE_2D_VERTICES | RSE::ARRAY_FLAG_USE_8_BONE_WEIGHTS | RSE::ARRAY_FLAG_COMPRESS_ATTRIBUTES | RSE::ARRAY_FLAG_USES_EMPTY_VERTEX_ARRAY;
	if (!surface || surface->primitive != RSE::PRIMITIVE_TRIANGLES || surface->vertex_count != 3 || surface->vertex_buffer_size != 36 || surface->skin_buffer_size != 48 || surface->index_count != 0 || (surface->format & attributes) != required || (surface->format & forbidden)) { return ERR_INVALID_DATA; }
	const auto &target = mi->surfaces[0];
	if (target.current_buffer > 1 || target.previous_buffer > 1) { return ERR_INVALID_DATA; }
	uint32_t selected_index = target.current_buffer;
	if (p_draw) {
		if (!after_views || p_draw->frame != capture_frame || p_draw->mesh_instance != instance->mesh_instance.get_id() || p_draw->surface != 0 || p_draw->device != uint64_t(rd->get_instance_id())) { return ERR_INVALID_DATA; }
		selected_index = p_draw->indices[p_previous ? 1 : 0];
		if (selected_index > 1 || target.vertex_buffer[selected_index].get_id() != p_draw->buffers[p_previous ? 1 : 0]) { return ERR_INVALID_DATA; }
	}
	// No dispatch call means certainly uninitialized. A nonzero count alone
	// cannot qualify initialization: the caller still needs actual t0 raw proof.
	if (target.p01_dispatch_count[selected_index] == 0 || target.p01_dispatch_count[selected_index] == UINT64_MAX) { return ERR_UNAVAILABLE; }
	const RID buffer_rid = target.vertex_buffer[selected_index];
	const auto *buffer = rd->_get_buffer_from_owner(buffer_rid);
	if (!buffer || buffer->size < 36 || !buffer->p01_allocation_serial || rd->p01_physical_registry.resolve(buffer->driver_id.id, buffer_rid.get_id()) != buffer->p01_allocation_serial) { return ERR_INVALID_DATA; }
	if (rd->p01_physical_registry.access_escaped(buffer->driver_id.id, buffer->p01_allocation_serial)) { return ERR_UNAVAILABLE; }
	if (p_draw && (buffer->p01_allocation_serial != p_draw->allocations[p_previous ? 1 : 0] || !p01_hook_buffer_unchanged(p_draw->device, buffer->p01_allocation_serial, p_draw->sequence, p_draw->epoch))) { return ERR_INVALID_DATA; }
	P01NativeSourceObservation result;
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
	result.allocation = buffer->p01_allocation_serial;
	result.buffer = buffer_rid.get_id();
	result.dispatch_call_count = target.p01_dispatch_count[selected_index];
	result.skeleton_version = skeleton->version;
	result.mesh_skeleton_version = mi->skeleton_version;
	result.last_change = target.last_change;
	result.current = target.current_buffer;
	result.previous = target.previous_buffer;
	result.queued = mi->array_update_list.in_list();
	const Error error = p_pool.record_native(p_slot, buffer_rid, 0);
	if (error != OK) { return error; }
	r_observation = result;
	return OK;
}

static thread_local uint64_t current_viewport = 0;
P01ViewportDrawScope::P01ViewportDrawScope(uint64_t p_viewport) : previous(current_viewport) { current_viewport = p_viewport; }
P01ViewportDrawScope::~P01ViewportDrawScope() { current_viewport = previous; }
static std::mutex draw_observer_mutex;
static std::vector<P01DrawObservation> draw_observer_events;
static uint64_t draw_observer_epoch = 0;
uint64_t P01NativeSource::draw_record_count() {
	RenderingDevice *rd = RenderingDevice::get_singleton();
	RenderingServer *server = RenderingServer::get_singleton();
	if (!rd || !server || !server->is_on_render_thread() || !rd->draw_list.active) { return 0; }
	return rd->draw_list.state.draw_count;
}
bool P01NativeSource::observe_draw(RID p_mesh_instance, uint32_t p_surface, RID p_vertex_array, uint64_t p_before_count, uint32_t p_renderer, uint32_t p_pass) {
	if (!p01_write_census_active()) { return false; }
	RenderingDevice *rd = RenderingDevice::get_singleton();
	RenderingServer *server = RenderingServer::get_singleton();
	auto *storage = RendererRD::MeshStorage::get_singleton();
	if (!rd || !server || !server->is_on_render_thread() || !storage || !rd->draw_list.active || rd->draw_list.state.draw_count != p_before_count + 1 || p_before_count == UINT64_MAX) { return false; }
	const auto *mi = storage->mesh_instance_owner.get_or_null(p_mesh_instance);
	if (!mi || p_surface >= mi->surfaces.size()) { return false; }
	const auto &surface = mi->surfaces[p_surface];
	P01DrawObservation result;
	bool found = false;
	for (uint32_t i = 0; i < surface.version_count; i++) {
		const auto &version = surface.versions[i];
		if (version.vertex_array != p_vertex_array) { continue; }
		result.indices[0] = version.current_buffer; result.indices[1] = version.previous_buffer;
		result.previous_bound = version.input_motion_vectors && (version.input_mask & RSE::ARRAY_FORMAT_VERTEX);
		found = true; break;
	}
	if (!found || result.indices[0] > 1 || result.indices[1] > 1 || rd->p01_physical_coverage_failed) { p01_write_census_poison(); return false; }
	for (uint32_t role = 0; role < 2; role++) {
		const RID rid = surface.vertex_buffer[result.indices[role]];
		const auto *buffer = rd->_get_buffer_from_owner(rid);
		if (!buffer || !buffer->p01_allocation_serial || rd->p01_physical_registry.resolve(buffer->driver_id.id, rid.get_id()) != buffer->p01_allocation_serial || rd->p01_physical_registry.access_escaped(buffer->driver_id.id, buffer->p01_allocation_serial)) { p01_write_census_poison(); return false; }
		result.buffers[role] = rid.get_id(); result.allocations[role] = buffer->p01_allocation_serial;
	}
	// Original operation marker shares the hazard publication sequence.
	const auto marker = p01_observe_draw_record(uint64_t(rd->get_instance_id()), p_mesh_instance.get_id(), p_vertex_array.get_id());
	if (!marker.sequence || p01_hook_is_poisoned()) { return false; }
	result.epoch = marker.epoch; result.sequence = marker.sequence;
	result.frame = RSG::rasterizer->get_frame_number(); result.mesh_instance = p_mesh_instance.get_id();
	result.vertex_array = p_vertex_array.get_id(); result.device = uint64_t(rd->get_instance_id());
	result.surface = p_surface; result.renderer = p_renderer; result.pass = p_pass;
	result.viewport = current_viewport;
	std::lock_guard<std::mutex> lock(draw_observer_mutex);
	if (draw_observer_epoch != result.epoch) { draw_observer_epoch = result.epoch; draw_observer_events.clear(); }
	if (draw_observer_events.size() >= 256) { p01_write_census_poison(); return false; }
	draw_observer_events.push_back(result);
	return true;
}
std::vector<P01DrawObservation> P01NativeSource::draw_observations() {
	std::lock_guard<std::mutex> lock(draw_observer_mutex);
	return draw_observer_events;
}

Error P01NativeSource::capture_current(RID p_scene, RID p_mesh, RID p_skeleton, P01RDPool &p_pool, uint32_t p_slot, P01NativeSourceObservation &r_observation) {
	return capture(p_scene, p_mesh, p_skeleton, p_pool, p_slot, r_observation, nullptr, false);
}
Error P01NativeSource::capture_selected(RID p_scene, RID p_mesh, RID p_skeleton, uint64_t p_draw_sequence, bool p_previous, P01RDPool &p_pool, uint32_t p_slot, P01NativeSourceObservation &r_observation) {
	P01DrawObservation selected;
	bool found = false;
	{
		std::lock_guard<std::mutex> lock(draw_observer_mutex);
		for (const auto &event : draw_observer_events) {
			if (event.sequence == p_draw_sequence) { selected = event; found = true; break; }
		}
	}
	if (!found || !selected.viewport || (p_previous && !selected.previous_bound)) { return ERR_INVALID_DATA; }
	return capture(p_scene, p_mesh, p_skeleton, p_pool, p_slot, r_observation, &selected, p_previous);
}

Error P01NativeSource::select_matching_draw(const std::vector<P01DrawObservation> &p_draws, uint64_t p_mesh_instance, uint64_t p_epoch, uint64_t p_frame, uint64_t p_viewport, uint64_t p_sequence, uint64_t &r_sequence) {
 if (!p_mesh_instance || !p_epoch || !p_frame || !p_viewport) { return ERR_INVALID_DATA; }
 uint64_t selected = 0;
 for (const auto &draw : p_draws) {
  if (draw.mesh_instance != p_mesh_instance || draw.surface != 0 || draw.epoch != p_epoch || draw.frame != p_frame || draw.viewport != p_viewport || (p_sequence && draw.sequence != p_sequence)) { continue; }
  if (selected || !draw.sequence) { return ERR_INVALID_DATA; }
  selected = draw.sequence;
 }
 if (!selected) { return ERR_INVALID_DATA; }
 r_sequence = selected;
 return OK;
}
Error P01NativeSource::select_draw(RID p_scene, RID p_mesh, RID p_skeleton, uint64_t p_epoch, uint64_t p_frame, RID p_viewport, uint64_t p_sequence, uint64_t &r_sequence) {
 auto *server = RenderingServer::get_singleton();
 auto *scene = RendererSceneCull::singleton;
 if (!server || !server->is_on_render_thread() || !scene) { return ERR_UNAVAILABLE; }
 const auto *instance = scene->instance_owner.get_or_null(p_scene);
 if (!instance || instance->base_type != RSE::INSTANCE_MESH || instance->base != p_mesh || instance->skeleton != p_skeleton || instance->mesh_instance.is_null()) { return ERR_INVALID_DATA; }
 return select_matching_draw(draw_observations(), instance->mesh_instance.get_id(), p_epoch, p_frame, p_viewport.get_id(), p_sequence, r_sequence);
}
