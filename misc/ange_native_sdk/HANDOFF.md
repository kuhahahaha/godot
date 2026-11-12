# Matching custom native interface SDK

The three integration products are `include/gdextension_interface.h`, `include/godot_cpp/core/gdextension_interface_loader.hpp`, and `src/gdextension_interface_loader.cpp`. Together they are the complete custom native interface SDK increment for schema SHA-256 `1a7a839114b3ec7a79a9d5026fe78214023c6ed2370cd7784a5a6bdf4ffec1ca` (399 types / 388 interfaces). This delivery contains no ANGE or RmlUi library and no engine/extension binary.

The full schema was copied from the active isolated native build source recorded in `PROVENANCE.json`. The actual frozen History package generators are copied unchanged under `tools/`; `INPUT_PINS.json` authenticates their hashes against the formal generator receipt. `support/` contains four exact real frozen godot-cpp headers required solely by the loader syntax probe, with their source paths and hashes retained in `verification/SOURCE_STABILITY.json`.

Reproduce from any location with Python 3 and clang on the path:

```sh
python3 -B /absolute/path/to/sdk/replay_sdk.py
```

The script checks all pinned local inputs, regenerates the three products, verifies exact equality with the formal product hashes, generates a second time in a fresh temporary directory, and runs C11/C++17 header probes plus actual generated loader C++17 syntax. It records exact commands, compiler version, exit codes, stdout/stderr hashes, product hashes and unchanged input results in `verification/SDK_REPLAY.json`. Both header probes explicitly name the original SceneInputV1 and HistoryStaticV2/HistoryStateV2 DTOs and their procedure types.

All generation and probe commands passed with empty compile stderr. No full build was launched by this SDK delivery; no executable link, library ABI/runtime, native/GPU or World acceptance is claimed. The isolated native engine build remains root's separate running build. A's checkout was only read during the preceding SDK comparison and was not modified.
