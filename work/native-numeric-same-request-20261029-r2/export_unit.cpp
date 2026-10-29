#include "particle/gpu/gpu_numeric_fixed_data.inc"
#include "particle/gpu/gpu_numeric_helpers.h"
#include "particle/gpu/gpu_qualification_unit.h"

#include "tests/particle/support/compiled_catalog_fixture.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
using namespace ANGE::particle;
static void check(bool x, const char *where) {
	if (!x) {
		std::cerr << where << "\n";
		std::exit(42);
	}
}
static std::string hex(const SchemaDigest &d) {
	std::ostringstream s;
	for (auto x : d.bytes) {
		s << std::hex << std::setfill('0') << std::setw(2) << unsigned(x);
	}
	return s.str();
}
static void write(const std::filesystem::path &path, const void *data, size_t size) {
	std::ofstream f(path, std::ios::binary);
	f.write(static_cast<const char *>(data), size);
	check(bool(f), "write");
}
static void word(std::vector<uint8_t> &bytes, size_t index, uint32_t value) {
	for (unsigned i = 0; i < 4; ++i) {
		bytes[4 * index + i] = uint8_t(value >> (8 * i));
	}
}
int main(int argc, char **argv) {
	check(argc == 2, "output directory");
	std::filesystem::path out = argv[1];
	std::filesystem::create_directories(out);
	// Exact original static-test profile; this bounded native-control profile
	// is not U08 production authorization or an E06 Verified result.
	NumericProfileData n{ "ange.gpu.native_f32_ftz_v1", 1, {}, ConstantPolicy::U02Binary32RNE, StoragePolicy::ExactBits,
		RuntimeFloatPolicy::SignedFTZ, RuntimeFloatPolicy::SignedFTZ, { NativeRounding::RNE, NativeRounding::RTZ },
		ContractionPolicy::ForbiddenAcrossIRNodes, ReassociationPolicy::Forbidden, NonfinitePolicy::Fault,
		OverflowPolicy::RNERangeGuardOnNormalizedOperands, SignedZeroPolicy::ExplicitHelper, ConversionPolicy::ExactChecked,
		{}, 1, { { "U32Binary32RNEThenSignedFTZV1", 1, {} } } };
	auto nr = NumericProfile::create(n);
	check(nr.has_value(), "numeric profile");
	GpuCompileLimits limits{ 10000, 128, 16, 1024, 1000000, 10000, 1048576, 4194304, 128, 128, 1048576, 100000, 128 };
	auto profile = GpuCompileProfile::create({ "fixed-numeric-recipe-test", 1, 450, 1, 2, 2, nr.value(), NumericTargetVariant::MetalQualifiedNativeV1, 1, limits, { BackendRequirement{ GpuBackend::Metal } }, GpuTargetFamily::RDCompute, GpuProfileTrust::Production });
	check(profile.has_value(), "profile");
	auto recipe = test::find_registered_test_numeric_recipe(profile.value(), 1u << 26);
	check(recipe.has_value(), "original retained catalog/recipe");
	constexpr uint32_t ordinal = 0;
	auto made = build_numeric_qualification_unit(recipe.value(), ordinal);
	check(made.has_value(), "original complete factory");
	const auto &unit = made.value();
	const auto &occurrence = unit.descriptor();
	check(unit.unit_count() == 2, "capture/body physical pair");
	auto input = fixed_numeric_input(recipe.value(), ordinal), initial = fixed_numeric_output_initial(recipe.value(), ordinal);
	check(input.has_value() && initial.has_value(), "original bytes");
	std::ofstream meta(out / "unit.json");
	meta << "{\n\"recipe\":\"" << recipe.value()->recipe_id() << "\",\"recipe_digest\":\"" << hex(recipe.value()->digest()) << "\",\"unit_digest\":\"" << hex(unit.identity()) << "\",\"profile_digest\":\"" << hex(profile.value()->digest()) << "\",\"required_compile_flags\":" << unit.required_compile_flags() << ",\"occurrence\":" << ordinal << ",\"kind\":" << uint32_t(occurrence.kind) << ",\"operation\":" << uint32_t(occurrence.operation) << ",\"first_sample\":" << occurrence.first_sample << ",\"sample_count\":" << occurrence.count << ",\"readback_bytes\":" << occurrence.observed_bytes << ",\"input_digest\":\"" << hex(occurrence.input_digest) << "\",\"physical\":[";
	std::vector<uint64_t> extents;
	for (uint32_t i = 0; i < unit.unit_count(); ++i) {
		auto v = unit.unit(i);
		check(v.has_value(), "physical unit");
		const auto &view = v.value();
		check(view.expectation->descriptors.size() == 8, "eight original role descriptors");
		write(out / (i ? "body.comp" : "capture.comp"), view.source.data(), view.source.size());
		if (i) {
			meta << ",";
		}
		meta << "{\"index\":" << i << ",\"identity\":\"" << hex(*view.identity) << "\",\"source_digest\":\"" << hex(*view.source_digest) << "\",\"interface_digest\":\"" << hex(view.expectation->digest) << "\",\"fault_map_digest\":\"" << hex(view.expectation->fault_site_map_digest) << "\",\"descriptors\":[";
		for (uint32_t j = 0; j < 8; ++j) {
			const auto &d = view.expectation->descriptors[j];
			check(d.set == 0 && d.binding == j, "original exact set/binding");
			if (!i) {
				extents.push_back(d.byte_extent);
			} else {
				check(extents[j] == d.byte_extent, "same pair extents");
			}
			if (j) {
				meta << ",";
			}
			meta << "{\"set\":" << d.set << ",\"binding\":" << d.binding << ",\"bytes\":" << d.byte_extent << "}";
		}
		meta << "]}";
	}
	meta << "],\"expected_samples\":[";
	for (uint32_t i = 0; i < occurrence.count; ++i) {
		const uint32_t index = occurrence.first_sample + i;
		fixed_numeric_data::Expected e{};
		switch (occurrence.kind) {
			case FixedNumericSampleKind::FloatBoundary:
				e = fixed_numeric_data::floats[index].metal;
				break;
			case FixedNumericSampleKind::IntegerBoundary:
				e = fixed_numeric_data::integers[index].expected;
				break;
			case FixedNumericSampleKind::DivisionCorpus: {
				const auto &d = fixed_numeric_data::division[index];
				e = { d.fault, 1, { d.bits, 0 } };
			} break;
		}
		check(e.count >= 1 && e.count <= 2, "original fixed expected arm");
		if (i) {
			meta << ",";
		}
		meta << "{\"fault\":" << e.fault << ",\"bits\":[" << e.bits[0];
		if (e.count == 2) {
			meta << "," << e.bits[1];
		}
		meta << "]}";
	}
	meta << "]}\n";
	check(bool(meta), "metadata");
	for (uint32_t role = 0; role < 8; ++role) {
		check(extents[role] % 4 == 0, "word extent");
		std::vector<uint8_t> bytes(extents[role]);
		if (role == 0) {
			word(bytes, 0, occurrence.count);
		}
		if (role == 4) {
			for (uint32_t i = 0; i < occurrence.count; ++i) {
				word(bytes, i, i);
			}
		}
		if (role == 6) {
			check(bytes.size() == input.value().size(), "original input extent");
			bytes = input.value();
		}
		if (role == 7) {
			check(bytes.size() == initial.value().size(), "original observed extent");
			bytes = initial.value();
		}
		write(out / ("role" + std::to_string(role) + ".bin"), bytes.data(), bytes.size());
	}
	std::cout << "ORIGINAL_QUALIFICATION_UNIT occurrence=" << ordinal << " samples=" << occurrence.count << " units=" << unit.unit_count() << " unit=" << hex(unit.identity()) << "\n";
}
