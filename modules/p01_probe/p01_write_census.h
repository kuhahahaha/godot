#pragma once
#include <cstdint>
#include <vector>

// A potential write recorded in the graph, not proof of GPU execution or bytes.
struct P01WritableUse {
	uint64_t driver = 0, device = 0, allocation = 0, sequence = 0, epoch = 0;
	uint32_t usage = 0, operation = 0;
};
enum P01WriteOperation : uint32_t { P01_DRAW = 1, P01_DRAW_INDIRECT, P01_COMPUTE, P01_COMPUTE_INDIRECT, P01_RAYTRACE, P01_CALLBACK };
bool p01_write_census_active();
void p01_write_census_record(uint64_t p_driver, uint64_t p_device, uint64_t p_allocation, uint32_t p_usage, uint32_t p_operation);
void p01_write_census_poison();
bool p01_write_census_is_poisoned();
std::vector<P01WritableUse> p01_write_census_events();
