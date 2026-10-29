# Public particle mainline integration status

Public local Godot branch: `codex/particle-mainline`. Current branch HEAD is fixed by `BRANCH_CREATION.json`; no reset, force, fetch, merge, push or remote publication was performed. The branch started from the pre-existing dirty `d/e06-policy-core` checkout and retains unrelated dirty paths; only explicitly named commits are part of this checkpoint.

## Accepted local source increment

`b364625644` adds the canonical C fixture `tests/core/object/fixtures/device_exit_dynamic_library.c` and commits the existing `drivers/metal/SCsub` compiler flag. The fixture is the original `work/typed-lifecycle-abi-v1/native-dynamic/fixture.c` body with only the include redirected to the generated in-tree core ABI header. Its private macOS dylib build and three real dynamic tests are recorded in `DYNAMIC_FIXTURE_*`; 3 cases / 32 assertions pass with the frozen editor. The generated library is a local product and remains excluded from Git.

The Metal flag applies only to `env_metal` and its third-party clone. metal-cpp `NS::SharedPtr` has an unconditional `m_pObject->release()` destructor/reset path; current production driver code contains legal empty SharedPtr locals (for example native call wait and shader compile options). The flag preserves the intended Objective-C nil-message control-flow assumption for this third-party source subtree. It is retained as a pre-existing production build input, not presented as a new isolated optimizer fix, universal backend proof, or substitute for real Metal object tests. Existing current binary/Metal controls and `drivers/metal/SCsub` hash are pinned in `INPUTS` and the source manifest.

## Current three-repository identity

- Godot: local `codex/particle-mainline`, HEAD recorded in `INPUTS`, fixed editor SHA `7e63110bf209811e2b390bea51322d36af01df63d9f1c7c516ad7ef42d8bf1d6`; generated C ABI body matches current ANGE godot-cpp SDK header from `#pragma once` onward.
- ANGE: author integration commit `bb778b6bbeecaa2182119e1c99da69f37d12a3bf`, DSO SHA `0bad54500da5143733a8c1ce23e48657981e9b1dd607368cf093ea853dece23d`; A runtime receipt is copied verbatim and input hashes are unchanged.
- SDK/API: generated `gdextension_interface.json` and core header snapshots are included; ABI comparison is body equality only and does not certify owner admission.

## Actual joint runtime

A's real non-headless Metal/mobile/safe pair exits rc2 at `install_main_storage`, with `host.storage_owner_transport`, E05, expected1/actual0; `U01_EXTENSION=1`. This is the first honest joint integration boundary. It is not an E09/Scope/Ready result. The original owner must still provide no-world U08 storage/account inputs before main pair/storage, native port, qualification context and observer can commit. U02 production holder→E06 request/wait/Prepared publication is also absent.

## Native scope and unresolved feature boundary

The independent D native substrate remains valid at its own receipts (original numeric safe Metal and QB-D05 map Error/raw code transport). Method-native is explicitly incomplete: C04 owns actual descriptor maps; C05 owns `materialize_uniform_sets` and returns the UniformSetLease/actual binding receipt; E06 is shader/pipeline only; E09 consumes that receipt. Texture load still needs its real pixel producer/reverse dependency/pin/native port. Dimensions stays metadata-only. Numeric set0 storage tests do not certify method set1/texture/sampler behavior. Unknown or unsupported native effects must refuse before new evidence.

This checkpoint is a shared source/input baseline, not a full cross-repository merge, FR completion, GPU qualification, E09 production receipt, Verified, U08, Ready or G1 claim.
