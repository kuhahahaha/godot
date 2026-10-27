# K03 final pair VFX/U01/close recheck receipt — 2026-10-27

- Final editor `2ee0e5c3840782ce44514ae76b063a65e13eee8c22ca76c27f6b4a2e9b1b8853` and final DSO `149342a6a099199bc345bd2e23844491f58485f02ef2fc8529dd7da4fe0811f7` were used for all three probes.
- VFX: rc0, entries=2, systems=1, enter=true, unload=true.
- No-VFX: rc0, entries=1, systems=0, enter=true, unload=true.
- Explicit close: rc0, enter=true, active true→false after `exit_world()`, unload=true.
- No `GDEXTENSION_CLOSE_FAILED`; only known tree/flower default warnings.
- Final DSO direct codesign verification rc0; detached SCons shell exit code is not separately attributed, while its log ends at link/postlink signing.
- No U08/E09, operation, completion, readback authority, packet, physical disposal, Released, Ready, Verified, or GPU claim.
