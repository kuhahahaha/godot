/**************************************************************************/
/*  test_texture_snapshot.cpp                                            */
/**************************************************************************/

#include "tests/test_macros.h"

TEST_FORCE_LINK(test_texture_snapshot)

#include "scene/resources/curve.h"

namespace TestTextureSnapshot {

TEST_CASE("[E22TextureSnapshot][Curve] empty bake initializes every value") {
	Ref<Curve> curve = memnew(Curve);
	for (int resolution : { 1, 2, 1000 }) {
		curve->set_bake_resolution(resolution);
		curve->bake();
		for (int i = 0; i < resolution; ++i) {
			CHECK(curve->_test_get_baked_value(i) == doctest::Approx(0.0));
		}
	}
}

TEST_CASE("[E22TextureSnapshot][Curve] resolution one retains the last point") {
	Ref<Curve> curve = memnew(Curve);
	curve->set_bake_resolution(1);
	curve->add_point(Vector2(0.0, 0.25));
	curve->add_point(Vector2(1.0, 0.75));
	curve->bake();
	CHECK(curve->_test_get_baked_value(0) == doctest::Approx(0.75));
}

TEST_CASE("[E22TextureSnapshot][Curve] set limits invalidates a warm bake") {
	Ref<Curve> curve = memnew(Curve);
	curve->add_point(Vector2(0.0, 0.0));
	curve->add_point(Vector2(1.0, 1.0));
	curve->set_bake_resolution(8);
	curve->bake();
	Array limits;
	limits.push_back(-1.0);
	limits.push_back(2.0);
	limits.push_back(0.0);
	limits.push_back(2.0);
	curve->set_limits(limits);
	const real_t after = curve->sample_baked(0.5);
	curve->bake();
	CHECK(after == curve->sample_baked(0.5));
}

} // namespace TestTextureSnapshot
