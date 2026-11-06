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
#include "tests/test_macros.h"

#include <cstring>

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
