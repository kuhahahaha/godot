#pragma once

#include "p01_public_snapshot.h"
#include "core/variant/variant.h"

// Encodes the literal shader's std430 scalar arrays, never C++ struct padding.
Error p01_encode_gpu_payload(const P01PublicSnapshot &p_snapshot, PackedByteArray &r_bytes);
PackedByteArray p01_initial_guarded_buffer();
