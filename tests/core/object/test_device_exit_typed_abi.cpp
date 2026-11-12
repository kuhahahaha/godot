/**************************************************************************/
/*  test_device_exit_typed_abi.cpp                                        */
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

#include "core/extension/gdextension.h"
#include "core/extension/gdextension_library_loader.h"
#include "core/extension/gdextension_manager.h"
#include "core/extension/gdextension_production_registration.h"
#include "core/io/config_file.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/object/message_queue.h"
#include "core/os/os.h"
#include "tests/core/object/device_exit_startup_fixture.h"
#include "tests/test_macros.h"
#include "tests/test_utils.h"

#include <cstring>
#include <thread>
#include <type_traits>

namespace GDExtensionDeviceExit {
GDExtensionInterfaceFunctionPtr test_typed_lifecycle_lookup(const char *name);
}
extern GDExtensionInterfaceFunctionPtr gdextension_get_proc_address(const char *name);

namespace TestDeviceExitEntry {
void test_v2_single_registration();
}
TEST_FORCE_LINK(test_device_exit_typed_abi)

TEST_CASE("[DeviceExit][TypedABI] generated lifecycle procs preserve rejected outputs") {
	REQUIRE(GDExtensionDeviceExit::test_typed_lifecycle_lookup("device_exit_register_guard_v1") != nullptr);
	REQUIRE(GDExtensionDeviceExit::test_typed_lifecycle_lookup("device_exit_borrow_guarded_library_v1") != nullptr);
	REQUIRE(GDExtensionDeviceExit::test_typed_lifecycle_lookup("device_exit_borrow_initialize_level_v1") != nullptr);
	REQUIRE(GDExtensionDeviceExit::test_typed_lifecycle_lookup("device_exit_borrow_deinit_level_v1") != nullptr);
	REQUIRE(GDExtensionDeviceExit::test_typed_lifecycle_lookup("device_exit_borrow_level_cleanup_v1") != nullptr);
	REQUIRE(GDExtensionDeviceExit::test_typed_lifecycle_lookup("device_exit_schedule_object_cleanup_v1") != nullptr);
	REQUIRE(GDExtensionDeviceExit::test_typed_lifecycle_lookup("device_exit_acquire_code_hold_v1") != nullptr);
	REQUIRE(GDExtensionDeviceExit::test_typed_lifecycle_lookup("device_exit_clone_code_hold_v1") != nullptr);
	REQUIRE(GDExtensionDeviceExit::test_typed_lifecycle_lookup("device_exit_release_code_hold_v1") != nullptr);
	REQUIRE(GDExtensionDeviceExit::test_typed_lifecycle_lookup("device_exit_after_binding_initialize_v1") != nullptr);
	REQUIRE(GDExtensionDeviceExit::test_typed_lifecycle_lookup("device_exit_begin_user_terminator_v1") != nullptr);
	REQUIRE(GDExtensionDeviceExit::test_typed_lifecycle_lookup("device_exit_after_user_terminator_v1") != nullptr);
	REQUIRE(GDExtensionDeviceExit::test_typed_lifecycle_lookup("device_exit_after_binding_deinit_v1") != nullptr);
	REQUIRE(GDExtensionDeviceExit::test_typed_lifecycle_lookup("device_exit_publish_exit_result_v1") != nullptr);
	REQUIRE(GDExtensionDeviceExit::test_typed_lifecycle_lookup("device_exit_terminate_initialize_blocked_v1") != nullptr);
	REQUIRE(GDExtensionDeviceExit::test_typed_lifecycle_lookup("device_exit_terminate_blocked_v1") != nullptr);
	if (!GDExtensionDeviceExit::test_typed_lifecycle_lookup("device_exit_register_guard_v1")) {
		return;
	}
	GDExtensionDeviceExitGuardRegistrationV1 registration{};
	registration.abi = { 1, sizeof(registration), 0 };
	auto register_guard = reinterpret_cast<GDExtensionInterfaceDeviceExitRegisterGuardV1>(GDExtensionDeviceExit::test_typed_lifecycle_lookup("device_exit_register_guard_v1"));
	GDExtensionDeviceExitGuardToken token{ 123, 456 };
	CHECK(register_guard(nullptr, &registration, &token) == ERR_INVALID_DATA);
	CHECK(token.registry_instance == 123);
	CHECK(token.handle_serial == 456);
	CHECK(register_guard(nullptr, nullptr, &token) == ERR_INVALID_PARAMETER);
	token = {};
	CHECK(register_guard(nullptr, &registration, &token) != OK);
	CHECK(token.registry_instance == 0);
	CHECK(token.handle_serial == 0);
	const GDExtensionDeviceExitAbi abi{ 1, sizeof(GDExtensionDeviceExitAbi), 0 };
	auto borrow = reinterpret_cast<GDExtensionInterfaceDeviceExitBorrowGuardedLibraryV1>(GDExtensionDeviceExit::test_typed_lifecycle_lookup("device_exit_borrow_guarded_library_v1"));
	GDExtensionDeviceExitGuardedLibraryRef empty{};
	CHECK(borrow(&abi, nullptr, &empty) != OK);
	CHECK(empty.registry_instance == 0);
	CHECK(empty.handle_serial == 0);
	for (const GDExtensionDeviceExitAbi invalid : { GDExtensionDeviceExitAbi{ 2, sizeof(abi), 0 }, GDExtensionDeviceExitAbi{ 1, 0, 0 }, GDExtensionDeviceExitAbi{ 1, sizeof(abi), 1 } }) {
		CHECK(borrow(&invalid, nullptr, &empty) == ERR_INVALID_PARAMETER);
		CHECK(empty.registry_instance == 0);
		CHECK(empty.handle_serial == 0);
	}
	GDExtensionDeviceExitGuardedLibraryRef occupied{ 123, 456 };
	CHECK(borrow(&abi, nullptr, &occupied) != OK);
	CHECK(occupied.registry_instance == 123);
	CHECK(occupied.handle_serial == 456);
	CHECK(borrow(&abi, nullptr, nullptr) == ERR_INVALID_PARAMETER);
	alignas(GDExtensionDeviceExitGuardedLibraryRef) unsigned char bytes[sizeof(empty) + 8] = {};
	CHECK(borrow(&abi, nullptr, reinterpret_cast<GDExtensionDeviceExitGuardedLibraryRef *>(bytes + 1)) == ERR_INVALID_PARAMETER);
	for (unsigned char byte : bytes) {
		CHECK(byte == 0);
	}
}

#include "core/extension/gdextension_function_loader.h"
namespace TestDeviceExitContract {
Error test_producer_admission(const GDExtensionDeviceExit::ProducerRegistrationRef &);
void test_producer_capacity(GDExtensionDeviceExit::InitializationLevelContextRef, GDExtensionDeviceExit::LibraryCodeHoldRef, const void *);
} //namespace TestDeviceExitContract

