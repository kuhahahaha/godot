#include "p01_native_source.h"
#include "servers/rendering/renderer_scene_cull.h"
#include "servers/rendering/renderer_rd/storage_rd/mesh_storage.h"
#include "servers/rendering/rendering_server.h"

Error P01NativeSource::capture_current(RID p_scene, RID p_mesh, RID p_skeleton, P01RDPool &p_pool, uint32_t p_slot, P01NativeSourceObservation &r_observation) {
	RenderingDevice *rd = RenderingDevice::get_singleton();
	RenderingServer *server = RenderingServer::get_singleton();
	RendererSceneCull *scene = RendererSceneCull::singleton;
	RendererRD::MeshStorage *storage = RendererRD::MeshStorage::get_singleton();
	if (!rd || !server || !server->is_on_render_thread() || !scene || !storage) { return ERR_UNAVAILABLE; }
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
	// No dispatch call means certainly uninitialized. A nonzero count alone
	// cannot qualify initialization: the caller still needs actual t0 raw proof.
	if (target.p01_dispatch_count[target.current_buffer] == 0 || target.p01_dispatch_count[target.current_buffer] == UINT64_MAX) { return ERR_UNAVAILABLE; }
	const RID buffer_rid = target.vertex_buffer[target.current_buffer];
	const auto *buffer = rd->_get_buffer_from_owner(buffer_rid);
	if (!buffer || buffer->size < 36 || !buffer->p01_allocation_serial || rd->p01_physical_registry.resolve(buffer->driver_id.id, buffer_rid.get_id()) != buffer->p01_allocation_serial) { return ERR_INVALID_DATA; }
	P01NativeSourceObservation result;
	result.scene = p_scene.get_id();
	result.mesh = p_mesh.get_id();
	result.mesh_instance = instance->mesh_instance.get_id();
	result.skeleton = p_skeleton.get_id();
	result.device = uint64_t(rd->get_instance_id());
	result.allocation = buffer->p01_allocation_serial;
	result.buffer = buffer_rid.get_id();
	result.dispatch_call_count = target.p01_dispatch_count[target.current_buffer];
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
