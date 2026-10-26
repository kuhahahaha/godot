# Default ANGE DSO fixed-pair initialization negative

This receipt freezes the latest fixed probe of the default ANGE DSO. It is an initialization failure, not a qualification result.

- Godot editor source host: `309e8c41366b93ad4b6554ff3f1722663e840e3c05fe9212b12ea762bb318ae4`.
- ANGE default `template_debug.arm64.dylib` SHA256: `af894638c9d532caef5b853c1717cfee1f07cd5facd685c2d0e14d2d41325071`.
- Non-headless Metal/mobile fixed probe: Apple M3 Pro.
- Result: rc79.
- First error: `GDEXTENSION_LEVEL_DISPATCH_FAILED error=002 level=2 phase=I`.

The failure occurs during initialization dispatch before the VFX/U01 world probe and before any qualification request, operation association, completion, readback, or Ready claim. It therefore cannot be interpreted as a failed GPU qualification or as evidence for a fallback. The probe is retained as a fail-closed negative.

This is distinct from the later matching dev-profile diagnosis, which reached the same initialization boundary at `SceneTree::get_singleton()==null`; that path also did not authorize bypassing the host lifecycle. No default flag, synthetic SceneTree/RenderingServer, or initialization-order relaxation was added.
