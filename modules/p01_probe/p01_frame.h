#pragma once

#include "p01_capture.h"

bool p01_validate_frame_event(const P01CaptureEvent &p_event, const P01CaptureEvent *p_prior);
