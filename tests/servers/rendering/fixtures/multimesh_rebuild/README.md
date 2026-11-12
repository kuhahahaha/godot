# Ordinary MultiMesh resource regression fixtures

These exercise existing public RenderingServer APIs on the actual main RD, outside active lists. They are K02 resource/API constituents. They do not replace original E34 M01–M12/N03 acceptance or qualify pixels, performance, physical release, allocation/driver-fault injection, full B4, or ExternalOutputStorage.

Use the actual tests-enabled editor binary with this directory as `--path`:

- `--rendering-method mobile --rendering-driver metal --script matrix.gd`: command contents, indirect transitions, same-mesh capacity/layout changes, null/empty and in-place surface changes, and CPU-cache-less automatic AABB refresh. Readback deliberately synchronizes; it does not prove non-stalling production scheduling. No MultiMesh is attached to a drawable scene instance, so the sentinel negative never submits an unsafe indirect draw.
- `--rendering-method mobile --rendering-driver metal --script cached_visible.gd`: a hidden CPU-cache dirty region survives two flushes and must upload when `visible=-1` admits it; explicit capacity is the independent healthy control.
- `--rendering-method gl_compatibility --rendering-driver opengl3 --script gles_visible.gd`: original visible1/all(-1)/zero behavior and invalid-allocation state preservation.
- `--rendering-method forward_plus --rendering-driver metal --script motion.gd`: original native TAA motion requirement, actual current/previous GPU buffer contents, motion enable and same-mesh resize.
- `--rendering-method mobile --rendering-driver metal --script invalid_allocation.gd`, also Compatibility/OpenGL: invalid enum rejects before original interpolation/native owner state mutation. Exactly one native dimensions diagnostic is expected; its result must preserve original capacity. No huge GPU allocation is attempted. The separate doctest checks arithmetic endpoints.

The surface encoder intentionally clamps a nonempty mesh AABB to a 1e-5 minimum size per axis (`RenderingServer::_surface_set_data`); the auto-AABB literals include that native z extent. Clear/remove use two nontrivial 2D instance transforms and separated surfaces, so refreshing before final mesh bounds would fail the exact x bounds.

An absent main RD exits77 for RD fixtures, never a GPU PASS. Full original rendering/physical-return/failure tests remain conjunctive and are not claimed here.
