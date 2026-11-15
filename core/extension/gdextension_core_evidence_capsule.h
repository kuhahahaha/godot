/**************************************************************************/
/*  gdextension_core_evidence_capsule.h                                   */
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

#pragma once

#include "core/error/error_list.h"
#include "core/os/mutex.h"
#include "core/string/ustring.h"
#include "core/templates/vector.h"
#include "core/variant/dictionary.h"

#include <cstddef>
#include <cstdint>

namespace GDExtensionDeviceExit {

// P4-D (K03): ANGECAP1 Core Evidence Capsule v3 post-unload OSClosed &
// zero-leak witness + v2 safe pre-OS CoreFailure capsule + CS-03 atomic
// batch seal coordinator.

static constexpr char ANGECAP1_MAGIC[8] = { 'A', 'N', 'G', 'E', 'C', 'A', 'P', '1' };
static constexpr uint32_t ANGECAP1_HEADER_BYTES = 40u;
static constexpr uint32_t ANGECAP1_TRAILER_BYTES = 32u;
static constexpr uint64_t ANGECAP1_MAX_PAYLOAD_BYTES = 1048576ull;

static constexpr uint32_t ANGECAP1_WIRE_VERSION_FAILURE_V2 = 2u;
static constexpr uint32_t ANGECAP1_WIRE_VERSION_COMPLETE_DEINIT_V3 = 3u;

enum AngeCapFrameKind : uint32_t {
	ANGECAP_FRAME_BATCH = 1u,
	ANGECAP_FRAME_CHILD_SEAL = 2u,
	ANGECAP_FRAME_CORE_FAILURE = 3u,
};

struct CompleteDeinitClosureV1 {
	uint64_t child_serial = 0;
	uint64_t registration_serial = 0;
	uint64_t attachment_serial = 0;
	uint64_t preclose_ack_sequence = 0;
	bool os_close_attempted = false;
	bool os_close_returned = false;
	Error os_close_error = ERR_UNCONFIGURED;
	bool dso_resident_after_dlclose = true;
	uint64_t leaked_rendering_device_rids = 0;
	uint64_t leaked_u08_budget_bytes = 0;
	uint64_t foreign_lifetime_violation_count = 0;
	uint64_t unsettled_fences = 0;
};

struct CoreFailureWitnessV2 {
	uint64_t child_serial = 0;
	uint64_t registration_serial = 0;
	Error failure_error = FAILED;
	String failure_origin;
	String source_stream_ref;
	String close_disposition;
};

struct DecodedAngeCapFrame {
	uint32_t wire_version = 0;
	uint32_t frame_kind = 0;
	uint64_t child_serial = 0;
	uint64_t frame_sequence = 0;
	Dictionary payload;
	String frame_digest_hex;
};

struct VerifiedCapsuleSummary {
	uint32_t wire_version = 0;
	uint64_t child_serial = 0;
	uint32_t batch_count = 0;
	uint32_t event_count = 0;
	bool is_core_failure = false;
	bool os_closed_verified = false;
	uint64_t leaked_rendering_device_rids = UINT64_MAX;
	uint64_t leaked_u08_budget_bytes = UINT64_MAX;
	uint64_t foreign_lifetime_violation_count = UINT64_MAX;
	uint64_t unsettled_fences = UINT64_MAX;
};

// CS-03 atomic batch seal-commit coordinator:
// - Allocates candidate batch sequence inside lock;
// - Injected pre-commit abort burns candidate sequence without exposing
//   uncommitted events or ack;
// - Post-commit retry with identical request_token returns committed ack
//   idempotently;
// - CLOSE_BEGIN requires committed preclose_ack.
class CapsuleBatchCoordinator {
public:
	enum BatchState : uint8_t {
		BATCH_IDLE = 0,
		BATCH_OPEN = 1,
		BATCH_SEALED = 2,
	};

	struct SealAck {
		bool valid = false;
		uint64_t batch_seq = 0;
		uint64_t request_token = 0;
		uint32_t committed_event_count = 0;
	};

private:
	mutable Mutex mutex;
	BatchState state = BATCH_IDLE;
	uint64_t next_batch_seq = 1;
	uint32_t staged_events = 0;
	SealAck committed_preclose_ack;
	bool close_begin_entered = false;

public:
	Error open_preclose_batch();
	Error stage_preclose_event();
	Error seal_preclose_batch(uint64_t p_request_token, bool p_inject_abort_before_commit, SealAck &r_ack);
	Error enter_close_begin(uint64_t &r_verified_ack_seq);

	BatchState get_state() const;
	uint64_t peek_next_batch_seq() const;
	SealAck get_committed_ack() const;
	bool has_entered_close_begin() const;
};

// POSIX RTLD_NOLOAD non-residency probe after dlclose:
// Returns OK and sets r_resident = false iff dlopen(path, RTLD_NOLOAD | RTLD_LAZY) == nullptr.
// If still resident, closes the extra handle opened by the probe and sets r_resident = true.
Error probe_dso_resident_rtld_noload(const char *p_library_path, bool &r_resident);

Error pack_angecap1_frame(
		uint32_t p_wire_version,
		AngeCapFrameKind p_frame_kind,
		uint64_t p_child_serial,
		uint64_t p_frame_sequence,
		const Dictionary &p_payload,
		Vector<uint8_t> &r_out_stream,
		String *r_frame_digest_hex = nullptr);

Error decode_angecap1_stream(
		const Vector<uint8_t> &p_stream,
		Vector<DecodedAngeCapFrame> &r_frames);

Error emit_complete_deinit_capsule_v3(
		const CompleteDeinitClosureV1 &p_closure,
		Vector<uint8_t> &r_out_stream);

Error emit_core_failure_capsule_v2(
		const CoreFailureWitnessV2 &p_failure,
		Vector<uint8_t> &r_out_stream);

// Strict importer/verifier:
// - Rejects wire_version == 1, mixed wire versions, non-1 or non-contiguous
//   frame_sequence, SHA-256 digest tamper, missing/non-final ChildSeal;
// - For wire_version == 3: requires PreClose committed ack + PostClose OSClosed
//   (os_close_attempted && os_close_returned && os_close_error == OK &&
//   !dso_resident_after_dlclose) AND leaked_rendering_device_rids == 0 &&
//   leaked_u08_budget_bytes == 0 && unsettled_fences == 0;
// - For wire_version == 2: requires single CoreFailure (frame_kind == 3) frame
//   with exact 3-member failure envelope.
Error verify_unload_capsule_stream(
		const Vector<uint8_t> &p_stream,
		VerifiedCapsuleSummary &r_summary);

Error write_unload_capsule_stream_atomic(
		const String &p_out_path,
		const Vector<uint8_t> &p_stream);

} // namespace GDExtensionDeviceExit
