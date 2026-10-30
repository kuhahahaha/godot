/**************************************************************************/
/*  render_capture_test_tracking.h                                        */
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

#pragma once
#include "modules/modules_enabled.gen.h"
#if defined(TESTS_ENABLED) && defined(MODULE_SKINNED_MESH_CAPTURE_TESTS_ENABLED)
#include <array>
#include <cstdint>
#include <mutex>

// One noncopyable instance per actual RD lifetime, with internally synchronized access.
// Serial is independent of session epoch. Capacity failure must poison coverage.
class RenderCaptureTestAllocationRegistry {
	struct Owner {
		uint64_t rid = 0;
	};
	struct Allocation {
		uint64_t driver = 0, serial = 0;
		bool escaped = false;
		std::array<Owner, 8> owners{};
	};
	std::array<Allocation, 128> allocations{};
	uint64_t next_serial = 1;
	bool device_access_escaped = false;
	mutable std::mutex mutex;

public:
	RenderCaptureTestAllocationRegistry() = default;
	RenderCaptureTestAllocationRegistry(const RenderCaptureTestAllocationRegistry &) = delete;
	RenderCaptureTestAllocationRegistry &operator=(const RenderCaptureTestAllocationRegistry &) = delete;
	uint64_t allocate(uint64_t p_driver, uint64_t p_rid);
	bool add_alias(uint64_t p_driver, uint64_t p_serial, uint64_t p_rid);
	bool release_owner(uint64_t p_driver, uint64_t p_serial, uint64_t p_rid);
	uint64_t resolve(uint64_t p_driver, uint64_t p_rid) const;
	uint64_t resolve_driver(uint64_t p_driver) const;
	bool physically_live(uint64_t p_driver, uint64_t p_serial) const;
	bool dispose(uint64_t p_driver, uint64_t p_serial);
	// Device/queue/descriptor exports can reach existing and future allocations.
	void mark_device_access_escaped();
	bool mark_access_escaped(uint64_t p_driver, uint64_t p_serial);
	// Unknown/stale identities cannot establish absence of native access.
	bool access_escaped(uint64_t p_driver, uint64_t p_serial) const;
};

// Tracks only the pool's explicitly registered native objects. This is not a
// device-wide or K03 retirement ledger. Registration resolves actual RD owners;
// release is observed after the original driver free returns.
enum class RenderCaptureTestResourceKind : uint8_t { Shader,
	ComputePipeline,
	UniformSet,
	Texture };
class RenderCaptureTestResourceRegistry {
	struct Entry {
		uint64_t driver = 0, token = 0;
		RenderCaptureTestResourceKind kind = RenderCaptureTestResourceKind::Shader;
		bool released = false;
	};
	std::array<Entry, 128> entries{};
	uint64_t next_token = 1;
	mutable std::mutex mutex;

public:
	enum class ResourceStatus { Unknown,
		Live,
		Released };
	uint64_t track(RenderCaptureTestResourceKind p_kind, uint64_t p_driver);
	bool observe_free(RenderCaptureTestResourceKind p_kind, uint64_t p_driver);
	ResourceStatus status(uint64_t p_token) const;
	bool acknowledge(uint64_t p_token);
};

#endif
