# K03 typed-close cancel owner boundary receipt — 2026-10-27

- Source census: RenderingServer cancel binding → RenderingServerDefault → ParticlesStorage.
- Current implementation: acquire has no source owner, seal has no lease, cancel returns `ERR_INVALID_PARAMETER`.
- Final release: `ExternalOutputStorage::release_native_owner()` → authenticated Registry metadata release; it is not cancel.
- Missing owner input: same upload/record-prepared ticket, same-request association, checked cancel disposition, and completion/retirement receipt.
- Queue status: external A/owner blocker recorded; independent local lifecycle work remains active.
- No operation/completion/readback/Ready/Verified/packet/physical/GPU claim.
