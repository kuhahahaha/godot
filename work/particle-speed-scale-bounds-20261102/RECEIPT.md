# Particle speed-scale non-negative range receipt

Date: 2026-11-02

CPU/GPU particle nodes and RD/GLES3 storage reject negative speed scales before state mutation. Zero remains valid. The source fixture is RED at pre-change `HEAD` (0/6) and GREEN on the working source (6/6); `git diff --cached --check` passes. The staged patch contains only the six speed-scale setter/helper functions and this fixture/receipt.

A pre-existing editor binary (`bin/godot.macos.editor.arm64`, controlled-build SHA256 `703dec7a66e99bf85e336cd3fb368e12f2a146f5aff318d0afa6c211a1406f65`) ran `[Particles] speed scale rejects negative values`: 1 case / 4 assertions passed. That binary predates this exact staged source patch, so this is corroborating runtime evidence, not a rebuild of the committed tree. A fresh source build remains unverified for this isolated commit.
