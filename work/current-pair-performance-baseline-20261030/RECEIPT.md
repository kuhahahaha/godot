# Current editor/DSO/PCK performance and retirement baseline — 2026-10-30

## Scope

This is a bounded macOS arm64 baseline for the current Godot editor, ANGE DSO, and 2,188-byte PCK pair. It records wall time, peak RSS, peak footprint, test counts, and first runtime boundary. The source and PCK probes fail closed at E05 owner transport; no number below is GPU, visible-output, completion, readback, or Ready evidence.

## Identity

- Godot source HEAD: `870846d0512dc83a20f16c187c39c0f710401d4f`.
- Editor SHA-256: `b39543b6c5b8d96e3acabae8b3212710cc60ffc0248fa4f8261eab9491bb1be9`.
- ANGE source HEAD: `8e2f7d282e96e406b5311b23db74c16328dcc4cf`.
- DSO SHA-256: `38df3a53ee04aa6d977b313853a2bb3bd7efafe207a960920f80644535a4fcab`.
- PCK SHA-256: `35dbfdafebb916de15ca67004da811ee0aa05ebaf0e8b7bf3787f038cd0f0645` (2,188 bytes).

## Measurements

`/usr/bin/time -l` values are process-level macOS measurements; RSS and peak footprint are bytes.

| Operation | rc | real seconds | max RSS | peak footprint | observation |
| --- | ---: | ---: | ---: | ---: | --- |
| Source Metal U01/VFX | 2 | 1.06 | 345,964,544 | 455,361,832 | E05 SceneReady refusal; one PHS1 line, U01_EXTENSION=1 |
| PCK `--main-pack` probe | 1 | 0.63 | 304,594,944 | 442,991,864 | E05 SceneReady refusal; script does not reach U01 marker |
| Editor import | 0 | 48.07 | 1,123,696,640 | 984,893,096 | project/GDExtension scan and cache generation |
| PCK export-pack | 0 | 48.44 | 1,113,210,880 | 933,283,448 | macOS arm64 key resolves; x86_64 DSO warning |

Focused test/resource counters:

| Selector | rc | cases | assertions | real seconds | max RSS |
| --- | ---: | ---: | ---: | ---: | ---: |
| RenderingDevice | 0 | 12 | 214 | 0.31 | 173,735,936 |
| Qualification | 0 | 102 | 1,896 | 0.42 | 182,566,912 |
| K03 | 0 | 44 | 1,153 | 3.55 | 189,005,824 |
| R01 | 0 | 16 | 293 | 0.31 | 173,441,024 |
| Unknown pipeline retirement | 0 | 1 | 30 | 0.30 | 173,326,336 |
| Pipeline identity mismatch | 0 | 1 | 8 | 0.30 | 173,228,032 |

Godot does not expose a stable ObjectDB/resource count in these CLI probes. The baseline therefore records doctest case/assertion counts and PHS1/SceneReady markers; no object count is invented.

## Boundary and limitations

Source and PCK pair probes both stop at `host.storage_owner_transport/E05/UNAVAILABLE`. This prevents measuring authenticated owner admission, native submit/completion, same-request readback, texture/sampler binding, visible Sprite/particle output, and Ready/G1. Import/pack timings include editor startup, GDExtension validation, generated cache work, and the current fixture's absolute DSO path; they are not production shipping performance targets.
