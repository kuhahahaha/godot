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
	if (!p_serial) { return true; }
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
