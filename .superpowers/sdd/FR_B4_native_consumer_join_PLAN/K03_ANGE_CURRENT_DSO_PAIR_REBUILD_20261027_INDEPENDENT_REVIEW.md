# ANGE current DSO/editor pair rebuild independent review — 2026-10-27

Read-only review of the pair rebuild report/receipt and canonical runtime logs;
no source edits, builds, or reruns. Build and all four runtime log hashes match
the recorded values.

The pair is editor `bbe896df…` with ANGE DSO `b672d414…`; SCons and codesign
both return rc0. Non-headless Metal/mobile probes pass: VFX entries=2/systems=1,
no-VFX with no VFX source stage, and explicit close active true→false, all with
extension/world/unload true. The malformed VFX probe fails closed rc1 with
`VFX-SYN-001`, rollback at `load_resources:vfx`, and unload true.

This validates pair lifecycle and source rollback only. It does not provide
U08/E09 producer association, operation/completion/readback, packet, physical
disposal, Ready, Verified, or GPU authority; all claims are `false`.

**Decision: limited-scope accept; no P0–P3 findings.**
