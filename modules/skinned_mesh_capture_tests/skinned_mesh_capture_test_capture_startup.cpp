/**************************************************************************/
/*  skinned_mesh_capture_test_capture_startup.cpp                         */
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

#include "skinned_mesh_capture_test_capture_startup.h"

#include "skinned_mesh_capture_test_capture_writer.h"

#include "core/io/file_access.h"
#include "core/os/os.h"

#include <climits>
#include <memory>
#include <mutex>

SkinnedMeshCaptureTestStartupParse skinned_mesh_capture_test_parse_capture_startup(const std::vector<std::string> &p_args, SkinnedMeshCaptureTestCaptureStartup &r_startup) {
	SkinnedMeshCaptureTestCaptureStartup candidate;
	bool seen[4] = {};
	const char *names[] = { "--p01-capture-fd", "--p01-run-id", "--p01-child-id", "--p01-manifest-sha256" };
	bool any = false;
	for (size_t i = 0; i < p_args.size(); i++) {
		for (size_t option = 0; option < 4; option++) {
			const std::string name = names[option];
			if (p_args[i].compare(0, name.size() + 1, name + "=") == 0) {
				return SkinnedMeshCaptureTestStartupParse::Invalid;
			}
			if (p_args[i] != name) {
				continue;
			}
			any = true;
			if (seen[option] || i + 1 >= p_args.size()) {
				return SkinnedMeshCaptureTestStartupParse::Invalid;
			}
			seen[option] = true;
			const auto &value = p_args[++i];
			if (option == 0) {
				if (value.empty() || value[0] == '0') {
					return SkinnedMeshCaptureTestStartupParse::Invalid;
				}
				uint64_t descriptor = 0;
				for (char c : value) {
					if (c < '0' || c > '9' || descriptor > (uint64_t(INT_MAX) - uint64_t(c - '0')) / 10) {
						return SkinnedMeshCaptureTestStartupParse::Invalid;
					}
					descriptor = descriptor * 10 + uint64_t(c - '0');
				}
				if (descriptor < 3) {
					return SkinnedMeshCaptureTestStartupParse::Invalid;
				}
				candidate.descriptor = int(descriptor);
			} else if (option == 1) {
				candidate.identity.run = value;
			} else if (option == 2) {
				candidate.identity.child = value;
			} else {
				candidate.identity.manifest_sha256 = value;
			}
			break;
		}
	}
	if (!any) {
		return SkinnedMeshCaptureTestStartupParse::Absent;
	}
	for (bool present : seen) {
		if (!present) {
			return SkinnedMeshCaptureTestStartupParse::Invalid;
		}
	}
	// Validate identities using the exact emitter grammar, without publishing.
	SkinnedMeshCaptureTestCaptureCodec check;
	std::string ignored;
	candidate.identity.source_build = "startup-validation";
	if (!check.start(candidate.identity, ignored)) {
		return SkinnedMeshCaptureTestStartupParse::Invalid;
	}
	candidate.identity.source_build.clear();
	r_startup = std::move(candidate);
	return SkinnedMeshCaptureTestStartupParse::Valid;
}
namespace {
std::mutex capture_mutex;
std::unique_ptr<SkinnedMeshCaptureTestCaptureWriter> writer;
SkinnedMeshCaptureTestCaptureCodec codec;
bool attempted = false;
bool valid = false;
bool claimed = false;
std::string child;
} //namespace
void skinned_mesh_capture_test_capture_startup() {
	std::lock_guard<std::mutex> lock(capture_mutex);
	if (attempted) {
		return;
	}
	attempted = true;
	std::vector<std::string> args;
	for (const String &arg : OS::get_singleton()->get_cmdline_args()) {
		args.emplace_back(arg.utf8().get_data());
	}
	SkinnedMeshCaptureTestCaptureStartup startup;
	if (skinned_mesh_capture_test_parse_capture_startup(args, startup) != SkinnedMeshCaptureTestStartupParse::Valid) {
		return;
	}
	const String binary_hash = FileAccess::get_sha256(OS::get_singleton()->get_executable_path());
	if (binary_hash.length() != 64) {
		return;
	}
	startup.identity.source_build = binary_hash.utf8().get_data();
	writer = std::make_unique<SkinnedMeshCaptureTestCaptureWriter>();
	if (!writer->start(startup.descriptor)) {
		writer.reset();
		return;
	}
	std::string line;
	valid = codec.start(startup.identity, line) && writer->publish(line);
	if (valid) {
		child = startup.identity.child;
	}
}
void skinned_mesh_capture_test_capture_publish(const SkinnedMeshCaptureTestHookEvent &p_event) {
	std::lock_guard<std::mutex> lock(capture_mutex);
	if (!valid || !writer) {
		return;
	}
	std::string line;
	// Raw operation stream deliberately does not fabricate a fixture action edge.
	valid = codec.event(p_event, "unassociated", line) && writer->publish(line);
}
bool skinned_mesh_capture_test_capture_transport_available() {
	std::lock_guard<std::mutex> lock(capture_mutex);
	return valid && writer && !writer->failed();
}
void skinned_mesh_capture_test_capture_shutdown() {
	std::lock_guard<std::mutex> lock(capture_mutex);
	// No seal until the full native fixture/retirement owner qualifies it.
	// EOF without seal remains incomplete at A's unchanged receiver.
	valid = false;
	if (writer) {
		writer->close();
		writer.reset();
	}
}

bool skinned_mesh_capture_test_capture_claim_child(const std::string &p_member, const std::string &p_route, uint64_t &r_epoch) {
	std::lock_guard<std::mutex> lock(capture_mutex);
	if (!valid || !writer || writer->failed() || claimed || child != p_member + "." + p_route) {
		return false;
	}
	claimed = true;
	// Exactly one acquired child session in this process. This local epoch is
	// scoped by the parent run/child/build envelope, not a cross-process identity.
	r_epoch = 1;
	return true;
}

bool skinned_mesh_capture_test_capture_blob(const std::string &p_artifact, const uint8_t *p_bytes, size_t p_size) {
	std::lock_guard<std::mutex> lock(capture_mutex);
	if (!valid || !writer || writer->failed()) {
		return false;
	}
	std::string line;
	valid = codec.blob(p_artifact, p_bytes, p_size, line) && writer->publish(line);
	return valid;
}
