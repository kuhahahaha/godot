/**************************************************************************/
/*  native_depth_allocation_test_fixture.cpp                              */
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

#include "native_depth_allocation_test_fixture.h"

#include "core/config/engine.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "servers/rendering/rendering_device.h"
#include "servers/rendering/rendering_server.h"

namespace {
using State = NativeDisposalWitness::State;
using Knowledge = NativeDepthAllocationProjection::ByteKnowledge;
Dictionary bytes_data(const NativeDepthAllocationProjection::Bytes &p_bytes) {
	Dictionary data;
	data["knowledge"] = p_bytes.knowledge == Knowledge::Known ? "Known" : "UnknownNotExposed";
	if (p_bytes.knowledge == Knowledge::Known) {
		data["bytes"] = p_bytes.bytes;
	}
	return data;
}
Dictionary witness_data(const NativeDisposalWitness::Report &p_report) {
	Dictionary data;
	data["state"] = int(p_report.state);
	data["rid"] = p_report.rid;
	data["serial"] = p_report.serial;
	return data;
}
Dictionary projection_data(const NativeDepthAllocationProjection::Report &p_report) {
	Dictionary data;
	data["depth"] = witness_data(p_report.depth);
	data["framebuffer"] = witness_data(p_report.framebuffer);
	data["shared_attachment_depth"] = witness_data(p_report.shared_attachment_depth);
	data["framebuffer_created"] = p_report.framebuffer_created;
	data["depth_driver_backing"] = bytes_data(p_report.depth_driver_backing);
	data["framebuffer_driver_backing"] = bytes_data(p_report.framebuffer_driver_backing);
	data["framebuffer_object"] = bytes_data(p_report.framebuffer_object);
	data["attachment_reference"] = bytes_data(p_report.attachment_reference);
	data["opaque_driver_overhead"] = bytes_data(p_report.opaque_driver_overhead);
	data["all_created_children_returned"] = p_report.all_created_children_returned;
	return data;
}
bool same_identity(const NativeDisposalWitness::Report &p_a, const NativeDisposalWitness::Report &p_b) {
	return p_a.rid && p_a.serial && p_a.rid == p_b.rid && p_a.serial == p_b.serial;
}
} // namespace

void NativeDepthAllocationTestFixture::_bind_methods() {
	ClassDB::bind_method(D_METHOD("run", "case_index"), &NativeDepthAllocationTestFixture::run);
	ClassDB::bind_method(D_METHOD("result"), &NativeDepthAllocationTestFixture::result);
}

bool NativeDepthAllocationTestFixture::check(bool p_ok, const String &p_label) {
	MutexLock lock(result_mutex);
	Dictionary row;
	row["check"] = p_label;
	row["ok"] = p_ok;
	checks.push_back(row);
	if (!p_ok) {
		failures.push_back(p_label);
	}
	return p_ok;
}

Error NativeDepthAllocationTestFixture::finish(Error p_error) {
	MutexLock lock(result_mutex);
	outcome = p_error != OK ? p_error : failures.is_empty() ? OK
															: ERR_BUG;
	complete = true;
	return outcome;
}

