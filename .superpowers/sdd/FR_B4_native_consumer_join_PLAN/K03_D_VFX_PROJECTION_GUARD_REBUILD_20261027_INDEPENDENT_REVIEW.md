# D VFX projection guard rebuild independent review — 2026-10-27

Read-only review of the D guard rebuild JSON/Markdown/receipt and probe logs;
no source edits, builds, or reruns. All four log hashes match their `/tmp`
artifacts.

ANGE commit `bba55f3ba` and DSO `38c3da7e…` are recorded with build and
codesign rc0. VFX (entries=2/systems=1), no-VFX (entries=1/systems=0), and
explicit close (active true→false) all pass rc0 with extension/world/unload
markers true. The malformed-source probe fails closed as intended: rc1,
`FILE-001 cannot read VFX source`, rollback at `load_resources:vfx`, no world
entry, and unload true.

Scope is D source validation/rollback and same-path erase guard only. No owner,
operation, completion, readback, packet, physical disposal, Released, Ready,
Verified, or GPU authority is created; all claims are `false`.

**Decision: limited-scope accept; no P0–P3 findings.**
