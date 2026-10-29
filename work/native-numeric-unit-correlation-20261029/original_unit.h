#pragma once
#include "particle/gpu/gpu_numeric_fixed_data.inc"
#include "particle/gpu/gpu_numeric_helpers.h"
#include "particle/gpu/gpu_qualification_unit.h"

#include "tests/particle/support/compiled_catalog_fixture.h"
namespace numeric_control {
inline ANGE::particle::Result<ANGE::particle::QualificationUnit> make_unit() {
	using namespace ANGE::particle;
	NumericProfileData n{ "ange.gpu.native_f32_ftz_v1", 1, {}, ConstantPolicy::U02Binary32RNE, StoragePolicy::ExactBits,
		RuntimeFloatPolicy::SignedFTZ, RuntimeFloatPolicy::SignedFTZ, { NativeRounding::RNE, NativeRounding::RTZ },
		ContractionPolicy::ForbiddenAcrossIRNodes, ReassociationPolicy::Forbidden, NonfinitePolicy::Fault,
		OverflowPolicy::RNERangeGuardOnNormalizedOperands, SignedZeroPolicy::ExplicitHelper, ConversionPolicy::ExactChecked,
		{}, 1, { { "U32Binary32RNEThenSignedFTZV1", 1, {} } } };
	auto nr = NumericProfile::create(n);
	if (!nr.has_value()) {
		return Result<QualificationUnit>::error(nr.error());
	}
	GpuCompileLimits limits{ 10000, 128, 16, 1024, 1000000, 10000, 1048576, 4194304, 128, 128, 1048576, 100000, 128 };
	auto profile = GpuCompileProfile::create({ "fixed-numeric-recipe-test", 1, 450, 1, 2, 2, nr.value(), NumericTargetVariant::MetalQualifiedNativeV1, 1, limits, { BackendRequirement{ GpuBackend::Metal } }, GpuTargetFamily::RDCompute, GpuProfileTrust::Production });
	if (!profile.has_value()) {
		return Result<QualificationUnit>::error(profile.error());
	}
	auto recipe = test::find_registered_test_numeric_recipe(profile.value(), 1u << 26);
	if (!recipe.has_value()) {
		return Result<QualificationUnit>::error(recipe.error());
	}
	return build_numeric_qualification_unit(recipe.value(), 0);
}
} //namespace numeric_control
