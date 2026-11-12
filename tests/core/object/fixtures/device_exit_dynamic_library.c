#include "core/extension/gdextension_interface.gen.h"

static GDExtensionInterfaceGetProcAddress lookup;
static GDExtensionClassLibraryPtr library;
static GDExtensionDeviceExitGuardToken token;
static const GDExtensionDeviceExitAbi abi = { 1, sizeof(GDExtensionDeviceExitAbi), 0 };
static int32_t preflight(GDExtensionDeviceExitDeinitPreflightContextRef context, void *userdata) {
    (void)userdata;
    GDExtensionDeviceExitExitDiagnosticV1 report = { { 1, sizeof(report), 0 }, 0, GDEXTENSION_INITIALIZATION_SCENE, 0 };
    return ((GDExtensionInterfaceDeviceExitPublishExitResultV1)lookup("device_exit_publish_exit_result_v1"))(&abi, token, context, &report);
}
static void initialize(void *userdata, GDExtensionInitializationLevel level) {
    (void)userdata;
    GDExtensionDeviceExitInitializationLevelContextRef frame = { 0 };
    if (((GDExtensionInterfaceDeviceExitBorrowInitializeLevelV1)lookup("device_exit_borrow_initialize_level_v1"))(&abi, library, level, &frame)) __builtin_trap();
    if (((GDExtensionInterfaceDeviceExitAfterBindingInitializeV1)lookup("device_exit_after_binding_initialize_v1"))(&abi, frame)) __builtin_trap();
}
static void terminate(void *userdata, GDExtensionInitializationLevel level) {
    (void)userdata;
    GDExtensionDeviceExitDeinitLevelContextRef frame = { 0 };
    if (((GDExtensionInterfaceDeviceExitBorrowDeinitLevelV1)lookup("device_exit_borrow_deinit_level_v1"))(&abi, library, level, &frame)) __builtin_trap();
    if (((GDExtensionInterfaceDeviceExitBeginUserTerminatorV1)lookup("device_exit_begin_user_terminator_v1"))(&abi, token, frame)) __builtin_trap();
    if (((GDExtensionInterfaceDeviceExitAfterUserTerminatorV1)lookup("device_exit_after_user_terminator_v1"))(&abi, frame)) __builtin_trap();
    if (((GDExtensionInterfaceDeviceExitAfterBindingDeinitV1)lookup("device_exit_after_binding_deinit_v1"))(&abi, frame)) __builtin_trap();
}
GDExtensionBool typed_dynamic_entry(GDExtensionInterfaceGetProcAddress get, GDExtensionClassLibraryPtr owner, GDExtensionInitialization *out) {
    lookup = get;
    library = owner;
    GDExtensionInterfaceDeviceExitRegisterGuardV1 register_guard = (GDExtensionInterfaceDeviceExitRegisterGuardV1)get("device_exit_register_guard_v1");
    if (!register_guard || !get("device_session_bind_v1")) return 0;
    *out = (GDExtensionInitialization){ GDEXTENSION_INITIALIZATION_SCENE, 0, initialize, terminate };
    GDExtensionDeviceExitGuardRegistrationV1 registration = { 0 };
    registration.abi = (GDExtensionDeviceExitAbi){ 1, sizeof(registration), 0 };
    registration.binding_protocol = 1;
    registration.level_bitmap = registration.binding_cleanup_bitmap = 12;
    registration.user_cleanup_bitmap = 4;
    registration.expected_initialization = *out;
    registration.user_initializer = initialize;
    registration.user_terminator = terminate;
    registration.preflight = preflight;
    return register_guard(owner, &registration, &token) == 0;
}
