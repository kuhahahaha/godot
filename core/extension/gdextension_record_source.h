/**************************************************************************/
/*  gdextension_record_source.h                                           */
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

#include "servers/rendering/rendering_device_driver.h"
#include "servers/rendering/rendering_device_graph.h"

class TestRecordSourceAccess;
namespace GDExtensionDeviceExit {
class Registry;
struct QualificationTicket;
// Origin belongs to the original Registry attachment. Copying source facts
// never changes this tag and never creates operation/scope/receipt authority.
class NativeSourceOrigin {
	friend class Registry;
	friend struct QualificationTicket;
#ifdef TESTS_ENABLED
	friend class ::TestRecordSourceAccess;
#endif
	enum class Kind : uint8_t { Qualification,
		Production };
	const Kind kind;
	explicit NativeSourceOrigin(Kind p_kind) : kind(p_kind) {}
	static NativeSourceOrigin qualification() { return NativeSourceOrigin(Kind::Qualification); }
	static NativeSourceOrigin production() { return NativeSourceOrigin(Kind::Production); }

public:
	NativeSourceOrigin() = delete;
	NativeSourceOrigin(const NativeSourceOrigin &) = default;
	bool is_qualification() const { return kind == Kind::Qualification; }
};
// The existing ticket's source membership row, now shared mechanical storage.
// Original source facts are copied at the actual issue site and never rewritten
// by completion observation. Recording and native serials remain separate.
struct NativeSourceMember {
	RDD::NativeCallFact source;
	uint32_t fact = 0;
	uint32_t frame = 0;
	uint32_t slot = UINT32_MAX;
	uint64_t epoch = 0;
	bool retained = false;
	bool owner_released = false;
	int64_t completion_native_code = 0;
	Error retention_error = OK;
};
struct NativeSourceObservation {
	enum class State : uint8_t { Pending,
		Complete,
		NativeError,
		Unknown };
	State state = State::Pending;
	Error error = OK;
	int64_t native_code = 0;
	int64_t completion_native_code = 0;
	RDD::IssueFact resolved_issue = RDD::IssueFact::NotIssued;
	Error retention_error = OK;
};
// Private core helpers consumed by the original RD/ticket owner. They carry no
// public API, callbacks, caller-authored proof, handle issuer or source registry.
Error retain_native_source(RDD &, NativeSourceMember &);
NativeSourceObservation poll_native_source(RDD &, NativeSourceMember &, const NativeSourceOrigin &);
Error release_native_source(RDD &, NativeSourceMember &);
// Checked admission arithmetic only. Actual driver bound remains independent.
Error record_source_slot_bound(uint32_t p_contexts, uint64_t p_graphs, uint32_t p_current_commands, uint32_t p_workers, uint32_t &r_slots, uint32_t &r_frames);
// The qualification policy retains its historical slot formula/cap. Sharing
// overflow arithmetic does not make that fixed policy a production profile.
Error qualification_source_slot_bound(uint64_t p_graphs, uint32_t p_current_commands, uint32_t p_workers, uint32_t p_capacity, uint32_t &r_slots);
struct ReadbackSourceState {
	uint64_t request_serial = 0;
	uint64_t backing_serial = 0;
	uint64_t offset = 0;
	uint64_t size = 0;
	uint64_t issued = 0;
	uint64_t completed = 0;
	uint64_t owned_bytes = 0;
	Error error = OK;
	bool delivered = false;
};
bool same_request_readback_complete(const ReadbackSourceState &, uint64_t p_request, uint64_t p_backing, uint64_t p_offset, uint64_t p_size);
struct CheckedRecordAppend {
	enum class State : uint8_t { Rejected,
		Appended,
		Unknown };
	State state = State::Rejected;
	Error error = OK;
	RDG::RecordHandle handle;
};
} // namespace GDExtensionDeviceExit
