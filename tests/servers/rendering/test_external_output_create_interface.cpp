/**************************************************************************/
/*  test_external_output_owner_state.cpp                                  */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "core/error/error_list.h"
#include "core/extension/gdextension_external_output_interface.h"
#include "core/os/thread.h"
#include "servers/rendering/renderer_rd/storage_rd/particles_storage.h"
#include "servers/rendering/rendering_server.h"
#include "tests/servers/rendering/test_external_output_custody_bridge.h"
#include "tests/test_macros.h"

#include <cstddef>
#include <cstring>
#include <type_traits>

extern GDExtensionInterfaceFunctionPtr gdextension_get_proc_address(const char *);
extern GDExtensionInterfaceFunctionPtr gdextension_get_guarded_proc_address(const char *);

TEST_FORCE_LINK(test_external_output_create_interface)

namespace TestExternalOutputCreateInterface {
using namespace GDExtensionDeviceExit;
static GDExtensionOutputCreateInputV2 input() {
	GDExtensionOutputCreateInputV2 value{};
	value.abi = { 2, sizeof(value), 0 };
	value.config.abi = { 1, sizeof(value.config), 0 };
	return value;
}
static GDExtensionOutputCreateResultV1 sentinel() {
	GDExtensionOutputCreateResultV1 value;
	memset(&value, 0xa5, sizeof(value));
	return value;
}
static void contract(const GDExtensionOutputCreateResultV1 &value) {
	CHECK(value.abi.abi_version == 1);
	CHECK(value.abi.struct_size == sizeof(value));
	CHECK(value.status.error == ERR_INVALID_PARAMETER);
	REQUIRE(value.status.detail_length == 8);
	CHECK(memcmp(value.status.detail, "CONTRACT", 8) == 0);
	CHECK(value.particles == 0);
	CHECK(value.generation == 0);
}
} // namespace TestExternalOutputCreateInterface

TEST_CASE("[B4][OutputCreateWire] V2 rejects outer headers before native receiver") {
	using namespace TestExternalOutputCreateInterface;
	for (uint32_t variant = 0; variant < 5; ++variant) {
		auto value = input();
		switch (variant) {
			case 0:
				value.abi.abi_version = 1;
				break;
			case 1:
				--value.abi.struct_size;
				break;
			case 2:
				value.abi.reserved = 1;
				break;
			case 3:
				value.reserved = 1;
				break;
			case 4:
				value.metadata_resource_slot = UINT32_MAX;
				break;
		}
		auto result = sentinel();
		device_session_output_create_v2({}, {}, &value, &result);
		contract(result);
	}
}

TEST_CASE("[B4][OutputCreateWire] V2 spans cannot borrow outer header selector or each other") {
	using namespace TestExternalOutputCreateInterface;
	for (uint32_t variant = 0; variant < 4; ++variant) {
		auto value = input();
		uint64_t shared = 0;
		switch (variant) {
			case 0:
				value.config.surfaces = { reinterpret_cast<const uint64_t *>(&value), 1, 0 };
				break;
			case 1:
				value.config.surface_materials = { reinterpret_cast<const uint64_t *>(&value.metadata_resource_slot), 1, 0 };
				break;
			case 2:
				value.config.surfaces = { reinterpret_cast<const uint64_t *>(&value.config), 1, 0 };
				break;
			case 3:
				value.config.surfaces = { &shared, 1, 0 };
				value.config.surface_materials = { &shared, 1, 0 };
				break;
		}
		auto result = sentinel();
		device_session_output_create_v2({}, {}, &value, &result);
		contract(result);
	}
}

TEST_CASE("[B4][OutputCreateWire] invalid extents and output aliases preserve borrowed bytes in both versions") {
	using namespace TestExternalOutputCreateInterface;
	for (uint32_t version = 1; version <= 2; ++version) {
		for (uint32_t variant = 0; variant < 4; ++variant) {
			auto value = input();
			auto result = sentinel();
			const auto before = result;
			switch (variant) {
				case 0:
					value.config.surfaces = { nullptr, 1, 0 };
					break;
				case 1:
					value.config.surfaces = { reinterpret_cast<const uint64_t *>(uintptr_t(1)), 1, 0 };
					break;
				case 2:
					value.config.surface_materials = { reinterpret_cast<const uint64_t *>(&result), 1, 0 };
					break;
				case 3:
					value.config.surfaces = { reinterpret_cast<const uint64_t *>(UINTPTR_MAX - 7), 2, 0 };
					break;
			}
			if (version == 1) {
				device_session_output_create_v1({}, {}, &value.config, &result);
			} else {
				device_session_output_create_v2({}, {}, &value, &result);
			}
			CHECK(memcmp(&before, &result, sizeof(result)) == 0);
		}
	}
	auto value = input();
	const auto before = value;
	device_session_output_create_v2({}, {}, &value, reinterpret_cast<GDExtensionOutputCreateResultV1 *>(&value));
	CHECK(memcmp(&before, &value, sizeof(value)) == 0);
}

