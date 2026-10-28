# Current native regression recheck

Date: 2026-10-29

Current Godot editor binary: `bin/godot.macos.editor.dev.arm64`, SHA256 `d216693315918595bf7cf46d57006d88ede1d7000fe3cd68fac06afb63edffac`.

- `--headless --test --test-case='*device_exit*'`: rc0, 133 cases / 2,619 assertions.
- `--headless --test --test-case='[R02][Graph]*'`: rc0, 6 cases / 99 assertions.
- `--headless --test --test-case='[K03][P2]*'`: rc0, 22 cases / 350 assertions.

Raw logs are retained beside this receipt. The typed-close output contains the known guarded mock initializer diagnostic but no failed test. These are native regression controls only; they do not supply the missing production R02 checked append, original operation association, native completion/readback join, U01 owner enrollment, Ready, or G1 proof.
