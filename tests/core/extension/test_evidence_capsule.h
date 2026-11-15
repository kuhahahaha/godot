/**************************************************************************/
/*  test_evidence_capsule.h                                               */
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

#include "core/extension/gdextension.h"
#include "core/extension/gdextension_core_evidence_capsule.h"
#include "core/extension/gdextension_library_loader.h"
#include "core/io/file_access.h"
#include "core/io/marshalls.h"
#include "core/os/os.h"
#include "tests/test_macros.h"
#include "tests/test_utils.h"

#if defined(UNIX_ENABLED) || defined(MACOS_ENABLED)
#include <dlfcn.h>
#endif

namespace TestEvidenceCapsule {

TEST_CASE("[evidence_capsule] CS-03 atomic Open->Sealed coordinator burns aborted sequence & gates CLOSE_BEGIN") {
	using Coord = GDExtensionDeviceExit::CapsuleBatchCoordinator;
	Coord coord;

	uint64_t ack_seq = 0;
	CHECK(coord.enter_close_begin(ack_seq) == ERR_UNAUTHORIZED);

	REQUIRE(coord.open_preclose_batch() == OK);
	REQUIRE(coord.stage_preclose_event() == OK);
	REQUIRE(coord.stage_preclose_event() == OK);

	// Injected abort before commit burns sequence 1 without sealing or exposing uncommitted ack:
	Coord::SealAck ack;
	CHECK(coord.seal_preclose_batch(0xabcdu, true, ack) == ERR_CANT_CREATE);
	CHECK_FALSE(ack.valid);
	CHECK(coord.get_state() == Coord::BATCH_OPEN);
	CHECK(coord.peek_next_batch_seq() == 2);
	CHECK(coord.enter_close_begin(ack_seq) == ERR_UNAUTHORIZED);

	// Retry seal commits cleanly at sequence 2:
	REQUIRE(coord.seal_preclose_batch(0xabcdu, false, ack) == OK);
	CHECK(ack.valid);
	CHECK(ack.batch_seq == 2);
	CHECK(ack.committed_event_count == 2);
	CHECK(coord.get_state() == Coord::BATCH_SEALED);

	// Lost-return retry with identical request_token returns committed ack idempotently:
	Coord::SealAck retry_ack;
	REQUIRE(coord.seal_preclose_batch(0xabcdu, false, retry_ack) == OK);
	CHECK(retry_ack.batch_seq == 2);
	CHECK(coord.peek_next_batch_seq() == 3);

	// Different request_token after seal is rejected:
	CHECK(coord.seal_preclose_batch(0x9999u, false, retry_ack) == ERR_INVALID_PARAMETER);

	// CLOSE_BEGIN now succeeds causally after committed preclose_ack:
	REQUIRE(coord.enter_close_begin(ack_seq) == OK);
	CHECK(ack_seq == 2);
	CHECK(coord.has_entered_close_begin());
}

TEST_CASE("[evidence_capsule] K03 ANGECAP1 v3 OSClosed + RTLD_NOLOAD non-residency + zero leaked RIDs") {
	bool dso_resident_after_close = false;
#if defined(MACOS_ENABLED)
	// Build a tiny real Mach-O dylib in test temp folder to exercise genuine dlopen/dlclose + RTLD_NOLOAD witness:
	const String c_path = TestUtils::get_temp_path("k03_witness_probe.c");
	const String dylib_path = TestUtils::get_temp_path("libk03_witness_probe.dylib");
	{
		Ref<FileAccess> f = FileAccess::open(c_path, FileAccess::WRITE);
		REQUIRE(f.is_valid());
		f->store_string("int ange_k03_probe_symbol(void) { return 42; }\n");
	}
	List<String> clang_args;
	clang_args.push_back("-dynamiclib");
	clang_args.push_back(c_path);
	clang_args.push_back("-o");
	clang_args.push_back(dylib_path);
	String pipe_out;
	int exit_code = -1;
	REQUIRE(OS::get_singleton()->execute("/usr/bin/clang", clang_args, &pipe_out, &exit_code, true) == OK);
	REQUIRE(exit_code == 0);

	const CharString dylib_utf8 = dylib_path.utf8();
	void *handle = dlopen(dylib_utf8.get_data(), RTLD_NOW | RTLD_LOCAL);
	REQUIRE(handle != nullptr);

	bool resident_while_open = false;
	REQUIRE(GDExtensionDeviceExit::probe_dso_resident_rtld_noload(dylib_utf8.get_data(), resident_while_open) == OK);
	CHECK(resident_while_open);

	REQUIRE(dlclose(handle) == 0);
	REQUIRE(GDExtensionDeviceExit::probe_dso_resident_rtld_noload(dylib_utf8.get_data(), dso_resident_after_close) == OK);
	CHECK_FALSE(dso_resident_after_close);
#endif

	GDExtensionDeviceExit::CompleteDeinitClosureV1 closure;
	closure.child_serial = 9001;
	closure.registration_serial = 77;
	closure.attachment_serial = 12;
	closure.preclose_ack_sequence = 2;
	closure.os_close_attempted = true;
	closure.os_close_returned = true;
	closure.os_close_error = OK;
	closure.dso_resident_after_dlclose = dso_resident_after_close;
	closure.leaked_rendering_device_rids = 0;
	closure.leaked_u08_budget_bytes = 0;
	closure.unsettled_fences = 0;

	Vector<uint8_t> stream;
	REQUIRE(GDExtensionDeviceExit::emit_complete_deinit_capsule_v3(closure, stream) == OK);
	CHECK(stream.size() > 200);

	GDExtensionDeviceExit::VerifiedCapsuleSummary summary;
	REQUIRE(GDExtensionDeviceExit::verify_unload_capsule_stream(stream, summary) == OK);
	CHECK(summary.wire_version == GDExtensionDeviceExit::ANGECAP1_WIRE_VERSION_COMPLETE_DEINIT_V3);
	CHECK(summary.child_serial == 9001);
	CHECK(summary.batch_count == 2);
	CHECK(summary.event_count == 4);
	CHECK_FALSE(summary.is_core_failure);
	CHECK(summary.os_closed_verified);
	CHECK(summary.leaked_rendering_device_rids == 0);
	CHECK(summary.leaked_u08_budget_bytes == 0);
	CHECK(summary.foreign_lifetime_violation_count == 0);
	CHECK(summary.unsettled_fences == 0);

	Vector<GDExtensionDeviceExit::DecodedAngeCapFrame> frames;
	REQUIRE(GDExtensionDeviceExit::decode_angecap1_stream(stream, frames) == OK);
	REQUIRE(frames.size() == 3);
	CHECK(frames[1].frame_kind == GDExtensionDeviceExit::ANGECAP_FRAME_BATCH);
	CHECK(String(frames[1].payload["phase"]) == "CorePostDeinit");
	CHECK(String(frames[1].payload["dso_close_state"]) == "OSClosed");
	CHECK(int64_t(frames[1].payload["foreign_lifetime_violation_count"]) == 0);
	CHECK(frames[2].frame_kind == GDExtensionDeviceExit::ANGECAP_FRAME_CHILD_SEAL);
	CHECK(String(frames[2].payload["phase"]) == "Sealed");
	CHECK(bool(frames[2].payload["ok"]));
}

TEST_CASE("[evidence_capsule] RED negative controls (leaked_rids=1, resident DSO, tamper, wire_version=1, mixed version) & v2 CoreFailure") {
	GDExtensionDeviceExit::CompleteDeinitClosureV1 base;
	base.child_serial = 42;
	base.registration_serial = 5;
	base.attachment_serial = 1;
	base.preclose_ack_sequence = 1;
	base.os_close_attempted = true;
	base.os_close_returned = true;
	base.os_close_error = OK;
	base.dso_resident_after_dlclose = false;

	// 1. RED control: leaked_rendering_device_rids = 1 MUST fail verification!
	{
		GDExtensionDeviceExit::CompleteDeinitClosureV1 leaked = base;
		leaked.leaked_rendering_device_rids = 1;
		Vector<uint8_t> s;
		REQUIRE(GDExtensionDeviceExit::emit_complete_deinit_capsule_v3(leaked, s) == OK);
		GDExtensionDeviceExit::VerifiedCapsuleSummary summary;
		CHECK(GDExtensionDeviceExit::verify_unload_capsule_stream(s, summary) == ERR_UNAUTHORIZED);
		CHECK_FALSE(summary.os_closed_verified);
	}

	// 2. RED control: dso_resident_after_dlclose = true MUST fail verification!
	{
		GDExtensionDeviceExit::CompleteDeinitClosureV1 resident = base;
		resident.dso_resident_after_dlclose = true;
		Vector<uint8_t> s;
		REQUIRE(GDExtensionDeviceExit::emit_complete_deinit_capsule_v3(resident, s) == OK);
		GDExtensionDeviceExit::VerifiedCapsuleSummary summary;
		CHECK(GDExtensionDeviceExit::verify_unload_capsule_stream(s, summary) == ERR_UNAUTHORIZED);
	}

	// 3. RED controls: byte tamper, wire_version = 1 fallback, truncated ChildSeal:
	Vector<uint8_t> valid_stream;
	REQUIRE(GDExtensionDeviceExit::emit_complete_deinit_capsule_v3(base, valid_stream) == OK);

	{
		Vector<uint8_t> tampered = valid_stream;
		tampered.write[45] ^= 0x01u;
		GDExtensionDeviceExit::VerifiedCapsuleSummary summary;
		CHECK(GDExtensionDeviceExit::verify_unload_capsule_stream(tampered, summary) == ERR_INVALID_DATA);
	}

	{
		Vector<uint8_t> v1_stream = valid_stream;
		encode_uint32(1u, v1_stream.ptrw() + 8);
		GDExtensionDeviceExit::VerifiedCapsuleSummary summary;
		CHECK(GDExtensionDeviceExit::verify_unload_capsule_stream(v1_stream, summary) == ERR_INVALID_DATA);
	}

	{
		// Truncate final ChildSeal frame -> must reject missing ChildSeal:
		const uint64_t f1_len = decode_uint64(valid_stream.ptr() + 32);
		const int64_t f1_total = 40 + int64_t(f1_len) + 32;
		const uint64_t f2_len = decode_uint64(valid_stream.ptr() + f1_total + 32);
		const int64_t two_frames = f1_total + 40 + int64_t(f2_len) + 32;
		Vector<uint8_t> no_seal = valid_stream.slice(0, two_frames);
		GDExtensionDeviceExit::VerifiedCapsuleSummary summary;
		CHECK(GDExtensionDeviceExit::verify_unload_capsule_stream(no_seal, summary) == ERR_INVALID_DATA);
	}

	// 4. Safe pre-OS CoreFailure capsule (wire_version = 2, frame_kind = 3, 3-member schema):
	{
		GDExtensionDeviceExit::CoreFailureWitnessV2 fail;
		fail.child_serial = 88;
		fail.registration_serial = 9;
		fail.failure_error = ERR_TIMEOUT;
		fail.failure_origin = "FenceSettleTimeoutBeforeClose";
		fail.source_stream_ref = "child-88-preclose-batch-1";
		fail.close_disposition = "OSCloseRefusedSafePreOS";

		Vector<uint8_t> fail_stream;
		REQUIRE(GDExtensionDeviceExit::emit_core_failure_capsule_v2(fail, fail_stream) == OK);
		GDExtensionDeviceExit::VerifiedCapsuleSummary summary;
		REQUIRE(GDExtensionDeviceExit::verify_unload_capsule_stream(fail_stream, summary) == OK);
		CHECK(summary.wire_version == GDExtensionDeviceExit::ANGECAP1_WIRE_VERSION_FAILURE_V2);
		CHECK(summary.is_core_failure);
		CHECK_FALSE(summary.os_closed_verified);
	}
}

#if defined(MACOS_ENABLED)
TEST_CASE("[evidence_capsule] D5-P1-7 Godot Core GDExtension close_library emits authentic post-dlclose ANGECAP1 v3 capsule") {
	const String c_path = TestUtils::get_temp_path("k03_gdext_close_witness.c");
	const String dylib_path = TestUtils::get_temp_path("libk03_gdext_close_witness.dylib");
	const String gdext_path = TestUtils::get_temp_path("k03_gdext_close_witness.gdextension");
	const String capsule_out = TestUtils::get_temp_path("k03_core_emitted_post_dlclose.angecap");

	if (FileAccess::exists(capsule_out)) {
		std::remove(capsule_out.utf8().get_data());
	}

	{
		Ref<FileAccess> fc = FileAccess::open(c_path, FileAccess::WRITE);
		REQUIRE(fc.is_valid());
		fc->store_string(
				"#include <stdint.h>\n"
				"typedef uint8_t GDExtensionBool;\n"
				"typedef int32_t GDExtensionInitializationLevel;\n"
				"typedef struct {\n"
				"\tGDExtensionInitializationLevel minimum_initialization_level;\n"
				"\tvoid *userdata;\n"
				"\tvoid (*initialize)(void *userdata, GDExtensionInitializationLevel p_level);\n"
				"\tvoid (*deinitialize)(void *userdata, GDExtensionInitializationLevel p_level);\n"
				"} GDExtensionInitialization;\n"
				"static void _k03_init(void *u, GDExtensionInitializationLevel l) { (void)u; (void)l; }\n"
				"static void _k03_deinit(void *u, GDExtensionInitializationLevel l) { (void)u; (void)l; }\n"
				"GDExtensionBool ange_k03_core_witness_entry(void *p_get_proc, void *p_lib, GDExtensionInitialization *r_init) {\n"
				"\t(void)p_get_proc;\n"
				"\t(void)p_lib;\n"
				"\tr_init->minimum_initialization_level = 2;\n"
				"\tr_init->userdata = 0;\n"
				"\tr_init->initialize = _k03_init;\n"
				"\tr_init->deinitialize = _k03_deinit;\n"
				"\treturn 1;\n"
				"}\n");
	}

	List<String> clang_args;
	clang_args.push_back("-dynamiclib");
	clang_args.push_back(c_path);
	clang_args.push_back("-o");
	clang_args.push_back(dylib_path);
	String pipe_out;
	int exit_code = -1;
	REQUIRE(OS::get_singleton()->execute("/usr/bin/clang", clang_args, &pipe_out, &exit_code, true) == OK);
	REQUIRE(exit_code == 0);

	{
		Ref<FileAccess> fg = FileAccess::open(gdext_path, FileAccess::WRITE);
		REQUIRE(fg.is_valid());
		fg->store_string(
				"[configuration]\n"
				"entry_symbol = \"ange_k03_core_witness_entry\"\n"
				"compatibility_minimum = \"4.1.0\"\n\n"
				"[libraries]\n"
				"macos = \"" +
				dylib_path.get_file() + "\"\n");
	}

	struct EnvRestoreGuard {
		~EnvRestoreGuard() {
			if (OS::get_singleton()) {
				OS::get_singleton()->set_environment("ANGE_K03_CAPSULE_OUT", "");
				OS::get_singleton()->set_environment("ANGE_K03_CAPSULE_LIBRARY_FILTER", "");
			}
		}
	} env_guard;

	OS::get_singleton()->set_environment("ANGE_K03_CAPSULE_OUT", capsule_out);
	OS::get_singleton()->set_environment("ANGE_K03_CAPSULE_LIBRARY_FILTER", "libk03_gdext_close_witness.dylib");

	Ref<GDExtension> ext;
	ext.instantiate();
	Ref<GDExtensionLibraryLoader> loader;
	loader.instantiate();
	REQUIRE(ext->open_library(gdext_path, loader) == OK);
	REQUIRE(ext->is_library_open());

	// While open, DSO MUST be resident in dyld:
	bool resident_open = false;
	REQUIRE(GDExtensionDeviceExit::probe_dso_resident_rtld_noload(dylib_path.utf8().get_data(), resident_open) == OK);
	CHECK(resident_open);

	REQUIRE(ext->initialize_library(GDExtension::INITIALIZATION_LEVEL_CORE) == OK);
	REQUIRE(ext->initialize_library(GDExtension::INITIALIZATION_LEVEL_SERVERS) == OK);
	REQUIRE(ext->initialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE) == OK);
	REQUIRE(ext->deinitialize_library(GDExtension::INITIALIZATION_LEVEL_SCENE) == OK);
	REQUIRE(ext->deinitialize_library(GDExtension::INITIALIZATION_LEVEL_SERVERS) == OK);
	REQUIRE(ext->deinitialize_library(GDExtension::INITIALIZATION_LEVEL_CORE) == OK);

