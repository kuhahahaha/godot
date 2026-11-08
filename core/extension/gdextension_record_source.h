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
		Production,
		SystemScalar };
	const Kind kind;
	explicit NativeSourceOrigin(Kind p_kind) : kind(p_kind) {}
	static NativeSourceOrigin qualification() { return NativeSourceOrigin(Kind::Qualification); }
	static NativeSourceOrigin production() { return NativeSourceOrigin(Kind::Production); }
	static NativeSourceOrigin system_scalar() { return NativeSourceOrigin(Kind::SystemScalar); }

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
struct NativeSourceIssueView {
	const RDD::QueueCallReport &report;
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
	// Independent raw completion failure, even when issue.error takes precedence.
	Error completion_error = OK;
};
// Immutable identity retained by the original R01 owner across checked
// admission, native completion, and exact readback. RD handles and serials
// cannot substitute for these owner-issued fields.
struct OriginalOperationIdentity {
	uint64_t owner_request_nonce = 0;
	uint64_t library_generation = 0;
	uint64_t qualification_session_serial = 0;
	uint64_t record_generation = 0;
	uint64_t record_range_begin = 0;
	uint64_t record_range_end = 0;
};
bool valid_original_operation_identity(const OriginalOperationIdentity &) noexcept;
bool same_original_operation_identity(const OriginalOperationIdentity &, const OriginalOperationIdentity &) noexcept;
// Private core helpers consumed by the original RD/ticket owner. They carry no
// public API, callbacks, caller-authored proof, handle issuer or source registry.
Error retain_native_source(RDD &, NativeSourceMember &);
NativeSourceObservation poll_native_source(RDD &, NativeSourceMember &, const NativeSourceOrigin &);
Error release_native_source(RDD &, NativeSourceMember &);
// Checked admission arithmetic only. Actual driver bound remains independent.
// A finite bound from the original 32-byte staging allocation contract.
Error production_staging_graph_bound(uint64_t p_bytes, uint32_t p_block_size, uint32_t &r_graphs);
Error record_source_slot_bound(uint32_t p_contexts, uint64_t p_graphs, uint32_t p_current_commands, uint32_t p_workers, uint32_t &r_slots, uint32_t &r_frames);
// The qualification policy retains its historical slot formula/cap. Sharing
// overflow arithmetic does not make that fixed policy a production profile.
Error qualification_source_slot_bound(uint64_t p_graphs, uint32_t p_current_commands, uint32_t p_workers, uint32_t p_capacity, uint32_t &r_slots);
// Append-only qualification receipt credits. This is accounting inside the
// original ticket; it cannot issue a source, graph, operation or observation.
// Every mutation is made under Registry::mutex before its corresponding effect.
struct QualificationFactCredits {
 uint32_t graphs = 0;
 uint32_t main = 0;
 uint32_t copy = 0;
 uint32_t workers = 0;
 uint32_t cleanup = 0;
 uint32_t pending = 0;
 uint32_t committed = 0;
 uint32_t capacity = 0;
 Error initialize(uint32_t p_capacity, uint64_t p_graphs, uint64_t p_main, uint32_t p_cleanup, uint64_t p_copy = 0);
 Error grow_plan(uint64_t p_graphs, uint64_t p_main, uint64_t p_copy = 0);
 Error reserve_workers(uint32_t p_count);
 Error admit_native(uint32_t p_count, bool p_worker, bool p_copy = false);
 Error commit_native(uint32_t p_count);
 Error cancel_native(uint32_t p_count,bool p_worker,bool p_copy = false);
 Error commit_graph();
 Error release_worker_quote(uint32_t p_count);
 uint64_t occupied() const;
};
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
	// Actual original Frame delivery-entry returns, after any map/unmap work.
	// Errors may return an entry without establishing CPU byte readiness.
	uint64_t returned = 0;
};
bool same_request_readback_complete(const ReadbackSourceState &, uint64_t p_request, uint64_t p_backing, uint64_t p_offset, uint64_t p_size);
// Bounded original source capture. Caller supplies arena cells before any
// staging/transfer effect; these fields never grant recording permission.
struct BufferUploadSourceSegment {
	RDG::RecordHandle handle;
	uint32_t begin = 0, end = 0;
};
struct BufferUploadSourceCapture {
	BufferUploadSourceSegment *segments = nullptr;
	uint32_t segment_stride = sizeof(BufferUploadSourceSegment);
	BufferUploadSourceSegment &segment(uint32_t index) const {
		return *reinterpret_cast<BufferUploadSourceSegment *>(reinterpret_cast<uint8_t *>(segments) + uint64_t(index) * segment_stride);
	}
	uint32_t capacity = 0, count = 0, consumed = 0;
	uint32_t transfer_worker = UINT32_MAX;
	uint64_t transfer_operation = 0;
	bool became_mutable = false, prefix_possible = false;
};
struct BufferReadbackSourceCapture {
	Registry *registry = nullptr;
	uint64_t request_serial = 0;
	ReadbackSourceState *state = nullptr;
	uint8_t *bytes = nullptr;
	uint64_t byte_capacity = 0;
	BufferUploadSourceSegment *segments = nullptr;
	uint32_t segment_stride = sizeof(BufferUploadSourceSegment);
	BufferUploadSourceSegment &segment(uint32_t index) const {
		return *reinterpret_cast<BufferUploadSourceSegment *>(reinterpret_cast<uint8_t *>(segments) + uint64_t(index) * segment_stride);
	}
	uint32_t capacity = 0, count = 0;
	uint32_t transfer_worker = UINT32_MAX;
	uint64_t transfer_operation = 0;
	bool became_mutable = false;
	int64_t native_code = 0;
};
struct CheckedRecordAppend {
	enum class State : uint8_t { Rejected,
		Appended,
		Unknown,
		KnownPrefix };
	State state = State::Rejected;
	Error error = OK;
	RDG::RecordHandle handle;
	struct Transfer {
		uint32_t worker = 0;
		uint64_t operation = 0;
	};
	Transfer transfers[2];
	uint32_t transfer_count = 0;
	bool became_mutable = false;
	OriginalOperationIdentity operation;
	bool operation_bound = false;
};
// Binds the append observation to the original owner identity. This is an
// internal join only; it does not create an identity from an RD handle.
Error bind_checked_record_append(CheckedRecordAppend &, const OriginalOperationIdentity &) noexcept;
} // namespace GDExtensionDeviceExit
