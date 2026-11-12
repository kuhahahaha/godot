/**************************************************************************/
/*  render_capture_test_tracking.cpp                                      */
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

#include "servers/rendering/render_capture_test_tracking.h"

uint64_t RenderCaptureTestAllocationRegistry::allocate(uint64_t p_driver, uint64_t p_rid) {
	std::lock_guard<std::mutex> lock(mutex);
	if (!p_driver || !p_rid || next_serial == UINT64_MAX) {
		return 0;
	}
	for (const Allocation &allocation : allocations) {
		if (allocation.driver == p_driver) {
			return 0;
		}
		for (const Owner &owner : allocation.owners) {
			if (owner.rid == p_rid) {
				return 0;
			}
		}
	}
	for (Allocation &allocation : allocations) {
		if (!allocation.serial) {
			allocation.driver = p_driver;
			allocation.serial = next_serial++;
			allocation.owners[0].rid = p_rid;
			return allocation.serial;
		}
	}
	return 0;
}
bool RenderCaptureTestAllocationRegistry::add_alias(uint64_t p_driver, uint64_t p_serial, uint64_t p_rid) {
	std::lock_guard<std::mutex> lock(mutex);
	if (!p_rid) {
		return false;
	}
	for (const Allocation &allocation : allocations) {
		for (const Owner &owner : allocation.owners) {
			if (owner.rid == p_rid) {
				return false;
			}
		}
	}
	for (Allocation &allocation : allocations) {
		if (allocation.serial && allocation.driver == p_driver && allocation.serial == p_serial) {
			for (Owner &owner : allocation.owners) {
				if (!owner.rid) {
					owner.rid = p_rid;
					return true;
				}
			}
		}
	}
	return false;
}
bool RenderCaptureTestAllocationRegistry::release_owner(uint64_t p_driver, uint64_t p_serial, uint64_t p_rid) {
	std::lock_guard<std::mutex> lock(mutex);
	if (!p_rid) {
		return false;
	}
	for (Allocation &allocation : allocations) {
		if (allocation.serial && allocation.driver == p_driver && allocation.serial == p_serial) {
			for (Owner &owner : allocation.owners) {
				if (owner.rid == p_rid) {
					owner.rid = 0;
					return true;
				}
			}
		}
	}
	return false;
}
uint64_t RenderCaptureTestAllocationRegistry::resolve(uint64_t p_driver, uint64_t p_rid) const {
	std::lock_guard<std::mutex> lock(mutex);
	if (!p_rid) {
		return 0;
	}
	for (const Allocation &allocation : allocations) {
		if (allocation.driver == p_driver && allocation.serial) {
			for (const Owner &owner : allocation.owners) {
				if (owner.rid == p_rid) {
					return allocation.serial;
				}
			}
		}
	}
	return 0;
}
bool RenderCaptureTestAllocationRegistry::physically_live(uint64_t p_driver, uint64_t p_serial) const {
	std::lock_guard<std::mutex> lock(mutex);
	if (!p_serial) {
		return false;
	}
	for (const Allocation &allocation : allocations) {
		if (allocation.driver == p_driver && allocation.serial == p_serial) {
			return true;
		}
	}
	return false;
}
bool RenderCaptureTestAllocationRegistry::dispose(uint64_t p_driver, uint64_t p_serial) {
	std::lock_guard<std::mutex> lock(mutex);
	if (!p_serial) {
		return false;
	}
	for (Allocation &allocation : allocations) {
		if (allocation.driver == p_driver && allocation.serial == p_serial) {
			for (const Owner &owner : allocation.owners) {
				if (owner.rid) {
					return false;
				}
			}
			allocation = Allocation();
			return true;
		}
	}
	return false;
}

bool RenderCaptureTestAllocationRegistry::mark_access_escaped(uint64_t p_driver, uint64_t p_serial) {
	std::lock_guard<std::mutex> lock(mutex);
	if (!p_serial) {
		return false;
	}
	for (Allocation &allocation : allocations) {
		if (allocation.driver == p_driver && allocation.serial == p_serial) {
			allocation.escaped = true;
			return true;
		}
	}
	return false;
}
bool RenderCaptureTestAllocationRegistry::access_escaped(uint64_t p_driver, uint64_t p_serial) const {
	std::lock_guard<std::mutex> lock(mutex);
	if (device_access_escaped || !p_serial) {
		return true;
	}
	for (const Allocation &allocation : allocations) {
		if (allocation.driver == p_driver && allocation.serial == p_serial) {
			return allocation.escaped;
		}
	}
	return true;
}

uint64_t RenderCaptureTestAllocationRegistry::resolve_driver(uint64_t p_driver) const {
	std::lock_guard<std::mutex> lock(mutex);
	if (!p_driver) {
		return 0;
	}
	for (const Allocation &allocation : allocations) {
		if (allocation.driver == p_driver && allocation.serial) {
			return allocation.serial;
		}
	}
	return 0;
}

void RenderCaptureTestAllocationRegistry::mark_device_access_escaped() {
	std::lock_guard<std::mutex> lock(mutex);
	device_access_escaped = true;
}

uint64_t RenderCaptureTestResourceRegistry::track(RenderCaptureTestResourceKind p_kind, uint64_t p_driver) {
	std::lock_guard<std::mutex> lock(mutex);
	if (!p_driver || !next_token) {
		return 0;
	}
	for (const auto &entry : entries) {
		if (entry.token && entry.kind == p_kind && entry.driver == p_driver && !entry.released) {
			return 0;
		}
	}
	for (auto &entry : entries) {
		if (!entry.token) {
			const uint64_t token = next_token;
			next_token = token == UINT64_MAX ? 0 : token + 1;
			entry = { p_driver, token, p_kind, false };
			return token;
		}
	}
	return 0;
}
bool RenderCaptureTestResourceRegistry::observe_free(RenderCaptureTestResourceKind p_kind, uint64_t p_driver) {
	std::lock_guard<std::mutex> lock(mutex);
	for (auto &entry : entries) {
		if (entry.token && entry.kind == p_kind && entry.driver == p_driver) {
			if (entry.released) {
				// A second free for the same still-retained token is stale or
				// duplicate evidence. Keep the released record for retirement
				// acknowledgement, but fail closed for this observation.
				return false;
			}
			entry.released = true;
			return true;
		}
	}
	// Other engine resources were never enrolled in this narrow pool registry.
	return true;
}
RenderCaptureTestResourceRegistry::Status RenderCaptureTestResourceRegistry::status(uint64_t p_token) const {
	std::lock_guard<std::mutex> lock(mutex);
	for (const auto &entry : entries) {
		if (p_token && entry.token == p_token) {
			return entry.released ? Status::Released : Status::Live;
		}
	}
	return Status::Unknown;
}
bool RenderCaptureTestResourceRegistry::acknowledge(uint64_t p_token) {
	std::lock_guard<std::mutex> lock(mutex);
	for (auto &entry : entries) {
		if (p_token && entry.token == p_token && entry.released) {
			entry = {};
			return true;
		}
	}
	return false;
}