Error NativeDepthAllocationTestFixture::run(int p_case) {
	{
		MutexLock lock(result_mutex);
		if (started) {
			return ERR_ALREADY_IN_USE;
		}
		started = true;
		case_index = p_case;
	}
	const uint64_t start = OS::get_singleton()->get_ticks_usec();
	const uint64_t draw_count = Engine::get_singleton()->get_frames_drawn();
	RenderingServer *rs = RenderingServer::get_singleton();
	RenderingDevice *rd = RenderingDevice::get_singleton();
	if (!check(rs && rd && rs->is_on_render_thread() && rs->get_rendering_device() == rd && rd->is_main_instance, "main_rd_rt")) {
		return finish(ERR_UNAVAILABLE);
	}
	if (!check(!rs->is_render_loop_enabled() && !rd->draw_list.active && !rd->compute_list.active && !rd->raytracing_list.active, "disabled_outside_lists")) {
		return finish(ERR_BUSY);
	}
	{
		MutexLock lock(result_mutex);
		device["driver"] = OS::get_singleton()->get_current_rendering_driver_name();
		device["renderer"] = OS::get_singleton()->get_current_rendering_method();
		device["name"] = rd->get_device_name();
		device["api"] = rd->get_device_api_version();
	}
	if (!check(p_case == 0 || p_case == 1, "fixed_case")) {
		return finish(ERR_INVALID_PARAMETER);
	}
	const uint64_t maximum = rd->limit_get(RenderingDevice::LIMIT_MAX_TEXTURE_SIZE_2D);
	const uint64_t before_backing = rd->next_backing_serial.load();
	const uint64_t before_native = rd->next_native_disposal_serial;
	const uint64_t before_memory = rd->texture_memory;
	const uint64_t before_frames = rd->frames_drawn;
	const int before_frame = rd->frame;
	const auto rejected_extent = [&](uint32_t p_width, uint32_t p_height, const String &p_name) {
		NativeDepthAllocationQuote rejected;
		const Error rejected_error = rd->_native_disposal_depth_allocation_quote(p_width, p_height, rejected);
		NativeDepthAllocationQuote::Report empty;
		empty.width = 123;
		empty.texture_native_requirement = { Knowledge::Known, 123 };
		const Error observed_empty = rd->_native_disposal_depth_allocation_quote_observe(rejected, empty);
		const bool unchanged = rd->next_backing_serial.load() == before_backing && rd->next_native_disposal_serial == before_native && rd->texture_memory == before_memory && rd->frames_drawn == before_frames && rd->frame == before_frame && Engine::get_singleton()->get_frames_drawn() == draw_count;
		Dictionary row;
		row["width"] = p_width;
		row["height"] = p_height;
		row["error"] = int(rejected_error);
		row["observe_error"] = int(observed_empty);
		row["quote_empty"] = empty.width == 0 && empty.height == 0 && empty.policy == NativeDepthAllocationQuote::Policy::Unavailable && empty.texture_native_requirement.knowledge == Knowledge::UnknownNotExposed && empty.texture_owner_reservation.knowledge == Knowledge::UnknownNotExposed && empty.alignment == 0 && empty.allocations == 0 && empty.parts == 0;
		row["original_counters_unchanged"] = unchanged;
		{
			MutexLock lock(result_mutex);
			boundary_data[p_name] = row;
		}
		return rejected_error == ERR_INVALID_PARAMETER && observed_empty == ERR_UNAUTHORIZED && bool(row["quote_empty"]) && unchanged;
	};
	const bool zero_width = rejected_extent(0, 64, "zero_width");
	const bool zero_height = rejected_extent(64, 0, "zero_height");
	if (!check(zero_width && zero_height, "quote_zero_extent_rejected")) {
		return finish(ERR_BUG);
	}
	bool over_width = false, over_height = false;
	if (maximum && maximum < UINT32_MAX) {
		over_width = rejected_extent(uint32_t(maximum + 1), 1, "over_width");
		over_height = rejected_extent(1, uint32_t(maximum + 1), "over_height");
	}
	{
		MutexLock lock(result_mutex);
		boundary_data["maximum_texture_size_2d"] = maximum;
	}
	if (!check(over_width && over_height, "quote_over_limit_rejected")) {
		return finish(ERR_BUG);
	}
	const uint32_t width = p_case == 0 ? 64 : 8192;
	const uint32_t height = p_case == 0 ? 64 : 4096;
	NativeDepthAllocationQuote quote;
	Error error = rd->_native_disposal_depth_allocation_quote(width, height, quote);
	// Typed unsupported stays unavailable. It is never a successful GPU case.
	if (!check(error == OK, "quote_issued")) {
		return finish(error);
	}
	NativeDepthAllocationQuote::Report quoted;
	error = rd->_native_disposal_depth_allocation_quote_observe(quote, quoted);
	if (!check(error == OK, "quote_observed")) {
		return finish(error);
	}
	{
		MutexLock lock(result_mutex);
		quote_data["width"] = quoted.width;
		quote_data["height"] = quoted.height;
		quote_data["policy"] = int(quoted.policy);
		quote_data["texture_native_requirement"] = bytes_data(quoted.texture_native_requirement);
		quote_data["texture_owner_reservation"] = bytes_data(quoted.texture_owner_reservation);
		quote_data["framebuffer_owner_reservation"] = bytes_data(quoted.framebuffer_owner_reservation);
		quote_data["framebuffer_driver_backing"] = bytes_data(quoted.framebuffer_driver_backing);
		quote_data["opaque_driver_overhead"] = bytes_data(quoted.opaque_driver_overhead);
		quote_data["alignment"] = quoted.alignment;
		quote_data["allocations"] = quoted.allocations;
		quote_data["parts"] = quoted.parts;
		quote_data["shared_attachment_references"] = quoted.shared_attachment_references;
	}
	if (!check(quoted.width == width && quoted.height == height && quoted.policy == (p_case == 0 ? NativeDepthAllocationQuote::Policy::MetalHeapPool : NativeDepthAllocationQuote::Policy::MetalHeapDedicated), "quote_descriptor_policy") ||
			!check(quoted.texture_native_requirement.knowledge == Knowledge::Known && quoted.texture_owner_reservation.knowledge == Knowledge::Known && quoted.texture_native_requirement.bytes > 0 && quoted.texture_native_requirement.bytes <= quoted.texture_owner_reservation.bytes && quoted.alignment && !(quoted.alignment & (quoted.alignment - 1)), "quote_native_bytes") ||
			!check(quoted.allocations == 2 && quoted.parts == 1 && quoted.shared_attachment_references == 1 && quoted.framebuffer_owner_reservation.knowledge == Knowledge::Known && quoted.framebuffer_owner_reservation.bytes == 0 && quoted.framebuffer_driver_backing.knowledge == Knowledge::UnknownNotExposed && quoted.opaque_driver_overhead.knowledge == Knowledge::UnknownNotExposed, "quote_scope_unknown")) {
		return finish(ERR_BUG);
	}
	RID texture_rid;
	NativeDepthAllocationQuote::CreateOutcome created;
	error = rd->_native_disposal_depth_texture_create_quoted(quote, texture_rid, depth, depth_projection, created);
	{
		MutexLock lock(result_mutex);
		prefix_created = texture_rid.is_valid();
		create_error = error;
		create_outcome = created;
	}
	if (!check(error == OK && created == NativeDepthAllocationQuote::CreateOutcome::CreatedVerified && texture_rid.is_valid(), "quoted_create_verified")) {
		// Original witness remains retained in this fixture on a failed actual
		// verification. No failure grants a refund or silently becomes success.
		return finish(error != OK ? error : ERR_BUG);
	}
	NativeDepthAllocationProjection::Report depth_live;
	error = rd->_native_disposal_depth_allocation_observe(depth_projection, depth_live);
	if (!check(error == OK && depth_live.depth.state == State::LIVE && depth_live.depth.rid == texture_rid.get_id() && !depth_live.framebuffer_created && !depth_live.all_created_children_returned && depth_live.depth_driver_backing.knowledge == Knowledge::Known && depth_live.depth_driver_backing.bytes > 0 && depth_live.depth_driver_backing.bytes <= quoted.texture_owner_reservation.bytes, "actual_depth_within_quote")) {
		return finish(error != OK ? error : ERR_BUG);
	}
	RID framebuffer_rid;
	error = rd->_native_disposal_depth_framebuffer_create(depth, framebuffer_rid, framebuffer);
	if (!check(error == OK && framebuffer_rid.is_valid(), "framebuffer_created")) {
		return finish(error != OK ? error : ERR_BUG);
	}
	error = rd->_native_disposal_depth_allocation_capture(depth, framebuffer, pair_projection);
	if (!check(error == OK, "pair_captured")) {
		return finish(error);
	}
	NativeDepthAllocationProjection::Report live;
	const auto observe = [&](const String &p_name, NativeDepthAllocationProjection::Report &r_report) {
		const Error observed = rd->_native_disposal_depth_allocation_observe(pair_projection, r_report);
		MutexLock lock(result_mutex);
		snapshots[p_name] = projection_data(r_report);
		return observed;
	};
	error = observe("live", live);
	if (!check(error == OK && live.framebuffer_created && live.depth.state == State::LIVE && live.framebuffer.state == State::LIVE && live.depth.rid == texture_rid.get_id() && live.framebuffer.rid == framebuffer_rid.get_id() && live.depth.rid != live.framebuffer.rid && live.depth.serial && live.framebuffer.serial && live.depth.serial != live.framebuffer.serial && !live.all_created_children_returned, "live_original_children") ||
			!check(same_identity(live.shared_attachment_depth, live.depth) && live.shared_attachment_depth.state == live.depth.state && live.depth_driver_backing.knowledge == Knowledge::Known && live.depth_driver_backing.bytes == depth_live.depth_driver_backing.bytes, "shared_attachment_identity") ||
			!check(live.framebuffer_driver_backing.knowledge == Knowledge::UnknownNotExposed && live.framebuffer_object.knowledge == Knowledge::UnknownNotExposed && live.attachment_reference.knowledge == Knowledge::UnknownNotExposed && live.opaque_driver_overhead.knowledge == Knowledge::UnknownNotExposed, "unknown_overhead_preserved")) {
		return finish(error != OK ? error : ERR_BUG);
	}
	// The original framebuffer factory creates an RDG cache first. Materialize
	// a real driver framebuffer through a real D32 clear before testing return;
	// retiring an empty lazy cache would not exercise driver framebuffer_free.
	const auto clear_list = rd->draw_list_begin(framebuffer_rid, RenderingDevice::DRAW_CLEAR_DEPTH, {}, 0.375f);
	if (!check(clear_list != RenderingDevice::INVALID_ID, "depth_clear_recorded")) {
		return finish(ERR_CANT_CREATE);
	}
	rd->draw_list_end();
	constexpr uint64_t DEADLINE_USEC = 20000000;
	bool materialized = false;
	while (OS::get_singleton()->get_ticks_usec() - start < DEADLINE_USEC) {
		{
			MutexLock lock(result_mutex);
			++maintenance_calls;
		}
		error = rd->_native_disposal_maintenance();
		if (error != OK && error != ERR_BUSY) {
			break;
		}
		// Only inspect after an entire original maintenance continuation has
		// finished. Both original children remain live during materialization.
		if (error == OK) {
			const auto *fb = rd->framebuffer_owner.get_or_null(framebuffer_rid);
			if (fb && fb->framebuffer_cache && !fb->framebuffer_cache->storage_map.is_empty()) {
				materialized = true;
				for (const auto &entry : fb->framebuffer_cache->storage_map) {
					materialized = materialized && bool(entry.value.framebuffer) && bool(entry.value.render_pass);
					Dictionary instance;
					instance["framebuffer"] = entry.value.framebuffer.id;
					instance["render_pass"] = entry.value.render_pass.id;
					MutexLock lock(result_mutex);
					native_framebuffer_instances.push_back(instance);
				}
				break;
			}
		}
		OS::get_singleton()->delay_usec(1000);
	}
	NativeDepthAllocationProjection::Report materialized_live;
	const Error observed_materialized = rd->_native_disposal_depth_allocation_observe(pair_projection, materialized_live);
	if (!check(error == OK && materialized && observed_materialized == OK && materialized_live.depth.state == State::LIVE && materialized_live.framebuffer.state == State::LIVE && !materialized_live.all_created_children_returned, "original_driver_framebuffer_materialized")) {
		return finish(error != OK && error != ERR_BUSY ? error : ERR_BUG);
	}
	// Force a real one-child-returned state: keep depth LIVE until FB returns.
	error = rd->_native_disposal_free(framebuffer);
	if (!check(error == OK, "retire_framebuffer")) {
		return finish(error);
	}
	NativeDepthAllocationProjection::Report enqueued;
	error = observe("framebuffer_enqueued", enqueued);
	if (!check(error == OK && enqueued.framebuffer.state == State::ENQUEUED && enqueued.depth.state == State::LIVE && !enqueued.all_created_children_returned && !rd->framebuffer_owner.owns(framebuffer_rid) && rd->texture_owner.owns(texture_rid), "framebuffer_enqueued_not_returned")) {
		return finish(error != OK ? error : ERR_BUG);
	}
	const auto drain = [&](bool p_pair, NativeDepthAllocationProjection::Report &r_report) {
		while (OS::get_singleton()->get_ticks_usec() - start < DEADLINE_USEC) {
			{
				MutexLock lock(result_mutex);
				++maintenance_calls;
			}
			Error step = rd->_native_disposal_maintenance();
			if (step != OK && step != ERR_BUSY) {
				return step;
			}
			step = rd->_native_disposal_depth_allocation_observe(pair_projection, r_report);
			if (step != OK || r_report.depth.state == State::UNKNOWN || r_report.framebuffer.state == State::UNKNOWN) {
				return step != OK ? step : ERR_INVALID_DATA;
			}
			// Independently check the conjunction on EVERY observation, including
			// partial-return states. Never assign or infer a witness from elapsed time.
			if (r_report.all_created_children_returned != (r_report.depth.state == State::RETURNED && r_report.framebuffer.state == State::RETURNED)) {
				return ERR_BUG;
			}
			if (p_pair ? r_report.all_created_children_returned : r_report.framebuffer.state == State::RETURNED) {
				return OK;
			}
			OS::get_singleton()->delay_usec(1000);
		}
		return ERR_TIMEOUT;
	};
	NativeDepthAllocationProjection::Report half_returned;
	error = drain(false, half_returned);
	if (!check(error == OK && half_returned.framebuffer.state == State::RETURNED && half_returned.depth.state == State::LIVE && !half_returned.all_created_children_returned, "only_framebuffer_driver_returned")) {
		return finish(error != OK ? error : ERR_BUG);
	}
	error = observe("framebuffer_returned", half_returned);
	if (!check(error == OK && same_identity(live.framebuffer, half_returned.framebuffer) && same_identity(live.depth, half_returned.depth) && same_identity(half_returned.shared_attachment_depth, half_returned.depth), "half_returned_original_identities")) {
		return finish(error != OK ? error : ERR_BUG);
	}
	error = rd->_native_disposal_free(depth);
	if (!check(error == OK, "retire_depth")) {
		return finish(error);
	}
	error = observe("depth_enqueued", enqueued);
	if (!check(error == OK && enqueued.depth.state == State::ENQUEUED && enqueued.framebuffer.state == State::RETURNED && !enqueued.all_created_children_returned && !rd->texture_owner.owns(texture_rid), "depth_enqueued_not_returned")) {
		return finish(error != OK ? error : ERR_BUG);
	}
	NativeDepthAllocationProjection::Report returned;
	error = drain(true, returned);
	if (!check(error == OK && returned.depth.state == State::RETURNED && returned.framebuffer.state == State::RETURNED && returned.all_created_children_returned, "both_driver_returned")) {
		return finish(error != OK ? error : ERR_BUG);
	}
	error = observe("returned", returned);
	if (!check(error == OK && same_identity(live.depth, returned.depth) && same_identity(live.framebuffer, returned.framebuffer) && same_identity(returned.shared_attachment_depth, returned.depth) && returned.shared_attachment_depth.state == State::RETURNED && returned.depth_driver_backing.knowledge == Knowledge::Known && returned.depth_driver_backing.bytes == live.depth_driver_backing.bytes, "returned_original_identities_bytes") ||
			!check(rd->_native_disposal_free(depth) == OK && rd->_native_disposal_free(framebuffer) == OK, "repeat_retire") ||
			!check(Engine::get_singleton()->get_frames_drawn() == draw_count && !rs->is_render_loop_enabled(), "no_next_draw")) {
		return finish(error != OK ? error : ERR_BUG);
	}
	{
		MutexLock lock(result_mutex);
		physical_prefix_returned = true;
		elapsed_usec = OS::get_singleton()->get_ticks_usec() - start;
	}
	return finish(OK);
}

Dictionary NativeDepthAllocationTestFixture::result() const {
	MutexLock lock(result_mutex);
	Dictionary report;
	report["schema"] = 1;
	report["started"] = started;
	report["complete"] = complete;
	report["error"] = outcome;
	report["case_index"] = case_index;
	report["create_error"] = create_error;
	report["create_outcome"] = int(create_outcome);
	report["checks"] = checks.duplicate(true);
	report["failures"] = failures.duplicate(true);
	report["device"] = device.duplicate(true);
	report["quote"] = quote_data.duplicate(true);
	report["boundaries"] = boundary_data.duplicate(true);
	report["snapshots"] = snapshots.duplicate(true);
	report["native_framebuffer_instances"] = native_framebuffer_instances.duplicate(true);
	report["maintenance_calls"] = maintenance_calls;
	report["elapsed_usec"] = elapsed_usec;
	report["deadline_usec"] = 20000000;
	report["physical_prefix_returned"] = physical_prefix_returned;
	report["retained_on_failure"] = prefix_created && !physical_prefix_returned;
	report["producer_complete"] = false;
	report["whole_output_released"] = false;
	report["reservation_refunded"] = false;
	return report;
}
