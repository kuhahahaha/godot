# Metal Mobile particle color-ramp synchronization matrix

Date: 2026-11-04. Fixed editor `bin/godot.macos.editor.arm64` SHA-256 `5feadf94534a134a4a1d204dcc16b6fed54839ece6bcb365fa753f03792b33a4`; its controlled-build sidecar SHA-256 `57dbe31a0c966c2ab5f3c79bef760a94749c13a4b0c2a8c7595483543289fad7`.

Four retained visible probes ran with `--rendering-driver metal --rendering-method mobile --fixed-fps 60`. Each kept a white sprite/quad texture, changed a retained `GradientTexture1D` from red to green in place, replaced it with blue, and then changed the detached old gradient back to red. Every process exited 0; Metal 4.0 Forward Mobile was identified; no engine/script error, seal error, or crash appeared. All four produced exact `(red, green, blue)` pixel counts `256,0,0 → 0,256,0 → 0,0,256 → 0,0,256`. Each probe's `replaced_blue.png` and `detached_old_modified.png` hashes match.

| Case | Probe SHA-256 | Raw Metal log SHA-256 | Matching final PNG SHA-256 |
| --- | --- | --- | --- |
| GPUParticles2D | `4f8132d29a81feef1805109a4e4e99c55bdfa9323f246a87c00b89e9db1ce68b` | `69b78596ce8e74070dc8dc47d385785a45f6cf8471b53ed89e71a21a3b490c1d` | `5cab4a7c147044f37cc277db82722cb96ae83ce78a58ce1de464638ae8022915` |
| GPUParticles3D | `96fee276f0a3725e2bdcb38959114e7f6a1e0951dc0e226f85b1b1424d80aad2` | `29480da056ad253f41f0988d8ef37be2f9bf3491146ec5848b57bb7ed7e7c773` | `384df9687ea9b408f29cafb07a54284836b7dff2f516f23b23d1847fd42b4cc6` |
| CPUParticles2D | `e5ba9cf8be74941c681c14e84bfd32161e66fcb50737d382587341e3eb120ba5` | `e824ad19dfddc61a76835e7f99dfc1fc12cf5b7514780a7f5f8473133d999576` | `5cab4a7c147044f37cc277db82722cb96ae83ce78a58ce1de464638ae8022915` |
| CPUParticles3D | `634f9682dd5ad055f84aa4cd9b728ccbe9ff9ea3abfd74c067c69ad39e7d80e8` | `9fe54bf12ec24cceec2a3f616e0880093c560a50901e0401695a10193bcc4664` | `384df9687ea9b408f29cafb07a54284836b7dff2f516f23b23d1847fd42b4cc6` |

Command form for each case: `bin/godot.macos.editor.arm64 --path work/<case-directory> --rendering-driver metal --rendering-method mobile --fixed-fps 60 --script res://probe.gd`. Raw logs are `run-metal.log` beside each probe. The original Compatibility and Vulkan Mobile results remain in each case's own `RECEIPT.md` and `result.json`.

This proves visible color-ramp mutation and detached-resource isolation for these four particle nodes on this Metal host. It does not cover other ramp slots, arbitrary particle shaders, other platforms, or product ticket correlation. No production source changed.
