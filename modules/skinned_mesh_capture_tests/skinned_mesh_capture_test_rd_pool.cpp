/**************************************************************************/
/*  skinned_mesh_capture_test_rd_pool.cpp                                 */
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

#include "skinned_mesh_capture_test_rd_pool.h"

#include "skinned_mesh_capture_test_gpu_payload.h"
#include "skinned_mesh_capture_test_kernels.h"
#include "skinned_mesh_capture_test_observer.h"

#include "servers/rendering/renderer_rd/effects/copy_effects.h"
#include "servers/rendering/rendering_server.h"

#include "modules/skinned_mesh_capture_tests/render_capture_test_access.h"

bool SkinnedMeshCaptureTestRDPool::on_owner_thread() const {
	RenderingServer *server = RenderingServer::get_singleton();
	RenderingDevice *current = RenderingDevice::get_singleton();
	return rd && current == rd && current->get_instance_id() == device_identity && server && server->is_on_render_thread();
}

bool SkinnedMeshCaptureTestRDPool::has_active_list() const {
	return RenderCaptureTestAccess::draw_list(rd).active || RenderCaptureTestAccess::compute_list(rd).active || RenderCaptureTestAccess::raytracing_list(rd).active;
}

RID SkinnedMeshCaptureTestRDPool::create_set(RID p_shader, RID p_source, RID p_destination) {
	Vector<RenderingDevice::Uniform> uniforms;
	for (int i = 0; i < 2; i++) {
		RenderingDevice::Uniform uniform;
		uniform.uniform_type = RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER;
		uniform.binding = i;
		uniform.append_id(i == 0 ? p_source : p_destination);
		uniforms.push_back(uniform);
	}
	const RID rid = rd->uniform_set_create(uniforms, p_shader, 0);
	if (rid.is_valid() && !track_resource(rid, RenderCaptureTestResourceKind::UniformSet)) {
		rd->free_rid(rid);
		return RID();
	}
	return rid;
}

