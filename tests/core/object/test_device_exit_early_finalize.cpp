#include "tests/test_macros.h"

TEST_FORCE_LINK(test_device_exit_early_finalize)

#include "core/extension/gdextension_function_loader.h"
#include "core/extension/gdextension_manager.h"

#include <cstdio>
#include <cstdlib>

namespace TestDeviceExitEarlyFinalize {

static GDExtensionInterfaceGetProcAddress lookup;
static GDExtensionClassLibraryPtr library;
static GDExtensionDeviceExitGuardToken guard;
static const GDExtensionDeviceExitAbi abi{ 1, sizeof(GDExtensionDeviceExitAbi), 0 };
static Object *host;
static int binding_frees;
static int terminators;
static bool scheduled;

template <typename Function>
static Function proc(const char *name) {
	return reinterpret_cast<Function>(lookup(name));
}

static void require(bool valid) {
	if (!valid) {
		std::_Exit(91);
	}
}

static const GDExtensionInstanceBindingCallbacks callbacks{
	[](void *, void *object) -> void * { return object; },
	[](void *, void *, void *) { binding_frees++; },
	nullptr
};

static void initialize(void *, GDExtensionInitializationLevel level) {
	GDExtensionDeviceExitInitializationLevelContextRef frame{};
	require(proc<GDExtensionInterfaceDeviceExitBorrowInitializeLevelV1>("device_exit_borrow_initialize_level_v1")(&abi, library, level, &frame) == OK);
	if (level == GDEXTENSION_INITIALIZATION_SCENE) {
		require(host->get_instance_binding(library, &callbacks) == host);
		if (scheduled) {
			GDExtensionDeviceExitRegisteredCleanupRef cleanup{};
			require(proc<GDExtensionInterfaceDeviceExitBorrowLevelCleanupV1>("device_exit_borrow_level_cleanup_v1")(&abi, frame, 1, &cleanup) == OK);
			require(proc<GDExtensionInterfaceDeviceExitScheduleObjectCleanupV1>("device_exit_schedule_object_cleanup_v1")(&abi, frame, cleanup, uint64_t(host->get_instance_id())) == OK);
		}
	}
	require(proc<GDExtensionInterfaceDeviceExitAfterBindingInitializeV1>("device_exit_after_binding_initialize_v1")(&abi, frame) == OK);
}

static void terminate(void *, GDExtensionInitializationLevel level) {
	GDExtensionDeviceExitDeinitLevelContextRef frame{};
	require(proc<GDExtensionInterfaceDeviceExitBorrowDeinitLevelV1>("device_exit_borrow_deinit_level_v1")(&abi, library, level, &frame) == OK);
	require(proc<GDExtensionInterfaceDeviceExitBeginUserTerminatorV1>("device_exit_begin_user_terminator_v1")(&abi, guard, frame) == OK);
	require(proc<GDExtensionInterfaceDeviceExitAfterUserTerminatorV1>("device_exit_after_user_terminator_v1")(&abi, frame) == OK);
	if (level == GDEXTENSION_INITIALIZATION_SCENE) {
		host->free_instance_binding(library);
	}
	require(proc<GDExtensionInterfaceDeviceExitAfterBindingDeinitV1>("device_exit_after_binding_deinit_v1")(&abi, frame) == OK);
	terminators++;
}

static int32_t preflight(GDExtensionDeviceExitDeinitPreflightContextRef frame, void *) {
	GDExtensionDeviceExitExitDiagnosticV1 diagnostic{
		{ 1, sizeof(GDExtensionDeviceExitExitDiagnosticV1), 0 }, OK, GDEXTENSION_INITIALIZATION_SCENE, 0
	};
	return proc<GDExtensionInterfaceDeviceExitPublishExitResultV1>("device_exit_publish_exit_result_v1")(&abi, guard, frame, &diagnostic);
}

static GDExtensionBool entry(GDExtensionInterfaceGetProcAddress get_proc, GDExtensionClassLibraryPtr owner, GDExtensionInitialization *initialization) {
	lookup = get_proc;
	library = owner;
	*initialization = { GDEXTENSION_INITIALIZATION_SCENE, nullptr, initialize, terminate };
	GDExtensionDeviceExitGuardRegistrationV1 registration{};
	registration.abi = { 1, sizeof(registration), 0 };
	registration.binding_protocol = 1;
	registration.level_bitmap = registration.user_cleanup_bitmap = registration.binding_cleanup_bitmap = 12;
	registration.expected_initialization = *initialization;
	registration.user_initializer = initialize;
	registration.user_terminator = terminate;
	registration.preflight = preflight;
	registration.entry_classes = 1u << 6;
	registration.needs[GDEXTENSION_INITIALIZATION_SCENE] = { (1u << 1) | (1u << 5) | (1u << 7), 1u << 7 };
	return proc<GDExtensionInterfaceDeviceExitRegisterGuardV1>("device_exit_register_guard_v1")(owner, &registration, &guard) == OK;
}

static void run(bool schedule) {
	scheduled = schedule;
	host = memnew(Object);
	const ObjectID host_id = host->get_instance_id();
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(entry);
	loader->set_device_exit_guard_version(1);
	auto *manager = GDExtensionManager::get_singleton();
	const String path = "libgodot://early-finalize-owner";
	require(manager->load_extension_with_loader(path, loader) == GDExtensionManager::LOAD_STATUS_OK);
	const Error admitted = manager->preflight_exit(GDExtensionDeviceExit::CloseRoute::SHUTDOWN);
	if (!schedule) {
		require(admitted == ERR_BUSY && binding_frees == 0 && terminators == 0 && ObjectDB::get_instance(host_id) == host);
		require(manager->preflight_exit(GDExtensionDeviceExit::CloseRoute::SHUTDOWN) == ERR_BUSY);
		require(manager->preflight_exit(GDExtensionDeviceExit::CloseRoute::EXPLICIT) == ERR_INVALID_DATA);
		require(binding_frees == 0 && terminators == 0);
		std::fprintf(stderr, "EARLY_FINALIZE blocked=1 host_live=1 frees=0 terminators=0\n");
	} else {
		require(admitted == OK);
		require(manager->deinitialize_guarded_extensions_before_main_loop() == OK);
		require(ObjectDB::get_instance(host_id) == host && binding_frees == 1 && terminators == 2);
		require(manager->deinitialize_guarded_extensions_before_main_loop() == OK);
		require(manager->shutdown() == OK);
		require(manager->preflight_exit(GDExtensionDeviceExit::CloseRoute::EXPLICIT) == ERR_INVALID_DATA);
		require(manager->deinitialize_guarded_extensions_before_main_loop() == OK);
		require(binding_frees == 1 && terminators == 2);
		memdelete(host);
		require(ObjectDB::get_instance(host_id) == nullptr);
		std::fprintf(stderr, "EARLY_FINALIZE admitted=1 host_live_at_binding=1 frees=1 terminators=2 repeated=OK foreign_route=REJECTED\n");
	}
	std::fflush(stderr);
	std::_Exit(0);
}

static void accepted() { run(true); }
static void blocked() { run(false); }
REGISTER_TEST_COMMAND("--device-exit-early-finalize-accepted", accepted);
REGISTER_TEST_COMMAND("--device-exit-early-finalize-blocked", blocked);

}
