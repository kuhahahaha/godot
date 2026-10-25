#include "core/extension/gdextension.h"
#include "tests/test_macros.h"

namespace GDExtensionDeviceExit {
GDExtensionInterfaceFunctionPtr test_typed_lifecycle_lookup(const char *name);
}
extern GDExtensionInterfaceFunctionPtr gdextension_get_proc_address(const char *name);

TEST_FORCE_LINK(test_device_exit_typed_abi)

TEST_CASE("[DeviceExit][TypedABI] generated lifecycle procs preserve rejected outputs") {
	REQUIRE(GDExtensionDeviceExit::test_typed_lifecycle_lookup("device_exit_register_guard_v1") != nullptr);
	REQUIRE(GDExtensionDeviceExit::test_typed_lifecycle_lookup("device_exit_borrow_guarded_library_v1") != nullptr);
	REQUIRE(GDExtensionDeviceExit::test_typed_lifecycle_lookup("device_exit_borrow_initialize_level_v1") != nullptr);
	REQUIRE(GDExtensionDeviceExit::test_typed_lifecycle_lookup("device_exit_borrow_deinit_level_v1") != nullptr);
	REQUIRE(GDExtensionDeviceExit::test_typed_lifecycle_lookup("device_exit_borrow_level_cleanup_v1") != nullptr);
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
static void initialize(void *, GDExtensionInitializationLevel level) {
	GDExtensionDeviceExitInitializationLevelContextRef context{};
	GDExtensionDeviceExitGuardedLibraryRef borrowed{};
	GDExtensionDeviceExitRegisteredCleanupRef cleanup{};
	CHECK(proc<GDExtensionInterfaceDeviceExitBorrowInitializeLevelV1>("device_exit_borrow_initialize_level_v1")(&abi, library, level, &context) == OK);
	CHECK(proc<GDExtensionInterfaceDeviceExitBorrowGuardedLibraryV1>("device_exit_borrow_guarded_library_v1")(&abi, library, &borrowed) == OK);
	CHECK(proc<GDExtensionInterfaceDeviceExitBorrowLevelCleanupV1>("device_exit_borrow_level_cleanup_v1")(&abi, context, 0, &cleanup) == OK);
	CHECK(proc<GDExtensionInterfaceDeviceExitAcquireCodeHoldV1>("device_exit_acquire_code_hold_v1")(&abi, borrowed, cleanup, &hold) == OK);
	GDExtensionDeviceExitLibraryCodeHoldRef clone{};
	CHECK(proc<GDExtensionInterfaceDeviceExitCloneCodeHoldV1>("device_exit_clone_code_hold_v1")(&abi, hold, &clone) == OK);
	CHECK(proc<GDExtensionInterfaceDeviceExitReleaseCodeHoldV1>("device_exit_release_code_hold_v1")(&abi, clone) == OK);
	CHECK(proc<GDExtensionInterfaceDeviceExitAfterBindingInitializeV1>("device_exit_after_binding_initialize_v1")(&abi, context) == OK);
}
static void terminate(void *, GDExtensionInitializationLevel level) {
	GDExtensionDeviceExitDeinitLevelContextRef context{};
	CHECK(proc<GDExtensionInterfaceDeviceExitBorrowDeinitLevelV1>("device_exit_borrow_deinit_level_v1")(&abi, library, level, &context) == OK);
	CHECK(proc<GDExtensionInterfaceDeviceExitBeginUserTerminatorV1>("device_exit_begin_user_terminator_v1")(&abi, token, context) == OK);
	CHECK(proc<GDExtensionInterfaceDeviceExitAfterUserTerminatorV1>("device_exit_after_user_terminator_v1")(&abi, context) == ERR_BUSY);
	CHECK(proc<GDExtensionInterfaceDeviceExitAfterBindingDeinitV1>("device_exit_after_binding_deinit_v1")(&abi, context) == ERR_INVALID_DATA);
	CHECK(proc<GDExtensionInterfaceDeviceExitReleaseCodeHoldV1>("device_exit_release_code_hold_v1")(&abi, hold) == OK);
	CHECK(proc<GDExtensionInterfaceDeviceExitAfterUserTerminatorV1>("device_exit_after_user_terminator_v1")(&abi, context) == OK);
	CHECK(proc<GDExtensionInterfaceDeviceExitAfterBindingDeinitV1>("device_exit_after_binding_deinit_v1")(&abi, context) == OK);
}
static int32_t preflight(GDExtensionDeviceExitDeinitPreflightContextRef context, void *) {
	GDExtensionDeviceExitExitDiagnosticV1 diagnostic{ { 1, sizeof(GDExtensionDeviceExitExitDiagnosticV1), 0 }, OK, GDEXTENSION_INITIALIZATION_SCENE, 0 };
	return proc<GDExtensionInterfaceDeviceExitPublishExitResultV1>("device_exit_publish_exit_result_v1")(&abi, token, context, &diagnostic);
}
static GDExtensionBool entry(GDExtensionInterfaceGetProcAddress, GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *result) {
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
	return proc<GDExtensionInterfaceDeviceExitRegisterGuardV1>("device_exit_register_guard_v1")(library, &registration, &token) == OK;
}
TEST_CASE("[DeviceExit][TypedABI] generated procs drive original bootstrap and SCENE lifetime") {
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
	REQUIRE(extension->preflight_exit(GDExtensionDeviceExit::CloseRoute::EXPLICIT) == OK);
	REQUIRE(extension->deinitialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE) == OK);
	CHECK(extension->close_library() == OK);
	CHECK(proc<GDExtensionInterfaceDeviceExitReleaseCodeHoldV1>("device_exit_release_code_hold_v1")(&abi, hold) == ERR_INVALID_DATA);
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
