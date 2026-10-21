#pragma once
#include "modules/p01_probe/p01_native_source.h"
#include "tests/test_macros.h"
namespace TestP01NativeSource {
TEST_CASE("[P01] Original native owner capture rejects missing RD without publishing metadata") {
	P01RDPool pool;
	P01NativeSourceObservation observation;
	observation.scene = 123;
	CHECK(P01NativeSource::capture_current(RID(), RID(), RID(), pool, 0, observation) == ERR_UNAVAILABLE);
	CHECK(observation.scene == 123);
}
}
