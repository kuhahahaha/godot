#include "p01_physical_registry.h"

uint64_t P01PhysicalRegistry::allocate(uint64_t p_driver, uint64_t p_rid) {
	std::lock_guard<std::mutex> lock(mutex);
	if (!p_driver || !p_rid || next_serial == UINT64_MAX) { return 0; }
	for (const Allocation &allocation : allocations) {
		if (allocation.driver == p_driver) { return 0; }
		for (const Owner &owner : allocation.owners) {
			if (owner.rid == p_rid) { return 0; }
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
bool P01PhysicalRegistry::add_alias(uint64_t p_driver, uint64_t p_serial, uint64_t p_rid) {
	std::lock_guard<std::mutex> lock(mutex);
	if (!p_rid) { return false; }
	for (const Allocation &allocation : allocations) {
		for (const Owner &owner : allocation.owners) {
			if (owner.rid == p_rid) { return false; }
		}
	}
	for (Allocation &allocation : allocations) {
		if (allocation.serial && allocation.driver == p_driver && allocation.serial == p_serial) {
			for (Owner &owner : allocation.owners) {
				if (!owner.rid) { owner.rid = p_rid; return true; }
			}
		}
	}
	return false;
}
bool P01PhysicalRegistry::release_owner(uint64_t p_driver, uint64_t p_serial, uint64_t p_rid) {
	std::lock_guard<std::mutex> lock(mutex);
	if (!p_rid) { return false; }
	for (Allocation &allocation : allocations) {
		if (allocation.serial && allocation.driver == p_driver && allocation.serial == p_serial) {
			for (Owner &owner : allocation.owners) {
				if (owner.rid == p_rid) { owner.rid = 0; return true; }
			}
		}
	}
	return false;
}
uint64_t P01PhysicalRegistry::resolve(uint64_t p_driver, uint64_t p_rid) const {
	std::lock_guard<std::mutex> lock(mutex);
	if (!p_rid) { return 0; }
	for (const Allocation &allocation : allocations) {
		if (allocation.driver == p_driver && allocation.serial) {
			for (const Owner &owner : allocation.owners) {
				if (owner.rid == p_rid) { return allocation.serial; }
			}
		}
	}
	return 0;
}
bool P01PhysicalRegistry::physically_live(uint64_t p_driver, uint64_t p_serial) const {
	std::lock_guard<std::mutex> lock(mutex);
	if (!p_serial) { return false; }
	for (const Allocation &allocation : allocations) {
		if (allocation.driver == p_driver && allocation.serial == p_serial) { return true; }
	}
	return false;
}
bool P01PhysicalRegistry::dispose(uint64_t p_driver, uint64_t p_serial) {
	std::lock_guard<std::mutex> lock(mutex);
	if (!p_serial) { return false; }
	for (Allocation &allocation : allocations) {
		if (allocation.driver == p_driver && allocation.serial == p_serial) {
			for (const Owner &owner : allocation.owners) { if (owner.rid) { return false; } }
			allocation = Allocation();
			return true;
		}
	}
	return false;
}

bool P01PhysicalRegistry::mark_access_escaped(uint64_t p_driver, uint64_t p_serial) {
	std::lock_guard<std::mutex> lock(mutex);
	if (!p_serial) { return false; }
	for (Allocation &allocation : allocations) {
		if (allocation.driver == p_driver && allocation.serial == p_serial) {
			allocation.escaped = true;
			return true;
		}
	}
	return false;
}
bool P01PhysicalRegistry::access_escaped(uint64_t p_driver, uint64_t p_serial) const {
	std::lock_guard<std::mutex> lock(mutex);
	if (device_access_escaped || !p_serial) { return true; }
	for (const Allocation &allocation : allocations) {
		if (allocation.driver == p_driver && allocation.serial == p_serial) { return allocation.escaped; }
	}
	return true;
}

uint64_t P01PhysicalRegistry::resolve_driver(uint64_t p_driver) const {
	std::lock_guard<std::mutex> lock(mutex);
	if (!p_driver) return 0;
	for (const Allocation &allocation : allocations) {
		if (allocation.driver == p_driver && allocation.serial) return allocation.serial;
	}
	return 0;
}

void P01PhysicalRegistry::mark_device_access_escaped() {
	std::lock_guard<std::mutex> lock(mutex);
	device_access_escaped = true;
}

uint64_t P01PoolResourceRegistry::track(P01PoolResourceKind p_kind, uint64_t p_driver) {
	std::lock_guard<std::mutex> lock(mutex);
	if (!p_driver || !next_token) { return 0; }
	for (const auto &entry : entries) { if (entry.token && entry.kind == p_kind && entry.driver == p_driver && !entry.released) { return 0; } }
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
bool P01PoolResourceRegistry::observe_free(P01PoolResourceKind p_kind, uint64_t p_driver) {
	std::lock_guard<std::mutex> lock(mutex);
	for (auto &entry : entries) {
		if (entry.token && entry.kind == p_kind && entry.driver == p_driver && !entry.released) { entry.released = true; return true; }
	}
	// Other engine resources were never enrolled in this narrow pool registry.
	return true;
}
P01PoolResourceRegistry::Status P01PoolResourceRegistry::status(uint64_t p_token) const {
	std::lock_guard<std::mutex> lock(mutex);
	for (const auto &entry : entries) { if (p_token && entry.token == p_token) { return entry.released ? Status::Released : Status::Live; } }
	return Status::Unknown;
}
bool P01PoolResourceRegistry::acknowledge(uint64_t p_token) {
	std::lock_guard<std::mutex> lock(mutex);
	for (auto &entry : entries) { if (p_token && entry.token == p_token && entry.released) { entry = {}; return true; } }
	return false;
}
