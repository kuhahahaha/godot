#include "p01_capture_codec.h"
#include "core/crypto/crypto_core.h"

static bool identifier(const std::string &value) {
	if (value.empty() || value.size() > 256) { return false; }
	for (const unsigned char c : value) {
		if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.' || c == ':')) { return false; }
	}
	return true;
}
static const char *hook_name(uint8_t kind) {
	static const char *hooks[] = { nullptr, "buffer_write", "readback_queue", "readback_complete", "buffer_allocate", "buffer_free", "buffer_clear", "buffer_copy", "writable_use", "buffer_dispose", "skeleton_setter", "skeleton_upload", "skeleton_version", "skeleton_signal_before", "skeleton_signal_after", "skeleton_palette_request", "draw_record", "driver_result" };
	return kind < sizeof(hooks) / sizeof(*hooks) ? hooks[kind] : nullptr;
}
std::string P01CaptureCodec::envelope(const char *p_kind) const {
	return std::string("{\"kind\":\"") + p_kind + "\",\"run_id\":\"" + identity.run + "\",\"child_id\":\"" + identity.child + "\",\"manifest_sha256\":\"" + identity.manifest_sha256 + "\",\"frame_seq\":\"" + std::to_string(frame_sequence) + "\"";
}
bool P01CaptureCodec::finish(std::string p_line, std::string &r_line) {
	p_line += "}\n";
	if (poisoned || p_line.size() > 65536 || metadata_bytes + p_line.size() > 16 * 1024 * 1024 || frame_sequence == UINT64_MAX) { poisoned = true; return false; }
	metadata_bytes += p_line.size();
	++frame_sequence;
	r_line = std::move(p_line);
	return true;
}
bool P01CaptureCodec::start(const P01CaptureIdentity &p_identity, std::string &r_line) {
	if (started || sealed || poisoned || !identifier(p_identity.run) || !identifier(p_identity.child) || !identifier(p_identity.source_build) || p_identity.manifest_sha256.size() != 64) { return false; }
	for (char c : p_identity.manifest_sha256) { if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) { return false; } }
	identity = p_identity;
	started = true;
	return finish(envelope("start") + ",\"source_build_identity\":\"" + identity.source_build + "\",\"stream_ids\":[\"publication\"]", r_line);
}
bool P01CaptureCodec::event(const P01HookEvent &p_event, const std::string &p_action, std::string &r_line) {
	const char *hook = hook_name(p_event.kind);
	if (!started || sealed || poisoned || !hook || !identifier(p_action) || !p_event.epoch || p_event.sequence != last_hook_sequence + 1 || event_count >= 4096) { poisoned = true; return false; }
	std::string line = envelope("event") + ",\"stream_id\":\"publication\",\"stream_seq\":\"" + std::to_string(event_count) + "\",\"event_id\":\"hook-" + std::to_string(p_event.sequence) + "\",\"origin\":\"engine\",\"hook\":\"" + hook + "\",\"action_ref\":\"" + p_action + "\",\"epoch\":\"" + std::to_string(p_event.epoch) + "\",\"payload\":{";
	bool first = true;
	auto number = [&](const char *key, uint64_t value) { if (!first) { line += ','; } first = false; line += std::string("\"") + key + "\":" + std::to_string(value); };
	number("rid", p_event.rid); number("allocation_serial", p_event.allocation_serial); number("sequence", p_event.sequence);
	number("offset", p_event.offset); number("size", p_event.size); number("range", p_event.range); number("kind", p_event.kind);
	number("device", p_event.device); number("driver", p_event.driver); number("source_rid", p_event.source_rid); number("source_offset", p_event.source_offset);
	number("session_serial_hint", p_event.session_serial_hint); number("source_allocation_serial", p_event.source_allocation_serial);
	number("writable_usage", p_event.writable_usage); number("write_operation", p_event.write_operation); number("content_version", p_event.content_version);
	line += ",\"operation_error\":" + std::to_string(p_event.operation_error);
	line += std::string(",\"success\":") + (p_event.success ? "true" : "false") + ",\"failed_prefix\":" + (p_event.failed_prefix ? "true" : "false") + "}";
	if (!finish(std::move(line), r_line)) { return false; }
	last_hook_sequence = p_event.sequence; ++event_count;
	return true;
}
bool P01CaptureCodec::blob64(const std::string &p_artifact, const uint8_t *p_bytes, size_t p_size, std::string &r_line) {
	if (!started || sealed || poisoned || !identifier(p_artifact) || !p_bytes || p_size != 64 || blobs.count(p_artifact) || blobs.size() >= 36) { poisoned = true; return false; }
	unsigned char hash[32];
	if (CryptoCore::sha256(p_bytes, p_size, hash) != OK) { poisoned = true; return false; }
	static const char hex[] = "0123456789abcdef", alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	std::string digest, encoded;
	for (auto byte : hash) { digest += hex[byte >> 4]; digest += hex[byte & 15]; }
	for (size_t i = 0; i < p_size; i += 3) {
		const uint32_t bits = uint32_t(p_bytes[i]) << 16 | (i + 1 < p_size ? uint32_t(p_bytes[i + 1]) << 8 : 0) | (i + 2 < p_size ? p_bytes[i + 2] : 0);
		encoded += alphabet[(bits >> 18) & 63]; encoded += alphabet[(bits >> 12) & 63]; encoded += i + 1 < p_size ? alphabet[(bits >> 6) & 63] : '='; encoded += i + 2 < p_size ? alphabet[bits & 63] : '=';
	}
	if (!finish(envelope("blob") + ",\"artifact_id\":\"" + p_artifact + "\",\"chunk_index\":\"0\",\"offset\":\"0\",\"declared_total\":\"64\",\"data\":\"" + encoded + "\"", r_line)) { return false; }
	blobs[p_artifact] = digest;
	return true;
}
bool P01CaptureCodec::seal(std::string &r_line) {
	if (!started || sealed || poisoned) { return false; }
	std::string line = envelope("seal") + ",\"event_counts\":{\"publication\":\"" + std::to_string(event_count) + "\"},\"blobs\":{";
	bool first = true;
	for (const auto &blob : blobs) {
		if (!first) { line += ','; } first = false;
		line += "\"" + blob.first + "\":{\"bytes\":\"64\",\"sha256\":\"" + blob.second + "\"}";
	}
	if (!finish(line + "}", r_line)) { return false; }
	sealed = true;
	return true;
}