Error SkinnedMeshCaptureTestRDPool::initialize(bool p_dual_view) {
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
	const char *sources[] = { skinned_mesh_capture_test_public_deformation_shader_source(), skinned_mesh_capture_test_transport_shader_source() };
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
		shaders[i] = rd->shader_create_from_spirv(stages, i == 0 ? "SkinnedMeshCaptureTestPublicDeformation" : "SkinnedMeshCaptureTestTransport");
		if (shaders[i].is_valid() && !track_resource(shaders[i], RenderCaptureTestResourceKind::Shader)) {
			release();
			return ERR_INVALID_DATA;
		}
		if (shaders[i].is_valid()) {
			pipelines[i] = rd->compute_pipeline_create(shaders[i]);
		}
		if (pipelines[i].is_valid() && !track_resource(pipelines[i], RenderCaptureTestResourceKind::ComputePipeline)) {
			release();
			return ERR_INVALID_DATA;
		}
		if (pipelines[i].is_null()) {
			release();
			return ERR_CANT_CREATE;
		}
	}
	for (uint32_t i = 0; i < capacity; i++) {
		Slot &slot = slots[i];
		const PackedByteArray initial = skinned_mesh_capture_test_initial_guarded_buffer();
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

Error SkinnedMeshCaptureTestRDPool::record_transport(uint32_t p_slot) {
	if (!rd) {
		return ERR_UNCONFIGURED;
	}
	if (!on_owner_thread()) {
		return ERR_UNAVAILABLE;
	}
	if (has_active_list()) {
		return ERR_BUSY;
	}
	if (p_slot >= capacity || !slots[p_slot].captured || slots[p_slot].transported) {
		return ERR_INVALID_PARAMETER;
	}
	for (RID rid : { slots[p_slot].snapshot, slots[p_slot].consumer }) {
		const Error error = qualify_buffer(rid);
		if (error != OK) {
			return error;
		}
	}
	const auto *snapshot_buffer = RenderCaptureTestAccess::buffer(rd, slots[p_slot].snapshot);
	if (!skinned_mesh_capture_test_hook_buffer_unchanged(uint64_t(device_identity), snapshot_buffer->capture_test_allocation_serial, slots[p_slot].capture_sequence, slots[p_slot].observation_epoch)) {
		return ERR_INVALID_DATA;
	}
	slots[p_slot].transported = true;
	const auto list = rd->compute_list_begin();
	if (list == RenderingDevice::INVALID_ID) {
		return ERR_CANT_CREATE;
	}
	rd->compute_list_bind_compute_pipeline(list, pipelines[1]);
	rd->compute_list_bind_uniform_set(list, slots[p_slot].transport_set, 0);
	const uint64_t before_dispatch = RenderCaptureTestAccess::compute_list(rd).state.dispatch_count;
	SkinnedMeshCaptureTestRecordingScope operation;
	rd->compute_list_dispatch(list, 1, 1, 1);
	const bool dispatched = RenderCaptureTestAccess::compute_list(rd).state.dispatch_count == before_dispatch + 1;
	rd->compute_list_end();
	if (!dispatched) {
		return ERR_INVALID_DATA;
	}
	uint64_t epoch = 0;
	const Error recorded = record_boundary(slots[p_slot].consumer, operation.identity(), slots[p_slot].transport_sequence, epoch);
	if (recorded != OK || epoch != slots[p_slot].observation_epoch) {
		return ERR_INVALID_DATA;
	}
	return OK;
}

Error SkinnedMeshCaptureTestRDPool::record_public(uint32_t p_slot, const SkinnedMeshCaptureTestPublicSnapshot &p_snapshot) {
	if (!rd) {
		return ERR_UNCONFIGURED;
	}
	if (!on_owner_thread()) {
		return ERR_UNAVAILABLE;
	}
	if (has_active_list()) {
		return ERR_BUSY;
	}
	if (p_slot >= capacity || slots[p_slot].consumed) {
		return ERR_INVALID_PARAMETER;
	}
	const Error qualification = qualify_buffer(slots[p_slot].snapshot);
	if (qualification != OK) {
		return qualification;
	}
	PackedByteArray payload;
	Error error = skinned_mesh_capture_test_encode_gpu_payload(p_snapshot, payload);
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
	const uint64_t before_dispatch = RenderCaptureTestAccess::compute_list(rd).state.dispatch_count;
	SkinnedMeshCaptureTestRecordingScope operation;
	rd->compute_list_dispatch(list, 1, 1, 1);
	const bool dispatched = RenderCaptureTestAccess::compute_list(rd).state.dispatch_count == before_dispatch + 1;
	rd->compute_list_end();
	if (!dispatched) {
		return ERR_INVALID_DATA;
	}
	if (record_boundary(slots[p_slot].snapshot, operation.identity(), slots[p_slot].capture_sequence, slots[p_slot].observation_epoch) != OK) {
		return ERR_INVALID_DATA;
	}
	slots[p_slot].captured = true;
	return OK;
}

Error SkinnedMeshCaptureTestRDPool::record_native(uint32_t p_slot, RID p_source, uint32_t p_offset) {
	if (!rd) {
		return ERR_UNCONFIGURED;
	}
	if (!on_owner_thread()) {
		return ERR_UNAVAILABLE;
	}
	if (has_active_list()) {
		return ERR_BUSY;
	}
	if (p_slot >= capacity || slots[p_slot].consumed || p_source.is_null() || p_offset > UINT32_MAX - 36) {
		return ERR_INVALID_PARAMETER;
	}
	for (RID rid : { p_source, slots[p_slot].snapshot }) {
		const Error qualification = qualify_buffer(rid);
		if (qualification != OK) {
			return qualification;
		}
	}
	slots[p_slot].consumed = true;
	SkinnedMeshCaptureTestRecordingScope operation;
	const Error error = rd->buffer_copy(p_source, slots[p_slot].snapshot, p_offset, 0, 36);
	if (error != OK) {
		return error;
	}
	if (record_boundary(slots[p_slot].snapshot, operation.identity(), slots[p_slot].capture_sequence, slots[p_slot].observation_epoch) != OK) {
		return ERR_INVALID_DATA;
	}
	slots[p_slot].captured = true;
	return OK;
}

Error SkinnedMeshCaptureTestRDPool::release() {
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
			if (rid.is_null()) {
				continue;
			}
			const RenderCaptureTestAccess::Buffer *buffer = RenderCaptureTestAccess::buffer(rd, rid);
			if (!buffer || !buffer->capture_test_allocation_serial || retirement_count >= 54 || RenderCaptureTestAccess::capture_test_allocation_registry(rd).resolve(buffer->driver_id.id, rid.get_id()) != buffer->capture_test_allocation_serial) {
				retirement_invalid = true;
			} else {
				retirements[retirement_count++] = { buffer->driver_id.id, buffer->capture_test_allocation_serial };
			}
		}
		for (RID rid : { slot.deformation_set, slot.transport_set, slot.upload, slot.snapshot, slot.consumer, slot.image }) {
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

Error SkinnedMeshCaptureTestRDPool::enqueue_readback(const SkinnedMeshCaptureTestReadbackIdentity &p_identity, SkinnedMeshCaptureTestReadbackCollector &p_collector) {
	if (!rd) {
		return ERR_UNCONFIGURED;
	}
	if (!on_owner_thread()) {
		return ERR_UNAVAILABLE;
	}
	if (has_active_list()) {
		return ERR_BUSY;
	}
	if (p_identity.slot >= capacity) {
		return ERR_INVALID_PARAMETER;
	}
	Slot &slot = slots[p_identity.slot];
	const uint32_t role = uint32_t(p_identity.consumer);
	const RID buffer = p_identity.consumer ? slot.consumer : slot.snapshot;
	if (!slot.transported || slot.readback_attempted[role] || buffer.get_id() != p_identity.rid) {
		return ERR_INVALID_PARAMETER;
	}
	// Godot ObjectID identifies this actual RD object lifetime, including its
	// generation bits. A pointer address is not a device lifetime identity.
	const RenderCaptureTestAccess::Buffer *actual = RenderCaptureTestAccess::buffer(rd, buffer);
	if (RenderCaptureTestAccess::capture_test_coverage_failed(rd) || !actual || p_identity.device != uint64_t(rd->get_instance_id()) || p_identity.allocation != actual->capture_test_allocation_serial || RenderCaptureTestAccess::capture_test_allocation_registry(rd).resolve(actual->driver_id.id, buffer.get_id()) != p_identity.allocation) {
		return ERR_INVALID_DATA;
	}
	const Error qualification = qualify_buffer(buffer);
	if (qualification != OK) {
		return qualification;
	}
	if (queued_collector && !queued_collector->shares_state(p_collector)) {
		return ERR_INVALID_PARAMETER;
	}
	Callable callback;
	Error error = p_collector.reserve(p_identity, callback);
	if (error != OK) {
		return error;
	}
	if (!queued_collector) {
		queued_collector = std::make_unique<SkinnedMeshCaptureTestReadbackCollector>(p_collector);
	}
	slot.readback_attempted[role] = true;
	// _stall_for_frame invokes this engine-owned callback only after its
	// frame fence wait and staging-byte copy. Never submit/sync the main RD.
	error = rd->buffer_get_data_async(buffer, callback, 0, 64);
	if (error != OK) {
		// RD stores the callback only after successful staging preparation.
		// Any recorded staging prefix still retires through normal RD frames;
		// cancellation is accounting, not physical retirement or reuse.
		const Error canceled = p_collector.cancel_unqueued(p_identity);
		return canceled == OK ? error : canceled;
	}
	return p_collector.mark_enqueued(p_identity);
}

Error SkinnedMeshCaptureTestRDPool::physical_retirement_status() {
	if (retirement_confirmed) {
		return OK;
	}
	if (!release_recorded) {
		return ERR_UNCONFIGURED;
	}
	if (retirement_invalid) {
		return ERR_INVALID_DATA;
	}
	RenderingDevice *device = RenderingDevice::get_singleton();
	RenderingServer *server = RenderingServer::get_singleton();
	if (!device || device->get_instance_id() != retirement_device || !server || !server->is_on_render_thread()) {
		return ERR_UNAVAILABLE;
	}
	if (RenderCaptureTestAccess::capture_test_coverage_failed(device)) {
		return ERR_INVALID_DATA;
	}
	for (uint32_t i = 0; i < retirement_count; i++) {
		if (RenderCaptureTestAccess::capture_test_allocation_registry(device).physically_live(retirements[i].driver, retirements[i].serial)) {
			return ERR_BUSY;
		}
	}
	for (uint32_t i = 0; i < resource_count; i++) {
		const auto status = RenderCaptureTestAccess::capture_test_resource_registry(device).status(resource_tokens[i]);
		if (status == RenderCaptureTestResourceRegistry::Status::Unknown) {
			return ERR_INVALID_DATA;
		}
		if (status == RenderCaptureTestResourceRegistry::Status::Live) {
			return ERR_BUSY;
		}
	}
	for (uint32_t i = 0; i < resource_count; i++) {
		if (!RenderCaptureTestAccess::capture_test_resource_registry(device).acknowledge(resource_tokens[i])) {
			return ERR_INVALID_DATA;
		}
	}
	retirement_confirmed = true;
	return OK;
}

Error SkinnedMeshCaptureTestRDPool::qualify_buffer(RID p_buffer) const {
	if (!rd || RenderCaptureTestAccess::capture_test_coverage_failed(rd)) {
		return ERR_INVALID_DATA;
	}
	const auto *buffer = RenderCaptureTestAccess::buffer(rd, p_buffer);
	if (!buffer || !buffer->capture_test_allocation_serial || RenderCaptureTestAccess::capture_test_allocation_registry(rd).resolve(buffer->driver_id.id, p_buffer.get_id()) != buffer->capture_test_allocation_serial) {
		return ERR_INVALID_DATA;
	}
	if (RenderCaptureTestAccess::capture_test_allocation_registry(rd).access_escaped(buffer->driver_id.id, buffer->capture_test_allocation_serial)) {
		return ERR_UNAVAILABLE;
	}
	return OK;
}

Error SkinnedMeshCaptureTestRDPool::qualify_collector(uint32_t p_slot, const SkinnedMeshCaptureTestReadbackCollector &p_collector) const {
	if (p_slot >= capacity) {
		return ERR_INVALID_PARAMETER;
	}
	const Slot &slot = slots[p_slot];
	if (!queued_collector || !queued_collector->shares_state(p_collector) || !slot.readback_attempted[0] || !slot.readback_attempted[1]) {
		return ERR_INVALID_DATA;
	}
	return OK;
}

Error SkinnedMeshCaptureTestRDPool::validate_pair(const SkinnedMeshCaptureTestReadbackIdentity &p_snapshot, const SkinnedMeshCaptureTestReadbackIdentity &p_consumer, const SkinnedMeshCaptureTestReadbackCollector &p_collector) const {
	if (!rd) {
		return ERR_UNCONFIGURED;
	}
	if (!on_owner_thread()) {
		return ERR_UNAVAILABLE;
	}
	if (p_snapshot.slot >= capacity || p_snapshot.slot != p_consumer.slot) {
		return ERR_INVALID_PARAMETER;
	}
	if (p_snapshot.consumer || !p_consumer.consumer) {
		return ERR_INVALID_DATA;
	}
	const Slot &slot = slots[p_snapshot.slot];
	if (qualify_collector(p_snapshot.slot, p_collector) != OK) {
		return ERR_INVALID_DATA;
	}
	for (const auto &identity : { p_snapshot, p_consumer }) {
		const Error error = qualify_readback(identity);
		if (error != OK) {
			return error;
		}
	}
	if (!skinned_mesh_capture_test_hook_buffer_unchanged(p_snapshot.device, p_snapshot.allocation, slot.capture_sequence, slot.observation_epoch) || !skinned_mesh_capture_test_hook_buffer_unchanged(p_consumer.device, p_consumer.allocation, slot.transport_sequence, slot.observation_epoch)) {
		return ERR_INVALID_DATA;
	}
	return p_collector.validate_pair(p_snapshot, p_consumer);
}

Error SkinnedMeshCaptureTestRDPool::qualify_readback(const SkinnedMeshCaptureTestReadbackIdentity &p_identity) const {
	if (!rd || p_identity.slot >= capacity || p_identity.device != uint64_t(rd->get_instance_id())) {
		return ERR_INVALID_DATA;
	}
	const Slot &slot = slots[p_identity.slot];
	const RID rid = p_identity.consumer ? slot.consumer : slot.snapshot;
	if (rid.is_null() || rid.get_id() != p_identity.rid) {
		return ERR_INVALID_DATA;
	}
	const Error error = qualify_buffer(rid);
	if (error != OK) {
		return error;
	}
	const auto *actual = RenderCaptureTestAccess::buffer(rd, rid);
	return actual->capture_test_allocation_serial == p_identity.allocation ? OK : ERR_INVALID_DATA;
}

Error SkinnedMeshCaptureTestRDPool::record_boundary(RID p_buffer, uint64_t p_operation, uint64_t &r_sequence, uint64_t &r_epoch) const {
	if (!p_operation || qualify_buffer(p_buffer) != OK || skinned_mesh_capture_test_hook_is_poisoned()) {
		return ERR_INVALID_DATA;
	}
	const auto *buffer = RenderCaptureTestAccess::buffer(rd, p_buffer);
	const auto events = skinned_mesh_capture_test_hook_events();
	for (auto it = events.rbegin(); it != events.rend(); ++it) {
		if (it->recording_token != p_operation) {
			continue;
		}
		if ((it->kind == 7 || it->kind == 8) && it->success && it->device == uint64_t(rd->get_instance_id()) && it->allocation_serial == buffer->capture_test_allocation_serial) {
			r_sequence = it->sequence;
			r_epoch = it->epoch;
			return OK;
		}
	}
	return ERR_INVALID_DATA;
}

Error SkinnedMeshCaptureTestRDPool::resolve_readback_identity(uint32_t p_slot, bool p_consumer, uint64_t p_run, uint64_t p_epoch, uint64_t p_request, SkinnedMeshCaptureTestReadbackIdentity &r_identity) const {
	if (!p_run || !p_epoch || !p_request || p_request > (UINT64_MAX - 1) / 2 || p_slot >= capacity) {
		return ERR_INVALID_PARAMETER;
	}
	const Slot &slot = slots[p_slot];
	if (!slot.transported || slot.observation_epoch != p_epoch) {
		return ERR_INVALID_DATA;
	}
	const RID rid = p_consumer ? slot.consumer : slot.snapshot;
	const Error qualification = qualify_buffer(rid);
	if (qualification != OK) {
		return qualification;
	}
	const auto *buffer = RenderCaptureTestAccess::buffer(rd, rid);
	SkinnedMeshCaptureTestReadbackIdentity result;
	result.run = p_run;
	result.epoch = p_epoch;
	result.token = p_request * 2 + uint64_t(p_consumer);
	// This is the original capture request sequence, not a native submission serial.
	result.queue = p_request;
	result.device = uint64_t(rd->get_instance_id());
	result.rid = result.owner = rid.get_id();
	result.alias = buffer->driver_id.id;
	result.allocation = buffer->capture_test_allocation_serial;
	result.slot = p_slot;
	result.consumer = p_consumer;
	r_identity = result;
	return OK;
}
Error SkinnedMeshCaptureTestRDPool::enqueue_pair(uint32_t p_slot, uint64_t p_run, uint64_t p_epoch, uint64_t p_request, SkinnedMeshCaptureTestReadbackCollector &p_collector, SkinnedMeshCaptureTestReadbackIdentity (&r_identities)[2], uint32_t &r_enqueued) {
	r_enqueued = 0;
	if (!rd) {
		return ERR_UNCONFIGURED;
	}
	if (!on_owner_thread()) {
		return ERR_UNAVAILABLE;
	}
	SkinnedMeshCaptureTestReadbackIdentity identities[2];
	for (uint32_t role = 0; role < 2; role++) {
		const Error error = resolve_readback_identity(p_slot, bool(role), p_run, p_epoch, p_request, identities[role]);
		if (error != OK) {
			return error;
		}
	}
	for (uint32_t role = 0; role < 2; role++) {
		r_identities[role] = identities[role];
		const Error error = enqueue_readback(identities[role], p_collector);
		if (error != OK) {
			return error;
		}
		++r_enqueued;
	}
	return OK;
}

bool SkinnedMeshCaptureTestRDPool::track_resource(RID p_rid, RenderCaptureTestResourceKind p_kind) {
	uint64_t driver = 0;
	switch (p_kind) {
		case RenderCaptureTestResourceKind::Texture: {
			const auto *value = RenderCaptureTestAccess::texture(rd, p_rid);
			if (value) {
				driver = value->driver_id.id;
			}
		} break;
		case RenderCaptureTestResourceKind::Shader: {
			const auto *value = RenderCaptureTestAccess::shader(rd, p_rid);
			if (value) {
				driver = value->driver_id.id;
			}
		} break;
		case RenderCaptureTestResourceKind::ComputePipeline: {
			const auto *value = RenderCaptureTestAccess::compute_pipeline(rd, p_rid);
			if (value) {
				driver = value->driver_id.id;
			}
		} break;
		case RenderCaptureTestResourceKind::UniformSet: {
			const auto *value = RenderCaptureTestAccess::uniform_set_owner(rd).get_or_null(p_rid);
			if (value) {
				driver = value->driver_id.id;
			}
		} break;
	}
	if (!driver || resource_count >= 58) {
		retirement_invalid = true;
		return false;
	}
	const uint64_t token = RenderCaptureTestAccess::capture_test_resource_registry(rd).track(p_kind, driver);
	if (!token) {
		retirement_invalid = true;
		return false;
	}
	resource_tokens[resource_count++] = token;
	return true;
}

Error SkinnedMeshCaptureTestRDPool::record_image(uint32_t p_slot, RID p_source, RID &r_image) {
	if (!rd) {
		return ERR_UNCONFIGURED;
	}
	if (!on_owner_thread()) {
		return ERR_UNAVAILABLE;
	}
	if (has_active_list()) {
		return ERR_BUSY;
	}
	if (p_slot >= capacity || p_source.is_null() || slots[p_slot].image.is_valid()) {
		return ERR_INVALID_PARAMETER;
	}
	auto *effects = RendererRD::CopyEffects::get_singleton();
	if (!effects) {
		return ERR_UNAVAILABLE;
	}
	const auto source_format = rd->texture_get_format(p_source);
	if (source_format.width != 128 || source_format.height != 64 || source_format.array_layers != 1 || source_format.depth != 1 || source_format.mipmaps != 1 || (source_format.format != RenderingDevice::DATA_FORMAT_R8G8B8A8_UNORM && source_format.format != RenderingDevice::DATA_FORMAT_R8G8B8A8_SRGB)) {
		return ERR_INVALID_DATA;
	}
	RenderingDevice::TextureFormat format;
	format.width = 128;
	format.height = 64;
	format.format = RenderingDevice::DATA_FORMAT_R8G8B8A8_UNORM;
	format.usage_bits = RenderingDevice::TEXTURE_USAGE_STORAGE_BIT | RenderingDevice::TEXTURE_USAGE_CAN_COPY_FROM_BIT;
	slots[p_slot].image = rd->texture_create(format, RenderingDevice::TextureView());
	if (slots[p_slot].image.is_null()) {
		return ERR_CANT_CREATE;
	}
	if (!track_resource(slots[p_slot].image, RenderCaptureTestResourceKind::Texture)) {
		rd->free_rid(slots[p_slot].image);
		slots[p_slot].image = RID();
		return ERR_INVALID_DATA;
	}
	// Sample the actual opaque viewport view, including its alpha swizzle.
	// No alpha override, CPU normalization or expected-value substitution.
	SkinnedMeshCaptureTestRecordingScope recording;
	effects->copy_to_rect(p_source, slots[p_slot].image, Rect2i(0, 0, 128, 64), false, false, true, true, false);
	const auto *actual = RenderCaptureTestAccess::texture(rd, slots[p_slot].image);
	if (!actual || !skinned_mesh_capture_test_image_recorded(uint64_t(rd->get_instance_id()), actual->driver_id.id, recording.identity())) {
		rd->free_rid(slots[p_slot].image);
		slots[p_slot].image = RID();
		return ERR_INVALID_DATA;
	}
	r_image = slots[p_slot].image;
	return OK;
}
