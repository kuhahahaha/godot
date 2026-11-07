/**************************************************************************/
/*  gdextension_qualification_rd.cpp                                      */
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

#include "core/extension/gdextension_qualification_internal.h"
using GDExtensionDeviceExit::QualificationPhysicalReturn;
using GDExtensionDeviceExit::QualificationTicket;
using GDExtensionDeviceExit::Registry;

bool RenderingDevice::_qualification_resource_busy(RID rid) {
	const Buffer *buffer = storage_buffer_owner.owns(rid) ? storage_buffer_owner.get_or_null(rid) : nullptr;
	if (!buffer) {
		buffer = uniform_buffer_owner.owns(rid) ? uniform_buffer_owner.get_or_null(rid) : nullptr;
	}
	if (buffer && buffer->qualification_users) {
		return true;
	}
	const auto *texture = texture_owner.owns(rid) ? texture_owner.get_or_null(rid) : nullptr;
	if (texture && texture->qualification_users) {
		return true;
	}
	const auto *sampler = sampler_owner.owns(rid) ? sampler_owner.get_or_null(rid) : nullptr;
	if (sampler && sampler->qualification_users) {
		return true;
	}
	if (texture && texture->qualification_return) {
		auto *cell = texture->qualification_return;
		if (!qualification_retiring || !cell->use_closed.load() || texture->transfer_worker_index >= 0 || !_qualification_free_event_available(cell->event_capacity_reserved) || uint32_t(frame) >= frames.size() || !frames[frame].qualification_epoch) {
			return true;
		}
		MutexLock lock(cell->mutex);
		if (cell->enqueued || cell->reserved_event || cell->free_event) {
			return true;
		}
	}
	if (sampler && sampler->qualification_return) {
		auto *cell = sampler->qualification_return;
		if (!qualification_retiring || !cell->use_closed.load() || !_qualification_free_event_available(cell->event_capacity_reserved) || uint32_t(frame) >= frames.size() || !frames[frame].qualification_epoch) {
			return true;
		}
		MutexLock lock(cell->mutex);
		if (cell->enqueued || cell->reserved_event || cell->free_event) {
			return true;
		}
	}
	const auto *set = uniform_set_owner.owns(rid) ? uniform_set_owner.get_or_null(rid) : nullptr;
	if (set && set->qualification_users) {
		return true;
	}
	if (set && set->qualification_return) {
		auto *cell = set->qualification_return;
		if (!qualification_retiring || !cell->use_closed.load() || !_qualification_free_event_available(cell->event_capacity_reserved) || uint32_t(frame) >= frames.size() || !frames[frame].qualification_epoch) {
			return true;
		}
		MutexLock lock(cell->mutex);
		if (cell->enqueued || cell->reserved_event || cell->free_event) {
			return true;
		}
	}
	const auto *pipeline = compute_pipeline_owner.owns(rid) ? compute_pipeline_owner.get_or_null(rid) : nullptr;
	const auto *raster_pipeline = render_pipeline_owner.owns(rid) ? render_pipeline_owner.get_or_null(rid) : nullptr;
	const auto *framebuffer = framebuffer_owner.owns(rid) ? framebuffer_owner.get_or_null(rid) : nullptr;
	if ((raster_pipeline && raster_pipeline->qualification_users) || (framebuffer && framebuffer->qualification_users)) {
		return true;
	}
	if (pipeline && pipeline->qualification_users) {
		return true;
	}
	for (auto *cell : { raster_pipeline ? raster_pipeline->qualification_return : nullptr, framebuffer ? framebuffer->qualification_return : nullptr }) {
		if (cell) {
			if (!qualification_retiring || !cell->use_closed.load() || !_qualification_free_event_available(cell->event_capacity_reserved) || uint32_t(frame) >= frames.size() || !frames[frame].qualification_epoch) {
				return true;
			}
			MutexLock lock(cell->mutex);
			if (cell->enqueued || cell->reserved_event || cell->free_event) {
				return true;
			}
		}
	}
	const auto *shader = shader_owner.owns(rid) ? shader_owner.get_or_null(rid) : nullptr;
	const auto *physical = buffer ? buffer->qualification_return : texture ? texture->qualification_return
			: sampler													   ? sampler->qualification_return
			: set														   ? set->qualification_return
			: pipeline													   ? pipeline->qualification_return
			: raster_pipeline											   ? raster_pipeline->qualification_return
			: framebuffer												   ? framebuffer->qualification_return
			: shader													   ? shader->qualification_return
																		   : nullptr;
	if (physical && (physical->production_uses || physical->dependent_children.load())) {
		return true;
	}
	const auto *dependencies = dependency_map.getptr(rid);
	if (dependencies) {
		for (RID child : *dependencies) {
			// A qualified texture is returned by its original owner. Do not let a
			// parent recursively consume it or exhaust an unreserved event sequence.
			const auto *child_pipeline = render_pipeline_owner.owns(child) ? render_pipeline_owner.get_or_null(child) : nullptr;
			const auto *child_framebuffer = framebuffer_owner.owns(child) ? framebuffer_owner.get_or_null(child) : nullptr;
			if ((child_pipeline && child_pipeline->qualification_return) || (child_framebuffer && child_framebuffer->qualification_return)) {
				return true;
			}
			const auto *child_texture = texture_owner.owns(child) ? texture_owner.get_or_null(child) : nullptr;
			if (child_texture && child_texture->qualification_return) {
				return true;
			}
			const auto *child_sampler = sampler_owner.owns(child) ? sampler_owner.get_or_null(child) : nullptr;
			if (child_sampler && child_sampler->qualification_return) {
				return true;
			}
			if (_qualification_resource_busy(child)) {
				return true;
			}
		}
	}
	return false;
}
Error RenderingDevice::_qualification_pin_resources(QualificationTicket *ticket, const LocalVector<RID> &resources) {
	// Production callers hold Registry::mutex and the original RD lock. Release
	// relies on that shared write discipline for both sides of the reverse edge.
	uint32_t added = 0;
	for (uint32_t resource_index = 0; resource_index < resources.size(); ++resource_index) {
		RID rid = resources[resource_index];
		bool already = false;
		for (RID held : ticket->resource_pins) {
			if (held == rid) {
				already = true;
				break;
			}
		}
		auto *texture = texture_owner.owns(rid) ? texture_owner.get_or_null(rid) : nullptr;
		auto *sampler = sampler_owner.owns(rid) ? sampler_owner.get_or_null(rid) : nullptr;
		auto *set = uniform_set_owner.owns(rid) ? uniform_set_owner.get_or_null(rid) : nullptr;
		auto *raster_pipeline = render_pipeline_owner.owns(rid) ? render_pipeline_owner.get_or_null(rid) : nullptr;
		auto *framebuffer = framebuffer_owner.owns(rid) ? framebuffer_owner.get_or_null(rid) : nullptr;
		// New admissions revalidate existing texture/sampler pins and count each
		// new child only once within the incoming batch.
		const bool typed_child = texture || sampler || raster_pipeline || framebuffer || (set && set->qualification_return);
		bool duplicate = false;
		if (typed_child && !already) {
			for (uint32_t prior = 0; prior < resource_index; ++prior) {
				if (resources[prior] == rid) {
					duplicate = true;
					break;
				}
			}
		}
		if (already && !typed_child) {
			continue;
		}
		if (!already && !duplicate && ticket->resource_pins.size() + (++added) > 128) {
			return ERR_PARAMETER_RANGE_ERROR;
		}
		Buffer *buffer = storage_buffer_owner.owns(rid) ? storage_buffer_owner.get_or_null(rid) : nullptr;
		if (!buffer) {
			buffer = uniform_buffer_owner.owns(rid) ? uniform_buffer_owner.get_or_null(rid) : nullptr;
		}
		auto *pipeline = compute_pipeline_owner.owns(rid) ? compute_pipeline_owner.get_or_null(rid) : nullptr;
		auto *shader = shader_owner.owns(rid) ? shader_owner.get_or_null(rid) : nullptr;
		if ((raster_pipeline && !raster_pipeline->qualification_return) || (framebuffer && !framebuffer->qualification_return)) {
			return ERR_UNAUTHORIZED;
		}
		if (texture && !texture->qualification_return) {
			return ERR_UNAUTHORIZED;
		}
		if (sampler && !sampler->qualification_return) {
			return ERR_UNAUTHORIZED;
		}
		auto *physical = buffer ? buffer->qualification_return : pipeline ? pipeline->qualification_return
				: raster_pipeline										  ? raster_pipeline->qualification_return
				: framebuffer											  ? framebuffer->qualification_return
				: shader												  ? shader->qualification_return
				: texture												  ? texture->qualification_return
				: sampler												  ? sampler->qualification_return
				: set													  ? set->qualification_return
																		  : nullptr;
		if (typed_child) {
			// The caller holds Registry and RD locks: authenticate the pointer against
			// the original bridge child before reading it or admitting any resource.
			bool original = false;
			auto &registry = Registry::get_singleton();
			for (auto *bridge : registry.qualification_bridges) {
				if (bridge && bridge->serial == ticket->bridge_serial) {
					if (raster_pipeline || framebuffer) {
						for (auto *child : bridge->units) {
							if (child && child->raster && !child->creating && !child->retiring && !child->acknowledged && physical && physical->bridge == bridge->serial && physical->child == child->serial && physical->rid == rid.get_id() && physical->serial && ((raster_pipeline && physical == &child->pipeline && rid.get_id() == child->raster->backing.pipeline_rid && physical->serial == child->raster->backing.pipeline_serial) || (framebuffer && physical == &child->raster->framebuffer && rid.get_id() == child->raster->backing.framebuffer_rid && physical->serial == child->raster->backing.framebuffer_serial))) {
								original = true;
								break;
							}
						}
					}
					auto match = [&](const auto *child) { return child && &child->physical == physical && !child->creating && !child->retiring && !child->acknowledged && child->serial && child->rid == rid.get_id() && child->object_serial && child->object_serial == child->physical.serial && child->physical.rid == child->rid && child->physical.bridge == bridge->serial && child->physical.child == child->serial; };
					if (texture) {
						for (auto *child : bridge->textures) {
							if (match(child)) {
								original = true;
								break;
							}
						}
					}
					if (sampler) {
						for (auto *child : bridge->samplers) {
							if (match(child)) {
								original = true;
								break;
							}
						}
					}
					if (set && ticket->system_scalar && physical == &ticket->system_set_physical && ticket->system_set == rid && !ticket->system_set_physical.use_closed.load()) {
						original = true;
					}
					if (set && !original) {
						for (auto *child : bridge->sets) {
							if (match(child)) {
								original = true;
								break;
							}
						}
					}
					break;
				}
			}
			if (!original) {
				return ERR_UNAUTHORIZED;
			}
		}
		if (physical && (physical->bridge != ticket->bridge_serial || physical->use_closed.load())) {
			return ERR_UNAUTHORIZED;
		}
		uint32_t *count = buffer ? &buffer->qualification_users : set ? &set->qualification_users
				: pipeline											  ? &pipeline->qualification_users
				: texture											  ? &texture->qualification_users
				: raster_pipeline									  ? &raster_pipeline->qualification_users
				: framebuffer										  ? &framebuffer->qualification_users
				: sampler											  ? &sampler->qualification_users
																	  : nullptr;
		if (!already && !duplicate && count && *count == UINT32_MAX) {
			return ERR_PARAMETER_RANGE_ERROR;
		}
		if (!buffer && !set && !pipeline && !shader && !texture && !sampler && !raster_pipeline && !framebuffer) {
			return ERR_INVALID_DATA;
		}
	}
	for (RID rid : resources) {
		bool already = false;
		for (RID held : ticket->resource_pins) {
			if (held == rid) {
				already = true;
				break;
			}
		}
		if (already) {
			continue;
		}
		Buffer *buffer = storage_buffer_owner.owns(rid) ? storage_buffer_owner.get_or_null(rid) : nullptr;
		if (!buffer) {
			buffer = uniform_buffer_owner.owns(rid) ? uniform_buffer_owner.get_or_null(rid) : nullptr;
		}
		auto *set = uniform_set_owner.owns(rid) ? uniform_set_owner.get_or_null(rid) : nullptr;
		auto *pipeline = compute_pipeline_owner.owns(rid) ? compute_pipeline_owner.get_or_null(rid) : nullptr;
		auto *raster_pipeline = render_pipeline_owner.owns(rid) ? render_pipeline_owner.get_or_null(rid) : nullptr;
		auto *framebuffer = framebuffer_owner.owns(rid) ? framebuffer_owner.get_or_null(rid) : nullptr;
		auto *shader = shader_owner.owns(rid) ? shader_owner.get_or_null(rid) : nullptr;
		auto *texture = texture_owner.owns(rid) ? texture_owner.get_or_null(rid) : nullptr;
		auto *sampler = sampler_owner.owns(rid) ? sampler_owner.get_or_null(rid) : nullptr;
		uint32_t *count = buffer ? &buffer->qualification_users : set ? &set->qualification_users
				: pipeline											  ? &pipeline->qualification_users
				: texture											  ? &texture->qualification_users
				: raster_pipeline									  ? &raster_pipeline->qualification_users
				: framebuffer										  ? &framebuffer->qualification_users
				: sampler											  ? &sampler->qualification_users
																	  : nullptr;
		if (count) {
			++*count;
		}
		ticket->resource_pins.push_back(rid);
		auto *physical = buffer ? buffer->qualification_return : pipeline ? pipeline->qualification_return
				: raster_pipeline										  ? raster_pipeline->qualification_return
				: framebuffer											  ? framebuffer->qualification_return
				: shader												  ? shader->qualification_return
				: texture												  ? texture->qualification_return
				: sampler												  ? sampler->qualification_return
				: set													  ? set->qualification_return
																		  : nullptr;
		if (physical) {
			bool known = false;
			for (auto *used : ticket->physical_uses) {
				if (used == physical) {
					known = true;
					break;
				}
			}
			if (!known) {
				physical->uses.push_back(ticket);
				ticket->physical_uses.push_back(physical);
			}
		}
	}
	ticket->resources_returned = false;
	ticket->physically_retired.store(false);
	return OK;
}
void RenderingDevice::_qualification_unpin_resources(QualificationTicket *ticket) {
	if (ticket->resources_returned) {
		return;
	}
	for (RID rid : ticket->resource_pins) {
		Buffer *buffer = storage_buffer_owner.owns(rid) ? storage_buffer_owner.get_or_null(rid) : nullptr;
		if (!buffer) {
			buffer = uniform_buffer_owner.owns(rid) ? uniform_buffer_owner.get_or_null(rid) : nullptr;
		}
		auto *set = uniform_set_owner.owns(rid) ? uniform_set_owner.get_or_null(rid) : nullptr;
		auto *pipeline = compute_pipeline_owner.owns(rid) ? compute_pipeline_owner.get_or_null(rid) : nullptr;
		auto *raster_pipeline = render_pipeline_owner.owns(rid) ? render_pipeline_owner.get_or_null(rid) : nullptr;
		auto *framebuffer = framebuffer_owner.owns(rid) ? framebuffer_owner.get_or_null(rid) : nullptr;
		auto *texture = texture_owner.owns(rid) ? texture_owner.get_or_null(rid) : nullptr;
		auto *sampler = sampler_owner.owns(rid) ? sampler_owner.get_or_null(rid) : nullptr;
		uint32_t *count = buffer ? &buffer->qualification_users : set ? &set->qualification_users
				: pipeline											  ? &pipeline->qualification_users
				: texture											  ? &texture->qualification_users
				: raster_pipeline									  ? &raster_pipeline->qualification_users
				: framebuffer										  ? &framebuffer->qualification_users
				: sampler											  ? &sampler->qualification_users
																	  : nullptr;
		if (count) {
			CRASH_COND(!*count);
			--*count;
		}
	}
	// A prepared zero-effect ticket can admit again. Keep only live count pins
	// in this set; physical reverse references remain until ticket acknowledgment.
	ticket->resource_pins.clear();
	ticket->resources_returned = true;
}
namespace {
bool qualification_knows_serial(const QualificationTicket &ticket,uint64_t serial) {
 for(uint64_t existing:ticket.admitted_native_serials)if(existing==serial)return true;
 for(const auto &member:ticket.natives)if(member.source.serial==serial)return true;
 return false;
}
}
Error RenderingDevice::_qualification_admit_native_serials(const uint64_t *serials,uint32_t count,uint32_t worker,bool p_copy) {
 if(frames.is_empty() || uint32_t(frame)>=frames.size() || frames[frame].qualification_tickets.is_empty())return OK;
 auto &registry=Registry::get_singleton();Registry::Lock lock(registry,Registry::Lock::TRY_OR_BORROW);if(!lock.is_locked())return ERR_BUSY;
 auto needed=[&](QualificationTicket *ticket){uint64_t n=0;
  for(uint32_t i=0;i<count;++i){if(qualification_knows_serial(*ticket,serials[i]))continue;bool duplicate=false;for(uint32_t j=0;j<i;++j)duplicate=duplicate||serials[j]==serials[i];if(!duplicate)++n;}return n;};
 for(uint32_t i=0;i<count;++i)if(!serials[i])return ERR_INVALID_DATA;
 // The entire native batch and all affected tickets are reserved atomically.
 // No later callback in this batch can refuse after a wait/work prefix issued.
 for(auto *ticket:frames[frame].qualification_tickets){
  if(!ticket->credits.capacity)continue;const uint64_t n=needed(ticket);
  if(p_copy){if(n>ticket->credits.copy)return ERR_PARAMETER_RANGE_ERROR;}
  else if(worker==UINT32_MAX){if(n>ticket->credits.main)return ERR_PARAMETER_RANGE_ERROR;}
  else {uint64_t remaining=0;for(const auto &quote:ticket->worker_credits)if(quote.worker==worker && quote.frame==uint32_t(frame) && quote.epoch==frames[frame].qualification_epoch)remaining+=quote.remaining;if(n>remaining)return ERR_PARAMETER_RANGE_ERROR;}
 }
 for(auto *ticket:frames[frame].qualification_tickets){
  if(!ticket->credits.capacity)continue;
  for(uint32_t i=0;i<count;++i){const uint64_t serial=serials[i];if(qualification_knows_serial(*ticket,serial))continue;
   if(!p_copy && worker!=UINT32_MAX)for(auto &quote:ticket->worker_credits)if(quote.worker==worker && quote.frame==uint32_t(frame) && quote.epoch==frames[frame].qualification_epoch && quote.remaining){--quote.remaining;break;}
   const Error admitted=ticket->credits.admit_native(1,!p_copy && worker!=UINT32_MAX,p_copy);CRASH_COND(admitted!=OK);ticket->admitted_native_serials.push_back(serial);
  }
 }
 return OK;
}
bool RenderingDevice::QualificationSourceAdmissionScope::can_block(void *p_owner) {
 auto &self=*static_cast<QualificationSourceAdmissionScope *>(p_owner);
 // A Metal device-loss handler may reenter the original Registry. Never wait
 // for that callback while this thread owns Registry or a bounded drain slice.
 return !Registry::Lock::owns(Registry::get_singleton()) && !self.rd.native_disposal_budget;
}
bool RenderingDevice::_qualification_capture_hidden_source(const RDD::QueueCallReport &report) {
 if(report.calls.is_empty())return false;
 if(frames.is_empty() || uint32_t(frame)>=frames.size())return false;
 // The original RD lock is held by resource creation/end-frame. Preserve the
 // real original CompletionRecord observer in the existing Frame report list.
 auto &registry=Registry::get_singleton();
 Registry::Lock lock(registry,Registry::Lock::BLOCK_OR_BORROW);
 frames[frame].qualification_hidden_reports.push_back(report);
 registry.production_capture_issue_locked(*this,UINT32_MAX,{report},report.error);
 _qualification_issue_tickets_locked(UINT32_MAX,report,report.error);
 return true;
}
void RenderingDevice::_qualification_cancel_native_serials(const uint64_t *serials,uint32_t count,uint32_t worker,bool p_copy) {
 if(frames.is_empty() || uint32_t(frame)>=frames.size())return;
 auto &registry=Registry::get_singleton();Registry::Lock lock(registry,Registry::Lock::BLOCK_OR_BORROW);
 // Only fresh serials which this exact backend batch never registered can
 // return a reservation. No receipt fact or completion member is erased.
 for(auto *ticket:frames[frame].qualification_tickets){if(!ticket->credits.capacity)continue;
  for(uint32_t i=0;i<count;++i){
   bool committed=false;for(const auto &member:ticket->natives)committed=committed||member.source.serial==serials[i];CRASH_COND(committed);
   for(uint32_t j=0;j<ticket->admitted_native_serials.size();++j)if(ticket->admitted_native_serials[j]==serials[i]){
    ticket->admitted_native_serials.remove_at(j);
    const Error canceled=ticket->credits.cancel_native(1,!p_copy && worker!=UINT32_MAX,p_copy);CRASH_COND(canceled!=OK);
    if(!p_copy && worker!=UINT32_MAX){bool found=false;for(auto &quote:ticket->worker_credits)if(quote.worker==worker && quote.frame==uint32_t(frame) && quote.epoch==frames[frame].qualification_epoch){++quote.remaining;found=true;break;}CRASH_COND(!found);}
    break;
   }
  }
 }
}
Error RenderingDevice::_qualification_admit_worker(uint32_t worker,uint64_t operation,const RDD::QueueCallReport &prefix,uint32_t future) {
 auto &registry=Registry::get_singleton();Registry::Lock lock(registry,Registry::Lock::TRY_OR_BORROW);if(!lock.is_locked())return ERR_BUSY;
 for(const auto &source:prefix.calls)if(!source.serial || !source.queue_index || !source.completion_serial || source.completion==RDD::CompletionCapability::Disabled || source.presentation_only)return ERR_INVALID_DATA;
 auto quote_for=[&](QualificationTicket *ticket)->QualificationTicket::WorkerCredit* {
  for(auto &quote:ticket->worker_credits)if(quote.worker==worker && quote.operation==operation && quote.frame==uint32_t(frame) && quote.epoch==frames[frame].qualification_epoch)return &quote;
  return nullptr;
 };
 auto needed=[&](QualificationTicket *ticket)->uint64_t {
  uint64_t count=future;
  for(uint32_t i=0;i<prefix.calls.size();++i){const auto &source=prefix.calls[i];bool duplicate=false;for(uint32_t j=0;j<i;++j)duplicate=duplicate||prefix.calls[j].serial==source.serial;if(!duplicate && !qualification_knows_serial(*ticket,source.serial))++count;}
  return count;
 };
 for(auto *ticket:frames[frame].qualification_tickets){
  if(!ticket->credits.capacity || quote_for(ticket))continue;
  const uint64_t count=needed(ticket);
  if(count>UINT32_MAX || count>ticket->credits.capacity-ticket->credits.occupied())return ERR_PARAMETER_RANGE_ERROR;
 }
 for(auto *ticket:frames[frame].qualification_tickets){
  if(!ticket->credits.capacity || quote_for(ticket))continue;
  const Error reserved=ticket->credits.reserve_workers(uint32_t(needed(ticket)));CRASH_COND(reserved!=OK);
  ticket->worker_credits.push_back({worker,uint32_t(frame),future,operation,frames[frame].qualification_epoch});
  for(const auto &source:prefix.calls)if(!qualification_knows_serial(*ticket,source.serial)){
   const Error admitted=ticket->credits.admit_native(1,true);CRASH_COND(admitted!=OK);
   ticket->admitted_native_serials.push_back(source.serial);
  }
 }
 return OK;
}
Error RenderingDevice::_qualification_activate_cleanup_credits() {
 if(frames.is_empty() || uint32_t(frame)>=frames.size())return OK;
 auto &registry=Registry::get_singleton();Registry::Lock lock(registry,Registry::Lock::TRY_OR_BORROW);
 if(!lock.is_locked())return ERR_BUSY;
 for(auto *ticket:frames[frame].qualification_tickets){
  if(!ticket->admission_closed && !ticket->append_failed && !ticket->mapping_failed)continue;
  const uint32_t main=MIN(ticket->credits.cleanup,3u);
  ticket->credits.main+=main;ticket->credits.copy+=ticket->credits.cleanup-main;
  ticket->credits.cleanup=0;
 }
 return OK;
}
Error RenderingDevice::_qualification_admit_extra_main(uint32_t slots) {
 auto &registry=Registry::get_singleton();Registry::Lock lock(registry,Registry::Lock::BLOCK_OR_BORROW);
 const uint64_t count=uint64_t(slots)*3;
 for(auto *ticket:frames[frame].qualification_tickets)if(ticket->credits.capacity && count>ticket->credits.capacity-ticket->credits.occupied())return ERR_PARAMETER_RANGE_ERROR;
 for(auto *ticket:frames[frame].qualification_tickets)if(ticket->credits.capacity){const Error reserved=ticket->credits.grow_plan(0,count);CRASH_COND(reserved!=OK);}
 return OK;
}
Error RenderingDevice::_qualification_admit_frame_ticket(QualificationTicket *ticket) {
 if(!ticket->credits.capacity)return OK;
 auto &registry=Registry::get_singleton();
 const bool caller_owned = Registry::Lock::owns(registry);
 Registry::Lock ticket_lock(registry,Registry::Lock::TRY_OR_BORROW);
 if(!ticket_lock.is_locked())return ERR_BUSY;
 for(auto *existing:frames[frame].qualification_tickets)if(existing==ticket)return OK;
 if(ticket->acknowledged || ticket->admission_closed)return ERR_BUSY;
 // The original recording pin blocks ACK/destruction while Registry is dropped.
 // Keep RD, then acquire pool/workers, and only then reacquire Registry.
 const bool was_recording = ticket->recording;
 ticket->recording = true;
 if(caller_owned)registry.unlock_mutex_for_call();else ticket_lock.temp_unlock();
 struct RestoreTicket {
  Registry &registry;Registry::Lock &lock;QualificationTicket *ticket;RenderingDevice &device;
  bool caller_owned,was_recording,relocked=false;
  void relock(){if(relocked)return;if(caller_owned)registry.relock_mutex_after_call();else lock.temp_relock();relocked=true;}
  ~RestoreTicket(){
   relock();ticket->recording=was_recording;
   // Cancellation may have observed the temporary original recording pin.
   // Publish its real no-effect return once that pin is removed under Registry.
   if(ticket->admission_closed && !ticket->recording)registry.qualification_ticket_publish(ticket,&device);
  }
 } restore{registry,ticket_lock,ticket,*this,caller_owned,was_recording};
 // Publishing a new graph membership also adopts every already-issued hidden
 // prefix and each currently required operation. Acquire all original worker
 // locks before quoting; zero-effect refusal leaves the ticket unenrolled.
 struct Locks {
  LocalVector<BinaryMutex*> held;
  bool take(BinaryMutex &mutex){if(!mutex.try_lock())return false;held.push_back(&mutex);return true;}
  ~Locks(){for(uint32_t i=held.size();i;--i)held[i-1]->unlock();}
 } locks;
 if(!locks.take(transfer_worker_pool_mutex))return ERR_BUSY;
 for(uint32_t i=0;i<transfer_worker_pool_size;++i){
  auto *worker=transfer_worker_pool[i];
  if(!locks.take(worker->thread_mutex) || !locks.take(worker->operations_mutex))return ERR_BUSY;
 }
 restore.relock();
 if(ticket->acknowledged || ticket->admission_closed)return ERR_BUSY;
 for(auto *existing:frames[frame].qualification_tickets)if(existing==ticket)return OK;
 struct Quote { TransferWorker *worker;RDD::QueueCallReport prefix;uint32_t future; };
 LocalVector<Quote> quotes;LocalVector<uint64_t> serials;uint64_t needed=0;
 auto collect=[&](const RDD::QueueCallReport &report)->Error {
  for(const auto &source:report.calls){
   if(!source.serial || !source.queue_index || !source.completion_serial || source.completion==RDD::CompletionCapability::Disabled || source.presentation_only)return ERR_INVALID_DATA;
   if(qualification_knows_serial(*ticket,source.serial))continue;
   bool known=false;for(uint64_t serial:serials)if(serial==source.serial){known=true;break;}
   if(!known){serials.push_back(source.serial);++needed;}
  }
  return OK;
 };
 for(const auto &report:frames[frame].qualification_hidden_reports){const Error error=collect(report);if(error!=OK)return error;}
 for(uint32_t i=0;i<transfer_worker_pool_size;++i){
  auto *worker=transfer_worker_pool[i];
  if(worker->operations_processed>=transfer_worker_operation_used_by_draw[i])continue;
  Quote quote{worker,{},0};
  if(worker->recording || worker->submission_pending){const Error error=driver->qualification_worker_source_quote(worker->command_buffer,quote.prefix,quote.future);if(error!=OK)return error;}
  else quote.prefix=worker->last_submission;
  const Error error=collect(quote.prefix);if(error!=OK)return error;
  needed+=quote.future;quotes.push_back(std::move(quote));
 }
 if(needed>UINT32_MAX || needed>ticket->credits.capacity-ticket->credits.occupied())return ERR_PARAMETER_RANGE_ERROR;
 const Error reserved=ticket->credits.reserve_workers(uint32_t(needed));CRASH_COND(reserved!=OK);
 for(uint64_t serial:serials){const Error admitted=ticket->credits.admit_native(1,true);CRASH_COND(admitted!=OK);ticket->admitted_native_serials.push_back(serial);}
 for(const auto &quote:quotes){
  ticket->worker_credits.push_back({quote.worker->index,uint32_t(frame),quote.future,quote.worker->operations_counter,frames[frame].qualification_epoch});
  quote.worker->qualification_batch_sealed=true;
 }
 frames[frame].qualification_tickets.push_back(ticket);++ticket->frame_refs;
 return OK;
}

