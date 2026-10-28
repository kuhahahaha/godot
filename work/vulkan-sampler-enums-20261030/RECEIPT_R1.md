# Vulkan sampler-enum validation — R22 — 2026-10-30

The Vulkan sampler boundary now rejects the remaining sentinel enum inputs before native creation: `COMPARE_OP_MAX` and `SAMPLER_BORDER_COLOR_MAX`, complementing the earlier filter and repeat guards. This is fail-closed input validation; valid sampler behavior and owner/operation/completion/readback/Ready/G1 semantics are unchanged.

Evidence:

- TDD RED: the old code accepted `COMPARE_OP_MAX` and `SAMPLER_BORDER_COLOR_MAX`; each focused test returned rc1 with one failed assertion.
- Final Debug focused cases: filter, repeat, compare, and border each pass 1 case / 3 assertions; Vulkan qualification regression is 22 cases / 92 assertions; K03/P2 is 22 cases / 350 assertions.
- Actual `use_ubsan=yes` Vulkan qualification is 22 cases / 92 assertions, rc0, with no sanitizer diagnostics.
- `template_release` build and `--version` rc0 (`4.7.2.stable.custom_build.0e55f5344`). Source diff check rc0.

Godot source HEAD at capture is `26abd06e9c95732be2cd49d72c5e7a6695657e2a`; implementation commit is `26abd06e9c`. This package is bounded native input-safety evidence and does not claim the separate owner or K03 operation/completion/readback joins.
