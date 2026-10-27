# ANGE E09 production caller gap — 2026-10-28

The source census confirms that `bind_main_device()` has one real registration path (`src/register_types.cpp:335`), but the production tree has no caller for `ParticleProgramLoader::request_native`, `poll_native`, `acquire_native`, or `request_qualification`. `StageExecutor::start_native_qualification()` also has no production caller; its references are declarations/definition and test-support paths.

The first missing production boundary is the original particle producer/admission path that supplies the loader request and same-request E09 qualification start. This is an external owner/producer gap. No synthetic call, test helper, serial, or diagnostic boolean is promoted to authority.
