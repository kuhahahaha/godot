/**************************************************************************/
/*  gdextension_production_legacy.cpp                                     */
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

#include "core/extension/gdextension_production_legacy_projection.h"
namespace GDExtensionDeviceExit {
namespace {
template <class T>
bool legacy_aligned(const T *p) {
	return p && uintptr_t(p) % alignof(T) == 0;
}
bool legacy_separate(const void *a, size_t an, const void *b, size_t bn) {
	auto x = uintptr_t(a), y = uintptr_t(b);
	return x <= y ? an <= y - x : bn <= x - y;
}
GDExtensionDeviceSessionResultV1 legacy_error(Error e, uint32_t reason = 0) {
	return { int32_t(e), reason, 0, 0 };
}
} //namespace
GDExtensionDeviceSessionResultV1 Registry::production_prepare_dispatch(GDExtensionProductionRequestRefV1 ref, const GDExtensionProductionDispatchV1 *input, const void *push, uint32_t bytes) {
	if (!legacy_aligned(input) || !production_dto_valid(*input) || input->reserved || input->push_bytes != bytes || (bytes && !push) || !input->groups_x || !input->groups_y || !input->groups_z) {
		return legacy_error(ERR_INVALID_PARAMETER, 2);
	}
	uint64_t begin = 0;
	const Error checked = production_contract_check(ref, 1, &begin, nullptr, true);
	if (checked != OK) {
		return legacy_error(checked, 16);
	}
	GDExtensionProductionDispatchV2 value = {};
	value.abi = production_dto_abi<GDExtensionProductionDispatchV2>();
	value.action_ordinal = input->canonical_suboperation >= begin ? input->canonical_suboperation - begin : UINT64_MAX;
	value.canonical_suboperation = input->canonical_suboperation;
	value.unit_slot = input->unit_slot;
	value.first_set_slot = input->first_set_slot;
	value.set_count = input->set_count;
	value.groups_x = input->groups_x;
	value.groups_y = input->groups_y;
	value.groups_z = input->groups_z;
	value.push_bytes = input->push_bytes;
	value.reserved = input->reserved;
	return production_prepare_dispatch_v2(ref, &value, push, bytes, 1);
}
GDExtensionDeviceSessionResultV1 Registry::production_prepare_copy(GDExtensionProductionRequestRefV1 ref, const GDExtensionProductionCopyV1 *input) {
	if (!legacy_aligned(input) || !production_dto_valid(*input) || !input->bytes) {
		return legacy_error(ERR_INVALID_PARAMETER, 2);
	}
	uint64_t begin = 0;
	const Error checked = production_contract_check(ref, 1, &begin, nullptr, true);
	if (checked != OK) {
		return legacy_error(checked, 16);
	}
	GDExtensionProductionCopyV2 value = {};
	value.abi = production_dto_abi<GDExtensionProductionCopyV2>();
	value.action_ordinal = input->canonical_suboperation >= begin ? input->canonical_suboperation - begin : UINT64_MAX;
	value.canonical_suboperation = input->canonical_suboperation;
	value.source_slot = input->source_slot;
	value.destination_slot = input->destination_slot;
	value.source_offset = input->source_offset;
	value.destination_offset = input->destination_offset;
	value.bytes = input->bytes;
	return production_prepare_copy_v2(ref, &value, 1);
}
GDExtensionDeviceSessionResultV1 Registry::production_prepare_upload(GDExtensionProductionRequestRefV1 ref, const GDExtensionProductionUploadV1 *input, const void *data, uint64_t bytes) {
	if (!legacy_aligned(input) || !production_dto_valid(*input) || input->reserved || !bytes || !data || bytes != input->bytes || bytes > SIZE_MAX) {
		return legacy_error(ERR_INVALID_PARAMETER, 2);
	}
	uint64_t begin = 0;
	const Error checked = production_contract_check(ref, 1, &begin, nullptr, true);
	if (checked != OK) {
		return legacy_error(checked, 16);
	}
	GDExtensionProductionUploadV2 value = {};
	value.abi = production_dto_abi<GDExtensionProductionUploadV2>();
	value.action_ordinal = input->canonical_suboperation >= begin ? input->canonical_suboperation - begin : UINT64_MAX;
	value.canonical_suboperation = input->canonical_suboperation;
	value.destination_slot = input->destination_slot;
	value.reserved = input->reserved;
	value.destination_offset = input->destination_offset;
	value.bytes = input->bytes;
	return production_prepare_upload_v2(ref, &value, data, bytes, 1);
}
GDExtensionDeviceSessionResultV1 Registry::production_prepare_readback(GDExtensionProductionRequestRefV1 ref, uint64_t canonical, uint32_t slot) {
	uint64_t begin = 0;
	const Error e = production_contract_check(ref, 1, &begin, nullptr, true);
	if (e != OK) {
		return legacy_error(e, 16);
	}
	return production_prepare_readback_v2(ref, canonical >= begin ? canonical - begin : UINT64_MAX, slot, 1);
}
GDExtensionDeviceSessionResultV1 Registry::production_observe(GDExtensionProductionRequestRefV1 ref, const GDExtensionProductionServiceBudgetV1 *budget, GDExtensionProductionReportV1 *out) {
	if (!legacy_aligned(budget) || !production_dto_valid(*budget) || budget->reserved || !legacy_aligned(out)) {
		return legacy_error(ERR_INVALID_PARAMETER, 2);
	}
	const Error checked = production_contract_check(ref, 1);
	if (checked != OK) {
		return legacy_error(checked, 16);
	}
	const Error service = production_observe_service(ref, *budget);
	Lock lock(*this);
	ProductionRequest *r = nullptr;
	RenderingDevice *rd = nullptr;
	Error e = production_request_locked(ref, false, r, rd);
	if (e != OK) {
		return legacy_error(e, 16);
	}
	production_report_locked(*r, *out);
	return legacy_error(service);
}
GDExtensionDeviceSessionResultV1 Registry::production_record_consume(GDExtensionProductionRequestRefV1 ref, uint64_t serial, GDExtensionProductionRecordReportV1 *out) {
	if (!legacy_aligned(out)) {
		return legacy_error(ERR_INVALID_PARAMETER);
	}
	uint64_t begin = 0;
	bool resolved = false;
	const Error e = production_contract_check(ref, 1, &begin, &resolved);
	if (e != OK) {
		if (!resolved) {
			*out = {};
		}
		return legacy_error(e);
	}
	GDExtensionProductionRecordReportV2 value = {};
	const auto result = production_record_consume_v2(ref, serial, &value, 1);
	*out = production_legacy_projection(value, begin);
	return result;
}
GDExtensionDeviceSessionResultV1 Registry::production_release(GDExtensionProductionRequestRefV1 ref, const GDExtensionProductionServiceBudgetV1 *input, GDExtensionProductionServiceBudgetV1 *remaining, GDExtensionProductionReportV1 *report) {
	if (!legacy_aligned(input) || !production_dto_valid(*input) || input->reserved || !legacy_aligned(remaining) || !legacy_aligned(report) || !legacy_separate(input, sizeof(*input), remaining, sizeof(*remaining)) || !legacy_separate(input, sizeof(*input), report, sizeof(*report)) || !legacy_separate(remaining, sizeof(*remaining), report, sizeof(*report))) {
		return legacy_error(ERR_INVALID_PARAMETER, 19);
	}
	uint64_t begin = 0;
	bool resolved = false;
	const Error e = production_contract_check(ref, 1, &begin, &resolved);
	if (e != OK) {
		if (!resolved) {
			*remaining = *input;
			*report = {};
		}
		return legacy_error(e, 16);
	}
	GDExtensionProductionReportV2 value = {};
	const auto result = production_release_v2(ref, input, remaining, &value, 1);
	*report = production_legacy_projection(value, begin);
	return result;
}

Error Registry::production_report_contents_locked(const ProductionRequest &r, GDExtensionProductionReportV1 &out) const {
	if (r.contract_version != 1) {
		return ERR_INVALID_DATA;
	}
	GDExtensionProductionReportV2 value = {};
	const Error error = production_report_contents_locked(r, value);
	out = production_legacy_projection(value, r.operation.canonical_begin);
	return error;
}
void Registry::production_report_locked(const ProductionRequest &r, GDExtensionProductionReportV1 &out) const {
	out = {};
	if (r.contract_version != 1) {
		out.cause = legacy_error(ERR_INVALID_DATA);
		return;
	}
	GDExtensionProductionReportV2 value = {};
	production_report_locked(r, value);
	out = production_legacy_projection(value, r.operation.canonical_begin);
}
Error Registry::production_close_segment_locked(ProductionRequest &r, GDExtensionProductionSegmentV1 &out, bool exit) {
	if (r.contract_version != 1) {
		return ERR_INVALID_DATA;
	}
	GDExtensionProductionSegmentV2 value = {};
	const Error error = production_close_segment_locked(r, value, exit);
	if (value.abi.abi_version) {
		out = production_legacy_projection(value, r.operation.canonical_begin);
	}
	return error;
}
GDExtensionDeviceSessionResultV1 Registry::production_prepare_upload_locked(ProductionRequest &r, const GDExtensionProductionUploadV1 &input, const void *data, uint64_t bytes) {
	if (!production_dto_valid(input) || input.reserved || !bytes || !data || bytes != input.bytes || bytes > SIZE_MAX) {
		return legacy_error(ERR_INVALID_PARAMETER, 2);
	}
	if (r.contract_version != 1) {
		return legacy_error(ERR_INVALID_DATA, 16);
	}
	GDExtensionProductionUploadV2 value = {};
	value.abi = production_dto_abi<GDExtensionProductionUploadV2>();
	value.action_ordinal = input.canonical_suboperation >= r.operation.canonical_begin ? input.canonical_suboperation - r.operation.canonical_begin : UINT64_MAX;
	value.canonical_suboperation = input.canonical_suboperation;
	value.destination_slot = input.destination_slot;
	value.reserved = input.reserved;
	value.destination_offset = input.destination_offset;
	value.bytes = input.bytes;
	return production_prepare_upload_locked(r, value, data, bytes);
}
} //namespace GDExtensionDeviceExit
