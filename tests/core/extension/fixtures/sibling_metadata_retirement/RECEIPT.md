# Sibling metadata retirement receipt

Two real SceneOnly generations created within one original request could not retire: each generation compared the request's two pending metadata allocations against only its own one allocation. Both returned `ERR_BUSY`, while actual `particles_free` required a released generation.

The fix records `metadata_generation_bound` in the original private resource row only after successful real `GENERATION` owner installation. Terminal accounting permits these sibling CPU allocations to outlive their individual root pins. The marker survives root unpin, so the second generation can terminate while the first RID and allocation remain alive. Binding metadata to an uninstalled pin does not set the marker. No public ABI or new authority registry is added.

Original source/device, list/action, possible-obligation, known/complete, other physical-return, full-report and final reclaim gates remain enforced. The final request still waits for actual allocation returns.

## Frozen source and compiled overlay

Base: `9558cdc89578886e081b59b472fc476f3202467d`.

- [SOURCE_CHANGE.patch](SOURCE_CHANGE.patch) is the exact final delta for `gdextension_production_request.h`, `gdextension_production_request.cpp`, and `test_production_request.cpp`.
- [COMPILED_OVERLAY.patch](COMPILED_OVERLAY.patch) separately records the four checked-draw source/test files included in the tested green binary.
- [RECEIPT.json](RECEIPT.json) records source and artifact hashes. The source patch is the final green delta; it is not represented as the initial red test revision.

## Observed red and green

| Run | Result | Evidence |
| --- | --- | --- |
| Initial sibling regression | First `native_error` was 44 (`ERR_BUSY`), expected 0; `native_released` was false | [Red log](checked-metadata-red.log) |
| Red process termination | Exit -6, subsequent live-RID fixture destructor SIGABRT; core dumps disabled | The semantic assertions above are the detection oracle, not an arbitrary crash |
| Green build | Exit 0 | [Build log](checked-draw-metadata-green-build.log) |
| Focused green | Exit 0; 66 cases, 6623 assertions passed | [Focused log](checked-draw-metadata-focused-green.log) |
| Combined green | Exit 0; 179 cases, 80359 assertions passed | [Combined log](checked-draw-metadata-combined-green.log) |

Green binary: 156730024 bytes, SHA-256 `d75aa4372d76d47aa98640cb39a7f5db55da86b6f9139daf480372ab10e11c79`. The red binary was not frozen; no red digest is claimed. Red used the initial two-generation test before the green uninstalled-pin negative control was added.

[COMMANDS.json](COMMANDS.json) preserves test commands from the original run records and the exact green build argv reported by root's actual execution, session 59782. The build log itself does not embed that argv. The original [red](checked-draw-metadata-red-runs.json) and [green](checked-draw-metadata-green-runs.json) run JSON files are copied unchanged. The red draw log belongs to the separately identified compiled overlay.

The regression uses two guarded `CreateV2` calls and two withdrawals, then requires both actual root terminals before either RID is freed. It checks pending allocation counts `2 -> 1 -> 0` through actual `particles_free`. The negative control binds extra metadata to a real but uninstalled pin: it blocks terminal with a nonzero root serial, remains blocking after unpin, and stops blocking only after actual metadata return.

Independent Sol review (`/root/review_collision_fixture`), relayed by root, passed: marker installation follows all pin/context/owner/conflict checks and successful generation linkage; slot or failed bindings do not set it. Terminal counting only exempts the proven CPU subset.

## Limits

This is original-request/SceneOnly CPU ownership and retirement evidence using the mechanical native fixture. It is not real GPU execution, native completion qualification, a dynamic source scenario, Full/profile/camera/read-issuer implementation, runtime/full-ready acceptance, or platform qualification. Aggregate green counts include the checked-draw overlay.
