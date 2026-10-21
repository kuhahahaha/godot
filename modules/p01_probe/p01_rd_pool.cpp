#include "p01_rd_pool.h"

#include "p01_gpu_payload.h"
#include "p01_kernels.h"
#include "servers/rendering/rendering_server.h"

bool P01RDPool::on_owner_thread() const {
	RenderingServer *server = RenderingServer::get_singleton();
	RenderingDevice *current = RenderingDevice::get_singleton();
	return rd && current == rd && current->get_instance_id() == device_identity && server && server->is_on_render_thread();
}

bool P01RDPool::has_active_list() const {
	return rd->draw_list.active || rd->compute_list.active || rd->raytracing_list.active;
}

RID P01RDPool::create_set(RID p_shader, RID p_source, RID p_destination) {
	Vector<RenderingDevice::Uniform> uniforms;
	for (int i = 0; i < 2; i++) {
		RenderingDevice::Uniform uniform;
		uniform.uniform_type = RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER;
		uniform.binding = i;
		uniform.append_id(i == 0 ? p_source : p_destination);
		uniforms.push_back(uniform);
	}
	return rd->uniform_set_create(uniforms, p_shader, 0);
}

Error P01RDPool::initialize(bool p_dual_view) {
	if (rd || release_recorded) {
		return ERR_ALREADY_IN_USE;
	}
	rd = RenderingDevice::get_singleton();
	device_identity = rd ? rd->get_instance_id() : ObjectID();
	if (!on_owner_thread()) {
		rd = nullptr;
		return ERR_UNAVAILABLE;
	}
	if (has_active_list()) {
		rd = nullptr;
		return ERR_BUSY;
	}
	capacity = p_dual_view ? 18 : 8;
	const char *sources[] = { p01_public_deformation_shader_source(), p01_transport_shader_source() };
	for (int i = 0; i < 2; i++) {
		String error;
		RenderingDevice::ShaderStageSPIRVData stage;
		stage.shader_stage = RenderingDevice::SHADER_STAGE_COMPUTE;
		stage.spirv = rd->shader_compile_spirv_from_source(stage.shader_stage, sources[i], RenderingDevice::SHADER_LANGUAGE_GLSL, &error);
		if (stage.spirv.is_empty() || !error.is_empty()) {
			release();
			return ERR_COMPILATION_FAILED;
		}
		Vector<RenderingDevice::ShaderStageSPIRVData> stages;
		stages.push_back(stage);
		shaders[i] = rd->shader_create_from_spirv(stages, i == 0 ? "P01Public" : "P01Transport");
		if (shaders[i].is_valid()) {
			pipelines[i] = rd->compute_pipeline_create(shaders[i]);
		}
		if (pipelines[i].is_null()) {
			release();
			return ERR_CANT_CREATE;
		}
	}
	for (uint32_t i = 0; i < capacity; i++) {
		Slot &slot = slots[i];
		const PackedByteArray initial = p01_initial_guarded_buffer();
		slot.snapshot = rd->storage_buffer_create(64, initial);
		slot.consumer = rd->storage_buffer_create(64, initial);
		if (slot.snapshot.is_valid() && slot.consumer.is_valid()) {
			slot.transport_set = create_set(shaders[1], slot.snapshot, slot.consumer);
		}
		if (slot.transport_set.is_null()) {
			release();
			return ERR_CANT_CREATE;
		}
	}
	return OK;
}

Error P01RDPool::record_transport(uint32_t p_slot) {
	if (!rd) { return ERR_UNCONFIGURED; }
	if (!on_owner_thread()) { return ERR_UNAVAILABLE; }
	if (has_active_list()) { return ERR_BUSY; }
	if (p_slot >= capacity || !slots[p_slot].captured || slots[p_slot].transported) { return ERR_INVALID_PARAMETER; }
	for (RID rid : {slots[p_slot].snapshot, slots[p_slot].consumer}) {
		const Error error = qualify_buffer(rid);
		if (error != OK) { return error; }
	}
	slots[p_slot].transported = true;
	const auto list = rd->compute_list_begin();
	if (list == RenderingDevice::INVALID_ID) {
		return ERR_CANT_CREATE;
	}
	rd->compute_list_bind_compute_pipeline(list, pipelines[1]);
	rd->compute_list_bind_uniform_set(list, slots[p_slot].transport_set, 0);
	rd->compute_list_dispatch(list, 1, 1, 1);
	rd->compute_list_end();
	return OK;
}