namespace TestExternalOutputAcquirePollInterface {
using namespace GDExtensionDeviceExit;
using Acquire = void (*)(GDExtensionProductionPhysicalContextRefV1, GDExtensionProductionRequestRefV1, uint64_t, const GDExtensionOutputAcquireInputV1 *, GDExtensionOutputAcquireResultV1 *);
using Poll = void (*)(GDExtensionProductionPhysicalContextRefV1, GDExtensionProductionRequestRefV1, const GDExtensionOutputGenerationInputV1 *, GDExtensionOutputPollResultV1 *);
static_assert(std::is_same_v<Acquire, GDExtensionInterfaceDeviceSessionOutputAcquireV1>);
static_assert(std::is_same_v<Poll, GDExtensionInterfaceDeviceSessionOutputPollV1>);
static_assert(std::is_same_v<Acquire, decltype(&device_session_output_acquire_v1)>);
static_assert(std::is_same_v<Poll, decltype(&device_session_output_poll_v1)>);
static_assert(std::is_standard_layout_v<GDExtensionOutputAcquireInputV1>);
static_assert(std::is_standard_layout_v<GDExtensionOutputGenerationInputV1>);
static_assert(std::is_standard_layout_v<GDExtensionOutputAcquireResultV1>);
static_assert(std::is_standard_layout_v<GDExtensionOutputPollResultV1>);
static_assert(offsetof(GDExtensionOutputAcquireResultV1, abi) == 0);
static_assert(offsetof(GDExtensionOutputPollResultV1, abi) == 0);

// Each boundary is exercised through the installed guarded entry, rather than
// a test registration or a replacement receiver. No original context is minted.
template <typename T>
static T port(const char *p_name) {
	CHECK(gdextension_get_proc_address(p_name) == nullptr);
	const auto entry = gdextension_get_guarded_proc_address(p_name);
	CHECK(entry != nullptr);
	return reinterpret_cast<T>(entry);
}

template <typename Input, typename Result, typename Call>
static void invalid_extents(Call p_call) {
	Input input{};
	input.abi = { 1, sizeof(Input), 0 };
	Result result;
	memset(&result, 0xa5, sizeof(result));
	uint8_t before[sizeof(result)];
	memcpy(before, &result, sizeof(result));
	alignas(Input) uint8_t bad_input[sizeof(Input) + alignof(Input)]{};
	const Input *inputs[] = {
		nullptr,
		reinterpret_cast<const Input *>(bad_input + 1),
		reinterpret_cast<const Input *>(UINTPTR_MAX - alignof(Input) + 1),
	};
	for (const Input *bad : inputs) {
		p_call(bad, &result);
		CHECK(memcmp(before, &result, sizeof(result)) == 0);
	}
	uint8_t input_before[sizeof(input)];
	memcpy(input_before, &input, sizeof(input));
	p_call(&input, static_cast<Result *>(nullptr));
	CHECK(memcmp(input_before, &input, sizeof(input)) == 0);
	alignas(Result) uint8_t bad_result[sizeof(Result) + alignof(Result)];
	memset(bad_result, 0xa5, sizeof(bad_result));
	uint8_t result_before[sizeof(bad_result)];
	memcpy(result_before, bad_result, sizeof(bad_result));
	p_call(&input, reinterpret_cast<Result *>(bad_result + 1));
	CHECK(memcmp(result_before, bad_result, sizeof(bad_result)) == 0);
	p_call(&input, reinterpret_cast<Result *>(UINTPTR_MAX - alignof(Result) + 1));
	CHECK(memcmp(input_before, &input, sizeof(input)) == 0);

	// Both directions of partial overlap and exact aliasing use a sufficiently
	// large real allocation; no smaller DTO is treated as a writable result.
	constexpr size_t alignment = alignof(Input) > alignof(Result) ? alignof(Input) : alignof(Result);
	static_assert(alignment < sizeof(Input) && alignment < sizeof(Result));
	alignas(Input) alignas(Result) uint8_t shared[sizeof(Input) + sizeof(Result) + alignment];
	uint8_t shared_before[sizeof(shared)];
	for (uint32_t variant = 0; variant < 3; ++variant) {
		memset(shared, 0xa5, sizeof(shared));
		memcpy(shared_before, shared, sizeof(shared));
		const size_t input_offset = variant == 2 ? alignment : 0;
		const size_t result_offset = variant == 1 ? alignment : 0;
		p_call(reinterpret_cast<const Input *>(shared + input_offset), reinterpret_cast<Result *>(shared + result_offset));
		CHECK(memcmp(shared_before, shared, sizeof(shared)) == 0);
	}
}

template <typename Result>
static void refused(const Result &p_result) {
	CHECK(p_result.abi.abi_version == 1);
	CHECK(p_result.abi.struct_size == sizeof(Result));
	CHECK(p_result.abi.reserved == 0);
	CHECK(p_result.status.error != OK);
	CHECK(p_result.status.detail_length > 0);
	CHECK(p_result.status.detail != nullptr);
	CHECK(p_result.custody == GDEXTENSION_OUTPUT_NO_CUSTODY_V1);
	CHECK(p_result.state == 0);
}
} // namespace TestExternalOutputAcquirePollInterface

