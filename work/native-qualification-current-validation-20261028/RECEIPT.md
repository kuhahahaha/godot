# Current native Qualification validation receipt

Date: 2026-10-28

The current mixed Godot checkout was exercised with the existing dev editor binary after the native setup2 cleanup build:

- `bin/godot.macos.editor.dev.arm64 --headless --test --test-case='*[Qualification]*'`: rc0, 27 cases / 357 assertions.
- `bin/godot.macos.editor.dev.arm64 --headless --test --test-case='*[QualificationRt]*'`: rc0, 4 cases / 378 assertions.

Raw logs and the source/binary hashes are in this directory. The result validates the current D native qualification and RT state only. It does not claim a fresh build in this turn, canonical ANGE fixed-pair acceptance, A Verified, operation/completion/readback authority, Ready, or GPU qualification.

The current source differs from the older `cfada39…` handoff in measured paths; these tests were run against the current mixed tree's binary and were not produced by applying the handoff over it.