Error P01RDPool::record_public(uint32_t p_slot, const P01PublicSnapshot &p_snapshot) {
	if (!rd) {
		return ERR_UNCONFIGURED;
	}
	if (!on_owner_thread()) {
		return ERR_UNAVAILABLE;
	}
	if (has_active_list()) { return ERR_BUSY; }
	if (p_slot >= capacity || slots[p_slot].consumed) {
		return ERR_INVALID_PARAMETER;
	}
	const Error qualification = qualify_buffer(slots[p_slot].snapshot);
	if (qualification != OK) { return qualification; }
	PackedByteArray payload;
	Error error = p01_encode_gpu_payload(p_snapshot, payload);
	if (error != OK) {
		return error;
	}
	Slot &slot = slots[p_slot];
	// Even a partial allocation/record failure consumes the slot. Never retry
	// against a destination whose write prefix may already be in the graph.
	slot.consumed = true;
	slot.upload = rd->storage_buffer_create(228, payload);
	if (slot.upload.is_null()) {
		return ERR_CANT_CREATE;
	}
	slot.deformation_set = create_set(shaders[0], slot.upload, slot.snapshot);
	if (slot.deformation_set.is_null()) {
		return ERR_CANT_CREATE;
	}
	const auto list = rd->compute_list_begin();
	if (list == RenderingDevice::INVALID_ID) {
		return ERR_CANT_CREATE;
	}
	rd->compute_list_bind_compute_pipeline(list, pipelines[0]);
	rd->compute_list_bind_uniform_set(list, slot.deformation_set, 0);
	rd->compute_list_dispatch(list, 1, 1, 1);
	rd->compute_list_end();
	slots[p_slot].captured = true;
	return OK;
}

Error P01RDPool::record_native(uint32_t p_slot, RID p_source, uint32_t p_offset) {
	if (!rd) {
		return ERR_UNCONFIGURED;
	}
	if (!on_owner_thread()) {
		return ERR_UNAVAILABLE;
	}
	if (has_active_list()) { return ERR_BUSY; }
	if (p_slot >= capacity || slots[p_slot].consumed || p_source.is_null() || p_offset > UINT32_MAX - 36) {
		return ERR_INVALID_PARAMETER;
	}
	for (RID rid : {p_source, slots[p_slot].snapshot}) {
		const Error qualification = qualify_buffer(rid);
		if (qualification != OK) { return qualification; }
	}
	slots[p_slot].consumed = true;
	const Error error = rd->buffer_copy(p_source, slots[p_slot].snapshot, p_offset, 0, 36);
	if (error != OK) {
		return error;
	}
	slots[p_slot].captured = true;
	return OK;
}

Error P01RDPool::release() {
	if (!rd) {
		return OK;
	}
	if (!on_owner_thread()) {
		return ERR_UNAVAILABLE;
	}
	for (const Slot &slot : slots) {
		if ((slot.readback_attempted[0] || slot.readback_attempted[1]) && (!queued_collector || !queued_collector->drained())) {
			return ERR_BUSY;
		}
	}
	retirement_device = rd->get_instance_id();
	release_recorded = true;
	for (Slot &slot : slots) {
		for (RID rid : { slot.upload, slot.snapshot, slot.consumer }) {
			if (rid.is_null()) { continue; }
			RenderingDevice::Buffer *buffer = rd->_get_buffer_from_owner(rid);
			if (!buffer || !buffer->p01_allocation_serial || retirement_count >= 54 || rd->p01_physical_registry.resolve(buffer->driver_id.id, rid.get_id()) != buffer->p01_allocation_serial) {
				retirement_invalid = true;
			} else {
				retirements[retirement_count++] = { buffer->driver_id.id, buffer->p01_allocation_serial };
			}
		}
		for (RID rid : { slot.deformation_set, slot.transport_set, slot.upload, slot.snapshot, slot.consumer }) {
			if (rid.is_valid()) {
				rd->free_rid(rid);
			}
		}
		slot = Slot();
	}
	for (int i = 0; i < 2; i++) {
		if (pipelines[i].is_valid()) {
			rd->free_rid(pipelines[i]);
		}
		if (shaders[i].is_valid()) {
			rd->free_rid(shaders[i]);
		}
		pipelines[i] = RID();
		shaders[i] = RID();
	}
	rd = nullptr;
	capacity = 0;
	queued_collector.reset();
	return OK;
}

