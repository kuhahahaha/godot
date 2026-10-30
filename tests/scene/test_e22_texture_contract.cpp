/**************************************************************************/
/*  test_e22_texture_contract.cpp                                         */
/**************************************************************************/

#include "core/os/memory_allocation_test_hooks.h"
#include "scene/resources/curve_texture.h"
#include "scene/resources/resource_snapshot_native.h"
#include "tests/test_macros.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <thread>

TEST_FORCE_LINK(test_e22_texture_contract)

namespace TestE22TextureContract {

constexpr uint32_t WIDTH = 32;
constexpr size_t PIXEL_BYTES = WIDTH * 16;
constexpr uint8_t POISON = 0xa5;

struct Fixture {
	Ref<Curve> curve;
	Ref<CurveTexture> texture;
	AngeTextureSnapshotPlanV1 plan = {};
	alignas(8) std::array<uint8_t, PIXEL_BYTES + 1> output;
	std::array<real_t, 1000> scratch;
	AngeTextureSnapshotResultV1 result;
	bool ready = false;

	Fixture() {
		curve.instantiate();
		curve->add_point(Vector2(0, 0));
		curve->add_point(Vector2(1, 1));
		curve->set_bake_resolution(8);
		texture.instantiate();
		texture->set_width(WIDTH);
		texture->set_texture_mode(CurveTexture::TEXTURE_MODE_RED);
		texture->set_curve(curve);
		ready = replan() == ANGE_TEXTURE_OK_V1;
		poison();
	}
	uint32_t replan() {
		return ange_texture_snapshot_plan_v1(texture.ptr(), ANGE_TEXTURE_FORMAT_RGBAF_V1, &plan);
	}
	void poison() {
		output.fill(POISON);
		scratch.fill(real_t(12345));
		std::memset(&result, POISON, sizeof(result));
	}
	uint32_t capture(uint64_t p_output_capacity = PIXEL_BYTES, uint64_t p_scratch_capacity = sizeof(scratch)) {
		return ange_texture_snapshot_v1(texture.ptr(), &plan, output.data(), p_output_capacity, scratch.data(), p_scratch_capacity, &result);
	}
};

void check_zero_result(const AngeTextureSnapshotResultV1 &p_result, uint32_t p_status) {
	const AngeTextureSourceTupleV1 zero = {};
	CHECK(std::memcmp(&p_result.source, &zero, sizeof(zero)) == 0);
	CHECK(p_result.initialized_pixels == 0);
	CHECK(p_result.status == p_status);
}

TEST_CASE("[E22TextureSnapshot][Contract] failed plan and semantic failures zero the records") {
	Fixture f;
	REQUIRE(f.ready);
	AngeTextureSnapshotPlanV1 failed;
	std::memset(&failed, POISON, sizeof(failed));
	CHECK(ange_texture_snapshot_plan_v1(f.texture.ptr(), ANGE_TEXTURE_FORMAT_RGBA8_V1, &failed) == ANGE_TEXTURE_UNSUPPORTED_V1);
	const AngeTextureSnapshotPlanV1 zero = {};
	CHECK(std::memcmp(&failed, &zero, sizeof(zero)) == 0);

	const auto scratch_before = f.scratch;
	CHECK(f.capture(PIXEL_BYTES - 1) == ANGE_TEXTURE_LIMIT_V1);
	check_zero_result(f.result, ANGE_TEXTURE_LIMIT_V1);
	for (uint8_t b : f.output) {
		CHECK(b == POISON);
	}
	CHECK(f.scratch == scratch_before);

	f.curve->set_bake_resolution(2);
	CHECK(f.capture() == ANGE_TEXTURE_STALE_SOURCE_V1);
	check_zero_result(f.result, ANGE_TEXTURE_STALE_SOURCE_V1);
	for (uint8_t b : f.output) {
		CHECK(b == POISON);
	}
	CHECK(f.scratch == scratch_before);

	REQUIRE(f.replan() == ANGE_TEXTURE_OK_V1);
	f.plan.real_bytes = sizeof(real_t) == 4 ? 8 : 4;
	CHECK(f.capture() == ANGE_TEXTURE_BAD_ARGUMENT_V1);
	check_zero_result(f.result, ANGE_TEXTURE_BAD_ARGUMENT_V1);
	for (uint8_t b : f.output) {
		CHECK(b == POISON);
	}
	CHECK(f.scratch == scratch_before);
}

TEST_CASE("[E22TextureSnapshot][Contract] final byte output accepts unaligned address") {
	Fixture f;
	REQUIRE(f.ready);
	REQUIRE(ange_texture_snapshot_v1(f.texture.ptr(), &f.plan, f.output.data() + 1, PIXEL_BYTES, f.scratch.data(), sizeof(f.scratch), &f.result) == ANGE_TEXTURE_OK_V1);
	CHECK(f.result.status == ANGE_TEXTURE_OK_V1);
	CHECK(f.result.initialized_pixels == WIDTH);
	CHECK(f.output[0] == POISON);
	for (uint32_t i = 0; i < WIDTH; ++i) {
		const float expected[] = { float(f.curve->sample_baked(float(i) / float(WIDTH))), 0, 0, 1 };
		CHECK(std::memcmp(f.output.data() + 1 + i * 16, expected, sizeof(expected)) == 0);
	}
}

TEST_CASE("[E22TextureSnapshot][Contract] structurally invalid spans touch no supplied bytes") {
	Fixture f;
	REQUIRE(f.ready);
	const auto original_output = f.output;
	const auto original_scratch = f.scratch;
	const auto original_result = f.result;
	const auto original_plan = f.plan;
	CHECK(ange_texture_snapshot_v1(f.texture.ptr(), &f.plan, f.output.data(), PIXEL_BYTES, f.scratch.data(), sizeof(f.scratch), nullptr) == ANGE_TEXTURE_BAD_ARGUMENT_V1);
	CHECK(ange_texture_snapshot_v1(f.texture.ptr(), &f.plan, f.output.data(), PIXEL_BYTES, f.scratch.data(), sizeof(f.scratch), reinterpret_cast<AngeTextureSnapshotResultV1 *>(&f.plan)) == ANGE_TEXTURE_BAD_ARGUMENT_V1);
	CHECK(ange_texture_snapshot_v1(f.texture.ptr(), &f.plan, f.output.data(), PIXEL_BYTES, f.scratch.data(), sizeof(f.scratch), reinterpret_cast<AngeTextureSnapshotResultV1 *>(f.output.data())) == ANGE_TEXTURE_BAD_ARGUMENT_V1);
	CHECK(ange_texture_snapshot_v1(f.texture.ptr(), &f.plan, f.output.data(), PIXEL_BYTES, f.scratch.data(), sizeof(f.scratch), reinterpret_cast<AngeTextureSnapshotResultV1 *>(reinterpret_cast<uint8_t *>(&f.result) + 1)) == ANGE_TEXTURE_BAD_ARGUMENT_V1);
	CHECK(ange_texture_snapshot_v1(f.texture.ptr(), &f.plan, f.output.data(), UINT64_MAX, f.scratch.data(), sizeof(f.scratch), &f.result) == ANGE_TEXTURE_BAD_ARGUMENT_V1);
	CHECK(ange_texture_snapshot_v1(f.texture.ptr(), &f.plan, f.output.data(), PIXEL_BYTES, reinterpret_cast<uint8_t *>(f.scratch.data()) + 1, sizeof(f.scratch) - 1, &f.result) == ANGE_TEXTURE_BAD_ARGUMENT_V1);
	CHECK(ange_texture_snapshot_v1(f.texture.ptr(), &f.plan, f.output.data(), PIXEL_BYTES, f.scratch.data(), sizeof(f.scratch), reinterpret_cast<AngeTextureSnapshotResultV1 *>(f.scratch.data())) == ANGE_TEXTURE_BAD_ARGUMENT_V1);
	CHECK(f.output == original_output);
	CHECK(f.scratch == original_scratch);
	CHECK(std::memcmp(&f.result, &original_result, sizeof(f.result)) == 0);
	CHECK(std::memcmp(&f.plan, &original_plan, sizeof(f.plan)) == 0);

	CHECK(ange_texture_snapshot_plan_v1(f.texture.ptr(), ANGE_TEXTURE_FORMAT_RGBAF_V1, nullptr) == ANGE_TEXTURE_BAD_ARGUMENT_V1);
	CHECK(ange_texture_snapshot_plan_v1(f.texture.ptr(), ANGE_TEXTURE_FORMAT_RGBAF_V1, reinterpret_cast<AngeTextureSnapshotPlanV1 *>(reinterpret_cast<uint8_t *>(&f.plan) + 1)) == ANGE_TEXTURE_BAD_ARGUMENT_V1);
	CHECK(std::memcmp(&f.plan, &original_plan, sizeof(f.plan)) == 0);
}

TEST_CASE("[E22TextureSnapshot][Numeric] invalid domain and floor bounds fail before output") {
	for (int branch = 0; branch < 4; ++branch) {
		CAPTURE(branch);
		Fixture f;
		REQUIRE(f.ready);
		Array limits = f.curve->get_limits();
		if (branch == 0) {
			limits[3] = limits[2];
		} else if (branch == 1) {
			limits[2] = std::numeric_limits<real_t>::quiet_NaN();
		} else if (branch == 2) {
			limits[2] = -std::numeric_limits<real_t>::max();
			limits[3] = std::numeric_limits<real_t>::max();
		} else {
			limits[2] = real_t(1.0e12);
			limits[3] = real_t(1.0e12) + real_t(1.0e5);
			f.curve->set_bake_resolution(1000);
		}
		f.curve->set_limits(limits);
		REQUIRE(f.replan() == ANGE_TEXTURE_OK_V1);
		f.poison();
		CHECK(f.capture() == ANGE_TEXTURE_INVALID_SOURCE_V1);
		check_zero_result(f.result, ANGE_TEXTURE_INVALID_SOURCE_V1);
		for (uint8_t b : f.output) {
			CHECK(b == POISON);
		}
	}
}

TEST_CASE("[E22TextureSnapshot][Contract] wrong thread leaves output untouched and allocates nothing") {
	Fixture f;
	REQUIRE(f.ready);
	const auto scratch_before = f.scratch;
	uint32_t status = ANGE_TEXTURE_OK_V1;
	uint64_t allocations = UINT64_MAX;
	std::thread worker([&] {
		MemoryAllocationTest::attempts = 0;
		MemoryAllocationTest::enabled = true;
		status = f.capture();
		MemoryAllocationTest::enabled = false;
		allocations = MemoryAllocationTest::attempts;
	});
	worker.join();
	CHECK(status == ANGE_TEXTURE_WRONG_THREAD_V1);
	CHECK(allocations == 0);
	check_zero_result(f.result, ANGE_TEXTURE_WRONG_THREAD_V1);
	for (uint8_t b : f.output) {
		CHECK(b == POISON);
	}
	CHECK(f.scratch == scratch_before);
}

TEST_CASE("[E22TextureSnapshot][Numeric] late nonfinite sample preserves prefix but zeros result") {
	Fixture f;
	REQUIRE(f.ready);
	// Three finite bake values: 0, +large, -large. In float builds the
	// final interpolation's subtraction overflows; in double builds values
	// are finite real_t but later samples cannot narrow to finite float.
	const double beyond_float = double(std::numeric_limits<float>::max()) * 4.0;
	const real_t large = sizeof(real_t) == sizeof(float) ? real_t(std::numeric_limits<float>::max()) * real_t(0.75) : static_cast<real_t>(beyond_float);
	Array data;
	for (const Vector2 &point : { Vector2(real_t(0), real_t(0)), Vector2(real_t(0.5), large), Vector2(real_t(1), -large) }) {
		data.push_back(point);
		data.push_back(0.0);
		data.push_back(0.0);
		data.push_back(int(Curve::TANGENT_FREE));
		data.push_back(int(Curve::TANGENT_FREE));
	}
	f.curve->set_block_signals(true);
	f.curve->set_data(data);
	f.curve->set_bake_resolution(3);
	REQUIRE(f.curve->get_point_count() == 3);
	REQUIRE(f.replan() == ANGE_TEXTURE_OK_V1);
	f.poison();
	const uint32_t status = f.capture();
	CHECK(status == ANGE_TEXTURE_INVALID_SOURCE_V1);
	check_zero_result(f.result, ANGE_TEXTURE_INVALID_SOURCE_V1);
	CHECK(std::isfinite(f.scratch[0]));
	CHECK(std::isfinite(f.scratch[1]));
	CHECK(std::isfinite(f.scratch[2]));
	const float first_pixel[] = { 0, 0, 0, 1 };
	CHECK(std::memcmp(f.output.data(), first_pixel, sizeof(first_pixel)) == 0);
	// At least one later pixel stays poisoned, demonstrating a partial prefix.
	bool untouched_pixel = false;
	for (uint32_t i = 1; i < WIDTH; ++i) {
		bool all_poison = true;
		for (int j = 0; j < 16; ++j) {
			all_poison &= f.output[i * 16 + j] == POISON;
		}
		untouched_pixel |= all_poison;
	}
	CHECK(untouched_pixel);
}

} // namespace TestE22TextureContract