	// Close dynamic library -> Godot Core itself must emit ANGECAP1 v3 AFTER OS::close_dynamic_library!
	REQUIRE(ext->close_library() == OK);
	CHECK_FALSE(ext->is_library_open());

	REQUIRE(FileAccess::exists(capsule_out));
	const Vector<uint8_t> emitted = FileAccess::get_file_as_bytes(capsule_out);
	REQUIRE(emitted.size() > 200);

	GDExtensionDeviceExit::VerifiedCapsuleSummary summary;
	REQUIRE(GDExtensionDeviceExit::verify_unload_capsule_stream(emitted, summary) == OK);
	CHECK(summary.wire_version == GDExtensionDeviceExit::ANGECAP1_WIRE_VERSION_COMPLETE_DEINIT_V3);
	CHECK_FALSE(summary.is_core_failure);
	CHECK(summary.os_closed_verified);
	CHECK(summary.leaked_rendering_device_rids == 0);
	CHECK(summary.leaked_u08_budget_bytes == 0);
	CHECK(summary.foreign_lifetime_violation_count == 0);
	CHECK(summary.unsettled_fences == 0);

	Vector<GDExtensionDeviceExit::DecodedAngeCapFrame> frames;
	REQUIRE(GDExtensionDeviceExit::decode_angecap1_stream(emitted, frames) == OK);
	REQUIRE(frames.size() == 3);
	CHECK(frames[1].frame_kind == GDExtensionDeviceExit::ANGECAP_FRAME_BATCH);
	CHECK(String(frames[1].payload["phase"]) == "CorePostDeinit");
	CHECK(String(frames[1].payload["dso_close_state"]) == "OSClosed");
	CHECK(int64_t(frames[1].payload["foreign_lifetime_violation_count"]) == 0);
	CHECK(frames[2].frame_kind == GDExtensionDeviceExit::ANGECAP_FRAME_CHILD_SEAL);
	CHECK(String(frames[2].payload["phase"]) == "Sealed");
	CHECK(bool(frames[2].payload["ok"]));
}
#endif

} // namespace TestEvidenceCapsule
