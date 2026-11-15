# TextureRD binding reset regression

Setting `texture_rd_rid` to an empty RID must clear the public native getter as
well as the RenderingServer wrapper and dimensions. The caller still owns the
original RenderingDevice texture.

This scene runs on the actual main RD render thread for Texture2DRD,
Texture2DArrayRD, TextureCubemapRD, TextureCubemapArrayRD and Texture3DRD. Two
bind/clear cycles verify native getters, dimensions, change notifications,
rebinding and caller ownership through clear and wrapper destruction. No main
RD or a 30-second timeout fails the run.

From the repository root after building the editor:

```sh
ANGE_MACOS_BACKGROUND_RENDER=1 bin/godot.macos.editor.arm64 --display-driver macos --rendering-method mobile --rendering-driver metal --audio-driver Dummy --path tests/scene/fixtures/texture_rd_unset --script run.gd
```

`RECEIPT.json` records the actual Metal/Apple M3 Pro red/green validation:
95 checks, 10 stale getter failures before the change, zero after it; process
exit 1 before and 0 after. Independent source review found no blocker. This
serial render-thread test does not cover existing cross-thread enqueue timing,
particle rendering or other GPU backends.
