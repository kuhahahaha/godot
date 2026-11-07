# Native preparation range and lifetime corrections

Vertex arrays now validate the complete binding-offset/attribute/stride fetch range and calculate instanced capacity from complete elements, preserving constant-stride and all-instance point-size paths. Checked pipeline recording validates the actual framebuffer format and subpass before cached binding or graph effects. Shader version destruction saves its independently allocated mutex before freeing the Version slot.

Root built with `scons -Q platform=macos target=editor tests=yes debug_symbols=no module_particles_runtime_tests_enabled=yes -j3` (exit 0). Focused validation passed 19 cases / 7909 assertions; combined validation passed 198 / 88268, exit 0. Combined output has three expected pipeline-factory failure diagnostics and three foreign-camera negative diagnostics. Exact argv and binary identity are in `native-preparation-green-runs.json`.

Original pipeline/range behavior failed the added regression assertions; red source snapshots, commands and identity are under `red/`, with full logs alongside. Red already contained the mutex fix and is not a red/green proof for that fix. Independent source reviews and detailed scope limits accompany this receipt.

These controlled CPU/dummy checks do not establish GPU qualification, TSan results, arbitrary concurrent same-RID safety, or completion of the native output issuer/end-to-end particle queue. The large stride/count negative fixture reaches the offset guard first; product overflow safety additionally follows from uint32 operand bounds.
