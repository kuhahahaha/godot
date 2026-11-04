/**************************************************************************/
/*  external_output_storage.cpp                                           */
/**************************************************************************/

#include "external_output_storage.h"

#include "external_output_config.h"

namespace RendererRD {

Error ExternalOutputStorage::allocate_instance192(uint32_t p_capacity, uint32_t p_surface_count) {
	if (attempted || retirement_requested) {
		return ERR_ALREADY_IN_USE;
	}
	if (p_capacity > (1u << 20) || p_surface_count == 0 || p_surface_count > 8) {
		return ERR_INVALID_PARAMETER;
	}
	const Error ready = device._native_disposal_preflight();
	if (ready != OK) {
		return ready;
	}
	// Compute every extent before any native creation. RD has no public maximum
	// storage-buffer-byte limit; actual allocation failure stays a native error.
	// Fixed control extents match ParticlesExternalGuard's Instance192 ABI.
	uint32_t bytes[RESOURCE_COUNT] = {};
	const uint64_t counts[RESOURCE_COUNT] = { p_capacity, p_capacity, 8, 16, 8, p_surface_count, 224 + uint64_t(p_surface_count) * 144 };
	const uint64_t strides[RESOURCE_COUNT] = { 192, 192, 4, 4, 4, 20, 1 };
	uint64_t planned_bytes = 0;
	for (uint32_t i = 0; i < RESOURCE_COUNT; ++i) {
		const Error size_error = ExternalOutputConfig::checked_buffer_bytes(counts[i], strides[i], UINT32_MAX, bytes[i]);
		if (size_error != OK) {
			return size_error;
		}
		// Seven u32 extents sum safely in u64; no narrowing of total occurs.
		planned_bytes += bytes[i];
	}
	for (uint32_t i = 0; i < RESOURCE_COUNT; ++i) {
		resources[i].bytes = bytes[i];
	}
	total_bytes = planned_bytes;
	attempted = true;
	for (uint32_t i = 0; i < RESOURCE_COUNT; ++i) {
		if (resources[i].bytes == 0) {
			continue; // Capacity zero has no current/previous record allocation.
		}
		const BitField<RenderingDevice::StorageBufferUsage> usage = i == PROTECTED_COMMANDS ? BitField<RenderingDevice::StorageBufferUsage>(RenderingDevice::STORAGE_BUFFER_USAGE_DISPATCH_INDIRECT) : BitField<RenderingDevice::StorageBufferUsage>();
		creation_error = device._native_disposal_storage_create(resources[i].bytes, usage, resources[i].rid, resources[i].witness);
		if (creation_error != OK) {
			// Preserve original creation error and every completed prefix entry.
			// A busy/failed disposal is retryable; it cannot lose or reuse storage.
			request_retirement();
			return creation_error;
		}
	}
	return OK;
}

Error ExternalOutputStorage::request_retirement() {
	if (!device.is_main_instance || device.render_thread_id != Thread::get_caller_id()) {
		return ERR_UNAUTHORIZED;
	}
	// Mark future use closed even if the RD is temporarily busy. This component
	// never records native uses itself; owner retirement of actual uses is extra.
	retirement_requested = true;
	Error first_error = OK;
	for (uint32_t i = RESOURCE_COUNT; i > 0; --i) {
		Resource &resource = resources[i - 1];
		if (resource.rid.is_null()) {
			continue;
		}
		const Error error = device._native_disposal_free(resource.witness);
		if (error != OK && first_error == OK) {
			first_error = error;
		}
	}
	Report report;
	const Error observation = observe(report);
	return observation != OK ? observation : first_error;
}

Error ExternalOutputStorage::observe(Report &r_report) {
	if (!device.is_main_instance || device.render_thread_id != Thread::get_caller_id()) {
		return ERR_UNAUTHORIZED;
	}
	Report report;
	report.total_bytes = total_bytes;
	report.creation_error = creation_error;
	report.retirement_requested = retirement_requested;
	Error first_error = OK;
	for (uint32_t i = 0; i < RESOURCE_COUNT; ++i) {
		report.resources[i].bytes = resources[i].bytes;
		if (resources[i].rid.is_null()) {
			continue;
		}
		++report.created_count;
		const Error error = device._native_disposal_observe(resources[i].witness, report.resources[i].native);
		if (error != OK && first_error == OK) {
			first_error = error;
		}
		switch (report.resources[i].native.state) {
			case NativeDisposalWitness::State::LIVE: ++report.live_count; break;
			case NativeDisposalWitness::State::ENQUEUED: ++report.enqueued_count; break;
			case NativeDisposalWitness::State::RETURNED: ++report.returned_count; break;
			case NativeDisposalWitness::State::UNKNOWN:
				++report.unknown_count;
				unknown = true;
				break;
			case NativeDisposalWitness::State::EMPTY: break; // Failed/unauthorized observation.
		}
	}
	report.all_driver_returned = !unknown && first_error == OK && report.returned_count == report.created_count;
	r_report = report;
	return unknown ? ERR_INVALID_DATA : first_error;
}

Error ExternalOutputStorage::maintenance() {
	return device._native_disposal_maintenance();
}

Error ExternalOutputStorage::destroy_if_returned(ExternalOutputStorage *&p_storage) {
	if (!p_storage) {
		return OK;
	}
	Report report;
	const Error error = p_storage->observe(report);
	if (error != OK) {
		return error;
	}
	if (!report.all_driver_returned) {
		return ERR_BUSY;
	}
	delete p_storage;
	p_storage = nullptr;
	return OK;
}
} // namespace RendererRD
