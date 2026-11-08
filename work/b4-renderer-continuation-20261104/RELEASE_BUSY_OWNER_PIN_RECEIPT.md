# Hidden native observer release BUSY coverage

The new RecordSource test drives the existing hidden-native-report path after matching completion. ERR_BUSY from native_call_release retains the report, frame observer, ticket frame reference, resource pin and original resource qualification usage. A successful retry returns that observer; an additional stall performs no repeated release. The existing controlled frame-ticket return then drops the resource pin and the separate ticket-native observer returns through its original release function.

Build command: `scons -Q tests=yes platform=macos target=editor debug_symbols=no module_particles_runtime_tests_enabled=yes -j2`, exit 0. Test command: `bin/godot.macos.editor.arm64 --headless --test --test-case='*[RecordSource]*'`, exit 0, 19 cases and 726 assertions passed.

The test passed on its first execution. This is additional coverage of existing behavior, not a production fix or behavioral RED/GREEN. It proves CPU finite-driver observer/pin retention, not real GPU failure recovery, physical GPU allocation retirement, E05/U09 ACK or complete Sprite/FullBirth output. No production source was changed.
