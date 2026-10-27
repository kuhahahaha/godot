# Current editor/DSO pair recheck after native landing — 2026-10-27

The fixed pair was rerun after the three native commits.

- Editor SHA-256: `d4938be792ae05f6868f73d13270c8ebe92292043eadadb6114717752924bb9f`
- ANGE DSO SHA-256: `a078f6dec6661d5fb5217c8b95c59d162f18ac5aa8bc81cef9e9c85c548fcb45`
- Godot source HEAD: `c2ebe2578021378700e0db0cd707d7406f6eecc6`
- ANGE source HEAD: `53ff6aa1d48aa3697beff91eb31fee850ca326af`
- Host: Apple M3 Pro, non-headless Metal/mobile.

VFX probe passed with `entries=2 systems=1`, extension status 0, world entry true, and unload true. The no-VFX control passed with `entries=1 systems=0`, extension status 0, world entry true, and unload true. The explicit close probe passed with extension status 0, world entry true, `active_before=true`, `active_after_exit=false`, and unload true.

These logs retain the known tree/flower authoring warnings only. This receipt does not prove U08/E09 association, operation, completion, readback bytes, packet publication, physical disposal, Released, Ready, Verified, or GPU qualification.
