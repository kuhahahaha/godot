# Metal Mobile heightfield expansion and shrink motion

Date: 2026-11-04. Source-matched editor: `work/particle-heightfield-mobile-extents-motion-20261105/source-matched-editor.arm64`, SHA-256 `6b8aa442e3843dabe4ef6904d7f03719481c438b07a092c2728de4772b53966b`. This is the editor previously built with the Mobile heightfield depth-clear change; the fixed `bin/` editor is older.

Both retained probes ran in visible Metal 4.0 Forward Mobile at fixed 60 Hz, rc 0, with no engine/script error, seal error, or crash. Each group returned eight finite active records and a 1024-byte RD particle buffer. The disabled-collision group shares the seed and particle settings with the collision group.

| Width transition | Collision group Y | Disabled control Y | Result |
| --- | ---: | ---: | --- |
| Static width 2 | -7.42889166 | -7.42889166 | Outside field |
| Midflight 2 → 8 | 0.10999990 | -7.42889166 | Collision admitted |
| Static width 8 | 0.10999990 | -7.42889166 | Collision admitted |
| Midflight 8 → 2 | -7.42889166 | -7.42889166 | Collision released |

The expansion probe also includes a sphere positive control: collision Y `1.00999999`, disabled control Y `-7.42889166`. Its `outside`, `expanded`, and `positive` predicates were all true. The shrink probe's `wide` and `shrunk` predicates were true.

Expansion command: `source-matched-editor.arm64 --path work/particle-heightfield-mobile-extents-motion-20261105 --rendering-driver metal --rendering-method mobile --fixed-fps 60 --script res://probe.gd`. Script SHA-256 `36e00e6f9d39966584ec364132d4cc002e92981350eecc39300c49822f21b51d`; raw `source-matched-metal-expansion.log` SHA-256 `f4aefd9482ef49f5ed2e6820dcc36d18566f92c0630d7eba1bb4ffe17d2719c5`.

Shrink command: same editor and flags, `--path work/particle-heightfield-forward-shrink-motion-20261105 --script /Users/macbookair/Documents/Work/godot_ANGE/work/particle-heightfield-forward-shrink-motion-20261105/probe.gd`. Script SHA-256 `18a03fc27af7e5dfa22c81543b4ded51bafb5be52c82cf61675915512737ee36`; raw `source-matched-metal-shrink.log` SHA-256 `bf14af37ce276830bedb706a00a72922d3a5ef6412ceb7bc808ed334e6021ca3`. The shrink script was passed by absolute path because Godot's CLI did not resolve `res://probe.gd` inside this project when called from the repository root. Earlier two failed script-path launches were discarded and did not contribute to these values.

This supplies Metal Mobile parity for live X-extent mutation on this host. It does not establish other platforms, arbitrary geometry, or the separate product ticket six-link gate. No production source or fixed editor was changed.
