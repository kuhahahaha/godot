/**************************************************************************/
/*  native_disposal_witness.h                                              */
/**************************************************************************/

#pragma once

#include "core/object/ref_counted.h"

class RenderingDevice;
class TestNativeDisposalAccess;

// Core-owned, single-object driver-free observation. This is neither a native
// operation/source capability nor an aggregate OutputReleased acknowledgement.
// All access is through the original main RD on its render thread.
class NativeDisposalWitness {
	friend class RenderingDevice;
#ifdef TESTS_ENABLED
	friend class TestNativeDisposalAccess;
#endif

public:
	enum class State : uint8_t { EMPTY, LIVE, ENQUEUED, RETURNED, UNKNOWN };
	struct Report {
		State state = State::EMPTY;
		uint64_t rid = 0;
		uint64_t serial = 0;
	};

private:
	class Cell : public RefCounted {
	public:
		uint64_t device = 0;
		uint64_t rid = 0;
		uint64_t serial = 0;
		uint64_t backing = 0;
		State state = State::LIVE;
		// Both the actual RD resource/pending-free entry and the observer retain
		// this cell. No pointer into K storage or DSO callback survives here.
		bool matches(uint64_t p_rid, uint64_t p_serial, uint64_t p_backing) const {
			return rid && serial && backing && rid == p_rid && serial == p_serial && backing == p_backing;
		}
		bool enqueue(uint64_t p_rid, uint64_t p_serial, uint64_t p_backing) {
			if (state != State::LIVE || !matches(p_rid, p_serial, p_backing)) {
				state = State::UNKNOWN;
				return false;
			}
			state = State::ENQUEUED;
			return true;
		}
		void returned(uint64_t p_rid, uint64_t p_serial, uint64_t p_backing) {
			state = state == State::ENQUEUED && matches(p_rid, p_serial, p_backing) ? State::RETURNED : State::UNKNOWN;
		}
	};
	Ref<Cell> cell;
};
