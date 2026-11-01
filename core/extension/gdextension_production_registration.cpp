/**************************************************************************/
/*  gdextension_production_registration.cpp                               */
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

#include "core/extension/gdextension_production_registration.h"

#include "core/extension/gdextension.h"
#include "core/os/thread.h"

#include <cstdio>
#include <cstring>
#include <type_traits>

namespace GDExtensionDeviceExit {

Error Registry::producer_registration_locked(const ProducerRegistrationRef &p_ref, Record *&r_record) {
	r_record = nullptr;
	if (p_ref.reference.registry != this || p_ref.reference.slot >= record_limit || !p_ref.registration_serial) {
		return ERR_INVALID_DATA;
	}
	Record &record = records[p_ref.reference.slot];
	if (!record.occupied || record.producer_registration_serial != p_ref.registration_serial ||
			!record.producer_attachment_identity || !record.snapshot.library_generation) {
		return ERR_INVALID_DATA;
	}
	r_record = &record;
	return OK;
}

Error Registry::admit_producer_source_locked(const ProducerRegistrationRef &p_ref, Record *&r_record) {
	Error error = producer_registration_locked(p_ref, r_record);
	if (error != OK) {
		return error;
	}
	const Record &record = *r_record;
	if (!record.attached || record.quiescing || record.close_started || record.close_error != OK ||
			record.guard_enrollment != GuardEnrollment::QUALIFIED) {
		return ERR_UNAVAILABLE;
	}
	for (const HolderRetirement *holder = record.holders; holder; holder = holder->record_next) {
		if (holder->typed_code_hold && holder->handle_serial == record.producer_code_holder &&
				!holder->pending && holder->generation == record.snapshot.library_generation) {
			return OK;
		}
	}
	return ERR_UNAVAILABLE;
}

Error Registry::validate_host_producer_binding_locked(InitializationLevelContextRef p_initialize, LibraryCodeHoldRef p_code,
		const void *p_attachment_identity, uint32_t &r_record) {
	r_record = INVALID_SLOT;
	if (!p_attachment_identity) {
		return ERR_INVALID_PARAMETER;
	}
	View *view = nullptr;
	Error error = resolve_borrow_locked(p_initialize.registry_instance, p_initialize.handle_serial, ViewDomain::INITIALIZE, view);
	if (error != OK) {
		return error;
	}
	const RegistrationContext *frame = RegistrationContext::current;
	Record &record = records[view->record];
	if (!frame || !frame->reference || frame->reference->registry != this || frame->reference->slot != view->record ||
			!frame->admitted || !frame->registration_allowed || frame->ack || frame->level_phase != LevelPhase::INITIALIZE ||
			frame->level != GDEXTENSION_INITIALIZATION_SCENE || record.owner_thread != Thread::get_caller_id()) {
		return ERR_UNAUTHORIZED;
	}
	if (!record.original_owner || !record.attached || !record.bootstrap_tuple_observed ||
			record.guard_enrollment != GuardEnrollment::QUALIFIED || record.quiescing || record.close_started || record.close_error != OK) {
		return ERR_UNAVAILABLE;
	}
	if (p_code.registry_instance != instance || !p_code.handle_serial) {
		return ERR_INVALID_DATA;
	}
	const LevelState &scene = record.levels[GDEXTENSION_INITIALIZATION_SCENE];
	const HolderRetirement *original_holder = nullptr;
	for (const HolderRetirement *holder = record.holders; holder; holder = holder->record_next) {
		if (holder->typed_code_hold && holder->handle_serial == p_code.handle_serial && !holder->pending &&
				holder->generation == record.snapshot.library_generation && holder->acquired_thread == record.owner_thread &&
				((scene.user_action.serial && holder->terminal_action == scene.user_action.serial && !scene.user_action.returned) ||
						(scene.binding_action.serial && holder->terminal_action == scene.binding_action.serial && !scene.binding_action.returned))) {
			original_holder = holder;
			break;
		}
	}
	if (!original_holder) {
		return ERR_UNAUTHORIZED;
	}
	if (record.producer_attachment_identity && (record.producer_attachment_identity != p_attachment_identity || record.producer_code_holder != original_holder->handle_serial)) {
		return ERR_INVALID_DATA;
	}
	r_record = view->record;
	return OK;
}

Error Registry::bind_host_producer(InitializationLevelContextRef p_initialize, LibraryCodeHoldRef p_code,
		const void *p_attachment_identity, ProducerRegistrationRef &r_ref) {
	if (r_ref.is_valid()) {
		return ERR_INVALID_PARAMETER;
	}
	MutexLock lock(mutex);
	uint32_t slot = INVALID_SLOT;
	Error error = validate_host_producer_binding_locked(p_initialize, p_code, p_attachment_identity, slot);
	if (error != OK) {
		return error;
	}
	Record &record = records[slot];
	if (record.private_refs >= private_limit) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	record.producer_attachment_identity = p_attachment_identity;
	record.producer_code_holder = p_code.handle_serial;
	record.private_refs++;
	r_ref.reference = RecordRef(this, slot);
	r_ref.registration_serial = record.producer_registration_serial;
	return OK;
}

Error Registry::clone_producer_registration(const ProducerRegistrationRef &p_ref, ProducerRegistrationRef &r_ref) {
	if (r_ref.is_valid()) {
		return ERR_INVALID_PARAMETER;
	}
	MutexLock lock(mutex);
	Record *record = nullptr;
	Error error = producer_registration_locked(p_ref, record);
	if (error != OK) {
		return error;
	}
	if (record->private_refs >= private_limit) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	// This retains already issued metadata, including after detach. It never
	// opens source admission or calls an original DSO producer/encoder.
	record->private_refs++;
	r_ref.reference = RecordRef(this, p_ref.reference.slot);
	r_ref.registration_serial = p_ref.registration_serial;
	return OK;
}

Error Registry::copy_producer_registration(const ProducerRegistrationRef &p_ref, ProducerRegistrationIdentity &r_identity) {
	MutexLock lock(mutex);
	Record *record = nullptr;
	Error error = producer_registration_locked(p_ref, record);
	return error == OK ? copy_producer_identity_locked(*record, r_identity) : error;
}

Error Registry::copy_producer_identity_locked(const Record &record, ProducerRegistrationIdentity &r_identity) {
	ProducerRegistrationIdentity identity;
	identity.registry_instance = instance;
	identity.library_generation = record.snapshot.library_generation;
	// Bounded core-generated export label for this one original Host site.
	// The original issuer serial is not an observation allocation serial.
	const int size = std::snprintf(identity.label, sizeof(identity.label), "godot-host:%llu:%llu",
			static_cast<unsigned long long>(instance), static_cast<unsigned long long>(record.producer_registration_serial));
	if (size <= 0 || static_cast<size_t>(size) >= sizeof(identity.label)) {
		return ERR_INVALID_DATA;
	}
	r_identity = identity;
	return OK;
}

Error Registry::producer_view_locked(GDExtensionProducerRegistrationRefV1 p_ref, uint32_t &r_view) {
	r_view = INVALID_SLOT;
	if (p_ref.registry_instance != instance || !p_ref.handle_serial) {
		return ERR_INVALID_DATA;
	}
	for (uint32_t i = 0; i < view_limit; ++i) {
		const View &view = views[i];
		if (view.domain == ViewDomain::PRODUCER_REGISTRATION && view.serial == p_ref.handle_serial) {
			const Record &record = records[view.record];
			if (!record.occupied || !record.producer_registration_serial || !record.producer_attachment_identity) {
				return ERR_INVALID_DATA;
			}
			r_view = i;
			return OK;
		}
	}
	return ERR_INVALID_DATA;
}

Error Registry::producer_view_capacity_locked(uint32_t &r_view) {
	r_view = INVALID_SLOT;
	if (serials.exhausted) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	for (uint32_t i = 0; i < view_limit; ++i) {
		if (!views[i].serial) {
			r_view = i;
			return OK;
		}
	}
	return ERR_PARAMETER_RANGE_ERROR;
}

void Registry::issue_producer_view_locked(uint32_t p_record, uint32_t p_view, GDExtensionProducerRegistrationRefV1 &r_ref) {
	View &view = views[p_view];
	serials.take(view.serial);
	view.record = p_record;
	view.domain = ViewDomain::PRODUCER_REGISTRATION;
	view.frame = 0; // Owned metadata, not an expiring initializer borrow.
	records[p_record].external_refs++;
	r_ref = { instance, view.serial };
}

Error Registry::bind_host_producer_view(InitializationLevelContextRef p_initialize, LibraryCodeHoldRef p_code,
		const void *p_attachment_identity, GDExtensionProducerRegistrationRefV1 &r_ref) {
	if (r_ref.registry_instance || r_ref.handle_serial) {
		return ERR_INVALID_DATA;
	}
	MutexLock lock(mutex);
	uint32_t record_slot = INVALID_SLOT;
	Error error = validate_host_producer_binding_locked(p_initialize, p_code, p_attachment_identity, record_slot);
	if (error != OK) {
		return error;
	}
	uint32_t view_slot = INVALID_SLOT;
	error = producer_view_capacity_locked(view_slot);
	if (error != OK) {
		return error;
	}
	Record &record = records[record_slot];
	if (record.external_refs == UINT32_MAX) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	// Capacity and all authentication checks precede the one original binding.
	record.producer_attachment_identity = p_attachment_identity;
	record.producer_code_holder = p_code.handle_serial;
	issue_producer_view_locked(record_slot, view_slot, r_ref);
	return OK;
}

Error Registry::clone_producer_registration_view(GDExtensionProducerRegistrationRefV1 p_source, GDExtensionProducerRegistrationRefV1 &r_ref) {
	if (r_ref.registry_instance || r_ref.handle_serial) {
		return ERR_INVALID_DATA;
	}
	MutexLock lock(mutex);
	uint32_t source = INVALID_SLOT;
	Error error = producer_view_locked(p_source, source);
	if (error != OK) {
		return error;
	}
	uint32_t destination = INVALID_SLOT;
	error = producer_view_capacity_locked(destination);
	if (error != OK) {
		return error;
	}
	const uint32_t record = views[source].record;
	if (records[record].external_refs == UINT32_MAX) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	issue_producer_view_locked(record, destination, r_ref);
	return OK;
}

Error Registry::copy_producer_registration_view(GDExtensionProducerRegistrationRefV1 p_source, ProducerRegistrationIdentity &r_identity) {
	MutexLock lock(mutex);
	uint32_t source = INVALID_SLOT;
	Error error = producer_view_locked(p_source, source);
	return error == OK ? copy_producer_identity_locked(records[views[source].record], r_identity) : error;
}

Error Registry::release_producer_registration_view(GDExtensionProducerRegistrationRefV1 p_source) {
	MutexLock lock(mutex);
	uint32_t source = INVALID_SLOT;
	Error error = producer_view_locked(p_source, source);
	if (error != OK) {
		return error;
	}
	const uint32_t record = views[source].record;
	views[source] = View();
	records[record].external_refs--;
	reclaim(record);
	return OK;
}

Error Registry::import_producer_registration(GDExtensionProducerRegistrationRefV1 p_source, ProducerRegistrationRef &r_ref) {
	if (r_ref.is_valid()) {
		return ERR_INVALID_DATA;
	}
	MutexLock lock(mutex);
	uint32_t source = INVALID_SLOT;
	Error error = producer_view_locked(p_source, source);
	if (error != OK) {
		return error;
	}
	const uint32_t slot = views[source].record;
	Record &record = records[slot];
	if (record.private_refs >= private_limit) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	// This imports exact retained identity only. The production source receiver
	// still checks admission plus original operation/context/scope/role grants.
	record.private_refs++;
	r_ref.reference = RecordRef(this, slot);
	r_ref.registration_serial = record.producer_registration_serial;
	return OK;
}

namespace {
template <typename T>
bool aligned_registration_pointer(const T *p_pointer) {
	return p_pointer && reinterpret_cast<uintptr_t>(p_pointer) % alignof(T) == 0;
}
bool registration_abi(const GDExtensionDeviceExitAbi *p_abi, uint32_t p_size = sizeof(GDExtensionDeviceExitAbi)) {
	return aligned_registration_pointer(p_abi) && p_abi->abi_version == 1 && p_abi->struct_size == p_size && !p_abi->reserved;
}
bool registration_output_separate(const GDExtensionDeviceExitAbi *p_abi, const void *p_output, size_t p_size) {
	const uintptr_t a = reinterpret_cast<uintptr_t>(p_abi);
	const uintptr_t b = reinterpret_cast<uintptr_t>(p_output);
	return a <= b ? sizeof(*p_abi) <= b - a : p_size <= a - b;
}
int32_t bind_registration(const GDExtensionDeviceExitAbi *p_abi, GDExtensionDeviceExitInitializationLevelContextRef p_initialize,
		GDExtensionDeviceExitLibraryCodeHoldRef p_code, const void *p_attachment, GDExtensionProducerRegistrationRefV1 *r_ref) {
	if (!registration_abi(p_abi) || !aligned_registration_pointer(r_ref) || !registration_output_separate(p_abi, r_ref, sizeof(*r_ref))) {
		return ERR_INVALID_PARAMETER;
	}
	return Registry::get_singleton().bind_host_producer_view({ p_initialize.registry_instance, p_initialize.handle_serial },
			{ p_code.registry_instance, p_code.handle_serial }, p_attachment, *r_ref);
}
int32_t clone_registration(const GDExtensionDeviceExitAbi *p_abi, GDExtensionProducerRegistrationRefV1 p_source, GDExtensionProducerRegistrationRefV1 *r_ref) {
	if (!registration_abi(p_abi) || !aligned_registration_pointer(r_ref) || !registration_output_separate(p_abi, r_ref, sizeof(*r_ref))) {
		return ERR_INVALID_PARAMETER;
	}
	return Registry::get_singleton().clone_producer_registration_view(p_source, *r_ref);
}
int32_t copy_registration(const GDExtensionDeviceExitAbi *p_abi, GDExtensionProducerRegistrationRefV1 p_source, GDExtensionProducerRegistrationIdentityV1 *r_identity) {
	if (!registration_abi(p_abi) || !aligned_registration_pointer(r_identity) ||
			!registration_abi(&r_identity->abi, sizeof(*r_identity)) || !registration_output_separate(p_abi, r_identity, sizeof(*r_identity))) {
		return ERR_INVALID_PARAMETER;
	}
	ProducerRegistrationIdentity identity;
	const Error error = Registry::get_singleton().copy_producer_registration_view(p_source, identity);
	if (error != OK) {
		return error;
	}
	GDExtensionProducerRegistrationIdentityV1 result{};
	result.abi = { 1, sizeof(result), 0 };
	result.registry_instance = identity.registry_instance;
	result.library_generation = identity.library_generation;
	std::memcpy(result.label, identity.label, sizeof(result.label));
	*r_identity = result;
	return OK;
}
int32_t release_registration(const GDExtensionDeviceExitAbi *p_abi, GDExtensionProducerRegistrationRefV1 p_source) {
	if (!registration_abi(p_abi)) {
		return ERR_INVALID_PARAMETER;
	}
	return Registry::get_singleton().release_producer_registration_view(p_source);
}
static_assert(sizeof(GDExtensionProducerRegistrationRefV1) == 16);
static_assert(sizeof(GDExtensionProducerRegistrationIdentityV1) == 128);
static_assert(std::is_same_v<decltype(&bind_registration), GDExtensionInterfaceDeviceSessionBindHostProducerV1>);
static_assert(std::is_same_v<decltype(&clone_registration), GDExtensionInterfaceDeviceSessionCloneProducerRegistrationV1>);
static_assert(std::is_same_v<decltype(&copy_registration), GDExtensionInterfaceDeviceSessionCopyProducerRegistrationV1>);
static_assert(std::is_same_v<decltype(&release_registration), GDExtensionInterfaceDeviceSessionReleaseProducerRegistrationV1>);
} // namespace

void register_producer_registration_interfaces() {
	GDExtension::register_interface_function("device_session_bind_host_producer_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&bind_registration));
	GDExtension::register_interface_function("device_session_clone_producer_registration_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&clone_registration));
	GDExtension::register_interface_function("device_session_copy_producer_registration_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&copy_registration));
	GDExtension::register_interface_function("device_session_release_producer_registration_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&release_registration));
}

} // namespace GDExtensionDeviceExit
