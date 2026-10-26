// Typed lifecycle ABI adapters. Authority remains in the original Registry.
#include "gdextension.h"
#include "gdextension_library_exit_guard.h"

#include <cstddef>
#include <cstring>
#include <type_traits>

namespace GDExtensionDeviceExit {
namespace {
int32_t register_scene_ready_abi(const GDExtensionDeviceExitAbi *abi, GDExtensionDeviceExitInitializationLevelContextRef frame, GDExtensionDeviceExitSceneReadyCallbackV1 callback, void *userdata) {
	return Registry::get_singleton().register_scene_ready(abi, { frame.registry_instance, frame.handle_serial }, callback, userdata);
}
int32_t register_scene_ready_v2_abi(const GDExtensionSceneInstallRequirementsV2 *requirements,GDExtensionDeviceExitInitializationLevelContextRef frame,GDExtensionDeviceExitSceneReadyCallbackV2 callback,void *userdata) {
 return Registry::get_singleton().register_scene_ready_v2(requirements,{frame.registry_instance,frame.handle_serial},callback,userdata);
}
static_assert(std::is_same_v<decltype(&register_scene_ready_v2_abi),GDExtensionInterfaceDeviceExitRegisterSceneReadyV2>);
int32_t schedule_scene_ready_abi(const GDExtensionDeviceExitAbi *abi, GDExtensionDeviceExitSceneReadyContextRef frame, uint32_t phase, uint64_t object_id) {
	return Registry::get_singleton().schedule_scene_ready_object_cleanup(abi, frame, phase, object_id);
}
int32_t abort_scene_ready_abi(const GDExtensionDeviceExitAbi *abi, GDExtensionDeviceExitSceneReadyContextRef frame, int32_t schedule_error, uint32_t reason, uint64_t object_id) {
	return Registry::get_singleton().scene_ready_abort_unscheduled_pair(abi, frame, schedule_error, reason, object_id);
}
static_assert(std::is_same_v<decltype(&register_scene_ready_abi), GDExtensionInterfaceDeviceExitRegisterSceneReadyV1>);
static_assert(std::is_same_v<decltype(&schedule_scene_ready_abi), GDExtensionInterfaceDeviceExitSceneReadyScheduleObjectCleanupV1>);
static_assert(std::is_same_v<decltype(&abort_scene_ready_abi), GDExtensionInterfaceDeviceExitSceneReadyAbortV1>);
static_assert(sizeof(GDExtensionDeviceExitSceneReadyContextRef) == 16);
static_assert(alignof(GDExtensionDeviceExitSceneReadyContextRef) == alignof(uint64_t));
static_assert(GDEXTENSION_DEVICE_EXIT_ENTRY_SCENE_READY_V1 == (UINT64_C(1) << 11));
template <typename T>
bool native_pointer(const T *p) {
	return p && reinterpret_cast<uintptr_t>(p) % alignof(T) == 0;
}
bool native_abi(const GDExtensionDeviceExitAbi *abi, uint32_t size = sizeof(GDExtensionDeviceExitAbi)) {
	return native_pointer(abi) && abi->abi_version == 1 && abi->struct_size == size && abi->reserved == 0;
}
template <typename To, typename From>
To handle_value(From value) {
	return { value.registry_instance, value.handle_serial };
}
static_assert(sizeof(GDExtensionDeviceExitGuardToken) == 2 * sizeof(uint64_t));
static_assert(sizeof(GDExtensionDeviceExitGuardedLibraryRef) ==
		2 * sizeof(uint64_t));
static_assert(sizeof(GDExtensionDeviceExitInitializationLevelContextRef) ==
		2 * sizeof(uint64_t));
static_assert(sizeof(GDExtensionDeviceExitDeinitLevelContextRef) ==
		2 * sizeof(uint64_t));
static_assert(sizeof(GDExtensionDeviceExitDeinitPreflightContextRef) ==
		2 * sizeof(uint64_t));
static_assert(sizeof(GDExtensionDeviceExitRegisteredCleanupRef) ==
		2 * sizeof(uint64_t));
static_assert(sizeof(GDExtensionDeviceExitLibraryCodeHoldRef) ==
		2 * sizeof(uint64_t));
static_assert(sizeof(GuardRegistrationV1) ==
		sizeof(GDExtensionDeviceExitGuardRegistrationV1));
static_assert(offsetof(GuardRegistrationV1, abi) ==
		offsetof(GDExtensionDeviceExitGuardRegistrationV1, abi));
static_assert(offsetof(GuardRegistrationV1, binding_protocol) ==
		offsetof(GDExtensionDeviceExitGuardRegistrationV1,
				binding_protocol));
static_assert(offsetof(GuardRegistrationV1, level_bitmap) ==
		offsetof(GDExtensionDeviceExitGuardRegistrationV1, level_bitmap));
static_assert(offsetof(GuardRegistrationV1, user_cleanup_bitmap) ==
		offsetof(GDExtensionDeviceExitGuardRegistrationV1,
				user_cleanup_bitmap));
static_assert(offsetof(GuardRegistrationV1, binding_cleanup_bitmap) ==
		offsetof(GDExtensionDeviceExitGuardRegistrationV1,
				binding_cleanup_bitmap));
static_assert(offsetof(GuardRegistrationV1, entry_classes) ==
		offsetof(GDExtensionDeviceExitGuardRegistrationV1,
				entry_classes));
static_assert(offsetof(GuardRegistrationV1, needs) ==
		offsetof(GDExtensionDeviceExitGuardRegistrationV1, needs));
static_assert(offsetof(GuardRegistrationV1, expected_initialization) ==
		offsetof(GDExtensionDeviceExitGuardRegistrationV1,
				expected_initialization));
static_assert(offsetof(GuardRegistrationV1, user_initializer) ==
		offsetof(GDExtensionDeviceExitGuardRegistrationV1,
				user_initializer));
static_assert(offsetof(GuardRegistrationV1, user_terminator) ==
		offsetof(GDExtensionDeviceExitGuardRegistrationV1,
				user_terminator));
static_assert(offsetof(GuardRegistrationV1, preflight) ==
		offsetof(GDExtensionDeviceExitGuardRegistrationV1, preflight));
static_assert(offsetof(GuardRegistrationV1, preflight_userdata) ==
		offsetof(GDExtensionDeviceExitGuardRegistrationV1,
				preflight_userdata));
static_assert(sizeof(ExitDiagnosticV1) ==
		sizeof(GDExtensionDeviceExitExitDiagnosticV1));
static_assert(offsetof(ExitDiagnosticV1, abi) ==
		offsetof(GDExtensionDeviceExitExitDiagnosticV1, abi));
static_assert(offsetof(ExitDiagnosticV1, disposition) ==
		offsetof(GDExtensionDeviceExitExitDiagnosticV1, disposition));
static_assert(offsetof(ExitDiagnosticV1, level) ==
		offsetof(GDExtensionDeviceExitExitDiagnosticV1, level));
static_assert(offsetof(ExitDiagnosticV1, pending_count) ==
		offsetof(GDExtensionDeviceExitExitDiagnosticV1, pending_count));

int32_t
register_guard_abi(GDExtensionClassLibraryPtr library,
		const GDExtensionDeviceExitGuardRegistrationV1 *registration,
		GDExtensionDeviceExitGuardToken *out_token) {
	if (!native_pointer(registration) || !native_abi(&registration->abi, sizeof(*registration))) {
		return ERR_INVALID_PARAMETER;
	}
	if (!native_pointer(out_token)) {
		return ERR_INVALID_PARAMETER;
	}
	if (out_token->registry_instance || out_token->handle_serial) {
		return ERR_INVALID_DATA;
	}
	GuardToken result{};
	GuardRegistrationV1 request{};
	request.abi = registration->abi;
	request.binding_protocol = registration->binding_protocol;
	request.level_bitmap = registration->level_bitmap;
	request.user_cleanup_bitmap = registration->user_cleanup_bitmap;
	request.binding_cleanup_bitmap = registration->binding_cleanup_bitmap;
	request.entry_classes = registration->entry_classes;
	request.expected_initialization = registration->expected_initialization;
	request.user_initializer = registration->user_initializer;
	request.user_terminator = registration->user_terminator;
	request.preflight = registration->preflight;
	request.preflight_userdata = registration->preflight_userdata;
	for (uint32_t i = 0; i < 4; i++) {
		request.needs[i] = { registration->needs[i].user_services,
			registration->needs[i].binding_services };
	}
	const Error error =
			Registry::get_singleton().register_guard(library, &request, &result);
	if (error == OK) {
		*out_token = handle_value<GDExtensionDeviceExitGuardToken>(result);
	}
	return error;
}
static_assert(std::is_same_v<decltype(&register_guard_abi),
		GDExtensionInterfaceDeviceExitRegisterGuardV1>);

int32_t borrow_guarded_library_abi(
		const GDExtensionDeviceExitAbi *abi, GDExtensionClassLibraryPtr library,
		GDExtensionDeviceExitGuardedLibraryRef *out_library) {
	if (!native_abi(abi)) {
		return ERR_INVALID_PARAMETER;
	}
	if (!native_pointer(out_library)) {
		return ERR_INVALID_PARAMETER;
	}
	if (out_library->registry_instance || out_library->handle_serial) {
		return ERR_INVALID_DATA;
	}
	GuardedLibraryRef result{};
	const Error error =
			Registry::get_singleton().borrow_guarded_library(abi, library, &result);
	if (error == OK) {
		*out_library = handle_value<GDExtensionDeviceExitGuardedLibraryRef>(result);
	}
	return error;
}
static_assert(
		std::is_same_v<decltype(&borrow_guarded_library_abi),
				GDExtensionInterfaceDeviceExitBorrowGuardedLibraryV1>);

int32_t borrow_initialize_level_abi(
		const GDExtensionDeviceExitAbi *abi, GDExtensionClassLibraryPtr library,
		uint32_t level,
		GDExtensionDeviceExitInitializationLevelContextRef *out_context) {
	if (!native_abi(abi)) {
		return ERR_INVALID_PARAMETER;
	}
	if (!native_pointer(out_context)) {
		return ERR_INVALID_PARAMETER;
	}
	if (out_context->registry_instance || out_context->handle_serial) {
		return ERR_INVALID_DATA;
	}
	InitializationLevelContextRef result{};
	const Error error = Registry::get_singleton().borrow_initialize_level(
			abi, library, level, &result);
	if (error == OK) {
		*out_context =
				handle_value<GDExtensionDeviceExitInitializationLevelContextRef>(
						result);
	}
	return error;
}
static_assert(
		std::is_same_v<decltype(&borrow_initialize_level_abi),
				GDExtensionInterfaceDeviceExitBorrowInitializeLevelV1>);

int32_t borrow_deinit_level_abi(
		const GDExtensionDeviceExitAbi *abi, GDExtensionClassLibraryPtr library,
		uint32_t level, GDExtensionDeviceExitDeinitLevelContextRef *out_context) {
	if (!native_abi(abi)) {
		return ERR_INVALID_PARAMETER;
	}
	if (!native_pointer(out_context)) {
		return ERR_INVALID_PARAMETER;
	}
	if (out_context->registry_instance || out_context->handle_serial) {
		return ERR_INVALID_DATA;
	}
	DeinitLevelContextRef result{};
	const Error error = Registry::get_singleton().borrow_deinit_level(
			abi, library, level, &result);
	if (error == OK) {
		*out_context =
				handle_value<GDExtensionDeviceExitDeinitLevelContextRef>(result);
	}
	return error;
}
static_assert(
		std::is_same_v<decltype(&borrow_deinit_level_abi),
				GDExtensionInterfaceDeviceExitBorrowDeinitLevelV1>);

int32_t borrow_level_cleanup_abi(
		const GDExtensionDeviceExitAbi *abi,
		GDExtensionDeviceExitInitializationLevelContextRef context, uint32_t phase,
		GDExtensionDeviceExitRegisteredCleanupRef *out_cleanup) {
	if (!native_abi(abi)) {
		return ERR_INVALID_PARAMETER;
	}
	if (!native_pointer(out_cleanup)) {
		return ERR_INVALID_PARAMETER;
	}
	if (out_cleanup->registry_instance || out_cleanup->handle_serial) {
		return ERR_INVALID_DATA;
	}
	RegisteredCleanupRef result{};
	const Error error = Registry::get_singleton().borrow_level_cleanup(
			abi, handle_value<InitializationLevelContextRef>(context), phase,
			&result);
	if (error == OK) {
		*out_cleanup =
				handle_value<GDExtensionDeviceExitRegisteredCleanupRef>(result);
	}
	return error;
}
static_assert(
		std::is_same_v<decltype(&borrow_level_cleanup_abi),
				GDExtensionInterfaceDeviceExitBorrowLevelCleanupV1>);

int32_t schedule_object_cleanup_abi(const GDExtensionDeviceExitAbi *abi,
		GDExtensionDeviceExitInitializationLevelContextRef context,
		GDExtensionDeviceExitRegisteredCleanupRef terminal, uint64_t object_id) {
	if (!native_abi(abi)) { return ERR_INVALID_PARAMETER; }
	return Registry::get_singleton().schedule_object_cleanup(abi,
			handle_value<InitializationLevelContextRef>(context),
			handle_value<RegisteredCleanupRef>(terminal), object_id);
}
static_assert(std::is_same_v<decltype(&schedule_object_cleanup_abi),
		GDExtensionInterfaceDeviceExitScheduleObjectCleanupV1>);

int32_t
acquire_code_hold_abi(const GDExtensionDeviceExitAbi *abi,
		GDExtensionDeviceExitGuardedLibraryRef library,
		GDExtensionDeviceExitRegisteredCleanupRef cleanup,
		GDExtensionDeviceExitLibraryCodeHoldRef *out_hold) {
	if (!native_abi(abi)) {
		return ERR_INVALID_PARAMETER;
	}
	if (!native_pointer(out_hold)) {
		return ERR_INVALID_PARAMETER;
	}
	if (out_hold->registry_instance || out_hold->handle_serial) {
		return ERR_INVALID_DATA;
	}
	LibraryCodeHoldRef result{};
	const Error error = Registry::get_singleton().acquire_code_hold(
			abi, handle_value<GuardedLibraryRef>(library),
			handle_value<RegisteredCleanupRef>(cleanup), &result);
	if (error == OK) {
		*out_hold = handle_value<GDExtensionDeviceExitLibraryCodeHoldRef>(result);
	}
	return error;
}
static_assert(std::is_same_v<decltype(&acquire_code_hold_abi),
		GDExtensionInterfaceDeviceExitAcquireCodeHoldV1>);

int32_t clone_code_hold_abi(const GDExtensionDeviceExitAbi *abi,
		GDExtensionDeviceExitLibraryCodeHoldRef source,
		GDExtensionDeviceExitLibraryCodeHoldRef *out_hold) {
	if (!native_abi(abi)) {
		return ERR_INVALID_PARAMETER;
	}
	if (!native_pointer(out_hold)) {
		return ERR_INVALID_PARAMETER;
	}
	if (out_hold->registry_instance || out_hold->handle_serial) {
		return ERR_INVALID_DATA;
	}
	LibraryCodeHoldRef result{};
	const Error error = Registry::get_singleton().clone_code_hold(
			abi, handle_value<LibraryCodeHoldRef>(source), &result);
	if (error == OK) {
		*out_hold = handle_value<GDExtensionDeviceExitLibraryCodeHoldRef>(result);
	}
	return error;
}
static_assert(std::is_same_v<decltype(&clone_code_hold_abi),
		GDExtensionInterfaceDeviceExitCloneCodeHoldV1>);

int32_t release_code_hold_abi(const GDExtensionDeviceExitAbi *abi,
		GDExtensionDeviceExitLibraryCodeHoldRef hold) {
	if (!native_abi(abi)) {
		return ERR_INVALID_PARAMETER;
	}
	return Registry::get_singleton().release_code_hold(
			abi, handle_value<LibraryCodeHoldRef>(hold));
}
static_assert(std::is_same_v<decltype(&release_code_hold_abi),
		GDExtensionInterfaceDeviceExitReleaseCodeHoldV1>);

int32_t after_binding_initialize_abi(
		const GDExtensionDeviceExitAbi *abi,
		GDExtensionDeviceExitInitializationLevelContextRef context) {
	if (!native_abi(abi)) {
		return ERR_INVALID_PARAMETER;
	}
	return Registry::get_singleton().after_binding_initialize(
			abi, handle_value<InitializationLevelContextRef>(context));
}
static_assert(
		std::is_same_v<decltype(&after_binding_initialize_abi),
				GDExtensionInterfaceDeviceExitAfterBindingInitializeV1>);

int32_t
begin_user_terminator_abi(const GDExtensionDeviceExitAbi *abi,
		GDExtensionDeviceExitGuardToken token,
		GDExtensionDeviceExitDeinitLevelContextRef context) {
	if (!native_abi(abi)) {
		return ERR_INVALID_PARAMETER;
	}
	return Registry::get_singleton().begin_user_terminator(
			abi, handle_value<GuardToken>(token),
			handle_value<DeinitLevelContextRef>(context));
}
static_assert(
		std::is_same_v<decltype(&begin_user_terminator_abi),
				GDExtensionInterfaceDeviceExitBeginUserTerminatorV1>);

int32_t
after_user_terminator_abi(const GDExtensionDeviceExitAbi *abi,
		GDExtensionDeviceExitDeinitLevelContextRef context) {
	if (!native_abi(abi)) {
		return ERR_INVALID_PARAMETER;
	}
	return Registry::get_singleton().after_user_terminator(
			abi, handle_value<DeinitLevelContextRef>(context));
}
static_assert(
		std::is_same_v<decltype(&after_user_terminator_abi),
				GDExtensionInterfaceDeviceExitAfterUserTerminatorV1>);

int32_t
after_binding_deinit_abi(const GDExtensionDeviceExitAbi *abi,
		GDExtensionDeviceExitDeinitLevelContextRef context) {
	if (!native_abi(abi)) {
		return ERR_INVALID_PARAMETER;
	}
	return Registry::get_singleton().after_binding_deinit(
			abi, handle_value<DeinitLevelContextRef>(context));
}
static_assert(
		std::is_same_v<decltype(&after_binding_deinit_abi),
				GDExtensionInterfaceDeviceExitAfterBindingDeinitV1>);

int32_t publish_exit_result_abi(
		const GDExtensionDeviceExitAbi *abi, GDExtensionDeviceExitGuardToken token,
		GDExtensionDeviceExitDeinitPreflightContextRef context,
		const GDExtensionDeviceExitExitDiagnosticV1 *diagnostic) {
	if (!native_abi(abi)) {
		return ERR_INVALID_PARAMETER;
	}
	if (!native_pointer(diagnostic) || !native_abi(&diagnostic->abi, sizeof(*diagnostic))) {
		return ERR_INVALID_PARAMETER;
	}
	ExitDiagnosticV1 request{ diagnostic->abi, diagnostic->disposition,
		diagnostic->level, diagnostic->pending_count };
	return Registry::get_singleton().publish_exit_result(
			abi, handle_value<GuardToken>(token),
			handle_value<DeinitPreflightContextRef>(context), &request);
}
static_assert(
		std::is_same_v<decltype(&publish_exit_result_abi),
				GDExtensionInterfaceDeviceExitPublishExitResultV1>);

[[noreturn]] void terminate_initialize_blocked_abi(
		GDExtensionDeviceExitInitializationLevelContextRef context,
		int32_t original_error) {
	Registry::get_singleton().terminate_initialize_blocked(
			handle_value<InitializationLevelContextRef>(context), original_error);
}
static_assert(
		std::is_same_v<decltype(&terminate_initialize_blocked_abi),
				GDExtensionInterfaceDeviceExitTerminateInitializeBlockedV1>);

[[noreturn]] void
terminate_blocked_abi(GDExtensionDeviceExitDeinitLevelContextRef context,
		int32_t original_error) {
	Registry::get_singleton().terminate_blocked(
			handle_value<DeinitLevelContextRef>(context), original_error);
}
static_assert(std::is_same_v<decltype(&terminate_blocked_abi),
		GDExtensionInterfaceDeviceExitTerminateBlockedV1>);
} // namespace

bool is_typed_lifecycle_interface(const char *name) {
	if (!name) {
		return false;
	}
	if (std::strcmp(name, "device_exit_register_scene_ready_v1") == 0 ||
			std::strcmp(name, "device_exit_register_scene_ready_v2") == 0 ||
			std::strcmp(name, "device_exit_scene_ready_schedule_object_cleanup_v1") == 0 ||
			std::strcmp(name, "device_exit_scene_ready_abort_v1") == 0) {
		return true;
	}
	if (std::strcmp(name, "device_exit_register_guard_v1") == 0) {
		return true;
	}
	if (std::strcmp(name, "device_exit_borrow_guarded_library_v1") == 0) {
		return true;
	}
	if (std::strcmp(name, "device_exit_borrow_initialize_level_v1") == 0) {
		return true;
	}
	if (std::strcmp(name, "device_exit_borrow_deinit_level_v1") == 0) {
		return true;
	}
	if (std::strcmp(name, "device_exit_borrow_level_cleanup_v1") == 0) {
		return true;
	}
	if (std::strcmp(name, "device_exit_schedule_object_cleanup_v1") == 0) {
		return true;
	}
	if (std::strcmp(name, "device_exit_acquire_code_hold_v1") == 0) {
		return true;
	}
	if (std::strcmp(name, "device_exit_clone_code_hold_v1") == 0) {
		return true;
	}
	if (std::strcmp(name, "device_exit_release_code_hold_v1") == 0) {
		return true;
	}
	if (std::strcmp(name, "device_exit_after_binding_initialize_v1") == 0) {
		return true;
	}
	if (std::strcmp(name, "device_exit_begin_user_terminator_v1") == 0) {
		return true;
	}
	if (std::strcmp(name, "device_exit_after_user_terminator_v1") == 0) {
		return true;
	}
	if (std::strcmp(name, "device_exit_after_binding_deinit_v1") == 0) {
		return true;
	}
	if (std::strcmp(name, "device_exit_publish_exit_result_v1") == 0) {
		return true;
	}
	if (std::strcmp(name, "device_exit_terminate_initialize_blocked_v1") == 0) {
		return true;
	}
	if (std::strcmp(name, "device_exit_terminate_blocked_v1") == 0) {
		return true;
	}
	return false;
}

#ifdef TESTS_ENABLED
GDExtensionInterfaceFunctionPtr test_typed_lifecycle_lookup(const char *name) {
	return GDExtension::get_interface_function(name);
}
#endif

void register_typed_lifecycle_interfaces() {
	GDExtension::register_interface_function("device_exit_register_scene_ready_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&register_scene_ready_abi));
	GDExtension::register_interface_function("device_exit_register_scene_ready_v2",reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&register_scene_ready_v2_abi));
	GDExtension::register_interface_function("device_exit_scene_ready_schedule_object_cleanup_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&schedule_scene_ready_abi));
	GDExtension::register_interface_function("device_exit_scene_ready_abort_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&abort_scene_ready_abi));
	GDExtension::register_interface_function(
			"device_exit_register_guard_v1",
			reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&register_guard_abi));
	GDExtension::register_interface_function(
			"device_exit_borrow_guarded_library_v1",
			reinterpret_cast<GDExtensionInterfaceFunctionPtr>(
					&borrow_guarded_library_abi));
	GDExtension::register_interface_function(
			"device_exit_borrow_initialize_level_v1",
			reinterpret_cast<GDExtensionInterfaceFunctionPtr>(
					&borrow_initialize_level_abi));
	GDExtension::register_interface_function(
			"device_exit_borrow_deinit_level_v1",
			reinterpret_cast<GDExtensionInterfaceFunctionPtr>(
					&borrow_deinit_level_abi));
	GDExtension::register_interface_function(
			"device_exit_borrow_level_cleanup_v1",
			reinterpret_cast<GDExtensionInterfaceFunctionPtr>(
					&borrow_level_cleanup_abi));
	GDExtension::register_interface_function(
			"device_exit_schedule_object_cleanup_v1",
			reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&schedule_object_cleanup_abi));
	GDExtension::register_interface_function(
			"device_exit_acquire_code_hold_v1",
			reinterpret_cast<GDExtensionInterfaceFunctionPtr>(
					&acquire_code_hold_abi));
	GDExtension::register_interface_function(
			"device_exit_clone_code_hold_v1",
			reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&clone_code_hold_abi));
	GDExtension::register_interface_function(
			"device_exit_release_code_hold_v1",
			reinterpret_cast<GDExtensionInterfaceFunctionPtr>(
					&release_code_hold_abi));
	GDExtension::register_interface_function(
			"device_exit_after_binding_initialize_v1",
			reinterpret_cast<GDExtensionInterfaceFunctionPtr>(
					&after_binding_initialize_abi));
	GDExtension::register_interface_function(
			"device_exit_begin_user_terminator_v1",
			reinterpret_cast<GDExtensionInterfaceFunctionPtr>(
					&begin_user_terminator_abi));
	GDExtension::register_interface_function(
			"device_exit_after_user_terminator_v1",
			reinterpret_cast<GDExtensionInterfaceFunctionPtr>(
					&after_user_terminator_abi));
	GDExtension::register_interface_function(
			"device_exit_after_binding_deinit_v1",
			reinterpret_cast<GDExtensionInterfaceFunctionPtr>(
					&after_binding_deinit_abi));
	GDExtension::register_interface_function(
			"device_exit_publish_exit_result_v1",
			reinterpret_cast<GDExtensionInterfaceFunctionPtr>(
					&publish_exit_result_abi));
	GDExtension::register_interface_function(
			"device_exit_terminate_initialize_blocked_v1",
			reinterpret_cast<GDExtensionInterfaceFunctionPtr>(
					&terminate_initialize_blocked_abi));
	GDExtension::register_interface_function(
			"device_exit_terminate_blocked_v1",
			reinterpret_cast<GDExtensionInterfaceFunctionPtr>(
					&terminate_blocked_abi));
}
} // namespace GDExtensionDeviceExit