namespace TestTypedLifecycleABI {
static_assert(!std::is_copy_constructible_v<GDExtensionDeviceExit::ProducerRegistrationRef>);
static_assert(!std::is_constructible_v<GDExtensionDeviceExit::ProducerRegistrationRef, uint64_t, uint64_t>);
static bool exercise_producer = false;
static GDExtensionDeviceExit::ProducerRegistrationRef producer;
static GDExtensionProducerRegistrationRefV1 producer_abi;
static GDExtensionDeviceExit::InitializationLevelContextRef producer_initialize;
static int producer_identity;
static int foreign_producer_identity;

template <typename T>
T proc(const char *name) {
	return reinterpret_cast<T>(GDExtensionDeviceExit::test_typed_lifecycle_lookup(name));
}
static GDExtensionClassLibraryPtr library;
static GDExtensionDeviceExitGuardToken token;
static GDExtensionDeviceExitLibraryCodeHoldRef hold;
static const GDExtensionDeviceExitAbi abi{ 1, sizeof(GDExtensionDeviceExitAbi), 0 };
// Negative API/lifecycle skeleton only: pending below is a fixture input, not B query completion evidence.
static bool exercise_prepare = false;
static bool prepare_retire_relay = false;
static bool fail_after_prepare_registration = false;
static uint64_t prepare_pending = 1;
static int prepare_calls = 0;
static bool prepare_write = true;
static int32_t prepare_result = OK;
static bool prepare_reenter = false;
static GDExtension *prepare_extension = nullptr;
static int32_t prepare_owner(void *, uint32_t route, uint64_t *pending) {
	++prepare_calls;
	CHECK(route == uint32_t(GDExtensionDeviceExit::CloseRoute::EXPLICIT));
	if (prepare_reenter) { CHECK(prepare_extension->preflight_exit(GDExtensionDeviceExit::CloseRoute::EXPLICIT) == ERR_BUSY); }
	// No preflight context is issued: this callback has only ordinary owner authority.
	if (prepare_write) { *pending = prepare_pending; }
	return prepare_result;
}
static_assert(std::is_same_v<decltype(&prepare_owner), GDExtensionDeviceExitPrepareFnV1>);
static bool exercise_callable = false;
static bool exercise_object_cleanup = false;
static Object *user_object = nullptr;
static Object *binding_object = nullptr;
static int object_frees = 0;
static void *create_object_binding(void *, void *object) {
	return object;
}
static void free_object_binding(void *, void *, void *) {
	object_frees++;
}
static const GDExtensionInstanceBindingCallbacks object_callbacks{ create_object_binding, free_object_binding, nullptr };
static Callable retained_callable;
static Callable equivalent_callable;
static int callable_calls = 0;
static int callable_frees = 0;
static GDExtensionDeviceExit::NormalRelayToken observed_relay;
static GDExtensionDeviceExit::NormalRelaySnapshot prepare_relay_snapshot;
static Error prepare_relay_forget_result = OK;
static bool prepare_relay_forgotten = false;
static bool release_relay_in_call = false;
static Error relay_release_in_call = OK;
static bool relay_observed_active = false;
static bool relay_observed_closed_in_free = false;
static int callable_hashes = 0;
static int callable_validations = 0;
static int callable_strings = 0;
static int callable_argument_counts = 0;
static GDExtensionBool callable_valid(void *) {
	callable_validations++;
	return true;
}
static void callable_string(void *, GDExtensionBool *valid, GDExtensionStringPtr result) {
	callable_strings++;
	*valid = true;
	*reinterpret_cast<String *>(result) = "typed callable";
}
static GDExtensionInt callable_arguments(void *, GDExtensionBool *valid) {
	callable_argument_counts++;
	*valid = true;
	return 0;
}
static void callable_call(void *, const GDExtensionConstVariantPtr *, GDExtensionInt, GDExtensionVariantPtr result, GDExtensionCallError *error) {
	callable_calls++;
	if (release_relay_in_call) {
		auto &registry = GDExtensionDeviceExit::Registry::get_singleton();
		GDExtensionDeviceExit::NormalRelaySnapshot snapshot;
		relay_observed_active = registry.observe_normal_relay(observed_relay, snapshot) == OK && snapshot.active_deliveries == 1;
		relay_release_in_call = registry.release_normal_relay_holder(observed_relay);
	}
	*reinterpret_cast<Variant *>(result) = 42;
	error->error = GDEXTENSION_CALL_OK;
}
static void callable_free(void *) {
	callable_frees++;
	if (observed_relay.serial) {
		GDExtensionDeviceExit::NormalRelaySnapshot snapshot;
		if (GDExtensionDeviceExit::Registry::get_singleton().observe_normal_relay(observed_relay, snapshot) == OK) {
			relay_observed_closed_in_free = snapshot.holder_closed;
		}
	}
}
static uint32_t callable_hash(void *) {
	callable_hashes++;
	return 42;
}
static GDExtensionBool callable_equal(void *, void *) {
	return true;
}
static GDExtensionBool callable_less(void *, void *) {
	return false;
}
// Exercise the original relay and its actual Callable destructor under prepare's
// admitted CallbackScope. Pending is derived from native observation, not a fixture input.
static int32_t prepare_relay_owner(void *, uint32_t route, uint64_t *pending) {
	++prepare_calls;
	CHECK(route == uint32_t(GDExtensionDeviceExit::CloseRoute::EXPLICIT));
	auto &registry = GDExtensionDeviceExit::Registry::get_singleton();
	if (prepare_relay_forgotten) {
		*pending = 0; // Only an actual successful native forget sets this receipt.
		return OK;
	}
	// Drop actual aliases only inside the ordinary admitted original scope.
	// This also handles teardown if setup failed before relay registration.
	retained_callable = Callable();
	equivalent_callable = Callable();
	if (!observed_relay.serial) {
		*pending = 0;
		return OK;
	}
	const Error cancelled = registry.cancel_normal_relay(observed_relay);
	if (cancelled != OK) { return cancelled; }
	const Error released = registry.release_normal_relay_holder(observed_relay);
	if (released != OK) { return released; }
	const Error observed = registry.observe_normal_relay(observed_relay, prepare_relay_snapshot);
	if (observed != OK) { return observed; }
	CHECK(prepare_relay_snapshot.cancelled);
	CHECK(prepare_relay_snapshot.release_requested);
	CHECK(prepare_relay_snapshot.cell_released);
	CHECK(prepare_relay_snapshot.active_deliveries == 0);
	*pending = prepare_relay_snapshot.active_deliveries || !prepare_relay_snapshot.cell_released || !prepare_relay_snapshot.holder_closed ? 1 : 0;
	// First call must return BUSY until this outer CallbackScope has returned.
	// On retry the same token acknowledges closure and may finally be forgotten.
	prepare_relay_forget_result = registry.forget_normal_relay(observed_relay);
	if (prepare_relay_forget_result == OK) { prepare_relay_forgotten = true; }
	return prepare_relay_forget_result;
}
static_assert(std::is_same_v<decltype(&prepare_relay_owner), GDExtensionDeviceExitPrepareFnV1>);

static void initialize(void *, GDExtensionInitializationLevel level) {
	CHECK(GDExtensionDeviceExit::Registry::get_singleton().check_current_entry_classes(1u << 0) == ERR_UNAVAILABLE);
	CHECK(GDExtensionDeviceExit::Registry::get_singleton().check_current_entry_classes(1u << 8) == (exercise_callable ? OK : ERR_UNAVAILABLE));
	if (exercise_callable) {
		GDExtensionCallableCustomInfo2 info{};
		info.token = library;
		info.call_func = callable_call;
		info.free_func = callable_free;
		info.hash_func = callable_hash;
		info.equal_func = callable_equal;
		info.less_than_func = callable_less;
		info.is_valid_func = callable_valid;
		info.to_string_func = callable_string;
		info.get_argument_count_func = callable_arguments;
		proc<GDExtensionInterfaceCallableCustomCreate2>("callable_custom_create2")(&retained_callable, &info);
		proc<GDExtensionInterfaceCallableCustomCreate2>("callable_custom_create2")(&equivalent_callable, &info);
	}
	GDExtensionDeviceExitInitializationLevelContextRef context{};
	GDExtensionDeviceExitGuardedLibraryRef borrowed{};
	GDExtensionDeviceExitRegisteredCleanupRef cleanup{};
	CHECK(proc<GDExtensionInterfaceDeviceExitBorrowInitializeLevelV1>("device_exit_borrow_initialize_level_v1")(&abi, library, level, &context) == OK);
	CHECK(proc<GDExtensionInterfaceDeviceExitBorrowGuardedLibraryV1>("device_exit_borrow_guarded_library_v1")(&abi, library, &borrowed) == OK);
	CHECK(proc<GDExtensionInterfaceDeviceExitBorrowLevelCleanupV1>("device_exit_borrow_level_cleanup_v1")(&abi, context, 0, &cleanup) == OK);
	if (exercise_object_cleanup) {
		auto schedule = proc<GDExtensionInterfaceDeviceExitScheduleObjectCleanupV1>("device_exit_schedule_object_cleanup_v1");
		GDExtensionDeviceExitRegisteredCleanupRef binding_cleanup{};
		CHECK(proc<GDExtensionInterfaceDeviceExitBorrowLevelCleanupV1>("device_exit_borrow_level_cleanup_v1")(&abi, context, 1, &binding_cleanup) == OK);
		user_object = memnew(Object);
		binding_object = memnew(Object);
		CHECK(user_object->get_instance_binding(library, &object_callbacks) == user_object);
		CHECK(binding_object->get_instance_binding(library, &object_callbacks) == binding_object);
		CHECK(schedule(&abi, context, cleanup, uint64_t(user_object->get_instance_id()) + 1) == ERR_DOES_NOT_EXIST);
		GDExtensionDeviceExitAbi bad_abi{ 2, sizeof(GDExtensionDeviceExitAbi), 0 };
		CHECK(schedule(&bad_abi, context, cleanup, uint64_t(user_object->get_instance_id())) == ERR_INVALID_PARAMETER);
		GDExtensionDeviceExitRegisteredCleanupRef wrong_terminal{ cleanup.registry_instance, UINT64_MAX };
		CHECK(schedule(&abi, context, wrong_terminal, uint64_t(user_object->get_instance_id())) == ERR_INVALID_DATA);
		Error wrong_thread = OK;
		std::thread worker([&] {
			wrong_thread = Error(schedule(&abi, context, cleanup, uint64_t(user_object->get_instance_id())));
		});
		worker.join();
		CHECK(wrong_thread == ERR_UNAUTHORIZED);
		CHECK(schedule(&abi, context, cleanup, uint64_t(user_object->get_instance_id())) == OK);
		CHECK(schedule(&abi, context, cleanup, uint64_t(user_object->get_instance_id())) == ERR_INVALID_DATA);
		CHECK(schedule(&abi, context, binding_cleanup, uint64_t(binding_object->get_instance_id())) == OK);
	}
	CHECK(proc<GDExtensionInterfaceDeviceExitAcquireCodeHoldV1>("device_exit_acquire_code_hold_v1")(&abi, borrowed, cleanup, &hold) == OK);
	if (exercise_producer) {
		using namespace GDExtensionDeviceExit;
		auto &registry = Registry::get_singleton();
		producer_initialize = { context.registry_instance, context.handle_serial };
		LibraryCodeHoldRef code{ hold.registry_instance, hold.handle_serial };
		TestDeviceExitContract::test_producer_capacity(producer_initialize, code, &producer_identity);
		ProducerRegistrationRef refused;
		CHECK(registry.bind_host_producer(producer_initialize, {}, &producer_identity, refused) != OK);
		CHECK_FALSE(refused.is_valid());
		Error wrong_thread = OK;
		std::thread other([&] { wrong_thread = registry.bind_host_producer(producer_initialize, code, &producer_identity, refused); });
		other.join();
		CHECK(wrong_thread != OK);
		CHECK_FALSE(refused.is_valid());
		auto bind = proc<GDExtensionInterfaceDeviceSessionBindHostProducerV1>("device_session_bind_host_producer_v1");
		CHECK(bind != nullptr);
		if (bind) {
			CHECK_EQ(bind(&abi, context, hold, &producer_identity, &producer_abi), OK);
			CHECK_EQ(registry.import_producer_registration(producer_abi, producer), OK);
		}
		GDExtensionProducerRegistrationRefV1 wrong_domain{ hold.registry_instance, hold.handle_serial };
		CHECK(registry.import_producer_registration(wrong_domain, refused) != OK);
		CHECK_FALSE(refused.is_valid());
		ProducerRegistrationRef repeated;
		CHECK_EQ(registry.bind_host_producer(producer_initialize, code, &producer_identity, repeated), OK);
		ProducerRegistrationIdentity original, same;
		CHECK_EQ(registry.copy_producer_registration(producer, original), OK);
		CHECK_EQ(registry.copy_producer_registration(repeated, same), OK);
		CHECK(std::strcmp(original.label, same.label) == 0);
		CHECK_NE(original.library_generation, 0);
		CHECK_EQ(registry.bind_host_producer(producer_initialize, code, &foreign_producer_identity, refused), ERR_INVALID_DATA);
		CHECK_FALSE(refused.is_valid());
		CHECK_EQ(TestDeviceExitContract::test_producer_admission(producer), OK);
	}
	GDExtensionDeviceExitLibraryCodeHoldRef clone{};
	CHECK(proc<GDExtensionInterfaceDeviceExitCloneCodeHoldV1>("device_exit_clone_code_hold_v1")(&abi, hold, &clone) == OK);
	if (exercise_producer) {
		GDExtensionDeviceExit::ProducerRegistrationRef refused;
		CHECK_EQ(GDExtensionDeviceExit::Registry::get_singleton().bind_host_producer(producer_initialize,
						 { clone.registry_instance, clone.handle_serial }, &producer_identity, refused),
				ERR_INVALID_DATA);
		CHECK_FALSE(refused.is_valid());
	}
	CHECK(proc<GDExtensionInterfaceDeviceExitReleaseCodeHoldV1>("device_exit_release_code_hold_v1")(&abi, clone) == OK);
	CHECK(proc<GDExtensionInterfaceDeviceExitAfterBindingInitializeV1>("device_exit_after_binding_initialize_v1")(&abi, context) == OK);
	if (exercise_object_cleanup) {
		CHECK(proc<GDExtensionInterfaceDeviceExitScheduleObjectCleanupV1>("device_exit_schedule_object_cleanup_v1")(&abi, context, cleanup, uint64_t(user_object->get_instance_id())) != OK);
	}
}
static void terminate(void *, GDExtensionInitializationLevel level) {
	GDExtensionDeviceExitDeinitLevelContextRef context{};
	CHECK(proc<GDExtensionInterfaceDeviceExitBorrowDeinitLevelV1>("device_exit_borrow_deinit_level_v1")(&abi, library, level, &context) == OK);
	CHECK(proc<GDExtensionInterfaceDeviceExitBeginUserTerminatorV1>("device_exit_begin_user_terminator_v1")(&abi, token, context) == OK);
	if (exercise_object_cleanup) {
		memdelete(user_object);
		user_object = nullptr;
	}
	CHECK(proc<GDExtensionInterfaceDeviceExitAfterUserTerminatorV1>("device_exit_after_user_terminator_v1")(&abi, context) == ERR_BUSY);
	CHECK(proc<GDExtensionInterfaceDeviceExitAfterBindingDeinitV1>("device_exit_after_binding_deinit_v1")(&abi, context) == ERR_INVALID_DATA);
	CHECK(proc<GDExtensionInterfaceDeviceExitReleaseCodeHoldV1>("device_exit_release_code_hold_v1")(&abi, hold) == OK);
	CHECK(proc<GDExtensionInterfaceDeviceExitAfterUserTerminatorV1>("device_exit_after_user_terminator_v1")(&abi, context) == OK);
	if (exercise_object_cleanup) {
		CHECK(object_frees == 1);
		memdelete(binding_object);
		binding_object = nullptr;
		CHECK(object_frees == 2);
	}
	CHECK(proc<GDExtensionInterfaceDeviceExitAfterBindingDeinitV1>("device_exit_after_binding_deinit_v1")(&abi, context) == OK);
}
static int32_t preflight(GDExtensionDeviceExitDeinitPreflightContextRef context, void *) {
	GDExtensionDeviceExitExitDiagnosticV1 diagnostic{ { 1, sizeof(GDExtensionDeviceExitExitDiagnosticV1), 0 }, OK, GDEXTENSION_INITIALIZATION_SCENE, 0 };
	return proc<GDExtensionInterfaceDeviceExitPublishExitResultV1>("device_exit_publish_exit_result_v1")(&abi, token, context, &diagnostic);
}
static GDExtensionBool entry(GDExtensionInterfaceGetProcAddress get_proc, GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *result) {
	library = p_library;
	*result = { GDEXTENSION_INITIALIZATION_SCENE, nullptr, initialize, terminate };
	GDExtensionDeviceExitGuardRegistrationV1 registration{};
	registration.abi = { 1, sizeof(registration), 0 };
	registration.binding_protocol = 1;
	registration.level_bitmap = registration.user_cleanup_bitmap = registration.binding_cleanup_bitmap = 12;
	registration.expected_initialization = *result;
	registration.user_initializer = initialize;
	registration.user_terminator = terminate;
	registration.preflight = preflight;
	registration.entry_classes = (exercise_callable ? (1u << 8) : 0) | (exercise_object_cleanup ? (1u << 6) : 0);
	registration.needs[GDEXTENSION_INITIALIZATION_SCENE] = { (1u << 1) | (1u << 5) | (1u << 7), 1u << 7 };
	auto register_guard = reinterpret_cast<GDExtensionInterfaceDeviceExitRegisterGuardV1>(get_proc("device_exit_register_guard_v1"));
	// Internal fixtures enter through the TESTS_ENABLED resolver; production
	// version one must obtain this exact operation from its selected lookup.
	if (!register_guard) {
		register_guard = proc<GDExtensionInterfaceDeviceExitRegisterGuardV1>("device_exit_register_guard_v1");
	}
	const int32_t enrolled = register_guard(library, &registration, &token);
	if (enrolled == OK && exercise_prepare) {
		const auto callback = prepare_retire_relay ? prepare_relay_owner : prepare_owner;
		auto install = proc<GDExtensionInterfaceDeviceExitRegisterPrepareV1>("device_exit_register_prepare_v1");
		CHECK(install != nullptr);
		if (!install) { return false; }
		CHECK(install(nullptr, token, callback, nullptr) == ERR_INVALID_PARAMETER);
		CHECK(install(&abi, token, nullptr, nullptr) == ERR_INVALID_PARAMETER);
		auto foreign = token; ++foreign.handle_serial;
		CHECK(install(&abi, foreign, callback, nullptr) == ERR_UNAUTHORIZED);
		CHECK(install(&abi, token, callback, nullptr) == OK);
		CHECK(install(&abi, token, callback, nullptr) == ERR_ALREADY_IN_USE);
	}
	return enrolled == OK && !fail_after_prepare_registration;
}
static bool guarded_lookup_seen = false;
static Object *cross_level_object = nullptr;
static Ref<RefCounted> cross_level_ref;
static int cross_level_frees = 0;
static void cross_level_binding_free(void *, void *, void *) {
	cross_level_frees++;
}
static const GDExtensionInstanceBindingCallbacks cross_level_callbacks{ create_object_binding, cross_level_binding_free, nullptr };
static void cross_level_initialize(void *, GDExtensionInitializationLevel level) {
	GDExtensionDeviceExitInitializationLevelContextRef context{};
	CHECK(proc<GDExtensionInterfaceDeviceExitBorrowInitializeLevelV1>("device_exit_borrow_initialize_level_v1")(&abi, library, level, &context) == OK);
	if (level == GDEXTENSION_INITIALIZATION_SCENE) {
		cross_level_object = memnew(Object);
		CHECK(cross_level_object->get_instance_binding(library, &cross_level_callbacks) == cross_level_object);
		cross_level_ref.instantiate();
		CHECK(cross_level_ref->get_instance_binding(library, &cross_level_callbacks) == cross_level_ref.ptr());
		GDExtensionDeviceExitRegisteredCleanupRef cleanup{};
		CHECK(proc<GDExtensionInterfaceDeviceExitBorrowLevelCleanupV1>("device_exit_borrow_level_cleanup_v1")(&abi, context, 0, &cleanup) == OK);
		CHECK(proc<GDExtensionInterfaceDeviceExitScheduleObjectCleanupV1>("device_exit_schedule_object_cleanup_v1")(&abi, context, cleanup, uint64_t(cross_level_object->get_instance_id())) == OK);
		CHECK(proc<GDExtensionInterfaceDeviceExitScheduleObjectCleanupV1>("device_exit_schedule_object_cleanup_v1")(&abi, context, cleanup, uint64_t(cross_level_ref->get_instance_id())) == OK);
	}
	CHECK(proc<GDExtensionInterfaceDeviceExitAfterBindingInitializeV1>("device_exit_after_binding_initialize_v1")(&abi, context) == OK);
}
static void cross_level_terminate(void *, GDExtensionInitializationLevel level) {
	GDExtensionDeviceExitDeinitLevelContextRef context{};
	CHECK(proc<GDExtensionInterfaceDeviceExitBorrowDeinitLevelV1>("device_exit_borrow_deinit_level_v1")(&abi, library, level, &context) == OK);
	CHECK(proc<GDExtensionInterfaceDeviceExitBeginUserTerminatorV1>("device_exit_begin_user_terminator_v1")(&abi, token, context) == OK);
	if (level == GDEXTENSION_INITIALIZATION_SCENE) {
		Ref<RefCounted> borrowed = cross_level_ref;
		CHECK(borrowed.is_valid());
		borrowed.unref();
		cross_level_ref.unref();
		memdelete(cross_level_object);
		cross_level_object = nullptr;
	}
	CHECK(proc<GDExtensionInterfaceDeviceExitAfterUserTerminatorV1>("device_exit_after_user_terminator_v1")(&abi, context) == OK);
	CHECK(proc<GDExtensionInterfaceDeviceExitAfterBindingDeinitV1>("device_exit_after_binding_deinit_v1")(&abi, context) == OK);
}
static GDExtensionBool cross_level_entry(GDExtensionInterfaceGetProcAddress get, GDExtensionClassLibraryPtr owner, GDExtensionInitialization *out) {
	library = owner;
	*out = { GDEXTENSION_INITIALIZATION_SCENE, nullptr, cross_level_initialize, cross_level_terminate };
	GDExtensionDeviceExitGuardRegistrationV1 registration{};
	registration.abi = { 1, sizeof(registration), 0 };
	registration.binding_protocol = 1;
	registration.level_bitmap = registration.user_cleanup_bitmap = registration.binding_cleanup_bitmap = 12;
	registration.expected_initialization = *out;
	registration.user_initializer = cross_level_initialize;
	registration.user_terminator = cross_level_terminate;
	registration.preflight = preflight;
	registration.entry_classes = 1u << 6;
	registration.needs[GDEXTENSION_INITIALIZATION_SCENE] = { (1u << 1) | (1u << 5) | (1u << 7), 1u << 7 };
	auto register_guard = reinterpret_cast<GDExtensionInterfaceDeviceExitRegisterGuardV1>(get("device_exit_register_guard_v1"));
	return register_guard && register_guard(owner, &registration, &token) == OK;
}
static GDExtensionBool guarded_entry(GDExtensionInterfaceGetProcAddress get_proc, GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *result) {
	guarded_lookup_seen = get_proc("device_exit_register_guard_v1") != nullptr && get_proc("device_session_bind_v1") != nullptr;
	if (!guarded_lookup_seen) {
		return false;
	}
	return entry(get_proc, p_library, result);
}
// Exercise the getter actually selected and passed by open_library, rather
// than querying the internal interface map or selecting a getter in the test.
static constexpr const char *production_lookup_names[] = {
	"device_session_production_quote_v1",
	"device_session_production_accept_v1",
	"device_session_production_install_record_receiver_v1",
	"device_session_production_reserve_v1",
	"device_session_production_record_schedule_v1",
	"device_session_production_record_consume_v1",
	"device_session_production_append_v1",
	"device_session_production_close_segment_v1",
	"device_session_production_ack_segment_v1",
	"device_session_production_open_v1",
	"device_session_production_set_create_v1",
	"device_session_production_storage_create_v1",
	"device_session_production_storage_retire_v1",
	"device_session_production_storage_observe_v1",
	"device_session_production_storage_release_v1",
	"device_session_production_import_unit_v1",
	"device_session_production_import_storage_v1",
	"device_session_production_import_set_v1",
	"device_session_production_prepare_dispatch_v1",
	"device_session_production_prepare_copy_v1",
	"device_session_production_prepare_upload_v1",
	"device_session_production_prepare_readback_v1",
	"device_session_production_observe_v1",
	"device_session_production_facts_v1",
	"device_session_production_copy_v1",
	"device_session_production_cancel_v1",
	"device_session_production_release_v1",
	"device_session_production_close_v1",
	"device_session_production_installation_observe_v1",
};
static_assert(sizeof(production_lookup_names) / sizeof(production_lookup_names[0]) == 29);
static constexpr const char *common_lookup_names[] = {
	"get_godot_version2",
	"variant_get_ptr_builtin_method",
	"device_exit_bootstrap_library_v1",
};
static GDExtensionInterfaceFunctionPtr guarded_common_lookup[3] = {}, legacy_common_lookup[3] = {};
static bool guarded_production_lookup_seen = false, legacy_production_lookup_seen = false;
static bool check_production_lookup(GDExtensionInterfaceGetProcAddress get_proc, bool guarded) {
	bool matches = true;
	for (const char *name : production_lookup_names) {
		const bool visible = get_proc(name) != nullptr;
		CHECK_MESSAGE(visible == guarded, "Production getter profile mismatch: ", name);
		matches &= visible == guarded;
	}
	for (uint32_t i = 0; i < 3; ++i) {
		auto found = get_proc(common_lookup_names[i]);
		CHECK_MESSAGE(found != nullptr, "Common lookup disappeared: ", common_lookup_names[i]);
		matches &= found != nullptr;
		(guarded ? guarded_common_lookup : legacy_common_lookup)[i] = found;
	}
	// These original nonproduction guarded procedures retain their policy.
	CHECK((get_proc("device_exit_register_guard_v1") != nullptr) == guarded);
	CHECK((get_proc("device_session_bind_v1") != nullptr) == guarded);
	return matches;
}
static GDExtensionBool production_guarded_entry(GDExtensionInterfaceGetProcAddress get_proc, GDExtensionClassLibraryPtr owner, GDExtensionInitialization *out) {
	guarded_production_lookup_seen = check_production_lookup(get_proc, true);
	return guarded_entry(get_proc, owner, out); // Original actual guard registration is still required.
}
static void lookup_initialize(void *, GDExtensionInitializationLevel) {}
static void lookup_terminate(void *, GDExtensionInitializationLevel) {}
static GDExtensionBool production_legacy_entry(GDExtensionInterfaceGetProcAddress get_proc, GDExtensionClassLibraryPtr, GDExtensionInitialization *out) {
	legacy_production_lookup_seen = check_production_lookup(get_proc, false);
	*out = { GDEXTENSION_INITIALIZATION_SCENE, nullptr, lookup_initialize, lookup_terminate };
	return true; // Original version-zero bootstrap, with no typed enrollment or work.
}
TEST_CASE("[DeviceExit][TypedABI] scheduled scene Object survives editor binding checkpoint") {
	cross_level_frees = 0;
	cross_level_object = nullptr;
	cross_level_ref.unref();
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(cross_level_entry);
	loader->set_device_exit_guard_version(1);
	Ref<GDExtension> extension;
	extension.instantiate();
	REQUIRE(extension->open_library("libgodot://typed-cross-level-object", loader) == OK);
	REQUIRE(extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE) == OK);
	REQUIRE(extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_EDITOR) == OK);
	REQUIRE(extension->preflight_exit(GDExtensionDeviceExit::CloseRoute::EXPLICIT) == OK);
	REQUIRE(extension->deinitialize_library(GDExtension::INITIALIZATION_LEVEL_EDITOR) == OK);
	CHECK(cross_level_object != nullptr);
	CHECK(cross_level_frees == 0);
	REQUIRE(extension->deinitialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE) == OK);
	CHECK(cross_level_object == nullptr);
	CHECK(cross_level_frees == 2);
	CHECK(extension->close_library() == OK);
}
// A live unscheduled Callable must stop the manager before it announces
// shutdown or dispatches any extension terminator. This runs in an isolated
// process because the rejected attempt deliberately leaves its gate sealed.
static void manager_initialize(void *, GDExtensionInitializationLevel level) {
	GDExtensionCallableCustomInfo2 info{};
	info.token = library;
	info.call_func = callable_call;
	info.free_func = callable_free;
	info.hash_func = callable_hash;
	info.equal_func = callable_equal;
	info.less_than_func = callable_less;
	proc<GDExtensionInterfaceCallableCustomCreate2>("callable_custom_create2")(&retained_callable, &info);
	GDExtensionDeviceExitInitializationLevelContextRef context{};
	if (proc<GDExtensionInterfaceDeviceExitBorrowInitializeLevelV1>("device_exit_borrow_initialize_level_v1")(&abi, library, level, &context) != OK ||
			proc<GDExtensionInterfaceDeviceExitAfterBindingInitializeV1>("device_exit_after_binding_initialize_v1")(&abi, context) != OK) {
		std::_Exit(93);
	}
}
static void manager_terminate(void *, GDExtensionInitializationLevel) {
	std::_Exit(94);
}
static GDExtensionBool manager_entry(GDExtensionInterfaceGetProcAddress get, GDExtensionClassLibraryPtr owner, GDExtensionInitialization *out) {
	library = owner;
	*out = { GDEXTENSION_INITIALIZATION_SCENE, nullptr, manager_initialize, manager_terminate };
	GDExtensionDeviceExitGuardRegistrationV1 registration{};
	registration.abi = { 1, sizeof(registration), 0 };
	registration.binding_protocol = 1;
	registration.level_bitmap = registration.user_cleanup_bitmap = registration.binding_cleanup_bitmap = 12;
	registration.expected_initialization = *out;
	registration.user_initializer = manager_initialize;
	registration.user_terminator = manager_terminate;
	registration.preflight = preflight;
	registration.entry_classes = 1u << 8;
	auto register_guard = reinterpret_cast<GDExtensionInterfaceDeviceExitRegisterGuardV1>(get("device_exit_register_guard_v1"));
	return register_guard && register_guard(owner, &registration, &token) == OK;
}
static void manager_shutdown_preflight_child() {
	callable_frees = 0;
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(manager_entry);
	loader->set_device_exit_guard_version(1);
	auto *manager = GDExtensionManager::get_singleton();
	const String path = "libgodot://typed-manager-shutdown-preflight";
	if (manager->load_extension_with_loader(path, loader) != GDExtensionManager::LOAD_STATUS_OK) {
		std::_Exit(91);
	}
	const Error stopped = manager->shutdown();
	const bool intact = stopped == ERR_BUSY && manager->is_extension_loaded(path) && manager->get_extension(path)->is_library_open() && callable_frees == 0;
	std::fprintf(stderr, "TYPED_MANAGER_SHUTDOWN_PREFLIGHT intact=%d error=%d\n", int(intact), int(stopped));
	std::fflush(stderr);
	std::_Exit(intact ? 0 : 92);
}
REGISTER_TEST_COMMAND("--device-exit-typed-manager-shutdown-preflight", manager_shutdown_preflight_child);
static void manager_level_preflight_child() {
	callable_frees = 0;
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(manager_entry);
	loader->set_device_exit_guard_version(1);
	auto *manager = GDExtensionManager::get_singleton();
	const String path = "libgodot://typed-manager-level-preflight";
	if (manager->load_extension_with_loader(path, loader) != GDExtensionManager::LOAD_STATUS_OK) {
		std::_Exit(91);
	}
	const Error stopped = manager->deinitialize_extensions(GDExtension::INITIALIZATION_LEVEL_EDITOR);
	const bool intact = stopped == ERR_BUSY && manager->is_extension_loaded(path) && manager->get_extension(path)->is_library_open() && callable_frees == 0;
	std::fprintf(stderr, "TYPED_MANAGER_LEVEL_PREFLIGHT intact=%d error=%d\n", int(intact), int(stopped));
	std::fflush(stderr);
	std::_Exit(intact ? 0 : 92);
}
REGISTER_TEST_COMMAND("--device-exit-typed-manager-level-preflight", manager_level_preflight_child);
// Keep this failure in a child: a rejected G1 remains sealed, so its
// unscheduled Object must never be destroyed by the ordinary test runner.
static Object *unscheduled_object = nullptr;
static int unscheduled_object_frees = 0;
static void *unscheduled_binding_create(void *, void *object) {
	return object;
}
static void unscheduled_binding_free(void *, void *, void *) {
	unscheduled_object_frees++;
}
static const GDExtensionInstanceBindingCallbacks unscheduled_callbacks{ unscheduled_binding_create, unscheduled_binding_free, nullptr };
static void unscheduled_initialize(void *, GDExtensionInitializationLevel level) {
	unscheduled_object = memnew(Object);
	if (unscheduled_object->get_instance_binding(library, &unscheduled_callbacks) != unscheduled_object) {
		std::_Exit(95);
	}
	GDExtensionDeviceExitInitializationLevelContextRef context{};
	if (proc<GDExtensionInterfaceDeviceExitBorrowInitializeLevelV1>("device_exit_borrow_initialize_level_v1")(&abi, library, level, &context) != OK ||
			proc<GDExtensionInterfaceDeviceExitAfterBindingInitializeV1>("device_exit_after_binding_initialize_v1")(&abi, context) != OK) {
		std::_Exit(96);
	}
}
static void unscheduled_terminate(void *, GDExtensionInitializationLevel) {
	std::_Exit(97);
}
static int32_t unscheduled_preflight(GDExtensionDeviceExitDeinitPreflightContextRef context, void *) {
	GDExtensionDeviceExitExitDiagnosticV1 diagnostic{ { 1, sizeof(GDExtensionDeviceExitExitDiagnosticV1), 0 }, OK, GDEXTENSION_INITIALIZATION_SCENE, 0 };
	return proc<GDExtensionInterfaceDeviceExitPublishExitResultV1>("device_exit_publish_exit_result_v1")(&abi, token, context, &diagnostic);
}
static GDExtensionBool unscheduled_entry(GDExtensionInterfaceGetProcAddress get, GDExtensionClassLibraryPtr owner, GDExtensionInitialization *out) {
	library = owner;
	*out = { GDEXTENSION_INITIALIZATION_SCENE, nullptr, unscheduled_initialize, unscheduled_terminate };
	GDExtensionDeviceExitGuardRegistrationV1 registration{};
	registration.abi = { 1, sizeof(registration), 0 };
	registration.binding_protocol = 1;
	registration.level_bitmap = registration.user_cleanup_bitmap = registration.binding_cleanup_bitmap = 12;
	registration.expected_initialization = *out;
	registration.user_initializer = unscheduled_initialize;
	registration.user_terminator = unscheduled_terminate;
	registration.preflight = unscheduled_preflight;
	registration.entry_classes = 1u << 6;
	auto register_guard = reinterpret_cast<GDExtensionInterfaceDeviceExitRegisterGuardV1>(get("device_exit_register_guard_v1"));
	return register_guard && register_guard(owner, &registration, &token) == OK;
}
static void unscheduled_object_preflight_child() {
	unscheduled_object_frees = 0;
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(unscheduled_entry);
	loader->set_device_exit_guard_version(1);
	auto *manager = GDExtensionManager::get_singleton();
	const String path = "libgodot://typed-unscheduled-object";
	if (manager->load_extension_with_loader(path, loader) != GDExtensionManager::LOAD_STATUS_OK) {
		std::_Exit(91);
	}
	const Error stopped = manager->shutdown();
	const bool intact = stopped == ERR_BUSY && manager->is_extension_loaded(path) && manager->get_extension(path)->is_library_open() && unscheduled_object_frees == 0;
	std::fprintf(stderr, "TYPED_UNSCHEDULED_OBJECT_PREFLIGHT intact=%d error=%d\n", int(intact), int(stopped));
	std::fflush(stderr);
	std::_Exit(intact ? 0 : 92);
}
REGISTER_TEST_COMMAND("--device-exit-typed-unscheduled-object-preflight", unscheduled_object_preflight_child);
TEST_CASE("[DeviceExit][TypedABI] original producer identity survives code close without reopening admission") {
	using namespace GDExtensionDeviceExit;
	auto &registry = Registry::get_singleton();
	struct ProducerTestCleanup {
		~ProducerTestCleanup() {
			exercise_producer = false;
			producer.reset();
			if (producer_abi.handle_serial) {
				Registry::get_singleton().release_producer_registration_view(producer_abi);
				producer_abi = {};
			}
		}
	} cleanup;
	exercise_producer = true;
	ProducerRegistrationIdentity previous{};
	ProducerRegistrationRef historical;
	for (int attempt = 0; attempt < 2; ++attempt) {
		token = {};
		hold = {};
		Ref<GDExtensionFunctionLoader> loader;
		loader.instantiate();
		loader->set_initialization_function(guarded_entry);
		loader->set_device_exit_guard_version(1);
		Ref<GDExtension> extension;
		extension.instantiate();
		REQUIRE_EQ(extension->open_library("libgodot://producer-registration", loader), OK);
		REQUIRE_EQ(extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE), OK);
		REQUIRE(producer.is_valid());
		ProducerRegistrationIdentity identity;
		CHECK_EQ(registry.copy_producer_registration(producer, identity), OK);
		auto clone_registration = proc<GDExtensionInterfaceDeviceSessionCloneProducerRegistrationV1>("device_session_clone_producer_registration_v1");
		auto copy_registration = proc<GDExtensionInterfaceDeviceSessionCopyProducerRegistrationV1>("device_session_copy_producer_registration_v1");
		auto release_registration = proc<GDExtensionInterfaceDeviceSessionReleaseProducerRegistrationV1>("device_session_release_producer_registration_v1");
		REQUIRE(clone_registration != nullptr);
		REQUIRE(copy_registration != nullptr);
		REQUIRE(release_registration != nullptr);
		GDExtensionProducerRegistrationRefV1 clone{};
		struct CloneCleanup {
			GDExtensionProducerRegistrationRefV1 &ref;
			~CloneCleanup() {
				if (ref.handle_serial) {
					Registry::get_singleton().release_producer_registration_view(ref);
				}
			}
		} clone_cleanup{ clone };
		CHECK_EQ(clone_registration(&abi, producer_abi, &clone), OK);
		CHECK_NE(clone.handle_serial, producer_abi.handle_serial);
		GDExtensionDeviceExitLibrarySnapshot wrong_view{ { 1, sizeof(GDExtensionDeviceExitLibrarySnapshot), 0 }, 0, 0, 0, 0, 0, 0, 0 };
		CHECK(registry.copy({ producer_abi.registry_instance, producer_abi.handle_serial }, &wrong_view) != OK);
		ProducerRegistrationRef refused;
		CHECK(registry.bind_host_producer(producer_initialize, { hold.registry_instance, hold.handle_serial }, &producer_identity, refused) != OK);
		CHECK_FALSE(refused.is_valid());
		if (attempt) {
			CHECK(std::strcmp(identity.label, previous.label) != 0);
			CHECK_NE(identity.library_generation, previous.library_generation);
			ProducerRegistrationIdentity old;
			CHECK_EQ(registry.copy_producer_registration(historical, old), OK);
			CHECK(std::strcmp(old.label, previous.label) == 0);
			CHECK_EQ(TestDeviceExitContract::test_producer_admission(historical), ERR_UNAVAILABLE);
		}
		REQUIRE_EQ(extension->preflight_exit(CloseRoute::EXPLICIT), OK);
		CHECK_EQ(TestDeviceExitContract::test_producer_admission(producer), ERR_UNAVAILABLE);
		REQUIRE_EQ(extension->deinitialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE), OK);
		CHECK_EQ(extension->close_library(), OK);
		ProducerRegistrationIdentity closed;
		CHECK_EQ(registry.copy_producer_registration(producer, closed), OK);
		CHECK(std::strcmp(identity.label, closed.label) == 0);
		CHECK_EQ(TestDeviceExitContract::test_producer_admission(producer), ERR_UNAVAILABLE);
		GDExtensionProducerRegistrationIdentityV1 exported{};
		exported.abi = { 1, sizeof(exported), 0 };
		CHECK_EQ(copy_registration(&abi, clone, &exported), OK);
		CHECK(std::strcmp(identity.label, exported.label) == 0);
		CHECK_EQ(exported.library_generation, identity.library_generation);
		CHECK_EQ(release_registration(&abi, producer_abi), OK);
		CHECK_EQ(release_registration(&abi, producer_abi), ERR_INVALID_DATA);
		ProducerRegistrationRef expired;
		CHECK_EQ(registry.import_producer_registration(producer_abi, expired), ERR_INVALID_DATA);
		CHECK_FALSE(expired.is_valid());
		producer_abi = {};
		CHECK_EQ(copy_registration(&abi, clone, &exported), OK);
		CHECK_EQ(release_registration(&abi, clone), OK);
		clone = {};
		if (!attempt) {
			CHECK_EQ(registry.clone_producer_registration(producer, historical), OK);
			previous = closed;
		}
		producer.reset();
	}
	historical.reset();
	exercise_producer = false;
}

