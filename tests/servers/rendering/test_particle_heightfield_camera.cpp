/**************************************************************************/
/*  test_particle_heightfield_camera.cpp                                  */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "servers/rendering/renderer_rd/renderer_scene_render_rd.h"
#include "tests/test_macros.h"

#include <limits>

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

TEST_CASE("[ParticleHeightfieldCamera] preflight rejects invalid constants without native allocation") {
	// This same preflight runs before framebuffer acquisition in the renderer.
	// It needs no RenderingDevice or ParticlesStorage singleton.
	const real_t infinity = std::numeric_limits<real_t>::infinity();
	const real_t nan = std::numeric_limits<real_t>::quiet_NaN();
	const Transform3D sentinel(Basis(), Vector3(17, 19, 23));
	Transform3D camera = sentinel;
	Projection projection;
	projection.set_orthogonal(-1, 1, -1, 1, 0, 2);
	const Projection original_projection = projection;
	for (const Vector3 &extents : { Vector3(0, 1, 1), Vector3(1, -1, 1), Vector3(1, 1, 0), Vector3(nan, 1, 1), Vector3(1, infinity, 1), Vector3(1, std::numeric_limits<real_t>::max(), 1) }) {
		CHECK_FALSE(RendererSceneRenderRD::heightfield_capture_preflight(Transform3D(), extents, camera, projection));
		CHECK(camera == sentinel);
		CHECK(projection == original_projection);
	}
	Transform3D field;
	field.origin.x = infinity;
	CHECK_FALSE(RendererSceneRenderRD::heightfield_capture_preflight(field, Vector3(1, 1, 1), camera, projection));
	field = Transform3D();
	field.basis.set_column(0, Vector3());
	CHECK_FALSE(RendererSceneRenderRD::heightfield_capture_preflight(field, Vector3(1, 1, 1), camera, projection));
	field = Transform3D();
	field.basis.set_column(1, Vector3(0, nan, 0));
	CHECK_FALSE(RendererSceneRenderRD::heightfield_capture_preflight(field, Vector3(1, 1, 1), camera, projection));
	CHECK(camera == sentinel);
	CHECK(projection == original_projection);
}

TEST_CASE("[ParticleHeightfieldCamera] preflight preserves rotated nonuniform capture") {
	Basis basis(Vector3(0, 0, 1), 1.5707963267948966);
	basis.scale_local(Vector3(2, 3, 4));
	const Transform3D field(basis, Vector3(10, 20, 30));
	Transform3D camera;
	Projection projection;
	REQUIRE(RendererSceneRenderRD::heightfield_capture_preflight(field, Vector3(1, 2, 3), camera, projection));
	CHECK(camera.origin.is_equal_approx(Vector3(4, 20, 30)));
	CHECK(camera.basis.get_column(Vector3::AXIS_Z).is_equal_approx(Vector3(-1, 0, 0)));
	CHECK(camera.is_finite());
	CHECK(camera.affine_inverse().is_finite());
	CHECK(projection.get_z_far() == doctest::Approx(12.0));
	const Projection inverse = projection.inverse();
	for (int column = 0; column < 4; ++column) {
		CHECK(projection[column].is_finite());
		CHECK(inverse[column].is_finite());
	}
}
