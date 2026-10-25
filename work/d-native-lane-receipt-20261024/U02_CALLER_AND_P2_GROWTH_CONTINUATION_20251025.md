# U02 caller and P2 recording-growth continuation receipt

**Date:** 2026-10-25 (Asia/Shanghai)
**Lane:** D / Godot native
**Disposition:** no new production edit; owner boundary and evidence state recorded

## U02 caller audit

The fixed VFX rollback reaches the ANGE-side caller
`src/godot/loader/world_loader.cpp:1980-1997`, in
`WorldLoader::load_vfx_sources()`. After `prepare_vfx_sources()` produces a
ready model with non-empty `emitter_views`, the caller emits
`VFX-TOOL-001 owner_unavailable: U02 System semantic issuer is not connected`
and returns `false`. The caller then rolls back the `load_resources:vfx` stage.
This happens before GPU/native-unit creation, DeviceSession use, RenderingDevice
recording, or Godot holder cleanup.

The issuer is the original U02 System semantic authority. The Godot native lane
has no adopted SystemRegistry projection, retained System/Emitter issuer, or
contract-authorized constructor from which it could issue one. `HostAttachment`
and `ParticleProgramLoader` only own the preparation/session children after
SCENE installation; they do not mint U02 semantic identity. Adding a fallback
there would fabricate authority and change the failure contract. No Godot or
ANGE production source was changed.

## P2 recording-growth audit

The P2 production path in `servers/rendering/rendering_device.cpp` already has
the reviewed sequence:

1. `_acquire_transfer_worker()` selects a locked worker and enters the
   `recording=true` full-staging branch.
2. `_end_transfer_worker()` and `_submit_transfer_worker()` preserve queue
   report, serial, issued state, and typed error.
3. `_wait_for_transfer_worker()` gates replacement on matching completion,
   checked fence recycle, and command-pool reset.
4. Any failed gate poisons/releases the worker and refuses the acquisition;
   the staging buffer is not replaced after an untrusted prefix.

The independent post-fix review is
`work/p2-transfer-successor/FINAL_REVIEW.md` with disposition
`BOUNDED_APPROVAL_AFTER_AUTHOR_FIX`; its pinned seven-mode growth evidence is
21 cases / 333 assertions, with focused growth 1 case / 57 assertions. This
remains a constituent tranche only (`acceptance=false`, `runtime_credit=false`,
`production_ready=false`). It does not authorize another defensive rewrite.

The current checkout's `rendering_device.cpp` is a shared dirty migration
(`d103df94...` at this receipt), so its bytes differ from the historical pinned
P2 source identity. I did not stage or attribute those shared changes to this
continuation. A direct local selector attempt against the existing binary
returned doctest success with **0 cases / 0 assertions and 1609 skipped**; this
binary therefore provides no P2 runtime credit and is explicitly not reported
as a passing focused test.

## Owner decision and next admissible action

- **U02 semantic issuer:** original ANGE/U02 owner; pending adopted issuer,
  projection, and retained System/Emitter input. D cannot implement it.
- **Godot native preparation/session:** D/A integration seam; already has
  fail-closed installation and native-device binding. No missing D-owned caller
  was found for this VFX failure.
- **P2 recording-growth:** D-owned source seam is present and independently
  reviewed. A new edit requires a newly observed defect in the current source,
  not a repeat of the closed seven-mode review.

The next valid D production change requires either (a) an adopted U02 issuer
and a concrete downstream Godot consumer call, or (b) a fresh P2 regression
that exercises the current binary/source and identifies a source defect. Until
then, the correct state is pending/blocker, not a fabricated caller or a
repeated close/build loop.

## Changed files and verification

This round changed only this receipt document. `git diff --check` is required
before commit. Existing Godot shared worktree changes and untracked migration
files were not reset, cleaned, staged, or attributed.
