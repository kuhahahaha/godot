#pragma once
#include "p01_observer.h"
#include <map>
#include <string>

struct P01CaptureIdentity { std::string run, child, manifest_sha256, source_build; };
// Private lossless JSONL codec for original hook records. Publication order is
// explicitly not a main/RT causal edge or a GPU completion certificate.
class P01CaptureCodec {
	P01CaptureIdentity identity;
	uint64_t frame_sequence = 0, event_count = 0, last_hook_sequence = 0, metadata_bytes = 0;
	bool started = false, sealed = false, poisoned = false;
	std::map<std::string, std::string> blobs;
	std::string envelope(const char *p_kind) const;
	bool finish(std::string p_line, std::string &r_line);
public:
	bool start(const P01CaptureIdentity &p_identity, std::string &r_line);
	bool event(const P01HookEvent &p_event, const std::string &p_action, std::string &r_line);
	bool blob64(const std::string &p_artifact, const uint8_t *p_bytes, size_t p_size, std::string &r_line);
	bool seal(std::string &r_line);
	bool failed() const { return poisoned; }
};
