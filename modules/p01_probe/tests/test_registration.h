#pragma once

// Included by modules_tests.gen.h only when this tests-only module is enabled.
// Keep the fixture test object in the module archive reachable by the linker.
namespace ForceLink {
void force_link_test_p01_probe_fixture();
}

namespace TestP01ProbeRegistration {
static const bool fixture_linked = []() {
	ForceLink::force_link_test_p01_probe_fixture();
	return true;
}();
}
