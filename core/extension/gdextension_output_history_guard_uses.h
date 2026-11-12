#pragma once

#include "core/extension/gdextension_interface.gen.h"

namespace GDExtensionDeviceExit {
// Pure declaration coverage only. The original Registry supplies its accepted
// rows and separately authenticates request/view/unit/backing/source owners.
// Success here is not a resource permission or a semantic HistoryGather proof.
template <class UseAt, class ActionAt>
bool production_history_guard_use_coverage(uint32_t slot, uint64_t acquire, uint64_t finalize, uint64_t seal,
        uint32_t source_begin, uint32_t source_count, uint32_t use_count, uint32_t action_count,
        UseAt use_at, ActionAt action_at, uint64_t selected = UINT64_MAX) {
    if (acquire >= action_count || finalize >= action_count || seal >= action_count ||
            finalize <= acquire || seal <= finalize || source_begin > use_count || source_count > use_count - source_begin) { return false; }
    uint32_t sources = 0;
    bool consumer = false;
    bool selected_use = selected == UINT64_MAX;
    for (uint32_t u = 0; u < use_count; ++u) {
        const auto &use = use_at(u);
        if (use.resource_slot != slot) { continue; }
        if (use.access != GDEXTENSION_PRODUCTION_READ_V1 || use.byte_offset || use.byte_size != 32) { return false; }
        if (u >= source_begin && u - source_begin < source_count) {
            ++sources;
            selected_use |= selected == acquire || selected == finalize || selected == seal;
            continue;
        }
        bool covered = false;
        for (uint32_t a = 0; a < action_count; ++a) {
            const auto &action = action_at(a);
            if (action.first_use > use_count || action.use_count > use_count - action.first_use) { return false; }
            if (u < action.first_use || u - action.first_use >= action.use_count) { continue; }
            if (action.action_ordinal != a || action.action_kind != GDEXTENSION_PRODUCTION_DISPATCH_V1 || a <= acquire || a >= finalize) { return false; }
            covered = true; consumer = true; selected_use |= selected == a;
        }
        if (!covered) { return false; }
    }
    return sources == 1 && consumer && selected_use;
}
} // namespace GDExtensionDeviceExit
