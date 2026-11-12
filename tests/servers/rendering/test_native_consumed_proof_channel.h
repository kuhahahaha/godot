/**************************************************************************/
/*  test_native_consumed_proof_channel.h                                  */
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

#include "servers/rendering/renderer_rd/storage_rd/external_output_packet.h"
#include "tests/test_macros.h"

namespace TestNativeConsumedProofChannel {
// The caller must supply its actual original native draw/list-close invocation.
// This driver never sets ADOPTED, history, receipt, graph or custody fields.
// It consumes the supplied original view on success. It is not wired to a
// renderer fixture yet, and therefore supplies no positive runtime evidence.
inline Error actual_color_path(RendererRD::ExternalPacketViewLease &view, Error (*draw_and_close)(void *), void *userdata) {
	if (!draw_and_close) { return ERR_INVALID_PARAMETER; }
	RendererRD::ExternalConsumedColorProofSnapshot proof;
	const Error before = view.copy_native_color_consumption(proof);
	CHECK(before == ERR_UNAVAILABLE);
	if (before != ERR_UNAVAILABLE) { return ERR_INVALID_DATA; }
	RendererRD::ExternalConsumedColorProofLease retained;
	struct ReturnRetained {
		RendererRD::ExternalConsumedColorProofLease &view;
		~ReturnRetained() {
			if (!view.is_empty()) { CHECK(view.release_checked() == OK); }
		}
	} cleanup{ retained };
	CHECK(retained.retain_from(view) == ERR_UNAVAILABLE);
	CHECK(retained.is_empty());
	const Error drawn = draw_and_close(userdata);
	if (drawn != OK) { return drawn; }
	const Error observed = view.copy_native_color_consumption(proof);
	CHECK(observed == OK);
	if (observed != OK) { return observed; }
	CHECK(proof.get_camera_source().is_valid());
	const GDExtensionOutputTokenV1 zero{};
	CHECK(memcmp(proof.get_token().bytes, zero.bytes, sizeof(zero.bytes)) != 0);
	const Error held = retained.retain_from(view);
	CHECK(held == OK);
	if (held != OK) { return held; }
	// Move the actual directory/pin relationship, then close original custody.
	RendererRD::ExternalConsumedColorProofLease moved(std::move(retained));
	retained = std::move(moved);
	CHECK(moved.is_empty());
	const Error returned = view.release_checked();
	CHECK(returned == OK);
	if (returned != OK) { return returned; }
	RendererRD::ExternalConsumedColorProofSnapshot checked;
	CHECK(retained.read_checked(checked) == OK);
	CHECK(checked.get_camera_source() == proof.get_camera_source());
	CHECK(checked.get_common().matches(proof.get_common()));
	const auto *copied = retained.get_snapshot();
	CHECK(copied != nullptr);
	if (!copied) { return ERR_INVALID_DATA; }
	CHECK(memcmp(copied->get_token().bytes, proof.get_token().bytes, sizeof(zero.bytes)) == 0);
	CHECK(copied->get_camera_source() == proof.get_camera_source());
	CHECK(copied->get_request().registry_instance == proof.get_request().registry_instance);
	CHECK(copied->get_request().handle_serial == proof.get_request().handle_serial);
	CHECK(copied->get_action_ordinal() == proof.get_action_ordinal());
	CHECK(copied->get_particles() == proof.get_particles());
	CHECK(copied->get_generation() == proof.get_generation());
	CHECK(copied->get_lease_serial() == proof.get_lease_serial());
	CHECK(memcmp(copied->get_lease_token().bytes, proof.get_lease_token().bytes, sizeof(zero.bytes)) == 0);
	CHECK(copied->get_view_epoch() == proof.get_view_epoch());
	CHECK(copied->get_common().matches(proof.get_common()));
	CHECK(copied->get_current_host() == proof.get_current_host());
	CHECK(copied->get_previous_host() == proof.get_previous_host());
	const Error closed = retained.release_checked();
	CHECK(closed == OK);
	if (closed != OK) { return closed; }
	CHECK(retained.get_snapshot() == nullptr);
	CHECK(retained.read_checked(checked) == ERR_DOES_NOT_EXIST);
	CHECK(checked.get_camera_source().is_null());
	return OK;
}

TEST_CASE("[Rendering][ExternalOutput] empty view cannot copy or retain a native consumed color proof") {
	RendererRD::ExternalPacketViewLease empty;
	RendererRD::ExternalConsumedColorProofLease retained;
	RendererRD::ExternalConsumedColorProofSnapshot proof;
	CHECK(retained.read_checked(proof) == ERR_DOES_NOT_EXIST);
	CHECK(empty.copy_native_color_consumption(proof) == ERR_DOES_NOT_EXIST);
	const GDExtensionOutputTokenV1 zero{};
	CHECK(memcmp(proof.get_token().bytes, zero.bytes, sizeof(zero.bytes)) == 0);
	CHECK(proof.get_camera_source().is_null());
	CHECK(retained.retain_from(empty) == ERR_DOES_NOT_EXIST);
	CHECK(retained.is_empty());
	CHECK(retained.get_snapshot() == nullptr);
}
} // namespace TestNativeConsumedProofChannel
