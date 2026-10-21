#pragma once

#include "p01_public_snapshot.h"
#include "p01_readback_collector.h"
#include "servers/rendering/rendering_device.h"

// Private RT-owned resources. Releasing these RIDs is logical release only;
// the collector/observer must separately establish physical retirement.
class P01RDPool {
	friend class P01RDPoolTestAccess;
	RenderingDevice *rd = nullptr;
	ObjectID device_identity;
	RID shaders[2];
	RID pipelines[2];
	struct Slot {
		RID snapshot;
		RID consumer;
		RID upload; // At most one immutable 228B upload and one set per slot.
		RID deformation_set;
		RID transport_set;
		uint64_t capture_sequence = 0, transport_sequence = 0, observation_epoch = 0;
		bool consumed = false;
		bool captured = false;
		bool transported = false;
		bool readback_attempted[2] = {};
	};
	Slot slots[18];
	uint32_t capacity = 0;
	struct Retirement { uint64_t driver = 0, serial = 0; };
	Retirement retirements[54];
	uint32_t retirement_count = 0;
	ObjectID retirement_device;
	bool release_recorded = false;
	bool retirement_invalid = false;
	bool retirement_confirmed = false;
	uint64_t resource_tokens[40] = {};
	uint32_t resource_count = 0;
	bool track_resource(RID p_rid, P01PoolResourceKind p_kind);
	std::unique_ptr<P01ReadbackCollector> queued_collector;
	Error qualify_buffer(RID p_buffer) const;
	Error qualify_collector(uint32_t p_slot, const P01ReadbackCollector &p_collector) const;
	Error resolve_readback_identity(uint32_t p_slot, bool p_consumer, uint64_t p_run, uint64_t p_epoch, uint64_t p_request, P01ReadbackIdentity &r_identity) const;
	Error record_boundary(RID p_buffer, uint64_t p_operation, uint64_t &r_sequence, uint64_t &r_epoch) const;
	Error qualify_readback(const P01ReadbackIdentity &p_identity) const;
	bool on_owner_thread() const;
	bool has_active_list() const;
	RID create_set(RID p_shader, RID p_source, RID p_destination);


public:
	P01RDPool() = default;
	P01RDPool(const P01RDPool &) = delete;
	P01RDPool &operator=(const P01RDPool &) = delete;
	Error initialize(bool p_dual_view);
	Error record_public(uint32_t p_slot, const P01PublicSnapshot &p_snapshot);
	// Caller establishes the actual source owner/generation and 36B borrow window.
	Error record_native(uint32_t p_slot, RID p_source, uint32_t p_offset);
	Error record_transport(uint32_t p_slot);
	// Identity must come from the actual allocation registry at this RT call.
	Error enqueue_readback(const P01ReadbackIdentity &p_identity, P01ReadbackCollector &p_collector);
	Error enqueue_pair(uint32_t p_slot, uint64_t p_run, uint64_t p_epoch, uint64_t p_request, P01ReadbackCollector &p_collector, P01ReadbackIdentity (&r_identities)[2], uint32_t &r_enqueued);
	Error validate_pair(const P01ReadbackIdentity &p_snapshot, const P01ReadbackIdentity &p_consumer, const P01ReadbackCollector &p_collector) const;
	Error release();
	// Only this pool's buffers, sets, shaders and pipelines; never K03/device retirement.
	Error physical_retirement_status();
	bool retirement_required() const { return rd || release_recorded; }
};
