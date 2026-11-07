/**************************************************************************/
/*  external_output_storage.h                                             */
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

#include "servers/rendering/rendering_device.h"

class TestExternalOutputPrefixAccess;
#if defined(TESTS_ENABLED) && defined(MODULE_PARTICLES_RUNTIME_TESTS_ENABLED)
class ParticlesExternalOutputTestFixture;
#endif

namespace RendererRD {
class ParticlesStorage;

// Single-use, Core-owned allocation prefix, not an output lease or capability.
// No source enrollment, uniform-set factory, budget grant, publication or
// OutputReleased acknowledgment is provided here. The original ParticlesStorage
// owner must retain this object AND its budget charges until destroy_if_returned
// succeeds; UNKNOWN retains them permanently. The original K03 device-holder
// lease must keep RenderingDevice alive throughout retention (including UNKNOWN).
// This reference is not that lease. Never observe/maintain after device destruction.
// No destructor submits/free guesses.
class ExternalOutputStorage {
	friend class GDExtensionDeviceExit::Registry;
	friend class ParticlesStorage;
#if defined(TESTS_ENABLED) && defined(MODULE_PARTICLES_RUNTIME_TESTS_ENABLED)
	friend class ::ParticlesExternalOutputTestFixture;
#endif
#ifdef TESTS_ENABLED
	friend class ::TestExternalOutputPrefixAccess;
	friend class ExternalLinkedOwnerTest;
#endif

	enum Role : uint32_t {
		CURRENT_RECORDS,
		PREVIOUS_RECORDS,
		OUTPUT_META,
		GUARD_SCRATCH,
		GUARD_STATUS,
		PROTECTED_COMMANDS,
		GEOMETRY_PROOF,
		RESOURCE_COUNT,
	};
	struct Resource {
		RID rid;
		NativeDisposalWitness witness;
		uint32_t bytes = 0;
	};
	struct ResourceReport {
		NativeDisposalWitness::Report native;
		uint32_t bytes = 0;
	};
	struct Report {
		ResourceReport resources[RESOURCE_COUNT];
		uint64_t total_bytes = 0; // Planned owned bytes only, not a budget grant.
		uint32_t created_count = 0;
		uint32_t live_count = 0;
		uint32_t enqueued_count = 0;
		uint32_t returned_count = 0;
		uint32_t unknown_count = 0;
		Error creation_error = OK;
		bool retirement_requested = false;
		bool all_driver_returned = false; // This prefix only, never OutputReleased.
	};

	RenderingDevice &device;
	Resource resources[RESOURCE_COUNT];
	uint64_t total_bytes = 0;
	Error creation_error = OK;
	bool attempted = false;
	bool retirement_requested = false;
	bool unknown = false;

	explicit ExternalOutputStorage(RenderingDevice &p_device) : device(p_device) {}
	ExternalOutputStorage(const ExternalOutputStorage &) = delete;
	ExternalOutputStorage &operator=(const ExternalOutputStorage &) = delete;
	~ExternalOutputStorage() = default; // Only controlled destroy after observation.

	// Mechanical allocation only. A future production caller still needs the
	// original source/operation and real budget admission BEFORE calling this.
	// They do not exist yet: no production acquire/seal path calls this method.
	struct Instance192Layout {
		uint32_t bytes[RESOURCE_COUNT] = {};
		uint64_t total_bytes = 0;
		uint32_t resource_count = 0;
	};
	static Error plan_instance192(uint32_t p_capacity, uint32_t p_surface_count, Instance192Layout &r_layout);
	Error allocate_instance192(uint32_t p_capacity, uint32_t p_surface_count);
	// Internal stable-slot path only. The caller installs storage and the exact
	// original pin before entry; this is neither charge nor source admission.
	Error allocate_instance192_owned(GDExtensionProductionPhysicalContextRefV1, GDExtensionDeviceExit::ProductionOutputPin &, uint64_t &, uint32_t, uint32_t);
	Error request_retirement();
	Error observe(Report &r_report);
	// Explicit delegation only. Scheduling/fences remain the original RD's job.
	Error maintenance();
	static Error destroy_if_returned(ExternalOutputStorage *&p_storage);
};
} // namespace RendererRD
