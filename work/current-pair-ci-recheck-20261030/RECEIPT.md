# Current-pair CI and runtime recheck — 2026-10-30

## Outcome

No new Godot production defect was found in this recheck, so no source code change is included. The rebuilt editor and explicitly paired current ANGE DSO reproduce the same fail-closed E05 owner boundary. The public CI failure in the existing record is a stale Linux run against an older public head; its MessageQueue capacity test repair is already present locally as commit `6237e0b4f545f0a6a8bc8e37f26bbf991731f0fc`.

## Pair identity

- Godot source HEAD: `331ddf1d7fc763598834eb5497549681f1710b23`.
- Rebuilt editor SHA-256: `cfbce3ac09a77a2ad9ea496e73183193b9a63767df4b19e4972e6aa25658884c`.
- ANGE source HEAD: `8e2f7d282e96e406b5311b23db74c16328dcc4cf`.
- Explicit current DSO used for this recheck: `/Users/macbookair/Documents/Work/ANGE/work/godot-native-chain-long-20261030/libange.macos.template_debug.arm64.dylib`, SHA-256 `38df3a53ee04aa6d977b313853a2bb3bd7efafe207a960920f80644535a4fcab`.
- Existing `work/u01-vfx-current-pair-20261030` fixture points at the older `/Users/macbookair/Documents/Work/ANGE/bin/...` DSO (SHA-256 `187850139bba4c1e124af25629ef4901a958ef9188e86b7b126e99e6b50fd96b`). To avoid a false pair claim, this run used a temporary fixture copy with its `.gdextension` path rewritten to the explicit current DSO.

## Commands and results

Editor build (rc 0):

```text
/usr/bin/python3 /Users/macbookair/.local/bin/scons -Q -j4 platform=macos target=editor dev_build=yes tests=yes builtin_glslang=yes
```

The only linker diagnostic was the pre-existing MoltenVK deployment-target warning (object macOS 12, editor target macOS 11).

Editor tests (all rc 0):

| Selector | Cases | Assertions |
| --- | ---: | ---: |
| `*[CallQueue]*` | 6 | 92 |
| `*[Qualification]*` | 102 | 1,896 |
| `*[CallQueue] Capacity diagnostics preserve accepted work*` | 1 | 39 |

The capacity test emits the expected diagnostic lines for rejected call/set/notification attempts and confirms the accepted message remains intact.

Metal runtime with the explicit current DSO (`runtime-vfx`, `runtime-no-vfx`, `runtime-invalid-vfx`) returned rc 2 for all three. Each reached Metal 4.0 startup and extension load, then refused scene installation at the same first boundary:

```text
[ANGE] guarded scene initialization refused stage=install_main_storage code=13 field=host.storage_owner_transport owner=E05 expected=1 actual=0
[ANGE] guarded SceneReady materialization refused stage=install_main_storage
U01_EXTENSION=1
```

No world admission, VFX parser result, native submit/completion/readback, Ready, G1, or GPU claim is made.

## CI classification

Existing `work/godot-ci-native-gpu-20261030/CI_STATUS.json` records public run `40930940598` attempt 2 failing at public head `65c6a08ee6fb6b1408d9ab4fe41859d3f7a9b83d`, job `Linux Editor doubles GCC ASAN/UBSAN linker=lld`, first error `tests/core/object/test_message_queue.cpp:265 CHECK(result == ERR_OUT_OF_MEMORY) values CHECK(0 == 6)`. The sanitizer build and link completed; this is a test-contract failure, not an lld setup failure. Current source contains the capacity-tail fix from `6237e0b4f5`, and the focused test is green locally. A new public CI attempt is still required before claiming Linux ASAN/UBSAN green; it cannot be run from this local macOS-only recheck.