TEST_CASE("[DeviceExit][TypedABI] version one selects guarded lookup and requires enrollment") {
	guarded_lookup_seen = false;
	guarded_production_lookup_seen = legacy_production_lookup_seen = false;
	token = {};
	hold = {};
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(production_guarded_entry);
	loader->set_device_exit_guard_version(1);
	Ref<GDExtension> extension;
	extension.instantiate();
	REQUIRE(extension->open_library("libgodot://typed-production", loader) == OK);
	CHECK(guarded_lookup_seen);
	CHECK(guarded_production_lookup_seen);
	REQUIRE(extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE) == OK);
	REQUIRE(extension->preflight_exit(GDExtensionDeviceExit::CloseRoute::EXPLICIT) == OK);
	REQUIRE(extension->deinitialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE) == OK);
	CHECK(extension->close_library() == OK);

	Ref<GDExtensionFunctionLoader> legacy_loader;
	legacy_loader.instantiate();
	legacy_loader->set_initialization_function(production_legacy_entry);
	legacy_loader->set_device_exit_guard_version(0);
	Ref<GDExtension> legacy_extension;
	legacy_extension.instantiate();
	REQUIRE(legacy_extension->open_library("libgodot://typed-production-legacy-lookup", legacy_loader) == OK);
	CHECK(legacy_production_lookup_seen);
	for (uint32_t i = 0; i < 3; ++i) {
		CHECK(guarded_common_lookup[i] == legacy_common_lookup[i]);
	}
	CHECK(legacy_extension->close_library() == OK);
}
static GDExtensionBool missing_guard_entry(GDExtensionInterfaceGetProcAddress get_proc, GDExtensionClassLibraryPtr, GDExtensionInitialization *result) {
	guarded_lookup_seen = get_proc("device_exit_register_guard_v1") != nullptr;
	*result = { GDEXTENSION_INITIALIZATION_SCENE, nullptr, initialize, terminate };
	return true;
}
TEST_CASE("[DeviceExit][TypedABI] version one refuses an entry without guard registration") {
	guarded_lookup_seen = false;
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(missing_guard_entry);
	loader->set_device_exit_guard_version(1);
	Ref<GDExtension> extension;
	extension.instantiate();
	ERR_PRINT_OFF;
	CHECK(extension->open_library("libgodot://typed-no-enrollment", loader) == ERR_UNAVAILABLE);
	ERR_PRINT_ON;
	CHECK(guarded_lookup_seen);
	CHECK_FALSE(loader->is_library_open());
}
TEST_CASE("[DeviceExit][TypedABI] version one loads an actual dynamic image") {
	const String image = DirAccess::create(DirAccess::ACCESS_FILESYSTEM)->get_current_dir().path_join("tests/core/object/fixtures/libtyped_dynamic.dylib");
	if (!FileAccess::exists(image)) {
		TEST_FAIL_COND_WARN(true, "Optional native dynamic fixture is absent; this dynamic-image case was not executed. Build it using tests/core/object/fixtures/README.md.");
		return;
	}
	const String path = TestUtils::get_temp_path("typed-dynamic.gdextension");
	Ref<ConfigFile> config;
	config.instantiate();
	config->set_value("configuration", "entry_symbol", "typed_dynamic_entry");
	config->set_value("configuration", "compatibility_minimum", "4.1");
	config->set_value("configuration", "device_exit_guard_version", 1);
	config->set_value("libraries", OS::get_singleton()->get_name().to_lower(), image);
	REQUIRE(config->save(path) == OK);
	Ref<GDExtensionLibraryLoader> loader;
	loader.instantiate();
	Ref<GDExtension> extension;
	extension.instantiate();
	REQUIRE(extension->open_library(path, loader) == OK);
	REQUIRE(extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE) == OK);
	REQUIRE(extension->preflight_exit(GDExtensionDeviceExit::CloseRoute::EXPLICIT) == OK);
	REQUIRE(extension->deinitialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE) == OK);
	CHECK(extension->close_library() == OK);
	CHECK(DirAccess::remove_absolute(path) == OK);
}
TEST_CASE("[DeviceExit][TypedABI] manager loads guarded SCENE from its declared minimum") {
	const String image = DirAccess::create(DirAccess::ACCESS_FILESYSTEM)->get_current_dir().path_join("tests/core/object/fixtures/libtyped_dynamic.dylib");
	if (!FileAccess::exists(image)) {
		TEST_FAIL_COND_WARN(true, "Optional native dynamic fixture is absent; this dynamic-image case was not executed. Build it using tests/core/object/fixtures/README.md.");
		return;
	}
	const String path = TestUtils::get_temp_path("typed-manager.gdextension");
	Ref<ConfigFile> config;
	config.instantiate();
	config->set_value("configuration", "entry_symbol", "typed_dynamic_entry");
	config->set_value("configuration", "compatibility_minimum", "4.1");
	config->set_value("configuration", "device_exit_guard_version", 1);
	config->set_value("libraries", OS::get_singleton()->get_name().to_lower(), image);
	REQUIRE(config->save(path) == OK);
	GDExtensionManager *manager = GDExtensionManager::get_singleton();
	REQUIRE(manager != nullptr);
	CHECK(manager->load_extension(path) == GDExtensionManager::LOAD_STATUS_OK);
	Ref<GDExtension> extension = manager->get_extension(path);
	REQUIRE(extension.is_valid());
	CHECK(extension->get_minimum_library_initialization_level() == GDExtension::INITIALIZATION_LEVEL_SCENE);
	// The manager may already be at SCENE or EDITOR. No CORE/SERVERS call
	// may reach the guarded entry after it declared SCENE as its minimum.
	CHECK(manager->unload_extension(path) == GDExtensionManager::LOAD_STATUS_OK);
	extension.unref();
	CHECK(DirAccess::remove_absolute(path) == OK);
}
TEST_CASE("[DeviceExit][TypedABI] generated procs drive original bootstrap and SCENE lifetime") {
	SUBCASE("SI2_03_single_reservation") {
		::TestDeviceExitEntry::test_v2_single_registration();
	}

	REQUIRE(GDExtensionDeviceExit::test_typed_lifecycle_lookup("device_exit_register_guard_v1") != nullptr);
	if (!GDExtensionDeviceExit::test_typed_lifecycle_lookup("device_exit_register_guard_v1")) {
		return;
	}
	token = {};
	hold = {};
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(entry);
	Ref<GDExtension> extension;
	extension.instantiate();
	REQUIRE(extension->open_library("libgodot://typed-abi", loader) == OK);
	REQUIRE(extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE) == OK);
	CHECK(proc<GDExtensionInterfaceDeviceExitReleaseCodeHoldV1>("device_exit_release_code_hold_v1")(&abi, hold) == ERR_INVALID_DATA);
	CHECK(GDExtensionDeviceExit::Registry::get_singleton().service_stopping(1u << 5) == ERR_BUSY);
	CHECK(GDExtensionDeviceExit::Registry::get_singleton().service_stopping(1u << 1) == ERR_BUSY);
	CHECK(GDExtensionDeviceExit::Registry::get_singleton().service_stopping(1u << 7) == ERR_BUSY);
	REQUIRE(extension->preflight_exit(GDExtensionDeviceExit::CloseRoute::EXPLICIT) == OK);
	REQUIRE(extension->deinitialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE) == OK);
	CHECK(extension->close_library() == OK);
	CHECK(proc<GDExtensionInterfaceDeviceExitReleaseCodeHoldV1>("device_exit_release_code_hold_v1")(&abi, hold) == ERR_INVALID_DATA);
}
TEST_CASE("[DeviceExit][TypedABI] original Object cleanup schedule gates USER and BINDING") {
	exercise_object_cleanup = true;
	object_frees = 0;
	token = {};
	hold = {};
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(entry);
	Ref<GDExtension> extension;
	extension.instantiate();
	REQUIRE(extension->open_library("libgodot://typed-object-schedule", loader) == OK);
	REQUIRE(extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE) == OK);
	REQUIRE(user_object != nullptr);
	REQUIRE(binding_object != nullptr);
	REQUIRE(extension->preflight_exit(GDExtensionDeviceExit::CloseRoute::EXPLICIT) == OK);
	REQUIRE(extension->deinitialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE) == OK);
	CHECK(extension->close_library() == OK);
	CHECK(object_frees == 2);
	exercise_object_cleanup = false;
}
static GDExtensionBool foreign_token_entry(GDExtensionInterfaceGetProcAddress, GDExtensionClassLibraryPtr, GDExtensionInitialization *result) {
	GDExtensionDeviceExit::RecordRef owner;
	CHECK(GDExtensionDeviceExit::Registry::get_singleton().retain_callable_owner(library, owner) == ERR_UNAUTHORIZED);
	CHECK_FALSE(owner.is_valid());
	*result = { GDEXTENSION_INITIALIZATION_SCENE, nullptr, initialize, terminate };
	return true;
}
TEST_CASE("[DeviceExit][TypedABI] guarded callable token cannot borrow a legacy frame") {
	token = {};
	hold = {};
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(entry);
	Ref<GDExtension> extension;
	extension.instantiate();
	REQUIRE(extension->open_library("libgodot://typed-owner", loader) == OK);
	GDExtensionDeviceExit::RecordRef owner;
	CHECK(GDExtensionDeviceExit::Registry::get_singleton().retain_callable_owner(library, owner) == ERR_UNAUTHORIZED);
	CHECK_FALSE(owner.is_valid());
	Ref<GDExtensionFunctionLoader> legacy_loader;
	legacy_loader.instantiate();
	legacy_loader->set_initialization_function(foreign_token_entry);
	Ref<GDExtension> legacy;
	legacy.instantiate();
	REQUIRE(legacy->open_library("libgodot://foreign-token", legacy_loader) == OK);
	CHECK(legacy->close_library() == OK);
	CHECK(extension->close_library() == OK);
}
TEST_CASE("[DeviceExit][TypedABI] unavailable service refuses initializer before admission") {
	token = {};
	hold = {};
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(entry);
	Ref<GDExtension> extension;
	extension.instantiate();
	REQUIRE(extension->open_library("libgodot://typed-service-admission", loader) == OK);
	auto &registry = GDExtensionDeviceExit::Registry::get_singleton();
	REQUIRE(registry.service_stopping(1u << 5) == OK);
	ERR_PRINT_OFF;
	CHECK(extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE) == ERR_UNAVAILABLE);
	ERR_PRINT_ON;
	CHECK(hold.handle_serial == 0);
	registry.service_started(1u << 5);
	REQUIRE(extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE) == OK);
	CHECK(extension->preflight_exit(GDExtensionDeviceExit::CloseRoute::EXPLICIT) == OK);
	CHECK(extension->deinitialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE) == OK);
	CHECK(extension->close_library() == OK);
}
TEST_CASE("[DeviceExit][TypedABI] original normal relay observes exact Callable retirement") {
	using namespace GDExtensionDeviceExit;
	auto &registry = Registry::get_singleton();
	exercise_callable = true;
	callable_calls = callable_frees = 0;
	observed_relay = {};
	relay_observed_closed_in_free = false;
	token = {};
	hold = {};
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(entry);
	Ref<GDExtension> extension;
	extension.instantiate();
	REQUIRE(extension->open_library("libgodot://typed-normal-relay", loader) == OK);
	REQUIRE(extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE) == OK);

	NormalRelayToken refused;
	CHECK(registry.register_normal_relay(Callable(), refused) == ERR_INVALID_DATA);
	CHECK(refused.serial == 0);
	// Bind/unbind wrappers are not the direct enrolled holder domain.
	CHECK(registry.register_normal_relay(retained_callable.unbind(1), refused) == ERR_UNAVAILABLE);
	const int validations_before_relay = callable_validations;
	const int hashes_before_relay = callable_hashes;
	REQUIRE(registry.register_normal_relay(retained_callable, observed_relay) == OK);
	CHECK(callable_validations == validations_before_relay);
	CHECK(callable_hashes == hashes_before_relay);
	const NormalRelayToken original = observed_relay;
	CHECK(registry.register_normal_relay(equivalent_callable, observed_relay) == ERR_INVALID_DATA);
	CHECK(observed_relay.serial == original.serial);
	NormalRelayToken foreign = original;
	foreign.registry_instance++;
	NormalRelaySnapshot snapshot;
	CHECK(registry.observe_normal_relay(foreign, snapshot) == ERR_INVALID_DATA);
	REQUIRE(registry.observe_normal_relay(original, snapshot) == OK);
	CHECK(snapshot.library_generation != 0);
	CHECK_FALSE(snapshot.cancelled);
	CHECK_FALSE(snapshot.cell_released);
	CHECK_FALSE(snapshot.holder_closed);
	CHECK(registry.forget_normal_relay(original) == ERR_BUSY);

	Error wrong_thread = OK;
	std::thread other([&] { wrong_thread = registry.release_normal_relay_holder(original); });
	other.join();
	CHECK(wrong_thread == ERR_UNAUTHORIZED);
	REQUIRE(registry.observe_normal_relay(original, snapshot) == OK);
	CHECK_FALSE(snapshot.cancelled);
	Variant result;
	Callable::CallError error;
	CHECK(registry.call_normal_relay(original, nullptr, 0, result, error) == OK);
	CHECK(callable_calls == 1);
	CHECK(int(result) == 42);

	SUBCASE("cancellation and one registry copy do not acknowledge queued external holder") {
		CallQueue queue;
		REQUIRE(queue.push_callable(retained_callable) == OK);
		CHECK(registry.cancel_normal_relay(original) == OK);
		CHECK(registry.cancel_normal_relay(original) == OK);
		CHECK(registry.call_normal_relay(original, nullptr, 0, result, error) == ERR_UNAVAILABLE);
		CHECK(callable_calls == 1);
		CHECK(registry.release_normal_relay_holder(original) == OK);
		REQUIRE(registry.observe_normal_relay(original, snapshot) == OK);
		CHECK(snapshot.cell_released);
		CHECK_FALSE(snapshot.holder_closed);
		retained_callable = Callable();
		equivalent_callable = Callable();
		CHECK(callable_frees == 1);
		CHECK(extension->preflight_exit(CloseRoute::EXPLICIT) == ERR_BUSY);
		CHECK(queue.flush() == OK);
		CHECK(callable_calls == 1);
	}
	SUBCASE("release inside admitted relay waits for actual entry return") {
		release_relay_in_call = true;
		relay_observed_active = false;
		CHECK(registry.call_normal_relay(original, nullptr, 0, result, error) == OK);
		release_relay_in_call = false;
		CHECK(relay_observed_active);
		CHECK(relay_release_in_call == ERR_BUSY);
		REQUIRE(registry.observe_normal_relay(original, snapshot) == OK);
		CHECK(snapshot.active_deliveries == 0);
		CHECK(snapshot.release_requested);
		CHECK_FALSE(snapshot.cell_released);
		retained_callable = Callable();
		equivalent_callable = Callable();
		CHECK(registry.release_normal_relay_holder(original) == OK);
	}
	SUBCASE("holder acknowledgment does not borrow another Callable in the same generation") {
		retained_callable = Callable();
		CHECK(registry.release_normal_relay_holder(original) == OK);
		REQUIRE(registry.observe_normal_relay(original, snapshot) == OK);
		CHECK(snapshot.holder_closed);
		CHECK(callable_frees == 1);
		CHECK(extension->preflight_exit(CloseRoute::EXPLICIT) == ERR_BUSY);
		observed_relay = {};
		equivalent_callable = Callable();
		observed_relay = original;
	}
	SUBCASE("original generation close suppresses delivery before explicit cancellation") {
		CHECK(extension->preflight_exit(CloseRoute::EXPLICIT) == ERR_BUSY);
		CHECK(registry.call_normal_relay(original, nullptr, 0, result, error) == ERR_UNAVAILABLE);
		CHECK(callable_calls == 1);
		CHECK(registry.register_normal_relay(retained_callable, refused) == ERR_UNAVAILABLE);
		CHECK(refused.serial == 0);
		retained_callable = Callable();
		equivalent_callable = Callable();
		CHECK(registry.release_normal_relay_holder(original) == OK);
	}
	CHECK(callable_frees == 2);
	CHECK_FALSE(relay_observed_closed_in_free);
	REQUIRE(registry.observe_normal_relay(original, snapshot) == OK);
	CHECK(snapshot.cell_released);
	CHECK(snapshot.holder_closed);
	CHECK(snapshot.active_deliveries == 0);
	CHECK(registry.forget_normal_relay(original) == OK);
	CHECK(registry.observe_normal_relay(original, snapshot) == ERR_INVALID_DATA);
	CHECK(registry.cancel_normal_relay(original) == ERR_INVALID_DATA);
	CHECK(registry.release_normal_relay_holder(original) == ERR_INVALID_DATA);
	observed_relay = {};
	CHECK(extension->preflight_exit(CloseRoute::EXPLICIT) == OK);
	CHECK(extension->deinitialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE) == OK);
	CHECK(extension->close_library() == OK);
	exercise_callable = false;
}

