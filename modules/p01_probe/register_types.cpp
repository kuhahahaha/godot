#include "register_types.h"

#include "p01_probe_fixture.h"
#include "p01_capture_startup.h"

#include "core/object/class_db.h"

void initialize_p01_probe_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		p01_capture_startup();
		GDREGISTER_CLASS(P01ProbeFixture);
	}
}

void uninitialize_p01_probe_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		P01ProbeFixture::close_registry();
		p01_capture_shutdown();
	}
}
