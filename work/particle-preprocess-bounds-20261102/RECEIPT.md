# Particle preprocess non-negative range receipt

Date: 2026-11-02

CPU/GPU particle nodes and RD/GLES3 storage reject negative pre-process and requested process times before state mutation. The source fixture is RED at pre-change `HEAD` (0/10) and GREEN on current source (10/10). The staged source patch is restricted to the relevant setter/helper functions.

A pre-existing editor binary (`bin/godot.macos.editor.arm64`, controlled-build SHA256 `703dec7a66e99bf85e336cd3fb368e12f2a146f5aff318d0afa6c211a1406f65`) ran `[CPUParticles] pre-process time rejects negative values`: 1 case / 2 assertions passed. That binary predates this exact staged source patch; a fresh rebuild of the isolated commit remains unverified.
