# CPUParticles2D ring property order, 2026-11-03

The paired ring setter contract introduced in `c86e683be3` requires the outer radius to be assigned before a larger inner radius. `CPUParticles2D::_bind_methods` still registered the inner property first. This follow-up registers the outer property first, matching the existing 3D and material property order and the CPU/GPU conversion assignment order.

The real SceneTree `ring_bounds.gd` control now packs and instantiates a 2D ring node with outer radius 5 and inner radius 4, and compares the restored values. The first exploratory control omitted `emission_shape=RING`, so PackedScene saved no radius properties; that failure is excluded as invalid evidence. After adding the ring shape and rebuilding with `scons platform=macos target=editor tests=yes -j4` (rc 0), the control exits 0 with `CPU_RING_BOUNDS={"failures":[]}`. This confirms the current registered property order and scene roundtrip; it does not claim an independently observed red on a valid prechange editor. The ordinary invalid-pair rejection and CPU/GPU conversion controls remain in the same fixture.

This is a focused serialization ordering correction. It does not establish GPU pixel output or ANGE product-request acceptance.