TEST_CASE("[DeviceExit][TypedABI] Callable holder blocks close until actual final free") {
	exercise_callable = true;
	callable_calls = callable_frees = callable_hashes = 0;
	callable_validations = callable_strings = callable_argument_counts = 0;
	token = {};
	hold = {};
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(entry);
	Ref<GDExtension> extension;
	extension.instantiate();
	REQUIRE(extension->open_library("libgodot://typed-callable", loader) == OK);
	REQUIRE(extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE) == OK);
	CHECK(callable_hashes == 2);
	CHECK(retained_callable.is_valid());
	CHECK(String(retained_callable) == "typed callable");
	bool count_valid = false;
	CHECK(retained_callable.get_argument_count(&count_valid) == 0);
	CHECK(count_valid);
	CHECK(callable_strings == 1);
	CHECK(callable_argument_counts == 1);
	CHECK(retained_callable == equivalent_callable);
	CHECK_FALSE(retained_callable < equivalent_callable);
	Callable copy = retained_callable;
	CallQueue queue;
	REQUIRE(queue.push_callable(copy) == OK);
	Variant result;
	Callable::CallError error;
	copy.callp(nullptr, 0, result, error);
	CHECK(callable_calls == 1);
	CHECK(int(result) == 42);
	CHECK(extension->preflight_exit(GDExtensionDeviceExit::CloseRoute::EXPLICIT) == ERR_BUSY);
	CHECK(copy == equivalent_callable);
	CHECK_FALSE(copy < equivalent_callable);
	const int validations_before = callable_validations;
	CHECK_FALSE(copy.is_valid());
	CHECK(String(copy) == "<CallableCustom>");
	CHECK(copy.get_argument_count(&count_valid) == 0);
	CHECK_FALSE(count_valid);
	CHECK(callable_validations == validations_before);
	CHECK(callable_strings == 1);
	CHECK(callable_argument_counts == 1);
	CHECK(extension->preflight_exit(GDExtensionDeviceExit::CloseRoute::EXPLICIT) == ERR_BUSY);
	equivalent_callable = Callable();
	CHECK(callable_frees == 1);
	copy.callp(nullptr, 0, result, error);
	CHECK(error.error == Callable::CallError::CALL_ERROR_INSTANCE_IS_NULL);
	CHECK(callable_calls == 1);
	retained_callable = Callable();
	CHECK(callable_frees == 1);
	copy = Callable();
	CHECK(callable_frees == 1);
	CHECK(extension->preflight_exit(GDExtensionDeviceExit::CloseRoute::EXPLICIT) == ERR_BUSY);
	CHECK(queue.flush() == OK);
	CHECK(callable_calls == 1);
	CHECK(callable_frees == 2);
	CHECK(extension->preflight_exit(GDExtensionDeviceExit::CloseRoute::EXPLICIT) == OK);
	CHECK(extension->deinitialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE) == OK);
	CHECK(extension->close_library() == OK);
	exercise_callable = false;
}
TEST_CASE("[DeviceExit][TypedABI][PresealPrepare] failed bootstrap retires registered prepare without invoking it") {
	exercise_callable = false;
	exercise_prepare = fail_after_prepare_registration = true;
	token = {}; hold = {}; prepare_calls = 0;
	Ref<GDExtensionFunctionLoader> loader; loader.instantiate(); loader->set_initialization_function(entry);
	loader->set_device_exit_guard_version(1);
	Ref<GDExtension> extension; extension.instantiate();
	ERR_PRINT_OFF;
	const Error opened = extension->open_library("libgodot://typed-preseal-bootstrap-abort", loader);
	ERR_PRINT_ON;
	CHECK(opened != OK);
	CHECK_FALSE(extension->is_library_open());
	CHECK_FALSE(loader->is_library_open());
	CHECK(prepare_calls == 0); // Failed PENDING_BOOTSTRAP is never an admitted callback.
	fail_after_prepare_registration = false;
	token = {}; hold = {}; // Retired opaque outputs cannot be reused as empty registration outputs.
	loader->set_initialization_function(entry); // Actual close consumes the function-loader entry.
	REQUIRE(extension->open_library("libgodot://typed-preseal-bootstrap-retry", loader) == OK);
	REQUIRE(extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE) == OK);
	prepare_pending = 0; prepare_write = true; prepare_result = OK;
	CHECK(extension->preflight_exit(GDExtensionDeviceExit::CloseRoute::EXPLICIT) == OK);
	CHECK(prepare_calls == 1);
	CHECK(extension->deinitialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE) == OK);
	CHECK(extension->close_library() == OK);
	exercise_prepare = false;
}

