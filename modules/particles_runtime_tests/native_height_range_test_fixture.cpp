/**************************************************************************/
/*  native_height_range_test_fixture.cpp                                  */
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

#include "native_height_range_test_fixture.h"

#include "core/config/engine.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "servers/rendering/rendering_device.h"
#include "servers/rendering/rendering_server.h"

namespace {
using State = NativeDisposalWitness::State;
constexpr uint64_t DEADLINE_USEC = 40000000;
Dictionary range_data(const NativeHeightRange::Snapshot &p_snapshot) {
	Dictionary d;
	d["recording_closed"] = p_snapshot.recording_closed;
	d["has_recorded_effect"] = p_snapshot.has_recorded_effect;
	d["mapping_closed"] = p_snapshot.mapping_closed;
	d["issue_closed"] = p_snapshot.issue_closed;
	d["native_complete"] = p_snapshot.native_complete;
	d["observers_returned"] = p_snapshot.observers_returned;
	d["graph_members"] = p_snapshot.graph_members;
	d["sealed_members"] = p_snapshot.sealed_members;
	d["issued_sources"] = p_snapshot.issued_sources;
	d["retained_sources"] = p_snapshot.retained_sources;
	d["completed_sources"] = p_snapshot.completed_sources;
	d["append_error"] = int(p_snapshot.append_error);
	d["mapping_error"] = int(p_snapshot.mapping_error);
	d["issue_error"] = int(p_snapshot.issue_error);
	d["retention_error"] = int(p_snapshot.retention_error);
	d["completion_error"] = int(p_snapshot.completion_error);
	d["return_error"] = int(p_snapshot.return_error);
	return d;
}
Dictionary witness_data(const NativeDisposalWitness::Report &p_report) {
	Dictionary d;
	d["rid"] = p_report.rid;
	d["serial"] = p_report.serial;
	d["state"] = int(p_report.state);
	return d;
}
bool live_pair(const NativeDepthAllocationProjection::Report &p_report) {
	return p_report.depth.state == State::LIVE && p_report.framebuffer.state == State::LIVE && !p_report.all_created_children_returned;
}
} // namespace

void NativeHeightRangeTestFixture::_bind_methods() {
	ClassDB::bind_method(D_METHOD("begin", "case_index"), &NativeHeightRangeTestFixture::begin);
	ClassDB::bind_method(D_METHOD("advance"), &NativeHeightRangeTestFixture::advance);
	ClassDB::bind_method(D_METHOD("result"), &NativeHeightRangeTestFixture::result);
}

bool NativeHeightRangeTestFixture::check(bool p_ok, const String &p_label) {
	Dictionary row;
	row["check"] = p_label;
	row["ok"] = p_ok;
	checks.push_back(row);
	if (!p_ok) {
		failures.push_back(p_label);
	}
	return p_ok;
}

void NativeHeightRangeTestFixture::publish() {
	data["schema"] = 1;
	data["started"] = started;
	data["complete"] = complete;
	data["error"] = int(outcome);
	data["case_index"] = case_index;
	data["path"] = case_index == 0 ? "bounded" : "ordinary";
	data["phase"] = phase;
	data["callbacks"] = callback_count;
	data["checks"] = checks;
	data["failures"] = failures;
	data["ranges"] = range_snapshots;
	data["physical"] = physical_snapshots;
	data["maintenance_calls"] = maintenance_calls;
	data["range_maintenance_calls"] = range_maintenance_calls;
	data["observe_polls"] = observe_polls;
	data["observer_releases"] = observer_releases;
	data["elapsed_usec"] = OS::get_singleton()->get_ticks_usec() - start_usec;
	data["deadline_usec"] = DEADLINE_USEC;
	data["physical_prefix_returned"] = physical_returned;
	data["retained_on_failure"] = prefix_created && !physical_returned;
	data["producer_complete"] = false;
	data["whole_output_released"] = false;
	data["reservation_refunded"] = false;
	MutexLock lock(result_mutex);
	published = data.duplicate(true);
}

void NativeHeightRangeTestFixture::finish(Error p_error) {
	outcome = p_error != OK ? p_error : failures.is_empty() ? OK
															: ERR_BUG;
	complete = true;
	publish();
}

Dictionary NativeHeightRangeTestFixture::result() const {
	MutexLock lock(result_mutex);
	return published.duplicate(true);
}

