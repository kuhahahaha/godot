#pragma once
#include "core/variant/callable.h"
#include "core/variant/variant.h"
#include <array>
#include <memory>
#include <mutex>

// Immutable identity copied into each engine-owned callback. Physical identity
// must be resolved by the caller at actual enqueue, never inferred from a RID.
struct P01ReadbackIdentity {
	uint64_t run = 0, epoch = 0, token = 0, queue = 0;
	uint64_t device = 0, allocation = 0, rid = 0, owner = 0, alias = 0;
	uint32_t slot = 0;
	bool consumer = false;
	bool operator==(const P01ReadbackIdentity &p_other) const;
};

class P01ReadbackCollector {
	struct State;
	class Callback;
	std::shared_ptr<State> state;

public:
	P01ReadbackCollector(uint64_t p_run, uint64_t p_epoch, bool p_dual_view);
	bool shares_state(const P01ReadbackCollector &p_other) const { return state == p_other.state; }
	Error reserve(const P01ReadbackIdentity &p_identity, Callable &r_callback);
	// Call after the actual RD enqueue accepts the callback.
	Error mark_enqueued(const P01ReadbackIdentity &p_identity);
	// Only after RD enqueue returns non-OK without retaining the callback.
	// Never cancel accepted work or use this for timeout cleanup.
	Error cancel_unqueued(const P01ReadbackIdentity &p_identity);
	Error copy_completed(const P01ReadbackIdentity &p_identity, PackedByteArray &r_bytes) const;
	// Requires both original role requests; checks complete64B guards/transport.
	Error validate_pair(const P01ReadbackIdentity &p_snapshot, const P01ReadbackIdentity &p_consumer) const;
	void close();
	bool drained() const;
	bool poisoned() const;
};
