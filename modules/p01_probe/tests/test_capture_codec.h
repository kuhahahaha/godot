#pragma once
#include "modules/p01_probe/p01_capture_codec.h"
#include "tests/test_macros.h"
TEST_CASE("[P01] Native codec fixes identities origin and lossless event integers") {
	P01CaptureCodec codec;
	P01CaptureIdentity identity { "run", "S02.N", std::string(64, 'a'), "build" };
	std::string line;
	CHECK(codec.start(identity, line));
	CHECK(line.find("\"frame_seq\":\"0\"") != std::string::npos);
	P01HookEvent event;
	event.kind = 11; event.epoch = 2; event.sequence = 1; event.rid = UINT64_MAX;
	CHECK(codec.event(event, "a01", line));
	CHECK(line.find("18446744073709551615") != std::string::npos);
	CHECK(line.find("\"hook\":\"skeleton_upload\"") != std::string::npos);
	CHECK(line.find("\"origin\":\"engine\"") != std::string::npos);
	CHECK(codec.seal(line));
	CHECK_FALSE(codec.event(event, "a02", line));
}
TEST_CASE("[P01] Native codec rejects unknown hooks and noncanonical launch identity") {
	P01CaptureCodec codec;
	std::string line;
	CHECK_FALSE(codec.start({"run", "S02.N", "bad", "build"}, line));
	CHECK(codec.start({"run", "S02.N", std::string(64, 'a'), "build"}, line));
	P01HookEvent event; event.kind = 255; event.epoch = 1; event.sequence = 1;
	CHECK_FALSE(codec.event(event, "a01", line));
	CHECK_FALSE(codec.seal(line));
}

TEST_CASE("[P01] Native raw64 blob preserves fixed bytes and exact digest") {
	P01CaptureCodec codec;
	std::string line;
	REQUIRE(codec.start({"run", "S02.N", std::string(64, 'a'), "build"}, line));
	uint8_t bytes[64] = {};
	CHECK(codec.blob64("snapshot-0", bytes, sizeof(bytes), line));
	CHECK(line.find("\"declared_total\":\"64\"") != std::string::npos);
	CHECK(line.find("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA==") != std::string::npos);
	CHECK(codec.seal(line));
	CHECK(line.find("f5a5fd42d16a20302798ef6ed309979b43003d2320d9f0e8ea9831a92759fb4b") != std::string::npos);
}
