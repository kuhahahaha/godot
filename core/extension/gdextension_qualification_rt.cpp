#include "gdextension_qualification_internal.h"

#include "core/os/thread.h"
#include "servers/rendering/rendering_server_default.h"

namespace GDExtensionDeviceExit {
thread_local Registry::QualificationBridge *Registry::active_qualification_bridge = nullptr;
thread_local uint64_t Registry::active_qualification_serial = 0;
namespace {
GDExtensionDeviceSessionResultV1 result(Error error, uint32_t reason = 0) {
	return { int32_t(error), reason, 0, 0 };
}
template <class T>
bool aligned(const T *p) {
	return p && uintptr_t(p) % alignof(T) == 0;
}
bool disjoint(const void *a, size_t as, const void *b, size_t bs) {
	const auto x = uintptr_t(a), y = uintptr_t(b);
	return x <= y ? as <= y - x : bs <= x - y;
}
bool abi(const GDExtensionDeviceExitAbi &value, uint32_t size = sizeof(GDExtensionDeviceExitAbi)) {
	return value.abi_version == 1 && value.struct_size == size && !value.reserved;
}
template <class T>
T pod(const void *bytes) {
	T value;
	memcpy(&value, bytes, sizeof(value));
	return value;
}
constexpr uint32_t request_sizes[] = { 0, 8, 56, 80, 104, 48, 312, 152, 136, 312 };
constexpr uint32_t result_sizes[] = { 0, 48, 72, 136, 96, 184, 200, 200, 120, 184 };
constexpr uint32_t MAX_REQUEST = 312, MAX_RESULT = 200, MAX_SEGMENTS = 4096;
constexpr uint64_t MAX_PAYLOAD = 67108864;
Error validate_request(const GDExtensionQualificationRtRequestV1 &h, const uint8_t *bytes) {
	switch (h.kind) {
		case 1:
			if (!pod<uint64_t>(bytes) || h.payload_bytes) {
				return ERR_INVALID_PARAMETER;
			}
			break;
		case 2: {
			const auto q = pod<GDExtensionStorageCreateRequestV1>(bytes);
			if (!abi(q.abi) || q.reserved || !q.bytes || q.bytes > 268435456 || !q.allocation_generation || !q.create_attempt || h.payload_bytes) {
				return ERR_INVALID_PARAMETER;
			}
		} break;
		case 3: {
			const auto q = pod<GDExtensionStorageRetireRequestV1>(bytes);
			if (!abi(q.abi) || q.reserved || !q.backing.registry_instance || !q.backing.handle_serial || !q.rid || !q.observed_backing_serial || !q.allocation_generation || !q.retire_attempt || h.payload_bytes) {
				return ERR_INVALID_PARAMETER;
			}
		} break;
		case 4: {
			const auto q = pod<GDExtensionNativeUnitCreateRequestV1>(bytes);
			if (!abi(q.abi) || q.reserved32 || q.reserved64 || !q.library_generation || !q.preparation_generation || !q.operation_serial || !q.container_byte_count || q.container_byte_count > MAX_PAYLOAD || h.payload_bytes != q.container_byte_count) {
				return ERR_INVALID_PARAMETER;
			}
		} break;
		case 5: {
			const auto q = pod<GDExtensionNativeUnitRetireRequestV1>(bytes);
			if (!abi(q.abi) || q.reserved || !q.unit.registry_instance || !q.unit.handle_serial || !q.retire_operation || h.payload_bytes) {
				return ERR_INVALID_PARAMETER;
			}
		} break;
		case 6:
		case 9: {
			const auto q = pod<GDExtensionQualificationRecordV1>(bytes);
			if (!abi(q.abi) || !q.record_generation || !q.request_nonce || !q.library_generation || !q.qualification_session_serial || !q.attempt || !q.sample_count || q.sample_count > 512 || q.range_end < q.range_begin || q.range_end - q.range_begin != q.sample_count || q.body_groups_x != (q.sample_count + 63) / 64 || !q.readback_size || q.readback_size > 16384 || h.payload_bytes != (h.kind == 9 ? 16 : 0)) {
				return ERR_INVALID_PARAMETER;
			}
		} break;
		case 7: {
			const auto q = pod<GDExtensionQualificationUploadPrepareV1>(bytes);
			if (!abi(q.abi) || q.reserved || !q.record_generation || !q.request_nonce || !q.library_generation || !q.qualification_session_serial || !q.parent_serial || !q.reservation_serial || !q.attempt || !q.upload_count || q.upload_count > 128 || !q.max_segments || q.max_segments > MAX_SEGMENTS || !q.expected_result_bytes || q.expected_result_bytes > 16384 || h.payload_bytes != uint64_t(q.upload_count) * sizeof(GDExtensionQualificationUploadPlanV1)) {
				return ERR_INVALID_PARAMETER;
			}
		} break;
		case 8: {
			const auto q = pod<GDExtensionQualificationUploadRequestV1>(bytes);
			if (!abi(q.abi) || q.reserved || !q.record_generation || !q.parent_serial || !q.rid || !q.backing_serial || !q.byte_size || q.byte_size > UINT32_MAX || q.byte_offset > UINT32_MAX || q.byte_size > UINT32_MAX - q.byte_offset || !q.expected_revision || q.role >= GDEXTENSION_QUALIFICATION_UPLOAD_ROLE_COUNT_V1 || q.owner_kind < 1 || q.owner_kind > 5 || h.payload_bytes != 16 + q.byte_size || h.segment_capacity < (q.byte_size + 31) / 32) {
				return ERR_INVALID_PARAMETER;
			}
		} break;
		default:
			return ERR_INVALID_PARAMETER;
	}
	return OK;
}
} //namespace
bool Registry::qualification_accepted_call_locked(QualificationBridge *bridge, uint32_t kind) {
	if (!kind || active_qualification_bridge != bridge || !active_qualification_serial || !bridge->rt) {
		return false;
	}
	auto *cell = bridge->rt;
	if (!cell->occupied || !cell->active || !cell->queued || !cell->executing || cell->serial != active_qualification_serial || cell->header.kind != kind || cell->admission_consumed) {
		return false;
	}
	// The accepted cell authorizes this one declared typed call. Consume at its
	// original admission point even when no close is in progress; later nested
	// work cannot inherit an unused close exception from this dispatch scope.
	cell->admission_consumed = true;
	return true;
}
GDExtensionDeviceSessionResultV1 Registry::qualification_rt_configure(GDExtensionQualificationBridgeRefV1 ref, const GDExtensionQualificationRtConfigV1 *config) {
	if (!aligned(config) || !abi(config->abi, sizeof(*config)) || config->slot_count != 1 || config->max_request_bytes < 8 || config->max_request_bytes > MAX_REQUEST || config->max_result_bytes < 48 || config->max_result_bytes > MAX_RESULT || !config->max_segment_count || config->max_segment_count > MAX_SEGMENTS || !config->max_payload_bytes || config->max_payload_bytes > MAX_PAYLOAD) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	const uint64_t total = sizeof(GDExtensionQualificationRtRequestV1) + uint64_t(config->max_request_bytes) + config->max_payload_bytes + config->max_result_bytes + uint64_t(config->max_segment_count) * sizeof(GDExtensionQualificationUploadSegmentV1);
	if (total > SIZE_MAX) {
		return result(ERR_PARAMETER_RANGE_ERROR, 19);
	}
	MutexLock lock(mutex);
	QualificationBridge *bridge = nullptr;
	RenderingDevice *rd = nullptr;
	Error error = qualification_bridge_locked(ref, false, true, bridge, rd);
	if (error != OK) {
		return result(error, error == ERR_UNAUTHORIZED ? 15 : 16);
	}
	if (bridge->rt && bridge->rt->ever_accepted) {
		return result(ERR_BUSY, 16);
	}
	auto *cell = memnew(QualificationRtCell);
	cell->config = *config;
	error = cell->request.resize(config->max_request_bytes);
	if (error == OK) {
		error = cell->payload.resize(config->max_payload_bytes);
	}
	if (error == OK) {
		error = cell->result.resize(config->max_result_bytes);
	}
	if (error == OK) {
		error = cell->segments.resize(config->max_segment_count);
	}
	if (error != OK) {
		memdelete(cell);
		return result(error, 19);
	}
	if (bridge->rt) {
		memdelete(bridge->rt);
	}
	bridge->rt = cell;
	return result(OK);
}
Error Registry::qualification_rt_ticket_locked(QualificationBridge *bridge, const GDExtensionQualificationRtRequestV1 &header, const uint8_t *request, const uint8_t *payload, QualificationTicket *&ticket) {
	ticket = nullptr;
	if (header.kind != 8 && header.kind != 9) {
		return OK;
	}
	const auto ref = pod<GDExtensionQualificationTicketRefV1>(payload);
	Error error = qualification_ticket_locked(bridge, ref, ticket);
	if (error != OK) {
		return ERR_DOES_NOT_EXIST;
	}
	if (!ticket->prepared || ticket->recording || ticket->admission_closed || ticket->mapping_failed || ticket->append_failed) {
		return ERR_BUSY;
	}
	const auto &origin = ticket->preparation;
	if (header.kind == 8) {
		const auto q = pod<GDExtensionQualificationUploadRequestV1>(request);
		if (q.upload_index >= ticket->uploads.size() || ticket->recorded) {
			return ERR_INVALID_DATA;
		}
		const auto &upload = ticket->uploads[q.upload_index];
		const auto &plan = upload.plan;
		if (q.record_generation != origin.record_generation || q.parent_serial != origin.parent_serial || q.rid != plan.rid || q.backing_serial != plan.backing_serial || q.byte_offset != plan.byte_offset || q.byte_size != plan.byte_size || q.role != plan.role || q.owner_kind != plan.owner_kind || memcmp(&q.e05_backing, &plan.e05_backing, sizeof(plan.e05_backing)) || memcmp(q.payload_digest, plan.payload_digest, 32) || (q.expected_revision != ticket->report.revision && (!upload.attempted || q.expected_revision != upload.last_request_revision)) || header.segment_capacity < upload.progress.max_segments) {
			return ERR_INVALID_DATA;
		}
	} else {
		const auto q = pod<GDExtensionQualificationRecordV1>(request);
		if (q.record_generation != origin.record_generation || q.request_nonce != origin.request_nonce || q.library_generation != origin.library_generation || q.qualification_session_serial != origin.qualification_session_serial || q.vector_ordinal != origin.vector_ordinal || q.attempt != origin.attempt || q.readback_size != origin.expected_result_bytes || memcmp(q.request_digest, origin.request_digest, 32) || memcmp(q.unit_digest, origin.unit_digest, 32)) {
			return ERR_INVALID_DATA;
		}
		if (ticket->recorded) {
			if (memcmp(&q, &ticket->origin, sizeof(q))) {
				return ERR_INVALID_DATA;
			}
		} else {
			for (const auto &upload : ticket->uploads) {
				if (!upload.attempted || upload.failed || upload.progress.consumed != upload.plan.byte_size || upload.report.state != 1 || upload.report.possible_count) {
					return ERR_BUSY;
				}
			}
		}
	}
	return OK;
}
GDExtensionDeviceSessionResultV1 Registry::qualification_rt_reserve(GDExtensionQualificationBridgeRefV1 ref, const GDExtensionQualificationRtRequestV1 *input, const void *request, const void *payload, GDExtensionQualificationRtRefV1 *out) {
	if (!aligned(input) || !aligned(out) || !request || uintptr_t(request) % 8 || !disjoint(input, sizeof(*input), out, sizeof(*out))) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	const auto header = *input;
	// Also check each supplied pointer itself when a malformed header declares
	// an empty span. Do not infer any additional caller allocation beyond it.
	if (!disjoint(request, 1, out, sizeof(*out)) || !disjoint(request, header.request_bytes, out, sizeof(*out)) || (payload && (!disjoint(payload, 1, out, sizeof(*out)) || header.payload_bytes > SIZE_MAX || !disjoint(payload, size_t(header.payload_bytes), out, sizeof(*out))))) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	if (!abi(header.abi, sizeof(header)) || header.flags || header.reserved || !header.operation_serial || header.kind < 1 || header.kind > 9 || header.request_bytes != request_sizes[header.kind] || header.result_capacity != result_sizes[header.kind] || header.segment_capacity > MAX_SEGMENTS || (header.kind == 8 ? !header.segment_capacity : header.segment_capacity != 0) || header.payload_bytes > MAX_PAYLOAD || ((header.payload_bytes != 0) != (payload != nullptr))) {
		*out = {};
		return result(ERR_INVALID_PARAMETER, 2);
	}
	if (!disjoint(request, header.request_bytes, input, sizeof(*input)) || (header.payload_bytes && (!disjoint(payload, header.payload_bytes, input, sizeof(*input)) || !disjoint(payload, header.payload_bytes, request, header.request_bytes)))) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	*out = {};
	MutexLock lock(mutex);
	QualificationBridge *bridge = nullptr;
	RenderingDevice *rd = nullptr;
	Error error = qualification_bridge_locked(ref, false, false, bridge, rd);
	if (error != OK) {
		return result(error, error == ERR_UNAUTHORIZED ? 15 : 17);
	}
	auto *cell = bridge->rt;
	if (!cell) {
		return result(ERR_UNAVAILABLE, 18);
	}
	if (header.request_bytes > cell->config.max_request_bytes || header.result_capacity > cell->config.max_result_bytes || header.payload_bytes > cell->config.max_payload_bytes || header.segment_capacity > cell->config.max_segment_count) {
		return result(ERR_PARAMETER_RANGE_ERROR, 19);
	}
	if (cell->occupied) {
		if (header.operation_serial != cell->header.operation_serial) {
			return result(ERR_BUSY, 16);
		}
		if (memcmp(&header, &cell->header, sizeof(header)) || memcmp(request, cell->request.ptr(), header.request_bytes) || (header.payload_bytes && memcmp(payload, cell->payload.ptr(), header.payload_bytes))) {
			return result(ERR_INVALID_DATA, 16);
		}
		*out = { instance, cell->serial };
		return result(OK);
	}
	// Copy only into the already configured scratch span. Until all checks
	// succeed no ref, serial, retain, child state or native work is published.
	memcpy(cell->request.ptrw(), request, header.request_bytes);
	error = validate_request(header, cell->request.ptr());
	if (error != OK) {
		return result(error, 2);
	}
	if (header.kind == 3 || header.kind == 5) {
		error = qualification_cleanup_retire_matches_original_child_locked(bridge, header.kind, cell->request.ptr());
		if (error != OK) {
			return result(error, error == ERR_DOES_NOT_EXIST ? 17 : 16);
		}
	} else {
		error = qualification_bridge_locked(ref, false, true, bridge, rd);
		if (error != OK) {
			return result(error, error == ERR_UNAUTHORIZED ? 15 : 16);
		}
	}
	if (header.operation_serial <= cell->retired_operation) {
		return result(ERR_DOES_NOT_EXIST, 17);
	}
	bool *cleanup_reserved = nullptr;
	uint64_t cleanup_child_serial = 0;
	if (header.kind == 3) {
		const auto q = pod<GDExtensionStorageRetireRequestV1>(cell->request.ptr());
		for (auto *child : bridge->storage) {
			if (child && child->serial == q.backing.handle_serial) {
				cleanup_reserved = &child->cleanup_serial_reserved;
				cleanup_child_serial = child->serial;
				break;
			}
		}
	} else if (header.kind == 5) {
		const auto q = pod<GDExtensionNativeUnitRetireRequestV1>(cell->request.ptr());
		for (auto *child : bridge->units) {
			if (child && child->serial == q.unit.handle_serial) {
				cleanup_reserved = &child->cleanup_serial_reserved;
				cleanup_child_serial = child->serial;
				break;
			}
		}
	}
	const bool use_cleanup_reservation = cleanup_reserved && *cleanup_reserved;
	const bool use_slot_credit = (header.kind == 3 || header.kind == 5) && bridge->cleanup_refs_reserved;
	const bool use_shared_queue = bridge->cleanup_refs_reserved && (use_slot_credit || header.kind == 8 || header.kind == 9);
	const uint32_t refs_needed = uint32_t(!use_slot_credit) + uint32_t(!use_shared_queue);
	if (use_slot_credit && bridge->cleanup_slot_in_use) {
		return result(ERR_BUSY, 19);
	}
	if (private_limit < refs_needed || records[bridge->library].private_refs > private_limit - refs_needed || (use_cleanup_reservation ? (qualification_serials.exhausted || !qualification_cleanup_serial_reservations) : !qualification_serial_capacity(1))) {
		return result(ERR_PARAMETER_RANGE_ERROR, 19);
	}
	if (header.payload_bytes) {
		memcpy(cell->payload.ptrw(), payload, header.payload_bytes);
	}
	if (header.kind == 7) {
		const auto q = pod<GDExtensionQualificationUploadPrepareV1>(cell->request.ptr());
		bool seen[128] = {};
		uint64_t segments = 0;
		for (uint32_t i = 0; i < q.upload_count; ++i) {
			const auto p = pod<GDExtensionQualificationUploadPlanV1>(cell->payload.ptr() + i * sizeof(GDExtensionQualificationUploadPlanV1));
			if (!abi(p.abi) || p.reserved || p.upload_index >= q.upload_count || seen[p.upload_index] || p.role >= 8 || p.owner_kind < 1 || p.owner_kind > 5 || !p.byte_size || p.byte_size > UINT32_MAX || p.byte_offset > UINT32_MAX || p.byte_size > UINT32_MAX - p.byte_offset) {
				return result(ERR_INVALID_PARAMETER, 2);
			}
			seen[p.upload_index] = true;
			segments += (p.byte_size + 31) / 32;
			if (segments > q.max_segments) {
				return result(ERR_PARAMETER_RANGE_ERROR, 19);
			}
		}
	}
	QualificationTicket *ticket = nullptr;
	error = qualification_rt_ticket_locked(bridge, header, cell->request.ptr(), cell->payload.ptr(), ticket);
	if (error != OK) {
		return result(error, error == ERR_DOES_NOT_EXIST ? 17 : 16);
	}
	if (ticket && ticket->accepted_rt_refs == UINT32_MAX) {
		return result(ERR_PARAMETER_RANGE_ERROR, 19);
	}
	memset(cell->result.ptrw(), 0, header.result_capacity);
	if (header.segment_capacity) {
		memset(cell->segments.ptrw(), 0, header.segment_capacity * sizeof(GDExtensionQualificationUploadSegmentV1));
	}
	if (use_cleanup_reservation) {
		--qualification_cleanup_serial_reservations;
		*cleanup_reserved = false;
	}
	qualification_serials.take(cell->serial);
	cell->cleanup_child_serial = use_cleanup_reservation ? cleanup_child_serial : 0;
	cell->header = header;
	cell->ticket = ticket;
	cell->occupied = true;
	cell->ever_accepted = true;
	cell->active = false;
	cell->queued = false;
	cell->executing = false;
	cell->observed = false;
	cell->admission_consumed = false;
	cell->state = 0;
	cell->call_result = {};
	cell->result_bytes = cell->segment_count = 0;
	cell->slot_ref_credit = use_slot_credit;
	cell->queue_ref_reserved = !use_shared_queue;
	if (use_slot_credit) {
		bridge->cleanup_slot_in_use = true;
	} else {
		++records[bridge->library].private_refs;
	}
	if (cell->queue_ref_reserved) {
		++records[bridge->library].private_refs;
	}
	if (ticket) {
		++ticket->accepted_rt_refs;
	}
	++bridge->children;
	*out = { instance, cell->serial };
	return result(OK);
}
Error Registry::qualification_rt_schedule(GDExtensionQualificationBridgeRefV1 ref) {
	QualificationBridge *bridge = nullptr;
	RenderingDevice *rd = nullptr;
	QualificationRtCell *cell = nullptr;
	{
		MutexLock lock(mutex);
		Error error = qualification_bridge_locked(ref, false, false, bridge, rd);
		if (error != OK) {
			return error;
		}
		cell = bridge->rt;
		if (!cell || !cell->occupied) {
			return ERR_DOES_NOT_EXIST;
		}
		if (!cell->active || cell->state != 1 || cell->queued) {
			return OK;
		}
		// Already accepted ticket work must drain after owner cancellation even
		// when its slot consumed the last unreserved library reference.
		const bool ticket_work = (cell->header.kind == 8 || cell->header.kind == 9) && cell->ticket;
		const bool use_queue_credit = (cell->header.kind == 3 || cell->header.kind == 5 || ticket_work) && bridge->cleanup_refs_reserved;
		if (use_queue_credit) {
			if (!bridge->claim_cleanup_queue(cell->queued, cell->queue_ref_credit)) {
				return OK;
			}
		} else if (cell->queue_ref_reserved) {
			cell->queued = true;
			++bridge->children;
		} else {
			if (records[bridge->library].private_refs >= private_limit) {
				return OK;
			}
			cell->queued = true;
			++bridge->children;
			++records[bridge->library].private_refs;
		}
	}
	auto *server = Object::cast_to<RenderingServerDefault>(RenderingServer::get_singleton());
	if (server && server->_try_qualification_rt_dispatch(bridge->serial, cell->serial)) {
		return OK;
	}
	MutexLock lock(mutex);
	CRASH_COND(!cell->queued);
	if (cell->queue_ref_credit) {
		bridge->release_cleanup_queue(cell->queued, cell->queue_ref_credit);
	} else {
		cell->queued = false;
		--bridge->children;
		if (!cell->queue_ref_reserved) {
			--records[bridge->library].private_refs;
		}
	}
	qualification_trim_cleanup_ref_credits_locked(bridge);
	return OK;
}
GDExtensionDeviceSessionResultV1 Registry::qualification_rt_activate(GDExtensionQualificationBridgeRefV1 ref, GDExtensionQualificationRtRefV1 rt_ref) {
	{
		MutexLock lock(mutex);
		QualificationBridge *bridge = nullptr;
		RenderingDevice *rd = nullptr;
		Error error = qualification_bridge_locked(ref, false, false, bridge, rd);
		if (error != OK) {
			return result(error, error == ERR_UNAUTHORIZED ? 15 : 17);
		}
		auto *cell = bridge->rt;
		if (rt_ref.registry_instance != instance || !cell || !cell->occupied || rt_ref.handle_serial != cell->serial) {
			return result(ERR_DOES_NOT_EXIST, 17);
		}
		if (cell->active) {
			// A completed, observed cleanup admission error can be retried on this
			// exact retained cell only while the original child proves no retirement
			// effect occurred. No other operation or uncertain result is replayed.
			if (!cell->cleanup_child_serial || cell->queued || cell->executing || !cell->observed || cell->state != 2 || cell->call_result.native_error == OK || qualification_cleanup_retire_matches_original_child_locked(bridge, cell->header.kind, cell->request.ptr()) != OK) {
				return result(ERR_BUSY, 16);
			}
			bool no_effect = false;
			if (cell->header.kind == 3) {
				for (auto *child : bridge->storage) {
					if (child && child->serial == cell->cleanup_child_serial) {
						MutexLock physical_lock(child->physical.mutex);
						no_effect = !child->creating && !child->retiring && !child->acknowledged && !child->physical.use_closed.load() && !child->physical.enqueued && !child->physical.free_event;
						break;
					}
				}
			}
			if (cell->header.kind == 5) {
				for (auto *child : bridge->units) {
					if (child && child->serial == cell->cleanup_child_serial) {
						MutexLock shader_lock(child->shader.mutex);
						MutexLock pipeline_lock(child->pipeline.mutex);
						no_effect = !child->creating && !child->retiring && !child->acknowledged && !child->shader.use_closed.load() && !child->pipeline.use_closed.load() && !child->shader.enqueued && !child->pipeline.enqueued && !child->shader.free_event && !child->pipeline.free_event;
						break;
					}
				}
			}
			if (!no_effect) {
				return result(ERR_BUSY, 16);
			}
			memset(cell->result.ptrw(), 0, cell->header.result_capacity);
			cell->result_bytes = 0;
			cell->segment_count = 0;
			cell->call_result = {};
			cell->observed = false;
			cell->admission_consumed = false;
		} else {
			cell->active = true;
		}
		cell->state = 1;
	}
	const Error error = qualification_rt_schedule(ref);
	return result(error, error == ERR_UNAUTHORIZED ? 15 : error != OK ? 17
																	  : 0);
}
GDExtensionDeviceSessionResultV1 Registry::qualification_rt_observe(GDExtensionQualificationBridgeRefV1 ref, GDExtensionQualificationRtRefV1 rt_ref, GDExtensionQualificationRtObservationV1 *out, void *result_bytes, uint32_t result_capacity, GDExtensionQualificationUploadSegmentV1 *segments, uint32_t segment_capacity) {
	if (!aligned(out) || !result_bytes || uintptr_t(result_bytes) % 8 || !result_capacity || result_capacity > MAX_RESULT || segment_capacity > MAX_SEGMENTS || (segment_capacity ? !aligned(segments) : segments != nullptr) || !disjoint(out, sizeof(*out), result_bytes, result_capacity) || (segment_capacity && (!disjoint(out, sizeof(*out), segments, sizeof(*segments) * segment_capacity) || !disjoint(result_bytes, result_capacity, segments, sizeof(*segments) * segment_capacity)))) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	*out = {};
	memset(result_bytes, 0, result_capacity);
	if (segment_capacity) {
		memset(segments, 0, sizeof(*segments) * segment_capacity);
	}
	{
		MutexLock lock(mutex);
		QualificationBridge *bridge = nullptr;
		RenderingDevice *rd = nullptr;
		Error error = qualification_bridge_locked(ref, false, false, bridge, rd);
		if (error != OK) {
			return result(error, error == ERR_UNAUTHORIZED ? 15 : 17);
		}
		auto *cell = bridge->rt;
		if (rt_ref.registry_instance != instance || !cell || !cell->occupied || rt_ref.handle_serial != cell->serial) {
			return result(ERR_DOES_NOT_EXIST, 17);
		}
		if (result_capacity != cell->header.result_capacity || segment_capacity != cell->header.segment_capacity) {
			return result(ERR_INVALID_PARAMETER, 2);
		}
	}
	Error error = qualification_rt_schedule(ref);
	if (error != OK) {
		return result(error, error == ERR_UNAUTHORIZED ? 15 : 17);
	}
	MutexLock lock(mutex);
	QualificationBridge *bridge = nullptr;
	RenderingDevice *rd = nullptr;
	error = qualification_bridge_locked(ref, false, false, bridge, rd);
	if (error != OK) {
		return result(error, error == ERR_UNAUTHORIZED ? 15 : 17);
	}
	auto *cell = bridge->rt;
	if (rt_ref.registry_instance != instance || !cell || !cell->occupied || rt_ref.handle_serial != cell->serial) {
		return result(ERR_DOES_NOT_EXIST, 17);
	}
	out->abi = { 1, sizeof(*out), 0 };
	out->kind = cell->header.kind;
	out->operation_serial = cell->header.operation_serial;
	out->slot_generation = cell->serial;
	out->state = 1;
	if (cell->state != 2 && cell->state != 3) {
		return result(OK);
	}
	// Registry acquire pairs with the RT's whole-result publication. A pending
	// observation never reads the result spans while the typed call owns them.
	CRASH_COND(cell->result_bytes != result_capacity || cell->segment_count > segment_capacity);
	memcpy(result_bytes, cell->result.ptr(), cell->result_bytes);
	if (cell->segment_count) {
		memcpy(segments, cell->segments.ptr(), cell->segment_count * sizeof(*segments));
	}
	out->state = cell->state;
	out->result_bytes = cell->result_bytes;
	out->segment_count = cell->segment_count;
	out->call_result = cell->call_result;
	cell->observed = true;
	return result(OK);
}
GDExtensionDeviceSessionResultV1 Registry::qualification_rt_release(GDExtensionQualificationBridgeRefV1 ref, GDExtensionQualificationRtRefV1 rt_ref) {
	MutexLock lock(mutex);
	QualificationBridge *bridge = nullptr;
	RenderingDevice *rd = nullptr;
	Error error = qualification_bridge_locked(ref, false, false, bridge, rd);
	if (error != OK) {
		return result(error, error == ERR_UNAUTHORIZED ? 15 : 17);
	}
	auto *cell = bridge->rt;
	if (rt_ref.registry_instance != instance || !cell || !cell->occupied || rt_ref.handle_serial != cell->serial) {
		return result(ERR_DOES_NOT_EXIST, 17);
	}
	if (cell->queued || cell->executing) {
		return result(ERR_BUSY, 19);
	}
	if (cell->active && (!cell->observed || (cell->state != 2 && cell->state != 3))) {
		return result(ERR_BUSY, 16);
	}
	if (cell->state == 3 && (!cell->ticket || !cell->ticket->physically_retired.load())) {
		return result(ERR_BUSY, 16);
	}
	if (cell->cleanup_child_serial) {
		bool *reserved = nullptr;
		bool acknowledged = false;
		if (cell->header.kind == 3) {
			for (auto *child : bridge->storage) {
				if (child && child->serial == cell->cleanup_child_serial) {
					reserved = &child->cleanup_serial_reserved;
					acknowledged = child->acknowledged;
					break;
				}
			}
		}
		if (cell->header.kind == 5) {
			for (auto *child : bridge->units) {
				if (child && child->serial == cell->cleanup_child_serial) {
					reserved = &child->cleanup_serial_reserved;
					acknowledged = child->acknowledged;
					break;
				}
			}
		}
		if (!reserved || *reserved) {
			return result(ERR_INVALID_DATA, 16);
		}
		const bool delivered = cell->active && cell->observed && cell->state == 2 && cell->call_result.native_error == OK;
		if (!acknowledged && !delivered) {
			// Keep the original ref occupied if releasing it would strand cleanup.
			if (!qualification_serial_capacity(1)) {
				return result(ERR_BUSY, 19);
			}
			++qualification_cleanup_serial_reservations;
			*reserved = true;
		}
	}
	if (cell->ticket) {
		CRASH_COND(!cell->ticket->accepted_rt_refs);
		--cell->ticket->accepted_rt_refs;
		cell->ticket = nullptr;
	}
	cell->retired_operation = cell->header.operation_serial;
	cell->occupied = false;
	cell->active = false;
	cell->observed = false;
	cell->state = 0;
	cell->cleanup_child_serial = 0;
	--bridge->children;
	if (cell->slot_ref_credit) {
		CRASH_COND(!bridge->cleanup_slot_in_use);
		bridge->cleanup_slot_in_use = false;
		cell->slot_ref_credit = false;
	} else {
		--records[bridge->library].private_refs;
	}
	if (cell->queue_ref_reserved) {
		--records[bridge->library].private_refs;
		cell->queue_ref_reserved = false;
	}
	qualification_trim_cleanup_ref_credits_locked(bridge);
	return result(OK);
}
void Registry::qualification_rt_dispatch(uint64_t bridge_serial, uint64_t serial) {
	QualificationBridge *bridge = nullptr;
	QualificationRtCell *cell = nullptr;
	RenderingDevice *rd = nullptr;
	Error admission = OK;
	{
		MutexLock lock(mutex);
		for (auto *entry : qualification_bridges) {
			if (entry && entry->serial == bridge_serial) {
				bridge = entry;
				break;
			}
		}
		CRASH_COND(!bridge || !bridge->rt);
		cell = bridge->rt;
		CRASH_COND(!cell->occupied || !cell->active || !cell->queued || cell->executing || cell->serial != serial || cell->state != 1);
		cell->executing = true;
		admission = qualification_bridge_locked({ instance, bridge_serial }, true, false, bridge, rd);
		if (cell->ticket && (cell->ticket->acknowledged || !cell->ticket->accepted_rt_refs)) {
			admission = ERR_DOES_NOT_EXIST;
		}
	}
	GDExtensionDeviceSessionResultV1 called = result(admission, admission == ERR_UNAUTHORIZED ? 15 : admission != OK ? 16
																													 : 0);
	uint32_t segment_count = 0;
	bool valid_output = true;
	const auto bridge_ref = GDExtensionQualificationBridgeRefV1{ instance, bridge_serial };
	auto save = [&](const auto &value, uint32_t offset = 0) { memcpy(cell->result.ptrw() + offset, &value, sizeof(value)); };
	if (admission == OK) {
		auto *previous_bridge = active_qualification_bridge;
		const uint64_t previous_serial = active_qualification_serial;
		active_qualification_bridge = bridge;
		active_qualification_serial = serial;
		switch (cell->header.kind) {
			case 1: {
				GDExtensionQualificationBackingV1 out = {};
				called = qualification_backing(bridge_ref, pod<uint64_t>(cell->request.ptr()), &out);
				save(out);
				valid_output = called.native_error != OK || (abi(out.abi) && out.rid && out.backing_serial);
			} break;
			case 2: {
				const auto q = pod<GDExtensionStorageCreateRequestV1>(cell->request.ptr());
				GDExtensionStorageBackingV1 out = {};
				called = qualification_storage_create(bridge_ref, &q, &out);
				save(out);
				valid_output = called.native_error != OK || (abi(out.abi) && out.backing.registry_instance == instance && out.backing.handle_serial);
			} break;
			case 3: {
				const auto q = pod<GDExtensionStorageRetireRequestV1>(cell->request.ptr());
				GDExtensionStorageRetireRefV1 child = {};
				GDExtensionStorageRetireReportV1 out = {};
				called = qualification_storage_retire(bridge_ref, &q, &child, &out);
				save(child);
				save(out, sizeof(child));
				valid_output = called.native_error != OK || (abi(out.abi) && child.registry_instance == instance && child.handle_serial);
			} break;
			case 4: {
				const auto q = pod<GDExtensionNativeUnitCreateRequestV1>(cell->request.ptr());
				GDExtensionNativeUnitBackingV1 out = {};
				called = qualification_native_unit_create(bridge_ref, &q, cell->payload.ptr(), q.container_byte_count, &out);
				save(out);
				valid_output = called.native_error != OK || (abi(out.abi) && out.unit.registry_instance == instance && out.unit.handle_serial);
			} break;
			case 5: {
				const auto q = pod<GDExtensionNativeUnitRetireRequestV1>(cell->request.ptr());
				GDExtensionNativeUnitRetireRefV1 child = {};
				GDExtensionNativeUnitRetireReportV1 out = {};
				called = qualification_native_unit_retire(bridge_ref, &q, &child, &out);
				save(child);
				save(out, sizeof(child));
				valid_output = called.native_error != OK || (abi(out.abi) && child.registry_instance == instance && child.handle_serial);
			} break;
			case 6: {
				const auto q = pod<GDExtensionQualificationRecordV1>(cell->request.ptr());
				GDExtensionQualificationTicketRefV1 child = {};
				GDExtensionQualificationReportV1 out = {};
				called = qualification_record(bridge_ref, &q, &child, &out);
				save(child);
				save(out, sizeof(child));
				valid_output = called.native_error != OK || (abi(out.abi) && child.registry_instance == instance && child.handle_serial);
			} break;
			case 7: {
				const auto q = pod<GDExtensionQualificationUploadPrepareV1>(cell->request.ptr());
				GDExtensionQualificationUploadPlanV1 plans[128];
				for (uint32_t i = 0; i < q.upload_count; ++i) {
					plans[i] = pod<GDExtensionQualificationUploadPlanV1>(cell->payload.ptr() + i * sizeof(plans[0]));
				}
				GDExtensionQualificationTicketRefV1 child = {};
				GDExtensionQualificationReportV1 out = {};
				called = qualification_prepare_uploads(bridge_ref, &q, plans, q.upload_count, &child, &out);
				save(child);
				save(out, sizeof(child));
				valid_output = called.native_error != OK || (abi(out.abi) && child.registry_instance == instance && child.handle_serial);
			} break;
			case 8: {
				const auto q = pod<GDExtensionQualificationUploadRequestV1>(cell->request.ptr());
				const auto ticket = pod<GDExtensionQualificationTicketRefV1>(cell->payload.ptr());
				GDExtensionQualificationUploadReportV1 out = {};
				called = qualification_upload(bridge_ref, ticket, &q, cell->payload.ptr() + sizeof(ticket), uint32_t(q.byte_size), cell->segments.ptrw(), cell->header.segment_capacity, &out);
				save(out);
				segment_count = out.out_written;
				valid_output = called.native_error != OK || (abi(out.abi) && segment_count <= cell->header.segment_capacity);
			} break;
			case 9: {
				const auto q = pod<GDExtensionQualificationRecordV1>(cell->request.ptr());
				const auto ticket = pod<GDExtensionQualificationTicketRefV1>(cell->payload.ptr());
				GDExtensionQualificationReportV1 out = {};
				called = qualification_record_prepared(bridge_ref, ticket, &q, &out);
				save(out);
				valid_output = called.native_error != OK || abi(out.abi);
			} break;
			default:
				called = result(ERR_INVALID_PARAMETER, 2);
				valid_output = false;
				break;
		}
		active_qualification_bridge = previous_bridge;
		active_qualification_serial = previous_serial;
	}
	if (called.native_error != OK) {
		for (uint32_t i = 0; i < cell->header.result_capacity; ++i) {
			if (cell->result[i]) {
				valid_output = false;
				break;
			}
		}
	}
	if (segment_count > cell->header.segment_capacity) {
		valid_output = false;
		segment_count = cell->header.segment_capacity;
	}
	{
		MutexLock lock(mutex);
		cell->call_result = called;
		cell->result_bytes = cell->header.result_capacity;
		cell->segment_count = segment_count;
		cell->state = valid_output ? 2 : 3;
		cell->executing = false;
		// Whole API result publication and this command's last bridge access share
		// the original lock. Its later scalar queue-node destructor touches no cell,
		// extension callback, caller bytes, or DSO-owned object.
		CRASH_COND(!cell->queued);
		if (cell->queue_ref_credit) {
			bridge->release_cleanup_queue(cell->queued, cell->queue_ref_credit);
		} else {
			cell->queued = false;
			--bridge->children;
			if (!cell->queue_ref_reserved) {
				--records[bridge->library].private_refs;
			}
		}
		qualification_trim_cleanup_ref_credits_locked(bridge);
	}
}
} // namespace GDExtensionDeviceExit
