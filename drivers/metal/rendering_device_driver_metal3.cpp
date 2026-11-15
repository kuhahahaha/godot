/**************************************************************************/
/*  rendering_device_driver_metal3.cpp                                    */
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

#include "rendering_device_driver_metal3.h"

#include "core/config/project_settings.h"
#include "core/os/os.h"
#include "core/string/ustring.h"
#include "drivers/metal/pixel_formats.h"
#include "drivers/metal/rendering_context_driver_metal.h"

namespace MTL3 {

#pragma mark - Fence

void RenderingDeviceDriverMetal::Fence::signal(MTL::CommandBuffer *p_cb) {
	if (p_cb) {
		value++;
		p_cb->encodeSignalEvent(event.get(), value);
	}
}

Error RenderingDeviceDriverMetal::Fence::wait(uint32_t p_timeout_ms) {
	if (unlikely(value == 0)) {
		WARN_PRINT_ONCE("Never signaled fence.");
		return OK;
	}
	bool signaled = event->waitUntilSignaledValue(value, p_timeout_ms);
	if (!signaled) {
#ifdef DEBUG_ENABLED
		ERR_PRINT("timeout waiting for fence");
#endif
		return ERR_TIMEOUT;
	}
	return OK;
}

#pragma mark - Constructor / Destructor

RenderingDeviceDriverMetal::RenderingDeviceDriverMetal(RenderingContextDriverMetal *p_context_driver) :
		::RenderingDeviceDriverMetal(p_context_driver) {
}

RenderingDeviceDriverMetal::~RenderingDeviceDriverMetal() {
	drain_completion_records();
	for (MDCommandBuffer *cb : command_buffers) {
		memdelete(cb);
	}
}

#pragma mark - Initialization

Error RenderingDeviceDriverMetal::_create_device() {
	Error err = ::RenderingDeviceDriverMetal::_create_device();
	ERR_FAIL_COND_V(err, err);

	device_queue = NS::TransferPtr(device->newCommandQueue());
	ERR_FAIL_NULL_V(device_queue.get(), ERR_CANT_CREATE);
	device_queue->setLabel(MTLSTR("Godot Main Command Queue"));

	return OK;
}

void RenderingDeviceDriverMetal::_resolve_sync_mode() {
	if (sync_mode == Barriers) {
		print_verbose("Metal 3: Barrier synchronization enabled.");
		base_hazard_tracking = MTL::ResourceHazardTrackingModeUntracked;
		// Apple GPUs only sample counters at stage boundaries, which is all the
		// timestamp encoders rely on.
		timestamp_queries_supported = device->supportsCounterSampling(MTL::CounterSamplingPointAtStageBoundary);
	} else {
		print_verbose("Metal 3: Native hazard tracking enabled.");
	}
}

Error RenderingDeviceDriverMetal::initialize(uint32_t p_device_index, uint32_t p_frame_count) {
	Error err = _initialize(p_device_index, p_frame_count);
	ERR_FAIL_COND_V(err, err);

	return OK;
}

#pragma mark - Residency

void RenderingDeviceDriverMetal::add_residency_set_to_main_queue(MTL::ResidencySet *p_set) {
}

void RenderingDeviceDriverMetal::remove_residency_set_to_main_queue(MTL::ResidencySet *p_set) {
}

#pragma mark - Fences

RDD::FenceID RenderingDeviceDriverMetal::fence_create() {
	Fence *fence = memnew(Fence(NS::TransferPtr(device->newSharedEvent())));
	return FenceID(fence);
}

Error RenderingDeviceDriverMetal::fence_wait(FenceID p_fence) {
	Fence *fence = (Fence *)(p_fence.id);
	// A never-armed or already recycled fence is a healthy legacy no-op.
	if (!fence->armed) {
		return OK;
	}
	const FencePollResult completion = fence_wait_observed(p_fence, fence->value);
	if (!completion.matches(fence->value)) {
		return FAILED;
	}
	return fence_recycle(p_fence, fence->value);
}

RenderingDeviceDriver::FencePollResult RenderingDeviceDriverMetal::fence_poll(FenceID p_fence, uint64_t p_expected_submission_serial) {
	Fence *fence = (Fence *)(p_fence.id);
	FencePollResult out;
	out.completed_serial = fence->value;
	int64_t native_code = 0;
	if (completion_records_failed(fence->completion_records, native_code)) {
		out.native_error = true;
		out.native_code = native_code;
		return out;
	}
	if (fence->armed && p_expected_submission_serial != 0 && p_expected_submission_serial == fence->value && fence->value != 0 && fence->event->signaledValue() >= fence->value && completion_records_complete(fence->completion_records)) {
		out.completed = true;
		out.completed_serial = fence->value;
		fence->observed = true;
	}
	return out;
}

RenderingDeviceDriver::FencePollResult RenderingDeviceDriverMetal::fence_wait_observed(FenceID p_fence, uint64_t p_expected_submission_serial) {
	Fence *fence = (Fence *)(p_fence.id);
	FencePollResult out;
	out.completed_serial = fence->value;
	if (!fence->armed || p_expected_submission_serial == 0 || p_expected_submission_serial != fence->value) {
		out.native_error = true;
		out.native_code = ERR_INVALID_PARAMETER;
		return out;
	}
	for (CompletionRecord *record : fence->completion_records) {
		record->command_buffer->waitUntilCompleted();
	}
	return fence_poll(p_fence, p_expected_submission_serial);
}

Error RenderingDeviceDriverMetal::fence_recycle(FenceID p_fence, uint64_t p_completed_serial) {
	Fence *fence = (Fence *)(p_fence.id);
	if (p_completed_serial == 0 || p_completed_serial != fence->value || p_completed_serial < fence->last_recycled_value || !fence->armed || !fence->observed) {
		return ERR_INVALID_PARAMETER;
	}
	fence->last_recycled_value = p_completed_serial;
	retire_completion_records(fence->completion_records);
	fence->completion_records.clear();
	fence->armed = false;
	fence->observed = false;
	// Shared-event values are monotonically increasing; there is no native reset.
	return OK;
}

void RenderingDeviceDriverMetal::fence_free(FenceID p_fence) {
	Fence *fence = (Fence *)(p_fence.id);
	memdelete(fence);
}

#pragma mark - Semaphores

RDD::SemaphoreID RenderingDeviceDriverMetal::semaphore_create() {
	if (sync_mode != HazardTracking) {
		Semaphore *sem = memnew(Semaphore(NS::TransferPtr(device->newEvent())));
		return SemaphoreID(sem);
	}
	return SemaphoreID(1);
}

void RenderingDeviceDriverMetal::semaphore_free(SemaphoreID p_semaphore) {
	if (sync_mode != HazardTracking) {
		Semaphore *sem = (Semaphore *)(p_semaphore.id);
		memdelete(sem);
	}
}

#pragma mark - Command Queues

RDD::CommandQueueID RenderingDeviceDriverMetal::command_queue_create(CommandQueueFamilyID p_cmd_queue_family, bool p_identify_as_main_queue) {
	return CommandQueueID(1);
}

Error RenderingDeviceDriverMetal::_execute_and_present_barriers(CommandQueueID queue, VectorView<SemaphoreID> waits, VectorView<CommandBufferID> commands, VectorView<SemaphoreID> signals, FenceID fence, VectorView<SwapChainID> swaps) {
	return _execute_and_present_common(queue, waits, commands, signals, fence, swaps, true);
}
Error RenderingDeviceDriverMetal::_execute_and_present(CommandQueueID queue, VectorView<SemaphoreID> waits, VectorView<CommandBufferID> commands, VectorView<SemaphoreID> signals, FenceID fence, VectorView<SwapChainID> swaps) {
	return _execute_and_present_common(queue, waits, commands, signals, fence, swaps, false);
}
bool RenderingDeviceDriverMetal::native_issue_bound(NativeIssueKind p_kind, uint32_t p_commands, uint32_t &r_bound) const {
	if (p_kind == NativeIssueKind::PREPARE_SEGMENT) {
		r_bound = 1;
		return true;
	}
	const uint32_t extra = p_kind == NativeIssueKind::MAIN_SLOT ? 2 : 1;
	if (p_commands > UINT32_MAX - extra) {
		return false;
	}
	r_bound = p_commands ? p_commands + extra : 0;
	return true;
}

bool RenderingDeviceDriverMetal::qualification_incremental_fact_bound(uint64_t graphs,uint32_t current,uint32_t &main,uint32_t &copy) const {
 // One original main slot (at most wait+work+signal) and both nonblocking
 // prepare-segment calls per continuation. The extra frame is the initial
 // membership/failure frame. Screen split slots are admitted before recording.
 if(graphs>4096)return false;
 uint64_t bound=uint64_t(current)*3+3*(graphs+1);
 const uint64_t copy_bound=2*(graphs+1);
 for(const auto *command:command_buffers)bound+=command->pending_completion_records.size();
 if(bound+copy_bound>4096)return false;main=uint32_t(bound);copy=uint32_t(copy_bound);return true;
}
Error RenderingDeviceDriverMetal::qualification_worker_source_quote(CommandBufferID command,QueueCallReport &prefix,uint32_t &future) const {
 prefix={};future=2; // worker has no waits: work and optional signal only.
 if(!command)return ERR_INVALID_PARAMETER;
 const auto *wrapper=reinterpret_cast<const MDCommandBuffer *>(command.id);
 if(wrapper->completion_record_failed)return ERR_UNAVAILABLE;
 // Original caller holds this worker's thread and operations locks. No
 // append, end, submit, recycle or wrapper reuse can change this prefix.
 for(const auto *record:wrapper->pending_completion_records){
  NativeCallFact fact;fact.queue_index=1;fact.serial=record->serial;
  fact.completion_serial=record->serial;fact.completion=CompletionCapability::CommandBuffer;
  fact.issue=IssueFact::IssuedUnknown;prefix.calls.push_back(fact);
 }
 return OK;
}
uint32_t RenderingDeviceDriverMetal::qualification_native_call_bound(uint32_t slots) const {
	// Each actual Metal3 queue slot can commit its command plus the barrier-mode
	// wait and signal commands. Include every already committed wrapper source;
	// future unexpected expansion remains a retained failure, never truncation.
	uint64_t count = uint64_t(slots) * 3 + 1;
	for (const auto *command : command_buffers) {
		count += command->pending_completion_records.size();
	}
	return count > 4096 ? 0 : uint32_t(count);
}
uint32_t RenderingDeviceDriverMetal::qualification_native_call_bound_for_profile(uint32_t p_main_slots, uint32_t p_worker_slots, uint32_t p_frames) const {
	if (!p_frames || p_frames > p_main_slots || p_worker_slots > UINT32_MAX - p_main_slots) {
		return 0;
	}
	// _submit_transfer_worker passes no waits: _execute_and_present_common can
	// commit its work and a signal command, but never a wait command. Main slots
	// retain all three. Also reserve a hidden copy-queue source for every frame,
	// and all already committed wrapper sources. No worker or prefix is omitted.
	uint64_t count = uint64_t(p_main_slots) * 3 + uint64_t(p_worker_slots) * 2 + p_frames;
	for (const auto *command : command_buffers) {
		count += command->pending_completion_records.size();
	}
#ifdef TESTS_ENABLED
	if (OS::get_singleton()->has_environment("GODOT_QUALIFICATION_CAPACITY_TRACE")) {
		print_line(vformat("QCAP backend=MTL3 sync_mode=%d main_slots=%d worker_slots=%d frames=%d pending=%d raw_profile_bound=%d", int(sync_mode), p_main_slots, p_worker_slots, p_frames, count - uint64_t(p_main_slots) * 3 - uint64_t(p_worker_slots) * 2 - p_frames, count));
	}
#endif
	return count > 4096 ? 0 : uint32_t(count);
}
Error RenderingDeviceDriverMetal::_execute_and_present_common(CommandQueueID queue, VectorView<SemaphoreID> waits, VectorView<CommandBufferID> commands, VectorView<SemaphoreID> signals, FenceID fence_id, VectorView<SwapChainID> swaps, bool barriers) {
	QueueCallReport report;
	report.present_attempted = swaps.size() > 0;
	if (commands.size() == 0) {
		record_queue_call_report(report);
		return OK;
	}
 // Reserve the exact pending prefix and complete finite new serial range
 // before removing the old wrapper owner or issuing any new native effect.
 const uint64_t count=uint64_t(commands.size())+(barriers && waits.size()>0?1:0)+(barriers && signals.size()>0?1:0);
 const uint64_t first=count<=UINT32_MAX?reserve_native_call_serials(uint32_t(count)):0;
 LocalVector<uint64_t> serials;
 for(uint32_t i=0;i<commands.size();++i){const auto *command=reinterpret_cast<const MDCommandBuffer *>(commands[i].id);for(const auto *record:command->pending_completion_records)serials.push_back(record->serial);}
 if(first)for(uint32_t i=0;i<count;++i)serials.push_back(first+i);
 const Error admission=admit_native_sources(serials.ptr(),serials.size());
 if(admission!=OK){report.error=admission;record_queue_call_report(report);return admission;}
 LocalVector<CompletionRecord *> records;
 for(uint32_t i=0;i<commands.size();++i){
  auto *command=reinterpret_cast<MDCommandBuffer *>(commands[i].id);
  for(auto *record:command->pending_completion_records)records.push_back(record);
  command->pending_completion_records.clear();if(command->completion_record_failed)report.error=ERR_UNAVAILABLE;
 }
 if(!first)report.error=ERR_UNAVAILABLE;
	auto *previous_issue_count = active_native_issue_count;
	active_native_issue_count = &report.native_issue_count;
	auto *previous_records = active_completion_records;
	const uint64_t previous_serial = active_native_serial;
	const uint32_t previous_count = active_native_serial_count;
	active_completion_records = &records;
	active_native_serial = first;
	active_native_serial_count = first ? count : 0;
	auto *fence = reinterpret_cast<Fence *>(fence_id.id);
	uint64_t presented_serial = 0;
	if (report.error == OK && barriers && waits.size() > 0) {
		MTL::CommandBuffer *cb = device_queue->commandBuffer();
		if (!cb) {
			report.error = ERR_CANT_CREATE;
		} else {
#ifdef DEV_ENABLED
			cb->setLabel(MTLSTR("Wait Command Buffer"));
#endif
			for (uint32_t i = 0; i < waits.size(); ++i) {
				auto *sem = reinterpret_cast<Semaphore *>(waits[i].id);
				cb->encodeWait(sem->event.get(), sem->value);
			}
			if (register_completion_record(cb, 0)) {
				++report.native_issue_count;
				cb->commit();
			} else {
				report.error = active_native_source_admission && active_native_source_admission->error!=OK ? active_native_source_admission->error : ERR_UNAVAILABLE;
			}
		}
	}
	for (uint32_t i = 0; report.error == OK && i < commands.size(); ++i) {
		auto *command = reinterpret_cast<MDCommandBuffer *>(commands[i].id);
		const bool tail = i + 1 == commands.size();
		if (tail && fence) {
			command->end();
			MTL::CommandBuffer *cb = command->ensure_command_buffer();
			if (!cb) {
				report.error = ERR_CANT_CREATE;
				break;
			}
			fence->signal(cb);
			fence->observed = false;
		}
		if (tail) {
			for (uint32_t j = 0; j < swaps.size(); ++j) {
				auto *swap = reinterpret_cast<SwapChain *>(swaps[j].id);
				auto *surface = reinterpret_cast<RenderingContextDriverMetal::Surface *>(swap->surface);
				surface->present(command);
			}
		}
		command->commit();
		if (command->completion_record_failed) {
			report.error = active_native_source_admission && active_native_source_admission->error!=OK ? active_native_source_admission->error : ERR_UNAVAILABLE;
			break;
		}
		if (tail && fence) {
			fence->armed = true;
		}
		if (tail && report.present_attempted && !records.is_empty()) {
			presented_serial = records[records.size() - 1]->serial;
		}
	}
	if (report.error == OK && barriers && signals.size() > 0) {
		MTL::CommandBuffer *cb = device_queue->commandBuffer();
		if (!cb) {
			report.error = ERR_CANT_CREATE;
		} else {
			for (uint32_t i = 0; i < signals.size(); ++i) {
				auto *sem = reinterpret_cast<Semaphore *>(signals[i].id);
				sem->value++;
				cb->encodeSignalEvent(sem->event.get(), sem->value);
			}
			if (register_completion_record(cb, fence ? fence->value : 0)) {
				++report.native_issue_count;
				cb->commit();
			} else {
				report.error = active_native_source_admission && active_native_source_admission->error!=OK ? active_native_source_admission->error : ERR_UNAVAILABLE;
			}
		}
	}
	active_native_issue_count = previous_issue_count;
	active_completion_records = previous_records;
	active_native_serial = previous_serial;
	active_native_serial_count = previous_count;
 if(first){
  LocalVector<uint64_t> unused;
  for(uint32_t i=0;i<count;++i){bool registered=false;for(const auto *record:records)registered=registered||record->serial==first+i;if(!registered)unused.push_back(first+i);}
  cancel_native_sources(unused.ptr(),unused.size());
 }
	if (fence && fence->armed) {
		fence->completion_records = records;
		for (auto *record : records) {
			record->fence_owned = true;
		}
		report.submission_serial = fence->value;
	}
	report.issued = !records.is_empty();
	for (auto *record : records) {
		NativeCallFact fact;
		fact.queue_index = uint32_t(queue.id);
		fact.serial = record->serial;
		fact.issue = IssueFact::IssuedUnknown;
		fact.completion = CompletionCapability::CommandBuffer;
		fact.completion_serial = record->serial;
		fact.present = record->serial == presented_serial;
		report.calls.push_back(fact);
	}
	record_queue_call_report(report);
	return report.error;
}

Error RenderingDeviceDriverMetal::command_queue_execute_and_present(CommandQueueID p_cmd_queue, VectorView<SemaphoreID> p_wait_sem, VectorView<CommandBufferID> p_cmd_buffers, VectorView<SemaphoreID> p_cmd_sem, FenceID p_cmd_fence, VectorView<SwapChainID> p_swap_chains) {
	reset_queue_call_report();
	if (p_cmd_fence && ((Fence *)p_cmd_fence.id)->armed) {
		QueueCallReport report;
		report.error = ERR_BUSY;
		record_queue_call_report(report);
		return ERR_BUSY;
	}
	if (p_cmd_fence && ((Fence *)p_cmd_fence.id)->value == UINT64_MAX) {
		QueueCallReport report;
		report.error = active_native_source_admission && active_native_source_admission->error!=OK ? active_native_source_admission->error : ERR_UNAVAILABLE;
		record_queue_call_report(report);
		return ERR_UNAVAILABLE;
	}
	Error res;
	if (sync_mode != HazardTracking) {
		res = _execute_and_present_barriers(p_cmd_queue, p_wait_sem, p_cmd_buffers, p_cmd_sem, p_cmd_fence, p_swap_chains);
	} else {
		res = _execute_and_present(p_cmd_queue, p_wait_sem, p_cmd_buffers, p_cmd_sem, p_cmd_fence, p_swap_chains);
	}
	ERR_FAIL_COND_V(res != OK, res);

	if (p_swap_chains.size() > 0) {
		// Used as a signal that we're presenting, so this is the end of a frame.
		MTL::CaptureScope *scope = device_scope.get();
		scope->endScope();
		scope->beginScope();
	}

	return OK;
}

void RenderingDeviceDriverMetal::command_queue_free(CommandQueueID p_cmd_queue) {
}

#pragma mark - Command Pools

RDD::CommandPoolID RenderingDeviceDriverMetal::command_pool_create(CommandQueueFamilyID p_cmd_queue_family, CommandBufferType p_cmd_buffer_type) {
	DEV_ASSERT(p_cmd_buffer_type == COMMAND_BUFFER_TYPE_PRIMARY);
	return CommandPoolID(reinterpret_cast<uint64_t>(device_queue.get()));
}

bool RenderingDeviceDriverMetal::command_pool_reset(CommandPoolID p_cmd_pool) {
	return true;
}

void RenderingDeviceDriverMetal::command_pool_free(CommandPoolID p_cmd_pool) {
	// Nothing to free - the device_queue is managed by SharedPtr.
}

#pragma mark - Timestamp

RDD::QueryPoolID RenderingDeviceDriverMetal::timestamp_query_pool_create(uint32_t p_query_count) {
	if (timestamp_queries_supported) {
		QueryPool *pool = QueryPool::create(device, p_query_count);
		if (pool != nullptr) {
			return QueryPoolID(pool);
		}
		WARN_PRINT_ONCE("Metal 3: The device does not expose the timestamp counter set; GPU timestamps are unavailable.");
		timestamp_queries_supported = false;
	}
	return ::RenderingDeviceDriverMetal::timestamp_query_pool_create(p_query_count);
}

void RenderingDeviceDriverMetal::timestamp_query_pool_free(QueryPoolID p_pool_id) {
	if (!timestamp_queries_supported) {
		return ::RenderingDeviceDriverMetal::timestamp_query_pool_free(p_pool_id);
	}
	QueryPool *pool = (QueryPool *)(p_pool_id.id);
	memdelete(pool);
}

void RenderingDeviceDriverMetal::timestamp_query_pool_get_results(QueryPoolID p_pool_id, uint32_t p_query_count, uint64_t *r_results) {
	if (!timestamp_queries_supported) {
		return ::RenderingDeviceDriverMetal::timestamp_query_pool_get_results(p_pool_id, p_query_count, r_results);
	}
	QueryPool *pool = (QueryPool *)(p_pool_id.id);
	pool->get_results(p_query_count, r_results);
}

uint64_t RenderingDeviceDriverMetal::timestamp_query_result_to_time(uint64_t p_result) {
	return p_result; // Already converted to nanoseconds in get_results.
}

void RenderingDeviceDriverMetal::command_timestamp_write(CommandBufferID p_cmd_buffer, QueryPoolID p_pool_id, uint32_t p_index) {
	if (!timestamp_queries_supported) {
		return ::RenderingDeviceDriverMetal::command_timestamp_write(p_cmd_buffer, p_pool_id, p_index);
	}
	MDCommandBuffer *cmd = (MDCommandBuffer *)(p_cmd_buffer.id);
	QueryPool *pool = (QueryPool *)(p_pool_id.id);
	cmd->timestamp_write(pool, p_index);
}

#pragma mark - Command Buffers

RDD::CommandBufferID RenderingDeviceDriverMetal::command_buffer_create(CommandPoolID p_cmd_pool) {
	MTL::CommandQueue *queue = reinterpret_cast<MTL::CommandQueue *>(p_cmd_pool.id);
	MDCommandBuffer *obj = memnew(MDCommandBuffer(queue, this));
	command_buffers.push_back(obj);
	return CommandBufferID(obj);
}

} // namespace MTL3