Error NativeHeightRangeTestFixture::observe_ranges(RenderingDevice *p_rd, const String &p_stage, NativeHeightRange::Snapshot (&r_snapshots)[2]) {
	Array rows;
	Error result = OK;
	for (int i = 0; i < 2; ++i) {
		const RenderingDevice::NativeDisposalBudget budget{ 64, 8, OS::get_singleton()->get_ticks_usec() + 100000 };
		RenderingDevice::NativeDisposalUsage usage;
		const Error error = p_rd->_native_height_range_observe(ranges[i], budget, usage, r_snapshots[i]);
		observe_polls += usage.polls;
		Dictionary row = range_data(r_snapshots[i]);
		row["observe_error"] = int(error);
		rows.push_back(row);
		if (error != OK && (error != ERR_BUSY || result == OK)) {
			result = error;
		}
	}
	range_snapshots[p_stage] = rows;
	return result;
}

Error NativeHeightRangeTestFixture::observe_pair(RenderingDevice *p_rd, const String &p_stage, NativeDepthAllocationProjection::Report &r_report) {
	const Error error = p_rd->_native_disposal_depth_allocation_observe(pair, r_report);
	Dictionary row;
	row["observe_error"] = int(error);
	row["depth"] = witness_data(r_report.depth);
	row["framebuffer"] = witness_data(r_report.framebuffer);
	row["all_created_children_returned"] = r_report.all_created_children_returned;
	physical_snapshots[p_stage] = row;
	return error;
}

