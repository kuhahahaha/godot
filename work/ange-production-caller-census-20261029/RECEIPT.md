# ANGE production qualification caller census — 2026-10-29

This is a source-level correction to the older native open-items audit. It
does not claim a linked DSO, GPU execution, completion, readback, Ready or G1
result.

## Current source identity

- ANGE HEAD observed: `f93ad7c09362ab084e576524a91ec6bf08e711b3`.
- The worktree is mixed dirty; only the four inspected source files below are
  part of this census. No D source was edited by this receipt.

## Real production caller chain

The current source contains a normal production service path:

```text
host_attachment.cpp:700
  HostAttachmentRoot::... -> ParticleProgramLoader::service_qualification
particle_program_loader.cpp:611
  ProgramPreparationStore::pump_native
particle_program_loader.cpp:643
  ParticleAttributeStorage::start_qualification
particle_attribute_storage.cpp:70
  StageExecutor::start_native_qualification
stage_executor.cpp:5
  E09NativeRecorder construction
```

This supersedes the older statement that no normal production actor issued
`StageExecutor::start_native_qualification`. The call is source-visible in the
ordinary host service path. The separate public-ish
`ParticleProgramLoader::request_qualification` method remains a definition/API
surface with no additional call site in the current `src` census; it is not
needed to establish the service path above.

## Remaining boundaries

- `register_types.cpp` calls `require_main_rd_qualification()` only under
  `ANGE_NATIVE_TEST || ANGE_NATIVE_EXPORT`; the ordinary branch still uses the
  optional RD-pair scene requirements. Main-RD issuer admission therefore
  remains owner/integration scoped.
- This census proves caller wiring only. It does not prove the complete checked
  append → native issue/completion → same-request exact readback chain.
- No operation token, completion, readback, Ready or G1 claim is made.
