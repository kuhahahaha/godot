/**************************************************************************/
/*  gdextension_qualification_raster.cpp                                  */
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

#include "core/crypto/crypto_core.h"
#include "core/extension/gdextension_qualification_internal.h"
#include "servers/rendering/rendering_shader_container.h"

namespace GDExtensionDeviceExit {
namespace {
GDExtensionDeviceSessionResultV1 result(Error error, uint32_t reason = 0) {
	return { int32_t(error), reason, 0, 0 };
}
template <class T>
bool aligned(const T *p) {
	return p && uintptr_t(p) % alignof(T) == 0;
}
bool separate(const void *a, size_t as, const void *b, size_t bs) {
	auto x = uintptr_t(a), y = uintptr_t(b);
	return x <= y ? as <= y - x : bs <= x - y;
}
bool valid_abi(const GDExtensionDeviceExitAbi *abi) {
	return aligned(abi) && abi->abi_version == 1 && abi->struct_size == sizeof(*abi) && !abi->reserved;
}
} //namespace
GDExtensionDeviceSessionResultV1 Registry::qualification_raster_unit_create(GDExtensionQualificationBridgeRefV1 ref, const GDExtensionQualificationRasterCreateV1 *q, const void *bytes, uint64_t size, GDExtensionQualificationRasterBackingV1 *out) {
	const auto *request = q ? &q->unit : nullptr;
	if (!aligned(out) || !aligned(q) || !separate(q, sizeof(*q), out, sizeof(*out))) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	*out = {};
	if (!valid_abi(&q->abi) || !valid_abi(&request->abi) || !q->source.session_serial || !q->source.generation || q->profile != 1 || q->push_constant_bytes > 128 || (q->push_constant_bytes & 3) || request->reserved32 || request->reserved64 || !request->library_generation || !request->preparation_generation || !request->operation_serial || !size || size > 67108864 || size != request->container_byte_count || !bytes || !separate(bytes, size, out, sizeof(*out)) || !separate(bytes, size, q, sizeof(*q))) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	QualificationBridge *bridge = nullptr;
	RenderingDevice *rd = nullptr;
	{
		Lock lock(*this);
		Error error = qualification_bridge_locked(ref, true, false, bridge, rd, 18);
		if (error != OK) {
			return result(error, error == ERR_UNAUTHORIZED ? 15 : 16);
		}
		if (request->library_generation != records[bridge->library].snapshot.library_generation) {
			return result(ERR_INVALID_DATA, 16);
		}
		if (!ShaderCompilePolicy::flags_valid(request->expected_flags, rd->driver->get_shader_container_format().supported_compile_flags())) {
			return result(ERR_UNAVAILABLE, 18);
		}
	}
	Vector<uint8_t> source;
	source.resize(size);
	memcpy(source.ptrw(), bytes, size);
	uint8_t digest[32];
	Error digest_error = CryptoCore::sha256(source.ptr(), size, digest);
	if (digest_error != OK) {
		return result(digest_error, 16);
	}
	if (memcmp(digest, request->container_digest, 32)) {
		return result(ERR_INVALID_DATA, 16);
	}
	// Validate the actual container before the first native allocation.
	Ref<RenderingShaderContainer> parsed = rd->driver->get_shader_container_format().create_container();
	if (parsed.is_null() || !parsed->from_bytes(source)) {
		return result(ERR_INVALID_DATA, 16);
	}
	if (uint64_t(parsed->get_compile_flags()) != request->expected_flags || parsed->get_shader_reflection().pipeline_type != RenderingDevice::PIPELINE_TYPE_RASTERIZATION) {
		return result(ERR_INVALID_DATA, 16);
	}
	const auto &reflection = parsed->get_shader_reflection();
	if (reflection.stages_bits != (RenderingDevice::SHADER_STAGE_VERTEX_BIT | RenderingDevice::SHADER_STAGE_FRAGMENT_BIT) || reflection.vertex_input_mask || reflection.fragment_output_mask || !reflection.uniform_sets.is_empty() || !reflection.specialization_constants.is_empty() || reflection.has_multiview || reflection.has_dynamic_buffers || reflection.push_constant_size != q->push_constant_bytes) {
		return result(ERR_INVALID_DATA, 16);
	}
	QualificationResource *target = nullptr;
	QualificationBridge::NativeUnit *child = nullptr;
	uint32_t slot = 0;
	RID shader_reservation, pipeline_reservation, framebuffer_reservation;
	{
		Lock lock(*this);
		Error error = qualification_bridge_locked(ref, true, false, bridge, rd, 18);
		if (error != OK) {
			return result(error, 16);
		}
		const bool accepted = qualification_accepted_call_locked(bridge, 18);
		bridge->collect_retired_children_locked();
		for (auto *existing : bridge->units) {
			if (existing && existing->origin.record_slot == request->record_slot && existing->origin.preparation_generation == request->preparation_generation && existing->origin.operation_serial == request->operation_serial && existing->origin.physical_unit == request->physical_unit) {
				if (!existing->raster || memcmp(&existing->raster->origin, q, sizeof(*q))) {
					return result(ERR_INVALID_DATA, 16);
				}
				if (existing->acknowledged) {
					return result(ERR_DOES_NOT_EXIST, 17);
				}
				if (existing->creating) {
					return result(ERR_BUSY, 16);
				}
				*out = existing->raster->backing;
				return result(OK);
			}
		}

		if (const auto *old = bridge->raster_origins.find_retired([&](const auto &origin) {
				return origin.unit.record_slot == request->record_slot && origin.unit.preparation_generation == request->preparation_generation && origin.unit.operation_serial == request->operation_serial && origin.unit.physical_unit == request->physical_unit;
			})) {
			return memcmp(&old->origin, q, sizeof(*q)) ? result(ERR_INVALID_DATA, 16) : result(ERR_DOES_NOT_EXIST, 17);
		}
		if (bridge->unit_origins.find_retired([&](const auto &origin) { return origin.record_slot == request->record_slot && origin.preparation_generation == request->preparation_generation && origin.operation_serial == request->operation_serial && origin.physical_unit == request->physical_unit; })) {
			return result(ERR_INVALID_DATA, 16);
		}
		error = qualification_resource_locked(bridge, q->target, 1, target);
		if (error != OK || target->creating || target->retiring || target->acknowledged || target->object_serial != q->target_object_serial || memcmp(&target->source, &q->source, sizeof(q->source)) || target->physical.use_closed.load() || target->physical.dependent_children.load() >= UINT32_MAX - 1) {
			return result(error != OK ? error : ERR_INVALID_DATA, 16);
		}
		const auto *texture = rd->texture_owner.get_or_null(RID::from_uint64(target->rid));
		if (!texture || texture->qualification_return != &target->physical || texture->format != RenderingDevice::DATA_FORMAT_D32_SFLOAT || texture->usage_flags != (RenderingDevice::TEXTURE_USAGE_SAMPLING_BIT | RenderingDevice::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) || texture->samples != RenderingDevice::TEXTURE_SAMPLES_1) {
			return result(ERR_INVALID_DATA, 16);
		}
		if (rd->production_finalization_started.load(std::memory_order_acquire) && !accepted) {
			return result(ERR_BUSY, GDEXTENSION_DEVICE_SESSION_REASON_NOT_ADMITTED_V1);
		}
		if ((bridge->closing || records[bridge->library].quiescing || records[bridge->library].close_started) && !accepted) {
			return result(ERR_BUSY, 16);
		}
		for (; slot < QualificationBridge::CHILD_CAPACITY && bridge->units[slot]; ++slot) {
		}
		const uint32_t refs_needed = bridge->cleanup_refs_reserved ? 1 : 3;
		if (bridge->unit_origins.used_count() + bridge->raster_origins.used_count() >= QualificationBridge::UNIT_HISTORY_CAPACITY || slot == QualificationBridge::CHILD_CAPACITY || private_limit < refs_needed || records[bridge->library].private_refs > private_limit - refs_needed || !qualification_serial_capacity(4) || !rd->_qualification_free_event_available(false) || !rd->next_qualification_object_serial || rd->next_qualification_object_serial > UINT64_MAX - 3) {
			return result(ERR_PARAMETER_RANGE_ERROR, 19);
		}
		if (rd->_recording_error() != OK) {
			return result(rd->_recording_error(), 16);
		}
		uint32_t history_index = UINT32_MAX;
		Error history_error = bridge->raster_origins.prepare(history_index);
		if (history_error != OK) {
			return result(history_error, 19);
		}
		child = memnew(QualificationBridge::NativeUnit);
		child->raster = memnew(QualificationBridge::RasterPayload);
		if (!rd->_qualification_reserve_free_event(&child->shader) || !rd->_qualification_reserve_free_event(&child->pipeline) || !rd->_qualification_reserve_free_event(&child->raster->framebuffer) || !rd->_qualification_free_event_available(false)) {
			if (child->shader.event_capacity_reserved) {
				rd->_qualification_release_free_event(&child->shader);
			}
			if (child->pipeline.event_capacity_reserved) {
				rd->_qualification_release_free_event(&child->pipeline);
			}
			if (child->raster->framebuffer.event_capacity_reserved) {
				rd->_qualification_release_free_event(&child->raster->framebuffer);
			}
			memdelete(child->raster);
			memdelete(child);
			return result(ERR_PARAMETER_RANGE_ERROR, 19);
		}
		if (!bridge->cleanup_refs_reserved) {
			records[bridge->library].private_refs += 2;
			bridge->cleanup_refs_reserved = true;
		}
		qualification_serials.take(child->serial);
		qualification_serials.take(child->retire_serial);
		qualification_serials.take(child->dependency_serial);
		++qualification_cleanup_serial_reservations;
		child->cleanup_serial_reserved = true;
		child->origin = *request;
		child->history_index = history_index;
		bridge->raster_origins.begin(history_index, *q, child->serial);
		child->raster->origin = *q;
		child->raster->target = target;
		child->raster->framebuffer.bridge = child->shader.bridge = child->pipeline.bridge = bridge->serial;
		child->raster->framebuffer.child = child->shader.child = child->pipeline.child = child->serial;
		child->shader.serial = rd->next_qualification_object_serial++;
		child->pipeline.serial = rd->next_qualification_object_serial++;
		child->raster->framebuffer.serial = rd->next_qualification_object_serial++;
		child->raster->backing.abi = { 1, sizeof(GDExtensionDeviceExitAbi), 0 };
		child->raster->backing.raster = { instance, child->serial };
		child->raster->backing.source = q->source;
		child->raster->backing.target = q->target;
		child->raster->backing.target_object_serial = q->target_object_serial;
		child->raster->backing.event_seq = rd->next_qualification_free_event++;
		shader_reservation = rd->shader_owner.allocate_rid();
		pipeline_reservation = rd->render_pipeline_owner.allocate_rid();
		framebuffer_reservation = rd->framebuffer_owner.allocate_rid();
		if (shader_reservation.is_null() || pipeline_reservation.is_null() || framebuffer_reservation.is_null()) {
			if (shader_reservation.is_valid()) {
				rd->shader_owner.initialize_rid(shader_reservation, RenderingDevice::Shader());
				rd->shader_owner.free(shader_reservation);
			}
			if (pipeline_reservation.is_valid()) {
				rd->render_pipeline_owner.initialize_rid(pipeline_reservation, RenderingDevice::RenderPipeline());
				rd->render_pipeline_owner.free(pipeline_reservation);
			}
			rd->_qualification_release_free_event(&child->shader);
			rd->_qualification_release_free_event(&child->pipeline);
			rd->_qualification_release_free_event(&child->raster->framebuffer);
			if (framebuffer_reservation.is_valid()) {
				rd->framebuffer_owner.initialize_rid(framebuffer_reservation, RenderingDevice::Framebuffer());
				rd->framebuffer_owner.free(framebuffer_reservation);
			}
			--qualification_cleanup_serial_reservations;
			bridge->raster_origins.discard(child->history_index, child->serial);
			memdelete(child->raster);
			memdelete(child);
			qualification_trim_cleanup_ref_credits_locked(bridge);
			return result(ERR_CANT_CREATE, 19);
		}
		bridge->units[slot] = child;
		++bridge->children;
		++records[bridge->library].private_refs;
		target->physical.dependent_children.fetch_add(1);
		child->raster->target_held = true;
	}
	RID shader = rd->_qualification_shader_create(source, RID(), {}, &child->shader, shader_reservation);
	if (shader.is_null()) {
		rd->shader_owner.initialize_rid(shader_reservation, RenderingDevice::Shader());
		rd->shader_owner.free(shader_reservation);
		rd->render_pipeline_owner.initialize_rid(pipeline_reservation, RenderingDevice::RenderPipeline());
		rd->render_pipeline_owner.free(pipeline_reservation);
		rd->_qualification_release_free_event(&child->shader);
		rd->_qualification_release_free_event(&child->pipeline);
		rd->_qualification_release_free_event(&child->raster->framebuffer);
		rd->framebuffer_owner.initialize_rid(framebuffer_reservation, RenderingDevice::Framebuffer());
		rd->framebuffer_owner.free(framebuffer_reservation);
		Lock lock(*this);
		target->physical.dependent_children.fetch_sub(1);
		CRASH_COND(!child->cleanup_serial_reserved || !qualification_cleanup_serial_reservations);
		--qualification_cleanup_serial_reservations;
		bridge->raster_origins.discard(child->history_index, child->serial);
		bridge->units[slot] = nullptr;
		--bridge->children;
		--records[bridge->library].private_refs;
		memdelete(child->raster);
		memdelete(child);
		qualification_trim_cleanup_ref_credits_locked(bridge);
		return result(FAILED, 16);
	}
	child->shader.rid = shader.get_id();
	child->raster->backing.shader_rid = shader.get_id();
	child->raster->backing.shader_serial = child->shader.serial;
	child->raster->backing.known_mask = 1;
	child->raster->backing.effective_flags = uint64_t(rd->shader_get_compile_flags(shader));
	RID pipeline, framebuffer;
	framebuffer = rd->_qualification_framebuffer_create(RID::from_uint64(target->rid), &child->raster->framebuffer, &target->physical, framebuffer_reservation);
	if (framebuffer.is_valid()) {
		child->raster->framebuffer.rid = framebuffer.get_id();
		child->raster->backing.framebuffer_rid = framebuffer.get_id();
		child->raster->backing.framebuffer_serial = child->raster->framebuffer.serial;
		child->raster->backing.known_mask |= 4;
	} else {
		rd->framebuffer_owner.initialize_rid(framebuffer_reservation, RenderingDevice::Framebuffer());
		rd->framebuffer_owner.free(framebuffer_reservation);
		rd->_qualification_release_free_event(&child->raster->framebuffer);
		child->raster->framebuffer.serial = 0;
	}
	if (!framebuffer.is_valid() || child->raster->backing.effective_flags != request->expected_flags) {
		child->raster->backing.state = 2;
		child->raster->backing.phase = framebuffer.is_valid() ? 1 : 3;
		child->raster->backing.native_error = framebuffer.is_valid() ? ERR_INVALID_DATA : FAILED;
	} else {
		pipeline = rd->_qualification_render_pipeline_create(shader, rd->framebuffer_get_format(framebuffer), &child->pipeline, pipeline_reservation);
		if (pipeline.is_null()) {
			child->raster->backing.state = 2;
			child->raster->backing.phase = 2;
			child->raster->backing.native_error = FAILED;
		}
	}
	if (pipeline.is_valid()) {
		child->pipeline.rid = pipeline.get_id();
		child->raster->backing.pipeline_rid = pipeline.get_id();
		child->raster->backing.pipeline_serial = child->pipeline.serial;
		child->raster->backing.known_mask |= 2;
		child->raster->backing.state = 1;
	} else {
		rd->render_pipeline_owner.initialize_rid(pipeline_reservation, RenderingDevice::RenderPipeline());
		rd->render_pipeline_owner.free(pipeline_reservation);
		rd->_qualification_release_free_event(&child->pipeline);
		child->pipeline.serial = 0;
	}
	Lock lock(*this);
	child->backing.known_mask = child->raster->backing.known_mask;
	child->backing.possible_mask = child->raster->backing.possible_mask;
	child->creating = false;
	*out = child->raster->backing;
	return result(OK);
}

Error Registry::qualification_raster_begin_retire_locked(QualificationBridge *bridge, uint64_t serial, uint64_t operation) {
	for (auto *child : bridge->units) {
		if (!child || child->serial != serial || !child->raster || child->creating || child->acknowledged) {
			continue;
		}
		auto &raster = *child->raster;
		if (child->retiring) {
			return raster.retirement.retire_operation == operation ? OK : ERR_INVALID_DATA;
		}
		child->retiring = true;
		child->shader.use_closed.store(true);
		child->pipeline.use_closed.store(true);
		raster.framebuffer.use_closed.store(true);
		raster.retirement = { { 1, sizeof(GDExtensionDeviceExitAbi), 0 }, { instance, serial }, operation, 0 };
		auto &r = raster.report;
		r.abi = { 1, sizeof(GDExtensionDeviceExitAbi), 0 };
		r.raster = { instance, serial };
		r.shader_rid = raster.backing.shader_rid;
		r.pipeline_rid = raster.backing.pipeline_rid;
		r.framebuffer_rid = raster.backing.framebuffer_rid;
		r.shader_serial = raster.backing.shader_serial;
		r.pipeline_serial = raster.backing.pipeline_serial;
		r.framebuffer_serial = raster.backing.framebuffer_serial;
		r.shader_frame_slot = r.shader_rid ? UINT32_MAX : 0;
		r.pipeline_frame_slot = r.pipeline_rid ? UINT32_MAX : 0;
		r.framebuffer_frame_slot = r.framebuffer_rid ? UINT32_MAX : 0;
		r.report_revision = 1;
		r.retire_operation = operation;
		child->report.report_revision = 1;
		child->report.retire_operation = operation;
		return OK;
	}
	return ERR_DOES_NOT_EXIST;
}

void Registry::qualification_raster_retire_step(QualificationBridge *bridge, RenderingDevice *rd, uint64_t serial) {
	// Called by the original retire dispatcher with its RD safe-window lock held.
	QualificationBridge::NativeUnit *child = nullptr;
	for (auto *entry : bridge->units) {
		if (entry && entry->serial == serial && entry->raster && entry->retiring) {
			child = entry;
			break;
		}
	}
	if (!child) {
		return;
	}
	auto &raster = *child->raster;
	bool failed;
	{
		Lock lock(*this);
		if (raster.report.report_revision == UINT64_MAX) {
			return;
		}
		failed = child->maintenance_error != OK;
	}
	auto enqueue = [&](QualificationPhysicalReturn &cell, uint32_t type) -> uint32_t {
		if (!cell.rid || cell.enqueued) {
			return 0;
		}
		if (cell.production_uses || cell.dependent_children.load()) {
			return 4;
		}
		for (auto *ticket : cell.uses) {
			if (!ticket->physically_retired.load()) {
				return 3;
			}
		}
		const RID rid = RID::from_uint64(cell.rid);
		const auto *dependents = rd->dependency_map.getptr(rid);
		if (dependents && !dependents->is_empty()) {
			return 4;
		}
		if (type == 1) {
			const auto *shader = rd->shader_owner.get_or_null(rid);
			if (!shader || shader->qualification_return != &cell) {
				return 6;
			}
			if (!rd->driver->shader_retire_ready_nonblocking(shader->driver_id)) {
				return 2;
			}
		} else if (type == 2) {
			const auto *pipeline = rd->render_pipeline_owner.get_or_null(rid);
			if (!pipeline || pipeline->qualification_return != &cell || pipeline->qualification_dependency != &child->shader) {
				return 6;
			}
		} else {
			const auto *framebuffer = rd->framebuffer_owner.get_or_null(rid);
			if (!framebuffer || framebuffer->qualification_return != &cell || !raster.target || framebuffer->qualification_dependency != &raster.target->physical) {
				return 6;
			}
		}
		if (!rd->_qualification_free_event_available(cell.event_capacity_reserved) || uint32_t(rd->frame) >= rd->frames.size() || !rd->frames[rd->frame].qualification_epoch) {
			return 5;
		}
		if (rd->_recording_error() != OK || rd->draw_list.active || rd->compute_list.active || rd->raytracing_list.active || rd->frame_wait_draining || rd->draw_graph.has_driver_callbacks()) {
			return 1;
		}
		const bool previous = rd->qualification_retiring;
		rd->qualification_retiring = true;
		rd->_thread_safe_.unlock();
		rd->free_rid(rid);
		rd->_thread_safe_.lock();
		rd->qualification_retiring = previous;
		return cell.enqueued ? 0 : 5;
	};
	uint32_t reason = 0;
	if (!failed && !raster.backing.possible_mask) {
		for (auto entry : { std::pair<QualificationPhysicalReturn *, uint32_t>{ &child->pipeline, 2 }, { &raster.framebuffer, 4 }, { &child->shader, 1 } }) {
			if ((raster.backing.known_mask & entry.second) && !entry.first->rid) {
				reason = 6;
				break;
			}
			const uint32_t next = enqueue(*entry.first, entry.second);
			if (next >= 5) {
				reason = next;
				break;
			}
			if (next) {
				reason = next;
			}
		}
	}
	Lock lock(*this);
	MutexLock shader_lock(child->shader.mutex);
	MutexLock pipeline_lock(child->pipeline.mutex);
	MutexLock framebuffer_lock(raster.framebuffer.mutex);
	auto report = raster.report;
	const auto &shader = child->shader;
	const auto &pipeline = child->pipeline;
	const auto &framebuffer = raster.framebuffer;
	report.enqueued_mask = (shader.enqueued ? 1U : 0U) | (pipeline.enqueued ? 2U : 0U) | (framebuffer.enqueued ? 4U : 0U);
	report.freed_mask = (shader.free_event ? 1U : 0U) | (pipeline.free_event ? 2U : 0U) | (framebuffer.free_event ? 4U : 0U);
	report.shader_frame_slot = shader.rid ? shader.frame_slot : 0;
	report.pipeline_frame_slot = pipeline.rid ? pipeline.frame_slot : 0;
	report.framebuffer_frame_slot = framebuffer.rid ? framebuffer.frame_slot : 0;
	report.shader_frame_epoch = shader.frame_epoch;
	report.pipeline_frame_epoch = pipeline.frame_epoch;
	report.framebuffer_frame_epoch = framebuffer.frame_epoch;
	report.shader_free_event_seq = shader.free_event;
	report.pipeline_free_event_seq = pipeline.free_event;
	report.framebuffer_free_event_seq = framebuffer.free_event;
	report.unresolved_mask = raster.backing.possible_mask;
	if (report.enqueued_mask == raster.backing.known_mask && !report.unresolved_mask) {
		report.dependency_set_serial = child->dependency_serial;
	}
	if (failed || reason >= 5 || report.unresolved_mask) {
		report.state = 2;
		report.return_status = 0;
		report.reason = reason == 6 ? 6 : 5;
		if (failed) {
			report.native_error = child->maintenance_error;
		}
	} else if (report.freed_mask == raster.backing.known_mask) {
		report.state = 1;
		report.return_status = 1;
		report.reason = 0;
	} else if (report.state != 2) {
		report.reason = reason ? reason : 1;
	}
	if (memcmp(&report, &raster.report, sizeof(report))) {
		++report.report_revision;
		raster.report = report;
	}
	// The common scheduler consults only this maintenance summary. Typed raster
	// reports, backing identity and all three physical proofs remain distinct.
	child->report.state = report.state;
	child->report.return_status = report.return_status;
	child->report.reason = report.reason;
	child->report.native_error = report.native_error;
	child->report.report_revision = report.report_revision;
}

GDExtensionDeviceSessionResultV1 Registry::qualification_raster_unit_retire(GDExtensionQualificationBridgeRefV1 ref, const GDExtensionQualificationRasterRetireRequestV1 *q, GDExtensionQualificationRasterRetireRefV1 *out, GDExtensionQualificationRasterRetireReportV1 *report) {
	if (!aligned(q) || !aligned(out) || !aligned(report) || !separate(q, sizeof(*q), out, sizeof(*out)) || !separate(q, sizeof(*q), report, sizeof(*report)) || !separate(out, sizeof(*out), report, sizeof(*report))) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	*out = {};
	*report = {};
	if (!valid_abi(&q->abi) || q->reserved || !q->retire_operation || q->raster.registry_instance != instance || !q->raster.handle_serial) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	QualificationBridge *bridge = nullptr;
	RenderingDevice *rd = nullptr;
	{
		Lock lock(*this);
		Error error = qualification_bridge_locked(ref, true, false, bridge, rd, 19);
		if (error != OK) {
			return result(error, 16);
		}
		error = qualification_raster_begin_retire_locked(bridge, q->raster.handle_serial, q->retire_operation);
		if (error != OK) {
			return result(error, error == ERR_DOES_NOT_EXIST ? 17 : 16);
		}
	}
	qualification_retire_step(bridge, rd, q->raster.handle_serial, 2);
	Lock lock(*this);
	for (auto *child : bridge->units) {
		if (child && child->raster && child->serial == q->raster.handle_serial) {
			*out = { instance, child->retire_serial };
			*report = child->raster->report;
			return result(OK);
		}
	}
	return result(ERR_DOES_NOT_EXIST, 17);
}

GDExtensionDeviceSessionResultV1 Registry::qualification_raster_unit_retire_observe(GDExtensionQualificationBridgeRefV1 ref, GDExtensionQualificationRasterRetireRefV1 retire, const GDExtensionQualificationBudgetV1 *budget, GDExtensionQualificationRasterRetireReportV1 *out) {
	if (!aligned(budget) || !aligned(out) || !separate(budget, sizeof(*budget), out, sizeof(*out))) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	*out = {};
	if (!valid_abi(&budget->abi) || budget->reserved || budget->max_passes < 1 || budget->max_passes > 32 || budget->max_micros < 1 || budget->max_micros > 5000) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	uint64_t serial = 0;
	{
		Lock lock(*this);
		QualificationBridge *bridge = nullptr;
		RenderingDevice *rd = nullptr;
		Error error = qualification_bridge_locked(ref, false, false, bridge, rd);
		if (error != OK) {
			return result(error, 17);
		}
		for (auto *child : bridge->units) {
			if (retire.registry_instance == instance && child && child->raster && child->retiring && !child->acknowledged && child->retire_serial == retire.handle_serial) {
				serial = child->serial;
				break;
			}
		}
	}
	if (!serial) {
		return result(ERR_DOES_NOT_EXIST, 17);
	}
	Error error = qualification_schedule(ref, serial, 2, *budget);
	if (error != OK) {
		return result(error, 17);
	}
	Lock lock(*this);
	QualificationBridge *bridge = nullptr;
	RenderingDevice *rd = nullptr;
	error = qualification_bridge_locked(ref, false, false, bridge, rd);
	if (error != OK) {
		return result(error, 17);
	}
	for (auto *child : bridge->units) {
		if (child && child->raster && child->serial == serial) {
			*out = child->raster->report;
			return result(OK);
		}
	}
	return result(ERR_DOES_NOT_EXIST, 17);
}

GDExtensionDeviceSessionResultV1 Registry::qualification_raster_unit_retire_release(GDExtensionQualificationBridgeRefV1 ref, GDExtensionQualificationRasterRetireRefV1 retire) {
	Lock lock(*this);
	QualificationBridge *bridge = nullptr;
	RenderingDevice *rd = nullptr;
	Error error = qualification_bridge_locked(ref, false, false, bridge, rd);
	if (error != OK) {
		return result(error, 17);
	}
	for (auto *child : bridge->units) {
		if (retire.registry_instance != instance || !child || !child->raster || !child->retiring || child->acknowledged || child->retire_serial != retire.handle_serial) {
			continue;
		}
		auto &raster = *child->raster;
		const auto &b = raster.backing;
		const auto &r = raster.report;
		const uint32_t known = b.known_mask;
		if (child->creating || child->queued || child->accepted_rt_refs || child->maintenance_error != OK || !valid_abi(&b.abi) || !valid_abi(&r.abi) || b.raster.registry_instance != instance || b.raster.handle_serial != child->serial || r.raster.registry_instance != instance || r.raster.handle_serial != child->serial || !b.event_seq || !r.report_revision || !r.retire_operation || r.retire_operation != raster.retirement.retire_operation || !child->dependency_serial || r.dependency_set_serial != child->dependency_serial || !known || (known & ~7U) || b.possible_mask || r.state != 1 || r.return_status != 1 || r.reason || r.native_error || r.native_code || r.unresolved_mask || r.enqueued_mask != known || r.freed_mask != known || r.absent_mask) {
			return result(ERR_BUSY, 16);
		}
		MutexLock shader_lock(child->shader.mutex);
		MutexLock pipeline_lock(child->pipeline.mutex);
		MutexLock framebuffer_lock(raster.framebuffer.mutex);
		auto matches = [&](const QualificationPhysicalReturn &cell, uint32_t bit, uint64_t rid, uint64_t serial, uint64_t report_rid, uint64_t report_serial, uint32_t frame, uint64_t epoch, uint64_t event) {
			if (cell.production_uses || cell.dependent_children.load()) {
				return false;
			}
			if (!(known & bit)) {
				return !rid && !serial && !report_rid && !report_serial && !frame && !epoch && !event && !cell.rid && !cell.serial && !cell.event_capacity_reserved && !cell.enqueued && !cell.reserved_event && !cell.free_event;
			}
			return rid && serial && cell.bridge == bridge->serial && cell.child == child->serial && cell.rid == rid && cell.serial == serial && cell.use_closed.load() && cell.enqueued && !cell.event_capacity_reserved && cell.frame_slot != UINT32_MAX && cell.frame_epoch && cell.free_event && cell.free_event == cell.reserved_event && cell.free_event > b.event_seq && report_rid == rid && report_serial == serial && frame == cell.frame_slot && epoch == cell.frame_epoch && event == cell.free_event;
		};
		if (!matches(child->shader, 1, b.shader_rid, b.shader_serial, r.shader_rid, r.shader_serial, r.shader_frame_slot, r.shader_frame_epoch, r.shader_free_event_seq) || !matches(child->pipeline, 2, b.pipeline_rid, b.pipeline_serial, r.pipeline_rid, r.pipeline_serial, r.pipeline_frame_slot, r.pipeline_frame_epoch, r.pipeline_free_event_seq) || !matches(raster.framebuffer, 4, b.framebuffer_rid, b.framebuffer_serial, r.framebuffer_rid, r.framebuffer_serial, r.framebuffer_frame_slot, r.framebuffer_frame_epoch, r.framebuffer_free_event_seq) || (r.shader_free_event_seq && (r.shader_free_event_seq == r.pipeline_free_event_seq || r.shader_free_event_seq == r.framebuffer_free_event_seq)) || (r.pipeline_free_event_seq && r.pipeline_free_event_seq == r.framebuffer_free_event_seq)) {
			return result(ERR_BUSY, 16);
		}
		if (!raster.target || !raster.target_held || !raster.target->physical.dependent_children.load()) {
			return result(ERR_BUSY, 16);
		}
		raster.target->physical.dependent_children.fetch_sub(1);
		raster.target_held = false;
		if (child->cleanup_serial_reserved) {
			CRASH_COND(!qualification_cleanup_serial_reservations);
			--qualification_cleanup_serial_reservations;
			child->cleanup_serial_reserved = false;
		}
		child->acknowledged = true;
		--bridge->children;
		--records[bridge->library].private_refs;
		qualification_trim_cleanup_ref_credits_locked(bridge);
		return result(OK);
	}
	return result(ERR_DOES_NOT_EXIST, 17);
}

} //namespace GDExtensionDeviceExit
