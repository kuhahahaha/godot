# macOS `template_release` packed static host receipt

Date: 2026-10-29

The existing `template_release` static aggregate was rerun with the regenerated minimal probe pack already recorded under `static-template-debug-probe/static_template_debug_probe.pck` (SHA256 `1cdad1929fc882c2044a21b5049d5af3b9bea2a5acdb6e5235018bf8826c0aff`).

- host: `static-template-host`
- host run rc: `0`
- output: exact schema1 identity (`core_stamp=902697c32cf22db0ba4083a2bb877aa6f9b5c0480ade9e9a2ed39eedc3fb9070`, `compiler_stamp=71aa3f9079901802e2d9a98c125d9d427d7f6838585e83354eaf9692eba1836b`, `BuiltinGlslangV1`, `StaticLibrary` target) and `STATIC_TEMPLATE_HOST_DESTROY_RETURNED`
- raw log: `STATIC_TEMPLATE_RELEASE_HOST_PACKED_R1.log`
- rc file: `STATIC_TEMPLATE_RELEASE_HOST_PACKED_R1.rc`

This supersedes the earlier no-pack host-unavailable observation for the bounded minimal project-pack host. It does not claim a production project pack, GPU execution, ANGE owner association, completion/readback, Ready, or G1.