TEST_CASE("[B4][OutputAcquirePollWire] installed guarded signatures reject invalid and overlapping DTO extents without touching bytes") {
	using namespace TestExternalOutputAcquirePollInterface;
	const auto acquire = port<Acquire>("device_session_output_acquire_v1");
	const auto poll = port<Poll>("device_session_output_poll_v1");
	REQUIRE(acquire != nullptr);
	REQUIRE(poll != nullptr);
	invalid_extents<GDExtensionOutputAcquireInputV1, GDExtensionOutputAcquireResultV1>([&](const auto *input, auto *result) { acquire({}, {}, 0, input, result); });
	invalid_extents<GDExtensionOutputGenerationInputV1, GDExtensionOutputPollResultV1>([&](const auto *input, auto *result) { poll({}, {}, input, result); });
}

TEST_CASE("[B4][OutputAcquirePollWire] valid DTO extents cannot acquire or observe authority without an original context") {
	using namespace TestExternalOutputAcquirePollInterface;
	const auto acquire = port<Acquire>("device_session_output_acquire_v1");
	const auto poll = port<Poll>("device_session_output_poll_v1");
	REQUIRE(acquire != nullptr);
	REQUIRE(poll != nullptr);
	GDExtensionOutputAcquireInputV1 input{};
	input.abi = { 1, sizeof(input), 0 };
	GDExtensionOutputGenerationInputV1 generation{};
	generation.abi = { 1, sizeof(generation), 0 };
	uint8_t input_before[sizeof(input)], generation_before[sizeof(generation)];
	memcpy(input_before, &input, sizeof(input));
	memcpy(generation_before, &generation, sizeof(generation));
	GDExtensionOutputAcquireResultV1 acquired;
	GDExtensionOutputPollResultV1 observed;
	memset(&acquired, 0xa5, sizeof(acquired));
	memset(&observed, 0xa5, sizeof(observed));
	acquire({}, {}, 0, &input, &acquired);
	poll({}, {}, &generation, &observed);
	refused(acquired);
	refused(observed);
	CHECK(memcmp(input_before, &input, sizeof(input)) == 0);
	CHECK(memcmp(generation_before, &generation, sizeof(generation)) == 0);
	const GDExtensionOutputTokenV1 empty_token{};
	CHECK(memcmp(&acquired.lease_token, &empty_token, sizeof(empty_token)) == 0);
	CHECK(acquired.writable_count == 0);
	CHECK(acquired.reserved == 0);
	for (uint32_t i = 0; i < 3; ++i) {
		CHECK(acquired.writables[i].role == 0);
		CHECK(acquired.writables[i].access == 0);
		CHECK(acquired.writables[i].rid == 0);
		CHECK(acquired.writables[i].allocation_generation == 0);
		CHECK(acquired.writables[i].byte_length == 0);
		CHECK(acquired.writables[i].stride == 0);
		CHECK(acquired.resource_slots[i] == UINT32_MAX);
		CHECK(acquired.use_indices[i] == UINT32_MAX);
	}
	CHECK(acquired.request_serial == 0);
	CHECK(acquired.action_ordinal == UINT64_MAX);
	CHECK(observed.generation == 0);
	CHECK(observed.known == 0);
	CHECK(observed.possible == 0);
	CHECK(observed.complete == 0);
	CHECK(observed.physical_returned == 0);
	CHECK(observed.request_serial == 0);
	CHECK(observed.action_ordinal == UINT64_MAX);
}