TEST_CASE("[DeviceExit][TypedABI][PresealPrepare] prepared bootstrap rejects late level initialization before seal") {
	exercise_callable = false;
	exercise_prepare = true;
	token = {}; hold = {}; prepare_calls = 0;
	prepare_pending = 0; prepare_write = true; prepare_result = OK;
	Ref<GDExtensionFunctionLoader> loader; loader.instantiate(); loader->set_initialization_function(entry);
	loader->set_device_exit_guard_version(1);
	Ref<GDExtension> extension; extension.instantiate();
	REQUIRE(extension->open_library("libgodot://typed-preseal-late-initialize", loader) == OK);
	REQUIRE(extension->prepare_exit(GDExtensionDeviceExit::CloseRoute::EXPLICIT) == OK);
	CHECK(prepare_calls == 1);
	ERR_PRINT_OFF;
	const Error initialized = extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE);
	ERR_PRINT_ON;
	CHECK(initialized == ERR_BUSY);
	REQUIRE(extension->preflight_exit(GDExtensionDeviceExit::CloseRoute::EXPLICIT) == OK);
	CHECK(prepare_calls == 1);
	if (initialized == OK) { // Preserve actual teardown on the red implementation.
		CHECK(extension->deinitialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE) == OK);
	}
	CHECK(extension->close_library() == OK);
	exercise_prepare = false;
}

