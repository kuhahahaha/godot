/**************************************************************************/
/*  particles_external_output_test_fixture.cpp                            */
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

#include "particles_external_output_test_fixture.h"

#include "core/config/engine.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "servers/rendering/renderer_rd/storage_rd/external_output_storage.h"
#include "servers/rendering/rendering_server.h"

void ParticlesExternalOutputTestFixture::_bind_methods() {
	ClassDB::bind_method(D_METHOD("run"), &ParticlesExternalOutputTestFixture::run);
	ClassDB::bind_method(D_METHOD("result"), &ParticlesExternalOutputTestFixture::result);
}

bool ParticlesExternalOutputTestFixture::check(bool p_ok, const String &p_label) {
	MutexLock lock(result_mutex);
	Dictionary event;
	event["check"] = p_label;
	event["ok"] = p_ok;
	observations.push_back(event);
	if (!p_ok) {
		failures.push_back(p_label);
	}
	return p_ok;
}

Error ParticlesExternalOutputTestFixture::finish(Error p_error) {
	MutexLock lock(result_mutex);
	outcome = p_error != OK ? p_error : failures.is_empty() ? OK
															: ERR_BUG;
	retained_on_failure = outcome != OK && storage != nullptr;
	complete = true;
	return outcome;
}

Error ParticlesExternalOutputTestFixture::run() {
	{
		MutexLock lock(result_mutex);
		if (started) {
			return ERR_ALREADY_IN_USE;
		}
		started = true;
	}
	const uint64_t start = OS::get_singleton()->get_ticks_usec();
	const uint64_t draw_count = Engine::get_singleton()->get_frames_drawn();
	RenderingServer *rs = RenderingServer::get_singleton();
	RenderingDevice *rd = RenderingDevice::get_singleton();
	if (!check(rs && rd && rs->is_on_render_thread() && rs->get_rendering_device() == rd && rd->is_main_instance, "owner")) {
		return finish(ERR_UNAVAILABLE);
	}
	if (!check(!rs->is_render_loop_enabled(), "render_loop_disabled") ||
			!check(!rd->draw_list.active && !rd->compute_list.active && !rd->raytracing_list.active, "outside_lists")) {
		return finish(ERR_BUSY);
	}
	{
		MutexLock lock(result_mutex);
		device_info["driver"] = OS::get_singleton()->get_current_rendering_driver_name();
		device_info["renderer"] = OS::get_singleton()->get_current_rendering_method();
		device_info["name"] = rd->get_device_name();
		device_info["api"] = rd->get_device_api_version();
	}
	using Storage = RendererRD::ExternalOutputStorage;
	using State = NativeDisposalWitness::State;
	static_assert(Storage::RESOURCE_COUNT == 7);
	storage = new Storage(*rd);
	Storage *const original_storage = storage;
	const auto observe = [&](Storage::Report &r_report) {
		const Error error = storage->observe(r_report);
		MutexLock lock(result_mutex);
		counts["created"] = r_report.created_count;
		counts["live"] = r_report.live_count;
		counts["enqueued"] = r_report.enqueued_count;
		counts["returned"] = r_report.returned_count;
		counts["unknown"] = r_report.unknown_count;
		counts["bytes"] = r_report.total_bytes;
		return error;
	};
	Error error = storage->allocate_instance192(1, 1);
	if (!check(error == OK, "allocate")) {
		return finish(error);
	}
	Storage::Report live;
	error = observe(live);
	if (!check(error == OK && live.creation_error == OK && live.created_count == 7 && live.live_count == 7 && live.enqueued_count == 0 && live.returned_count == 0 && live.unknown_count == 0 && !live.retirement_requested && !live.all_driver_returned, "live_counts")) {
		return finish(error != OK ? error : ERR_BUG);
	}
	const uint32_t expected_bytes[7] = { 192, 192, 32, 64, 32, 20, 384 };
	bool extents = live.total_bytes == 916;
	bool identities = true;
	bool distinct = true;
	for (uint32_t i = 0; i < Storage::RESOURCE_COUNT; ++i) {
		const auto &native = live.resources[i].native;
		extents = extents && live.resources[i].bytes == expected_bytes[i];
		const RID rid = storage->resources[i].rid;
		const auto *buffer = rd->storage_buffer_owner.get_or_null(rid);
		identities = identities && native.state == State::LIVE && native.rid == rid.get_id() && native.rid && native.serial && buffer && buffer->driver_id && buffer->backing_serial == native.serial;
		for (uint32_t j = 0; j < i; ++j) {
			distinct = distinct && native.rid != live.resources[j].native.rid && native.serial != live.resources[j].native.serial;
		}
	}
	if (!check(extents, "live_bytes") || !check(identities, "live_original_identities") || !check(distinct, "live_distinct_identities")) {
		return finish(ERR_BUG);
	}
	const auto same_identities = [&](const Storage::Report &p_report, State p_state) {
		for (uint32_t i = 0; i < Storage::RESOURCE_COUNT; ++i) {
			const auto &a = live.resources[i];
			const auto &b = p_report.resources[i];
			if (a.native.rid != b.native.rid || a.native.serial != b.native.serial || a.bytes != b.bytes || b.native.state != p_state) {
				return false;
			}
		}
		return true;
	};
	if (!check(Storage::destroy_if_returned(storage) == ERR_BUSY, "live_destroy_busy") || !check(storage == original_storage, "live_owner_retained")) {
		return finish(ERR_BUG);
	}
	error = storage->request_retirement();
	if (!check(error == OK, "retire")) {
		return finish(error);
	}
	Storage::Report enqueued;
	error = observe(enqueued);
	if (!check(error == OK, "enqueued_observe")) {
		return finish(error);
	}
	if (!check(enqueued.retirement_requested && enqueued.created_count == 7 && enqueued.enqueued_count == 7 && enqueued.live_count == 0 && enqueued.returned_count == 0 && enqueued.unknown_count == 0 && !enqueued.all_driver_returned, "enqueued_counts") ||
			!check(same_identities(enqueued, State::ENQUEUED), "enqueued_original_identities")) {
		return finish(ERR_BUG);
	}
	bool removed = true;
	for (uint32_t i = 0; i < Storage::RESOURCE_COUNT; ++i) {
		removed = removed && !rd->storage_buffer_owner.owns(storage->resources[i].rid);
	}
	if (!check(removed, "logical_owners_removed") || !check(Storage::destroy_if_returned(storage) == ERR_BUSY, "enqueued_destroy_busy") || !check(storage == original_storage, "enqueued_owner_retained")) {
		return finish(ERR_BUG);
	}
	error = storage->request_retirement();
	if (!check(error == OK, "repeat_retire")) {
		return finish(error);
	}
	Storage::Report repeated;
	error = observe(repeated);
	if (!check(error == OK, "repeat_observe")) {
		return finish(error);
	}
	if (!check(repeated.created_count == 7 && repeated.enqueued_count == 7 && repeated.returned_count == 0 && repeated.unknown_count == 0 && repeated.total_bytes == 916 && same_identities(repeated, State::ENQUEUED), "repeat_unchanged")) {
		return finish(ERR_BUG);
	}
	// One callback owns the entire interval: Main cannot issue the automatic
	// draw(false) that pending frees otherwise cause even with rendering disabled.
	// Time limits only fail the test. Only original driver-free witnesses pass it.
	Storage::Report returned;
	uint32_t calls = 0;
	constexpr uint64_t DEADLINE_USEC = 20000000;
	while (true) {
		if (OS::get_singleton()->get_ticks_usec() - start >= DEADLINE_USEC) {
			error = ERR_TIMEOUT;
			break;
		}
		++calls;
		error = storage->maintenance();
		if (error != OK && error != ERR_BUSY) {
			break; // Sticky failure: no later observation may turn this test green.
		}
		error = observe(returned);
		if (error != OK || returned.unknown_count) {
			error = error != OK ? error : ERR_INVALID_DATA;
			break;
		}
		if (returned.all_driver_returned) {
			break;
		}
		OS::get_singleton()->delay_usec(1000);
	}
	{
		MutexLock lock(result_mutex);
		maintenance_calls = calls;
		elapsed_usec = OS::get_singleton()->get_ticks_usec() - start;
		if (error == OK && elapsed_usec >= DEADLINE_USEC) {
			error = ERR_TIMEOUT;
		}
	}
	if (!check(error == OK && returned.all_driver_returned, "maintenance_returned")) {
		return finish(error != OK ? error : ERR_BUG);
	}
	error = observe(returned);
	if (!check(error == OK, "returned_observe")) {
		return finish(error);
	}
	if (!check(returned.retirement_requested && returned.creation_error == OK && returned.created_count == 7 && returned.returned_count == 7 && returned.live_count == 0 && returned.enqueued_count == 0 && returned.unknown_count == 0 && returned.all_driver_returned && returned.total_bytes == 916, "returned_counts") ||
			!check(same_identities(returned, State::RETURNED), "returned_original_identities") ||
			!check(Engine::get_singleton()->get_frames_drawn() == draw_count && !rs->is_render_loop_enabled(), "no_next_draw")) {
		return finish(ERR_BUG);
	}
	error = Storage::destroy_if_returned(storage);
	if (!check(error == OK, "destroy_returned") || !check(storage == nullptr, "destroy_pointer_empty")) {
		return finish(error != OK ? error : ERR_BUG);
	}
	{
		MutexLock lock(result_mutex);
		physical_prefix_returned = true;
	}
	return finish(OK);
}

Dictionary ParticlesExternalOutputTestFixture::result() const {
	MutexLock lock(result_mutex);
	Dictionary report;
	report["schema"] = 1;
	report["started"] = started;
	report["complete"] = complete;
	report["error"] = outcome;
	report["checks"] = observations.duplicate(true);
	report["failures"] = failures.duplicate(true);
	report["device"] = device_info.duplicate(true);
	report["counts"] = counts.duplicate(true);
	report["maintenance_calls"] = maintenance_calls;
	report["elapsed_usec"] = elapsed_usec;
	report["deadline_usec"] = 20000000;
	report["retained_on_failure"] = retained_on_failure;
	report["physical_prefix_returned"] = physical_prefix_returned;
	report["whole_output_released"] = false;
	report["source_authority_supplied"] = false;
	report["dispatch_recorded"] = false;
	report["device_holder_claimed"] = false;
	return report;
}
