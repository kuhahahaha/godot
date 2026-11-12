/**************************************************************************/
/*  test_production_host_transform.cpp                                    */
/**************************************************************************/

#include "core/extension/gdextension_production_request.h"
#include "tests/test_macros.h"

#include <cmath>
#include <limits>

TEST_FORCE_LINK(test_production_host_transform)

TEST_CASE("[ProductionRequest][HostTransform] immutable row-major affine preserves opaque token and real inverse") {
	using namespace GDExtensionDeviceExit;
	GDExtensionProductionHostTransformInputV1 input{};
	input.abi = production_dto_abi<GDExtensionProductionHostTransformInputV1>();
	const float affine[12] = { 2, 1, 0, 3, 0, 4, 0, -8, 0, 0, -2, 6 };
	const float inverse[12] = { 0.5f, -0.125f, 0, -2.5f, 0, 0.25f, 0, 2, 0, 0, -0.5f, 3 };
	memcpy(input.affine, affine, sizeof(affine));
	for (uint32_t i = 0; i < 32; ++i) {
		input.host_token.bytes[i] = uint8_t(i * 7);
	}
	GDExtensionProductionHostTransformV1 output{};
	CHECK(production_host_transform(input, output) == OK);
	CHECK(production_dto_valid(output));
	CHECK(memcmp(output.affine, affine, sizeof(affine)) == 0);
	for (uint32_t i = 0; i < 12; ++i) {
		CHECK(output.inverse[i] == inverse[i]);
	}
	CHECK(memcmp(&output.host_token, &input.host_token, sizeof(input.host_token)) == 0);
	// Token data never changes the transform; the Runtime original owner issues it.
	input.host_token.bytes[9] ^= 0xff;
	CHECK(production_host_transform(input, output) == OK);
	for (uint32_t i = 0; i < 12; ++i) {
		CHECK(output.inverse[i] == inverse[i]);
	}
}

TEST_CASE("[ProductionRequest][HostTransform] finite extreme scales retain representable inverses") {
	using namespace GDExtensionDeviceExit;
	for (float scale : { 1e20f, 1e-16f }) {
		GDExtensionProductionHostTransformInputV1 input{};
		input.abi = production_dto_abi<GDExtensionProductionHostTransformInputV1>();
		input.affine[0] = input.affine[5] = input.affine[10] = scale;
		GDExtensionProductionHostTransformV1 output{};
		REQUIRE(production_host_transform(input, output) == OK);
		for (uint32_t row = 0; row < 3; ++row) {
			for (uint32_t column = 0; column < 4; ++column) {
				CHECK(output.inverse[row * 4 + column] == (row == column ? float(1.0 / double(scale)) : 0.0f));
			}
		}
	}
}

TEST_CASE("[ProductionRequest][HostTransform] invalid or unrepresentable inverse leaves caller output untouched") {
	using namespace GDExtensionDeviceExit;
	GDExtensionProductionHostTransformInputV1 input{};
	input.abi = production_dto_abi<GDExtensionProductionHostTransformInputV1>();
	input.affine[0] = input.affine[5] = input.affine[10] = 1;
	GDExtensionProductionHostTransformV1 output;
	memset(&output, 0x5a, sizeof(output));
	const auto sentinel = output;
	SUBCASE("Singular original transform") {
		input.affine[5] = 0;
		CHECK(production_host_transform(input, output) == ERR_INVALID_DATA);
	}
	SUBCASE("Nonfinite source component") {
		input.affine[3] = std::numeric_limits<float>::infinity();
		CHECK(production_host_transform(input, output) == ERR_INVALID_DATA);
	}
	SUBCASE("Inverse overflows F32") {
		input.affine[0] = std::numeric_limits<float>::denorm_min();
		CHECK(production_host_transform(input, output) == ERR_INVALID_DATA);
	}
	SUBCASE("Wrong containing ABI") {
		input.abi.reserved = 1;
		CHECK(production_host_transform(input, output) == ERR_INVALID_PARAMETER);
	}
	CHECK(memcmp(&output, &sentinel, sizeof(output)) == 0);
}

TEST_CASE("[ProductionRequest][HostTransform] exact rank-two cancellation refuses without changing output") {
	using namespace GDExtensionDeviceExit;
	GDExtensionProductionHostTransformInputV1 input{};
	input.abi = production_dto_abi<GDExtensionProductionHostTransformInputV1>();
	// Every integer is F32-exact; the third row is the sum of the first two.
	const float affine[12] = { -1142697, 1257174, 1522286, 0, 981000, -1256992, -1498005, 0, -161697, 182, 24281, 0 };
	memcpy(input.affine, affine, sizeof(affine));
	GDExtensionProductionHostTransformV1 output;
	memset(&output, 0x5a, sizeof(output));
	const auto sentinel = output;
	CHECK(production_host_transform(input, output) == ERR_INVALID_DATA);
	CHECK(memcmp(&output, &sentinel, sizeof(output)) == 0);
}

TEST_CASE("[ProductionRequest][HostTransform] one-ULP nonsingular neighbor retains its finite inverse") {
	using namespace GDExtensionDeviceExit;
	GDExtensionProductionHostTransformInputV1 input{};
	input.abi = production_dto_abi<GDExtensionProductionHostTransformInputV1>();
	const float affine[12] = { -1142697, 1257174, 1522286, 0, 981000, -1256992, -1498005, 0, -161697, 182, 24281, 0 };
	memcpy(input.affine, affine, sizeof(affine));
	input.affine[8] = std::nextafter(input.affine[8], 0.0f);
	// Independently computed with exact rational cofactors/determinant, then
	// rounded to nearest F32 (ties to even). No epsilon classifies singularity.
	const float expected[12] = {
		-0x1.0000000000000p+6f, -0x1.0000000000000p+6f, 0x1.0000000000000p+6f, 0x0.0p+0f,
		0x1.ce19d60000000p+8f, 0x1.ce19d60000000p+8f, -0x1.ce19d80000000p+8f, 0x0.0p+0f,
		-0x1.adaa620000000p+8f, -0x1.adaa620000000p+8f, 0x1.adaa640000000p+8f, 0x0.0p+0f,
	};
	GDExtensionProductionHostTransformV1 output{};
	REQUIRE(production_host_transform(input, output) == OK);
	for (uint32_t i = 0; i < 12; ++i) {
		CHECK(output.inverse[i] == expected[i]);
	}
}

TEST_CASE("[ProductionRequest][HostTransform] inverse translation retains exact cancellation residual") {
	using namespace GDExtensionDeviceExit;
	GDExtensionProductionHostTransformInputV1 input{};
	input.abi = production_dto_abi<GDExtensionProductionHostTransformInputV1>();
	const float affine[12] = { 1, 0x1p30f, 0, 1, 0, 1, 0x1p30f, 0x1p30f, 0, 0, 1, 1 };
	memcpy(input.affine, affine, sizeof(affine));
	const float expected[12] = { 1, -0x1p30f, 0x1p60f, -1, 0, 1, -0x1p30f, 0, 0, 0, 1, -1 };
	GDExtensionProductionHostTransformV1 output{};
	REQUIRE(production_host_transform(input, output) == OK);
	for (uint32_t i = 0; i < 12; ++i) {
		CHECK(output.inverse[i] == expected[i]);
	}
}