namespace TestExternalOutputMetadataInterface {
using namespace GDExtensionDeviceExit;
using Query = void (*)(const GDExtensionOutputMetadataQueryInputV1 *, GDExtensionOutputMetadataQueryResultV1 *);
using Create = void (*)(GDExtensionProductionPhysicalContextRefV1, GDExtensionProductionRequestRefV1, const GDExtensionOutputCreateInputV3 *, GDExtensionOutputCreateResultV1 *);
static_assert(std::is_same_v<Query, GDExtensionInterfaceDeviceSessionOutputMetadataQueryV1>);
static_assert(std::is_same_v<Create, GDExtensionInterfaceDeviceSessionOutputCreateV3>);
static_assert(std::is_same_v<Query, decltype(&device_session_output_metadata_query_v1)>);
static_assert(std::is_same_v<Create, decltype(&device_session_output_create_v3)>);
static_assert(std::is_standard_layout_v<GDExtensionOutputMetadataQueryInputV1>);
static_assert(std::is_standard_layout_v<GDExtensionOutputMetadataQueryResultV1>);
static_assert(std::is_standard_layout_v<GDExtensionOutputCreateInputV3>);
static_assert(offsetof(GDExtensionOutputMetadataQueryResultV1, abi) == 0);

static GDExtensionOutputMetadataQueryInputV1 query_input() {
	GDExtensionOutputMetadataQueryInputV1 value{};
	value.abi = { 1, sizeof(value), 0 };
	value.config.abi = { 1, sizeof(value.config), 0 };
	value.limits.abi = { 1, sizeof(value.limits), 0 };
	return value;
}
static GDExtensionOutputCreateInputV3 create_input() {
	GDExtensionOutputCreateInputV3 value{};
	value.abi = { 3, sizeof(value), 0 };
	value.config.abi = { 1, sizeof(value.config), 0 };
	value.requirements.abi = { 1, sizeof(value.requirements), 0 };
	value.requirements.limits.abi = { 1, sizeof(value.requirements.limits), 0 };
	return value;
}
template <typename Result>
static void refused(const Result &p_result, Error p_error) {
	CHECK(p_result.abi.abi_version == 1);
	CHECK(p_result.abi.struct_size == sizeof(Result));
	CHECK(p_result.abi.reserved == 0);
	CHECK(p_result.status.error == p_error);
	CHECK(p_result.status.detail_length > 0);
	CHECK(p_result.status.detail != nullptr);
}
static void cleared(const GDExtensionOutputMetadataQueryResultV1 &p_result) {
	const GDExtensionOutputMetadataRequirementsV1 empty{};
	CHECK(memcmp(&p_result.requirements, &empty, sizeof(empty)) == 0);
}
static void cleared(const GDExtensionOutputCreateResultV1 &p_result) {
	CHECK(p_result.particles == 0);
	CHECK(p_result.generation == 0);
	CHECK(p_result.capabilities == 0);
	CHECK(p_result.reserved == 0);
}

// All nested checks run before renderer lookup. Exercise each borrowed RID
// span independently, preserving the complete caller allocation on no-write paths.
template <typename Input, typename Result, typename Call>
static void nested_spans(Input p_input, Call p_call) {
	for (uint32_t span = 0; span < 2; ++span) {
		for (uint32_t variant = 0; variant < 7; ++variant) {
			auto input = p_input;
			Result result;
			memset(&result, 0xa5, sizeof(result));
			const auto result_before = result;
			auto &selected = span ? input.config.surface_materials : input.config.surfaces;
			selected.count = 1;
			switch (variant) {
				case 0:
					selected.data = nullptr;
					break;
				case 1:
					selected.data = reinterpret_cast<const uint64_t *>(uintptr_t(1));
					break;
				case 2:
					selected.data = reinterpret_cast<const uint64_t *>(UINTPTR_MAX - 7);
					selected.count = 2;
					break;
				case 3:
					selected.data = reinterpret_cast<const uint64_t *>(&result);
					break;
				case 4:
					selected.data = reinterpret_cast<const uint64_t *>(&input);
					break;
				case 5:
					// Include the tail after config: query limits / V3 resource selector.
					selected.data = reinterpret_cast<const uint64_t *>(reinterpret_cast<const uint8_t *>(&input) + sizeof(Input) - sizeof(uint64_t));
					break;
				case 6:
					// A partial output overlap must not clear even the result header.
					selected.data = reinterpret_cast<const uint64_t *>(reinterpret_cast<const uint8_t *>(&result) + sizeof(Result) - sizeof(uint64_t));
					selected.count = 2;
					break;
			}
			const auto input_before = input;
			p_call(&input, &result);
			CHECK(memcmp(&input_before, &input, sizeof(input)) == 0);
			if (variant == 4 || variant == 5) {
				refused(result, ERR_INVALID_PARAMETER);
				REQUIRE(result.status.detail_length == 8);
				CHECK(memcmp(result.status.detail, "CONTRACT", 8) == 0);
				cleared(result);
			} else {
				CHECK(memcmp(&result_before, &result, sizeof(result)) == 0);
			}
		}
	}
	for (uint32_t partial = 0; partial < 2; ++partial) {
		auto input = p_input;
		uint64_t shared[3] = { 11, 22, 33 };
		input.config.surfaces = { shared, 2, 0 };
		input.config.surface_materials = { shared + partial, 2, 0 };
		const auto input_before = input;
		Result result;
		memset(&result, 0xa5, sizeof(result));
		p_call(&input, &result);
		refused(result, ERR_INVALID_PARAMETER);
		REQUIRE(result.status.detail_length == 8);
		CHECK(memcmp(result.status.detail, "CONTRACT", 8) == 0);
		cleared(result);
		CHECK(memcmp(&input_before, &input, sizeof(input)) == 0);
		CHECK(shared[0] == 11);
		CHECK(shared[1] == 22);
		CHECK(shared[2] == 33);
	}
}
} // namespace TestExternalOutputMetadataInterface