void NativeHeightRangeTestFixture::begin(int p_case) {
	if (started) {
		return;
	}
	started = true;
	case_index = p_case;
	start_usec = OS::get_singleton()->get_ticks_usec();
	++callback_count;
	RenderingServer *rs = RenderingServer::get_singleton();
	RenderingDevice *rd = RenderingDevice::get_singleton();
	if (!check(rs && rd && rs->is_on_render_thread() && rs->get_rendering_device() == rd && rd->is_main_instance, "main_rd_rt")) {
		finish(ERR_UNAVAILABLE);
		return;
	}
	if (!check(!rs->is_render_loop_enabled() && !rd->draw_list.active && !rd->compute_list.active && !rd->raytracing_list.active, "loop_disabled_for_setup") || !check(p_case == 0 || p_case == 1, "fixed_case")) {
		finish(ERR_INVALID_PARAMETER);
		return;
	}
	Dictionary device;
	device["driver"] = OS::get_singleton()->get_current_rendering_driver_name();
	device["renderer"] = OS::get_singleton()->get_current_rendering_method();
	device["name"] = rd->get_device_name();
	device["api"] = rd->get_device_api_version();
	data["device"] = device;
	NativeDepthAllocationQuote quote;
	Error error = rd->_native_disposal_depth_allocation_quote(64, 64, quote);
	if (!check(error == OK, "quote_issued")) {
		finish(error);
		return;
	}
	NativeDepthAllocationQuote::Report quoted;
	error = rd->_native_disposal_depth_allocation_quote_observe(quote, quoted);
	if (!check(error == OK && quoted.width == 64 && quoted.height == 64 && quoted.texture_owner_reservation.knowledge == NativeDepthAllocationProjection::ByteKnowledge::Known, "quote_observed")) {
		finish(error != OK ? error : ERR_BUG);
		return;
	}
	data["texture_reservation_bytes"] = quoted.texture_owner_reservation.bytes;
	NativeDepthAllocationProjection depth_projection;
	NativeDepthAllocationQuote::CreateOutcome created;
	error = rd->_native_disposal_depth_texture_create_quoted(quote, depth_rid, depth, depth_projection, created);
	prefix_created = depth_rid.is_valid();
	if (!check(error == OK && created == NativeDepthAllocationQuote::CreateOutcome::CreatedVerified && prefix_created, "quoted_depth_created")) {
		finish(error != OK ? error : ERR_BUG);
		return;
	}
	error = rd->_native_disposal_depth_framebuffer_create(depth, framebuffer_rid, framebuffer);
	if (!check(error == OK && framebuffer_rid.is_valid(), "framebuffer_created")) {
		finish(error != OK ? error : ERR_BUG);
		return;
	}
	error = rd->_native_disposal_depth_allocation_capture(depth, framebuffer, pair);
	if (!check(error == OK, "pair_captured")) {
		finish(error);
		return;
	}
	const auto list = rd->draw_list_begin(framebuffer_rid, RenderingDevice::DRAW_CLEAR_DEPTH, {}, 0.25f);
	if (!check(list != RenderingDevice::INVALID_ID, "initial_clear_recorded")) {
		finish(ERR_CANT_CREATE);
		return;
	}
	rd->draw_list_end();
	do {
		++maintenance_calls;
		error = rd->_native_disposal_maintenance();
		if (error != ERR_BUSY) {
			break;
		}
		OS::get_singleton()->delay_usec(1000);
	} while (OS::get_singleton()->get_ticks_usec() - start_usec < DEADLINE_USEC);
	const auto *fb = rd->framebuffer_owner.get_or_null(framebuffer_rid);
	bool materialized = fb && fb->framebuffer_cache && !fb->framebuffer_cache->storage_map.is_empty();
	if (materialized) {
		for (const auto &entry : fb->framebuffer_cache->storage_map) {
			materialized = materialized && bool(entry.value.framebuffer) && bool(entry.value.render_pass);
		}
	}
	if (!check(error == OK && materialized, "initial_clear_completed")) {
		finish(error != OK ? error : ERR_BUG);
		return;
	}
	const auto *texture = rd->texture_owner.get_or_null(depth_rid);
	data["pending_clear_before_range"] = texture ? texture->pending_clear : true;
	if (!check(texture && !texture->pending_clear && texture->transfer_worker_index < 0 && !texture->owner.is_valid() && !texture->shared_fallback, "standalone_ready")) {
		finish(ERR_UNAVAILABLE);
		return;
	}
	for (int i = 0; i < 2; ++i) {
		error = rd->_native_height_range_prepare(ranges[i]);
		if (error != OK) {
			check(false, "ranges_prepared");
			finish(error);
			return;
		}
	}
	check(ranges[0].is_valid() && ranges[1].is_valid() && ranges[0] != ranges[1], "ranges_prepared");
	data["independent_ranges"] = ranges[0] != ranges[1];
	data["recorded_frame"] = rd->frame;
	for (int i = 0; i < 2; ++i) {
		error = rd->_native_height_range_record_depth_clear(ranges[i], depth, framebuffer, i == 0 ? 0.375f : 0.625f);
		if (error != OK) {
			check(false, "ranges_recorded");
			finish(error);
			return;
		}
	}
	check(true, "ranges_recorded");
	for (int i = 0; i < 2; ++i) {
		error = rd->_native_height_range_close(ranges[i]);
		if (error != OK) {
			check(false, "ranges_closed");
			finish(error);
			return;
		}
	}
	check(true, "ranges_closed");
	NativeHeightRange::Snapshot snapshots[2];
	error = observe_ranges(rd, "closed", snapshots);
	if (!check((error == OK || error == ERR_BUSY) && snapshots[0].recording_closed && snapshots[1].recording_closed && snapshots[0].has_recorded_effect && snapshots[1].has_recorded_effect && snapshots[0].graph_members > 0 && snapshots[1].graph_members > 0 && !snapshots[0].native_complete && !snapshots[1].native_complete && !snapshots[0].observers_returned && !snapshots[1].observers_returned, "closed_not_complete")) {
		finish(error != OK && error != ERR_BUSY ? error : ERR_BUG);
		return;
	}
	NativeDepthAllocationProjection::Report live;
	error = observe_pair(rd, "recorded", live);
	if (!check(error == OK && live_pair(live), "pair_live_before_issue")) {
		finish(error != OK ? error : ERR_BUG);
		return;
	}
	initial_depth_serial = live.depth.serial;
	initial_framebuffer_serial = live.framebuffer.serial;
	recorded_draws = Engine::get_singleton()->get_frames_drawn();
	data["draws_before_range_issue"] = recorded_draws;
	phase = 1;
	publish();
	if (case_index == 0) {
		// Stay inside this original RT callback. Main::iteration waits for
		// RenderingServer::sync before its mandatory pending-resource draw and
		// frames_drawn increment; yielding back to Main between bounded steps
		// would allow that unrelated ordinary draw to advance this range.
		while (!complete) {
			advance_impl();
			if (!complete) {
				OS::get_singleton()->delay_usec(1000);
			}
		}
	}
}

