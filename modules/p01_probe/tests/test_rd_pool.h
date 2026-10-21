#pragma once
#include "modules/p01_probe/p01_rd_pool.h"
#include "tests/test_macros.h"

namespace TestP01RDPool {
TEST_CASE("[P01] Main RD pool rejects absent device without admitting recordings") {
	// doctest uses the dummy renderer. This exercises rejection only, never GPU.
	REQUIRE(RenderingDevice::get_singleton() == nullptr);
	P01RDPool pool;
	CHECK(pool.initialize(false) == ERR_UNAVAILABLE);
	CHECK(pool.initialize(true) == ERR_UNAVAILABLE);
	CHECK(pool.record_public(0, P01PublicSnapshot()) == ERR_UNCONFIGURED);
	CHECK(pool.record_native(0, RID(), 0) == ERR_UNCONFIGURED);
	CHECK(pool.release() == OK);
	CHECK(pool.release() == OK);
}
}
