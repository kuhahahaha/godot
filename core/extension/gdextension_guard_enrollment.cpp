/**************************************************************************/
/*  gdextension_guard_enrollment.cpp                                      */
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
#include "core/object/object.h"
#include "core/os/memory.h"
#include "core/os/thread.h"

#include <cstdio>
#include <cstdlib>
#include <utility>
#ifdef WINDOWS_ENABLED
#include <io.h>
#else
#include <unistd.h>
#endif

namespace GDExtensionDeviceExit {
namespace {
template <typename T>
bool aligned_pointer(const T *p) {
	return p && reinterpret_cast<uintptr_t>(p) % alignof(T) == 0;
}
bool abi_valid(const GDExtensionDeviceExitAbi *p, uint32_t size = 16) {
	return aligned_pointer(p) && p->abi_version == 1 && p->struct_size == size && p->reserved == 0;
}
template <typename T>
bool empty_output(T *p) {
	return aligned_pointer(p) && p->registry_instance == 0 && p->handle_serial == 0;
}
} //namespace

Error Registry::check_entry_classes(const RecordRef &ref, uint64_t classes) {
	if (!ref.is_valid()) {
		return OK;
	}
	if (ref.registry != this) {
		return ERR_INVALID_DATA;
	}
	MutexLock lock(mutex);
	const auto &record = records[ref.slot];
	if (record.guard_enrollment == GuardEnrollment::UNAVAILABLE) {
		return OK;
	}
	if (record.guard_enrollment != GuardEnrollment::QUALIFIED || record.quiescing || record.close_started) {
		return ERR_UNAVAILABLE;
	}
	return (classes & ~record.guard.entry_classes) ? ERR_UNAVAILABLE : OK;
}
Error Registry::check_current_entry_classes(uint64_t classes) {
	const auto *context = RegistrationContext::current;
	return context && context->reference ? check_entry_classes(*context->reference, classes) : OK;
}

Error Registry::retain_callable_owner(void *token, RecordRef &out) {
	if (out.is_valid()) {
		return ERR_INVALID_DATA;
	}
	const auto *frame = RegistrationContext::current;
	if (!frame || !frame->reference || frame->reference->registry != this) {
		MutexLock lock(mutex);
		for (const auto &binding : binding_owners) {
			if (binding.token == token && token && (binding.record == INVALID_SLOT || records[binding.record].guard_enrollment != GuardEnrollment::UNAVAILABLE)) {
				return ERR_UNAUTHORIZED;
			}
		}
		return OK;
	}
	MutexLock lock(mutex);
	auto &record = records[frame->reference->slot];
	if (record.guard_enrollment == GuardEnrollment::UNAVAILABLE) {
		for (const auto &binding : binding_owners) {
			if (binding.token == token && token && (binding.record == INVALID_SLOT || records[binding.record].guard_enrollment != GuardEnrollment::UNAVAILABLE)) {
				return ERR_UNAUTHORIZED;
			}
		}
		return OK;
	}
	if (record.guard_enrollment != GuardEnrollment::QUALIFIED || !frame->admitted || !frame->registration_allowed ||
			record.original_owner != token || record.owner_thread != Thread::get_caller_id()) {
		return ERR_UNAUTHORIZED;
	}
	if (record.quiescing || record.close_started || !(record.guard.entry_classes & (1u << 8))) {
		return ERR_UNAVAILABLE;
	}
	if (record.private_refs >= private_limit) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	record.private_refs++;
	out = RecordRef(this, frame->reference->slot);
	return OK;
}
Error Registry::register_callable_cleanup(const RecordRef &ref, uint64_t identity, CleanupAction &action, uint64_t &serial) {
	if (authenticate_registration(ref) != OK) {
		return ERR_UNAUTHORIZED;
	}
	uint64_t issued = 0;
	{
		MutexLock lock(mutex);
		if (!action_serials.take(issued)) {
			return ERR_PARAMETER_RANGE_ERROR;
		}
	}
	// This is the original core Callable allocation identity, not a public ObjectID.
	const Error error = register_cleanup_action(ref, identity, Thread::get_caller_id(), issued, action);
	if (error == OK) {
		serial = issued;
	}
	return error;
}

void Registry::service_started(uint32_t service) {
	MutexLock lock(mutex);
	live_services |= service;
}
Error Registry::service_stopping(uint32_t service) {
	MutexLock lock(mutex);
	for (const auto &record : records) {
		if (!record.occupied || record.guard_enrollment != GuardEnrollment::QUALIFIED) {
			continue;
		}
		for (const auto &level : record.levels) {
			if ((level.initialized || level.initialize_frame) && !level.finalized &&
					((level.user_action.services | level.binding_action.services) & service)) {
				return ERR_BUSY;
			}
		}
	}
	live_services &= ~service;
	return OK;
}

Error Registry::register_guard(GDExtensionClassLibraryPtr p_library, const GuardRegistrationV1 *p_registration, GuardToken *r_token) {
	if (!aligned_pointer(p_registration) || !abi_valid(&p_registration->abi, sizeof(GuardRegistrationV1)) || !empty_output(r_token)) {
		return ERR_INVALID_DATA;
	}
	const auto &request = *p_registration;
	if (request.binding_protocol != 1 || (request.entry_classes & ~uint64_t(0xfff))) {
		return ERR_UNAVAILABLE;
	}
#ifndef TOOLS_ENABLED
	if (request.entry_classes & (uint64_t(1) << 10)) {
		return ERR_UNAVAILABLE;
	}
#endif
	const uint32_t minimum = uint32_t(request.expected_initialization.minimum_initialization_level);
	if (minimum > 3 || request.level_bitmap != (0xfu & (0xfu << minimum)) || request.binding_cleanup_bitmap != request.level_bitmap ||
			(request.user_cleanup_bitmap & ~request.level_bitmap) || !request.expected_initialization.initialize ||
			!request.expected_initialization.deinitialize || !request.user_initializer || !request.preflight ||
			(bool(request.user_cleanup_bitmap) != bool(request.user_terminator))) {
		return ERR_INVALID_DATA;
	}
	uint32_t actions = 0;
	uint32_t contexts = 1; // First control invocation plus every level wrapper.
	const bool scene_ready = request.entry_classes & GDEXTENSION_DEVICE_EXIT_ENTRY_SCENE_READY_V1;
	if (scene_ready) {
		if (!(request.level_bitmap & (1u << GDEXTENSION_INITIALIZATION_SCENE))) {
			return ERR_INVALID_DATA;
		}
		contexts++;
	}
	for (uint32_t i = 0; i < 4; i++) {
		if ((request.needs[i].user_services | request.needs[i].binding_services) & ~0xffu) {
			return ERR_UNAVAILABLE;
		}
		if (!(request.level_bitmap & (1u << i))) {
			if (request.needs[i].user_services || request.needs[i].binding_services) {
				return ERR_INVALID_DATA;
			}
		} else {
			actions += 1 + bool(request.user_cleanup_bitmap & (1u << i));
			contexts += 2;
		}
	}
	BootstrapScope *scope = BootstrapScope::current;
	RegistrationContext *context = RegistrationContext::current;
	if (!scope || scope->expected != p_library || context != &scope->context || !context->admitted || scope->reference.registry != this) {
		return ERR_UNAUTHORIZED;
	}
	MutexLock lock(mutex);
	auto &record = records[scope->reference.slot];
	if (record.original_owner != p_library || record.owner_thread != Thread::get_caller_id()) {
		return ERR_UNAUTHORIZED;
	}
	if (record.guard_enrollment != GuardEnrollment::UNAVAILABLE) {
		return ERR_ALREADY_IN_USE;
	}
	if (!record.attached || record.quiescing || record.close_started) {
		return ERR_UNAVAILABLE;
	}
	uint32_t free_views = 0;
	for (uint32_t i = 0; i < view_limit; i++) {
		free_views += views[i].serial == 0;
	}
	if (actions > action_limit || serials.exhausted || action_serials.exhausted || frame_serials.exhausted ||
			contexts > free_views || contexts > private_limit || record.private_refs > private_limit - contexts ||
			uint64_t(contexts) > UINT64_MAX - serials.next || uint64_t(contexts - 1) > UINT64_MAX - frame_serials.next ||
			uint64_t(actions - 1) > UINT64_MAX - action_serials.next) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	// Bounded descriptors reside in the original Record. All validation precedes
	// serial reservation/publication; failed attempts never reuse burned IDs.
	uint64_t token;
	serials.take(token);
	record.guard = request;
	frame_serials.take(record.reserved_control_frame);
	serials.take(record.reserved_control_view);
	for (uint32_t i = 0; i < view_limit; i++) {
		if (!views[i].serial) {
			record.control_view_slot = i;
			views[i] = { record.reserved_control_view, scope->reference.slot, ViewDomain::PREFLIGHT, record.reserved_control_frame, 0 };
			break;
		}
	}
	record.private_refs++;
	record.control_reference_reserved = true;
	if (scene_ready) {
		frame_serials.take(record.reserved_scene_ready_frame);
		serials.take(record.scene_ready_view_serial);
		for (uint32_t slot = 0; slot < view_limit; slot++) {
			if (!views[slot].serial) {
				record.scene_ready_view_slot = slot;
				views[slot] = { record.scene_ready_view_serial, scope->reference.slot, ViewDomain::SCENE_READY, record.reserved_scene_ready_frame, 0 };
				break;
			}
		}
		record.private_refs++;
	}
	for (uint32_t i = 0; i < 4; i++) {
		if (!(request.level_bitmap & (1u << i))) {
			continue;
		}
		auto reserve_context = [&](ViewDomain domain, uint64_t &frame, uint64_t &view_serial) {
			frame_serials.take(frame);
			serials.take(view_serial);
			for (uint32_t slot = 0; slot < view_limit; slot++) {
				if (!views[slot].serial) {
					views[slot] = { view_serial, scope->reference.slot, domain, frame, 0 };
					break;
				}
			}
			record.private_refs++; // Transferred to the actual LevelScope on entry.
		};
		auto &level = record.levels[i];
		reserve_context(ViewDomain::INITIALIZE, level.reserved_initialize_frame, level.initialize_view);
		reserve_context(ViewDomain::DEINITIALIZE, level.reserved_deinitialize_frame, level.deinitialize_view);
		auto issue = [&](LevelAction &action, TerminalActionKind kind, uint32_t services) {
			action_serials.take(action.serial);
			action.generation = record.snapshot.library_generation;
			action.owner_thread = record.owner_thread;
			action.level = i;
			action.kind = kind;
			action.services = services;
			action.wrapper = request.expected_initialization.deinitialize;
			action.terminator = kind == TerminalActionKind::LEVEL_USER ? request.user_terminator : nullptr;
			action.userdata = request.expected_initialization.userdata;
		};
		if (request.user_cleanup_bitmap & (1u << i)) {
			issue(record.levels[i].user_action, TerminalActionKind::LEVEL_USER, request.needs[i].user_services);
		}
		issue(record.levels[i].binding_action, TerminalActionKind::LEVEL_BINDING, request.needs[i].binding_services);
	}
	record.guard_token = token;
	record.guard_enrollment = GuardEnrollment::PENDING_BOOTSTRAP;
	*r_token = { instance, token };
	return OK;
}

void Registry::retire_scene_ready_reservation_locked(Record &record) {
	if (record.reserved_scene_ready_frame) {
		expire_frame_locked(record.reserved_scene_ready_frame);
		record.private_refs--;
		record.reserved_scene_ready_frame = 0;
	}
	record.scene_ready_view_slot = INVALID_SLOT;
	record.scene_ready_view_serial = 0;
	record.scene_ready_callback = nullptr;
	record.scene_ready_callback_v2 = nullptr;
	record.scene_ready_userdata = nullptr;
}

Error Registry::cancel_pending_scene_ready(const RecordRef &ref, CloseRoute route) {
	if (ref.registry != this || (route != CloseRoute::SETUP_ABORT && route != CloseRoute::SHUTDOWN)) {
		return ERR_INVALID_DATA;
	}
	if (RegistrationContext::current) {
		return ERR_UNAUTHORIZED;
	}
	MutexLock lock(mutex);
	auto &record = records[ref.slot];
	if (record.owner_thread != Thread::get_caller_id()) {
		return ERR_UNAUTHORIZED;
	}
	if (record.quiescing || record.close_started) {
		return ERR_BUSY;
	}
	using State = Record::SceneReadyState;
	if (record.scene_ready_state == State::CANCELLED_BEFORE_EFFECTS) {
		return record.scene_ready_cancel_route == route ? OK : ERR_INVALID_DATA;
	}
	if (record.scene_ready_state == State::IN_FRAME) {
		return ERR_BUSY;
	}
	if (record.scene_ready_state != State::PENDING) {
		return OK;
	}
	record.scene_ready_state = State::CANCELLED_BEFORE_EFFECTS;
	record.scene_ready_cancel_route = route;
	retire_scene_ready_reservation_locked(record);
	return OK;
}

Error Registry::register_scene_ready(const GDExtensionDeviceExitAbi *abi, InitializationLevelContextRef context, GDExtensionDeviceExitSceneReadyCallbackV1 callback, void *userdata) {
	if (!abi_valid(abi) || !callback) {
		return ERR_INVALID_PARAMETER;
	}
	MutexLock lock(mutex);
	View *view = nullptr;
	Error error = resolve_borrow_locked(context.registry_instance, context.handle_serial, ViewDomain::INITIALIZE, view);
	if (error != OK) {
		return error;
	}
	auto *frame = RegistrationContext::current;
	auto &record = records[view->record];
	if (frame->level != GDEXTENSION_INITIALIZATION_SCENE || frame->level_phase != LevelPhase::INITIALIZE || !frame->registration_allowed || frame->ack) {
		return ERR_UNAUTHORIZED;
	}
	if (record.guard_enrollment != GuardEnrollment::QUALIFIED || !(record.guard.entry_classes & GDEXTENSION_DEVICE_EXIT_ENTRY_SCENE_READY_V1)) {
		return ERR_UNAVAILABLE;
	}
	if (record.quiescing || record.close_started) {
		return ERR_BUSY;
	}
	if (userdata != record.original_initialization.userdata || record.scene_ready_state != Record::SceneReadyState::ABSENT) {
		return ERR_INVALID_DATA;
	}
	if (!record.reserved_scene_ready_frame || record.scene_ready_view_slot == INVALID_SLOT) {
		return ERR_UNAVAILABLE;
	}
	record.scene_ready_flavor = Record::SceneReadyFlavor::V1;
	record.scene_install_requirements = { { 1, sizeof(GDExtensionSceneInstallRequirementsV2), 0 }, 3, 0, 0 };
	record.scene_ready_callback = callback;
	record.scene_ready_userdata = userdata;
	record.scene_ready_state = Record::SceneReadyState::REGISTERED;
	return OK;
}

Error Registry::register_scene_ready_v2(const GDExtensionSceneInstallRequirementsV2 *input, InitializationLevelContextRef context, GDExtensionDeviceExitSceneReadyCallbackV2 callback, void *userdata) {
	if (!aligned_pointer(input) || !callback || !abi_valid(&input->abi, sizeof(*input))) {
		return ERR_INVALID_PARAMETER;
	}
	const auto requirements = *input;
	if (requirements.abi.abi_version != 1 || requirements.abi.struct_size != sizeof(requirements) || requirements.abi.reserved || requirements.reserved ||
			!((requirements.required_features == 1 && requirements.optional_features == 2) || (requirements.required_features == 3 && requirements.optional_features == 0))) {
		return ERR_INVALID_PARAMETER;
	}
	MutexLock lock(mutex);
	View *view = nullptr;
	Error error = resolve_borrow_locked(context.registry_instance, context.handle_serial, ViewDomain::INITIALIZE, view);
	if (error != OK) {
		return error;
	}
	auto *frame = RegistrationContext::current;
	auto &record = records[view->record];
	if (frame->level != GDEXTENSION_INITIALIZATION_SCENE || frame->level_phase != LevelPhase::INITIALIZE || !frame->registration_allowed || frame->ack) {
		return ERR_UNAUTHORIZED;
	}
	if (record.guard_enrollment != GuardEnrollment::QUALIFIED || !(record.guard.entry_classes & GDEXTENSION_DEVICE_EXIT_ENTRY_SCENE_READY_V1)) {
		return ERR_UNAVAILABLE;
	}
	if (record.quiescing || record.close_started) {
		return ERR_BUSY;
	}
	if (userdata != record.original_initialization.userdata || record.scene_ready_state != Record::SceneReadyState::ABSENT) {
		return ERR_INVALID_DATA;
	}
	if (!record.reserved_scene_ready_frame || record.scene_ready_view_slot == INVALID_SLOT) {
		return ERR_UNAVAILABLE;
	}
	// Reuse the original bit11 reservation; no additional identity or retain.
	record.scene_ready_flavor = Record::SceneReadyFlavor::V2;
	record.scene_install_requirements = requirements;
	record.scene_ready_callback_v2 = callback;
	record.scene_ready_userdata = userdata;
	record.scene_ready_state = Record::SceneReadyState::REGISTERED;
	return OK;
}

Error Registry::dispatch_scene_ready(const RecordRef &ref) {
	if (ref.registry != this) {
		return ERR_INVALID_DATA;
	}
	RecordRef retained;
	RegistrationContext context;
	GDExtensionDeviceExitSceneReadyCallbackV1 callback = nullptr;
	GDExtensionDeviceExitSceneReadyCallbackV2 callback_v2 = nullptr;
	GDExtensionSceneInstallPlanV2 plan{};
	void *userdata = nullptr;
	GDExtensionDeviceExitSceneReadyContextRef ready{};
	{
		MutexLock lock(mutex);
		auto &record = records[ref.slot];
		using State = Record::SceneReadyState;
		if (record.owner_thread != Thread::get_caller_id()) {
			return ERR_UNAUTHORIZED;
		}
		if (!(record.guard.entry_classes & GDEXTENSION_DEVICE_EXIT_ENTRY_SCENE_READY_V1)) {
			return OK;
		}
		if (record.quiescing || record.close_started || record.active_callbacks) {
			return ERR_BUSY;
		}
		if (record.scene_ready_state == State::DONE) {
			return OK;
		}
		if (record.scene_ready_state == State::FAILED_LATE_PARTIAL) {
			return record.scene_ready_error;
		}
		if (record.scene_ready_state != State::PENDING || record.guard_enrollment != GuardEnrollment::QUALIFIED || !record.attached ||
				!record.bootstrap_tuple_observed || !record.levels[GDEXTENSION_INITIALIZATION_SCENE].initialize_returned ||
				!record.reserved_scene_ready_frame || (!record.scene_ready_callback && !record.scene_ready_callback_v2) || record.scene_ready_view_slot == INVALID_SLOT) {
			return ERR_UNAVAILABLE;
		}
		SceneReadyFailureReason identity_failure = SceneReadyFailureReason::ACTUAL_RD_UNSUPPORTED;
		if ((record.scene_ready_flavor == Record::SceneReadyFlavor::V2 || record.scene_install_plan_selected) && !scene_install_plan_matches_locked(record, &identity_failure)) {
			record.scene_ready_state = State::UNSUPPORTED_BEFORE_EFFECTS;
			record.scene_ready_error = ERR_UNAVAILABLE;
			record.scene_ready_failure_reason = identity_failure;
#ifdef TESTS_ENABLED
			std::fprintf(stderr, "SI2_TEST_PLAN_IDENTITY_REJECT reason=%u\n", uint32_t(identity_failure));
#endif
			retire_scene_ready_reservation_locked(record);
			return ERR_UNAVAILABLE;
		}
		if (record.active_callbacks >= UINT32_MAX - record.cleanup_reservations) {
			return ERR_PARAMETER_RANGE_ERROR;
		}
		// Transfer the original reserved private reference, never allocate a new
		// callback/view after the extension's early effects already exist.
		retained = RecordRef(this, ref.slot);
		context.enter(retained, true);
		context.frame_id = record.reserved_scene_ready_frame;
		record.reserved_scene_ready_frame = 0;
		context.level = GDEXTENSION_INITIALIZATION_SCENE;
		context.level_phase = LevelPhase::SCENE_READY;
		record.scene_ready_state = State::IN_FRAME;
		record.active_callbacks++;
		callback = record.scene_ready_callback;
		callback_v2 = record.scene_ready_callback_v2;
		plan = record.scene_install_plan;
		userdata = record.scene_ready_userdata;
		ready = { instance, record.scene_ready_view_serial };
	}
	const int32_t returned = callback_v2 ? callback_v2(userdata, ready, &plan) : callback(userdata, ready);
	const Error error = returned >= OK && returned < ERR_MAX ? Error(returned) : ERR_INVALID_DATA;
	context.leave(); // No borrowed Ready handle outlives the actual DSO return.
	{
		MutexLock lock(mutex);
		auto &record = records[ref.slot];
		record.scene_ready_error = error;
		record.scene_ready_state = error == OK ? Record::SceneReadyState::DONE : Record::SceneReadyState::FAILED_LATE_PARTIAL;
		record.scene_ready_view_slot = INVALID_SLOT;
		record.scene_ready_view_serial = 0;
		record.scene_ready_callback = nullptr;
		record.scene_ready_callback_v2 = nullptr;
		record.scene_ready_userdata = nullptr;
		record.active_callbacks--;
	}
	retained.reset();
	return error;
}

bool Registry::scene_ready_pending(const RecordRef &ref) {
	if (ref.registry != this) {
		return false;
	}
	MutexLock lock(mutex);
	return records[ref.slot].scene_ready_state == Record::SceneReadyState::PENDING;
}

Error Registry::scene_ready_initialization_result(const RecordRef &ref) {
	if (ref.registry != this) {
		return ERR_INVALID_DATA;
	}
	MutexLock lock(mutex);
	auto &record = records[ref.slot];
	if (!(record.guard.entry_classes & GDEXTENSION_DEVICE_EXIT_ENTRY_SCENE_READY_V1)) {
		return OK;
	}
	if (record.owner_thread != Thread::get_caller_id()) {
		return ERR_UNAUTHORIZED;
	}
	if (!record.levels[GDEXTENSION_INITIALIZATION_SCENE].initialize_returned) {
		return ERR_BUSY;
	}
	if (record.scene_ready_state == Record::SceneReadyState::PENDING) {
		return OK;
	}
	// The early wrapper really returned, but did not install its mandatory
	// original callback. Preserve early actions for the owner's InitAbort.
	if (record.scene_ready_state == Record::SceneReadyState::ABSENT) {
		record.scene_ready_error = ERR_UNAVAILABLE;
		retire_scene_ready_reservation_locked(record);
	}
	return record.scene_ready_error == OK ? ERR_UNAVAILABLE : record.scene_ready_error;
}

Error Registry::reject_pending_scene_ready_before_effects(const RecordRef &ref, SceneReadyFailureReason reason) {
	if (ref.registry != this) {
		return ERR_INVALID_DATA;
	}
	MutexLock lock(mutex);
	auto &record = records[ref.slot];
	if (record.owner_thread != Thread::get_caller_id()) {
		return ERR_UNAUTHORIZED;
	}
	if (record.quiescing || record.close_started || record.active_callbacks) {
		return ERR_BUSY;
	}
	if (record.scene_ready_state != Record::SceneReadyState::PENDING ||
			(reason != SceneReadyFailureReason::NO_SCENE_TREE && reason != SceneReadyFailureReason::NO_ROOT_WORLD &&
					reason != SceneReadyFailureReason::NO_PUBLISHED_RENDERING_SERVER && reason != SceneReadyFailureReason::ACTUAL_RD_UNSUPPORTED)) {
		return ERR_INVALID_DATA;
	}
	record.scene_ready_failure_reason = reason;
	record.scene_ready_error = ERR_UNAVAILABLE;
	record.scene_ready_state = Record::SceneReadyState::UNSUPPORTED_BEFORE_EFFECTS;
	retire_scene_ready_reservation_locked(record);
	return ERR_UNAVAILABLE;
}

Error Registry::schedule_scene_ready_object_cleanup(const GDExtensionDeviceExitAbi *abi, GDExtensionDeviceExitSceneReadyContextRef ready, uint32_t p_phase, uint64_t object_id) {
	// Every attempt in this actual core frame invalidates the preceding fatal
	// receipt, including malformed input and a foreign/stale supplied view.
	// This is frame-local diagnostic storage, not a holder/output mutation.
	auto *attempt = RegistrationContext::current;
	if (attempt && attempt->admitted && attempt->reference && attempt->reference->registry == this && attempt->level_phase == LevelPhase::SCENE_READY) {
		attempt->scene_ready_schedule_object = 0;
		attempt->scene_ready_schedule_error = OK;
	}
	if (!abi_valid(abi)) {
		return ERR_INVALID_PARAMETER;
	}
	MutexLock lock(mutex);
	View *view = nullptr;
	Error error = resolve_borrow_locked(ready.registry_instance, ready.handle_serial, ViewDomain::SCENE_READY, view);
	if (error != OK) {
		return error;
	}
	auto *frame = RegistrationContext::current;
	auto &record = records[view->record];
	if (frame->level_phase != LevelPhase::SCENE_READY || !frame->registration_allowed || frame->level != GDEXTENSION_INITIALIZATION_SCENE ||
			record.guard_enrollment != GuardEnrollment::QUALIFIED || record.scene_ready_state != Record::SceneReadyState::IN_FRAME) {
		return ERR_UNAUTHORIZED;
	}
	if (record.quiescing || record.close_started) {
		return ERR_BUSY;
	}
	frame->scene_ready_schedule_object = object_id;
	frame->scene_ready_schedule_error = OK;
	if (p_phase > 1) {
		frame->scene_ready_schedule_error = ERR_INVALID_DATA;
		return ERR_INVALID_DATA;
	}
	const auto &level = record.levels[GDEXTENSION_INITIALIZATION_SCENE];
	const auto &action = p_phase == 0 ? level.user_action : level.binding_action;
	error = !action.serial || action.returned ? ERR_UNAVAILABLE : schedule_object_cleanup_pair_locked(record, action.serial, object_id);
	frame->scene_ready_schedule_error = error;
	return error;
}

Error Registry::scene_ready_abort_unscheduled_pair(const GDExtensionDeviceExitAbi *abi, GDExtensionDeviceExitSceneReadyContextRef ready, int32_t schedule_error, uint32_t reason, uint64_t object_id) {
	if (!abi_valid(abi)) {
		return ERR_INVALID_PARAMETER;
	}
	MutexLock lock(mutex);
	View *view = nullptr;
	Error error = resolve_borrow_locked(ready.registry_instance, ready.handle_serial, ViewDomain::SCENE_READY, view);
	if (error != OK) {
		return error;
	}
	auto *frame = RegistrationContext::current;
	const auto &record = records[view->record];
	if (frame->level_phase != LevelPhase::SCENE_READY || !frame->registration_allowed || record.scene_ready_state != Record::SceneReadyState::IN_FRAME) {
		return ERR_UNAUTHORIZED;
	}
	if (schedule_error < FAILED || schedule_error >= ERR_MAX) {
		return ERR_INVALID_PARAMETER;
	}
	if (reason != GDEXTENSION_DEVICE_EXIT_SCENE_READY_ABORT_UNSCHEDULED_PAIR_V1 ||
			frame->scene_ready_schedule_object != object_id || frame->scene_ready_schedule_error != schedule_error ||
			validate_object_cleanup_pair_locked(record, object_id) != OK) {
		return ERR_INVALID_DATA;
	}
	char message[192];
	const int length = std::snprintf(message, sizeof(message), "SCENE_READY_UNSCHEDULED_PAIR object=%llu error=%d reason=%u\n", static_cast<unsigned long long>(object_id), schedule_error, reason);
	const size_t size = length > 0 && size_t(length) < sizeof(message) ? size_t(length) : 0;
#ifdef WINDOWS_ENABLED
	(void)_write(2, message, static_cast<unsigned int>(size));
#else
	const auto written = write(2, message, size);
	(void)written;
#endif
	std::_Exit(86);
}

void Registry::clear_guard_control_locked(Record &record) {
	retire_scene_ready_reservation_locked(record);
	record.guard = {};
	record.guard_token = 0;
	if (record.control_reference_reserved) {
		record.private_refs--;
		record.control_reference_reserved = false;
	}
	if (record.control_view_slot != INVALID_SLOT) {
		views[record.control_view_slot] = {};
		record.control_view_slot = INVALID_SLOT;
	}
	record.reserved_control_frame = record.reserved_control_view = 0;
	for (auto &level : record.levels) {
		if (level.reserved_initialize_frame) {
			expire_frame_locked(level.reserved_initialize_frame);
			record.private_refs--;
		}
		if (level.reserved_deinitialize_frame) {
			expire_frame_locked(level.reserved_deinitialize_frame);
			record.private_refs--;
		}
		level.reserved_initialize_frame = level.reserved_deinitialize_frame = 0;
		level.initialize_view = level.deinitialize_view = 0;
		level.user_action = {};
		level.binding_action = {};
	}
	record.control_complete = true;
}
void Registry::revoke_guard_locked(Record &record) {
	clear_guard_control_locked(record);
	record.guard_enrollment = GuardEnrollment::REVOKED;
	record.control_complete = true;
}
bool Registry::guard_qualified(const RecordRef &ref) {
	if (ref.registry != this) {
		return false;
	}
	MutexLock lock(mutex);
	return records[ref.slot].guard_enrollment == GuardEnrollment::QUALIFIED;
}
Error Registry::issue_borrow_locked(ViewDomain domain, uint64_t action, uint64_t &out) {
	auto *frame = RegistrationContext::current;
	if (!frame || !frame->admitted || frame->reference->registry != this) {
		return ERR_UNAUTHORIZED;
	}
	for (uint32_t i = 0; i < view_limit; i++) {
		if (views[i].serial) {
			continue;
		}
		if (!serials.take(out)) {
			return ERR_PARAMETER_RANGE_ERROR;
		}
		views[i] = { out, frame->reference->slot, domain, frame->frame_id, action };
		return OK;
	}
	return ERR_PARAMETER_RANGE_ERROR;
}
Error Registry::resolve_borrow_locked(uint64_t registry_instance, uint64_t serial, ViewDomain domain, View *&out) {
	if (registry_instance != instance || !serial) {
		return ERR_INVALID_DATA;
	}
	for (uint32_t i = 0; i < view_limit; i++) {
		auto &view = views[i];
		if (view.serial != serial || view.domain != domain || !view.frame) {
			continue;
		}
		auto *frame = RegistrationContext::current;
		if (!frame || !frame->admitted || frame->reference->registry != this || frame->reference->slot != view.record || frame->frame_id != view.frame ||
				records[view.record].owner_thread != Thread::get_caller_id()) {
			return ERR_UNAUTHORIZED;
		}
		out = &view;
		return OK;
	}
	return ERR_INVALID_DATA;
}
void Registry::expire_frame_locked(uint64_t frame) {
	for (auto &view : views) {
		if (view.domain != ViewDomain::METADATA && view.frame == frame) {
			auto &record = records[view.record];
			if (record.control_view_slot != INVALID_SLOT && &view == &views[record.control_view_slot] && record.control_reference_reserved) {
				view.frame = 0; // Reservation remains; resolve rejects expired control.
			} else {
				view = {};
			}
		}
	}
}
Error Registry::borrow_guarded_library(const GDExtensionDeviceExitAbi *abi, GDExtensionClassLibraryPtr library, GuardedLibraryRef *out) {
	if (!abi_valid(abi) || !empty_output(out)) {
		return ERR_INVALID_DATA;
	}
	MutexLock lock(mutex);
	auto *frame = RegistrationContext::current;
	if (!frame || !frame->admitted || frame->reference->registry != this) {
		return ERR_UNAUTHORIZED;
	}
	auto &record = records[frame->reference->slot];
	if (record.original_owner != library || record.owner_thread != Thread::get_caller_id()) {
		return ERR_UNAUTHORIZED;
	}
	if (record.guard_enrollment != GuardEnrollment::QUALIFIED) {
		return ERR_UNAVAILABLE;
	}
	uint64_t serial;
	Error error = issue_borrow_locked(ViewDomain::LIBRARY, 0, serial);
	if (error == OK) {
		*out = { instance, serial };
	}
	return error;
}
Error Registry::borrow_initialize_level(const GDExtensionDeviceExitAbi *abi, GDExtensionClassLibraryPtr library, uint32_t level, InitializationLevelContextRef *out) {
	if (!abi_valid(abi) || !empty_output(out) || level > 3) {
		return ERR_INVALID_DATA;
	}
	MutexLock lock(mutex);
	auto *frame = RegistrationContext::current;
	if (!frame || frame->level_phase != LevelPhase::INITIALIZE || frame->level != int32_t(level) || frame->reference->registry != this) {
		return ERR_UNAUTHORIZED;
	}
	auto &record = records[frame->reference->slot];
	if (record.original_owner != library || record.owner_thread != Thread::get_caller_id()) {
		return ERR_UNAUTHORIZED;
	}
	if (record.guard_enrollment != GuardEnrollment::QUALIFIED) {
		return ERR_UNAVAILABLE;
	}
	*out = { instance, record.levels[level].initialize_view };
	return OK;
}
Error Registry::borrow_deinit_level(const GDExtensionDeviceExitAbi *abi, GDExtensionClassLibraryPtr library, uint32_t level, DeinitLevelContextRef *out) {
	if (!abi_valid(abi) || !empty_output(out) || level > 3) {
		return ERR_INVALID_DATA;
	}
	MutexLock lock(mutex);
	auto *frame = RegistrationContext::current;
	if (!frame || frame->level_phase != LevelPhase::DEINITIALIZE || frame->level != int32_t(level) || frame->reference->registry != this) {
		return ERR_UNAUTHORIZED;
	}
	auto &record = records[frame->reference->slot];
	if (record.original_owner != library || record.owner_thread != Thread::get_caller_id()) {
		return ERR_UNAUTHORIZED;
	}
	if (record.guard_enrollment != GuardEnrollment::QUALIFIED) {
		return ERR_UNAVAILABLE;
	}
	*out = { instance, record.levels[level].deinitialize_view };
	return OK;
}
Error Registry::borrow_level_cleanup(const GDExtensionDeviceExitAbi *abi, InitializationLevelContextRef context, uint32_t p_phase, RegisteredCleanupRef *out) {
	if (!abi_valid(abi) || !empty_output(out) || p_phase > 1) {
		return ERR_INVALID_DATA;
	}
	MutexLock lock(mutex);
	View *view = nullptr;
	Error error = resolve_borrow_locked(context.registry_instance, context.handle_serial, ViewDomain::INITIALIZE, view);
	if (error != OK) {
		return error;
	}
	auto *frame = RegistrationContext::current;
	if (frame->ack) {
		return ERR_UNAUTHORIZED;
	}
	auto &level = records[view->record].levels[frame->level];
	// Public USER=0/BINDING=1 is decoded explicitly, never cast to CleanupPhase.
	const LevelAction &action = p_phase == 0 ? level.user_action : level.binding_action;
	if (!action.serial || action.returned) {
		return ERR_UNAVAILABLE;
	}
	uint64_t serial;
	error = issue_borrow_locked(ViewDomain::ACTION, action.serial, serial);
	if (error == OK) {
		*out = { instance, serial };
	}
	return error;
}
Error Registry::after_binding_initialize(const GDExtensionDeviceExitAbi *abi, InitializationLevelContextRef context) {
	if (!abi_valid(abi)) {
		return ERR_INVALID_DATA;
	}
	MutexLock lock(mutex);
	View *view = nullptr;
	Error error = resolve_borrow_locked(context.registry_instance, context.handle_serial, ViewDomain::INITIALIZE, view);
	if (error != OK) {
		return error;
	}
	auto *frame = RegistrationContext::current;
	if (frame->level_phase != LevelPhase::INITIALIZE || frame->ack) {
		return ERR_INVALID_DATA;
	}
	frame->ack = true;
	return OK;
}
bool Registry::action_safe_locked(const Record &record, uint64_t action) const {
	if (!action) {
		return true;
	} // No USER cleanup bit means no USER action exists.
	for (auto *holder = record.holders; holder; holder = holder->record_next) {
		const uint64_t owner_action = holder->typed_code_hold ? holder->terminal_action : holder->scheduled_action;
		if (owner_action == action &&
				(!holder->pending || holder->return_frame != RegistrationContext::current->frame_id)) {
			return false;
		}
	}
	return true;
}

Error Registry::validate_object_cleanup_pair_locked(const Record &record, uint64_t object_id) const {
	if (!object_id) {
		return ERR_INVALID_DATA;
	}
	if (!ObjectDB::get_instance(ObjectID(object_id))) {
		return ERR_DOES_NOT_EXIST;
	}
	bool found_cleanup = false;
	bool found_binding = false;
	uint64_t cleanup_serial = 0;
	for (auto *holder = record.holders; holder; holder = holder->record_next) {
		if (holder->core_object != object_id || holder->generation != record.snapshot.library_generation) {
			continue;
		}
		if (holder->cleanup) {
			if (!holder->core_object_action || holder->terminal_action != object_id || holder->scheduled_action || holder->acquired_thread != record.owner_thread || holder->pending) {
				return ERR_INVALID_DATA;
			}
			found_cleanup = true;
			cleanup_serial = holder->serial;
		}
	}
	if (!found_cleanup) {
		return ERR_DOES_NOT_EXIST;
	}
	for (auto *holder = record.holders; holder; holder = holder->record_next) {
		if (holder->core_object != object_id || holder->cleanup || holder->generation != record.snapshot.library_generation) {
			continue;
		}
		if (!holder->core_owned || holder->terminal_action != cleanup_serial || holder->scheduled_action || holder->acquired_thread != record.owner_thread || holder->pending) {
			return ERR_INVALID_DATA;
		}
		found_binding = true;
	}
	if (!found_binding) {
		return ERR_DOES_NOT_EXIST;
	}
	return OK;
}

Error Registry::schedule_object_cleanup_pair_locked(Record &record, uint64_t action, uint64_t object_id) {
	if (!action) {
		return ERR_INVALID_DATA;
	}
	const Error error = validate_object_cleanup_pair_locked(record, object_id);
	if (error != OK) {
		return error;
	}
	for (auto *holder = record.holders; holder; holder = holder->record_next) {
		if (holder->core_object == object_id && holder->generation == record.snapshot.library_generation) {
			holder->scheduled_action = action;
		}
	}
	return OK;
}

Error Registry::schedule_object_cleanup(const GDExtensionDeviceExitAbi *abi, InitializationLevelContextRef context, RegisteredCleanupRef terminal, uint64_t object_id) {
	if (!abi_valid(abi) || !object_id) {
		return ERR_INVALID_DATA;
	}
	MutexLock lock(mutex);
	View *level_view = nullptr;
	View *action_view = nullptr;
	Error error = resolve_borrow_locked(context.registry_instance, context.handle_serial, ViewDomain::INITIALIZE, level_view);
	if (error != OK) {
		return error;
	}
	error = resolve_borrow_locked(terminal.registry_instance, terminal.handle_serial, ViewDomain::ACTION, action_view);
	if (error != OK) {
		return error;
	}
	auto *frame = RegistrationContext::current;
	auto &record = records[level_view->record];
	const auto &level = record.levels[frame->level];
	if (frame->level_phase != LevelPhase::INITIALIZE || !frame->registration_allowed || frame->ack ||
			level_view->record != action_view->record || record.quiescing ||
			(action_view->action != level.user_action.serial && action_view->action != level.binding_action.serial)) {
		return ERR_UNAUTHORIZED;
	}
	return schedule_object_cleanup_pair_locked(record, action_view->action, object_id);
}

Error Registry::begin_user_terminator(const GDExtensionDeviceExitAbi *abi, GuardToken token, DeinitLevelContextRef context) {
	if (!abi_valid(abi)) {
		return ERR_INVALID_DATA;
	}
	MutexLock lock(mutex);
	View *view = nullptr;
	Error error = resolve_borrow_locked(context.registry_instance, context.handle_serial, ViewDomain::DEINITIALIZE, view);
	if (error != OK) {
		return error;
	}
	auto *frame = RegistrationContext::current;
	if (token.registry_instance != instance || token.handle_serial != records[view->record].guard_token || frame->terminal_stage != 0) {
		return ERR_INVALID_DATA;
	}
	frame->terminal_stage = 1;
	return OK;
}
Error Registry::after_user_terminator(const GDExtensionDeviceExitAbi *abi, DeinitLevelContextRef context) {
	if (!abi_valid(abi)) {
		return ERR_INVALID_DATA;
	}
	MutexLock lock(mutex);
	View *view = nullptr;
	Error error = resolve_borrow_locked(context.registry_instance, context.handle_serial, ViewDomain::DEINITIALIZE, view);
	if (error != OK) {
		return error;
	}
	auto *frame = RegistrationContext::current;
	auto &record = records[view->record];
	if (frame->terminal_stage != 1) {
		return ERR_INVALID_DATA;
	}
	if (!action_safe_locked(record, record.levels[frame->level].user_action.serial)) {
		return ERR_BUSY;
	}
	frame->terminal_stage = 2;
	return OK;
}
Error Registry::after_binding_deinit(const GDExtensionDeviceExitAbi *abi, DeinitLevelContextRef context) {
	if (!abi_valid(abi)) {
		return ERR_INVALID_DATA;
	}
	MutexLock lock(mutex);
	View *view = nullptr;
	Error error = resolve_borrow_locked(context.registry_instance, context.handle_serial, ViewDomain::DEINITIALIZE, view);
	if (error != OK) {
		return error;
	}
	auto *frame = RegistrationContext::current;
	auto &record = records[view->record];
	if (frame->terminal_stage != 2 || frame->ack) {
		return ERR_INVALID_DATA;
	}
	if (!action_safe_locked(record, record.levels[frame->level].binding_action.serial)) {
		return ERR_BUSY;
	}
	frame->terminal_stage = 3;
	frame->ack = true;
	return OK;
}

namespace {
struct CodeHolder : HolderRetirement {
	explicit CodeHolder(RecordRef &&ref) : HolderRetirement(std::move(ref)) {
		typed_code_hold = true;
		destroy = [](HolderRetirement *p) { memdelete(static_cast<CodeHolder *>(p)); };
	}
};
} //namespace
Error Registry::acquire_code_hold(const GDExtensionDeviceExitAbi *abi, GuardedLibraryRef library, RegisteredCleanupRef terminal, LibraryCodeHoldRef *out) {
	if (!abi_valid(abi) || !empty_output(out)) {
		return ERR_INVALID_DATA;
	}
	MutexLock lock(mutex);
	View *lib = nullptr, *action_view = nullptr;
	Error error = resolve_borrow_locked(library.registry_instance, library.handle_serial, ViewDomain::LIBRARY, lib);
	if (error != OK) {
		return error;
	}
	error = resolve_borrow_locked(terminal.registry_instance, terminal.handle_serial, ViewDomain::ACTION, action_view);
	if (error != OK) {
		return error;
	}
	if (lib->record != action_view->record) {
		return ERR_INVALID_DATA;
	}
	if (RegistrationContext::current->ack) {
		return ERR_UNAUTHORIZED;
	}
	auto &record = records[lib->record];
	if (record.quiescing || record.close_started) {
		return ERR_UNAVAILABLE;
	}
	if (record.private_refs >= private_limit || record.binding_holders == UINT32_MAX || holder_serials.exhausted || serials.exhausted) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	record.private_refs++;
	auto *holder = memnew(CodeHolder(RecordRef(this, lib->record)));
	holder_serials.take(holder->serial);
	serials.take(holder->handle_serial);
	holder->generation = record.snapshot.library_generation;
	holder->terminal_action = action_view->action;
	holder->record_next = record.holders;
	record.holders = holder;
	record.binding_holders++;
	*out = { instance, holder->handle_serial };
	return OK;
}
Error Registry::clone_code_hold(const GDExtensionDeviceExitAbi *abi, LibraryCodeHoldRef source, LibraryCodeHoldRef *out) {
	if (!abi_valid(abi) || !empty_output(out) || source.registry_instance != instance || !source.handle_serial) {
		return ERR_INVALID_DATA;
	}
	MutexLock lock(mutex);
	for (uint32_t i = 0; i < record_limit; i++) {
		auto &record = records[i];
		for (auto *holder = record.holders; holder; holder = holder->record_next) {
			if (!holder->typed_code_hold || holder->handle_serial != source.handle_serial || holder->pending) {
				continue;
			}
			auto *frame = RegistrationContext::current;
			if (!frame || !frame->admitted || frame->reference->registry != this || frame->reference->slot != i || holder->acquired_thread != Thread::get_caller_id()) {
				return ERR_UNAUTHORIZED;
			}
			if (record.quiescing || record.close_started) {
				return ERR_UNAVAILABLE;
			}
			if (frame->ack) {
				return ERR_UNAUTHORIZED;
			}
			if (holder->generation != record.snapshot.library_generation || record.owner_thread != holder->acquired_thread) {
				return ERR_INVALID_DATA;
			}
			bool action_live = false;
			for (const auto &level : record.levels) {
				for (const auto *action : { &level.user_action, &level.binding_action }) {
					if (action->serial && action->serial == holder->terminal_action && !action->returned &&
							action->generation == holder->generation && action->owner_thread == holder->acquired_thread &&
							action->wrapper == record.original_initialization.deinitialize && action->userdata == record.original_initialization.userdata) {
						action_live = true;
					}
				}
			}
			if (!action_live) {
				return ERR_INVALID_DATA;
			}
			if (record.private_refs >= private_limit || record.binding_holders == UINT32_MAX || holder_serials.exhausted || serials.exhausted) {
				return ERR_PARAMETER_RANGE_ERROR;
			}
			record.private_refs++;
			auto *copy = memnew(CodeHolder(RecordRef(this, i)));
			holder_serials.take(copy->serial);
			serials.take(copy->handle_serial);
			copy->generation = holder->generation;
			copy->terminal_action = holder->terminal_action;
			copy->record_next = record.holders;
			record.holders = copy;
			record.binding_holders++;
			*out = { instance, copy->handle_serial };
			return OK;
		}
	}
	return ERR_INVALID_DATA;
}
Error Registry::release_code_hold(const GDExtensionDeviceExitAbi *abi, LibraryCodeHoldRef hold) {
	if (!abi_valid(abi) || hold.registry_instance != instance || !hold.handle_serial) {
		return ERR_INVALID_DATA;
	}
	MutexLock lock(mutex);
	for (uint32_t i = 0; i < record_limit; i++) {
		auto &record = records[i];
		for (auto *holder = record.holders; holder; holder = holder->record_next) {
			if (!holder->typed_code_hold || holder->handle_serial != hold.handle_serial || holder->pending) {
				continue;
			}
			if (holder->qualification_children) {
				return ERR_BUSY;
			}
			auto *frame = RegistrationContext::current;
			if (!frame || !frame->admitted || frame->reference->registry != this || frame->reference->slot != i) {
				return ERR_INVALID_DATA;
			}
			if (holder->acquired_thread != Thread::get_caller_id()) {
				return ERR_UNAUTHORIZED;
			}
			// A foreign top must never search through to an older matching frame.
			// Among contiguous frames of this generation, use the outermost real
			// admitted frame; identity-only frames cannot acknowledge return.
			for (auto *outer = frame->previous; outer; outer = outer->previous) {
				if (outer->reference->registry != this || outer->reference->slot != i) {
					break;
				}
				if (outer->admitted) {
					frame = outer;
				}
			}
			if (holder->generation != record.snapshot.library_generation) {
				return ERR_INVALID_DATA;
			}
			if (frame->level < 0 || frame->level > 3) {
				return ERR_UNAUTHORIZED;
			}
			const auto &level = record.levels[frame->level];
			const bool user = holder->terminal_action == level.user_action.serial && level.user_action.serial;
			const bool binding = holder->terminal_action == level.binding_action.serial && level.binding_action.serial;
			if (!user && !binding) {
				return ERR_INVALID_DATA;
			}
			if (frame->level_phase != LevelPhase::INITIALIZE && !(frame->level_phase == LevelPhase::DEINITIALIZE && ((user && frame->terminal_stage == 1) || (binding && frame->terminal_stage == 2)))) {
				return ERR_UNAUTHORIZED;
			}
			// Do not cross a foreign frame or borrow an identity-only scope. The
			// exact terminal initializer/cleanup frame owns this return obligation.
			holder->pending = true;
			holder->retired_thread = Thread::get_caller_id();
			holder->retirement_attempt = record.exit_attempt;
			holder->return_frame = frame->frame_id;
			holder->frame_next = frame->retiring_holders;
			frame->retiring_holders = holder;
			record.return_pending_holders++;
			return OK;
		}
	}
	return ERR_INVALID_DATA;
}

Error Registry::publish_exit_result(const GDExtensionDeviceExitAbi *abi, GuardToken token, DeinitPreflightContextRef context, const ExitDiagnosticV1 *diagnostic) {
	if (!abi_valid(abi) || !aligned_pointer(diagnostic) || !abi_valid(&diagnostic->abi, sizeof(ExitDiagnosticV1)) || (diagnostic->level > 3 && diagnostic->level != UINT32_MAX) ||
			(diagnostic->disposition != OK && diagnostic->disposition != ERR_BUSY && diagnostic->disposition != ERR_UNAVAILABLE)) {
		return ERR_INVALID_DATA;
	}
	MutexLock lock(mutex);
	View *view = nullptr;
	Error error = resolve_borrow_locked(context.registry_instance, context.handle_serial, ViewDomain::PREFLIGHT, view);
	if (error != OK) {
		return error;
	}
	auto *frame = RegistrationContext::current;
	if (token.registry_instance != instance || token.handle_serial != records[view->record].guard_token || frame->diagnostic_published) {
		return ERR_INVALID_DATA;
	}
	frame->diagnostic_published = true;
	frame->diagnostic_result = diagnostic->disposition;
	frame->diagnostic_pending = diagnostic->pending_count;
	return OK;
}

Error Registry::guarded_preflight(const RecordRef &ref, CloseRoute route) {
	RecordRef retained;
	RegistrationContext context;
	PreflightFnV1 callback = nullptr;
	void *userdata = nullptr;
	DeinitPreflightContextRef borrowed{};
	{
		MutexLock lock(mutex);
		auto &record = records[ref.slot];
		if (record.guard_enrollment != GuardEnrollment::QUALIFIED || record.owner_thread != Thread::get_caller_id()) {
			return ERR_UNAUTHORIZED;
		}
		if (record.close_started || record.close_error != OK) {
			return ERR_UNAVAILABLE;
		}
		if (record.scene_ready_state == Record::SceneReadyState::PENDING || record.scene_ready_state == Record::SceneReadyState::IN_FRAME) {
			return ERR_BUSY;
		}
		if (record.preflight_sealed && record.preflight_route != route) {
			return ERR_INVALID_DATA;
		}
		if (record.control_complete || record.preflight_accepted) {
			return OK;
		}
		// Only original core-owned services with wired teardown boundaries qualify.
		// The common Main/test schedule guarantees these through SCENE, not lower levels.
		constexpr uint32_t supported_services = (1u << 1) | (1u << 5) | (1u << 7);
		for (uint32_t i = 0; i < 4; i++) {
			const auto &level = record.levels[i];
			const uint32_t needs = level.user_action.services | level.binding_action.services;
			if ((needs & ~supported_services) || (needs && i < GDEXTENSION_INITIALIZATION_SCENE) ||
					(level.initialized && !level.finalized && (needs & ~live_services))) {
				return ERR_UNAVAILABLE;
			}
		}
		if (!record.control_reference_reserved || record.control_view_slot == INVALID_SLOT) {
			return ERR_UNAVAILABLE;
		}
		if (!record.reserved_control_frame && (frame_serials.exhausted || serials.exhausted)) {
			return ERR_PARAMETER_RANGE_ERROR;
		}
		if (!record.preflight_sealed) {
			if (!record.exit_attempt && !exit_attempts.take(record.exit_attempt)) {
				return ERR_PARAMETER_RANGE_ERROR;
			}
			record.preflight_route = route;
			record.preflight_sealed = record.quiescing = true;
			for (auto *holder = record.holders; holder; holder = holder->record_next) {
				if (holder->pending) {
					holder->retirement_attempt = record.exit_attempt;
				}
			}
		}
		record.preflight_accepted = false;
		if (record.active_callbacks) {
			return ERR_BUSY;
		}
		for (auto *holder = record.holders; holder; holder = holder->record_next) {
			if (!holder->typed_code_hold && (!holder->scheduled_action || !holder->core_object || (holder->cleanup ? !holder->core_object_action || holder->terminal_action != holder->core_object : !holder->core_owned))) {
				return ERR_BUSY;
			}
			bool scheduled = false;
			for (const auto &level : record.levels) {
				for (const auto *action : { &level.user_action, &level.binding_action }) {
					if (action->serial && action->serial == (holder->typed_code_hold ? holder->terminal_action : holder->scheduled_action) && !action->returned && level.initialized && !level.finalized &&
							action->generation == holder->generation && action->generation == record.snapshot.library_generation &&
							action->owner_thread == holder->acquired_thread && action->owner_thread == record.owner_thread &&
							action->wrapper == record.original_initialization.deinitialize && action->userdata == record.original_initialization.userdata) {
						scheduled = true;
					}
				}
			}
			if (!scheduled) {
				return ERR_BUSY;
			}
		}
		// Transfer the reserved control reference into this real call, then back
		// into the record after return. Retry never needs fresh holder/view quota.
		retained = RecordRef(this, ref.slot);
		record.active_callbacks++;
		context.enter(retained, false);
		uint64_t serial = record.reserved_control_view;
		if (record.reserved_control_frame) {
			context.frame_id = record.reserved_control_frame;
			record.reserved_control_frame = record.reserved_control_view = 0;
		} else {
			frame_serials.take(context.frame_id);
			serials.take(serial); // Each retry must issue a fresh nonreused identity.
		}
		context.level_phase = LevelPhase::PREFLIGHT;
		views[record.control_view_slot] = { serial, ref.slot, ViewDomain::PREFLIGHT, context.frame_id, 0 };
		borrowed = { instance, serial };
		callback = record.guard.preflight;
		userdata = record.guard.preflight_userdata;
	}
	const int32_t result = callback(borrowed, userdata);
	const bool matched = context.diagnostic_published && result == context.diagnostic_result;
	const bool empty = context.diagnostic_pending == 0;
	context.leave();
	{
		MutexLock lock(mutex);
		auto &record = records[ref.slot];
		record.active_callbacks--;
		retained.registry = nullptr; // Return the reserved reference to Record.
		record.preflight_accepted = matched && result == OK && empty;
		if (!record.preflight_accepted) {
			return matched ? (result == OK ? ERR_BUSY : Error(result)) : ERR_UNAVAILABLE;
		}
		bool complete = true;
		for (const auto &level : record.levels) {
			complete &= !level.initialized || level.finalized;
		}
		if (complete) {
			clear_guard_control_locked(record);
		}
	}
	return OK;
}
[[noreturn]] void Registry::terminate_initialize_blocked(InitializationLevelContextRef context, int32_t original_error) {
	int32_t level = -1;
	Error error = ERR_INVALID_DATA;
	{
		MutexLock lock(mutex);
		View *view = nullptr;
		if (resolve_borrow_locked(context.registry_instance, context.handle_serial, ViewDomain::INITIALIZE, view) == OK) {
			level = RegistrationContext::current->level;
			error = original_error > OK && original_error < ERR_PRINTER_ON_FIRE ? Error(original_error) : ERR_UNAVAILABLE;
		}
	}
	require_level_dispatch_success(error, level, true);
	CRASH_NOW();
}
[[noreturn]] void Registry::terminate_blocked(DeinitLevelContextRef context, int32_t original_error) {
	int32_t level = -1;
	Error error = ERR_INVALID_DATA;
	{
		MutexLock lock(mutex);
		View *view = nullptr;
		if (resolve_borrow_locked(context.registry_instance, context.handle_serial, ViewDomain::DEINITIALIZE, view) == OK) {
			level = RegistrationContext::current->level;
			error = original_error > OK && original_error < ERR_PRINTER_ON_FIRE ? Error(original_error) : ERR_UNAVAILABLE;
		}
	}
	require_level_dispatch_success(error, level, false);
	CRASH_NOW();
}
} // namespace GDExtensionDeviceExit
