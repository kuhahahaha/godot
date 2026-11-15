#pragma once
#include "core/extension/gdextension_interface.gen.h"
namespace GDExtensionDeviceExit {
// Pure final-mask arithmetic; its inputs are supplied only by the original
// native consumed archive and same real camera invocation in Registry.
inline uint32_t production_history_final_mask(uint32_t frozen, bool has_tuple, uint64_t previous_epoch,
        uint64_t previous_present, const GDExtensionOutputCameraV1 &previous,
        const GDExtensionOutputAcquireInputV1 &actual) {
    if (!has_tuple) { return frozen; }
    if (previous_epoch != actual.view_epoch) { frozen |= 1u << 2; }
    const auto &camera = actual.common.actual_previous_camera;
    if (!actual.common.has_actual_previous_camera || previous_present >= actual.common.present_seq ||
            previous.view_id != camera.view_id || previous.view_epoch != camera.view_epoch || previous.capture_ordinal != camera.capture_ordinal) { frozen |= 1u << 7; }
    return frozen;
}
inline uint32_t production_history_reset_reason(uint32_t mask) {
    for (uint32_t reason = 1; reason <= 8; ++reason) { if (mask & (1u << reason)) { return reason; } }
    return 0;
}
// Pure declaration equality/coverage. Owner, backing, preparation and native
// lifetime checks remain on the original Registry's real cells.
inline bool production_history_same_output_roles(const GDExtensionProductionOutputRoleV1 *primary,
        const GDExtensionProductionOutputRoleV1 *fallback) {
    for (uint32_t i = 0; i < 3; ++i) {
        if (primary[i].role != fallback[i].role || primary[i].reserved || fallback[i].reserved ||
                primary[i].resource_slot != fallback[i].resource_slot || primary[i].allocation_generation != fallback[i].allocation_generation ||
                primary[i].bytes != fallback[i].bytes) { return false; }
    }
    return true;
}
template <class UseAt, class ResourceAt>
bool production_history_acquire_use_coverage(const GDExtensionProductionOutputPrepareV1 &input,
        const GDExtensionProductionActionV2 &action, uint32_t resource_count, uint32_t use_count,
        UseAt use_at, ResourceAt resource_at) {
    if (action.action_ordinal != input.action_ordinal || action.action_kind != GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1 ||
            action.first_use > use_count || action.use_count > use_count - action.first_use || !input.source_use_count ||
            input.first_source_use < action.first_use || input.first_source_use - action.first_use > action.use_count ||
            input.source_use_count > action.use_count - (input.first_source_use - action.first_use)) { return false; }
    for (uint32_t role = 0; role < 3; ++role) {
        const auto &r = input.roles[role];
        if (!r.bytes) {
            if (r.resource_slot != UINT32_MAX || r.use_index != UINT32_MAX || r.allocation_generation) { return false; }
            continue;
        }
        if (r.resource_slot >= resource_count || r.use_index < action.first_use || r.use_index - action.first_use >= action.use_count) { return false; }
        const auto &u = use_at(r.use_index); const auto &resource = resource_at(r.resource_slot);
        if (u.resource_slot != r.resource_slot || u.access != GDEXTENSION_PRODUCTION_WRITE_V1 || u.byte_offset || u.byte_size != r.bytes ||
                resource.resource_kind != GDEXTENSION_PRODUCTION_STORAGE_V1 || resource.ownership != GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1 ||
                resource.original_generation != r.allocation_generation || resource.physical_bytes != r.bytes) { return false; }
    }
    for (uint32_t i = 0; i < action.use_count; ++i) {
        const auto index = action.first_use + i; const auto &use = use_at(index);
        if (use.resource_slot >= resource_count) { return false; }
        if (index >= input.first_source_use && index - input.first_source_use < input.source_use_count) {
            if (use.access != GDEXTENSION_PRODUCTION_READ_V1 || !use.byte_size) { return false; }
            continue;
        }
        bool output = false;
        for (const auto &role : input.roles) {
            output |= role.bytes && role.use_index == index && role.resource_slot == use.resource_slot && use.access == GDEXTENSION_PRODUCTION_WRITE_V1;
        }
        if (!output) { return false; }
    }
    return true;
}
}
