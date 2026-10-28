# E20 root adoption review — 2026-10-29

This is a current-source review of the frozen r2 typed texture/sampler
candidate. It does not install the candidate ABI or claim a native texture,
sampler, transfer, completion, readback, Ready, or G1 result.

## Source identity

- Candidate source audit pin: `beb1e2c9b38bce8488f173486efad9dd5e38faa3`.
- Current Godot tree after the independent macOS external-glslang change:
  `5700c0fe5757584d5d60b2ce4e66ea4313ad9fcd`.
- The candidate is therefore a design/implementation input, not a patch that
  can be applied by commit identity.

## Current anchor census

The current tree still has the V1 qualification and rendering seams named by
the candidate: `Registry::qualification_storage_create`,
`qualification_storage_retire`, `RenderingDevice::_qualification_storage_create`,
`RenderingDevice::_qualification_freed`, `sampler_owner`,
`RenderingDevice::texture_create`, `RenderingDeviceGraph::add_texture_update`,
and `add_texture_clear_color`.

The current tree does **not** have any of the candidate's typed production
symbols or fields: `qualification_storage_create_v2`,
`storage_backing_v2`, `has_dependency_path`, typed `Texture`/`Sampler`
qualification return metadata, or the checked texture transfer/record branch.
The graph methods still return `void`, and `sampler_owner` still stores raw
`RDD::SamplerID`. This confirms that the candidate remains uninstalled.

## Decision

Accept the r2 package as the implementation contract for a future owner-owned
Godot/ANGE integration pass, subject to rebasing all hunk locations and
revalidating the exact current source before edits. Keep V1 ABI behavior
unchanged. Production implementation remains open because the complete chain
requires the same-bridge Storage child, existing RT transport, B/C checked
upload and record admission, actual buffer-to-image transfer, and original
physical-return hooks. No partial ABI or local texture/sampler registry should
be introduced to make one side appear complete.

The ABI layout evidence remains bounded to the candidate's four syntax-only
runs (C11/C++17 on arm64/x86_64, all rc0). It is not a production build or
runtime acceptance.
