#include "p01_frame.h"

bool p01_validate_frame_event(const P01CaptureEvent &p_event, const P01CaptureEvent *p_prior) {
	if (p_event.epoch == 0 || p_event.token == 0 || p_event.rid == 0 || p_event.allocation_serial == 0 || p_event.owner == 0 || p_event.alias == 0 || p_event.queue_sequence == 0) return false;
	if (p_prior && p_event.sequence <= p_prior->sequence) return false;
	return UINT32_MAX - p_event.offset >= p_event.size;
}
