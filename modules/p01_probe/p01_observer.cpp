#include "p01_observer.h"

// Hook observations are scoped to the owning thread. RenderingDevice instances
// may issue work concurrently; a process-global mutable snapshot would lose
// events and introduce a data race between instances.
static thread_local P01HookSnapshot snapshot;

void p01_observe_buffer_write(uint64_t p_rid, uint32_t p_offset, uint32_t p_size, bool p_success) {
	(void)p_rid;
	(void)p_offset;
	snapshot.write_begin_count++;
	if (p_success) {
		snapshot.write_bytes += p_size;
	} else {
		snapshot.write_failure_count++;
	}
}
void p01_observe_readback_queued(uint64_t p_rid, uint32_t p_size) {
	(void)p_rid;
	(void)p_size;
	snapshot.readback_queued_count++;
}
void p01_observe_readback_completed(uint64_t p_rid, uint32_t p_size) {
	(void)p_rid;
	(void)p_size;
	snapshot.readback_completed_count++;
}
P01HookSnapshot p01_hook_snapshot() { return snapshot; }
void p01_hook_reset() { snapshot = {}; }