void NativeHeightRangeTestFixture::advance() {
	RenderingServer *rs = RenderingServer::get_singleton();
	if (!started || complete || !rs || !rs->is_on_render_thread()) {
		return;
	}
	++callback_count;
	const bool finish_cleanup_here = case_index == 1 && phase == 2;
	advance_impl();
	// After Main disables the ordinary loop, keep the entire bounded cleanup
	// continuation in this callback. Main must never see a suspended partial
	// frame advance and enter its mandatory pending-resource draw in between.
	while (finish_cleanup_here && !complete) {
		advance_impl();
		if (!complete) {
			OS::get_singleton()->delay_usec(1000);
		}
	}
}

void NativeHeightRangeTestFixture::advance_impl() {
	RenderingServer *rs = RenderingServer::get_singleton();
	RenderingDevice *rd = RenderingDevice::get_singleton();
	if (OS::get_singleton()->get_ticks_usec() - start_usec >= DEADLINE_USEC) {
		finish(ERR_TIMEOUT);
		return;
	}
	const RenderingDevice::NativeDisposalBudget budget{ 4096, 32, OS::get_singleton()->get_ticks_usec() + 100000 };
	Error error = OK;
	if (phase == 1) {
		if (case_index == 0) {
			if (rs->is_render_loop_enabled()) {
				finish(ERR_BUG);
				return;
			}
			RenderingDevice::NativeDisposalUsage usage;
			++maintenance_calls;
			++range_maintenance_calls;
			error = rd->_native_disposal_maintenance_step(budget, usage);
			if (error != OK && error != ERR_BUSY) {
				finish(error);
				return;
			}
		}
		NativeHeightRange::Snapshot s[2];
		error = observe_ranges(rd, "poll", s);
		if (error != OK && error != ERR_BUSY) {
			finish(error);
			return;
		}
		if (s[0].native_complete && s[1].native_complete) {
			range_snapshots["completed"] = range_snapshots["poll"];
			if (!check(s[0].mapping_closed && s[1].mapping_closed && s[0].issue_closed && s[1].issue_closed && s[0].issued_sources > 0 && s[1].issued_sources > 0 && s[0].issued_sources == s[0].retained_sources && s[1].issued_sources == s[1].retained_sources && s[0].issued_sources == s[0].completed_sources && s[1].issued_sources == s[1].completed_sources && !s[0].observers_returned && !s[1].observers_returned, "native_ranges_complete")) {
				finish(ERR_BUG);
				return;
			}
			NativeDepthAllocationProjection::Report physical;
			error = observe_pair(rd, "native_complete", physical);
			if (!check(error == OK && live_pair(physical), "completion_is_not_physical_return")) {
				finish(error != OK ? error : ERR_BUG);
				return;
			}
			const uint64_t draws = Engine::get_singleton()->get_frames_drawn();
			data["draws_at_native_complete"] = draws;
			if (!check(case_index == 0 ? draws == recorded_draws && range_maintenance_calls > 0 : draws > recorded_draws && range_maintenance_calls == 0 && rs->is_render_loop_enabled(), "correct_issue_path")) {
				finish(ERR_BUG);
				return;
			}
			phase = 2; // Main thread must disable ordinary loop before returning observers.
		}
	} else if (phase == 2) {
		if (!check(!rs->is_render_loop_enabled(), "loop_disabled_for_return")) {
			finish(ERR_BUG);
			return;
		}
		phase = 3;
	} else if (phase == 3 || phase == 4) {
		RenderingDevice::NativeDisposalUsage usage;
		error = rd->_native_height_range_return(ranges[phase - 3], budget, usage);
		observer_releases += usage.releases;
		if (error != OK && error != ERR_BUSY) {
			finish(error);
			return;
		}
		if (error == OK) {
			NativeHeightRange::Snapshot s[2];
			error = observe_ranges(rd, phase == 3 ? "first_returned" : "both_returned", s);
			if (phase == 3) {
				if (!check(error == OK && s[0].native_complete && s[0].observers_returned, "first_range_returned") || !check(s[1].native_complete && !s[1].observers_returned, "second_range_still_complete")) {
					finish(ERR_BUG);
					return;
				}
			} else {
				if (!check(error == OK && s[0].native_complete && s[1].native_complete && s[0].observers_returned && s[1].observers_returned, "second_range_returned")) {
					finish(ERR_BUG);
					return;
				}
				NativeDepthAllocationProjection::Report physical;
				error = observe_pair(rd, "observers_returned", physical);
				if (!check(error == OK && live_pair(physical), "range_return_is_not_physical_return")) {
					finish(error != OK ? error : ERR_BUG);
					return;
				}
				error = rd->_native_disposal_free(framebuffer);
				if (!check(error == OK, "retire_framebuffer")) {
					finish(error);
					return;
				}
				error = observe_pair(rd, "framebuffer_enqueued", physical);
				if (!check(error == OK && physical.framebuffer.state == State::ENQUEUED && physical.depth.state == State::LIVE && !physical.all_created_children_returned, "framebuffer_enqueued_not_returned")) {
					finish(error != OK ? error : ERR_BUG);
					return;
				}
			}
			++phase;
		}
	} else if (phase == 5 || phase == 6) {
		RenderingDevice::NativeDisposalUsage usage;
		++maintenance_calls;
		error = rd->_native_disposal_maintenance_step(budget, usage);
		if (error != OK && error != ERR_BUSY) {
			finish(error);
			return;
		}
		NativeDepthAllocationProjection::Report physical;
		error = observe_pair(rd, phase == 5 ? "framebuffer_returned" : "returned", physical);
		if (error != OK || physical.depth.state == State::UNKNOWN || physical.framebuffer.state == State::UNKNOWN || physical.all_created_children_returned != (physical.depth.state == State::RETURNED && physical.framebuffer.state == State::RETURNED)) {
			finish(error != OK ? error : ERR_BUG);
			return;
		}
		if (phase == 5 && physical.framebuffer.state == State::RETURNED) {
			if (!check(physical.depth.state == State::LIVE && !physical.all_created_children_returned, "framebuffer_returned_depth_live")) {
				finish(ERR_BUG);
				return;
			}
			error = rd->_native_disposal_free(depth);
			if (!check(error == OK, "retire_depth")) {
				finish(error);
				return;
			}
			error = observe_pair(rd, "depth_enqueued", physical);
			if (!check(error == OK && physical.depth.state == State::ENQUEUED && physical.framebuffer.state == State::RETURNED && !physical.all_created_children_returned, "depth_enqueued_not_returned")) {
				finish(error != OK ? error : ERR_BUG);
				return;
			}
			phase = 6;
		} else if (phase == 6 && physical.all_created_children_returned) {
			physical_returned = true;
			data["draws_at_physical_return"] = Engine::get_singleton()->get_frames_drawn();
			check(true, "both_physical_returned");
			check(physical.depth.rid == depth_rid.get_id() && physical.framebuffer.rid == framebuffer_rid.get_id() && physical.depth.serial == initial_depth_serial && physical.framebuffer.serial == initial_framebuffer_serial, "identities_preserved");
			NativeHeightRange::Snapshot s[2];
			error = observe_ranges(rd, "physical_returned", s);
			check(error == OK && s[0].native_complete && s[1].native_complete && s[0].observers_returned && s[1].observers_returned, "native_complete_after_return");
			check(true, "no_producer_claim");
			if (error != OK) {
				finish(error);
				return;
			}
			phase = 7;
		}
	} else if (phase == 7) {
		// A physical witness can return before the bounded continuation has
		// reopened its next recording frame. Finish that original cursor before
		// publishing complete and allowing Main or shutdown to resume drawing.
		if (rd->native_disposal_active.load()) {
			RenderingDevice::NativeDisposalUsage usage;
			++maintenance_calls;
			error = rd->_native_disposal_maintenance_step(budget, usage);
			if (error != OK && error != ERR_BUSY) {
				finish(error);
				return;
			}
			if (rd->native_disposal_active.load()) {
				publish();
				return;
			}
		}
		data["cleanup_continuation_active"] = rd->native_disposal_active.load();
		data["cleanup_recording_error"] = int(rd->_recording_error());
		if (!check(!rd->native_disposal_active.load() && rd->_recording_error() == OK, "cleanup_continuation_settled")) {
			finish(rd->_recording_error() != OK ? rd->_recording_error() : ERR_BUG);
			return;
		}
		finish(OK);
		return;
	}
	publish();
}
