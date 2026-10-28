# E20 graph handle/path native slice — 2026-10-29

This receipt covers one bounded Godot-native slice from the E20 implementation
input. It adds only real graph command handles for texture clear/update and a
read-only adjacency-path query. It does not add typed texture/sampler owners,
checked transfers, physical-return hooks, completion, readback, Ready or G1.

## TDD evidence

The new tests were written before production changes. The first build failed
for the intended missing API (`add_texture_update`/`add_texture_clear_color`
still returned `void`, and `has_dependency_path` was absent). A first red run
also exposed a test-only `VectorView<ResourceTracker>` spelling error; after
fixing that test setup, `RED_BUILD_R2.rc=2` contains only the expected missing
production API errors. The red logs are retained in this directory.

The minimal implementation then:

- returns the existing `{graph_instance, tracking_frame, kind=1, command_index,
  offset=0}` handle from texture clear/update;
- accepts those two recorded command kinds in `contains_record`;
- traverses only the graph's existing validated adjacency lists with a bounded
  BFS, rejecting foreign/stale handles and malformed/backward edges while
  reporting the actual positive path length.

## Verification

- Source baseline: `6a300cedfbf0cc678148bb76c85c47b937f11409`.
- `tests=yes` macOS arm64 editor build, builtin glslang: `GREEN_BUILD.rc=0`.
- Graph focused run: `GRAPH_RUN.rc=0`, 8 cases / 109 assertions.
- Native regression `*device_exit*`: rc0, 133 cases / 2,619 assertions.
- Native regression `[K03][P2]*`: rc0, 22 cases / 350 assertions.
- Current editor SHA256: `0fa27ca15538200a127ff4350cf3cbb630be79d347bc5b7614d110778268456a`.
- `git diff --check` on the three changed source/test files: rc0.

The independent release target probe was attempted with
`target=template_release dev_build=no tests=no builtin_glslang=yes`, but
stopped before compilation at the existing identity parser with
`ValueError: unclosed original native linker operand: -S` (`RELEASE_BUILD.rc=2`).
This is retained as a separate unsupported-linker branch and does not alter the
green debug graph result. UBSan, GPU execution, native completion/readback and
Ready/G1 were not inferred and are not claimed here.
