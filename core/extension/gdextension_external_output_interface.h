/**************************************************************************/
/*  gdextension_external_output_interface.h                               */
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

// Internal implementation declarations; public ABI comes only from JSON.
#include "gdextension_interface.gen.h"

#include <cstdint>

typedef struct {
	uint32_t abi_version;
	uint32_t struct_size;
	uint32_t reserved;
	uint64_t particles_rid;
	uint64_t next_present_seq;
	uint32_t has_host_affine;
	uint32_t reserved2;
	float current_host_affine[12];
	float previous_host_affine[12];
} GDExtensionFastRefreshBeginInputV1;

typedef struct {
	uint32_t abi_version;
	uint32_t struct_size;
	int32_t native_error;
	uint32_t slot_index;
	uint64_t lease_serial;
	uint64_t current_present_seq;
	uint32_t profile;
	uint32_t capacity;
	uint32_t surface_count;
	uint32_t aux_params_enabled;
	uint64_t current_records_rid;
	uint64_t previous_records_rid;
	uint64_t output_meta_rid;
	uint64_t aux_params_rid;
	uint64_t indexed_staging_rid;
	uint64_t indexed_history_rid;
	uint64_t guard_status_rid;
	uint64_t protected_commands_rid;
	uint32_t motion_required;
	uint32_t reserved;
} GDExtensionFastRefreshTargetsV1;

typedef struct {
	uint32_t abi_version;
	uint32_t struct_size;
	uint32_t slot_index;
	uint32_t parity_bank;
	uint64_t particles_rid;
	uint64_t lease_serial;
	uint64_t next_present_seq;
	uint64_t source_count_rid;
	uint64_t source_gate_rids[4];
	uint32_t source_count_word_offset;
	uint32_t source_gate_word_offsets[4];
	uint32_t reserved;
} GDExtensionFastRefreshEndInputV1;

enum {
	GDEXTENSION_FAST_REFRESH_END_FLAG_TRUSTED_TIER2 = 0x1u,
};

