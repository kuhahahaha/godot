#pragma once
#include <array>
#include <cstdint>
#include <mutex>

// One noncopyable instance per actual RD lifetime, with internally synchronized access.
// Serial is independent of session epoch. Capacity failure must poison coverage.
class P01PhysicalRegistry {
	struct Owner { uint64_t rid = 0; };
	struct Allocation {
		uint64_t driver = 0, serial = 0;
		bool escaped = false;
		std::array<Owner, 8> owners{};
	};
	std::array<Allocation, 128> allocations{};
	uint64_t next_serial = 1;
	bool device_access_escaped = false;
	mutable std::mutex mutex;
public:
	P01PhysicalRegistry() = default;
	P01PhysicalRegistry(const P01PhysicalRegistry &) = delete;
	P01PhysicalRegistry &operator=(const P01PhysicalRegistry &) = delete;
	uint64_t allocate(uint64_t p_driver, uint64_t p_rid);
	bool add_alias(uint64_t p_driver, uint64_t p_serial, uint64_t p_rid);
	bool release_owner(uint64_t p_driver, uint64_t p_serial, uint64_t p_rid);
	uint64_t resolve(uint64_t p_driver, uint64_t p_rid) const;
	uint64_t resolve_driver(uint64_t p_driver) const;
	bool physically_live(uint64_t p_driver, uint64_t p_serial) const;
	bool dispose(uint64_t p_driver, uint64_t p_serial);
	// Device/queue/descriptor exports can reach existing and future allocations.
	void mark_device_access_escaped();
	bool mark_access_escaped(uint64_t p_driver, uint64_t p_serial);
	// Unknown/stale identities cannot establish absence of native access.
	bool access_escaped(uint64_t p_driver, uint64_t p_serial) const;
};
