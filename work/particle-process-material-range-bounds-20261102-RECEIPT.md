# Particle process material range bounds receipt

Date: 2026-11-02. Base before this change: `f9782e2e43`.

This commit aligns eight `ParticleProcessMaterial` setters with their exposed ranges: finite parameter vector, point count 1..1000000, turbulence strength 0..20, turbulence speed randomness 0..4, sub-emitter frequency 0.01..100, and each sub-emitter amount 1..32. The point-count editor property lower bound also changes from zero to one. Only those nine changes are staged from a much larger dirty material file.

The source fixture failed on the pre-fix HEAD and passed 10/10 on the worktree. The existing mixed-worktree editor binary SHA256 `b87ce88c36142d213b2ecc6e79e22995e4de5fa544ca3971dab89f7cc46e409d` ran three focused doctests: setter bounds 2/2 assertions, turbulence/collision bounds 6/6, and sub-emitter bounds 4/4. Those tests check selected state-preserving rejection paths. The current commit's source was not built in isolation, and no live GPU material rendering is claimed.
