#pragma once

#include "p01_public_snapshot.h"
#include "p01_readback_collector.h"
#include "servers/rendering/rendering_device.h"

// Private RT-owned resources. Releasing these RIDs is logical release only;
// the collector/observer must separately establish physical retirement.
class P01RDPool {
	RenderingDevice *rd = nullptr;
	RID shaders[2];
	RID pipelines[2];
	struct Slot {
		RID snapshot;
		RID consumer;
		RID upload; // At most one immutable 228B upload and one set per slot.
		RID deformation_set;
		RID transport_set;
		bool consumed = false;
		bool captured = false;
		bool transported = false;
		bool readback_attempted[2] = {};
	};
	Slot slots[18];
	uint32_t capacity = 0;
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
	Error release();
};
