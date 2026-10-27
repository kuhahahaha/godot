# Native template-release artifact receipt — 2026-10-28

The current Godot tree produced `bin/godot.macos.template_release.arm64` during a live SCons build observed at PID 71888. The resulting file is a 64-bit arm64 Mach-O, passes strict deep code-sign verification, and starts headless far enough to report `4.7.2.stable.custom_build.75d99c2e6`.

Artifact SHA-256: `0c2983a63ede8f0b441f09a4283804b7f41c40fb046149fe239ef213fcc51c7f`

The builder's terminal exit code was not visible to this thread, so this receipt is bounded to artifact identity and startup verification. It does not claim full native acceptance, qualification, owner authority, completion, readback, Ready, GPU, or G1 closure. The four current native open-items remain recorded in `native-open-items-revalidation-r2-20261028/`.
