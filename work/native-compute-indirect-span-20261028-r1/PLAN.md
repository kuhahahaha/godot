# Compute indirect 12B command bounds defect

Authorized original RD defect, not PF optimization. Existing public compute_list_dispatch_indirect uses u32 offset+12 and lacks 4B alignment. Draw path already uses always-on aligned u64 spans. Preserve list/usage/qualification-return gates; reject malformed span before descriptor/graph/driver effects. No new manager/registry.

RED: extend existing TestExternalOutputAccess receiver to execute actual original public entrance on 32B command buffer: legal offsets0/20, misaligned2, end overrun24, UINT32_MAX-3 wrap. Observe graph count, descriptor-bound state, dispatch count, actual driver bind/dispatch after graph finish. Existing production should admit2 and wrap and fail.
GREEN: always-on offset%4 and uint64_t(offset)+12 bounds. Compile only changed TU plus existing test TU and privately link original archives; leave shared canonical editor unchanged while A uses it. Run focused new and adjacent B4 native packet join tests. Independent source/evidence review; exact commit/handoff. No real GPU acceptance claimed for this receiver test.