TEST_CASE("[B4][OutputMetadataWire] guarded query and V3 signatures reject invalid and overlapping outer extents without writes") {
	using namespace TestExternalOutputMetadataInterface;
	using TestExternalOutputAcquirePollInterface::invalid_extents;
	using TestExternalOutputAcquirePollInterface::port;
	const auto query = port<Query>("device_session_output_metadata_query_v1");
	const auto create = port<Create>("device_session_output_create_v3");
	REQUIRE(query != nullptr);
	REQUIRE(create != nullptr);
	invalid_extents<GDExtensionOutputMetadataQueryInputV1, GDExtensionOutputMetadataQueryResultV1>([&](const auto *input, auto *result) { query(input, result); });
	invalid_extents<GDExtensionOutputCreateInputV3, GDExtensionOutputCreateResultV1>([&](const auto *input, auto *result) { create({}, {}, input, result); });
}

TEST_CASE("[B4][OutputMetadataWire] both nested spans reject invalid extents output outer and mutual overlaps") {
	using namespace TestExternalOutputMetadataInterface;
	using TestExternalOutputAcquirePollInterface::port;
	const auto query = port<Query>("device_session_output_metadata_query_v1");
	const auto create = port<Create>("device_session_output_create_v3");
	REQUIRE(query != nullptr);
	REQUIRE(create != nullptr);
	nested_spans<GDExtensionOutputMetadataQueryInputV1, GDExtensionOutputMetadataQueryResultV1>(query_input(), [&](const auto *input, auto *result) { query(input, result); });
	nested_spans<GDExtensionOutputCreateInputV3, GDExtensionOutputCreateResultV1>(create_input(), [&](const auto *input, auto *result) { create({}, {}, input, result); });
}

