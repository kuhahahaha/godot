/**************************************************************************/
/*  rendering_device_output_retire.cpp                                    */
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

#include "core/extension/gdextension_library_exit_guard.h"
#include "core/extension/gdextension_production_request.h"
#include "core/os/os.h"
#include "servers/rendering/rendering_device.h"

// Existing original OBSERVE/capture boundary only; zero group is a pure source
// closure preflight. A copied budget, arbitrary RID or metadata bool never joins.
Error RenderingDevice::_production_output_retire_service(GDExtensionDeviceExit::ProductionOutputPin &pin, uint64_t serial,
		const GDExtensionProductionServiceBudgetV1 &budget, GDExtensionDeviceExit::ProductionServiceUsage &usage,
		GDExtensionDeviceExit::ProductionDisposalObservation &observation) {
	using namespace GDExtensionDeviceExit;
	observation = {};
	if (!pin.registry) {
		return ERR_INVALID_PARAMETER;
	}
	Registry &registry = *pin.registry;
	MutexLock lock(registry.mutex);
	if (!registry.production_output_pin_matches_locked(pin, { registry.instance, pin.request_serial }, *this)) {
		return ERR_UNAUTHORIZED;
	}
	auto &request = *pin.request;
	auto &control = request.control;
	if (control.state != ProductionControlState::EXECUTING || control.input.kind != ProductionControlKind::OBSERVE ||
			control.input.request_serial != request.serial || &budget != &control.input.budget ||
			&usage != &control.output.service_usage) {
		return ERR_UNAUTHORIZED;
	}
	if (!budget.controls || !budget.passes || !budget.receipts || usage.passes >= MIN(budget.passes, 64u) ||
			usage.receipts >= MIN(budget.receipts, 128u) || !budget.deadline_micros ||
			OS::get_singleton()->get_ticks_usec() >= budget.deadline_micros || request.source_device_lost) {
		return ERR_BUSY;
	}
	GDExtensionProductionReportV2 source = {};
	const Error projected = registry.production_report_contents_locked(request, source);
	if (projected != OK) {
		return projected;
	}
	if (request.capture_inflight != 1 || request.compute_list_open ||
			(!request.cancelled && request.next_action != request.action_count) || source.possible_obligations ||
			source.known_obligations != source.complete_obligations) {
		return ERR_BUSY;
	}
	if (!serial) {
		return OK;
	}
	ProductionUseCell *group = nullptr;
	for (uint32_t i = request.use_count + request.binding_used; i < request.capacity.resource_uses; ++i) {
		auto &cell = request.uses[i];
		if (cell.output_role == ProductionUseCell::OutputRole::DISPOSAL_GROUP &&
				cell.disposal_group_serial == serial && cell.disposal_group_pin_serial == pin.serial) {
			group = &cell;
			break;
		}
	}
	if (!group) {
		return ERR_DOES_NOT_EXIST;
	}
	// Source closure excludes future factories. Return one original unbound
	// reservation per pass; bound physical members remain exact obligations.
	for (uint32_t i = request.use_count + request.binding_used; i < request.capacity.resource_uses; ++i) {
		auto &cell = request.uses[i];
		if (cell.disposal_group_serial == serial && cell.output_role == ProductionUseCell::OutputRole::DISPOSAL_RESERVED) {
			cell.disposal.state.store(6, std::memory_order_release);
			cell.output_role = ProductionUseCell::OutputRole::DISPOSAL_MEMBER;
			++usage.passes;
			usage.controls = 1;
			return ERR_BUSY;
		}
	}
	if (!group->disposal_group_release) {
		if (!_thread_safe_.try_lock()) {
			return ERR_BUSY;
		}
		bool covered = true;
		for (uint32_t command = 0; command < draw_graph.qualification_command_count(); ++command) {
			bool captured = false;
			for (uint32_t i = 0; i < request.graph_count; ++i) {
				captured |= registry.production_output_graph_command_locked(request, *this, i, command);
			}
			covered &= captured;
		}
		if (!covered || OS::get_singleton()->get_ticks_usec() >= budget.deadline_micros) {
			_thread_safe_.unlock();
			return ERR_BUSY;
		}
		++usage.passes;
		usage.controls = 1;
		group->disposal_group_release = true;
		_thread_safe_.unlock();
		return ERR_BUSY; // Original Registry executor performs actual drop/fence work.
	}
	const Error error = _production_output_hold_returned_locked(pin, serial, observation);
	if (error != OK) {
		return error;
	}
	if (observation.unknown) {
		return ERR_INVALID_DATA;
	}
	if (!observation.settled) {
		return ERR_BUSY; // Leave this turn's budget for original native progress.
	}
	++usage.passes;
	usage.controls = 1;
	return OK;
}
