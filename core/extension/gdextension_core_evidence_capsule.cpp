/**************************************************************************/
/*  gdextension_core_evidence_capsule.cpp                                 */
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

#include "gdextension_core_evidence_capsule.h"

#include "core/crypto/crypto_core.h"
#include "core/extension/gdextension_library_exit_guard.h"
#include "core/extension/gdextension_loader.h"
#include "core/extension/gdextension_production_request.h"
#include "core/io/json.h"
#include "core/io/marshalls.h"
#include "core/os/os.h"
#include "core/variant/array.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#if defined(UNIX_ENABLED) || defined(MACOS_ENABLED)
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace GDExtensionDeviceExit {

namespace {

String _sha256_to_hex(const unsigned char p_digest[32]) {
	static constexpr char HEX_DIGITS[16] = {
		'0', '1', '2', '3', '4', '5', '6', '7',
		'8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
	};
	char buf[65];
	for (int i = 0; i < 32; i++) {
		buf[i * 2] = HEX_DIGITS[(p_digest[i] >> 4) & 0x0f];
		buf[i * 2 + 1] = HEX_DIGITS[p_digest[i] & 0x0f];
	}
	buf[64] = '\0';
	return String::utf8(buf, 64);
}

bool _dict_get_uint64(const Dictionary &p_dict, const char *p_key, uint64_t &r_val) {
	const String key = String::utf8(p_key);
	if (!p_dict.has(key)) {
		return false;
	}
	const Variant v = p_dict[key];
	if (v.get_type() != Variant::INT && v.get_type() != Variant::FLOAT) {
		return false;
	}
	const int64_t signed_val = int64_t(v);
	if (signed_val < 0) {
		return false;
	}
	if (v.get_type() == Variant::FLOAT) {
		const double d = double(v);
		if (d < 0.0 || double(signed_val) != d) {
			return false;
		}
	}
	r_val = uint64_t(signed_val);
	return true;
}

bool _dict_get_int64(const Dictionary &p_dict, const char *p_key, int64_t &r_val) {
	const String key = String::utf8(p_key);
	if (!p_dict.has(key)) {
		return false;
	}
	const Variant v = p_dict[key];
	if (v.get_type() != Variant::INT && v.get_type() != Variant::FLOAT) {
		return false;
	}
	const int64_t signed_val = int64_t(v);
	if (v.get_type() == Variant::FLOAT && double(signed_val) != double(v)) {
		return false;
	}
	r_val = signed_val;
	return true;
}

bool _dict_get_bool(const Dictionary &p_dict, const char *p_key, bool &r_val) {
	const String key = String::utf8(p_key);
	if (!p_dict.has(key)) {
		return false;
	}
	const Variant v = p_dict[key];
	if (v.get_type() != Variant::BOOL) {
		return false;
	}
	r_val = bool(v);
	return true;
}

bool _dict_get_nonempty_string(const Dictionary &p_dict, const char *p_key, String &r_val) {
	const String key = String::utf8(p_key);
	if (!p_dict.has(key)) {
		return false;
	}
	const Variant v = p_dict[key];
	if (v.get_type() != Variant::STRING) {
		return false;
	}
	const String s = String(v);
	if (s.is_empty()) {
		return false;
	}
	r_val = s;
	return true;
}

} // namespace

Error CapsuleBatchCoordinator::open_preclose_batch() {
	MutexLock lock(mutex);
	if (state != BATCH_IDLE || close_begin_entered) {
		return ERR_INVALID_PARAMETER;
	}
	state = BATCH_OPEN;
	staged_events = 0;
	return OK;
}

Error CapsuleBatchCoordinator::stage_preclose_event() {
	MutexLock lock(mutex);
	if (state != BATCH_OPEN || close_begin_entered) {
		return ERR_INVALID_PARAMETER;
	}
	if (staged_events == UINT32_MAX) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	staged_events++;
	return OK;
}

Error CapsuleBatchCoordinator::seal_preclose_batch(
		uint64_t p_request_token,
		bool p_inject_abort_before_commit,
		SealAck &r_ack) {
	if (p_request_token == 0) {
		return ERR_INVALID_PARAMETER;
	}
	MutexLock lock(mutex);
	// Post-commit lost-return retry with identical request_token is idempotent.
	if (state == BATCH_SEALED && committed_preclose_ack.valid && committed_preclose_ack.request_token == p_request_token) {
		r_ack = committed_preclose_ack;
		return OK;
	}
	if (state != BATCH_OPEN || staged_events == 0 || close_begin_entered) {
		return ERR_INVALID_PARAMETER;
	}
	if (next_batch_seq == UINT64_MAX) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	const uint64_t candidate_seq = next_batch_seq;
	next_batch_seq++;
	if (p_inject_abort_before_commit) {
		// Burn candidate_seq without publishing uncommitted events or ack.
		return ERR_CANT_CREATE;
	}
	committed_preclose_ack.valid = true;
	committed_preclose_ack.batch_seq = candidate_seq;
	committed_preclose_ack.request_token = p_request_token;
	committed_preclose_ack.committed_event_count = staged_events;
	state = BATCH_SEALED;
	r_ack = committed_preclose_ack;
	return OK;
}

Error CapsuleBatchCoordinator::enter_close_begin(uint64_t &r_verified_ack_seq) {
	MutexLock lock(mutex);
	if (state != BATCH_SEALED || !committed_preclose_ack.valid || committed_preclose_ack.batch_seq == 0) {
		return ERR_UNAUTHORIZED;
	}
	close_begin_entered = true;
	r_verified_ack_seq = committed_preclose_ack.batch_seq;
	return OK;
}

CapsuleBatchCoordinator::BatchState CapsuleBatchCoordinator::get_state() const {
	MutexLock lock(mutex);
	return state;
}

uint64_t CapsuleBatchCoordinator::peek_next_batch_seq() const {
	MutexLock lock(mutex);
	return next_batch_seq;
}

CapsuleBatchCoordinator::SealAck CapsuleBatchCoordinator::get_committed_ack() const {
	MutexLock lock(mutex);
	return committed_preclose_ack;
}

bool CapsuleBatchCoordinator::has_entered_close_begin() const {
	MutexLock lock(mutex);
	return close_begin_entered;
}

Error probe_dso_resident_rtld_noload(const char *p_library_path, bool &r_resident) {
	if (!p_library_path || p_library_path[0] == '\0') {
		return ERR_INVALID_PARAMETER;
	}
#if defined(UNIX_ENABLED) || defined(MACOS_ENABLED)
	void *probe_handle = dlopen(p_library_path, RTLD_NOLOAD | RTLD_LAZY);
	if (probe_handle != nullptr) {
		dlclose(probe_handle);
		r_resident = true;
	} else {
		r_resident = false;
	}
	return OK;
#else
	r_resident = true;
	return ERR_UNAVAILABLE;
#endif
}

Error pack_angecap1_frame(
		uint32_t p_wire_version,
		AngeCapFrameKind p_frame_kind,
		uint64_t p_child_serial,
		uint64_t p_frame_sequence,
		const Dictionary &p_payload,
		Vector<uint8_t> &r_out_stream,
		String *r_frame_digest_hex) {
	if (p_wire_version != ANGECAP1_WIRE_VERSION_FAILURE_V2 &&
			p_wire_version != ANGECAP1_WIRE_VERSION_COMPLETE_DEINIT_V3) {
		return ERR_INVALID_PARAMETER;
	}
	const uint32_t raw_kind = uint32_t(p_frame_kind);
	if (raw_kind < ANGECAP_FRAME_BATCH || raw_kind > ANGECAP_FRAME_CORE_FAILURE) {
		return ERR_INVALID_PARAMETER;
	}
	if (p_child_serial == 0 || p_frame_sequence == 0) {
		return ERR_INVALID_PARAMETER;
	}

	const String canonical_json = JSON::stringify(p_payload, "", true, true);
	const CharString utf8 = canonical_json.utf8();
	const uint64_t payload_len = uint64_t(utf8.length());
	if (payload_len == 0 || payload_len > ANGECAP1_MAX_PAYLOAD_BYTES) {
		return ERR_INVALID_PARAMETER;
	}

	const int64_t prev_size = r_out_stream.size();
	const int64_t frame_bytes = int64_t(ANGECAP1_HEADER_BYTES) + int64_t(payload_len) + int64_t(ANGECAP1_TRAILER_BYTES);
	if (r_out_stream.resize(prev_size + frame_bytes) != OK) {
		return ERR_OUT_OF_MEMORY;
	}

	uint8_t *dst = r_out_stream.ptrw() + prev_size;
	std::memcpy(dst, ANGECAP1_MAGIC, 8);
	encode_uint32(p_wire_version, dst + 8);
	encode_uint32(raw_kind, dst + 12);
	encode_uint64(p_child_serial, dst + 16);
	encode_uint64(p_frame_sequence, dst + 24);
	encode_uint64(payload_len, dst + 32);
	std::memcpy(dst + ANGECAP1_HEADER_BYTES, utf8.get_data(), size_t(payload_len));

	unsigned char digest[32] = {};
	const Error sha_err = CryptoCore::sha256(dst, size_t(ANGECAP1_HEADER_BYTES + payload_len), digest);
	if (sha_err != OK) {
		r_out_stream.resize(prev_size);
		return sha_err;
	}
	std::memcpy(dst + ANGECAP1_HEADER_BYTES + payload_len, digest, ANGECAP1_TRAILER_BYTES);
	if (r_frame_digest_hex != nullptr) {
		*r_frame_digest_hex = _sha256_to_hex(digest);
	}
	return OK;
}

Error decode_angecap1_stream(
		const Vector<uint8_t> &p_stream,
		Vector<DecodedAngeCapFrame> &r_frames) {
	r_frames.clear();
	const int64_t total_len = p_stream.size();
	if (total_len < int64_t(ANGECAP1_HEADER_BYTES + 2u + ANGECAP1_TRAILER_BYTES)) {
		return ERR_INVALID_DATA;
	}

	const uint8_t *src = p_stream.ptr();
	int64_t offset = 0;
	uint64_t expected_sequence = 1;
	uint32_t stream_wire_version = 0;
	uint64_t stream_child_serial = 0;

	while (offset < total_len) {
		const int64_t remaining = total_len - offset;
		if (remaining < int64_t(ANGECAP1_HEADER_BYTES + ANGECAP1_TRAILER_BYTES)) {
			r_frames.clear();
			return ERR_INVALID_DATA;
		}
		const uint8_t *hdr = src + offset;
		if (std::memcmp(hdr, ANGECAP1_MAGIC, 8) != 0) {
			r_frames.clear();
			return ERR_INVALID_DATA;
		}
		const uint32_t wire_version = decode_uint32(hdr + 8);
		const uint32_t frame_kind = decode_uint32(hdr + 12);
		const uint64_t child_serial = decode_uint64(hdr + 16);
		const uint64_t frame_sequence = decode_uint64(hdr + 24);
		const uint64_t payload_len = decode_uint64(hdr + 32);

		if (wire_version != ANGECAP1_WIRE_VERSION_FAILURE_V2 &&
				wire_version != ANGECAP1_WIRE_VERSION_COMPLETE_DEINIT_V3) {
			r_frames.clear();
			return ERR_INVALID_DATA;
		}
		if (frame_kind < ANGECAP_FRAME_BATCH || frame_kind > ANGECAP_FRAME_CORE_FAILURE) {
			r_frames.clear();
			return ERR_INVALID_DATA;
		}
		if (child_serial == 0 || frame_sequence != expected_sequence ||
				payload_len == 0 || payload_len > ANGECAP1_MAX_PAYLOAD_BYTES) {
			r_frames.clear();
			return ERR_INVALID_DATA;
		}
		if (expected_sequence == 1) {
			stream_wire_version = wire_version;
			stream_child_serial = child_serial;
		} else if (wire_version != stream_wire_version || child_serial != stream_child_serial) {
			// Mixed wire versions or mixed child_serial in one stream are forbidden.
			r_frames.clear();
			return ERR_INVALID_DATA;
		}

		const int64_t frame_total = int64_t(ANGECAP1_HEADER_BYTES) + int64_t(payload_len) + int64_t(ANGECAP1_TRAILER_BYTES);
		if (remaining < frame_total) {
			r_frames.clear();
			return ERR_INVALID_DATA;
		}

		unsigned char computed_digest[32] = {};
		if (CryptoCore::sha256(hdr, size_t(ANGECAP1_HEADER_BYTES + payload_len), computed_digest) != OK) {
			r_frames.clear();
			return ERR_BUG;
		}
		const uint8_t *trailer = hdr + ANGECAP1_HEADER_BYTES + payload_len;
		if (std::memcmp(computed_digest, trailer, ANGECAP1_TRAILER_BYTES) != 0) {
			r_frames.clear();
			return ERR_INVALID_DATA;
		}

		String json_text;
		if (json_text.append_utf8(reinterpret_cast<const char *>(hdr + ANGECAP1_HEADER_BYTES), int(payload_len)) != OK) {
			r_frames.clear();
			return ERR_INVALID_DATA;
		}
		Ref<JSON> parser;
		parser.instantiate();
		if (parser->parse(json_text) != OK || parser->get_data().get_type() != Variant::DICTIONARY) {
			r_frames.clear();
			return ERR_INVALID_DATA;
		}

		DecodedAngeCapFrame decoded;
		decoded.wire_version = wire_version;
		decoded.frame_kind = frame_kind;
		decoded.child_serial = child_serial;
		decoded.frame_sequence = frame_sequence;
		decoded.payload = Dictionary(parser->get_data());
		decoded.frame_digest_hex = _sha256_to_hex(computed_digest);
		r_frames.push_back(decoded);

		offset += frame_total;
		if (expected_sequence == UINT64_MAX && offset < total_len) {
			r_frames.clear();
			return ERR_INVALID_DATA;
		}
		expected_sequence++;
	}

	return r_frames.is_empty() ? ERR_INVALID_DATA : OK;
}

Error emit_complete_deinit_capsule_v3(
		const CompleteDeinitClosureV1 &p_closure,
		Vector<uint8_t> &r_out_stream) {
	r_out_stream.clear();
	if (p_closure.child_serial == 0 ||
			p_closure.registration_serial == 0 ||
			p_closure.preclose_ack_sequence == 0) {
		return ERR_INVALID_PARAMETER;
	}

	Dictionary preclose_payload;
	preclose_payload["attachment_serial"] = int64_t(p_closure.attachment_serial);
	preclose_payload["batch_seq"] = int64_t(1);
	preclose_payload["close_begin_after_committed_ack"] = true;
	preclose_payload["event_count"] = int64_t(2);
	preclose_payload["phase"] = "PreClose";
	preclose_payload["preclose_ack_sequence"] = int64_t(p_closure.preclose_ack_sequence);
	preclose_payload["registration_serial"] = int64_t(p_closure.registration_serial);
	preclose_payload["schema"] = "ange-core-evidence-batch/v3";

	const bool os_closed_ok =
			p_closure.os_close_attempted &&
			p_closure.os_close_returned &&
			p_closure.os_close_error == OK;

	Dictionary postclose_payload;
	postclose_payload["attachment_serial"] = int64_t(p_closure.attachment_serial);
	postclose_payload["batch_seq"] = int64_t(2);
	postclose_payload["dso_close_state"] = os_closed_ok ? "OSClosed" : "OSCloseIncomplete";
	postclose_payload["dso_resident_after_dlclose"] = p_closure.dso_resident_after_dlclose;
	postclose_payload["event_count"] = int64_t(2);
	postclose_payload["foreign_lifetime_violation_count"] = int64_t(p_closure.foreign_lifetime_violation_count);
	postclose_payload["leaked_rendering_device_rids"] = int64_t(p_closure.leaked_rendering_device_rids);
	postclose_payload["leaked_u08_budget_bytes"] = int64_t(p_closure.leaked_u08_budget_bytes);
	postclose_payload["os_close_attempted"] = p_closure.os_close_attempted;
	postclose_payload["os_close_error"] = int64_t(p_closure.os_close_error);
	postclose_payload["os_close_returned"] = p_closure.os_close_returned;
	postclose_payload["os_disposition"] = (os_closed_ok && !p_closure.dso_resident_after_dlclose)
			? "OSClosed"
			: (os_closed_ok ? "OSClosedDyldPinned" : "OSCloseIncomplete");
	postclose_payload["phase"] = "CorePostDeinit";
	postclose_payload["registration_serial"] = int64_t(p_closure.registration_serial);
	postclose_payload["schema"] = "ange-core-evidence-batch/v3";
	postclose_payload["unsettled_fences"] = int64_t(p_closure.unsettled_fences);

	String digest_batch_1;
	String digest_batch_2;
	Error err = pack_angecap1_frame(
			ANGECAP1_WIRE_VERSION_COMPLETE_DEINIT_V3,
			ANGECAP_FRAME_BATCH,
			p_closure.child_serial,
			1,
			preclose_payload,
			r_out_stream,
			&digest_batch_1);
	if (err != OK) {
		r_out_stream.clear();
		return err;
	}
	err = pack_angecap1_frame(
			ANGECAP1_WIRE_VERSION_COMPLETE_DEINIT_V3,
			ANGECAP_FRAME_BATCH,
			p_closure.child_serial,
			2,
			postclose_payload,
			r_out_stream,
			&digest_batch_2);
	if (err != OK) {
		r_out_stream.clear();
		return err;
	}

	Array batch_digests;
	batch_digests.push_back(digest_batch_1);
	batch_digests.push_back(digest_batch_2);

	Dictionary seal_payload;
	seal_payload["batch_count"] = int64_t(2);
	seal_payload["batch_digests"] = batch_digests;
	seal_payload["event_count"] = int64_t(4);
	seal_payload["ok"] = true;
	seal_payload["phase"] = "Sealed";
	seal_payload["schema"] = "ange-core-evidence-seal/v3";

	err = pack_angecap1_frame(
			ANGECAP1_WIRE_VERSION_COMPLETE_DEINIT_V3,
			ANGECAP_FRAME_CHILD_SEAL,
			p_closure.child_serial,
			3,
			seal_payload,
			r_out_stream,
			nullptr);
	if (err != OK) {
		r_out_stream.clear();
		return err;
	}
	return OK;
}

Error emit_core_failure_capsule_v2(
		const CoreFailureWitnessV2 &p_failure,
		Vector<uint8_t> &r_out_stream) {
	r_out_stream.clear();
	if (p_failure.child_serial == 0 ||
			p_failure.registration_serial == 0 ||
			p_failure.failure_error == OK ||
			p_failure.failure_origin.is_empty() ||
			p_failure.source_stream_ref.is_empty() ||
			p_failure.close_disposition.is_empty()) {
		return ERR_INVALID_PARAMETER;
	}

	Dictionary payload;
	payload["CoreClose_source_stream_ref"] = p_failure.source_stream_ref;
	payload["CoreFailureClose"] = p_failure.close_disposition;
	payload["CoreFailureOrigin"] = p_failure.failure_origin;
	payload["failure_error"] = int64_t(p_failure.failure_error);
	payload["member_count"] = int64_t(3);
	payload["registration_serial"] = int64_t(p_failure.registration_serial);
	payload["schema"] = "device-exit-core-failure/v1";

	const Error err = pack_angecap1_frame(
			ANGECAP1_WIRE_VERSION_FAILURE_V2,
			ANGECAP_FRAME_CORE_FAILURE,
			p_failure.child_serial,
			1,
			payload,
			r_out_stream,
			nullptr);
	if (err != OK) {
		r_out_stream.clear();
		return err;
	}
	return OK;
}

Error verify_unload_capsule_stream(
		const Vector<uint8_t> &p_stream,
		VerifiedCapsuleSummary &r_summary) {
	r_summary = VerifiedCapsuleSummary();
	Vector<DecodedAngeCapFrame> frames;
	const Error dec_err = decode_angecap1_stream(p_stream, frames);
	if (dec_err != OK || frames.is_empty()) {
		return dec_err == OK ? ERR_INVALID_DATA : dec_err;
	}

	const uint32_t wire_version = frames[0].wire_version;
	const uint64_t child_serial = frames[0].child_serial;

	if (wire_version == ANGECAP1_WIRE_VERSION_FAILURE_V2) {
		if (frames.size() != 1 || frames[0].frame_kind != ANGECAP_FRAME_CORE_FAILURE) {
			return ERR_INVALID_DATA;
		}
		const Dictionary &d = frames[0].payload;
		String schema;
		String origin;
		String stream_ref;
		String close_disp;
		uint64_t member_count = 0;
		uint64_t reg_serial = 0;
		int64_t failure_err = 0;
		if (!_dict_get_nonempty_string(d, "schema", schema) ||
				schema != "device-exit-core-failure/v1" ||
				!_dict_get_uint64(d, "member_count", member_count) ||
				member_count != 3 ||
				!_dict_get_uint64(d, "registration_serial", reg_serial) ||
				reg_serial == 0 ||
				!_dict_get_int64(d, "failure_error", failure_err) ||
				failure_err == int64_t(OK) ||
				!_dict_get_nonempty_string(d, "CoreFailureOrigin", origin) ||
				!_dict_get_nonempty_string(d, "CoreClose_source_stream_ref", stream_ref) ||
				!_dict_get_nonempty_string(d, "CoreFailureClose", close_disp)) {
			return ERR_INVALID_DATA;
		}
		r_summary.wire_version = wire_version;
		r_summary.child_serial = child_serial;
		r_summary.is_core_failure = true;
		return OK;
	}

	if (wire_version != ANGECAP1_WIRE_VERSION_COMPLETE_DEINIT_V3) {
		return ERR_INVALID_DATA;
	}

	// Ordinary v3 complete-deinit stream:
	// >= 2 Batch frames (PreClose + PostClose) followed by exactly 1 final ChildSeal.
	if (frames.size() < 3) {
		return ERR_INVALID_DATA;
	}

	const int64_t batch_frame_count = frames.size() - 1;
	uint64_t total_events = 0;
	bool saw_preclose = false;
	bool saw_postclose = false;
	uint64_t postclose_leaked_rids = UINT64_MAX;
	uint64_t postclose_leaked_budget = UINT64_MAX;
	uint64_t postclose_foreign_violations = UINT64_MAX;
	uint64_t postclose_unsettled_fences = UINT64_MAX;

	for (int64_t i = 0; i < batch_frame_count; i++) {
		if (frames[i].frame_kind != ANGECAP_FRAME_BATCH) {
			return ERR_INVALID_DATA;
		}
		const Dictionary &b = frames[i].payload;
		String schema;
		String phase;
		uint64_t batch_seq = 0;
		uint64_t event_count = 0;
		uint64_t reg_serial = 0;
		if (!_dict_get_nonempty_string(b, "schema", schema) ||
				schema != "ange-core-evidence-batch/v3" ||
				!_dict_get_nonempty_string(b, "phase", phase) ||
				!_dict_get_uint64(b, "batch_seq", batch_seq) ||
				batch_seq != uint64_t(i + 1) ||
				!_dict_get_uint64(b, "event_count", event_count) ||
				event_count == 0 ||
				!_dict_get_uint64(b, "registration_serial", reg_serial) ||
				reg_serial == 0) {
			return ERR_INVALID_DATA;
		}
		total_events += event_count;

		if (phase == "PreClose") {
			bool close_after_ack = false;
			uint64_t ack_seq = 0;
			if (saw_postclose ||
					!_dict_get_bool(b, "close_begin_after_committed_ack", close_after_ack) ||
					!close_after_ack ||
					!_dict_get_uint64(b, "preclose_ack_sequence", ack_seq) ||
					ack_seq == 0) {
				return ERR_INVALID_DATA;
			}
			saw_preclose = true;
		} else if (phase == "CorePostDeinit" || phase == "PostClose") {
			if (!saw_preclose || saw_postclose) {
				return ERR_INVALID_DATA;
			}
			bool os_attempted = false;
			bool os_returned = false;
			bool dso_resident = true;
			int64_t os_err = -1;
			String os_disp;
			String dso_close_state;
			if (!_dict_get_bool(b, "os_close_attempted", os_attempted) ||
					!_dict_get_bool(b, "os_close_returned", os_returned) ||
					!_dict_get_int64(b, "os_close_error", os_err) ||
					!_dict_get_bool(b, "dso_resident_after_dlclose", dso_resident) ||
					!_dict_get_nonempty_string(b, "os_disposition", os_disp) ||
					!_dict_get_nonempty_string(b, "dso_close_state", dso_close_state) ||
					!_dict_get_uint64(b, "leaked_rendering_device_rids", postclose_leaked_rids) ||
					!_dict_get_uint64(b, "leaked_u08_budget_bytes", postclose_leaked_budget) ||
					!_dict_get_uint64(b, "foreign_lifetime_violation_count", postclose_foreign_violations) ||
					!_dict_get_uint64(b, "unsettled_fences", postclose_unsettled_fences)) {
				return ERR_INVALID_DATA;
			}
			if (!os_attempted || !os_returned || os_err != int64_t(OK) ||
					dso_resident || os_disp != "OSClosed" || dso_close_state != "OSClosed") {
				return ERR_UNAUTHORIZED;
			}
			if (postclose_leaked_rids != 0 ||
					postclose_leaked_budget != 0 ||
					postclose_foreign_violations != 0 ||
					postclose_unsettled_fences != 0) {
				return ERR_UNAUTHORIZED;
			}
			saw_postclose = true;
		} else {
			return ERR_INVALID_DATA;
		}
	}

	if (!saw_preclose || !saw_postclose) {
		return ERR_INVALID_DATA;
	}

	const DecodedAngeCapFrame &seal = frames[batch_frame_count];
	if (seal.frame_kind != ANGECAP_FRAME_CHILD_SEAL) {
		return ERR_INVALID_DATA;
	}
	String seal_schema;
	uint64_t seal_batch_count = 0;
	uint64_t seal_event_count = 0;
	if (!_dict_get_nonempty_string(seal.payload, "schema", seal_schema) ||
			seal_schema != "ange-core-evidence-seal/v3" ||
			!_dict_get_uint64(seal.payload, "batch_count", seal_batch_count) ||
			seal_batch_count != uint64_t(batch_frame_count) ||
			!_dict_get_uint64(seal.payload, "event_count", seal_event_count) ||
			seal_event_count != total_events ||
			!seal.payload.has("batch_digests") ||
			seal.payload["batch_digests"].get_type() != Variant::ARRAY) {
		return ERR_INVALID_DATA;
	}
	const Array digests = Array(seal.payload["batch_digests"]);
	if (digests.size() != batch_frame_count) {
		return ERR_INVALID_DATA;
	}
	for (int64_t i = 0; i < batch_frame_count; i++) {
		if (digests[i].get_type() != Variant::STRING ||
				String(digests[i]) != frames[i].frame_digest_hex) {
			return ERR_INVALID_DATA;
		}
	}

	r_summary.wire_version = wire_version;
	r_summary.child_serial = child_serial;
	r_summary.batch_count = uint32_t(seal_batch_count);
	r_summary.event_count = uint32_t(seal_event_count);
	r_summary.is_core_failure = false;
	r_summary.os_closed_verified = true;
	r_summary.leaked_rendering_device_rids = postclose_leaked_rids;
	r_summary.leaked_u08_budget_bytes = postclose_leaked_budget;
	r_summary.foreign_lifetime_violation_count = postclose_foreign_violations;
	r_summary.unsettled_fences = postclose_unsettled_fences;
	return OK;
}

Error write_unload_capsule_stream_atomic(const String &p_out_path, const Vector<uint8_t> &p_stream) {
	if (p_out_path.is_empty() || p_stream.is_empty()) {
		return ERR_INVALID_PARAMETER;
	}
	const CharString final_utf8 = p_out_path.utf8();
	const String tmp_path = p_out_path + ".tmp";
	const CharString tmp_utf8 = tmp_path.utf8();

#if defined(UNIX_ENABLED) || defined(MACOS_ENABLED)
	auto reject_non_regular_lstat = [](const char *p_path_utf8) -> Error {
		struct stat st = {};
		if (::lstat(p_path_utf8, &st) == 0) {
			if (!S_ISREG(st.st_mode)) {
				return ERR_UNAUTHORIZED;
			}
		} else if (errno != ENOENT) {
			return ERR_CANT_OPEN;
		}
		return OK;
	};

	Error path_err = reject_non_regular_lstat(final_utf8.get_data());
	if (path_err != OK) {
		return path_err;
	}
	// Pre-unlink any existing regular file at p_out_path so a failed write or tmp open never leaves a stale capsule.
	if (::unlink(final_utf8.get_data()) != 0 && errno != ENOENT) {
		return ERR_FILE_CANT_WRITE;
	}
	path_err = reject_non_regular_lstat(tmp_utf8.get_data());
	if (path_err != OK) {
		return path_err;
	}

	int flags = O_WRONLY | O_CREAT | O_TRUNC;
#ifdef O_NOFOLLOW
	flags |= O_NOFOLLOW;
#endif
#ifdef O_CLOEXEC
	flags |= O_CLOEXEC;
#endif
	const int fd = ::open(tmp_utf8.get_data(), flags, 0600);
	if (fd < 0) {
		return errno == ELOOP ? ERR_UNAUTHORIZED : ERR_CANT_OPEN;
	}
	struct stat fd_st = {};
	if (::fstat(fd, &fd_st) != 0 || !S_ISREG(fd_st.st_mode)) {
		::close(fd);
		return ERR_UNAUTHORIZED;
	}

	const uint8_t *buf = p_stream.ptr();
	size_t remaining = size_t(p_stream.size());
	while (remaining > 0) {
		const ssize_t n = ::write(fd, buf, remaining);
		if (n < 0) {
			if (errno == EINTR) {
				continue;
			}
			::close(fd);
			::unlink(tmp_utf8.get_data());
			return ERR_FILE_CANT_WRITE;
		}
		buf += size_t(n);
		remaining -= size_t(n);
	}
	if (::fsync(fd) != 0) {
		::close(fd);
		::unlink(tmp_utf8.get_data());
		return ERR_FILE_CANT_WRITE;
	}
	if (::close(fd) != 0) {
		::unlink(tmp_utf8.get_data());
		return ERR_FILE_CANT_WRITE;
	}

	// Re-check destination immediately before atomic rename so a symlink cannot be swapped in.
	path_err = reject_non_regular_lstat(final_utf8.get_data());
	if (path_err != OK) {
		::unlink(tmp_utf8.get_data());
		return path_err;
	}
	if (std::rename(tmp_utf8.get_data(), final_utf8.get_data()) != 0) {
		::unlink(tmp_utf8.get_data());
		return ERR_FILE_CANT_WRITE;
	}
	return OK;
#else
	std::FILE *fp = std::fopen(tmp_utf8.get_data(), "wb");
	if (!fp) {
		return ERR_CANT_OPEN;
	}
	const size_t written = std::fwrite(p_stream.ptr(), 1, size_t(p_stream.size()), fp);
	const int flush_rc = std::fflush(fp);
	const int close_rc = std::fclose(fp);
	if (written != size_t(p_stream.size()) || flush_rc != 0 || close_rc != 0) {
		std::remove(tmp_utf8.get_data());
		return ERR_FILE_CANT_WRITE;
	}
	if (std::rename(tmp_utf8.get_data(), final_utf8.get_data()) != 0) {
		std::remove(tmp_utf8.get_data());
		return ERR_FILE_CANT_WRITE;
	}
	return OK;
#endif
}

#if defined(TOOLS_ENABLED) || defined(TESTS_ENABLED) || defined(DEBUG_ENABLED)
static String _read_unload_audit_env_var(const char *p_primary, const char *p_fallback = nullptr) {
	const char *names[2] = { p_primary, p_fallback };
	for (const char *name : names) {
		if (!name) {
			continue;
		}
		if (OS::get_singleton()) {
			const String val = OS::get_singleton()->get_environment(name).strip_edges();
			if (!val.is_empty()) {
				return val;
			}
		}
		const char *raw = std::getenv(name);
		if (raw) {
			const String val = String::utf8(raw).strip_edges();
			if (!val.is_empty()) {
				return val;
			}
		}
	}
	return String();
}
#endif

Error Registry::maybe_emit_post_close_capsule(const RecordRef &p_ref, const String &p_resolved_library_path) {
#if !defined(TOOLS_ENABLED) && !defined(TESTS_ENABLED) && !defined(DEBUG_ENABLED)
	(void)p_ref;
	(void)p_resolved_library_path;
	return OK;
#else
	const String out_path = _read_unload_audit_env_var("GODOT_EXTENSION_UNLOAD_AUDIT_OUT", "ANGE_K03_CAPSULE_OUT");
	if (out_path.is_empty()) {
		return OK;
	}
	const String lib_filter = _read_unload_audit_env_var("GODOT_EXTENSION_UNLOAD_AUDIT_MATCH", "ANGE_K03_CAPSULE_LIBRARY_FILTER");
	if (!lib_filter.is_empty() && !p_resolved_library_path.contains(lib_filter)) {
		return OK;
	}

	uint64_t child_serial = 0;
	uint64_t registration_serial = 0;
	uint64_t attachment_serial = 0;
	uint64_t preclose_ack_sequence = 0;
	bool preclose_committed = false;
	bool os_close_attempted = false;
	bool os_close_returned = false;
	Error os_close_error = ERR_UNCONFIGURED;
	Error close_error = OK;
	uint64_t leaked_rids = 0;
	uint64_t leaked_u08_bytes = 0;
	uint64_t foreign_violations = 0;
	uint64_t unsettled_fences = 0;

	{
		if (p_ref.registry != this || p_ref.slot >= RECORD_CAPACITY) {
			return ERR_INVALID_DATA;
		}
		Lock lock(*this);
		const Record &record = records[p_ref.slot];
		if (!record.occupied) {
			return ERR_INVALID_DATA;
		}
		const bool is_dynamic =
				record.close_source == uint32_t(GDExtensionLoader::LoaderKind::DYNAMIC) ||
				record.snapshot.loader_kind == uint32_t(GDExtensionLoader::LoaderKind::DYNAMIC) ||
				!p_resolved_library_path.is_empty();
		if (!is_dynamic) {
			// Skip internal in-process function loaders so they never clobber a real DSO capsule.
			return OK;
		}
		if (lib_filter.is_empty()) {
			// When library filter is unset, only emit for guard-qualified extensions
			// so auxiliary non-guarded helper extensions never overwrite the audited capsule.
			if (record.guard_enrollment != GuardEnrollment::QUALIFIED) {
				return OK;
			}
		}

		child_serial = record.snapshot.library_generation;
		registration_serial = record.producer_registration_serial ? record.producer_registration_serial : (record.guard_token ? record.guard_token : record.snapshot.library_generation);
		attachment_serial = record.last_attachment_serial;
		preclose_ack_sequence = record.evidence_serial;
		preclose_committed = record.evidence_committed && record.evidence_ack_received && preclose_ack_sequence != 0;
		os_close_attempted = record.os_close_attempted;
		os_close_returned = record.os_close_returned;
		os_close_error = record.os_close_error;
		close_error = Error(record.close_error);
		foreign_violations = (close_error != OK ? 1u : 0u) + uint64_t(record.return_pending_holders);

		unsettled_fences = uint64_t(record.active_callbacks) +
				uint64_t(record.binding_holders) +
				uint64_t(record.return_pending_holders) +
				uint64_t(record.cleanup_reservations);

		for (const auto &dref : device_refs) {
			if (dref.serial != 0 && dref.library == p_ref.slot) {
				unsettled_fences++;
			}
		}
		for (const auto *att : production_attachments) {
			if (!att || att->library != p_ref.slot) {
				continue;
			}
			leaked_u08_bytes += att->quote.allocation_bytes;
			unsettled_fences += 1 + uint64_t(att->children);
			if (att->resources != nullptr) {
				for (uint32_t ri = 0; ri < att->quote.capacity.resources; ri++) {
					const ProductionResourceSlot &rslot = att->resources[ri];
					if ((rslot.creating || rslot.owned_serial != 0) && !rslot.returned) {
						leaked_rids++;
					}
				}
			}
		}
	}

#if defined(UNIX_ENABLED) || defined(MACOS_ENABLED)
	{
		const CharString final_utf8 = out_path.utf8();
		struct stat st = {};
		if (::lstat(final_utf8.get_data(), &st) == 0) {
			if (!S_ISREG(st.st_mode)) {
				return ERR_UNAUTHORIZED;
			}
			if (::unlink(final_utf8.get_data()) != 0 && errno != ENOENT) {
				return ERR_FILE_CANT_WRITE;
			}
		} else if (errno != ENOENT) {
			return ERR_CANT_OPEN;
		}
	}
#else
	std::remove(out_path.utf8().get_data());
#endif

	Vector<uint8_t> stream;
	if (!preclose_committed || close_error != OK || !os_close_attempted || !os_close_returned || os_close_error != OK || p_resolved_library_path.is_empty()) {
		CoreFailureWitnessV2 failure;
		failure.child_serial = child_serial ? child_serial : 1u;
		failure.registration_serial = registration_serial ? registration_serial : failure.child_serial;
		failure.failure_error = close_error != OK ? close_error : (os_close_error != OK ? os_close_error : ERR_UNAVAILABLE);
		failure.failure_origin = "gdextension.close_library";
		failure.source_stream_ref = p_resolved_library_path.is_empty() ? String("dynamic_loader") : p_resolved_library_path;
		failure.close_disposition = os_close_attempted ? String("OSCloseFailed") : String("PreOSAbort");
		const Error emit_err = emit_core_failure_capsule_v2(failure, stream);
		if (emit_err != OK) {
			return emit_err;
		}
		return write_unload_capsule_stream_atomic(out_path, stream);
	}

	bool dso_resident = true;
	const CharString lib_utf8 = p_resolved_library_path.utf8();
	const Error probe_err = probe_dso_resident_rtld_noload(lib_utf8.get_data(), dso_resident);
	if (probe_err != OK) {
		CoreFailureWitnessV2 failure;
		failure.child_serial = child_serial;
		failure.registration_serial = registration_serial;
		failure.failure_error = probe_err;
		failure.failure_origin = "gdextension.rtld_noload_probe";
		failure.source_stream_ref = p_resolved_library_path;
		failure.close_disposition = "ProbeError";
		const Error emit_err = emit_core_failure_capsule_v2(failure, stream);
		if (emit_err != OK) {
			return emit_err;
		}
		return write_unload_capsule_stream_atomic(out_path, stream);
	}

	CompleteDeinitClosureV1 closure;
	closure.child_serial = child_serial;
	closure.registration_serial = registration_serial;
	closure.attachment_serial = attachment_serial;
	closure.preclose_ack_sequence = preclose_ack_sequence;
	closure.os_close_attempted = os_close_attempted;
	closure.os_close_returned = os_close_returned;
	closure.os_close_error = os_close_error;
	closure.dso_resident_after_dlclose = dso_resident;
	closure.leaked_rendering_device_rids = leaked_rids;
	closure.leaked_u08_budget_bytes = leaked_u08_bytes;
	closure.foreign_lifetime_violation_count = foreign_violations;
	closure.unsettled_fences = unsettled_fences;

	const Error emit_err = emit_complete_deinit_capsule_v3(closure, stream);
	if (emit_err != OK) {
		return emit_err;
	}
	return write_unload_capsule_stream_atomic(out_path, stream);
#endif
}

} // namespace GDExtensionDeviceExit
