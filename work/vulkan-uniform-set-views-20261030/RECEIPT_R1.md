# Vulkan uniform-set binding-view validation — R39 — 2026-10-30

`uniform_set_create()` now rejects a nonempty `BoundUniform` view with a null backing pointer before uniform/resource access. The malformed path previously crashed with SIGSEGV. This is fail-closed native input validation; valid uniform-set creation and owner/operation/completion/readback/Ready/G1 semantics are unchanged.

Evidence:

- TDD RED: the null binding view case crashed with SIGSEGV, rc134.
- Current Debug: focused case passes 1 case / 3 assertions; Vulkan qualification regression is 40 cases / 146 assertions; K03/P2 is 22 cases / 350 assertions.
- Actual `use_ubsan=yes` build/run: Vulkan qualification 40/146 rc0 with no sanitizer diagnostics.
- `template_release` build and `--version` rc0 (`4.7.2.stable.custom_build.e9a7c1760`). Source diff check rc0.

Godot source HEAD at capture is `bafb67f9e72d255b6b5a750569704bd0783d5a91`; implementation commit is `bafb67f9e7`. This is bounded native descriptor-input validation and does not close the separate owner or K03 operation/completion/readback joins.
