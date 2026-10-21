#pragma once
#include "modules/p01_probe/p01_capture_writer.h"
#include "tests/test_macros.h"
#ifdef UNIX_ENABLED
#include <unistd.h>
TEST_CASE("[P01] Inherited writer emits original bytes and closes pipe without render work") {
	int handles[2];
	REQUIRE(pipe(handles) == 0);
	P01CaptureWriter writer;
	REQUIRE(writer.start(handles[1]));
	CHECK(writer.publish("{\"kind\":\"start\"}\n"));
	CHECK(writer.publish("{\"kind\":\"seal\"}\n"));
	writer.close();
	CHECK_FALSE(writer.failed());
	char bytes[256];
	const auto size = read(handles[0], bytes, sizeof(bytes));
	REQUIRE(size > 0);
	CHECK(std::string(bytes, size) == "{\"kind\":\"start\"}\n{\"kind\":\"seal\"}\n");
	CHECK(read(handles[0], bytes, sizeof(bytes)) == 0);
	CHECK_FALSE(writer.publish("{}\n"));
	::close(handles[0]);
}
TEST_CASE("[P01] Writer rejects nonpipe and broken receiver without SIGPIPE termination") {
	P01CaptureWriter missing;
	CHECK_FALSE(missing.start(-1));
	int handles[2]; REQUIRE(pipe(handles) == 0);
	::close(handles[0]);
	P01CaptureWriter writer;
	REQUIRE(writer.start(handles[1]));
	CHECK(writer.publish("{}\n"));
	writer.close();
	CHECK(writer.failed());
}
#endif
