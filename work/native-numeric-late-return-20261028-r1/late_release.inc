if (report.readback_request != issued_readback || facts.size() < before_cancel.size()) {
	std::_Exit(219);
}
for (size_t i = 0; i < before_cancel.size(); ++i) {
	const auto &a = facts[i];
	const auto &b = before_cancel[i];
	if (a.graph_instance != b.graph_instance || a.graph_frame != b.graph_frame || a.graph_kind != b.graph_kind || a.graph_index != b.graph_index || a.graph_offset != b.graph_offset || a.semantic_kind != b.semantic_kind || a.source_backing_serial != b.source_backing_serial || a.source_offset != b.source_offset || a.source_size != b.source_size) {
		std::_Exit(220);
	}
	// Original native-call entries are immutable identities. An encoded graph's
	// mapping may refine from the hidden-worker prefix to its real queue issue.
	if ((b.semantic_kind == 4 || b.semantic_kind == 5) && (a.native_serial != b.native_serial || a.completion_serial != b.completion_serial || a.backend_queue != b.backend_queue || a.command_slot != b.command_slot)) {
		std::_Exit(220);
	}
}
for (size_t i = before_cancel.size(); i < facts.size(); ++i) {
	const auto &f = facts[i];
	if ((f.semantic_kind != 4 && f.semantic_kind != 5) || f.parent_fact >= before_cancel.size() || !before_cancel[f.parent_fact].graph_instance) {
		std::_Exit(220);
	}
}
auto held_release = release_ticket(bridge, ticket);
if (held_release.native_error != ERR_BUSY || held_release.reason != 16) {
	std::_Exit(221);
}
require(rt_release(bridge, retained_record_rt).native_error, "late_record_reply_ack");
if (!rt_release(bridge, retained_record_rt).native_error) {
	std::_Exit(222);
}
std::printf("LATE_ORIGINAL_RETURN request=%llu native=%llu exact_prefix_preserved=1 no_new_graph=1 revoked_copy_denied=1 retained_reply_blocked_release=1 reply_ack_once=1\n", (unsigned long long)issued_readback, (unsigned long long)submitted_serial);
