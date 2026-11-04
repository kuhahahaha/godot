/**************************************************************************/
/*  external_output_storage.h                                             */
/**************************************************************************/

#pragma once

#include "servers/rendering/rendering_device.h"

class TestExternalOutputPrefixAccess;

namespace RendererRD {
class ParticlesStorage;

// Single-use, Core-owned allocation prefix, not an output lease or capability.
// No source enrollment, uniform-set factory, budget grant, publication or
// OutputReleased acknowledgement is provided here. The original ParticlesStorage
// owner must retain this object AND its budget charges until destroy_if_returned
// succeeds; UNKNOWN retains them permanently. The original K03 device-holder
// lease must keep RenderingDevice alive throughout retention (including UNKNOWN).
// This reference is not that lease. Never observe/maintain after device destruction.
// No destructor submits/free guesses.
class ExternalOutputStorage {
	friend class ParticlesStorage;
#ifdef TESTS_ENABLED
	friend class ::TestExternalOutputPrefixAccess;
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
	Error allocate_instance192(uint32_t p_capacity, uint32_t p_surface_count);
	Error request_retirement();
	Error observe(Report &r_report);
	// Explicit delegation only. Scheduling/fences remain the original RD's job.
	Error maintenance();
	static Error destroy_if_returned(ExternalOutputStorage *&p_storage);
};
} // namespace RendererRD
