/**************************************************************************/
/*  gdextension_record_source.cpp                                  */
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

#include "core/extension/gdextension_record_source.h"

namespace GDExtensionDeviceExit {
bool valid_original_operation_identity(const OriginalOperationIdentity &id) noexcept {
	return id.owner_request_nonce && id.library_generation &&
			id.qualification_session_serial && id.record_generation &&
			id.record_range_begin < id.record_range_end;
}
bool same_original_operation_identity(const OriginalOperationIdentity &a, const OriginalOperationIdentity &b) noexcept {
	return valid_original_operation_identity(a) && valid_original_operation_identity(b) &&
		a.owner_request_nonce == b.owner_request_nonce &&
		a.library_generation == b.library_generation &&
		a.qualification_session_serial == b.qualification_session_serial &&
		a.record_generation == b.record_generation &&
		a.record_range_begin == b.record_range_begin &&
		a.record_range_end == b.record_range_end;
}
Error bind_checked_record_append(CheckedRecordAppend &append, const OriginalOperationIdentity &identity) noexcept {
	if (!valid_original_operation_identity(identity)) {
		return ERR_INVALID_DATA;
	}
	if (append.operation_bound && !same_original_operation_identity(append.operation, identity)) {
		return ERR_UNAUTHORIZED;
	}
	append.operation = identity;
	append.operation_bound = true;
	return OK;
}
Error retain_native_source(RDD &driver, NativeSourceMember &member) {
	if (member.retained || member.owner_released) {
		return OK;
	}
	if (!member.source.serial || !member.source.queue_index || !member.source.completion_serial || member.source.completion == RDD::CompletionCapability::Disabled || member.source.presentation_only) {
		member.retention_error = ERR_INVALID_DATA;
		return ERR_INVALID_DATA;
	}
	const Error error = driver.native_call_retain(member.source.serial);
	member.retention_error = error;
	if (error == OK) {
		member.retained = true;
	}
	return error;
}
NativeSourceObservation poll_native_source(RDD &driver, NativeSourceMember &member, const NativeSourceOrigin &origin) {
	NativeSourceObservation result;
	if (member.owner_released) {
		result.state = NativeSourceObservation::State::Unknown;
		result.error = ERR_DOES_NOT_EXIST;
		return result;
	}
	const Error retained = retain_native_source(driver, member);
	if (retained != OK) {
		result.retention_error = retained;
		// Retention has its own diagnostic. It cannot replace an existing
		// raw issue cause in the outward original source fact/report.
		result.error = retained == ERR_BUSY || member.source.error == OK ? retained : member.source.error;
		result.native_code = member.source.error == OK ? 0 : member.source.native_code;
		result.state = retained == ERR_BUSY ? NativeSourceObservation::State::Pending : NativeSourceObservation::State::Unknown;
		return result;
	}
	// Both arms observe exactly the retained original serial. No wait, global
	// last-report lookup, tail-fence inference or timeout promotion is allowed.
	const auto observed = driver.native_call_poll(member.source.serial);
	result.completion_native_code = observed.native_code;
	member.completion_native_code = observed.native_code;
	result.native_code = observed.native_code;
	if (member.source.error != OK) {
		// Preserve the immutable issue cause outward as well as in source.
		// A later quiet poll cannot erase it; its own code has a separate cell.
		result.error = member.source.error;
		result.native_code = member.source.native_code;
		result.state = member.source.issue == RDD::IssueFact::KnownAccepted ? NativeSourceObservation::State::NativeError : NativeSourceObservation::State::Unknown;
		return result;
	}
	if (observed.native_error) {
		result.error = FAILED;
		result.state = member.source.issue == RDD::IssueFact::IssuedUnknown ? NativeSourceObservation::State::Unknown : NativeSourceObservation::State::NativeError;
	} else if (observed.matches(member.source.completion_serial)) {
		const bool known = member.source.issue == RDD::IssueFact::KnownAccepted;
		const bool refined = member.source.issue == RDD::IssueFact::IssuedUnknown && observed.resolved_issue == RDD::IssueFact::KnownAccepted;
		result.state = (known || refined) && member.source.error == OK ? NativeSourceObservation::State::Complete : NativeSourceObservation::State::Unknown;
		if (result.state == NativeSourceObservation::State::Unknown) {
			result.native_code = member.source.native_code;
		}
		if (result.state == NativeSourceObservation::State::Complete) {
			result.resolved_issue = RDD::IssueFact::KnownAccepted;
		}
	}
	// Tag is deliberately not a completion override. In particular qualification
	// cannot authorize production, and production cannot weaken source checking.
	(void)origin;
	return result;
}
Error release_native_source(RDD &driver, NativeSourceMember &member) {
	if (!member.retained || member.owner_released) {
		return OK;
	}
	const Error error = driver.native_call_release(member.source.serial);
	if (error == OK) {
		member.owner_released = true;
	}
	return error;
}
Error record_source_slot_bound(uint32_t contexts, uint64_t graphs, uint32_t current, uint32_t workers, uint32_t &slots, uint32_t &frames) {
	slots = frames = 0;
	if (!contexts || !graphs) {
		return ERR_UNAVAILABLE;
	}
	if (graphs > UINT32_MAX || graphs > UINT32_MAX - uint64_t(contexts)) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	const uint64_t f = graphs + contexts;
	const uint64_t base = uint64_t(current) + graphs;
	if (base > UINT32_MAX || uint64_t(workers) + 1 > (UINT32_MAX - base) / f) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	frames = uint32_t(f);
	slots = uint32_t(base + f * (uint64_t(workers) + 1));
	return OK;
}
Error qualification_source_slot_bound(uint64_t graphs, uint32_t current, uint32_t workers, uint32_t capacity, uint32_t &slots) {
	slots = 0;
	if (graphs > capacity || graphs == UINT64_MAX) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	const uint64_t frames = graphs + 1;
	const uint64_t base = uint64_t(current) + frames;
	if (base > capacity || workers > (capacity - base) / frames) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	slots = uint32_t(base + uint64_t(workers) * frames);
	return OK;
}
bool same_request_readback_complete(const ReadbackSourceState &source, uint64_t request, uint64_t backing, uint64_t offset, uint64_t size) {
	return request && backing && size && offset <= UINT64_MAX - size && source.request_serial == request && source.backing_serial == backing && source.offset == offset && source.size == size && source.issued == size && source.completed == size && source.owned_bytes == size && source.error == OK && source.delivered;
}
} // namespace GDExtensionDeviceExit
