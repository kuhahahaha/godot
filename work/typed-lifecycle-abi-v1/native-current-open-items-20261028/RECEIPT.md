# Current native open-items audit

Date: 2026-10-28

This audit answers whether the Godot native lane has no remaining unlanded work. It does not: the native implementation line is real and independently validated, but production integration and owner joins remain open. The open items are recorded separately so they do not stop unrelated queue work.

## Current evidence

- Godot source HEAD: `ac8d1ba9492e9c7ab1864659eed3858229cceb6d`.
- Reviewed native handoff: `cfada39de8cbd730df2d08b7765bc26c2abfbc81`, with historical committed-host evidence of 161 CPU cases / 3001 assertions, six QRT controls and nine PHS1 rows / 233 checks. The current mixed checkout differs from that handoff in 19 of 42 selected paths; no blind overwrite was performed.
- Current native controls remain green in their bounded scopes: Qualification 27/357, QualificationRt 4/378, QualificationNative 3/59, and SceneReady 11/145. Their receipts explicitly do not claim owner operation/completion/readback, Ready, Verified, or GPU qualification.
- Current ANGE HEAD used for the source census: `745aa3d804d18187ba918d55a532e531951de24f`. Exact command output and source hashes are in `COMMAND_OUTPUT.json` and `SOURCE_HASHES.txt`.

## Unlanded native items

1. **Main-RD qualification issuer/registration.** `require_main_rd_qualification()` is present in the godot-cpp binding and in a native test, but there is no production `register_types` caller in the current source census. The default remains fail-closed. The A/SDK owner must wire the original main-RD issuer and preserve the existing admission checks.
2. **Production particle qualification caller.** `StageExecutor::start_native_qualification` and `ParticleProgramLoader::request_qualification` are implemented definitions, but the current production census has no normal preparation/record actor issuing that sequence. Definitions and tests are not promoted to a production caller.
3. **R02/K01 graph-to-native join.** Godot now has real graph `RecordHandle` and command-buffer-slot mapping, including qualification ticket storage, but the full checked append outcome, request-owned operation/native issuer association, completion settlement, and same-request exact-range readback are still absent from the current acceptance proof.
4. **Runtime wrapper owner scheduling.** The original SceneTree/Engine wrapper objects created after SCENE initialization still require exact BINDING terminal enrollment and final instance-binding disposal before G1. Constructor phase, ObjectID, or holder count cannot substitute for the original owner action.

## Queue policy

Items 1–4 are precise integration/owner boundaries. They constrain only their corresponding native/R01 acceptance rows. The independent D authoring/cache/provider work continues; no fallback, synthetic association/operation/completion/readback, Ready promotion, or G1 relaxation was introduced.

## Independent controls run during this audit

- `./bin/ange_test '[vfx_authoring]' --reporter compact`: rc 0, 131 cases / 2340 assertions.
- `./bin/ange_test_artifact_cache --reporter compact`: rc 0, 11 cases / 316 assertions.

These are bounded CPU/source controls and do not promote native or Ready authority.
