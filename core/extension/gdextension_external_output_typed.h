/**************************************************************************/
/*  gdextension_external_output_typed.h                                   */
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

/* Four-operation payload subset. Acquire/poll results and external registration remain pending. */
#ifndef GDEXTENSION_EXTERNAL_OUTPUT_TYPED_V1_H
#define GDEXTENSION_EXTERNAL_OUTPUT_TYPED_V1_H
#include "gdextension_interface.gen.h"

#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	uint8_t bytes[32];
} GDExtensionOutputTokenV1;
typedef struct {
	double position[3];
	double size[3];
} GDExtensionOutputBoundsV1;
typedef struct {
	uint64_t view_id, view_epoch, capture_ordinal;
} GDExtensionOutputCameraV1;
typedef struct {
	const uint64_t *data;
	uint32_t count, reserved;
} GDExtensionOutputRidSpanV1;

enum {
	GDEXTENSION_OUTPUT_ROLE_INVALID_V1 = 0,
	GDEXTENSION_OUTPUT_ROLE_CURRENT_RECORDS_V1 = 1,
	GDEXTENSION_OUTPUT_ROLE_ALIGNED_PREVIOUS_RECORDS_V1 = 2,
	GDEXTENSION_OUTPUT_ROLE_CURRENT_VERTICES_V1 = 3,
	GDEXTENSION_OUTPUT_ROLE_ALIGNED_PREVIOUS_VERTICES_V1 = 4,
	GDEXTENSION_OUTPUT_ROLE_VERTEX_HISTORY_VALIDITY_V1 = 5,
	GDEXTENSION_OUTPUT_ROLE_INDEX_STAGING_V1 = 6,
	GDEXTENSION_OUTPUT_ROLE_OUTPUT_META_V1 = 7,
	GDEXTENSION_OUTPUT_ACCESS_INVALID_V1 = 0,
	GDEXTENSION_OUTPUT_ACCESS_WRITE_ONLY_V1 = 1,
	GDEXTENSION_OUTPUT_SEAL_INVALID_V1 = 0,
	GDEXTENSION_OUTPUT_SEAL_RECORDED_ORDERED_V1 = 1
};

typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint32_t profile, capacity, max_views, max_inflight;
	uint32_t space, pass_mask, motion, query_enabled;
	GDExtensionOutputRidSpanV1 surfaces, surface_materials;
	GDExtensionOutputBoundsV1 bounds, geometry_bounds, query_bounds;
	GDExtensionOutputTokenV1 producer_tag, device_tag, layout_digest, mapping_digest, numeric_profile_digest;
} GDExtensionOutputCreateInputV1;

typedef struct {
	uint64_t generation, present_seq;
	GDExtensionOutputTokenV1 output_tag, view_tag;
	GDExtensionOutputBoundsV1 bounds;
	uint32_t history_reset, has_actual_previous_camera, has_previous_consumed_tuple, reserved;
	GDExtensionOutputCameraV1 current_camera, actual_previous_camera;
	GDExtensionOutputTokenV1 current_host_token, previous_consumed_tuple_token;
} GDExtensionOutputCommonInputV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint64_t particles;
	GDExtensionOutputCommonInputV1 common;
	uint64_t content_version, view_epoch;
	uint32_t pass_mask, reserved;
	GDExtensionOutputTokenV1 layout_digest, mapping_digest, source_token;
} GDExtensionOutputAcquireInputV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint64_t particles;
	GDExtensionOutputTokenV1 lease_token;
	GDExtensionOutputCommonInputV1 common;
	GDExtensionOutputTokenV1 record_token;
} GDExtensionOutputSealInputV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint64_t particles;
	GDExtensionOutputTokenV1 lease_token;
} GDExtensionOutputCancelInputV1;

typedef struct {
	GDExtensionDeviceExitAbi abi;
	uint64_t particles, generation;
} GDExtensionOutputGenerationInputV1;

/* Detail is static engine-lifetime UTF-8; never points into temporary results. */
typedef struct {
	int32_t error;
	uint32_t detail_length;
	const char *detail;
} GDExtensionOutputStatusV1;
typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionOutputStatusV1 status;
	uint64_t particles, generation;
	uint32_t capabilities, reserved;
} GDExtensionOutputCreateResultV1;
typedef struct {
	uint32_t role, access;
	uint64_t rid, allocation_generation;
	uint32_t byte_length, stride;
} GDExtensionOutputWritableV1;
typedef struct {
	GDExtensionDeviceExitAbi abi;
	GDExtensionOutputStatusV1 status;
	uint32_t state, reserved;
	GDExtensionOutputTokenV1 packet_token;
} GDExtensionOutputSealResultV1;

/* Acquire/poll result ABI and call leaves are intentionally not declared by this four-operation subset. */
typedef void (*GDExtensionInterfaceOutputCreateV1)(GDExtensionProductionPhysicalContextRefV1, GDExtensionProductionRequestRefV1, const GDExtensionOutputCreateInputV1 *, GDExtensionOutputCreateResultV1 *);
typedef void (*GDExtensionInterfaceOutputSealV1)(GDExtensionProductionPhysicalContextRefV1, GDExtensionProductionRequestRefV1, const GDExtensionOutputSealInputV1 *, GDExtensionOutputSealResultV1 *);
typedef GDExtensionOutputStatusV1 (*GDExtensionInterfaceOutputCancelV1)(GDExtensionProductionPhysicalContextRefV1, GDExtensionProductionRequestRefV1, const GDExtensionOutputCancelInputV1 *);
typedef GDExtensionOutputStatusV1 (*GDExtensionInterfaceOutputWithdrawV1)(GDExtensionProductionPhysicalContextRefV1, GDExtensionProductionRequestRefV1, const GDExtensionOutputGenerationInputV1 *);
#ifdef __cplusplus
}
#endif
#endif
