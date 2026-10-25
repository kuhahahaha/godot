# VFX rollback close failure: first-failure and owner-boundary blocker

**Date:** 2026-10-25 (Asia/Shanghai)
**Lane:** D / Godot native
**Disposition:** `BLOCKED`; source change intentionally not adopted in this round

## Decision

The available evidence does not establish a first `CLOSE_FAILED error=030`.
The reproducible dynamic-world chain instead reaches
`GDEXTENSION_LEVEL_DISPATCH_FAILED error=044 level=2 phase=D` first. The
`GDEXTENSION_CLOSE_FAILED error=030 route=5` observation belongs to an earlier
world-enter/explicit-unload failure chain after the LAST_REF/preflight path had
already rejected the close. It is therefore a secondary shutdown result, not a
safe root-cause target for another edit to `gdextension_library_exit_guard`.

No D source change is accepted from this investigation. Adding another typed
close gate, weakening the existing G1 gate, deleting `shutdown_callback`, or
converting the narrow control into a general runtime path would duplicate or
undo already closed lifecycle work.

## Reproduction and evidence

The fixed non-headless macOS Vulkan/mobile controls were run with the Godot
binary selected by the baseline and the guarded ANGE DSO configured by the
project. The control form is:

```sh
/Users/macbookair/Documents/Work/godot_ANGE/bin/godot.macos.editor.dev.arm64 \
  --rendering-method mobile --rendering-driver vulkan \
  --path /Users/macbookair/Documents/Work/ANGE/work/particle-integration-baseline-20261025/guarded-dso \
  --script world-global.gd
```

The corresponding explicit-unload control is the same command with
`explicit-unload.gd`. The authoritative captured outputs are:

- `.../d-current-world-global.log`: `ANGE_WORLD_LOAD_STATUS=0`,
  `ANGE_WORLD_CREATED=true`, `ANGE_WORLD_FREED=true`, followed by
  `GDEXTENSION_CLOSE_FAILED error=044 route=5` in the current capture.
- `.../d-current-explicit-unload.gd.log`: load/class/unload all report success;
  this is a control for explicit unload only.
- `.../d-object-schedule-scene-ack.log`: three
  `remove_resource_format_loader(null)` calls, a null
  `PhysicsServer3D::get_singleton()`, then
  `GDEXTENSION_LEVEL_DISPATCH_FAILED error=044 level=2 phase=D`, exit status 79.
- `.../guarded-holder-census.json`: ordinary holders are AudioLoader 3,
  ImageLoader 3, GLTFLoader 3, ResourceLoader 2, OS 2,
  RenderingDevice 2, and RenderingServer 2; typed code holds 1, active
  callbacks 0, live services 162, with the ordinary phase `UNSCHEDULED`.

A separate historical capture in the handoff records `error=030 route=5`
after the dynamic world path, but it does not contain a reliable preceding
`error=030` call-site failure. The same handoff identifies the earlier
LAST_REF refusal as an Input singleton instance-binding/BINDING terminal-action
mismatch. That historical mismatch was assigned to the A wrapper owner; it is
not evidence that the Godot close guard is the owner of the first failure.

## Failure path and ownership

The currently supported causal chain is:

1. Guarded DSO loads and the world is created and freed successfully.
2. Global shutdown enters the SCENE USER holder/service cleanup path.
3. Original cleanup calls are made with null loader arguments and the physics
   singleton is absent in this run.
4. The SCENE level dispatch preflight rejects the operation with `ERR_BUSY`
   (`044`, phase D).
5. A later close/retirement observation may surface as route-5 `030`, but that
   later code cannot be treated as the first failing call.

The actionable owner is the Godot manager/guard scheduling and the original
service-cleanup caller order, subject to the original Registry/Callable
contract. If the failing holder is an extension binding or wrapper terminal
action, that portion remains A-owned; D must not silently implement or rename
that binding. The census shows the concrete holders but does not authorize
inventing a new owner or exporting a replacement service registry.

The D-owned safe scope is limited to a contract-backed repair in the existing
manager scheduling/SCENE cleanup path, followed by a fresh fixed-binary,
fixed-DSO reproduction. Until that contract and the original caller ownership
are confirmed, this record remains a blocker.

## Verification status

- Source/log review completed against the Godot handoff and the particle
  integration baseline listed above.
- No full build or full test matrix was repeated: existing shared working-tree
  changes are broad, and the current evidence is sufficient to identify the
  first observed failure without creating a second binary.
- `git diff --check` is required for this receipt before commit.
- No runtime, GPU, PCK, renderer-output, K01 completion, R02 acceptance, or
  full particle-chain qualification is claimed.

## Changed-file and dirty-tree accounting

This round changes only this blocker document. The Godot checkout already has
shared dirty and untracked migration work (including extension lifecycle,
rendering, test-module, and `work/` evidence files); those files were not
modified, staged, or attributed to this receipt. The pre-existing deleted
`modules/p01_probe` lineage is likewise outside this change.

## Required next evidence

A future source change is admissible only after the original manager/Registry
owner supplies the exact SCENE dispatch and cleanup contract, or a new LLDB
trace identifies a D-owned caller. The next run must preserve the same Vulkan /
mobile invocation, record the Godot and DSO SHA-256 values, capture the first
error before any route-5 close result, and run both explicit-unload and
world-global controls. A later `030` without that first-failure evidence must
remain a secondary symptom.
