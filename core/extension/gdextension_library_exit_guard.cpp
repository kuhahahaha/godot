/**************************************************************************/
/*  gdextension_library_exit_guard.cpp                                    */
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

#include "gdextension_library_exit_guard.h"

#include "core/crypto/crypto_core.h"
#include "core/extension/gdextension.h"
#include "core/io/marshalls.h"
#include "core/os/thread.h"
#include "core/templates/safe_refcount.h"

#include <cstddef>
#include <cstdlib>
#ifdef WINDOWS_ENABLED
#include <io.h>
#else
#include <unistd.h>
#endif
#include <cstring>
#include <new>
#include <utility>

#ifdef TESTS_ENABLED
#include "tests/core/object/device_exit_startup_fixture.h"
#endif

namespace GDExtensionDeviceExit {

struct SharedRecord::Storage {
	SafeRefCount count;
	RecordRef reference;
	explicit Storage(RecordRef &&p_reference) : reference(std::move(p_reference)) { count.init(); }
};
SharedRecord::SharedRecord(RecordRef &&p_reference) {
	if (p_reference.is_valid()) {
		storage = memnew(Storage(std::move(p_reference)));
	}
}
SharedRecord::SharedRecord(const SharedRecord &p_other) : storage(p_other.storage) {
	if (storage) {
		CRASH_COND(!storage->count.ref());
	}
}
SharedRecord::SharedRecord(SharedRecord &&p_other) noexcept : storage(p_other.storage) {
	p_other.storage = nullptr;
}
SharedRecord &SharedRecord::operator=(SharedRecord p_other) {
	std::swap(storage, p_other.storage);
	return *this;
}
SharedRecord::~SharedRecord() {
	if (storage && storage->count.unref()) {
		memdelete(storage);
	}
}
const RecordRef &SharedRecord::get_reference() const {
	static const RecordRef empty;
	return storage ? storage->reference : empty;
}

HolderRetirement::HolderRetirement(RecordRef &&p_reference) : reference(std::move(p_reference)), acquired_thread(Thread::get_caller_id()) {}

struct BindingLifetime::Storage : HolderRetirement {
	SafeRefCount count;
	uint64_t source_object = 0;
	uint64_t source_action = 0;
	explicit Storage(RecordRef &&p_reference) : HolderRetirement(std::move(p_reference)) {
		count.init();
		destroy = [](HolderRetirement *p_cell) { memdelete(static_cast<Storage *>(p_cell)); };
	}
};
BindingLifetime::BindingLifetime(const BindingLifetime &p_other) : storage(p_other.storage) {
	if (storage) {
		CRASH_COND(!storage->count.ref());
	}
}
BindingLifetime::BindingLifetime(BindingLifetime &&p_other) noexcept : storage(std::exchange(p_other.storage, nullptr)) {}
BindingLifetime &BindingLifetime::operator=(BindingLifetime p_other) {
	std::swap(storage, p_other.storage);
	return *this;
}
BindingLifetime::~BindingLifetime() {
	if (storage && storage->count.unref()) {
		if (storage->terminal_action != storage->source_action || storage->core_object != storage->source_object) {
			stop_lifetime_entry(LifetimeFailure::UNREGISTERED);
		}
		storage->reference.registry->retire_binding_holder(storage);
	}
}
void BindingLifetime::record_core_disposal() {
	if (!storage) {
		return;
	}
	auto *registry = storage->reference.registry;
	MutexLock lock(registry->mutex);
	CRASH_COND(storage->pending || !storage->core_object || !storage->terminal_action || storage->generation != registry->records[storage->reference.slot].snapshot.library_generation);
	if (storage->terminal_action != storage->source_action || storage->core_object != storage->source_object) {
		stop_lifetime_entry(LifetimeFailure::UNREGISTERED);
	}
	storage->core_disposal_returned = true;
	storage->core_disposal_thread = Thread::get_caller_id();
	storage->core_disposal_frame = 0;
	// Only original Object/ClassDB disposal sources may cross a cohort's
	// foreign top after the exact owned callback has actually returned.
	for (auto *frame = RegistrationContext::current; frame; frame = frame->previous) {
		if (frame->admitted && frame->reference->registry == registry && frame->reference->slot == storage->reference.slot) {
			storage->core_disposal_frame = frame->frame_id;
		}
	}
}
Error BindingLifetime::acquire_core(const RecordRef &p_reference) {
	const Error result = acquire(p_reference);
	if (result == OK && storage) {
		storage->core_owned = true;
	}
	return result;
}
Error BindingLifetime::acquire(const RecordRef &p_reference) {
	if (storage || !p_reference.registry) {
		return ERR_INVALID_DATA;
	}
	Registry *registry = p_reference.registry;
	MutexLock lock(registry->mutex);
	auto &record = registry->records[p_reference.slot];
	if (!record.attached || record.quiescing || record.close_started || record.close_error != OK) {
		return ERR_BUSY;
	}
	if (record.private_refs >= registry->private_limit || record.binding_holders == UINT32_MAX || registry->holder_serials.exhausted) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	record.private_refs++;
	record.binding_holders++;
	storage = memnew(Storage(RecordRef(registry, p_reference.slot)));
	registry->holder_serials.take(storage->serial);
	storage->generation = record.snapshot.library_generation;
	storage->record_next = record.holders;
	record.holders = storage;
	return OK;
}

void Registry::unlink_holder_locked(HolderRetirement *p_holder) {
	Record &record = records[p_holder->reference.slot];
	auto **link = &record.holders;
	while (*link && *link != p_holder) {
		link = &(*link)->record_next;
	}
	CRASH_COND(!*link || record.binding_holders == 0);
	*link = p_holder->record_next;
	record.binding_holders--;
	if (p_holder->cleanup) {
		record.cleanup_by_phase[static_cast<uint8_t>(p_holder->phase)]--;
	}
	if (p_holder->pending) {
		record.return_pending_holders--;
		if (p_holder->cleanup) {
			record.retiring_cleanup_by_phase[static_cast<uint8_t>(p_holder->phase)]--;
		}
	}
}

void Registry::retire_binding_holder(HolderRetirement *p_holder) {
	// Identity-only frames cannot acknowledge return. Retain each exact storage
	// cell on the outermost actual frame of this generation on this thread.
	RegistrationContext *outer = nullptr;
	for (auto *frame = RegistrationContext::current; frame; frame = frame->previous) {
		if (frame->reference->registry != this || frame->reference->slot != p_holder->reference.slot) {
			if (!outer && p_holder->core_disposal_returned && p_holder->core_disposal_thread == Thread::get_caller_id()) {
				continue; // Exact core disposal receipt, never public release authority.
			}
			if (!outer) {
				stop_lifetime_entry(LifetimeFailure::UNREGISTERED);
			}
			break;
		}
		if (frame->admitted && (!p_holder->core_disposal_returned || !p_holder->core_disposal_frame || frame->frame_id == p_holder->core_disposal_frame)) {
			outer = frame;
		}
	}
	{
		MutexLock lock(mutex);
		Record &record = records[p_holder->reference.slot];
		CRASH_COND(p_holder->pending || record.binding_holders == 0 || p_holder->generation != record.snapshot.library_generation);
		const bool core_disposal = p_holder->core_disposal_returned && p_holder->core_disposal_thread == Thread::get_caller_id();
		if (!p_holder->cleanup && !core_disposal) {
			// Published Object code ownership can retire only at the original
			// disposal source. A same-generation callback is not that source.
			// The private acquire_core reservation may be cancelled before
			// binding any action, while its original admitted frame is live.
			const bool unpublished_reservation = p_holder->core_owned && !p_holder->terminal_action &&
					outer && p_holder->acquired_thread == Thread::get_caller_id();
			if (!unpublished_reservation) {
				stop_lifetime_entry(LifetimeFailure::UNREGISTERED);
			}
		}
		if (!core_disposal && (p_holder->acquired_thread != Thread::get_caller_id() || !outer)) {
			// An unused object cleanup registration has no executable body to
			// retire. Its original owner may cancel that reserved action.
			if (!p_holder->cleanup || p_holder->acquired_thread != Thread::get_caller_id()) {
				stop_lifetime_entry(LifetimeFailure::UNREGISTERED);
			}
		}
		p_holder->retired_thread = Thread::get_caller_id();
		p_holder->retirement_attempt = record.exit_attempt;
		if (p_holder->cleanup) {
			record.cleanup_reservations--;
		}
		if (outer) {
			p_holder->pending = true;
			p_holder->return_frame = outer->frame_id;
			p_holder->frame_next = outer->retiring_holders;
			outer->retiring_holders = p_holder;
			record.return_pending_holders++;
			if (p_holder->cleanup) {
				record.retiring_cleanup_by_phase[static_cast<uint8_t>(p_holder->phase)]++;
			}
		} else {
			unlink_holder_locked(p_holder);
		}
	}
	if (!outer) {
		p_holder->destroy(p_holder);
	}
}

void Registry::finish_retiring_holders(const RegistrationContext &p_frame) {
	CRASH_COND(RegistrationContext::current != &p_frame || !p_frame.admitted || !p_frame.reference || p_frame.reference->registry != this);
	HolderRetirement *p_head = p_frame.retiring_holders;
	while (p_head) {
		auto *next = p_head->frame_next;
		{
			MutexLock lock(mutex);
			CRASH_COND(!p_head->pending || p_head->retired_thread != Thread::get_caller_id() ||
					p_head->reference.registry != this || p_head->reference.slot != p_frame.reference->slot ||
					p_head->return_frame != p_frame.frame_id || p_frame.frame_id == 0 ||
					p_head->retirement_attempt != records[p_head->reference.slot].exit_attempt ||
					p_head->generation != records[p_head->reference.slot].snapshot.library_generation);
			unlink_holder_locked(p_head);
		}
		// Core-only deleter outside the mutex; its RecordRef may reclaim a record.
		p_head->destroy(p_head);
		p_head = next;
	}
}

static thread_local Error binding_entry_result = OK;
void record_binding_entry_result(Error p_error) {
	binding_entry_result = p_error;
}
const char *get_binding_entry_diagnostic() {
	switch (binding_entry_result) {
		case OK:
			return nullptr;
		case ERR_BUSY:
			return "BINDING_ASSOCIATION_BUSY";
		case ERR_PARAMETER_RANGE_ERROR:
			return "BINDING_ASSOCIATION_LIMIT";
		case ERR_UNAVAILABLE:
			return "ENTRY_CLOSED";
		default:
			return "UNREGISTERED_LIFETIME_ENTRY";
	}
}

[[noreturn]] void stop_lifetime_entry(LifetimeFailure p_failure) {
	const char *message = nullptr;
	size_t length = 0;
	switch (p_failure) {
		case LifetimeFailure::ASSOCIATION_BUSY:
			message = "BINDING_ASSOCIATION_BUSY\n";
			length = sizeof("BINDING_ASSOCIATION_BUSY\n") - 1;
			break;
		case LifetimeFailure::LIMIT:
			message = "BINDING_ASSOCIATION_LIMIT\n";
			length = sizeof("BINDING_ASSOCIATION_LIMIT\n") - 1;
			break;
		case LifetimeFailure::UNREGISTERED:
			message = "UNREGISTERED_LIFETIME_ENTRY\n";
			length = sizeof("UNREGISTERED_LIFETIME_ENTRY\n") - 1;
			break;
	}
#ifdef WINDOWS_ENABLED
	(void)_write(2, message, static_cast<unsigned int>(length));
#else
	const auto written = write(2, message, length);
	(void)written;
#endif
	std::_Exit(86);
}

void require_close_success(Error p_error, CloseRoute p_route) {
	if (p_error == OK) {
		return;
	}
#ifdef TESTS_ENABLED
	TestDeviceExitLibraryClose::observe_close_failure();
#endif
	char message[] = "GDEXTENSION_CLOSE_FAILED error=000 route=0\n";
	const unsigned int error = static_cast<unsigned int>(p_error);
	message[31] = '0' + (error / 100) % 10;
	message[32] = '0' + (error / 10) % 10;
	message[33] = '0' + error % 10;
	message[41] = '0' + static_cast<int>(p_route);
#ifdef WINDOWS_ENABLED
	(void)_write(2, message, sizeof(message) - 1);
#else
	const auto written = write(2, message, sizeof(message) - 1);
	(void)written;
#endif
	std::_Exit(78);
}

void require_level_dispatch_success(Error p_error, int32_t p_level, bool p_initializing) {
	if (p_error == OK) {
		return;
	}
	char message[] = "GDEXTENSION_LEVEL_DISPATCH_FAILED error=000 level=? phase=I\n";
	constexpr size_t error_offset = sizeof("GDEXTENSION_LEVEL_DISPATCH_FAILED error=") - 1;
	constexpr size_t level_offset = sizeof("GDEXTENSION_LEVEL_DISPATCH_FAILED error=000 level=") - 1;
	constexpr size_t phase_offset = sizeof("GDEXTENSION_LEVEL_DISPATCH_FAILED error=000 level=? phase=") - 1;
	const unsigned int error = static_cast<unsigned int>(p_error);
	message[error_offset] = '0' + (error / 100) % 10;
	message[error_offset + 1] = '0' + (error / 10) % 10;
	message[error_offset + 2] = '0' + error % 10;
	message[level_offset] = p_level >= 0 && p_level <= 3 ? '0' + p_level : '?';
	message[phase_offset] = p_initializing ? 'I' : 'D';
#ifdef WINDOWS_ENABLED
	(void)_write(2, message, sizeof(message) - 1);
#else
	const auto written = write(2, message, sizeof(message) - 1);
	(void)written;
#endif
	std::_Exit(79);
}

thread_local RegistrationContext *RegistrationContext::current = nullptr;
thread_local NativeCloseScope *NativeCloseScope::current = nullptr;
NativeCloseScope::NativeCloseScope(const RecordRef &p_reference, const GDExtensionLoader *p_loader) : previous(current), reference(p_reference), loader(p_loader) {
	current = this;
}
NativeCloseScope::~NativeCloseScope() {
	current = previous;
}
void NativeCloseScope::observe(const GDExtensionLoader *p_loader, bool p_returned, Error p_error) {
	NativeCloseScope *scope = current;
	if (!scope || scope->loader != p_loader || !scope->reference.registry) {
		return;
	}
	MutexLock lock(scope->reference.registry->mutex);
	Registry::Record &record = scope->reference.registry->records[scope->reference.slot];
	if (!p_returned) {
		record.os_close_attempted = true;
	} else {
		record.os_close_returned = true;
		record.os_close_error = p_error;
	}
}
void RegistrationContext::enter(const RecordRef &p_reference, bool p_registration_allowed, bool p_admitted) {
	previous = current;
	reference = &p_reference;
	registration_allowed = p_registration_allowed;
	admitted = p_admitted;
	current = this;
}
void RegistrationContext::leave() {
	if (reference) {
		CRASH_COND(current != this);
		if (retiring_holders) {
			reference->registry->finish_retiring_holders(*this);
			retiring_holders = nullptr;
		}
		if (reference->registry && frame_id) {
			MutexLock lock(reference->registry->mutex);
			reference->registry->expire_frame_locked(frame_id);
		}
		current = previous;
		reference = nullptr;
	}
}
CallbackIdentityScope::CallbackIdentityScope(const RecordRef &p_reference) {
	bool allowed = false;
	for (const RegistrationContext *frame = RegistrationContext::current; frame; frame = frame->previous) {
		if (p_reference.is_valid() && frame->reference->registry == p_reference.registry && frame->reference->slot == p_reference.slot) {
			allowed = frame->registration_allowed;
			break;
		}
	}
	context.enter(p_reference, allowed, false);
}
CallbackIdentityScope::~CallbackIdentityScope() {
	context.leave();
}
Error Registry::authenticate_registration(const RecordRef &p_ref) {
	const RegistrationContext *context = RegistrationContext::current;
	if (p_ref.registry != this || !context || !context->registration_allowed ||
			context->reference->registry != this || context->reference->slot != p_ref.slot) {
		return ERR_UNAUTHORIZED;
	}
	MutexLock lock(mutex);
	const Record &record = records[p_ref.slot];
	return record.attached && !record.quiescing && !record.close_started && record.close_error == OK ? OK : ERR_UNAVAILABLE;
}

struct CleanupAction::Storage : HolderRetirement {
	SafeRefCount count;
	const uint64_t object_id;
	const uint64_t thread_id;
	const uint64_t action_id;
	uint64_t reserved_frame = 0;
	bool active = false; // Protected by the generation mutex.
	Storage(RecordRef &&p_reference, uint64_t p_object, uint64_t p_thread, uint64_t p_action, CleanupPhase p_phase) :
			HolderRetirement(std::move(p_reference)), object_id(p_object), thread_id(p_thread), action_id(p_action) {
		count.init();
		cleanup = true;
		phase = p_phase;
		destroy = [](HolderRetirement *p_cell) { memdelete(static_cast<Storage *>(p_cell)); };
	}
};
void BindingLifetime::bind_core_action(uint64_t p_object) {
	if (!storage) {
		return;
	}
	auto *registry = storage->reference.registry;
	MutexLock lock(registry->mutex);
	CRASH_COND(!p_object || storage->pending || storage->source_action || !storage->core_owned);
	for (auto *holder = registry->records[storage->reference.slot].holders; holder; holder = holder->record_next) {
		if (!holder->cleanup) {
			continue;
		}
		auto *action = static_cast<CleanupAction::Storage *>(holder);
		if (action->object_id == p_object) {
			storage->source_object = storage->core_object = p_object;
			storage->source_action = storage->terminal_action = action->serial;
			return;
		}
	}
	stop_lifetime_entry(LifetimeFailure::UNREGISTERED);
}
void CleanupAction::record_core_disposal() {
	if (!storage) {
		return;
	}
	auto *registry = storage->reference.registry;
	MutexLock lock(registry->mutex);
	CRASH_COND(storage->pending || storage->generation != registry->records[storage->reference.slot].snapshot.library_generation);
	if (storage->terminal_action != storage->action_id || storage->core_object != storage->object_id) {
		stop_lifetime_entry(LifetimeFailure::UNREGISTERED);
	}
	storage->core_disposal_frame = 0;
	storage->core_disposal_returned = true;
	storage->core_disposal_thread = Thread::get_caller_id();
	for (auto *frame = RegistrationContext::current; frame; frame = frame->previous) {
		if (frame->admitted && frame->reference->registry == registry && frame->reference->slot == storage->reference.slot) {
			storage->core_disposal_frame = frame->frame_id;
		}
	}
}
CleanupAction::~CleanupAction() {
	if (storage && storage->count.unref()) {
		if (storage->terminal_action != storage->action_id || storage->core_object != storage->object_id) {
			stop_lifetime_entry(LifetimeFailure::UNREGISTERED);
		}
		storage->reference.registry->retire_binding_holder(storage);
	}
}
CleanupPhase CleanupAction::get_phase() const {
	return storage ? storage->phase : CleanupPhase::UNSCHEDULED;
}
bool CleanupAction::is_scheduled() const {
	if (!storage) {
		return false;
	}
	Registry *registry = storage->reference.registry;
	MutexLock lock(registry->mutex);
	return storage->scheduled_action != 0;
}
CleanupScope::CleanupScope(const CleanupAction &p_action, const RecordRef &p_generation, uint64_t p_object_id, uint64_t p_action_id, bool p_comparison_only) : comparison_only(p_comparison_only) {
	auto *candidate = p_action.storage;
	if (!candidate || p_generation.registry != candidate->reference.registry || p_generation.slot != candidate->reference.slot ||
			p_object_id != candidate->object_id || p_action_id != candidate->action_id ||
			candidate->terminal_action != candidate->action_id || candidate->core_object != candidate->object_id ||
			Thread::get_caller_id() != candidate->thread_id) {
		return;
	}
	Registry *registry = candidate->reference.registry;
	MutexLock lock(registry->mutex);
	auto &record = registry->records[candidate->reference.slot];
	if (!record.attached || record.close_started || record.close_error != OK) {
		error = ERR_UNAVAILABLE;
		return;
	}
	if (candidate->phase != CleanupPhase::UNSCHEDULED) {
		// Scheduled actions need an original guarded level issuer, not an enum.
		error = ERR_UNAUTHORIZED;
		return;
	}
	if (record.quiescing && candidate->scheduled_action) {
		const auto *issuer = RegistrationContext::current;
		if (!issuer || !issuer->admitted || issuer->reference->registry != registry || issuer->reference->slot != candidate->reference.slot ||
				issuer->level_phase != LevelPhase::DEINITIALIZE || issuer->level < 0 || issuer->level > 3) {
			error = ERR_UNAUTHORIZED;
			return;
		}
		const auto &level = record.levels[issuer->level];
		if (!((issuer->terminal_stage == 1 && candidate->scheduled_action == level.user_action.serial) ||
					(issuer->terminal_stage == 2 && candidate->scheduled_action == level.binding_action.serial))) {
			error = ERR_UNAUTHORIZED;
			return;
		}
	}
	if (candidate->active) {
		error = ERR_BUSY;
		return;
	}
	// The action reserved its holder identity while Open; cleanup entry consumes
	// that existing identity and must not issue fresh quota at callback time.
	// Storage, private reference and one active slot were reserved while Open.
	// Mandatory cleanup performs no allocation and consumes no new quota.
	CRASH_COND(!candidate->count.ref());
	candidate->active = true;
	record.cleanup_reservations--;
	record.active_callbacks++;
	record.active_cleanup_by_phase[static_cast<uint8_t>(candidate->phase)]++;
	storage = candidate;
	context.enter(storage->reference, false);
	context.frame_id = candidate->reserved_frame;
	error = OK;
}
CleanupScope::~CleanupScope() {
	if (storage && (storage->terminal_action != storage->action_id || storage->core_object != storage->object_id)) {
		stop_lifetime_entry(LifetimeFailure::UNREGISTERED);
	}
	context.leave();
	if (!storage) {
		return;
	}
	{
		MutexLock lock(storage->reference.registry->mutex);
		auto &record = storage->reference.registry->records[storage->reference.slot];
		record.active_callbacks--;
		record.active_cleanup_by_phase[static_cast<uint8_t>(storage->phase)]--;
		record.cleanup_reservations++;
		storage->active = false;
		if (!comparison_only) {
			storage->core_disposal_returned = true;
			storage->core_disposal_thread = Thread::get_caller_id();
		}
	}
	if (storage->count.unref()) {
		storage->reference.registry->retire_binding_holder(storage);
	}
}
Error Registry::register_cleanup_action(const RecordRef &p_ref, uint64_t p_object_id, uint64_t p_thread_id, uint64_t p_action_id, CleanupAction &r_action, CleanupPhase p_phase) {
	if (p_ref.registry != this || r_action.storage || !p_object_id || !p_thread_id || !p_action_id) {
		return ERR_INVALID_DATA;
	}
	if (static_cast<uint8_t>(p_phase) > static_cast<uint8_t>(CleanupPhase::FUTURE_LEVEL)) {
		return ERR_INVALID_PARAMETER;
	}
	MutexLock lock(mutex);
	auto &record = records[p_ref.slot];
	if (p_phase != CleanupPhase::UNSCHEDULED) {
		return ERR_UNAUTHORIZED; // Typed level actions are issued only at bootstrap.
	}
	if (!record.attached || record.quiescing || record.close_started || record.close_error != OK) {
		return ERR_UNAVAILABLE;
	}
	if (record.private_refs >= private_limit || record.binding_holders == UINT32_MAX ||
			record.active_callbacks >= UINT32_MAX - record.cleanup_reservations || holder_serials.exhausted || frame_serials.exhausted) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	record.private_refs++;
	record.binding_holders++;
	record.cleanup_reservations++;
	record.cleanup_by_phase[static_cast<uint8_t>(p_phase)]++;
	r_action.storage = memnew(CleanupAction::Storage(RecordRef(this, p_ref.slot), p_object_id, p_thread_id, p_action_id, p_phase));
	holder_serials.take(r_action.storage->serial);
	frame_serials.take(r_action.storage->reserved_frame);
	r_action.storage->generation = record.snapshot.library_generation;
	r_action.storage->core_object = p_object_id;
	r_action.storage->terminal_action = p_action_id;
	r_action.storage->record_next = record.holders;
	record.holders = r_action.storage;
	return OK;
}

Error Registry::mark_core_object_action(const RecordRef &p_ref, uint64_t p_object_id, CleanupAction &r_action) {
	if (p_ref.registry != this || !r_action.storage || !p_object_id) {
		return ERR_INVALID_DATA;
	}
	MutexLock lock(mutex);
	auto *action = r_action.storage;
	const auto &record = records[p_ref.slot];
	if (action->reference.slot != p_ref.slot || action->object_id != p_object_id || action->action_id != p_object_id ||
			action->generation != record.snapshot.library_generation || action->thread_id != record.owner_thread || action->pending || action->scheduled_action) {
		return ERR_INVALID_DATA;
	}
	action->core_object_action = true;
	return OK;
}

bool Registry::core_admission_matches_locked(const CoreEvidenceAdmission &p_admission) const {
	if (p_admission.reference.registry != this) {
		return false;
	}
	const Record &record = records[p_admission.reference.slot];
	return record.occupied && record.snapshot.library_generation == p_admission.generation &&
			record.exit_attempt == p_admission.attempt && record.evidence_attempt == p_admission.attempt &&
			record.evidence_serial == p_admission.publication_serial && record.evidence_route == p_admission.route &&
			record.preflight_sealed && record.preflight_route == p_admission.route;
}

Error Registry::prepare_core_evidence(const RecordRef &p_ref, CloseRoute p_route, CoreEvidenceAdmission &r_admission) {
	if (p_ref.registry != this || r_admission.reference.is_valid()) {
		return ERR_INVALID_DATA;
	}
	MutexLock lock(mutex);
	Record &record = records[p_ref.slot];
	if (!record.preflight_sealed || record.preflight_route != p_route) {
		return ERR_INVALID_DATA;
	}
	if (!physical_can_close_locked(p_ref.slot)) {
		return ERR_BUSY;
	}
	if (record.private_refs >= private_limit) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	if (!record.evidence_serial) {
		if (!evidence_serials.take(record.evidence_serial)) {
			return ERR_PARAMETER_RANGE_ERROR;
		}
		record.evidence_attempt = record.exit_attempt;
		record.evidence_route = p_route;
	} else if (record.evidence_attempt != record.exit_attempt || record.evidence_route != p_route) {
		return ERR_INVALID_DATA;
	}
	record.private_refs++;
	r_admission.reference = RecordRef(this, p_ref.slot);
	r_admission.generation = record.snapshot.library_generation;
	r_admission.attempt = record.exit_attempt;
	r_admission.publication_serial = record.evidence_serial;
	r_admission.route = p_route;
	return OK;
}

Error Registry::register_core_event_export(const CoreEvidenceAdmission &p_admission, CoreExportGrant &r_export) {
	MutexLock lock(mutex);
	if (!core_admission_matches_locked(p_admission)) {
		return ERR_UNAUTHORIZED;
	}
	const Record &record = records[p_admission.reference.slot];
	if (!physical_can_close_locked(p_admission.reference.slot)) {
		return ERR_BUSY;
	}
	// Capture only actual core source facts. No addresses, DSO-owned storage,
	// inferred G2 result, future OS result or caller-provided payload is copied.
	// Fixed-width little-endian fields make padding irrelevant to the digest.
	r_export = {};
	auto put32 = [&](uint32_t p_value) { r_export.size += encode_uint32(p_value, r_export.bytes + r_export.size); };
	auto put64 = [&](uint64_t p_value) { r_export.size += encode_uint64(p_value, r_export.bytes + r_export.size); };
	put32(1); // Private core record projection revision, not ANGECAP wire version.
	put64(instance);
	put64(record.snapshot.library_generation);
	put64(record.exit_attempt);
	put64(record.evidence_serial);
	put32(static_cast<uint32_t>(record.evidence_route));
	put32(record.snapshot.loader_kind);
	put32(record.snapshot.loader_initialize_returned);
	put32(record.snapshot.loader_initialize_error);
	put32(record.bootstrap_tuple_observed);
	put32(record.snapshot.guarded_qualified);
	for (const LevelState &level : record.levels) {
		put64(level.initialize_frame);
		put64(level.deinitialize_frame);
		put32(level.initialize_returned);
		put32(level.deinitialize_returned);
	}
	put32(record.active_callbacks);
	put32(record.binding_holders);
	put32(record.return_pending_holders);
	put32(record.cleanup_reservations);
	for (uint32_t i = 0; i < 4; i++) {
		put32(record.cleanup_by_phase[i]);
		put32(record.active_cleanup_by_phase[i]);
		put32(record.retiring_cleanup_by_phase[i]);
	}
	put32(record.quiescing);
	static_assert(CORE_EVIDENCE_BYTES >= 224);
	return OK;
}

Error Registry::commit_core_evidence(const CoreEvidenceAdmission &p_admission, PublicationAck &r_ack) {
	if (r_ack.reference.is_valid()) {
		return ERR_INVALID_DATA;
	}
	CoreExportGrant captured;
	Error error = register_core_event_export(p_admission, captured);
	if (error != OK) {
		return error;
	}
	uint8_t digest[32];
	// Synchronous copy/hash has no Registry lock or DSO callback. All fallible
	// work precedes publication; the lock below revalidates the source identity.
	error = CryptoCore::sha256(captured.bytes, captured.size, digest);
	if (error != OK) {
		return error;
	}
	MutexLock lock(mutex);
	if (!core_admission_matches_locked(p_admission) || !physical_can_close_locked(p_admission.reference.slot)) {
		return ERR_UNAUTHORIZED;
	}
	Record &record = records[p_admission.reference.slot];
	if (record.private_refs >= private_limit) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	if (record.evidence_committed) {
		// Lost delivery may retrieve the same product, never overwrite it.
		if (record.evidence.size != captured.size || memcmp(record.evidence.bytes, captured.bytes, captured.size) ||
				memcmp(record.evidence_digest, digest, sizeof(digest))) {
			return ERR_INVALID_DATA;
		}
	} else {
		record.evidence = captured;
		memcpy(record.evidence_digest, digest, sizeof(digest));
		record.evidence_revision = 1;
		record.evidence_member_count = 1; // Exact single original core record export.
		record.evidence_committed = true;
	}
	// Bytes, seal revision, exact membership and prebuilt ack become visible
	// under this one lock. Delivery is not a second source event or self-ack.
	record.private_refs++;
	r_ack.reference = RecordRef(this, p_admission.reference.slot);
	r_ack.generation = p_admission.generation;
	r_ack.attempt = p_admission.attempt;
	r_ack.publication_serial = p_admission.publication_serial;
	r_ack.route = p_admission.route;
	memcpy(r_ack.digest, digest, sizeof(digest));
	return OK;
}

Error Registry::consume_core_ack(const RecordRef &p_ref, const PublicationAck &p_ack) {
	if (p_ref.registry != this || p_ack.reference.registry != this || p_ack.reference.slot != p_ref.slot) {
		return ERR_INVALID_DATA;
	}
	MutexLock lock(mutex);
	Record &record = records[p_ref.slot];
	if (!record.evidence_committed || record.evidence_serial != p_ack.publication_serial || record.exit_attempt != p_ack.attempt ||
			record.evidence_attempt != p_ack.attempt || record.evidence_route != p_ack.route || record.preflight_route != p_ack.route ||
			record.snapshot.library_generation != p_ack.generation || memcmp(record.evidence_digest, p_ack.digest, sizeof(p_ack.digest))) {
		return ERR_INVALID_DATA;
	}
	if (!physical_can_close_locked(p_ref.slot)) {
		return ERR_BUSY;
	}
	record.evidence_ack_received = true;
	return OK;
}

Error Registry::prepare_selected_preclose(const RecordRef &p_ref, CloseRoute p_route) {
	CoreEvidenceAdmission admission;
	Error error = prepare_core_evidence(p_ref, p_route, admission);
	if (error != OK) {
		return error;
	}
	PublicationAck ack;
	error = commit_core_evidence(admission, ack);
	return error == OK ? consume_core_ack(p_ref, ack) : error;
}

Error Registry::preflight_exit(const RecordRef &p_ref, CloseRoute p_route) {
	if (p_ref.registry != this) {
		return ERR_INVALID_DATA;
	}
	switch (p_route) {
		case CloseRoute::INIT_ABORT:
		case CloseRoute::RELOAD:
		case CloseRoute::LAST_REF:
		case CloseRoute::EXPLICIT:
		case CloseRoute::SHUTDOWN:
		case CloseRoute::TEST_CLEANUP:
		case CloseRoute::LEVEL_DEINIT:
		case CloseRoute::SETUP_ABORT:
		case CloseRoute::DISPLAY_ABORT:
			break;
		default:
			return ERR_INVALID_PARAMETER;
	}
	MutexLock lock(mutex);
	Record &record = records[p_ref.slot];
	if (!record.attached || record.close_started || record.close_error != OK) {
		return ERR_UNAVAILABLE;
	}
	if (record.owner_thread != Thread::get_caller_id()) {
		return ERR_UNAUTHORIZED;
	}
	if (record.preflight_sealed && record.preflight_route != p_route) {
		return ERR_INVALID_DATA;
	}
	// Validate capacity before publishing the seal. The fixed Record already
	// reserves byte storage; two independent core references serve copy + ack.
	if (private_limit < 2 || record.private_refs > private_limit - 2 || (!record.evidence_serial && evidence_serials.exhausted)) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	if (!record.preflight_sealed) {
		if (!record.quiescing && !exit_attempts.take(record.exit_attempt)) {
			return ERR_PARAMETER_RANGE_ERROR;
		}
		for (HolderRetirement *holder = record.holders; holder; holder = holder->record_next) {
			if (holder->pending && holder->retirement_attempt == 0) {
				holder->retirement_attempt = record.exit_attempt;
			}
		}
		record.preflight_sealed = true;
		record.preflight_route = p_route;
		record.quiescing = true;
	}
	// Unscheduled holders cannot be assumed to have a future terminal action.
	// Busy retains the original admission seal/attempt for an identical retry.
	return record.active_callbacks == 0 && record.binding_holders == 0 ? OK : ERR_BUSY;
}

Error Registry::begin_quiesce(const RecordRef &p_ref) {
	if (p_ref.registry != this) {
		return ERR_INVALID_DATA;
	}
	MutexLock lock(mutex);
	Record &record = records[p_ref.slot];
	if (!record.attached || record.close_started || record.close_error != OK) {
		return ERR_UNAVAILABLE;
	}
	// The same lock admits callbacks, holders and complete-slot publication.
	// A Busy observation never rolls this transition back to Open.
	if (!record.quiescing && !exit_attempts.take(record.exit_attempt)) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	for (HolderRetirement *holder = record.holders; holder; holder = holder->record_next) {
		if (holder->pending && holder->retirement_attempt == 0) {
			holder->retirement_attempt = record.exit_attempt;
		}
	}
	record.quiescing = true;
	return record.active_callbacks == 0 && record.binding_holders == 0 ? OK : ERR_BUSY;
}

Error Registry::begin_close(const RecordRef &p_ref, CloseRoute p_route) {
	if (p_ref.registry != this) {
		return ERR_INVALID_DATA;
	}
	MutexLock lock(mutex);
	Record &record = records[p_ref.slot];
	// G3 cannot be entered by bypassing the G1 admission seal. A caller that
	// has not first quiesced the generation must observe Busy without mutating
	// close_started, so the normal retry path can still perform the full gate.
	if (!record.attached || !record.quiescing || record.close_started || !physical_can_close_locked(p_ref.slot) ||
			(!record.preflight_sealed || record.preflight_route != p_route || !record.evidence_committed || !record.evidence_ack_received)) {
		return ERR_BUSY;
	}
	record.close_route = p_route;
	record.close_source = record.snapshot.loader_kind;
	record.close_started = true;
	return OK;
}

bool Registry::physical_can_close_locked(uint32_t p_slot) const {
	const Record &record = records[p_slot];
	for (const auto &ref : device_refs) {
		if (ref.serial && ref.library == p_slot) {
			return false;
		}
	}
	for (uint32_t i = 0; i < 4; i++) {
		if (record.cleanup_by_phase[i] || record.active_cleanup_by_phase[i] || record.retiring_cleanup_by_phase[i]) {
			return false;
		}
	}
	if (record.guard_enrollment == GuardEnrollment::QUALIFIED) {
		if (!record.control_complete) {
			return false;
		}
		for (const auto &level : record.levels) {
			if (level.initialized && !level.finalized) {
				return false;
			}
		}
	}
	return record.attached && record.quiescing && !record.close_started && record.active_callbacks == 0 &&
			record.binding_holders == 0 && record.return_pending_holders == 0 && record.cleanup_reservations == 0 &&
			record.close_error == OK;
}

void Registry::observe_close(const RecordRef &p_ref, Error p_error) {
	if (p_ref.registry != this) {
		return;
	}
	MutexLock lock(mutex);
	Record &record = records[p_ref.slot];
	record.close_returned = true;
	record.close_error = p_error;
}

Error Registry::resolve_core_close_return(const RecordRef &p_ref, CoreCloseReturnFact &r_fact) {
	if (p_ref.registry != this) {
		return ERR_INVALID_DATA;
	}
	MutexLock lock(mutex);
	const Record &record = records[p_ref.slot];
	// RecordRef pins this slot after the wrapper detaches.
	if (!record.occupied) {
		return ERR_INVALID_DATA;
	}
	if (!record.close_returned) {
		return ERR_BUSY;
	}
	CoreCloseReturnFact fact;
	fact.library_generation = record.snapshot.library_generation;
	fact.exit_attempt = record.exit_attempt;
	fact.route = record.close_route;
	fact.close_returned = record.close_returned;
	fact.close_error = Error(record.close_error);
	fact.os_close_attempted = record.os_close_attempted;
	fact.os_close_returned = record.os_close_returned;
	fact.os_close_error = record.os_close_error;
	r_fact = fact;
	return OK;
}

static_assert(sizeof(GDExtensionDeviceExitAbi) == 16);
static_assert(sizeof(GDExtensionDeviceExitLibraryCapability) == 16);
static_assert(sizeof(GDExtensionDeviceExitLibraryViewRef) == 16);
static_assert(sizeof(GDExtensionDeviceExitLibrarySnapshot) == 48);
static_assert(offsetof(GDExtensionDeviceExitLibrarySnapshot, library_generation) == 16);
static_assert(offsetof(GDExtensionDeviceExitLibrarySnapshot, loader_kind) == 24);
static_assert(offsetof(GDExtensionDeviceExitLibrarySnapshot, wrapper_association) == 28);
static_assert(offsetof(GDExtensionDeviceExitLibrarySnapshot, loader_initialize_returned) == 32);
static_assert(offsetof(GDExtensionDeviceExitLibrarySnapshot, loader_initialize_error) == 36);
static_assert(offsetof(GDExtensionDeviceExitLibrarySnapshot, guarded_qualified) == 40);
static_assert(offsetof(GDExtensionDeviceExitLibrarySnapshot, reserved) == 44);

template <typename T>
static bool aligned(const T *p_pointer) {
	return p_pointer && (reinterpret_cast<uintptr_t>(p_pointer) % alignof(T)) == 0;
}
static bool valid_abi(const GDExtensionDeviceExitAbi *p_abi, uint32_t p_size = 16) {
	return aligned(p_abi) && p_abi->abi_version == 1 && p_abi->struct_size == p_size && p_abi->reserved == 0;
}

bool Registry::Sequence::take(uint64_t &r_value) {
	if (exhausted) {
		return false;
	}
	r_value = next;
	if (next == UINT64_MAX) {
		exhausted = true;
	} else {
		next++;
	}
	return true;
}

bool Registry::issue_instance(uint64_t &r_instance) {
	struct Issuer {
		BinaryMutex mutex;
		Sequence sequence;
	};
	alignas(Issuer) static unsigned char storage[sizeof(Issuer)];
	static Issuer *issuer = ::new (static_cast<void *>(storage)) Issuer;
	MutexLock lock(issuer->mutex);
#ifdef TESTS_ENABLED
	// Isolated process only, before its first identity. Never reset an issuer
	// after issuance or revive exhausted identities.
	if (issuer->sequence.next == 1 && TestDeviceExitStartup::instance_exhaustion_requested()) {
		issuer->sequence.next = UINT64_MAX;
	}
#endif
	return issuer->sequence.take(r_instance);
}

Registry &Registry::get_singleton() {
	// Never destroyed: late metadata releases must not touch torn-down services.
	alignas(Registry) static unsigned char storage[sizeof(Registry)];
	static Registry *registry = ::new (static_cast<void *>(storage)) Registry;
	return *registry;
}

RecordRef::RecordRef(RecordRef &&p_other) noexcept : registry(std::exchange(p_other.registry, nullptr)), slot(p_other.slot) {}
RecordRef &RecordRef::operator=(RecordRef &&p_other) noexcept {
	if (this != &p_other) {
		reset();
		registry = std::exchange(p_other.registry, nullptr);
		slot = p_other.slot;
	}
	return *this;
}
RecordRef::~RecordRef() {
	reset();
}
void RecordRef::reset() {
	if (Registry *owner = std::exchange(registry, nullptr)) {
		owner->release_private(slot);
	}
}

BindingPublicationScope::BindingPublicationScope(const RecordRef &p_reference) : registry(p_reference.registry) {
	if (!registry) {
		return;
	}
	registry->mutex.lock();
	const auto &record = registry->records[p_reference.slot];
	error = record.attached && !record.quiescing && !record.close_started && record.close_error == OK ? OK : ERR_BUSY;
}
BindingPublicationScope::~BindingPublicationScope() {
	if (registry) {
		registry->mutex.unlock();
	}
}

uint64_t RecordRef::get_generation() const {
	if (!registry) {
		return 0;
	}
	MutexLock lock(registry->mutex);
	return registry->records[slot].snapshot.library_generation;
}

CallbackScope::CallbackScope(const RecordRef &p_reference) {
	Registry *registry = p_reference.registry;
	if (!registry) {
		return;
	}
	MutexLock lock(registry->mutex);
	auto &record = registry->records[p_reference.slot];
	if (record.guard_enrollment == GuardEnrollment::PENDING_BOOTSTRAP || record.guard_enrollment == GuardEnrollment::REVOKED) {
		error = ERR_UNAVAILABLE;
		return;
	}
	if (!record.attached || record.quiescing || record.close_started || record.close_error != OK) {
		error = ERR_BUSY;
		return;
	}
	if (record.private_refs >= registry->private_limit || record.active_callbacks >= UINT32_MAX - record.cleanup_reservations || registry->frame_serials.exhausted) {
		error = ERR_PARAMETER_RANGE_ERROR;
		return;
	}
	record.private_refs++;
	record.active_callbacks++;
	reference = RecordRef(registry, p_reference.slot);
	context.enter(reference, true);
	registry->frame_serials.take(context.frame_id);
	error = OK;
}
CallbackScope::~CallbackScope() {
	context.leave();
	if (error != OK) {
		return;
	}
	{
		MutexLock lock(reference.registry->mutex);
		reference.registry->records[reference.slot].active_callbacks--;
	}
	// The core-only reference outlives the DSO return acknowledgment.
	reference.reset();
}

LevelScope::LevelScope(const RecordRef &p_reference, GDExtension *p_owner, const GDExtensionInitialization &p_initialization, int32_t p_level, bool p_initializing) :
		level(p_level), initializing(p_initializing), error(ERR_INVALID_DATA) {
	Registry *registry = p_reference.registry;
	if (!registry || level < 0 || level >= 4) {
		return;
	}
	MutexLock lock(registry->mutex);
	auto &record = registry->records[p_reference.slot];
	const auto &original = record.original_initialization;
	if (!record.bootstrap_tuple_observed || record.original_owner != p_owner || original.initialize != p_initialization.initialize ||
			original.deinitialize != p_initialization.deinitialize || original.userdata != p_initialization.userdata ||
			original.minimum_initialization_level != p_initialization.minimum_initialization_level) {
		error = ERR_UNAUTHORIZED;
		return;
	}
	guarded = record.guard_enrollment == GuardEnrollment::QUALIFIED;
	if (!record.attached || record.close_started || record.close_error != OK) {
		error = ERR_BUSY;
		return;
	}
	auto &state = record.levels[level];
	if (guarded) {
		if (record.owner_thread != Thread::get_caller_id() || !(record.guard.level_bitmap & (1u << level))) {
			error = ERR_UNAUTHORIZED;
			return;
		}
		if (initializing) {
			if ((state.user_action.services | state.binding_action.services) & ~registry->live_services) {
				error = ERR_UNAVAILABLE;
				return;
			}
			if (record.quiescing || state.initialized || state.initialize_frame) {
				error = ERR_BUSY;
				return;
			}
		} else {
			if (!record.preflight_accepted || !record.quiescing || !state.initialized || state.finalized || record.active_callbacks) {
				error = ERR_BUSY;
				return;
			}
			for (int i = level + 1; i < 4; i++) {
				if (record.levels[i].initialized && !record.levels[i].finalized) {
					error = ERR_INVALID_DATA;
					return;
				}
			}
		}
	} else if (record.quiescing || record.guard_enrollment == GuardEnrollment::PENDING_BOOTSTRAP || record.guard_enrollment == GuardEnrollment::REVOKED) {
		error = ERR_BUSY;
		return;
	}
	if ((!guarded && (record.private_refs >= registry->private_limit || registry->frame_serials.exhausted)) || record.active_callbacks >= UINT32_MAX - record.cleanup_reservations) {
		error = ERR_PARAMETER_RANGE_ERROR;
		return;
	}
	if (!guarded) {
		record.private_refs++;
	}
	record.active_callbacks++;
	reference = RecordRef(registry, p_reference.slot);
	context.enter(reference, initializing || !guarded);
	if (guarded) {
		auto &reserved_frame = initializing ? state.reserved_initialize_frame : state.reserved_deinitialize_frame;
		CRASH_COND(!reserved_frame);
		context.frame_id = reserved_frame;
		reserved_frame = 0;
	} else {
		registry->frame_serials.take(context.frame_id);
	}
	context.level = level;
	context.level_phase = initializing ? LevelPhase::INITIALIZE : LevelPhase::DEINITIALIZE;
	if (initializing) {
		state.initialize_frame = context.frame_id;
		state.initialize_returned = false;
	} else {
		state.deinitialize_frame = context.frame_id;
		state.deinitialize_returned = false;
	}
	error = OK;
}
LevelScope::~LevelScope() {
	if (error != OK) {
		return;
	}
	Registry *registry = reference.registry;
	// A void wrapper cannot quietly return a partially completed typed phase.
	if (guarded && (!context.ack || (!initializing && context.terminal_stage != 3))) {
		require_level_dispatch_success(ERR_UNAVAILABLE, level, initializing);
	}
	context.leave(); // Drain exact pending holders only after actual DSO return.
	{
		MutexLock lock(registry->mutex);
		auto &record = registry->records[reference.slot];
		auto &state = record.levels[level];
		if (initializing) {
			state.initialize_returned = true;
			state.initialized = guarded;
			if (guarded && level == GDEXTENSION_INITIALIZATION_SCENE && record.scene_ready_state == Registry::Record::SceneReadyState::REGISTERED) {
				record.scene_ready_state = Registry::Record::SceneReadyState::PENDING;
			}
		} else {
			state.deinitialize_returned = true;
			state.finalized = guarded;
			if (guarded) {
				state.user_action.returned = state.binding_action.returned = true;
				state.user_action.wrapper = state.binding_action.wrapper = nullptr;
				state.user_action.terminator = state.binding_action.terminator = nullptr;
				state.user_action.userdata = state.binding_action.userdata = nullptr;
				bool complete = true;
				for (const auto &other : record.levels) {
					complete &= !other.initialized || other.finalized;
				}
				if (complete) {
					registry->clear_guard_control_locked(record);
				}
			}
		}
		record.active_callbacks--;
	}
	reference.reset();
}

thread_local BootstrapScope *BootstrapScope::current = nullptr;
BootstrapScope::BootstrapScope(RecordRef &&p_reference, GDExtension *p_expected) : previous(current), reference(std::move(p_reference)), expected(p_expected) {
	MutexLock lock(reference.registry->mutex);
	reference.registry->records[reference.slot].scope_live = true;
	reference.registry->records[reference.slot].active_callbacks++;
	current = this;
	context.enter(reference, true);
	context.frame_id = reference.registry->records[reference.slot].bootstrap_frame;
}
Error BootstrapScope::observe_tuple(const GDExtensionInitialization &p_initialization, Error p_error) {
	MutexLock lock(reference.registry->mutex);
	auto &record = reference.registry->records[reference.slot];
	if (current != this || RegistrationContext::current != &context || record.original_owner != expected || record.owner_thread != Thread::get_caller_id()) {
		return ERR_UNAUTHORIZED;
	}
	if (record.guard_enrollment == GuardEnrollment::PENDING_BOOTSTRAP) {
		const auto &wanted = record.guard.expected_initialization;
		if (p_error != OK || wanted.initialize != p_initialization.initialize || wanted.deinitialize != p_initialization.deinitialize ||
				wanted.userdata != p_initialization.userdata || wanted.minimum_initialization_level != p_initialization.minimum_initialization_level) {
			record.guard_enrollment = GuardEnrollment::REVOKED;
			return p_error == OK ? ERR_INVALID_DATA : p_error;
		}
		record.guard_enrollment = GuardEnrollment::QUALIFIED;
	}
	if (p_error != OK) {
		return p_error;
	}
	record.original_initialization = p_initialization;
	record.bootstrap_tuple_observed = true;
	// Public guarded_qualified remains zero until the complete public census
	// and generated binding integration are activated reciprocally.
	return OK;
}
BootstrapScope::~BootstrapScope() {
	context.leave();
	current = previous;
	{
		MutexLock lock(reference.registry->mutex);
		reference.registry->records[reference.slot].scope_live = false;
		auto &record = reference.registry->records[reference.slot];
		record.active_callbacks--;
		if (record.guard_enrollment == GuardEnrollment::REVOKED && record.active_callbacks == 0 && record.binding_holders == 0) {
			reference.registry->revoke_guard_locked(record);
		}
	}
	// Revoke TLS/scope authority before the last private reference can reclaim.
	reference.reset();
}

Error Registry::begin_cycle(uint64_t &r_cycle) {
	r_cycle = 0;
#ifdef TESTS_ENABLED
	const Error injected = TestDeviceExitStartup::consume_begin_error();
	if (injected != OK) {
		return injected;
	}
#endif
	MutexLock lock(mutex);
	if (phase != Phase::FINALIZED) {
		return ERR_BUSY;
	}
	if (!issue_instance(instance)) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	device_epochs = Sequence{};
	device_ref_serials = Sequence{};
	phase = Phase::RUNNING;
	engine_cycle_ended = false;
	r_cycle = instance;
	return OK;
}

bool Registry::begin_finalizing(uint64_t p_cycle) {
	MutexLock lock(mutex);
	if (p_cycle == 0 || p_cycle != instance || phase != Phase::RUNNING) {
		return false;
	}
	phase = Phase::FINALIZING;
	return true;
}

void Registry::finish_engine_cycle(uint64_t p_cycle) {
	MutexLock lock(mutex);
	if (p_cycle == 0 || p_cycle != instance || phase != Phase::FINALIZING) {
		return;
	}
	engine_cycle_ended = true;
	try_finalize();
}
void Registry::try_finalize() {
	for (const auto &device : device_records) {
		if (device.epoch) {
			return;
		}
	}
	for (const auto &ref : device_refs) {
		if (ref.serial) {
			return;
		}
	}
	if (phase != Phase::FINALIZING || !engine_cycle_ended) {
		return;
	}
	for (uint32_t i = 0; i < record_limit; i++) {
		if (records[i].occupied) {
			return;
		}
	}
	phase = Phase::FINALIZED;
}
void Registry::reclaim(uint32_t p_slot) {
	Record &record = records[p_slot];
	if (!record.attached && !record.scope_live && record.private_refs == 0 && record.external_refs == 0) {
		for (BindingOwner &owner : binding_owners) {
			if (owner.record == p_slot) {
				owner.record = INVALID_SLOT;
			}
		}
		record = Record();
		try_finalize();
	}
}
void Registry::release_private(uint32_t p_slot) {
	MutexLock lock(mutex);
	records[p_slot].private_refs--;
	reclaim(p_slot);
}

Error Registry::reserve_open(uint32_t p_kind, RecordRef &r_wrapper, RecordRef &r_scope, GDExtension *p_owner) {
	if (r_wrapper.is_valid() || r_scope.is_valid()) {
		return ERR_BUSY;
	}
	MutexLock lock(mutex);
	if (phase != Phase::RUNNING) {
		return ERR_BUSY;
	}
	if (p_kind < 1 || p_kind > 3) {
		return ERR_INVALID_PARAMETER;
	}
	BindingOwner *owner_slot = nullptr;
	if (p_owner) {
		for (BindingOwner &owner : binding_owners) {
			if (owner.token == p_owner) {
				owner_slot = &owner;
				break;
			}
			if (!owner.token && !owner_slot) {
				owner_slot = &owner;
			}
		}
		if (!owner_slot) {
			std::memcpy(reservation_failure, "binding owner capacity exhausted", sizeof("binding owner capacity exhausted"));
			return ERR_PARAMETER_RANGE_ERROR;
		}
	}
	uint32_t slot = INVALID_SLOT;
	for (uint32_t i = 0; i < record_limit; i++) {
		if (!records[i].occupied) {
			slot = i;
			break;
		}
	}
	if (slot == INVALID_SLOT || private_limit < 2 || generations.exhausted || serials.exhausted || frame_serials.exhausted) {
		std::memcpy(reservation_failure, "library reservation capacity or issuer exhausted", sizeof("library reservation capacity or issuer exhausted"));
		return ERR_PARAMETER_RANGE_ERROR;
	}
	Record &record = records[slot];
	record = Record();
	record.occupied = true;
	record.private_refs = 2;
	record.snapshot.abi = { 1, 48, 0 };
	generations.take(record.snapshot.library_generation);
	serials.take(record.authority_serial);
	record.snapshot.loader_kind = p_kind;
	record.original_owner = p_owner;
	record.owner_thread = Thread::get_caller_id();
	frame_serials.take(record.bootstrap_frame);
	if (owner_slot) {
		owner_slot->token = p_owner;
		owner_slot->record = slot;
	}
	r_wrapper = RecordRef(this, slot);
	r_scope = RecordRef(this, slot);
	return OK;
}

Error Registry::retain_binding_owner(void *p_token, RecordRef &r_ref) {
	if (r_ref.is_valid()) {
		return ERR_INVALID_DATA;
	}
	MutexLock lock(mutex);
	for (const BindingOwner &owner : binding_owners) {
		if (!owner.token || owner.token != p_token) {
			continue;
		}
		if (owner.record == INVALID_SLOT) {
			return ERR_UNAVAILABLE;
		}
		Record &record = records[owner.record];
		if (!record.attached || record.quiescing || record.close_started || record.close_error != OK) {
			return ERR_UNAVAILABLE;
		}
		if (record.private_refs >= private_limit) {
			return ERR_PARAMETER_RANGE_ERROR;
		}
		record.private_refs++;
		r_ref = RecordRef(this, owner.record);
		return OK;
	}
	return ERR_DOES_NOT_EXIST;
}

void Registry::forget_binding_owner(GDExtension *p_owner) {
	MutexLock lock(mutex);
	for (BindingOwner &owner : binding_owners) {
		if (owner.token == p_owner) {
			owner = BindingOwner();
			return;
		}
	}
}

void Registry::attach(const RecordRef &p_ref) {
	MutexLock lock(mutex);
	records[p_ref.slot].attached = true;
	records[p_ref.slot].snapshot.wrapper_association = 1;
}
void Registry::observe_initialize(const RecordRef &p_ref, Error p_error) {
	MutexLock lock(mutex);
	Record &record = records[p_ref.slot];
	record.snapshot.loader_initialize_returned = 1;
	record.snapshot.loader_initialize_error = p_error;
	if (p_error != OK) {
		std::memcpy(record.failure_detail, "loader initialize returned an error", sizeof("loader initialize returned an error"));
	}
}
void Registry::detach(const RecordRef &p_ref) {
	if (p_ref.registry != this) {
		return;
	}
	MutexLock lock(mutex);
	records[p_ref.slot].attached = false;
	records[p_ref.slot].snapshot.wrapper_association = 2;
}
Error Registry::retain_private(const RecordRef &p_ref, RecordRef &r_ref) {
	if (p_ref.registry != this || r_ref.is_valid()) {
		return ERR_INVALID_DATA;
	}
	MutexLock lock(mutex);
	Record &record = records[p_ref.slot];
	if (record.private_refs == private_limit) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	record.private_refs++;
	r_ref = RecordRef(this, p_ref.slot);
	return OK;
}

uint32_t Registry::find_view(GDExtensionDeviceExitLibraryViewRef p_view) const {
	if (p_view.registry_instance != instance || p_view.handle_serial == 0) {
		return INVALID_SLOT;
	}
	for (uint32_t i = 0; i < view_limit; i++) {
		if (views[i].domain == ViewDomain::METADATA && views[i].serial == p_view.handle_serial) {
			return i;
		}
	}
	return INVALID_SLOT;
}
Error Registry::issue_view(uint32_t p_record, GDExtensionDeviceExitLibraryViewRef *r_view) {
	if (serials.exhausted) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	for (uint32_t i = 0; i < view_limit; i++) {
		if (views[i].serial == 0) {
			serials.take(views[i].serial);
			views[i].record = p_record;
			records[p_record].external_refs++;
			*r_view = { instance, views[i].serial };
			return OK;
		}
	}
	return ERR_PARAMETER_RANGE_ERROR;
}

int32_t Registry::bootstrap(const GDExtensionDeviceExitAbi *p_request, GDExtensionClassLibraryPtr p_library, GDExtensionDeviceExitLibraryCapability *r_library) {
	if (!aligned(r_library)) {
		return ERR_INVALID_PARAMETER;
	}
	*r_library = {};
	if (!valid_abi(p_request)) {
		return ERR_INVALID_PARAMETER;
	}
	BootstrapScope *scope = BootstrapScope::current;
	// Only the exact top bootstrap frame may issue metadata authority.
	// A nested callback must not borrow the still-live outer bootstrap scope.
	if (!scope || RegistrationContext::current != &scope->context || scope->reference.registry != this || scope->expected != p_library) {
		return ERR_INVALID_DATA;
	}
	MutexLock lock(mutex);
	if (phase != Phase::RUNNING) {
		return ERR_BUSY;
	}
	*r_library = { instance, records[scope->reference.slot].authority_serial };
	return OK;
}
int32_t Registry::retain(const GDExtensionDeviceExitAbi *p_request, GDExtensionDeviceExitLibraryCapability p_library, GDExtensionDeviceExitLibraryViewRef *r_view) {
	if (!aligned(r_view)) {
		return ERR_INVALID_PARAMETER;
	}
	*r_view = {};
	if (!valid_abi(p_request)) {
		return ERR_INVALID_PARAMETER;
	}
	BootstrapScope *scope = BootstrapScope::current;
	if (!scope || RegistrationContext::current != &scope->context || scope->reference.registry != this) {
		return ERR_INVALID_DATA;
	}
	MutexLock lock(mutex);
	const uint32_t slot = scope->reference.slot;
	if (p_library.registry_instance != instance || p_library.handle_serial != records[slot].authority_serial) {
		return ERR_INVALID_DATA;
	}
	if (phase != Phase::RUNNING) {
		return ERR_BUSY;
	}
	return issue_view(slot, r_view);
}
int32_t Registry::copy(GDExtensionDeviceExitLibraryViewRef p_view, GDExtensionDeviceExitLibrarySnapshot *r_snapshot) {
	if (!aligned(r_snapshot) || !valid_abi(&r_snapshot->abi, 48)) {
		return ERR_INVALID_PARAMETER;
	}
	MutexLock lock(mutex);
	const uint32_t slot = find_view(p_view);
	if (slot == INVALID_SLOT) {
		return ERR_INVALID_DATA;
	}
	*r_snapshot = records[views[slot].record].snapshot;
	return OK;
}
int32_t Registry::release(const GDExtensionDeviceExitAbi *p_request, GDExtensionDeviceExitLibraryViewRef p_view) {
	if (!valid_abi(p_request)) {
		return ERR_INVALID_PARAMETER;
	}
	MutexLock lock(mutex);
	const uint32_t slot = find_view(p_view);
	if (slot == INVALID_SLOT) {
		return ERR_INVALID_DATA;
	}
	const uint32_t record = views[slot].record;
	views[slot] = View();
	records[record].external_refs--;
	reclaim(record);
	return OK;
}
int32_t Registry::clone(const GDExtensionDeviceExitAbi *p_request, GDExtensionDeviceExitLibraryViewRef p_source, GDExtensionDeviceExitLibraryViewRef *r_view) {
	if (!aligned(r_view)) {
		return ERR_INVALID_PARAMETER;
	}
	*r_view = {};
	if (!valid_abi(p_request)) {
		return ERR_INVALID_PARAMETER;
	}
	MutexLock lock(mutex);
	const uint32_t slot = find_view(p_source);
	if (slot == INVALID_SLOT) {
		return ERR_INVALID_DATA;
	}
	if (phase != Phase::RUNNING) {
		return ERR_BUSY;
	}
	return issue_view(views[slot].record, r_view);
}

static int32_t bootstrap_library(const GDExtensionDeviceExitAbi *p_request, GDExtensionClassLibraryPtr p_library, GDExtensionDeviceExitLibraryCapability *r_library) {
	return Registry::get_singleton().bootstrap(p_request, p_library, r_library);
}
static int32_t retain_view(const GDExtensionDeviceExitAbi *p_request, GDExtensionDeviceExitLibraryCapability p_library, GDExtensionDeviceExitLibraryViewRef *r_view) {
	return Registry::get_singleton().retain(p_request, p_library, r_view);
}
static int32_t copy_snapshot(GDExtensionDeviceExitLibraryViewRef p_view, GDExtensionDeviceExitLibrarySnapshot *r_snapshot) {
	return Registry::get_singleton().copy(p_view, r_snapshot);
}
static int32_t release_view(const GDExtensionDeviceExitAbi *p_request, GDExtensionDeviceExitLibraryViewRef p_view) {
	return Registry::get_singleton().release(p_request, p_view);
}
static int32_t clone_view(const GDExtensionDeviceExitAbi *p_request, GDExtensionDeviceExitLibraryViewRef p_source, GDExtensionDeviceExitLibraryViewRef *r_view) {
	return Registry::get_singleton().clone(p_request, p_source, r_view);
}
void register_typed_lifecycle_interfaces();

void register_device_session_interfaces();
void Registry::register_interfaces() {
	register_device_session_interfaces();
	register_typed_lifecycle_interfaces();
	GDExtension::register_interface_function("device_exit_bootstrap_library_v1", (GDExtensionInterfaceFunctionPtr)&bootstrap_library);
	GDExtension::register_interface_function("device_exit_retain_library_view_v1", (GDExtensionInterfaceFunctionPtr)&retain_view);
	GDExtension::register_interface_function("device_exit_copy_library_snapshot_v1", (GDExtensionInterfaceFunctionPtr)&copy_snapshot);
	GDExtension::register_interface_function("device_exit_release_library_view_v1", (GDExtensionInterfaceFunctionPtr)&release_view);
	GDExtension::register_interface_function("device_exit_clone_library_view_v1", (GDExtensionInterfaceFunctionPtr)&clone_view);
}
} // namespace GDExtensionDeviceExit
