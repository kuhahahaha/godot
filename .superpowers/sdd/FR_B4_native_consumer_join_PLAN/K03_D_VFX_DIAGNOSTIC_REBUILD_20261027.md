# D VFX semantic diagnostic rebuild — 2026-10-27

ANGE commit `253d6fa22` lands the WorldLoader diagnostic path that preserves the semantic bundle classification and remains fail-closed. A full SCons rebuild completed rc0 and codesign verification completed rc0; the DSO SHA-256 is `a60b3fa5a2f9de72cbb0be6ccca0d3ae33b87c5108a4b6caf2585d0ae4106a33`.

An isolated non-headless Metal/mobile malformed-source fixture failed as intended: first error `FILE-001 cannot read VFX source`, followed by resource-stage failure and rollback at `load_resources:vfx`; extension status remained 0 and unload status remained 0. No fallback registry, synthetic owner, or qualification claim was introduced.

The canonical VFX fixture remains a separate positive pair receipt. This package only proves D-side diagnostic classification and rollback behavior.
