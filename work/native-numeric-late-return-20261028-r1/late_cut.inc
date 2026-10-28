// Executed inside the original ready callback, after successful kind9 record.
if (!retained_record_rt.handle_serial) {
	std::_Exit(210);
}
std::vector<GDExtensionQualificationFactV1> before_cancel;
uint64_t submitted_serial = 0;
for (uint32_t pass = 0; pass < 5000 && !submitted_serial; ++pass) {
	require(observe_ticket(bridge, ticket, &budget, &report).native_error, "observe_actual_submit");
	if (report.state == 8) {
		std::fprintf(stderr, "LATE_CASE_INCONCLUSIVE already_complete=1 before_cancel=1\n");
		std::_Exit(211);
	}
	before_cancel.resize(report.fact_count);
	for (uint32_t first = 0; first < before_cancel.size();) {
		uint32_t n = 0;
		require(facts_ticket(bridge, ticket, report.revision, first, std::min<uint32_t>(64, before_cancel.size() - first), before_cancel.data() + first, &n).native_error, "pre_cancel_sources");
		if (!n) {
			std::_Exit(212);
		}
		first += n;
	}
	for (const auto &f : before_cancel) {
		if (f.semantic_kind == 4 && f.issue == 2 && f.native_serial) {
			std::printf("LATE_OBSERVED_UNKNOWN_PREFIX native=%llu completion_source=%llu pending=1\n", (unsigned long long)f.native_serial, (unsigned long long)f.completion_serial);
		}
	}
	for (const auto &fact : before_cancel) {
		if ((fact.semantic_kind == 4 || fact.semantic_kind == 5) && fact.native_serial && (fact.issue == 1 || fact.issue == 2) && fact.completion == 1) {
			submitted_serial = fact.native_serial;
		}
	}
	if (!submitted_serial) {
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}
if (!submitted_serial || !report.readback_request) {
	std::fprintf(stderr, "LATE_PENDING_DIAGNOSTIC state=%u phase=%u request=%llu facts=%u known=%u possible=%u error=%d\n", report.state, report.phase, (unsigned long long)report.readback_request, report.fact_count, report.known_count, report.possible_count, report.native_error);
	for (const auto &x : before_cancel) {
		std::fprintf(stderr, "LATE_PENDING_FACT semantic=%u issue=%u completion=%u native=%llu error=%u\n", x.semantic_kind, x.issue, x.completion, (unsigned long long)x.native_serial, x.error);
	}
	std::_Exit(213);
}
bool capture_issued = false, body_issued = false, readback_issued = false;
for (const auto &fact : before_cancel) {
	if (fact.native_serial != submitted_serial) {
		continue;
	}
	capture_issued |= fact.semantic_kind == 1;
	body_issued |= fact.semantic_kind == 2;
	readback_issued |= fact.semantic_kind == 3 && fact.source_backing_serial == buffers[7].backing_serial && fact.source_offset == 0 && fact.source_size == occurrence.observed_bytes;
}
if (!capture_issued || !body_issued || !readback_issued) {
	std::_Exit(214);
}
const uint64_t issued_readback = report.readback_request;
for (const auto &f : before_cancel) {
	if ((f.semantic_kind == 4 || f.semantic_kind == 5) && f.native_serial == submitted_serial) {
		std::printf("LATE_CUT_NATIVE semantic=%u issue=%u completion=%u native=%llu\n", f.semantic_kind, f.issue, f.completion, (unsigned long long)f.native_serial);
	}
}
std::printf("LATE_SUBMITTED_PENDING ticket=%llu revision=%llu request=%llu native=%llu facts=%zu capture=1 body=1 exact_readback_issued=1 reply_still_held=1\n", (unsigned long long)ticket.handle_serial, (unsigned long long)report.revision, (unsigned long long)issued_readback, (unsigned long long)submitted_serial, before_cancel.size());
{
	std::ofstream before(std::filesystem::path(result_directory) / "before_cancel_facts.bin", std::ios::binary);
	before.write(reinterpret_cast<const char *>(before_cancel.data()), before_cancel.size() * sizeof(before_cancel[0]));
	if (!before) {
		std::_Exit(198);
	}
	std::ofstream state(std::filesystem::path(result_directory) / "before_cancel_report.bin", std::ios::binary);
	state.write(reinterpret_cast<const char *>(&report), sizeof(report));
	if (!state) {
		std::_Exit(198);
	}
}
// Request real physical retirement while the same submitted source is pending.
// The final identical requests must return these same original retirement cells.
GDExtensionStorageRetireRequestV1 early_storage{ abi, buffers[7].backing, buffers[7].rid, buffers[7].backing_serial, 7, 0, 41, 2, 0 };
GDExtensionStorageRetireRefV1 early_storage_ref{};
GDExtensionStorageRetireReportV1 early_storage_report{};
GDExtensionNativeUnitRetireRequestV1 early_unit{ abi, natives[0].unit, 3, 0 };
GDExtensionNativeUnitRetireRefV1 early_unit_ref{};
GDExtensionNativeUnitRetireReportV1 early_unit_report{};
require(direct_storage_retire(bridge, &early_storage, &early_storage_ref, &early_storage_report).native_error, "early_original_backing_retire");
require(direct_unit_retire(bridge, &early_unit, &early_unit_ref, &early_unit_report).native_error, "early_original_unit_retire");
if (early_storage_report.state != 0 || early_storage_report.reason != 3 || early_storage_report.event_seq || early_unit_report.state != 0 || early_unit_report.shader_free_event_seq || early_unit_report.pipeline_free_event_seq) {
	std::_Exit(215);
}
auto early_release = release_ticket(bridge, ticket);
if (early_release.native_error != ERR_BUSY || early_release.reason != 16) {
	std::_Exit(216);
}
require(cancel_ticket(bridge, ticket).native_error, "cancel_original_submitted_request");
GDExtensionQualificationReportV1 denied_report{};
auto denied = direct_prepared(bridge, ticket, &request, &denied_report);
if (denied.native_error != ERR_BUSY || denied.reason != 16) {
	std::_Exit(217);
}
auto early_close = close_bridge(bridge);
if (early_close.native_error != ERR_BUSY || early_close.reason != 16) {
	std::_Exit(218);
}
std::printf("LATE_STOPPED cancel=1 new_record_refused=%d reason=%u early_backing_pending=1 early_unit_pending=1 ticket_release_busy=1 bridge_close_busy=1\n", denied.native_error, denied.reason);
cancel_prefix = true; // Continue only the original revoked-output settlement path.
