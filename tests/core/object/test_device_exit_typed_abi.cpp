#include "core/extension/gdextension.h"
#include "tests/test_macros.h"
#include "core/object/message_queue.h"
#include "core/extension/gdextension_library_loader.h"
#include "core/extension/gdextension_manager.h"
#include "core/io/config_file.h"
#include "core/io/file_access.h"
#include "core/io/dir_access.h"
#include "core/os/os.h"
#include "tests/test_utils.h"
#include <thread>

namespace GDExtensionDeviceExit {
GDExtensionInterfaceFunctionPtr test_typed_lifecycle_lookup(const char *name);
}
extern GDExtensionInterfaceFunctionPtr gdextension_get_proc_address(const char *name);

namespace TestDeviceExitEntry { void test_v2_single_registration(); }
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
namespace TestTypedLifecycleABI {
template <typename T>
T proc(const char *name) {
	return reinterpret_cast<T>(GDExtensionDeviceExit::test_typed_lifecycle_lookup(name));
}
static GDExtensionClassLibraryPtr library;
static GDExtensionDeviceExitGuardToken token;
static GDExtensionDeviceExitLibraryCodeHoldRef hold;
static const GDExtensionDeviceExitAbi abi{ 1, sizeof(GDExtensionDeviceExitAbi), 0 };
static bool exercise_callable = false;
static bool exercise_object_cleanup = false;
static Object *user_object = nullptr;
static Object *binding_object = nullptr;
static int object_frees = 0;
static void *create_object_binding(void *, void *object) { return object; }
static void free_object_binding(void *, void *, void *) { object_frees++; }
static const GDExtensionInstanceBindingCallbacks object_callbacks{ create_object_binding, free_object_binding, nullptr };
static Callable retained_callable;
static Callable equivalent_callable;
static int callable_calls = 0;
static int callable_frees = 0;
static int callable_hashes = 0;
static int callable_validations = 0;
static int callable_strings = 0;
static int callable_argument_counts = 0;
static GDExtensionBool callable_valid(void *) { callable_validations++; return true; }
static void callable_string(void *, GDExtensionBool *valid, GDExtensionStringPtr result) {
	callable_strings++; *valid = true; *reinterpret_cast<String *>(result) = "typed callable";
}
static GDExtensionInt callable_arguments(void *, GDExtensionBool *valid) { callable_argument_counts++; *valid = true; return 0; }
static void callable_call(void *, const GDExtensionConstVariantPtr *, GDExtensionInt, GDExtensionVariantPtr result, GDExtensionCallError *error) {
	callable_calls++;
	*reinterpret_cast<Variant *>(result) = 42;
	error->error = GDEXTENSION_CALL_OK;
}
static void callable_free(void *) { callable_frees++; }
static uint32_t callable_hash(void *) { callable_hashes++; return 42; }
static GDExtensionBool callable_equal(void *, void *) { return true; }
static GDExtensionBool callable_less(void *, void *) { return false; }
static void initialize(void *, GDExtensionInitializationLevel level) {
	CHECK(GDExtensionDeviceExit::Registry::get_singleton().check_current_entry_classes(1u << 0) == ERR_UNAVAILABLE);
	CHECK(GDExtensionDeviceExit::Registry::get_singleton().check_current_entry_classes(1u << 8) == (exercise_callable ? OK : ERR_UNAVAILABLE));
	if (exercise_callable) {
		GDExtensionCallableCustomInfo2 info{};
		info.token = library; info.call_func = callable_call; info.free_func = callable_free; info.hash_func = callable_hash;
		info.equal_func = callable_equal; info.less_than_func = callable_less;
		info.is_valid_func = callable_valid; info.to_string_func = callable_string; info.get_argument_count_func = callable_arguments;
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
	GDExtensionDeviceExitLibraryCodeHoldRef clone{};
	CHECK(proc<GDExtensionInterfaceDeviceExitCloneCodeHoldV1>("device_exit_clone_code_hold_v1")(&abi, hold, &clone) == OK);
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
	if (!register_guard) { register_guard = proc<GDExtensionInterfaceDeviceExitRegisterGuardV1>("device_exit_register_guard_v1"); }
	return register_guard(library, &registration, &token) == OK;
}
static bool guarded_lookup_seen = false;
static Object *cross_level_object = nullptr;
static Ref<RefCounted> cross_level_ref;
static int cross_level_frees = 0;
static void cross_level_binding_free(void *, void *, void *) { cross_level_frees++; }
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
	if (!guarded_lookup_seen) { return false; }
	return entry(get_proc, p_library, result);
}
TEST_CASE("[DeviceExit][TypedABI] scheduled scene Object survives editor binding checkpoint") {
	cross_level_frees = 0;
	cross_level_object = nullptr;
	cross_level_ref.unref();
	Ref<GDExtensionFunctionLoader> loader; loader.instantiate();
	loader->set_initialization_function(cross_level_entry);
	loader->set_device_exit_guard_version(1);
	Ref<GDExtension> extension; extension.instantiate();
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
	info.token = library; info.call_func = callable_call; info.free_func = callable_free; info.hash_func = callable_hash;
	info.equal_func = callable_equal; info.less_than_func = callable_less;
	proc<GDExtensionInterfaceCallableCustomCreate2>("callable_custom_create2")(&retained_callable, &info);
	GDExtensionDeviceExitInitializationLevelContextRef context{};
	if (proc<GDExtensionInterfaceDeviceExitBorrowInitializeLevelV1>("device_exit_borrow_initialize_level_v1")(&abi, library, level, &context) != OK ||
			proc<GDExtensionInterfaceDeviceExitAfterBindingInitializeV1>("device_exit_after_binding_initialize_v1")(&abi, context) != OK) { std::_Exit(93); }
}
static void manager_terminate(void *, GDExtensionInitializationLevel) { std::_Exit(94); }
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
	Ref<GDExtensionFunctionLoader> loader; loader.instantiate();
	loader->set_initialization_function(manager_entry);
	loader->set_device_exit_guard_version(1);
	auto *manager = GDExtensionManager::get_singleton();
	const String path = "libgodot://typed-manager-shutdown-preflight";
	if (manager->load_extension_with_loader(path, loader) != GDExtensionManager::LOAD_STATUS_OK) { std::_Exit(91); }
	const Error stopped = manager->shutdown();
	const bool intact = stopped == ERR_BUSY && manager->is_extension_loaded(path) && manager->get_extension(path)->is_library_open() && callable_frees == 0;
	std::fprintf(stderr, "TYPED_MANAGER_SHUTDOWN_PREFLIGHT intact=%d error=%d\n", int(intact), int(stopped));
	std::fflush(stderr);
	std::_Exit(intact ? 0 : 92);
}
REGISTER_TEST_COMMAND("--device-exit-typed-manager-shutdown-preflight", manager_shutdown_preflight_child);
static void manager_level_preflight_child() {
	callable_frees = 0;
	Ref<GDExtensionFunctionLoader> loader; loader.instantiate();
	loader->set_initialization_function(manager_entry);
	loader->set_device_exit_guard_version(1);
	auto *manager = GDExtensionManager::get_singleton();
	const String path = "libgodot://typed-manager-level-preflight";
	if (manager->load_extension_with_loader(path, loader) != GDExtensionManager::LOAD_STATUS_OK) { std::_Exit(91); }
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
static void *unscheduled_binding_create(void *, void *object) { return object; }
static void unscheduled_binding_free(void *, void *, void *) { unscheduled_object_frees++; }
static const GDExtensionInstanceBindingCallbacks unscheduled_callbacks{ unscheduled_binding_create, unscheduled_binding_free, nullptr };
static void unscheduled_initialize(void *, GDExtensionInitializationLevel level) {
	unscheduled_object = memnew(Object);
	if (unscheduled_object->get_instance_binding(library, &unscheduled_callbacks) != unscheduled_object) { std::_Exit(95); }
	GDExtensionDeviceExitInitializationLevelContextRef context{};
	if (proc<GDExtensionInterfaceDeviceExitBorrowInitializeLevelV1>("device_exit_borrow_initialize_level_v1")(&abi, library, level, &context) != OK ||
			proc<GDExtensionInterfaceDeviceExitAfterBindingInitializeV1>("device_exit_after_binding_initialize_v1")(&abi, context) != OK) { std::_Exit(96); }
}
static void unscheduled_terminate(void *, GDExtensionInitializationLevel) { std::_Exit(97); }
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
	Ref<GDExtensionFunctionLoader> loader; loader.instantiate();
	loader->set_initialization_function(unscheduled_entry);
	loader->set_device_exit_guard_version(1);
	auto *manager = GDExtensionManager::get_singleton();
	const String path = "libgodot://typed-unscheduled-object";
	if (manager->load_extension_with_loader(path, loader) != GDExtensionManager::LOAD_STATUS_OK) { std::_Exit(91); }
	const Error stopped = manager->shutdown();
	const bool intact = stopped == ERR_BUSY && manager->is_extension_loaded(path) && manager->get_extension(path)->is_library_open() && unscheduled_object_frees == 0;
	std::fprintf(stderr, "TYPED_UNSCHEDULED_OBJECT_PREFLIGHT intact=%d error=%d\n", int(intact), int(stopped));
	std::fflush(stderr);
	std::_Exit(intact ? 0 : 92);
}
REGISTER_TEST_COMMAND("--device-exit-typed-unscheduled-object-preflight", unscheduled_object_preflight_child);
TEST_CASE("[DeviceExit][TypedABI] version one selects guarded lookup and requires enrollment") {
	guarded_lookup_seen = false;
	token = {}; hold = {};
	Ref<GDExtensionFunctionLoader> loader; loader.instantiate();
	loader->set_initialization_function(guarded_entry);
	loader->set_device_exit_guard_version(1);
	Ref<GDExtension> extension; extension.instantiate();
	REQUIRE(extension->open_library("libgodot://typed-production", loader) == OK);
	CHECK(guarded_lookup_seen);
	REQUIRE(extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE) == OK);
	REQUIRE(extension->preflight_exit(GDExtensionDeviceExit::CloseRoute::EXPLICIT) == OK);
	REQUIRE(extension->deinitialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE) == OK);
	CHECK(extension->close_library() == OK);
}
static GDExtensionBool missing_guard_entry(GDExtensionInterfaceGetProcAddress get_proc, GDExtensionClassLibraryPtr, GDExtensionInitialization *result) {
	guarded_lookup_seen = get_proc("device_exit_register_guard_v1") != nullptr;
	*result = { GDEXTENSION_INITIALIZATION_SCENE, nullptr, initialize, terminate };
	return true;
}
TEST_CASE("[DeviceExit][TypedABI] version one refuses an entry without guard registration") {
	guarded_lookup_seen = false;
	Ref<GDExtensionFunctionLoader> loader; loader.instantiate();
	loader->set_initialization_function(missing_guard_entry);
	loader->set_device_exit_guard_version(1);
	Ref<GDExtension> extension; extension.instantiate();
	ERR_PRINT_OFF;
	CHECK(extension->open_library("libgodot://typed-no-enrollment", loader) == ERR_UNAVAILABLE);
	ERR_PRINT_ON;
	CHECK(guarded_lookup_seen);
	CHECK_FALSE(loader->is_library_open());
}
TEST_CASE("[DeviceExit][TypedABI] version one loads an actual dynamic image") {
	const String image = DirAccess::create(DirAccess::ACCESS_FILESYSTEM)->get_current_dir().path_join("work/typed-lifecycle-abi-v1/native-dynamic/libtyped_dynamic.dylib");
	if (!FileAccess::exists(image)) { return; }
	const String path = TestUtils::get_temp_path("typed-dynamic.gdextension");
	Ref<ConfigFile> config; config.instantiate();
	config->set_value("configuration", "entry_symbol", "typed_dynamic_entry");
	config->set_value("configuration", "compatibility_minimum", "4.1");
	config->set_value("configuration", "device_exit_guard_version", 1);
	config->set_value("libraries", OS::get_singleton()->get_name().to_lower(), image);
	REQUIRE(config->save(path) == OK);
	Ref<GDExtensionLibraryLoader> loader; loader.instantiate();
	Ref<GDExtension> extension; extension.instantiate();
	REQUIRE(extension->open_library(path, loader) == OK);
	REQUIRE(extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE) == OK);
	REQUIRE(extension->preflight_exit(GDExtensionDeviceExit::CloseRoute::EXPLICIT) == OK);
	REQUIRE(extension->deinitialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE) == OK);
	CHECK(extension->close_library() == OK);
	CHECK(DirAccess::remove_absolute(path) == OK);
}
TEST_CASE("[DeviceExit][TypedABI] manager loads guarded SCENE from its declared minimum") {
	const String image = DirAccess::create(DirAccess::ACCESS_FILESYSTEM)->get_current_dir().path_join("work/typed-lifecycle-abi-v1/native-dynamic/libtyped_dynamic.dylib");
	if (!FileAccess::exists(image)) { return; }
	const String path = TestUtils::get_temp_path("typed-manager.gdextension");
	Ref<ConfigFile> config; config.instantiate();
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
 SUBCASE("SI2_03_single_reservation"){::TestDeviceExitEntry::test_v2_single_registration();}

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
	token = {}; hold = {};
	Ref<GDExtensionFunctionLoader> loader; loader.instantiate(); loader->set_initialization_function(entry);
	Ref<GDExtension> extension; extension.instantiate();
	REQUIRE(extension->open_library("libgodot://typed-owner", loader) == OK);
	GDExtensionDeviceExit::RecordRef owner;
	CHECK(GDExtensionDeviceExit::Registry::get_singleton().retain_callable_owner(library, owner) == ERR_UNAUTHORIZED);
	CHECK_FALSE(owner.is_valid());
	Ref<GDExtensionFunctionLoader> legacy_loader; legacy_loader.instantiate(); legacy_loader->set_initialization_function(foreign_token_entry);
	Ref<GDExtension> legacy; legacy.instantiate();
	REQUIRE(legacy->open_library("libgodot://foreign-token", legacy_loader) == OK);
	CHECK(legacy->close_library() == OK);
	CHECK(extension->close_library() == OK);
}
TEST_CASE("[DeviceExit][TypedABI] unavailable service refuses initializer before admission") {
	token = {}; hold = {};
	Ref<GDExtensionFunctionLoader> loader; loader.instantiate(); loader->set_initialization_function(entry);
	Ref<GDExtension> extension; extension.instantiate();
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
TEST_CASE("[DeviceExit][TypedABI] Callable holder blocks close until actual final free") {
	exercise_callable = true;
	callable_calls = callable_frees = callable_hashes = 0;
	callable_validations = callable_strings = callable_argument_counts = 0;
	token = {}; hold = {};
	Ref<GDExtensionFunctionLoader> loader; loader.instantiate(); loader->set_initialization_function(entry);
	Ref<GDExtension> extension; extension.instantiate();
	REQUIRE(extension->open_library("libgodot://typed-callable", loader) == OK);
	REQUIRE(extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE) == OK);
	CHECK(callable_hashes == 2);
	CHECK(retained_callable.is_valid());
	CHECK(String(retained_callable) == "typed callable");
	bool count_valid = false;
	CHECK(retained_callable.get_argument_count(&count_valid) == 0);
	CHECK(count_valid);
	CHECK(callable_strings == 1); CHECK(callable_argument_counts == 1);
	CHECK(retained_callable == equivalent_callable);
	CHECK_FALSE(retained_callable < equivalent_callable);
	Callable copy = retained_callable;
	CallQueue queue;
	REQUIRE(queue.push_callable(copy) == OK);
	Variant result; Callable::CallError error;
	copy.callp(nullptr, 0, result, error);
	CHECK(callable_calls == 1); CHECK(int(result) == 42);
	CHECK(extension->preflight_exit(GDExtensionDeviceExit::CloseRoute::EXPLICIT) == ERR_BUSY);
	CHECK(copy == equivalent_callable); CHECK_FALSE(copy < equivalent_callable);
	const int validations_before = callable_validations;
	CHECK_FALSE(copy.is_valid());
	CHECK(String(copy) == "<CallableCustom>");
	CHECK(copy.get_argument_count(&count_valid) == 0); CHECK_FALSE(count_valid);
	CHECK(callable_validations == validations_before);
	CHECK(callable_strings == 1); CHECK(callable_argument_counts == 1);
	CHECK(extension->preflight_exit(GDExtensionDeviceExit::CloseRoute::EXPLICIT) == ERR_BUSY);
	equivalent_callable = Callable(); CHECK(callable_frees == 1);
	copy.callp(nullptr, 0, result, error);
	CHECK(error.error == Callable::CallError::CALL_ERROR_INSTANCE_IS_NULL); CHECK(callable_calls == 1);
	retained_callable = Callable(); CHECK(callable_frees == 1);
	copy = Callable(); CHECK(callable_frees == 1);
	CHECK(extension->preflight_exit(GDExtensionDeviceExit::CloseRoute::EXPLICIT) == ERR_BUSY);
	CHECK(queue.flush() == OK);
	CHECK(callable_calls == 1);
	CHECK(callable_frees == 2);
	CHECK(extension->preflight_exit(GDExtensionDeviceExit::CloseRoute::EXPLICIT) == OK);
	CHECK(extension->deinitialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE) == OK);
	CHECK(extension->close_library() == OK);
	exercise_callable = false;
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
	CHECK(gdextension_get_proc_address("get_godot_version") != nullptr);
}