TEST_CASE("[DeviceExit][TypedABI][PresealPrepare] final Callable retirement waits for prepare return before forget") {
	using namespace GDExtensionDeviceExit;
	// Reset mode/state even if a REQUIRE aborts this test. Original teardown still
	// uses the existing fixture; no synthetic holder cleanup is introduced here.
	struct ResetPrepareRelayState {
		~ResetPrepareRelayState() {
			exercise_callable = exercise_prepare = prepare_retire_relay = false;
			prepare_calls = callable_calls = callable_frees = 0;
			prepare_pending = 1; prepare_write = true; prepare_result = OK;
			prepare_reenter = false; prepare_extension = nullptr;
			observed_relay = {}; prepare_relay_snapshot = {}; prepare_relay_forget_result = OK; prepare_relay_forgotten = false;
			relay_observed_closed_in_free = false;
		}
	} reset;
	exercise_callable = exercise_prepare = prepare_retire_relay = true;
	token = {}; hold = {}; observed_relay = {}; prepare_relay_snapshot = {}; prepare_relay_forget_result = OK; prepare_relay_forgotten = false;
	prepare_calls = callable_calls = callable_frees = 0;
	relay_observed_closed_in_free = false;
	auto &registry = Registry::get_singleton();
	Ref<GDExtensionFunctionLoader> loader; loader.instantiate(); loader->set_initialization_function(entry);
	Ref<GDExtension> extension; extension.instantiate();
	// Declared after extension/loader: owner cleanup runs before their destructors
	// on REQUIRE unwinding, including setup failures and early native forget.
	struct ClosePrepareRelayOwner {
		GDExtension *extension;
		bool scene_initialized = false;
		~ClosePrepareRelayOwner() {
			if (!extension->is_library_open()) { return; }
			Error prepared = ERR_BUSY;
			// A real scope return can retire holders between attempts; no spin
			// occurs inside the callback and no holder observation is fabricated.
			for (int attempt = 0; attempt < 3 && prepared == ERR_BUSY; ++attempt) {
				prepared = extension->preflight_exit(CloseRoute::EXPLICIT);
			}
			CHECK(prepared == OK);
			if (prepared != OK) { return; }
			if (scene_initialized) {
				const Error deinitialized = extension->deinitialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE);
				CHECK(deinitialized == OK);
				if (deinitialized != OK) { return; }
			}
			CHECK(extension->close_library() == OK);
		}
	} close_owner{ extension.ptr() };
	REQUIRE(extension->open_library("libgodot://typed-prepare-final-callable", loader) == OK);
	const Error initialized = extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE);
	// The fixture issues hold only from initialize; SCENE ready may report
	// an error after that original initializer has already run.
	close_owner.scene_initialized = initialized == OK || hold.handle_serial != 0;
	REQUIRE(initialized == OK);
	REQUIRE(registry.register_normal_relay(retained_callable, observed_relay) == OK);
	const NormalRelayToken original = observed_relay;

	CHECK(extension->preflight_exit(CloseRoute::EXPLICIT) == ERR_BUSY);
	CHECK(prepare_calls == 1);
	CHECK(prepare_relay_forget_result == ERR_BUSY);
	CHECK(prepare_relay_snapshot.cell_released);
	CHECK_FALSE(prepare_relay_snapshot.holder_closed); // Observed inside prepare.
	CHECK(callable_frees == 2); // Supporting evidence only; native observation owns closure.
	CHECK_FALSE(relay_observed_closed_in_free);
	CHECK(retained_callable.is_null());
	CHECK(equivalent_callable.is_null());
	NormalRelaySnapshot after_return;
	REQUIRE(registry.observe_normal_relay(original, after_return) == OK);
	CHECK(after_return.cell_released);
	CHECK(after_return.holder_closed); // Actual prepare CallbackScope has now left.
	CHECK(after_return.active_deliveries == 0);

	REQUIRE(extension->preflight_exit(CloseRoute::EXPLICIT) == OK);
	CHECK(prepare_calls == 2);
	CHECK(prepare_relay_forget_result == OK);
	CHECK(prepare_relay_snapshot.holder_closed); // Observed by the retry callback.
	CHECK(registry.observe_normal_relay(original, after_return) == ERR_INVALID_DATA); // Forgotten inside retry.
	CHECK(extension->preflight_exit(CloseRoute::EXPLICIT) == OK);
	CHECK(prepare_calls == 2); // Accepted prepare is cached for this route.
	const Error deinitialized = extension->deinitialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE);
	if (deinitialized == OK) { close_owner.scene_initialized = false; }
	REQUIRE(deinitialized == OK);
	CHECK(extension->close_library() == OK);
}

