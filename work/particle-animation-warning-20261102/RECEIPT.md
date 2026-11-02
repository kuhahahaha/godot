# GPUParticles3D animation warning across draw-pass surfaces

Date: 2026-11-02

`GPUParticles3D::get_configuration_warnings()` now accumulates animation-capable materials across all surfaces of each draw-pass mesh. When a material override exists, that override determines whether the animation warning is needed; without an override, the surface result remains intact. The focused native test covers animated and ordinary materials on two surfaces, then changes the override and surface assignments.

At pre-change `HEAD`, the source fixture is RED (2/4); current source is GREEN (4/4). `scons tests=yes platform=macos target=editor -j4` completed rc 0 on the current mixed tree; binary SHA256 `9816569d5bd1e7ba35eed3eb9319f740acafca17731d7519f2843617766c07c0`. The focused native test passed 1 case / 5 assertions; staged diff check passed. The staged patch contains only the warning function, focused test, material include, fixture and this receipt. This is editor warning behavior, so no GPU rendering qualification is inferred from it.