TEST_CASE("[B4][OutputMetadataWire] renderer absence wrong thread and absent original context preserve input") {
	using namespace TestExternalOutputMetadataInterface;
	using TestExternalOutputAcquirePollInterface::port;
	const auto query = port<Query>("device_session_output_metadata_query_v1");
	const auto create = port<Create>("device_session_output_create_v3");
	REQUIRE(query != nullptr);
	REQUIRE(create != nullptr);
	REQUIRE(RendererRD::ParticlesStorage::get_singleton() == nullptr);
	auto input = query_input();
	auto creation = create_input();
	// Valid SceneOnly fields ensure refusal is not a malformed config fallback.
	input.config.profile = 3;
	input.config.space = 1;
	input.config.max_views = 1;
	input.config.max_inflight = 1;
	input.config.query_enabled = 1;
	for (uint32_t i = 0; i < 3; ++i) {
		input.config.bounds.size[i] = 1;
		input.config.query_bounds.size[i] = 1;
	}
	creation.config = input.config;
	const auto input_before = input;
	const auto creation_before = creation;
	GDExtensionOutputMetadataQueryResultV1 quoted;
	GDExtensionOutputCreateResultV1 created;
	memset(&quoted, 0xa5, sizeof(quoted));
	memset(&created, 0xa5, sizeof(created));
	query(&input, &quoted);
	create({}, {}, &creation, &created);
	refused(quoted, ERR_UNAVAILABLE);
	refused(created, ERR_UNAVAILABLE);
	cleared(quoted);
	cleared(created);
	CHECK(memcmp(&input_before, &input, sizeof(input)) == 0);
	CHECK(memcmp(&creation_before, &creation, sizeof(creation)) == 0);

	// Existing empty component setup installs storage only. It creates no
	// protected RECORD, owner, admission, request, pin or renderer authority.
	RenderingDevice device;
	{
		RendererRD::ExternalLinkedOwnerTest fixture(device, true);
		memset(&quoted, 0xa5, sizeof(quoted));
		memset(&created, 0xa5, sizeof(created));
		query(&input, &quoted);
		create({}, {}, &creation, &created);
		// A preceding SceneTree test may leave a real server/main RD alive.
		// Query needs the main render thread, but no production context.
		auto *server = RenderingServer::get_singleton();
		auto *rd = RenderingDevice::get_singleton();
		const bool main_renderer = rd && server && server->is_on_render_thread() && server->get_rendering_device() == rd;
		if (main_renderer) {
			CHECK(quoted.status.error == OK);
			CHECK(quoted.abi.abi_version == 1);
			CHECK(quoted.abi.struct_size == sizeof(quoted));
			CHECK(quoted.requirements.required_metadata_bytes == RendererRD::ParticlesStorage::particles_external_scene_metadata_bytes());
			CHECK(quoted.requirements.required_alignment > 0);
			CHECK(quoted.requirements.slot_count == 1);
			CHECK(quoted.requirements.flattened_surface_count == 0);
			CHECK(quoted.requirements.binding_count == 0);
		} else {
			refused(quoted, ERR_UNAUTHORIZED);
			cleared(quoted);
		}
		refused(created, ERR_UNAUTHORIZED);
		cleared(created);
#ifdef THREADS_ENABLED
		struct WrongThreadQuery {
			Query query;
			const GDExtensionOutputMetadataQueryInputV1 *input;
			GDExtensionOutputMetadataQueryResultV1 result;
			Thread::ID caller = Thread::UNASSIGNED_ID;
			static void run(void *p_data) {
				auto &probe = *static_cast<WrongThreadQuery *>(p_data);
				probe.caller = Thread::get_caller_id();
				probe.query(probe.input, &probe.result);
			}
		} wrong{ query, &input, {} };
		memset(&wrong.result, 0xa5, sizeof(wrong.result));
		Thread worker;
		const Thread::ID worker_id = worker.start(&WrongThreadQuery::run, &wrong);
		worker.wait_to_finish();
		CHECK(worker_id != Thread::UNASSIGNED_ID);
		CHECK(wrong.caller == worker_id);
		CHECK(wrong.caller != Thread::get_caller_id());
		refused(wrong.result, ERR_UNAUTHORIZED);
		cleared(wrong.result);
		const char query_detail[] = "main_render_thread_required";
		REQUIRE(wrong.result.status.detail_length == sizeof(query_detail) - 1);
		CHECK(memcmp(wrong.result.status.detail, query_detail, sizeof(query_detail) - 1) == 0);
#endif
		const char create_detail[] = "original_record_context_required";
		REQUIRE(created.status.detail_length == sizeof(create_detail) - 1);
		CHECK(memcmp(created.status.detail, create_detail, sizeof(create_detail) - 1) == 0);
		CHECK(memcmp(&input_before, &input, sizeof(input)) == 0);
		CHECK(memcmp(&creation_before, &creation, sizeof(creation)) == 0);
		CHECK_FALSE(fixture.exists());
	}
	CHECK(RendererRD::ParticlesStorage::get_singleton() == nullptr);
}
