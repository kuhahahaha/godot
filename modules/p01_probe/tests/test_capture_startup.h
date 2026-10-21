#pragma once
#include "modules/p01_probe/p01_capture_startup.h"
#include "tests/test_macros.h"
TEST_CASE("[P01] Startup parses exactly one inherited endpoint and parent fixed identity") {
	P01CaptureStartup startup;
	CHECK(p01_parse_capture_startup({}, startup) == P01StartupParse::Absent);
	CHECK(p01_parse_capture_startup({"--p01-capture-fd", "9"}, startup) == P01StartupParse::Invalid);
	std::vector<std::string> args {"--p01-capture-fd", "9", "--p01-run-id", "run", "--p01-child-id", "S02.N", "--p01-manifest-sha256", std::string(64, 'a')};
	CHECK(p01_parse_capture_startup(args, startup) == P01StartupParse::Valid);
	CHECK(startup.descriptor == 9);
	args.push_back("--p01-capture-fd"); args.push_back("9");
	CHECK(p01_parse_capture_startup(args, startup) == P01StartupParse::Invalid);
	CHECK(p01_parse_capture_startup({"--p01-capture-fd=9"}, startup) == P01StartupParse::Invalid);
}
