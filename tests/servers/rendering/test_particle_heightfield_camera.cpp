#include "servers/rendering/renderer_rd/renderer_scene_render_rd.h"
#include "tests/test_macros.h"

TEST_FORCE_LINK(test_particle_heightfield_camera)

TEST_CASE("[ParticleHeightfieldCamera] height capture camera follows field local Y") {
	const Transform3D rotated(Basis(Vector3(0, 0, 1), 1.5707963267948966), Vector3(10, 20, 30));
	const Transform3D camera = RendererSceneRenderRD::heightfield_capture_camera(rotated, 3.0);
	CHECK(camera.origin.is_equal_approx(Vector3(7, 20, 30)));
	CHECK(camera.basis.get_column(Vector3::AXIS_Z).is_equal_approx(Vector3(-1, 0, 0)));
}

TEST_CASE("[ParticleHeightfieldCamera] height capture camera keeps identity field centered") {
	const Transform3D field(Basis(), Vector3(10, 20, 30));
	const Transform3D camera = RendererSceneRenderRD::heightfield_capture_camera(field, 3.0);
	CHECK(camera.origin.is_equal_approx(Vector3(10, 23, 30)));
	CHECK(camera.basis.get_column(Vector3::AXIS_Z).is_equal_approx(Vector3(0, 1, 0)));
}

TEST_CASE("[ParticleHeightfieldCamera] scaled field applies half height once") {
	Basis basis(Vector3(0, 0, 1), 1.5707963267948966);
	basis.scale(Vector3(2, 2, 2));
	const Transform3D field(basis, Vector3(10, 20, 30));
	const Transform3D camera = RendererSceneRenderRD::heightfield_capture_camera(field, 6.0);
	CHECK(camera.origin.is_equal_approx(Vector3(4, 20, 30)));
	CHECK(camera.basis.get_column(Vector3::AXIS_Z).is_equal_approx(Vector3(-1, 0, 0)));
}
