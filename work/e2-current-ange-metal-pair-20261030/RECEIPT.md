# e2 Godot / ANGE Metal pair — 2026-10-30

The fixed candidate Godot editor SHA-256 `35956fceb7c19a83d1d742e8b6a7841e731063597ca351a1d758bc9361d6c396` and existing ANGE DSO SHA-256 `38df3a53ee04aa6d977b313853a2bb3bd7efafe207a960920f80644535a4fcab` were run on non-headless macOS Metal 4.0 Forward Mobile, Apple M3 Pro, safe render thread. Exact commands, paths, rc and pre/post identities are in RUN.json; both binaries remained hash-stable.

VFX, no-VFX and invalid-VFX controls all exited 2 after `install_main_storage` refused `host.storage_owner_transport`, owner E05, code 13, expected 1 / actual 0; `U01_EXTENSION=1` and no enter-world or unload marker followed. The first failure precedes VFX source processing. This is a fail-closed negative pair result, not U01, native completion/readback, Ready or G1 success; the editor candidate itself remains subject to its independent integration review.
