# D VFX semantic diagnostic rebuild independent review — 2026-10-27

Read-only review of the diagnostic JSON/Markdown and negative-probe log; no
source edits or reruns.

The package records ANGE commit `253d6fa22`, DSO SHA `a60b3fa5…`, full SCons
rebuild rc0, and codesign rc0. The malformed non-headless Metal/mobile fixture
fails closed as intended: first error `FILE-001 cannot read VFX source`, then
resource-stage failure with rollback at `load_resources:vfx`; extension and
unload statuses remain 0. The negative log hash matches the recorded value.

This package is limited to D-side diagnostic classification and rollback. It
introduces no fallback/provider/owner synthesis and does not establish native
operation, completion, readback, packet, physical disposal, Released, Ready,
Verified, or GPU authority. All claims are `false`.

**Decision: limited-scope accept; no P0–P3 findings.**
