# D VFX projection guard rebuild — 2026-10-27

ANGE commit `bba55f3ba` adds mount-time validation of published `SystemRegistryProjection` before scene mount and strengthens same-path deferred runtime erase validity checks. The rebuilt DSO is `38c3da7e…`, SCons and codesign both rc0.

Real non-headless Metal/mobile probes passed for VFX (2/1), no-VFX (1/0), and explicit close (active true→false). The malformed-source negative failed closed with `FILE-001`, rollback at `load_resources:vfx`, world entry false, and unload true.

No owner, operation, completion, readback, Ready, Verified, or GPU authority is created.