void RenderingDevice::_qualification_add_frame_ticket(QualificationTicket *ticket) {
	// Caller holds the original Registry mutex while publishing graph membership.
	bool present = false;
	for (auto *existing : frames[frame].qualification_tickets) {
		if (existing == ticket) {
			present = true;
			break;
		}
	}
	if (!present) {
  CRASH_COND(ticket->credits.capacity); // exact admission must precede the graph effect
		frames[frame].qualification_tickets.push_back(ticket);
		++ticket->frame_refs;
	}
	for (const auto &report : frames[frame].qualification_hidden_reports) {
		_qualification_issue_tickets_locked(UINT32_MAX, report, report.error);
	}
}
void RenderingDevice::_qualification_seal_tickets() {
	{
		auto &registry = Registry::get_singleton();
		GDExtensionDeviceExit::Registry::Lock lock(registry);
		registry.production_seal_graphs_locked(*this);
	}
	if (frames[frame].qualification_tickets.is_empty()) {
		return;
	}
	auto &registry = Registry::get_singleton();
	GDExtensionDeviceExit::Registry::Lock lock(registry);
	for (auto *ticket : frames[frame].qualification_tickets) {
		for (auto &member : ticket->graphs) {
			if (member.frame != uint32_t(frame) || member.epoch != frames[frame].qualification_epoch || member.sealed) {
				continue;
			}
			auto &fact = ticket->facts[member.fact];
			uint32_t slot = UINT32_MAX;
			if (draw_graph.record_mapping_state(member.handle) != RDG::RecordMappingState::Recorded || !draw_graph.resolve_record_command_buffer_slot(member.handle, slot)) {
				ticket->mapping_failed = true;
				fact.error = FAILED;
				fact.completion = 4;
				ticket->observation_dirty = true;
				continue;
			}
			member.sealed = true;
			fact.command_slot = slot;
			fact.phase = 2;
			ticket->observation_dirty = true;
		}
		if (ticket->system_scalar && ticket->body.is_valid() && draw_graph.contains_record(ticket->body)) {
			ticket->ordering_closed = true;
		} else if (ticket->capture.is_valid() && ticket->body.is_valid() && draw_graph.contains_record(ticket->capture) && draw_graph.contains_record(ticket->body)) {
			ticket->ordering_path_length = 0;
			uint32_t path_length = 0;
			ticket->ordering_closed = draw_graph.has_dependency_path(ticket->capture, ticket->body, path_length);
			if (ticket->ordering_closed) {
				ticket->ordering_path_length = path_length;
			}
			if (ticket->prepared) {
				for (auto &graph : ticket->graphs) {
					if (ticket->facts[graph.fact].semantic_kind == 6) {
						if (draw_graph.contains_record(graph.handle)) {
							uint32_t source_path_length = 0;
							const bool source_path = draw_graph.has_dependency_path(graph.handle, ticket->capture, source_path_length);
							graph.path_length = source_path ? source_path_length : 0;
							ticket->ordering_closed = ticket->ordering_closed && source_path;
						} else {
							graph.path_length = 0;
							ticket->ordering_closed = ticket->ordering_closed && graph.returned && ticket->facts[graph.fact].completion == 2;
						}
					}
				}
			}
			if (!ticket->ordering_closed) {
				ticket->mapping_failed = true;
			}
		}
		// Each actual result copy belongs after this request's Body. Same-frame
		// recording order alone is insufficient: require the graph dependency path.
		for (auto &graph : ticket->graphs) {
			if ((ticket->multi_readback || ticket->system_scalar) && ticket->facts[graph.fact].semantic_kind == 3 && draw_graph.contains_record(graph.handle)) {
				uint32_t path_length = 0;
				const bool current_body = draw_graph.contains_record(ticket->body);
				const bool ordered = current_body ? draw_graph.has_dependency_path(ticket->body, graph.handle, path_length) : ticket->body_returned_successfully();
				graph.path_length = current_body && ordered ? path_length : 0;
				ticket->ordering_closed = ticket->ordering_closed && ordered;
				if (!ordered) {
					ticket->mapping_failed = true;
				}
			}
		}
		registry.qualification_ticket_publish(ticket, this);
	}
}
void RenderingDevice::_qualification_issue_tickets(uint32_t slot, const RDD::QueueCallReport &report, Error error) {
	{
		auto &registry = Registry::get_singleton();
		GDExtensionDeviceExit::Registry::Lock lock(registry);
		registry.production_capture_issue_locked(*this, slot, { report }, error);
	}
	if (frames[frame].qualification_tickets.is_empty()) {
		return;
	}
	auto &registry = Registry::get_singleton();
	GDExtensionDeviceExit::Registry::Lock lock(registry);
	_qualification_issue_tickets_locked(slot, report, error);
}
void RenderingDevice::_qualification_issue_tickets_locked(uint32_t slot, const RDD::QueueCallReport &report, Error error) {
	auto &registry = Registry::get_singleton();
	for (auto *ticket : frames[frame].qualification_tickets) {
		uint32_t parent = UINT32_MAX;
		for (const auto &graph : ticket->graphs) {
			if (graph.frame == uint32_t(frame) && graph.epoch == frames[frame].qualification_epoch && (slot == UINT32_MAX || ticket->facts[graph.fact].command_slot == slot)) {
				parent = graph.fact;
				break;
			}
		}
		// An already-admitted hidden source can precede this ticket's first
		// graph. Keep its real native receipt as a permanent no-graph root;
		// it declares no graph dependency and cannot satisfy main-slot mapping.
		// Existing graph-backed and conservative receipts retain their policy.
		if (parent == UINT32_MAX && !(ticket->credits.capacity && slot == UINT32_MAX)) {
			continue;
		}
		for (const auto &source : report.calls) {
			if (source.presentation_only) {
				ticket->mapping_failed = true;
				ticket->observation_dirty = true;
				continue;
			}
			bool known = false;
			for (const auto &member : ticket->natives) {
				if (member.source.serial == source.serial && source.serial) {
					known = true;
					break;
				}
			}
			if (known) {
				continue;
			}
			if (ticket->facts.size() >= QualificationTicket::FACT_CAPACITY || !source.queue_index || !source.serial || !source.completion_serial || source.completion == RDD::CompletionCapability::Disabled) {
				ticket->mapping_failed = true;
				continue;
			}
   if(ticket->credits.capacity){
    if(!qualification_knows_serial(*ticket,source.serial) || ticket->credits.commit_native(1)!=OK){ticket->mapping_failed=true;continue;}
   }
			GDExtensionQualificationFactV1 fact = {};
			fact.backend_queue = source.queue_index;
			fact.semantic_kind = slot == UINT32_MAX ? 4 : 5;
			fact.command_slot = slot;
			fact.parent_fact = parent;
			fact.phase = 3;
			fact.issue = uint32_t(source.issue);
			fact.completion = 1;
			fact.error = source.error;
			fact.native_serial = source.serial;
			fact.completion_serial = source.completion_serial;
			fact.native_code = source.native_code;
			const uint32_t index = ticket->facts.size();
			ticket->facts.push_back(fact);
			ticket->observation_dirty = true;
			ticket->natives.push_back({ source, index, uint32_t(frame), slot, frames[frame].qualification_epoch, false, false });
		}
		if ((error != OK && error != ERR_BUSY) || report.error != OK || report.calls.is_empty()) {
			ticket->append_failed = true;
			if (ticket->report.native_error == OK) {
				ticket->report.native_error = error != OK ? error : report.error != OK ? report.error
																					   : FAILED;
				ticket->report.native_code = report.native_code;
			}
			if (!report.issued) {
				ticket->mapping_failed = true;
			}
		}
		registry.qualification_ticket_publish(ticket, this);
	}
}
Error RenderingDevice::_qualification_retain_tickets(uint32_t index) {
	const Error production = _production_retain_frame_sources(index);
	if (production != OK) {
		return production;
	}
	auto &registry = Registry::get_singleton();
	GDExtensionDeviceExit::Registry::Lock lock(registry);
	for (auto *ticket : frames[index].qualification_tickets) {
		for (auto &member : ticket->natives) {
			if (member.retained || member.owner_released) {
				continue;
			}
			const Error error = retain_native_source(*driver, member);
			if (error != OK) {
				return error;
			}
		}
	}
	return OK;
}
Error RenderingDevice::_qualification_poll_tickets(uint32_t index) {
	const Error production = _production_retain_frame_sources(index);
	if (production != OK) {
		return production;
	}
	if (frames[index].qualification_tickets.is_empty()) {
		return OK;
	}
	auto &registry = Registry::get_singleton();
	for (auto *ticket : frames[index].qualification_tickets) {
		for (auto &member : ticket->natives) {
			if (member.owner_released) {
				continue;
			}
			const auto completion = poll_native_source(*driver, member, ticket->source_origin);
			if (completion.error == ERR_BUSY) {
				return ERR_BUSY;
			}
			GDExtensionDeviceExit::Registry::Lock lock(registry);
			auto &fact = ticket->facts[member.fact];
			const auto old = fact;
			if (completion.error != OK) {
				fact.issue = uint32_t(member.source.issue);
				if (ticket->report.native_error == OK) {
					ticket->report.native_error = completion.error;
					ticket->report.native_code = completion.native_code;
				}
				fact.completion = completion.state == GDExtensionDeviceExit::NativeSourceObservation::State::Unknown ? 4 : 3;
				fact.error = completion.error;
				fact.native_code = completion.native_code;
				ticket->append_failed = true;
				ticket->observation_dirty = true;
				if (!member.retained) {
					ticket->mapping_failed = true;
				}
				registry.qualification_ticket_publish(ticket, this);
				return completion.error;
			}
			if (completion.state == GDExtensionDeviceExit::NativeSourceObservation::State::Complete) {
				fact.issue = uint32_t(completion.resolved_issue);
				fact.completion = 2;
				fact.native_code = completion.native_code;
				fact.phase = 4;
			} else if (completion.state == GDExtensionDeviceExit::NativeSourceObservation::State::Unknown) {
				fact.issue = uint32_t(member.source.issue);
				fact.completion = 4;
				fact.native_code = completion.native_code;
				ticket->mapping_failed = true;
			}
			if (memcmp(&old, &fact, sizeof(fact))) {
				ticket->observation_dirty = true;
			}
			registry.qualification_ticket_publish(ticket, this);
			if (completion.state != GDExtensionDeviceExit::NativeSourceObservation::State::Complete) {
				return ERR_BUSY;
			}
		}
	}
	return OK;
}
void RenderingDevice::_qualification_return_frame_tickets(uint32_t index) {
	if (frames[index].qualification_tickets.is_empty()) {
		return;
	}
	auto &registry = Registry::get_singleton();
	GDExtensionDeviceExit::Registry::Lock lock(registry);
	for (auto *ticket : frames[index].qualification_tickets) {
		for (auto &member : ticket->graphs) {
			if (member.frame == index && member.epoch == frames[index].qualification_epoch) {
				member.returned = true;
			}
		}
		CRASH_COND(!ticket->frame_refs);
		--ticket->frame_refs;
		registry.qualification_ticket_publish(ticket, this);
	}
	frames[index].qualification_tickets.clear();
}
