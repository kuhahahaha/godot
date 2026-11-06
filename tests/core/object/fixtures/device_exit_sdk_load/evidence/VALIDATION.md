# SDK62 dynamic load validation

Actual macOS arm64 run passed guarded and legacy controls using SDK62b1b20c2ddeddaca0236e9651c66a2f5727e83e. Run command: python3 tests/core/object/fixtures/device_exit_sdk_load/run.py --sdk-root /Users/macbookair/.codex/worktrees/e25-e26-final-integration/ANGE/godot-cpp --binary /tmp/b4-sdk62-load-runtime/host.arm64 --output /tmp/b4-sdk-load-repo-fixture-mypy-green.

The runtime host is frozen SHA256 dd3f837f678d75e81cfe55de8e4cd8321e9e8c78aee5928c35484fb44726591c, identifying5d56fc5de plus internal relay/test working differences. Original typed exports and lookup source match fixed5d56fc5de36df5c1c5a7becf6fcfae6ed6d50557. This is exact runtime-byte evidence, not a pristine-build reproducibility claim. Full command/process/output artifact hashes are in RESULT.json; raw runtime artifacts remain in the stated /tmp directory.

Guarded original entry: all41 addresses present, direct SDK load true, original guard registration and eight distinct lifecycle APIs (14calls on this editor) all0, real manager unload0, process0/stderr empty. Legacy original entry:41 hidden, loadfalse, normal shutdown callbacks complete, process0/stderr empty. No qualification/session/SceneReady/GPU business-call coverage is claimed.

Independent source and artifact-hash review found no issue. Parent fixed Python receipt typing after mypy caught inference errors, then reran both real controls successfully. ruff0.15.8 check/format, mypy1.19.1, Python compile and clang-format passed. The independent probe uses .cc because tests/SCsub collects .cpp recursively; external SDK remains explicit runtime-test input.

After adding the required copyright header, the real runner was repeated with the current engine snapshot and both guarded/legacy controls passed again: /tmp/b4-sdk-probe-standalone-ci-context-green. STANDALONE_CONTEXT_RESULT.json identifies those exact bytes. The engine-only Linux clangd compilation database lacks this external SDK; .github/changed_files.yml excludes only probe.cc from that database check. Its normal source formatting checks and actual standalone clang++ -Wall -Wextra -Werror compilation remain enabled.