TEST_CASE("[DeviceExit][TypedABI][PresealPrepare] BUSY and malformed pending keep actual ordinary entry open") {
	exercise_callable = exercise_prepare = true;
	token = {}; hold = {}; prepare_calls = 0; prepare_pending = 1; prepare_write = true; prepare_result = OK;
	callable_calls = callable_frees = 0;
	Ref<GDExtensionFunctionLoader> loader; loader.instantiate(); loader->set_initialization_function(entry);
	Ref<GDExtension> extension; extension.instantiate(); prepare_extension = extension.ptr();
	REQUIRE(extension->open_library("libgodot://typed-preseal-prepare", loader) == OK);
	REQUIRE(extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE) == OK);
	auto install = proc<GDExtensionInterfaceDeviceExitRegisterPrepareV1>("device_exit_register_prepare_v1");
	CHECK(install(&abi, token, prepare_owner, nullptr) == ERR_UNAUTHORIZED); // Past original bootstrap.
	Error wrong_thread = OK;
	std::thread foreign_owner([&] { wrong_thread = extension->preflight_exit(GDExtensionDeviceExit::CloseRoute::EXPLICIT); });
	foreign_owner.join();
	CHECK(wrong_thread == ERR_UNAUTHORIZED);
	CHECK(prepare_calls == 0);
	CHECK(extension->prepare_exit(GDExtensionDeviceExit::CloseRoute(0)) == ERR_INVALID_PARAMETER);
	CHECK(prepare_calls == 0);
	prepare_reenter = true;
	CHECK(extension->preflight_exit(GDExtensionDeviceExit::CloseRoute::EXPLICIT) == ERR_BUSY);
	CHECK(prepare_calls == 1);
	Variant result; Callable::CallError error;
	retained_callable.callp(nullptr, 0, result, error);
	CHECK(error.error == Callable::CallError::CALL_OK); // Real adopted entry remains Open.
	CHECK(callable_calls == 1);
	prepare_reenter = false; prepare_write = false;
	CHECK(extension->preflight_exit(GDExtensionDeviceExit::CloseRoute::EXPLICIT) == ERR_INVALID_DATA);
	retained_callable.callp(nullptr, 0, result, error);
	CHECK(error.error == Callable::CallError::CALL_OK);
	CHECK(callable_calls == 2);
	prepare_write = true; prepare_pending = 0;
	prepare_result = -1;
	CHECK(extension->preflight_exit(GDExtensionDeviceExit::CloseRoute::EXPLICIT) == ERR_INVALID_DATA);
	retained_callable.callp(nullptr, 0, result, error);
	CHECK(error.error == Callable::CallError::CALL_OK);
	prepare_result = ERR_MAX;
	CHECK(extension->preflight_exit(GDExtensionDeviceExit::CloseRoute::EXPLICIT) == ERR_INVALID_DATA);
	prepare_result = OK;
	// Dispose actual aliases before post-seal holder census, not by modifying a count.
	equivalent_callable = Callable(); retained_callable = Callable();
	CHECK(callable_frees == 2);
	REQUIRE(extension->preflight_exit(GDExtensionDeviceExit::CloseRoute::EXPLICIT) == OK);
	const int prepared_once = prepare_calls;
	CHECK(extension->preflight_exit(GDExtensionDeviceExit::CloseRoute::EXPLICIT) == OK);
	CHECK(prepare_calls == prepared_once); // Manager's second pass/close retry must not call B twice.
	REQUIRE(extension->deinitialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE) == OK);
	CHECK(extension->close_library() == OK);
	exercise_callable = exercise_prepare = false; prepare_extension = nullptr;
}

} // namespace TestTypedLifecycleABI

TEST_CASE("[DeviceExit][TypedABI] legacy lookup hides lifecycle and preserves metadata") {
	CHECK(gdextension_get_proc_address("device_exit_register_guard_v1") == nullptr);
	CHECK(gdextension_get_proc_address("device_exit_borrow_guarded_library_v1") == nullptr);
	CHECK(gdextension_get_proc_address("device_exit_borrow_initialize_level_v1") == nullptr);
	CHECK(gdextension_get_proc_address("device_exit_borrow_deinit_level_v1") == nullptr);
	CHECK(gdextension_get_proc_address("device_exit_borrow_level_cleanup_v1") == nullptr);
	CHECK(gdextension_get_proc_address("device_exit_acquire_code_hold_v1") == nullptr);
	CHECK(gdextension_get_proc_address("device_exit_clone_code_hold_v1") == nullptr);
	CHECK(gdextension_get_proc_address("device_exit_release_code_hold_v1") == nullptr);
	CHECK(gdextension_get_proc_address("device_exit_after_binding_initialize_v1") == nullptr);
	CHECK(gdextension_get_proc_address("device_exit_begin_user_terminator_v1") == nullptr);
	CHECK(gdextension_get_proc_address("device_exit_after_user_terminator_v1") == nullptr);
	CHECK(gdextension_get_proc_address("device_exit_after_binding_deinit_v1") == nullptr);
	CHECK(gdextension_get_proc_address("device_exit_publish_exit_result_v1") == nullptr);
	CHECK(gdextension_get_proc_address("device_exit_terminate_initialize_blocked_v1") == nullptr);
	CHECK(gdextension_get_proc_address("device_exit_terminate_blocked_v1") == nullptr);
	CHECK(gdextension_get_proc_address("device_exit_bootstrap_library_v1") != nullptr);
	CHECK(gdextension_get_proc_address("get_godot_version2") != nullptr);
#ifndef DISABLE_DEPRECATED
	CHECK(gdextension_get_proc_address("get_godot_version") != nullptr);
#endif
}

TEST_CASE("[DeviceExit][TypedABI] producer registration ABI preserves rejection and guarded lookup") {
	using namespace TestTypedLifecycleABI;
	for (const char *name : { "device_session_bind_host_producer_v1", "device_session_clone_producer_registration_v1", "device_session_copy_producer_registration_v1", "device_session_release_producer_registration_v1" }) {
		CHECK(gdextension_get_proc_address(name) == nullptr);
		CHECK(proc<GDExtensionInterfaceFunctionPtr>(name) != nullptr);
	}
	auto bind = proc<GDExtensionInterfaceDeviceSessionBindHostProducerV1>("device_session_bind_host_producer_v1");
	auto clone = proc<GDExtensionInterfaceDeviceSessionCloneProducerRegistrationV1>("device_session_clone_producer_registration_v1");
	auto copy = proc<GDExtensionInterfaceDeviceSessionCopyProducerRegistrationV1>("device_session_copy_producer_registration_v1");
	auto release = proc<GDExtensionInterfaceDeviceSessionReleaseProducerRegistrationV1>("device_session_release_producer_registration_v1");
	REQUIRE(bind != nullptr);
	REQUIRE(clone != nullptr);
	REQUIRE(copy != nullptr);
	REQUIRE(release != nullptr);
	GDExtensionProducerRegistrationRefV1 empty{};
	CHECK_EQ(bind(nullptr, {}, {}, &producer_identity, &empty), ERR_INVALID_PARAMETER);
	CHECK_EQ(empty.handle_serial, 0);
	GDExtensionProducerRegistrationRefV1 occupied{ 11, 17 };
	CHECK_EQ(bind(&abi, {}, {}, &producer_identity, &occupied), ERR_INVALID_DATA);
	CHECK_EQ(occupied.registry_instance, 11);
	CHECK_EQ(occupied.handle_serial, 17);
	for (const GDExtensionDeviceExitAbi bad : { GDExtensionDeviceExitAbi{ 2, sizeof(abi), 0 }, GDExtensionDeviceExitAbi{ 1, 0, 0 }, GDExtensionDeviceExitAbi{ 1, sizeof(abi), 1 } }) {
		CHECK_EQ(clone(&bad, {}, &empty), ERR_INVALID_PARAMETER);
		CHECK_EQ(release(&bad, {}), ERR_INVALID_PARAMETER);
		CHECK_EQ(empty.handle_serial, 0);
	}
	CHECK_EQ(clone(&abi, {}, &empty), ERR_INVALID_DATA);
	CHECK_EQ(release(&abi, {}), ERR_INVALID_DATA);
	GDExtensionProducerRegistrationIdentityV1 identity{};
	identity.abi = { 1, sizeof(identity), 0 };
	identity.library_generation = 37;
	CHECK_EQ(copy(&abi, {}, &identity), ERR_INVALID_DATA);
	CHECK_EQ(identity.library_generation, 37);
	identity.abi.struct_size = 0;
	CHECK_EQ(copy(&abi, {}, &identity), ERR_INVALID_PARAMETER);
	CHECK_EQ(identity.library_generation, 37);
	alignas(GDExtensionProducerRegistrationRefV1) unsigned char bytes[sizeof(empty) + 8] = {};
	CHECK_EQ(clone(&abi, {}, reinterpret_cast<GDExtensionProducerRegistrationRefV1 *>(bytes + 1)), ERR_INVALID_PARAMETER);
	for (const unsigned char byte : bytes) {
		CHECK_EQ(byte, 0);
	}
	CHECK_EQ(clone(&abi, {}, reinterpret_cast<GDExtensionProducerRegistrationRefV1 *>(const_cast<GDExtensionDeviceExitAbi *>(&abi))), ERR_INVALID_PARAMETER);
}