Error P01RDPool::enqueue_readback(const P01ReadbackIdentity &p_identity, P01ReadbackCollector &p_collector) {
	if (!rd) { return ERR_UNCONFIGURED; }
	if (!on_owner_thread()) { return ERR_UNAVAILABLE; }
	if (has_active_list()) { return ERR_BUSY; }
	if (p_identity.slot >= capacity) { return ERR_INVALID_PARAMETER; }
	Slot &slot = slots[p_identity.slot];
	const uint32_t role = uint32_t(p_identity.consumer);
	const RID buffer = p_identity.consumer ? slot.consumer : slot.snapshot;
	if (!slot.transported || slot.readback_attempted[role] || buffer.get_id() != p_identity.rid) { return ERR_INVALID_PARAMETER; }
	// Godot ObjectID identifies this actual RD object lifetime, including its
	// generation bits. A pointer address is not a device lifetime identity.
	RenderingDevice::Buffer *actual = rd->_get_buffer_from_owner(buffer);
	if (rd->p01_physical_coverage_failed || !actual || p_identity.device != uint64_t(rd->get_instance_id()) || p_identity.allocation != actual->p01_allocation_serial || rd->p01_physical_registry.resolve(actual->driver_id.id, buffer.get_id()) != p_identity.allocation) {
		return ERR_INVALID_DATA;
	}
	const Error qualification = qualify_buffer(buffer);
	if (qualification != OK) { return qualification; }
	if (queued_collector && !queued_collector->shares_state(p_collector)) { return ERR_INVALID_PARAMETER; }
	Callable callback;
	Error error = p_collector.reserve(p_identity, callback);
	if (error != OK) { return error; }
	if (!queued_collector) { queued_collector = std::make_unique<P01ReadbackCollector>(p_collector); }
	slot.readback_attempted[role] = true;
	// _stall_for_frame invokes this engine-owned callback only after its
	// frame fence wait and staging-byte copy. Never submit/sync the main RD.
	error = rd->buffer_get_data_async(buffer, callback, 0, 64);
	if (error != OK) {
		// RD stores the callback only after successful staging preparation.
		// Any recorded staging prefix still retires through normal RD frames;
		// cancellation is accounting, not physical retirement or reuse.
		p_collector.cancel_unqueued(p_identity);
		return error;
	}
	return p_collector.mark_enqueued(p_identity);
}

Error P01RDPool::physical_retirement_status() const {
	if (!release_recorded) { return ERR_UNCONFIGURED; }
	if (retirement_invalid) { return ERR_INVALID_DATA; }
	RenderingDevice *device = RenderingDevice::get_singleton();
	RenderingServer *server = RenderingServer::get_singleton();
	if (!device || device->get_instance_id() != retirement_device || !server || !server->is_on_render_thread()) { return ERR_UNAVAILABLE; }
	if (device->p01_physical_coverage_failed) { return ERR_INVALID_DATA; }
	for (uint32_t i = 0; i < retirement_count; i++) {
		if (device->p01_physical_registry.physically_live(retirements[i].driver, retirements[i].serial)) { return ERR_BUSY; }
	}
	return OK;
}

Error P01RDPool::qualify_buffer(RID p_buffer) const {
	if (!rd || rd->p01_physical_coverage_failed) { return ERR_INVALID_DATA; }
	const auto *buffer = rd->_get_buffer_from_owner(p_buffer);
	if (!buffer || !buffer->p01_allocation_serial || rd->p01_physical_registry.resolve(buffer->driver_id.id, p_buffer.get_id()) != buffer->p01_allocation_serial) { return ERR_INVALID_DATA; }
	if (rd->p01_physical_registry.access_escaped(buffer->driver_id.id, buffer->p01_allocation_serial)) { return ERR_UNAVAILABLE; }
	return OK;
}

Error P01RDPool::validate_pair(const P01ReadbackIdentity &p_snapshot, const P01ReadbackIdentity &p_consumer, const P01ReadbackCollector &p_collector) const {
	if (!rd) { return ERR_UNCONFIGURED; }
	if (!on_owner_thread()) { return ERR_UNAVAILABLE; }
	if (p_snapshot.slot >= capacity || p_snapshot.slot != p_consumer.slot) { return ERR_INVALID_PARAMETER; }
	const Slot &slot = slots[p_snapshot.slot];
	if (slot.snapshot.get_id() != p_snapshot.rid || slot.consumer.get_id() != p_consumer.rid) { return ERR_INVALID_DATA; }
	for (RID rid : {slot.snapshot, slot.consumer}) {
		const Error error = qualify_buffer(rid);
		if (error != OK) { return error; }
	}
	return p_collector.validate_pair(p_snapshot, p_consumer);
}
