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
	const Buffer *buffer = storage_buffer_owner.get_or_null(rid);
	if (!buffer) {
		buffer = uniform_buffer_owner.get_or_null(rid);
	}
	if (buffer && buffer->qualification_users) {
		return true;
	}
	const auto *set = uniform_set_owner.get_or_null(rid);
	if (set && set->qualification_users) {
		return true;
	}
	const auto *pipeline = compute_pipeline_owner.get_or_null(rid);
	if (pipeline && pipeline->qualification_users) {
		return true;
	}
	const auto *dependencies = dependency_map.getptr(rid);
	if (dependencies) {
		for (RID child : *dependencies) {
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
		if (ticket->resource_pins.size() + (++added) > 128) {
			return ERR_PARAMETER_RANGE_ERROR;
		}
		Buffer *buffer = storage_buffer_owner.get_or_null(rid);
		if (!buffer) {
			buffer = uniform_buffer_owner.get_or_null(rid);
		}
		auto *set = uniform_set_owner.get_or_null(rid);
		auto *pipeline = compute_pipeline_owner.get_or_null(rid);
		auto *shader = shader_owner.get_or_null(rid);
		auto *physical = buffer ? buffer->qualification_return : pipeline ? pipeline->qualification_return
				: shader												  ? shader->qualification_return
																		  : nullptr;
		if (physical && (physical->bridge != ticket->bridge_serial || physical->use_closed.load())) {
			return ERR_UNAUTHORIZED;
		}
		uint32_t *count = buffer ? &buffer->qualification_users : set ? &set->qualification_users
				: pipeline											  ? &pipeline->qualification_users
																	  : nullptr;
		if (count && *count == UINT32_MAX) {
			return ERR_PARAMETER_RANGE_ERROR;
		}
		if (!buffer && !set && !pipeline && !shader) {
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
		Buffer *buffer = storage_buffer_owner.get_or_null(rid);
		if (!buffer) {
			buffer = uniform_buffer_owner.get_or_null(rid);
		}
		auto *set = uniform_set_owner.get_or_null(rid);
		auto *pipeline = compute_pipeline_owner.get_or_null(rid);
		auto *shader = shader_owner.get_or_null(rid);
		uint32_t *count = buffer ? &buffer->qualification_users : set ? &set->qualification_users
				: pipeline											  ? &pipeline->qualification_users
																	  : nullptr;
		if (count) {
			++*count;
		}
		ticket->resource_pins.push_back(rid);
		auto *physical = buffer ? buffer->qualification_return : pipeline ? pipeline->qualification_return
				: shader												  ? shader->qualification_return
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
		Buffer *buffer = storage_buffer_owner.get_or_null(rid);
		if (!buffer) {
			buffer = uniform_buffer_owner.get_or_null(rid);
		}
		auto *set = uniform_set_owner.get_or_null(rid);
		auto *pipeline = compute_pipeline_owner.get_or_null(rid);
		uint32_t *count = buffer ? &buffer->qualification_users : set ? &set->qualification_users
				: pipeline											  ? &pipeline->qualification_users
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
		frames[frame].qualification_tickets.push_back(ticket);
		++ticket->frame_refs;
	}
	for (const auto &report : frames[frame].qualification_hidden_reports) {
		_qualification_issue_tickets_locked(UINT32_MAX, report, report.error);
	}
}
void RenderingDevice::_qualification_seal_tickets() {
	if (frames[frame].qualification_tickets.is_empty()) {
		return;
	}
	auto &registry = Registry::get_singleton();
	MutexLock lock(registry.mutex);
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
		if (ticket->capture.is_valid() && ticket->body.is_valid() && draw_graph.contains_record(ticket->capture) && draw_graph.contains_record(ticket->body)) {
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
		registry.qualification_ticket_publish(ticket, this);
	}
}
void RenderingDevice::_qualification_issue_tickets(uint32_t slot, const RDD::QueueCallReport &report, Error error) {
	if (frames[frame].qualification_tickets.is_empty()) {
		return;
	}
	auto &registry = Registry::get_singleton();
	MutexLock lock(registry.mutex);
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
		if (parent == UINT32_MAX) {
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
			if (ticket->facts.size() >= QualificationTicket::FACT_CAPACITY || !source.queue_index || !source.serial || !source.completion_serial || source.completion == RDD::CompletionCapability::NoCompletion) {
				ticket->mapping_failed = true;
				continue;
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
			ticket->report.native_error = error != OK ? error : report.error != OK ? report.error
																				   : FAILED;
			if (!report.issued) {
				ticket->mapping_failed = true;
			}
		}
		registry.qualification_ticket_publish(ticket, this);
	}
}
Error RenderingDevice::_qualification_retain_tickets(uint32_t index) {
	auto &registry = Registry::get_singleton();
	MutexLock lock(registry.mutex);
	for (auto *ticket : frames[index].qualification_tickets) {
		for (auto &member : ticket->natives) {
			if (member.retained || member.owner_released) {
				continue;
			}
			const Error error = driver->native_call_retain(member.source.serial);
			if (error != OK) {
				return error;
			}
			member.retained = true;
		}
	}
	return OK;
}
Error RenderingDevice::_qualification_poll_tickets(uint32_t index) {
	if (frames[index].qualification_tickets.is_empty()) {
		return OK;
	}
	auto &registry = Registry::get_singleton();
	for (auto *ticket : frames[index].qualification_tickets) {
		for (auto &member : ticket->natives) {
			if (member.owner_released) {
				continue;
			}
			if (!member.retained) {
				const Error error = driver->native_call_retain(member.source.serial);
				if (error == ERR_BUSY) {
					return ERR_BUSY;
				}
				if (error != OK) {
					MutexLock lock(registry.mutex);
					ticket->mapping_failed = true;
					ticket->report.native_error = error;
					registry.qualification_ticket_publish(ticket, this);
					return error;
				}
				member.retained = true;
			}
			const auto completion = driver->native_call_poll(member.source.serial);
			MutexLock lock(registry.mutex);
			auto &fact = ticket->facts[member.fact];
			const auto old = fact;
			if (completion.native_error) {
				fact.issue = uint32_t(member.source.issue);
				ticket->report.native_error = FAILED;
				ticket->report.native_code = completion.native_code;
				fact.completion = member.source.issue == RDD::IssueFact::IssuedUnknown ? 4 : 3;
				fact.error = FAILED;
				fact.native_code = completion.native_code;
				ticket->append_failed = true;
				ticket->observation_dirty = true;
				registry.qualification_ticket_publish(ticket, this);
				return FAILED;
			}
			if (completion.matches(member.source.completion_serial)) {
				fact.issue = 1;
				fact.completion = 2;
				fact.native_code = completion.native_code;
				fact.phase = 4;
			}
			if (memcmp(&old, &fact, sizeof(fact))) {
				ticket->observation_dirty = true;
			}
			registry.qualification_ticket_publish(ticket, this);
			if (!completion.matches(member.source.completion_serial)) {
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
	MutexLock lock(registry.mutex);
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
