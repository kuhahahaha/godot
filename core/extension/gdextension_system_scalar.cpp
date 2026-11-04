/**************************************************************************/
/*  gdextension_system_scalar.cpp                                         */
/**************************************************************************/

#include "core/extension/gdextension.h"
#include "core/extension/gdextension_qualification_internal.h"
#include "core/os/thread.h"

namespace GDExtensionDeviceExit {
namespace {
GDExtensionDeviceSessionResultV1 scalar_result(Error error, uint32_t reason = 0) { return { int32_t(error), reason, 0, 0 }; }
template <class T> bool scalar_aligned(const T *p) { return p && uintptr_t(p) % alignof(T) == 0; }
bool scalar_separate(const void *a, size_t as, const void *b, size_t bs) {
	const uintptr_t x = uintptr_t(a), y = uintptr_t(b);
	return x <= y ? as <= y - x : bs <= x - y;
}
bool scalar_abi(const GDExtensionDeviceExitAbi &a) { return a.abi_version == 1 && a.struct_size == sizeof(a) && !a.reserved; }
struct ScalarUnlock {
	Mutex *mutex = nullptr;
	~ScalarUnlock() { if (mutex) mutex->unlock(); }
};
void scalar_report(const QualificationTicket *ticket, GDExtensionSystemScalarReportV1 *out) { *out = ticket->system_report; }
} // namespace

GDExtensionDeviceSessionResultV1 Registry::system_scalar_record(GDExtensionQualificationBridgeRefV1 ref, const GDExtensionSystemScalarRecordV1 *q, GDExtensionSystemScalarTicketRefV1 *out_ticket, GDExtensionSystemScalarReportV1 *out_report) {
	if (!scalar_aligned(q) || !scalar_aligned(out_ticket) || !scalar_aligned(out_report) ||
			!scalar_separate(q, sizeof(*q), out_ticket, sizeof(*out_ticket)) ||
			!scalar_separate(q, sizeof(*q), out_report, sizeof(*out_report)) ||
			!scalar_separate(out_ticket, sizeof(*out_ticket), out_report, sizeof(*out_report)) ||
			out_ticket->registry_instance || out_ticket->handle_serial) return scalar_result(ERR_INVALID_PARAMETER, 2);
	*out_ticket = {};
	*out_report = {};
	const uint64_t bytes = uint64_t(q->output_word_count) * sizeof(uint32_t);
	if (!scalar_abi(q->abi) || !scalar_abi(q->unit.abi) || !scalar_abi(q->output.abi) ||
			q->reserved || !q->library_generation || !q->session_serial || !q->record_generation || !q->request_nonce ||
			!q->output_allocation_generation || !q->output_word_count || bytes > 16384 ||
			q->output_offset > UINT32_MAX || bytes > UINT32_MAX - q->output_offset ||
			q->output_offset % sizeof(uint32_t) || q->output_offset > q->output.byte_size ||
			bytes > q->output.byte_size - q->output_offset) return scalar_result(ERR_INVALID_PARAMETER, 2);
	QualificationBridge *bridge = nullptr;
	RenderingDevice *rd = nullptr;
	QualificationTicket *ticket = nullptr;
	ScalarUnlock unlock;
	{
		MutexLock lock(mutex);
		Error error = qualification_bridge_locked(ref, true, true, bridge, rd);
		if (error != OK) return scalar_result(error, error == ERR_UNAUTHORIZED ? 15 : 16);
		if (q->library_generation != records[bridge->library].snapshot.library_generation) return scalar_result(ERR_INVALID_DATA, 16);
		for (auto *old : bridge->tickets) if (old && old->system_scalar && old->system_origin.record_generation == q->record_generation && old->system_origin.request_nonce == q->request_nonce) {
			if (old->acknowledged) return scalar_result(ERR_DOES_NOT_EXIST, 17);
			if (memcmp(&old->system_origin, q, sizeof(*q))) return scalar_result(ERR_INVALID_DATA, 16);
			*out_ticket = { instance, old->serial };
			scalar_report(old, out_report);
			return scalar_result(OK);
		}
		QualificationBridge::NativeUnit *unit = nullptr;
		for (auto *candidate : bridge->units) if (candidate && !candidate->creating && !candidate->retiring && !candidate->acknowledged && candidate->backing.unit.registry_instance == instance && candidate->backing.unit.handle_serial == q->unit.unit.handle_serial && !memcmp(&candidate->backing, &q->unit, sizeof(q->unit)) && candidate->backing.state == 1 && candidate->backing.known_mask == 3 && !candidate->backing.possible_mask) { unit = candidate; break; }
		QualificationBridge::Storage *output = nullptr;
		for (auto *candidate : bridge->storage) if (candidate && !candidate->creating && !candidate->retiring && !candidate->acknowledged && candidate->origin.allocation_generation == q->output_allocation_generation && candidate->backing.backing.registry_instance == instance && candidate->backing.backing.handle_serial == q->output.backing.handle_serial && !memcmp(&candidate->backing, &q->output, sizeof(q->output))) { output = candidate; break; }
		if (!unit || !output || unit->shader.use_closed.load() || unit->pipeline.use_closed.load() || output->physical.use_closed.load()) return scalar_result(ERR_UNAUTHORIZED, 16);
		if (!rd->driver->native_call_observation_supported()) return scalar_result(ERR_UNAVAILABLE, 18);
		if (!rd->_thread_safe_.try_lock()) return scalar_result(ERR_BUSY, 19);
		unlock.mutex = &rd->_thread_safe_;
		auto *shader = rd->shader_owner.get_or_null(RID::from_uint64(q->unit.shader_rid));
		auto *pipeline = rd->compute_pipeline_owner.get_or_null(RID::from_uint64(q->unit.pipeline_rid));
		auto *buffer = rd->storage_buffer_owner.get_or_null(RID::from_uint64(q->output.rid));
		if (!shader || !pipeline || !buffer || shader->qualification_return != &unit->shader || pipeline->qualification_return != &unit->pipeline || buffer->qualification_return != &output->physical || pipeline->shader.get_id() != q->unit.shader_rid || buffer->backing_serial != q->output.backing_serial || pipeline->push_constant_size || pipeline->set_formats.size() != 1 || shader->uniform_sets.size() != 1 || shader->uniform_sets[0].size() != 1 || shader->uniform_sets[0][0].binding != 0 || shader->uniform_sets[0][0].type != RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER || !shader->uniform_sets[0][0].writable || pipeline->local_group_size[0] != 1 || pipeline->local_group_size[1] != 1 || pipeline->local_group_size[2] != 1 || rd->_recording_error() != OK || rd->draw_list.active || rd->compute_list.active || rd->raytracing_list.active || rd->draw_graph.has_driver_callbacks()) return scalar_result(ERR_INVALID_DATA, 16);
		uint32_t slot = 0;
		for (; slot < QualificationBridge::CHILD_CAPACITY && bridge->tickets[slot]; ++slot) {}
		const uint32_t refs_needed = bridge->cleanup_refs_reserved ? 1 : 3;
		if (slot == QualificationBridge::CHILD_CAPACITY || private_limit < refs_needed || records[bridge->library].private_refs > private_limit - refs_needed || !qualification_serial_capacity(2) || !rd->next_qualification_object_serial || rd->next_qualification_object_serial == UINT64_MAX) return scalar_result(ERR_PARAMETER_RANGE_ERROR, 19);
		ticket = memnew(QualificationTicket(true));
		ticket->system_origin = *q;
		ticket->bridge_serial = bridge->serial;
		ticket->library = bridge->library;
		ticket->facts.reserve(QualificationTicket::FACT_CAPACITY);
		ticket->graphs.reserve(QualificationTicket::FACT_CAPACITY);
		ticket->natives.reserve(QualificationTicket::FACT_CAPACITY);
		ticket->resource_pins.reserve(128);
		ticket->physical_uses.reserve(128);
		ticket->result.instantiate();
		error = ticket->result->bytes.resize(bytes);
		if (error != OK) { memdelete(ticket); return scalar_result(error, 19); }
		ticket->result->qualification_owned = true;
		qualification_serials.take(ticket->serial);
		qualification_serials.take(ticket->result->request_serial);
		ticket->original_readback_serial = ticket->result->request_serial;
		auto &r = ticket->report;
		r.abi = { 1, sizeof(GDExtensionDeviceExitAbi), 0 };
		r.revision = 1;
		r.record_generation = q->record_generation;
		r.request_nonce = q->request_nonce;
		r.source_backing_serial = q->output.backing_serial;
		r.source_offset = q->output_offset;
		r.source_size = bytes;
		if (!bridge->cleanup_refs_reserved) { records[bridge->library].private_refs += 2; bridge->cleanup_refs_reserved = true; }
		bridge->tickets[slot] = ticket;
		++bridge->children;
		++records[bridge->library].private_refs;
		ticket->system_set_physical.bridge = bridge->serial;
		ticket->system_set_physical.child = ticket->serial;
		ticket->system_set_physical.serial = rd->next_qualification_object_serial++;
		RenderingDevice::Uniform uniform;
		uniform.binding = 0;
		uniform.uniform_type = RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER;
		uniform.append_id(RID::from_uint64(q->output.rid));
		Vector<RenderingDevice::Uniform> uniforms;
		uniforms.push_back(uniform);
		LocalVector<QualificationPhysicalReturn *> children;
		children.push_back(&unit->shader);
		children.push_back(&output->physical);
		ticket->system_set = rd->_qualification_uniform_set_create(uniforms, RID::from_uint64(q->unit.shader_rid), 0, &ticket->system_set_physical, children);
		if (ticket->system_set.is_null()) {
			bridge->tickets[slot] = nullptr;
			--bridge->children;
			--records[bridge->library].private_refs;
			memdelete(ticket);
			qualification_trim_cleanup_ref_credits_locked(bridge);
			return scalar_result(FAILED, 16);
		}
		LocalVector<RID> resources;
		resources.push_back(RID::from_uint64(q->unit.shader_rid));
		resources.push_back(RID::from_uint64(q->unit.pipeline_rid));
		resources.push_back(RID::from_uint64(q->output.rid));
		resources.push_back(ticket->system_set);
		error = rd->_qualification_pin_resources(ticket, resources);
		if (error != OK) {
			ticket->append_failed = true;
			r.native_error = error;
			qualification_ticket_publish(ticket, rd);
			*out_ticket = { instance, ticket->serial };
			scalar_report(ticket, out_report);
			return scalar_result(OK);
		}
		ticket->recording = true;
	}
	rd->qualification_recording = true;
	auto list = rd->compute_list_begin();
	if (list != RenderingDevice::INVALID_ID) {
		rd->compute_list_bind_compute_pipeline(list, RID::from_uint64(q->unit.pipeline_rid));
		rd->compute_list_bind_uniform_set(list, ticket->system_set, 0);
		ticket->body_append = rd->_record_source_dispatch({ q->request_nonce, q->library_generation, q->session_serial, q->record_generation, 0, q->output_word_count }, list, 1, 1, 1);
		ticket->body = ticket->body_append.handle;
		rd->compute_list_end();
	}
	{
		MutexLock lock(mutex);
		if (ticket->body.is_valid()) {
			GDExtensionQualificationFactV1 fact = {};
			fact.graph_instance = ticket->body.graph_instance; fact.graph_frame = ticket->body.graph_frame; fact.graph_kind = ticket->body.kind; fact.graph_index = ticket->body.index; fact.graph_offset = ticket->body.offset;
			fact.semantic_kind = 2; fact.command_slot = UINT32_MAX; fact.parent_fact = UINT32_MAX; fact.phase = 1;
			ticket->graphs.push_back({ ticket->body, 0, uint32_t(rd->frame), 0, rd->frames[rd->frame].qualification_epoch, false, false });
			ticket->facts.push_back(fact);
			rd->_qualification_add_frame_ticket(ticket);
			ticket->recorded = true;
		}
	}
	Error append_error = ticket->body.is_valid() ? OK : ticket->body_append.error == OK ? FAILED : ticket->body_append.error;
	if (ticket->body.is_valid()) {
		const Error copy_error = rd->_buffer_get_data_async(RID::from_uint64(q->output.rid), Callable(), q->output_offset, bytes, ticket->result);
		MutexLock lock(mutex);
		for (; ticket->readback_copies_joined < ticket->result->copies.size(); ++ticket->readback_copies_joined) {
			const auto &copy = ticket->result->copies[ticket->readback_copies_joined];
			GDExtensionQualificationFactV1 fact = {};
			fact.graph_instance = copy.handle.graph_instance; fact.graph_frame = copy.handle.graph_frame; fact.graph_kind = copy.handle.kind; fact.graph_index = copy.handle.index; fact.graph_offset = copy.handle.offset;
			fact.semantic_kind = 3; fact.command_slot = UINT32_MAX; fact.parent_fact = UINT32_MAX; fact.phase = 1;
			fact.source_backing_serial = q->output.backing_serial; fact.source_offset = copy.region.src_offset; fact.source_size = copy.region.size;
			ticket->graphs.push_back({ copy.handle, ticket->facts.size(), uint32_t(rd->frame), 0, rd->frames[rd->frame].qualification_epoch, false, false });
			ticket->facts.push_back(fact);
		}
		if (copy_error != OK && copy_error != ERR_BUSY) append_error = copy_error;
	}
	rd->qualification_recording = false;
	{
		MutexLock lock(mutex);
		ticket->recording = false;
		if (append_error != OK) { ticket->append_failed = true; if (ticket->report.native_error == OK) ticket->report.native_error = append_error; }
		qualification_ticket_publish(ticket, rd);
		*out_ticket = { instance, ticket->serial };
		scalar_report(ticket, out_report);
	}
	return scalar_result(OK);
}

GDExtensionDeviceSessionResultV1 Registry::system_scalar_observe(GDExtensionQualificationBridgeRefV1 ref, GDExtensionSystemScalarTicketRefV1 ticket_ref, const GDExtensionQualificationBudgetV1 *budget, GDExtensionSystemScalarReportV1 *out) {
	if (!scalar_aligned(budget) || !scalar_aligned(out) || !scalar_separate(budget, sizeof(*budget), out, sizeof(*out))) return scalar_result(ERR_INVALID_PARAMETER, 2);
	*out = {};
	if (!scalar_abi(budget->abi) || budget->reserved || !budget->max_passes || budget->max_passes > 32 || !budget->max_micros || budget->max_micros > 5000) return scalar_result(ERR_INVALID_PARAMETER, 2);
	{
		MutexLock lock(mutex);
		QualificationBridge *bridge = nullptr; RenderingDevice *rd = nullptr;
		Error error = qualification_bridge_locked(ref, false, false, bridge, rd);
		if (error != OK) return scalar_result(error, 17);
		if (ticket_ref.registry_instance != instance || !ticket_ref.handle_serial) return scalar_result(ERR_INVALID_DATA, 17);
		bool found = false;
		for (auto *ticket : bridge->tickets) if (ticket && ticket->system_scalar && !ticket->acknowledged && ticket->serial == ticket_ref.handle_serial) { found = true; break; }
		if (!found) return scalar_result(ERR_DOES_NOT_EXIST, 17);
	}
	Error error = qualification_schedule(ref, ticket_ref.handle_serial, 3, *budget);
	if (error != OK) return scalar_result(error, 17);
	MutexLock lock(mutex);
	QualificationBridge *bridge = nullptr; RenderingDevice *rd = nullptr;
	error = qualification_bridge_locked(ref, false, false, bridge, rd);
	if (error != OK) return scalar_result(error, 17);
	for (auto *ticket : bridge->tickets) if (ticket && ticket->system_scalar && !ticket->acknowledged && ticket->serial == ticket_ref.handle_serial) { scalar_report(ticket, out); return scalar_result(OK); }
	return scalar_result(ERR_DOES_NOT_EXIST, 17);
}

GDExtensionDeviceSessionResultV1 Registry::system_scalar_facts(GDExtensionQualificationBridgeRefV1 ref, GDExtensionSystemScalarTicketRefV1 ticket_ref, uint64_t revision, uint32_t first, uint32_t capacity, GDExtensionQualificationFactV1 *facts, uint32_t *written) {
	if (!scalar_aligned(written) || !scalar_aligned(facts) || !capacity || capacity > 64 || first > UINT32_MAX - capacity || !scalar_separate(facts, size_t(capacity) * sizeof(*facts), written, sizeof(*written))) return scalar_result(ERR_INVALID_PARAMETER, 2);
	*written = 0;
	MutexLock lock(mutex);
	QualificationBridge *bridge = nullptr; RenderingDevice *rd = nullptr;
	Error error = qualification_bridge_locked(ref, false, false, bridge, rd);
	if (error != OK) return scalar_result(error, 17);
	if (ticket_ref.registry_instance != instance || !ticket_ref.handle_serial) return scalar_result(ERR_INVALID_DATA, 17);
	for (auto *ticket : bridge->tickets) if (ticket && ticket->system_scalar && !ticket->acknowledged && ticket->serial == ticket_ref.handle_serial) {
		if (!revision || revision != ticket->system_report.revision || first > ticket->facts.size()) return scalar_result(ERR_INVALID_DATA, 17);
		const uint32_t count = MIN(capacity, ticket->facts.size() - first);
		for (uint32_t i = 0; i < count; ++i) facts[i] = ticket->facts[first + i];
		*written = count;
		return scalar_result(OK);
	}
	return scalar_result(ERR_DOES_NOT_EXIST, 17);
}

GDExtensionDeviceSessionResultV1 Registry::system_scalar_copy(GDExtensionQualificationBridgeRefV1 ref, GDExtensionSystemScalarTicketRefV1 ticket_ref, uint64_t revision, void *bytes, uint32_t capacity, uint32_t *written) {
	if (!scalar_aligned(written) || !bytes || !capacity || capacity > 16384 || !scalar_separate(bytes, capacity, written, sizeof(*written))) return scalar_result(ERR_INVALID_PARAMETER, 2);
	*written = 0;
	MutexLock lock(mutex);
	QualificationBridge *bridge = nullptr; RenderingDevice *rd = nullptr;
	Error error = qualification_bridge_locked(ref, false, false, bridge, rd);
	if (error != OK) return scalar_result(error, 17);
	if (ticket_ref.registry_instance != instance || !ticket_ref.handle_serial) return scalar_result(ERR_INVALID_DATA, 17);
	for (auto *ticket : bridge->tickets) if (ticket && ticket->system_scalar && !ticket->acknowledged && ticket->serial == ticket_ref.handle_serial) {
		if (!revision || revision != ticket->system_report.revision || ticket->system_report.state != 8 || !ticket->physically_retired.load() || ticket->result.is_null() || !ticket->readback_complete(0) || capacity != ticket->system_report.owned_size || capacity != ticket->result->bytes.size()) return scalar_result(ERR_BUSY, 16);
		memcpy(bytes, ticket->result->bytes.ptr(), capacity);
		*written = capacity;
		return scalar_result(OK);
	}
	return scalar_result(ERR_DOES_NOT_EXIST, 17);
}

GDExtensionDeviceSessionResultV1 Registry::system_scalar_cancel(GDExtensionQualificationBridgeRefV1 ref, GDExtensionSystemScalarTicketRefV1 ticket_ref) {
	{
		MutexLock lock(mutex);
		QualificationBridge *bridge = nullptr; RenderingDevice *rd = nullptr;
		Error error = qualification_bridge_locked(ref, false, false, bridge, rd);
		if (error != OK) return scalar_result(error, 17);
		if (ticket_ref.registry_instance != instance || !ticket_ref.handle_serial) return scalar_result(ERR_INVALID_DATA, 17);
		QualificationTicket *ticket = nullptr;
		for (auto *candidate : bridge->tickets) if (candidate && candidate->system_scalar && !candidate->acknowledged && candidate->serial == ticket_ref.handle_serial) { ticket = candidate; break; }
		if (!ticket) return scalar_result(ERR_DOES_NOT_EXIST, 17);
		if (ticket->stop_requested) return scalar_result(OK);
		if (ticket->report.state == 8 || (ticket->admission_closed && ticket->physically_retired.load())) return scalar_result(ERR_BUSY, 16);
		ticket->stop_requested = true;
		ticket->admission_closed = true;
		ticket->continuation_pending = false;
		ticket->physically_retired.store(false);
	}
	const GDExtensionQualificationBudgetV1 budget = { { 1, sizeof(GDExtensionDeviceExitAbi), 0 }, 1, 1000, 0 };
	Error error = qualification_schedule(ref, ticket_ref.handle_serial, 3, budget);
	return scalar_result(error, error == OK ? 0 : 17);
}

GDExtensionDeviceSessionResultV1 Registry::system_scalar_release(GDExtensionQualificationBridgeRefV1 ref, GDExtensionSystemScalarTicketRefV1 ticket_ref) {
	return qualification_release(ref, { ticket_ref.registry_instance, ticket_ref.handle_serial }, true);
}

void Registry::system_scalar_set_return_step(QualificationTicket *ticket, RenderingDevice *rd) {
	if (!ticket->system_scalar || ticket->system_set.is_null() || !ticket->physically_retired.load() || ticket->system_set_return_requested || rd->frame_wait_draining || rd->draw_graph.has_driver_callbacks()) return;
	if (!rd->_thread_safe_.try_lock()) return;
	ScalarUnlock unlock{ &rd->_thread_safe_ };
	if (rd->_recording_error() != OK || rd->draw_list.active || rd->compute_list.active || rd->raytracing_list.active || !rd->frames[rd->frame].qualification_epoch || !rd->_qualification_free_event_available(ticket->system_set_physical.event_capacity_reserved)) return;
	{
		MutexLock lock(mutex);
		if (!ticket->system_set.is_valid() || ticket->system_set_return_requested || !ticket->physically_retired.load()) return;
		auto *set = rd->uniform_set_owner.get_or_null(ticket->system_set);
		if (!set || set->qualification_return != &ticket->system_set_physical || set->qualification_users || ticket->system_set_physical.dependent_children.load()) { ticket->system_set_return_error = ERR_INVALID_DATA; return; }
		ticket->system_set_physical.use_closed.store(true);
	}
	rd->qualification_retiring = true;
	rd->free_rid(ticket->system_set);
	rd->qualification_retiring = false;
	MutexLock lock(mutex);
	if (!rd->uniform_set_owner.owns(ticket->system_set) && ticket->system_set_physical.enqueued) {
		ticket->system_set_return_requested = true;
	} else {
		ticket->system_set_physical.use_closed.store(false);
	}
}
} // namespace GDExtensionDeviceExit