// Separate original owners ensure no global fixture token can stand in for a sibling.
namespace TestNativeOuterExitPrepare {
using namespace GDExtensionDeviceExit;
using TestTypedLifecycleABI::proc;
static const GDExtensionDeviceExitAbi abi{ 1, sizeof(GDExtensionDeviceExitAbi), 0 };
struct Owner {
	GDExtensionClassLibraryPtr library = nullptr;
	GDExtensionDeviceExitGuardToken token{};
	Callable callable;
	NormalRelayToken relay;
	NormalRelaySnapshot observation;
	uint32_t passes = 0;
	uint32_t frees = 0;
	bool forgotten = false;
};
static Owner owners[2];
static uint32_t loading_owner = 0;
static uint32_t expected_route = uint32_t(CloseRoute::TEST_CLEANUP);
static Error second_pass_error = OK;
static void check(bool condition) { if (!condition) { std::_Exit(95); } }
static void callable_call(void *, const GDExtensionConstVariantPtr *, GDExtensionInt, GDExtensionVariantPtr result, GDExtensionCallError *error) {
	*reinterpret_cast<Variant *>(result) = Variant();
	error->error = GDEXTENSION_CALL_OK;
}
static void callable_free(void *userdata) { static_cast<Owner *>(userdata)->frees++; }
static void initialize(void *userdata, GDExtensionInitializationLevel level) {
	auto &owner = *static_cast<Owner *>(userdata);
	if (level == GDEXTENSION_INITIALIZATION_SCENE) {
		GDExtensionCallableCustomInfo2 info{};
		info.token = owner.library;
		info.callable_userdata = &owner;
		info.call_func = callable_call;
		info.free_func = callable_free;
		proc<GDExtensionInterfaceCallableCustomCreate2>("callable_custom_create2")(&owner.callable, &info);
		check(!owner.callable.is_null());
	}
	GDExtensionDeviceExitInitializationLevelContextRef context{};
	check(proc<GDExtensionInterfaceDeviceExitBorrowInitializeLevelV1>("device_exit_borrow_initialize_level_v1")(&abi, owner.library, level, &context) == OK);
	check(proc<GDExtensionInterfaceDeviceExitAfterBindingInitializeV1>("device_exit_after_binding_initialize_v1")(&abi, context) == OK);
}
static int32_t prepare(void *userdata, uint32_t route, uint64_t *pending) {
	auto &owner = *static_cast<Owner *>(userdata);
	check(route == expected_route && !owner.forgotten);
	owner.passes++;
	auto &registry = Registry::get_singleton();
	check(registry.cancel_normal_relay(owner.relay) == OK);
	owner.callable = Callable();
	check(registry.release_normal_relay_holder(owner.relay) == OK);
	check(registry.observe_normal_relay(owner.relay, owner.observation) == OK);
	check(owner.observation.cell_released && owner.observation.active_deliveries == 0);
	if (owner.passes == 1) {
		check(!owner.observation.holder_closed && owner.frees == 1);
		check(registry.forget_normal_relay(owner.relay) == ERR_BUSY);
		*pending = 1;
		return ERR_BUSY;
	}
	check(owner.passes == 2 && owner.observation.holder_closed);
	check(registry.forget_normal_relay(owner.relay) == OK);
	owner.forgotten = true;
	*pending = 0;
	if (second_pass_error != OK) {
		// A genuine hard callback outcome must leave the outer continuation now;
		// another callback would hit the forgotten-token assertion above.
		std::fprintf(stderr, "NATIVE_OUTER_OWNER_HARD route=%u passes=%u error=%d holder_closed=%d\n", route, owner.passes, int(second_pass_error), int(owner.observation.holder_closed));
		std::fflush(stderr);
		return second_pass_error;
	}
	return OK;
}
static int32_t preflight(GDExtensionDeviceExitDeinitPreflightContextRef context, void *userdata) {
	// The first sealed owner must see both original owner prepares completed.
	for (const Owner &owner : owners) {
		check(owner.passes == 2 && owner.forgotten && owner.observation.holder_closed && owner.frees == 1);
	}
	auto &owner = *static_cast<Owner *>(userdata);
	GDExtensionDeviceExitExitDiagnosticV1 diagnostic{ { 1, sizeof(diagnostic), 0 }, OK, GDEXTENSION_INITIALIZATION_SCENE, 0 };
	return proc<GDExtensionInterfaceDeviceExitPublishExitResultV1>("device_exit_publish_exit_result_v1")(&abi, owner.token, context, &diagnostic);
}
static void terminate(void *userdata, GDExtensionInitializationLevel level) {
	auto &owner = *static_cast<Owner *>(userdata);
	GDExtensionDeviceExitDeinitLevelContextRef context{};
	check(proc<GDExtensionInterfaceDeviceExitBorrowDeinitLevelV1>("device_exit_borrow_deinit_level_v1")(&abi, owner.library, level, &context) == OK);
	check(proc<GDExtensionInterfaceDeviceExitBeginUserTerminatorV1>("device_exit_begin_user_terminator_v1")(&abi, owner.token, context) == OK);
	check(proc<GDExtensionInterfaceDeviceExitAfterUserTerminatorV1>("device_exit_after_user_terminator_v1")(&abi, context) == OK);
	check(proc<GDExtensionInterfaceDeviceExitAfterBindingDeinitV1>("device_exit_after_binding_deinit_v1")(&abi, context) == OK);
	if (level == GDEXTENSION_INITIALIZATION_SCENE) {
		std::fprintf(stderr, "NATIVE_OUTER_PREPARE_RETURN route=%u passes=%u holder_closed=%d forgotten=%d frees=%u\n", expected_route, owner.passes, int(owner.observation.holder_closed), int(owner.forgotten), owner.frees);
		std::fflush(stderr);
	}
}
static GDExtensionBool entry(GDExtensionInterfaceGetProcAddress get, GDExtensionClassLibraryPtr library, GDExtensionInitialization *out) {
	auto &owner = owners[loading_owner];
	owner.library = library;
	*out = { GDEXTENSION_INITIALIZATION_SCENE, &owner, initialize, terminate };
	GDExtensionDeviceExitGuardRegistrationV1 registration{};
	registration.abi = { 1, sizeof(registration), 0 };
	registration.binding_protocol = 1;
	registration.level_bitmap = registration.user_cleanup_bitmap = registration.binding_cleanup_bitmap = 12;
	registration.expected_initialization = *out;
	registration.user_initializer = initialize;
	registration.user_terminator = terminate;
	registration.preflight = preflight;
	registration.preflight_userdata = &owner;
	registration.entry_classes = 1u << 8;
	auto enroll = reinterpret_cast<GDExtensionInterfaceDeviceExitRegisterGuardV1>(get("device_exit_register_guard_v1"));
	if (!enroll) { enroll = proc<GDExtensionInterfaceDeviceExitRegisterGuardV1>("device_exit_register_guard_v1"); }
	if (!enroll || enroll(library, &registration, &owner.token) != OK) { return false; }
	return proc<GDExtensionInterfaceDeviceExitRegisterPrepareV1>("device_exit_register_prepare_v1")(&abi, owner.token, prepare, &owner) == OK;
}
static void load_owners() {
	auto *manager = GDExtensionManager::get_singleton();
	for (loading_owner = 0; loading_owner < 2; loading_owner++) {
		Ref<GDExtensionFunctionLoader> loader; loader.instantiate();
		loader->set_initialization_function(entry);
		loader->set_device_exit_guard_version(1);
		check(manager->load_extension_with_loader("libgodot://native-outer-prepare-" + itos(loading_owner), loader) == GDExtensionManager::LOAD_STATUS_OK);
		check(Registry::get_singleton().register_normal_relay(owners[loading_owner].callable, owners[loading_owner].relay) == OK);
	}
}
static void natural_test_cleanup_child() {
	expected_route = uint32_t(CloseRoute::TEST_CLEANUP);
	load_owners();
	// Return to the original test runner. Main::test_cleanup owns the real pump,
	// preflight and teardown; no direct prepare/preflight or success _Exit here.
}
REGISTER_TEST_COMMAND("--device-exit-native-outer-test-cleanup", natural_test_cleanup_child);
static void natural_prepare_hard_error_child() {
	expected_route = uint32_t(CloseRoute::TEST_CLEANUP);
	second_pass_error = ERR_TIMEOUT;
	load_owners();
	// Original Main::test_cleanup must report the returned hard result through
	// require_close_success (exit 78), before preflight or any terminator.
}
REGISTER_TEST_COMMAND("--device-exit-native-outer-prepare-hard-error", natural_prepare_hard_error_child);
static void shutdown_prepare_boundary_child() {
	expected_route = uint32_t(CloseRoute::SHUTDOWN);
	load_owners();
	auto *manager = GDExtensionManager::get_singleton();
	check(manager->prepare_exit(CloseRoute::SHUTDOWN) == ERR_BUSY);
	check(owners[0].passes == 1 && owners[1].passes == 1);
	check(manager->prepare_exit(CloseRoute::SHUTDOWN) == OK);
	check(manager->preflight_exit(CloseRoute::SHUTDOWN) == OK);
	std::fprintf(stderr, "NATIVE_OUTER_SHUTDOWN_PREPARED both=1 route=%u\n", expected_route);
	std::fflush(stderr);
	// This boundary-only SHUTDOWN child deliberately does not enter TEST_CLEANUP.
	std::_Exit(0);
}
REGISTER_TEST_COMMAND("--device-exit-native-outer-shutdown-prepare", shutdown_prepare_boundary_child);
#ifdef MACOS_ENABLED
static void macos_pre_finalize_boundary_child() {
	expected_route = uint32_t(CloseRoute::SHUTDOWN);
	load_owners();
	// Invoke the exact platform hook shared by NSApp, RunLoop and Embedded.
	// It must advance ordinary prepares before its own strict preflight.
	TestDeviceExitStartup::macos_main_loop_finalize_boundary();
	for (const Owner &owner : owners) {
		check(owner.passes == 2 && owner.forgotten && owner.observation.holder_closed && owner.frees == 1);
	}
	std::fprintf(stderr, "NATIVE_MACOS_PRE_FINALIZE_PREPARED both=1 route=%u\n", expected_route);
	std::fflush(stderr);
	// This SHUTDOWN-boundary probe must not enter TEST_CLEANUP afterward.
	std::_Exit(0);
}
REGISTER_TEST_COMMAND("--device-exit-native-macos-shutdown-prepare", macos_pre_finalize_boundary_child);
#endif
} // namespace TestNativeOuterExitPrepare
