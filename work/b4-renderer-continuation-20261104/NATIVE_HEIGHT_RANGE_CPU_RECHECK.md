# NativeHeightRange CPU recheck

Fresh command:
`bin/godot.macos.editor.arm64 --headless --test --test-case='*[NativeHeightRange]*'`

Result: exit 0, 21 cases, 688 assertions, 0 failures.

This rechecks exact slot/epoch mapping, checked sink refusal and restoration, worker custody, native completion/return, post-drain BUSY retry, and finalization guards. It remains CPU/fake-driver evidence. No Scene HeightFrame producer, geometry-range consumer, populated height draw, public Sprite admission, or GPU qualification is inferred.