namespace GDExtensionDeviceExit {
typedef struct {
	uint32_t abi_version; // 1
	uint32_t struct_size; // sizeof(GDExtensionViewportDepthPairQueryInputV1)
	uint32_t arm_capture; // 1 = keep slot.pending_capture armed for next render
	uint32_t reserved;
	uint64_t viewport_rid;
	float slab_thickness_m; // > 0 overrides slot default; <= 0 keeps existing/0.50f
	float silhouette_jump_m;
} GDExtensionViewportDepthPairQueryInputV1;

typedef struct {
	uint32_t abi_version; // 1
	uint32_t struct_size; // sizeof(GDExtensionViewportDepthPairResultV1)
	int32_t native_error;
	uint32_t populated; // 1 iff current_snapshot is valid & FLAG_PRESENT
	uint32_t width;
	uint32_t height;
	uint32_t flags;
	uint64_t snapshot_generation;
	uint64_t meta_ssbo_rid;
	uint64_t depth_texture_rid;
} GDExtensionViewportDepthPairResultV1;

int32_t ange_external_output_begin_fast_refresh_v1(const GDExtensionFastRefreshBeginInputV1 *p_input, GDExtensionFastRefreshTargetsV1 *p_out_targets);
int32_t ange_external_output_end_fast_refresh_v1(const GDExtensionFastRefreshEndInputV1 *p_input);
int32_t ange_external_output_release_fast_refresh_v1(uint64_t p_particles_rid, uint32_t p_slot_index);
int32_t ange_external_output_begin_fast_refresh_batch_v1(uint32_t p_count, const GDExtensionFastRefreshBeginInputV1 *p_inputs, GDExtensionFastRefreshTargetsV1 *p_out_targets);
int32_t ange_external_output_end_fast_refresh_batch_v1(uint32_t p_count, const GDExtensionFastRefreshEndInputV1 *p_inputs, uint32_t p_flags, int32_t *p_out_errors);
int32_t ange_external_output_query_viewport_depth_pair_v1(const GDExtensionViewportDepthPairQueryInputV1 *p_input, GDExtensionViewportDepthPairResultV1 *p_out_result);
GDExtensionDeviceSessionResultV1 device_session_output_selection_facts_v2(GDExtensionProductionAttachmentRefV1, GDExtensionOutputMetadataSelectionRefV2, const GDExtensionOutputSelectionFactsInputV2 *, GDExtensionOutputSelectionFactsV2 *);
GDExtensionDeviceSessionResultV1 device_session_output_metadata_selection_query_v2(const GDExtensionOutputMetadataSelectionQueryV2*, GDExtensionOutputMetadataSelectionRequirementsV2*);
GDExtensionDeviceSessionResultV1 device_session_output_metadata_selection_prepare_v2(GDExtensionProductionPhysicalContextRefV1, GDExtensionProductionRequestRefV1, const GDExtensionOutputMetadataSelectionPrepareV2*, GDExtensionOutputMetadataSelectionResultV2*);
GDExtensionDeviceSessionResultV1 device_session_output_metadata_selection_read_v2(GDExtensionProductionAttachmentRefV1, GDExtensionOutputMetadataSelectionRefV2, uint32_t, GDExtensionOutputMetadataSelectionObservationV2*);
GDExtensionDeviceSessionResultV1 device_session_output_metadata_selection_return_v2(GDExtensionProductionAttachmentRefV1, GDExtensionOutputMetadataSelectionRefV2);
GDExtensionDeviceSessionResultV1 device_session_output_create_v4(GDExtensionProductionPhysicalContextRefV1, GDExtensionProductionRequestRefV1, const GDExtensionOutputCreateInputV4*, GDExtensionOutputCreateResultV1*);
GDExtensionDeviceSessionResultV1 device_session_output_scene_query_commit_v2(GDExtensionProductionPhysicalContextRefV1, GDExtensionProductionRequestRefV1, const GDExtensionOutputSceneQueryCommitV2*, GDExtensionOutputSceneQueryResultV2*);
void device_session_output_create_v1(GDExtensionProductionPhysicalContextRefV1, GDExtensionProductionRequestRefV1, const GDExtensionOutputCreateInputV1 *, GDExtensionOutputCreateResultV1 *);
void device_session_output_metadata_query_v1(const GDExtensionOutputMetadataQueryInputV1 *, GDExtensionOutputMetadataQueryResultV1 *);
void device_session_output_create_v3(GDExtensionProductionPhysicalContextRefV1, GDExtensionProductionRequestRefV1, const GDExtensionOutputCreateInputV3 *, GDExtensionOutputCreateResultV1 *);
void device_session_output_create_v2(GDExtensionProductionPhysicalContextRefV1, GDExtensionProductionRequestRefV1, const GDExtensionOutputCreateInputV2 *, GDExtensionOutputCreateResultV1 *);
void device_session_output_acquire_v1(GDExtensionProductionPhysicalContextRefV1, GDExtensionProductionRequestRefV1, uint64_t, const GDExtensionOutputAcquireInputV1 *, GDExtensionOutputAcquireResultV1 *);
void device_session_output_acquire_v2(GDExtensionProductionPhysicalContextRefV1, GDExtensionProductionRequestRefV1, uint64_t, const GDExtensionOutputAcquireInputV1 *, GDExtensionOutputAcquireResultV2 *);
void device_session_output_poll_v1(GDExtensionProductionPhysicalContextRefV1, GDExtensionProductionRequestRefV1, const GDExtensionOutputGenerationInputV1 *, GDExtensionOutputPollResultV1 *);
void device_session_output_seal_v1(GDExtensionProductionPhysicalContextRefV1, GDExtensionProductionRequestRefV1, const GDExtensionOutputSealInputV1 *, GDExtensionOutputSealResultV1 *);
GDExtensionOutputStatusV1 device_session_output_cancel_v1(GDExtensionProductionPhysicalContextRefV1, GDExtensionProductionRequestRefV1, const GDExtensionOutputCancelInputV1 *);
GDExtensionOutputStatusV1 device_session_output_withdraw_v1(GDExtensionProductionPhysicalContextRefV1, GDExtensionProductionRequestRefV1, const GDExtensionOutputGenerationInputV1 *);
} // namespace GDExtensionDeviceExit
