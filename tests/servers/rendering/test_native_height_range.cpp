/**************************************************************************/
/*  test_native_height_range.cpp                                          */
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

// These controls exercise the original RD reducers, not actual GPU completion.
#include "core/os/os.h"
#include "servers/rendering/native_height_range.h"
#include "servers/rendering/rendering_device.h"
#include "servers/rendering/renderer_rd/height_capture_checked_sink.h"
#include "servers/rendering/renderer_rd/storage_rd/height_frame_resources.h"
#include "tests/servers/rendering/transfer_test_driver_base.h"
#include "tests/test_macros.h"

#include <cstdio>
#include <cstdlib>

TEST_FORCE_LINK(test_native_height_range)

class TestNativeHeightRangeAccess {
	using RDD = RenderingDeviceDriver;
	static void child_require(bool condition) {
		if (!condition) {
			std::fprintf(stderr, "NHR_FINALIZE_ASSERTION_FAILED\n");
			std::fflush(stderr);
			std::_Exit(89);
		}
	}
	class Driver : public TransferTestDriverBase {
	public:
		uint32_t retains = 0, polls = 0, releases = 0, recycles = 0, issues = 0, command_ends = 0, segment_ends = 0;
		uint32_t texture_frees = 0;
		uint32_t dispatches = 0, segment_begins = 0;
		void command_compute_dispatch(CommandBufferID, uint32_t x, uint32_t y, uint32_t z) override { CHECK(x == 1); CHECK(y == 1); CHECK(z == 1); ++dispatches; }
		bool command_buffer_begin(CommandBufferID) override { return true; }
		void begin_segment(uint32_t, uint32_t) override { ++segment_begins; }
		Error qualification_prepare_segment(QueueCallReport &out, bool) override { out = {}; return OK; }
		FencePollResult fence_poll(FenceID, uint64_t serial) override { CHECK(serial == 901); return completion; }
		bool depth_resources = false;
		bool retain_busy = false, retain_fail = false, issue_fail = false, quote_fail = false, quote_excess = false, release_busy = false, release_fail = false;
		bool finalize_sentinel = false;
		RDD::FencePollResult completion;
		TextureID texture_create(const TextureFormat &, const TextureView &) override { CHECK(depth_resources); return TextureID(51); }
		void texture_free(TextureID id) override { CHECK(depth_resources); CHECK(id == TextureID(51)); ++texture_frees; }
		uint64_t texture_get_allocation_size(TextureID) override { CHECK(depth_resources); return 128; }
		uint64_t limit_get(Limit) override { CHECK(depth_resources); return 8192; }
		uint64_t api_trait_get(ApiTrait trait) override { return depth_resources ? (trait == API_TRAIT_HONORS_PIPELINE_BARRIERS ? 1 : 0) : TransferTestDriverBase::api_trait_get(trait); }
		BitField<TextureUsageBits> texture_get_usages_supported_by_format(DataFormat, bool) override { CHECK(depth_resources); return TEXTURE_USAGE_SAMPLING_BIT | TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT; }
		RenderPassID render_pass_create(VectorView<Attachment>, VectorView<Subpass>, VectorView<SubpassDependency>, uint32_t, AttachmentReference) override { CHECK(depth_resources); return RenderPassID(101); }
		void render_pass_free(RenderPassID) override { CHECK(depth_resources); }
		void set_object_name(ObjectType, ID, const String &) override { CHECK(depth_resources); }
		bool native_call_observation_supported() const override { return true; }
		bool native_issue_bound(NativeIssueKind kind, uint32_t commands, uint32_t &bound) const override {
			if (kind == NativeIssueKind::PREPARE_SEGMENT && commands == 0) { bound = 0; return true; }
			if ((kind != NativeIssueKind::MAIN_SLOT && kind != NativeIssueKind::TRANSFER_WORKER) || commands != 1) {
				return false;
			}
			bound = 1;
			return true;
		}
		Error qualification_worker_source_quote(CommandBufferID, QueueCallReport &prefix, uint32_t &future) const override {
			prefix = {};
			future = quote_excess ? 17 : 1;
			return quote_fail ? FAILED : OK;
		}
		Error fence_recycle(FenceID, uint64_t) override { ++recycles; return OK; }
		bool command_pool_reset(CommandPoolID) override { return true; }
		Error native_call_retain(uint64_t serial) override {
			if (finalize_sentinel) {
				child_require(serial == 71);
			} else {
				CHECK(serial == 71);
			}
			++retains;
			if (retain_fail) return FAILED;
			return retain_busy ? ERR_BUSY : OK;
		}
		FencePollResult native_call_poll(uint64_t serial) override {
			if (finalize_sentinel) {
				child_require(serial == 71);
			} else {
				CHECK(serial == 71);
			}
			++polls;
			return completion;
		}
		FencePollResult native_call_wait(uint64_t serial) override { return native_call_poll(serial); }
		Error native_call_recycle(uint64_t serial) override {
			if (finalize_sentinel) {
				child_require(serial == 71);
			} else {
				CHECK(serial == 71);
			}
			++recycles;
			return OK;
		}
		Error native_call_recycle_wait(uint64_t serial) override { return native_call_recycle(serial); }
		void command_pool_free(CommandPoolID) override {
			if (finalize_sentinel) {
				std::fprintf(stderr, "NHR_DEPENDENT_FREE\n");
				std::fflush(stderr);
				std::_Exit(87);
			}
			CRASH_NOW_MSG("Unexpected native-height fixture pool free.");
			std::abort();
		}
		void command_buffer_end(CommandBufferID) override { ++command_ends; }
		void end_segment() override { ++segment_ends; }
		Error command_queue_execute_and_present(CommandQueueID, VectorView<SemaphoreID>, VectorView<CommandBufferID>, VectorView<SemaphoreID>, FenceID, VectorView<SwapChainID>) override {
			++issues;
			record_queue_call_report(TestNativeHeightRangeAccess::report());
			return issue_fail ? FAILED : OK;
		}
		Error native_call_release(uint64_t serial) override {
			if (finalize_sentinel) {
				child_require(serial == 71);
			} else {
				CHECK(serial == 71);
			}
			++releases;
			if (release_fail) {
				return FAILED;
			}
			return release_busy ? ERR_BUSY : OK;
		}
	};
	struct Fixture {
		Driver driver;
		RenderingDevice rd;
		Fixture() {
			rd.driver = &driver;
			rd.is_main_instance = true;
			rd.render_thread_id = Thread::get_caller_id();
			rd.frames.resize(1);
			rd.frames[0].qualification_epoch = 1;
			rd.frames[0].primary_command_buffer_begun = true;
		}
		~Fixture() {
			// Test-owned synthetic rows only. Actual resource/graph cases must use
			// original explicit return and ordinary disposal before detaching.
			rd.native_height_ranges.clear();
			rd.frames.clear();
			rd.driver = nullptr;
		}
	};
	static Ref<NativeHeightRange> sealed(Fixture &f) {
		Ref<NativeHeightRange> range;
		REQUIRE(f.rd._native_height_range_prepare(range) == OK);
		REQUIRE(range.is_valid());
		// Deliberately synthetic sealed member: only reducer tests use this.
		// Actual graph mapping acceptance is a separate normal/bounded test.
		range->graph_count = 1;
		auto &member = range->graphs[0];
		member.frame = 0;
		member.frame_epoch = f.rd.frames[0].qualification_epoch;
		member.state = NativeHeightRange::MemberState::Sealed;
		member.slot = 2;
		member.command_buffer = RDD::CommandBufferID(45);
		range->recording_closed = true;
		range->has_recorded_effect = true;
		f.rd.frames[0].native_height_ranges.push_back(range);
		return range;
	}
	static RDD::QueueCallReport report() {
		RDD::QueueCallReport report;
		RDD::NativeCallFact fact;
		fact.serial = 71;
		fact.completion_serial = 901;
		fact.completion = RDD::CompletionCapability::CommandBuffer;
		fact.issue = RDD::IssueFact::KnownAccepted;
		report.calls.push_back(fact);
		report.native_issue_count = 1;
		report.submission_serial = 901;
		report.issued = true;
		return report;
	}
	static Error issue(Fixture &f, uint32_t slot, uint64_t buffer, Error error, const RDD::QueueCallReport &report) {
		return f.rd._native_height_issue_slot(0, f.rd.frames[0].qualification_epoch, slot, RDD::CommandBufferID(buffer), error, report);
	}

public:
	// Same original graph/enrollment setup as actual_reader_graph_membership_controls.
	// This mechanical fixture does not assert Scene producer/source admission.
	static Ref<NativeHeightRange> actual_closed_prefix(Fixture &f) {
		f.rd.draw_graph.initialize(&f.driver, [](RDD *, VectorView<RDD::AttachmentLoadOp>, VectorView<RDD::AttachmentStoreOp>, void *) { return RDD::RenderPassID(); }, 1, RDD::CommandQueueFamilyID(1), 0);
		f.rd.draw_graph.begin(true);
		f.rd.frames[0].command_buffer = RDD::CommandBufferID(45);
		Ref<NativeHeightRange> range;
		REQUIRE(f.rd._native_height_range_prepare(range) == OK);
		range->reader_scope = true;
		range->capture_frame = 0;
		range->capture_epoch = 1;
		f.rd.frames[0].native_height_ranges.push_back(range);
		f.rd.draw_graph.add_compute_list_begin();
		const auto dispatch = f.rd.draw_graph.add_compute_list_dispatch(1, 1, 1);
		REQUIRE(f.rd._native_height_reader_enroll(range, dispatch) == OK);
		range->reader_owns_list = true;
		range->reader_dispatches = 1;
		f.rd.native_height_active_checked_range = range;
		f.rd.compute_list.active = true;
		REQUIRE(f.rd._compute_list_end_recorded().is_valid());
		REQUIRE(range->recording_closed);
		REQUIRE(range->graph_count == 2);
		REQUIRE(range->source_count == 0);
		return range;
	}
	static Error selected_service(Fixture &f, const Ref<NativeHeightRange> &range, bool request_return, const RenderingDevice::NativeDisposalBudget &budget, RenderingDevice::NativeDisposalUsage &usage, NativeHeightRange::Snapshot &snapshot, bool &admitted) {
#ifdef NHR_BASELINE_RED
		// The closest existing original owner pump only observes sources. It
		// has no current graph issue admission; no fake issue fallback is used.
		admitted = false;
		if (request_return) f.rd._native_height_reader_request_return(range);
		return f.rd._native_height_reader_returns_step(budget, usage);
#else
		return f.rd._native_height_range_service_step(range, request_return, budget, usage, snapshot, admitted);
#endif
	}
	static void exact_paid_no_following_draw() {
		Fixture f;
		auto range = actual_closed_prefix(f);
		Ref<NativeHeightRange> foreign;
		REQUIRE(f.rd._native_height_range_prepare(foreign) == OK);
		f.driver.completion.completed = true;
		f.driver.completion.completed_serial = 901;
		f.driver.completion.resolved_issue = RDD::IssueFact::KnownAccepted;
		f.driver.retain_busy = true;
		NativeHeightRange::Snapshot snapshot;
		bool admitted = false;
		// A finite original grant is reused with cumulative usage. No swap,
		// draw, finalization, direct issue hook, or prepared cursor is invoked.
		RenderingDevice::NativeDisposalBudget grant{ 32, 64, UINT64_MAX };
		RenderingDevice::NativeDisposalUsage usage;
		const Error paused = selected_service(f, range, false, grant, usage, snapshot, admitted);
		CHECK(paused == ERR_BUSY);
#ifndef NHR_BASELINE_RED
		CHECK_MESSAGE(snapshot.has_recorded_effect, "NHR_R2_01_PENDING: admitted Busy retains the actual recorded prefix");
		CHECK(snapshot.recording_closed);
		CHECK(snapshot.mapping_closed);
		CHECK(snapshot.issue_closed);
		CHECK(snapshot.graph_members == 2);
		CHECK(snapshot.sealed_members == 2);
		CHECK_MESSAGE(snapshot.issued_sources == 1, "NHR_R2_01_PENDING: admitted Busy projects the actual original issue source");
		CHECK(snapshot.retained_sources == 0);
		CHECK(snapshot.completed_sources == 0);
		CHECK_FALSE(snapshot.native_complete);
		CHECK_FALSE(snapshot.prefix_settled);
		CHECK_FALSE(snapshot.observers_returned);
		CHECK(snapshot.issue_error == OK);
		CHECK(snapshot.retention_error == OK);
		CHECK(f.driver.polls == 0);
		CHECK(usage.polls == 0);
		CHECK(f.driver.releases == 0);
#endif
		CHECK_MESSAGE(f.driver.issues == 1, "NHR_NO_DRAW_RED: original closed prefix must issue once without following draw");
		CHECK_MESSAGE(range->source_count == 1, "NHR_NO_DRAW_RED: original issue report must supply its native source");
		if (!range->source_count) {
			// An expected old-source assertion failure is sufficient RED. Leave
			// test-owned graph state safe; never use a destructor crash oracle.
			f.rd.draw_graph.finalize();
			return;
		}
		CHECK(admitted);
		CHECK_FALSE(range->sources[0].retained);
		CHECK(f.rd.native_disposal_active);
		CHECK(f.rd.native_disposal_continuation.phase == 2);
#ifndef NHR_BASELINE_RED
		CHECK(f.rd.native_disposal_continuation.height_selection == range);
#endif
		CHECK(f.rd.frames[0].qualification_epoch == 1);
		RenderingDevice::NativeDisposalUsage foreign_usage;
		CHECK(selected_service(f, foreign, true, grant, foreign_usage, snapshot, admitted) == ERR_BUSY);
		CHECK_FALSE(admitted);
		CHECK_FALSE(foreign->owner_requested_return);
		CHECK(foreign_usage.passes == 0);
		f.driver.retain_busy = false;
		const auto issued = f.driver.issues;
		for (uint32_t turn = 0; turn < 16 && !range->native_complete; ++turn) {
			const auto result = selected_service(f, range, false, grant, usage, snapshot, admitted);
			CHECK((result == OK || result == ERR_BUSY));
		}
		CHECK(range->native_complete);
		CHECK(f.driver.issues == issued);
		CHECK(range->sources[0].retained);
		CHECK_FALSE(range->owner_requested_return);
		CHECK_FALSE(range->observers_returned);
		CHECK(f.driver.releases == 0);
		CHECK_FALSE(f.rd.native_disposal_active);
		CHECK(f.driver.dispatches == 1);
		CHECK(f.driver.segment_begins == 1);
		CHECK(usage.retains == f.driver.retains);
		CHECK(usage.issues == f.driver.issues);
		const auto before_return = usage;
		CHECK(selected_service(f, range, true, grant, usage, snapshot, admitted) == OK);
		CHECK(range->observers_returned);
		CHECK(snapshot.observers_returned);
		CHECK(f.driver.releases == 1);
		CHECK(usage.releases == 1);
		CHECK(usage.passes > before_return.passes);
		CHECK(usage.passes <= grant.passes);
		CHECK(usage.polls + usage.frees + usage.issues + usage.retains + usage.releases + usage.recycles <= grant.receipts);
		f.rd.draw_graph.finalize();
	}
	static void exact_paid_refusal_controls() {
#ifndef NHR_BASELINE_RED
		Fixture f;
		auto range = actual_closed_prefix(f);
		NativeHeightRange::Snapshot snapshot;
		bool admitted = true;
		RenderingDevice::NativeDisposalUsage usage;
		const RenderingDevice::NativeDisposalBudget valid{ 16, 32, UINT64_MAX };
		for (const auto grant : { RenderingDevice::NativeDisposalBudget{}, RenderingDevice::NativeDisposalBudget{16,32,1} }) {
			CHECK(selected_service(f, range, true, grant, usage, snapshot, admitted) == ERR_BUSY);
			CHECK_FALSE(admitted);
			CHECK_FALSE(range->owner_requested_return);
			CHECK_FALSE(range->owner_service_work_admitted);
			CHECK_FALSE(f.rd.native_disposal_active);
			CHECK(usage.passes == 0);
		}
		usage.passes = valid.passes;
		CHECK(selected_service(f, range, true, valid, usage, snapshot, admitted) == ERR_BUSY);
		CHECK_FALSE(admitted);
		CHECK_FALSE(range->owner_requested_return);
		usage = {};
		usage.issues = valid.receipts;
		CHECK(selected_service(f, range, true, valid, usage, snapshot, admitted) == ERR_BUSY);
		CHECK_FALSE(admitted);
		CHECK_FALSE(range->owner_requested_return);
		usage = {};
		f.rd.frames[0].present_attempted = true;
		CHECK(selected_service(f, range, true, valid, usage, snapshot, admitted) == ERR_BUSY);
		CHECK_FALSE(admitted);
		CHECK_FALSE(range->owner_requested_return);
		CHECK(usage.passes == 0);
		f.rd.frames[0].present_attempted = false;
		// This actual second closed command is absent from the original range.
		f.rd.draw_graph.add_compute_list_begin();
		f.rd.draw_graph.add_compute_list_dispatch(1, 1, 1);
		f.rd.draw_graph.add_compute_list_end();
		CHECK(selected_service(f, range, true, valid, usage, snapshot, admitted) == ERR_BUSY);
		CHECK_FALSE(admitted);
		CHECK_FALSE(range->owner_requested_return);
		CHECK_FALSE(range->owner_service_work_admitted);
		CHECK_FALSE(f.rd.native_disposal_active);
		CHECK(usage.passes == 0);
		CHECK(f.driver.issues == 0);
		CHECK(f.driver.retains == 0);
		CHECK(range->source_count == 0);
		f.rd.draw_graph.finalize();
#endif
	}
	static void exact_paid_sticky_prefix_return() {
#ifndef NHR_BASELINE_RED
		for (bool fail_release : { false, true }) {
			Fixture f;
			auto range = actual_closed_prefix(f);
			// Existing known business-failure reducer control, attached to actual
			// recorded members rather than a synthetic sealed member/source.
			range->append_error = ERR_CANT_CREATE;
			f.driver.completion.completed = true;
			f.driver.completion.completed_serial = 901;
			f.driver.completion.resolved_issue = RDD::IssueFact::KnownAccepted;
			RenderingDevice::NativeDisposalBudget grant{ 64, 128, UINT64_MAX };
			RenderingDevice::NativeDisposalUsage usage;
			NativeHeightRange::Snapshot snapshot;
			bool admitted = false;
			CHECK(selected_service(f, range, false, grant, usage, snapshot, admitted) == ERR_CANT_CREATE);
			REQUIRE(range->prefix_settled);
			CHECK_FALSE(range->native_complete);
			f.driver.release_fail = fail_release;
			CHECK(selected_service(f, range, true, grant, usage, snapshot, admitted) == (fail_release ? FAILED : ERR_CANT_CREATE));
			CHECK(snapshot.append_error == ERR_CANT_CREATE);
			CHECK(snapshot.return_error == (fail_release ? FAILED : OK));
			CHECK(snapshot.observers_returned == !fail_release);
			CHECK(f.driver.releases == 1);
			if (fail_release) {
				f.driver.release_fail = false;
				CHECK(selected_service(f, range, true, grant, usage, snapshot, admitted) == FAILED);
				CHECK(f.driver.releases == 1); // Sticky physical failure never replays.
			}
			f.rd.draw_graph.finalize();
		}
#endif
	}
	static void exact_paid_one_pass_grants() {
#ifndef NHR_BASELINE_RED
		Fixture f;
		auto range = actual_closed_prefix(f);
		f.driver.completion.completed = true;
		f.driver.completion.completed_serial = 901;
		f.driver.completion.resolved_issue = RDD::IssueFact::KnownAccepted;
		NativeHeightRange::Snapshot snapshot;
		bool admitted = false;
		for (uint32_t original_grant = 0; original_grant < 32 && !range->observers_returned; ++original_grant) {
			// Each iteration models a distinct explicit original payer grant.
			// A duplicate execution of that same grant shares the original usage.
			const RenderingDevice::NativeDisposalBudget one{ 1, 1, UINT64_MAX };
			RenderingDevice::NativeDisposalUsage usage;
			const auto before = f.driver.issues + f.driver.retains + f.driver.polls + f.driver.releases + f.driver.recycles;
			const Error result = selected_service(f, range, true, one, usage, snapshot, admitted);
			CHECK((result == OK || result == ERR_BUSY));
			CHECK(admitted);
			CHECK(usage.passes == 1);
			const auto receipts = usage.issues + usage.retains + usage.polls + usage.releases + usage.recycles + usage.frees;
			CHECK(receipts <= 1);
			const auto used = usage.passes;
			const auto calls = f.driver.issues + f.driver.retains + f.driver.polls + f.driver.releases + f.driver.recycles;
			CHECK(calls - before <= receipts); // Fence polls have a separate callback.
			CHECK(selected_service(f, range, true, one, usage, snapshot, admitted) == ERR_BUSY);
			CHECK_FALSE(admitted);
			CHECK(usage.passes == used);
			CHECK(f.driver.issues + f.driver.retains + f.driver.polls + f.driver.releases + f.driver.recycles == calls);
		}
		CHECK(range->observers_returned);
		CHECK(f.driver.issues == 1);
		CHECK(f.driver.dispatches == 1);
		CHECK(f.driver.releases == 1);
		f.rd.draw_graph.finalize();
#endif
	}

	static void exact_paid_early_error_snapshot() {
#ifndef NHR_BASELINE_RED
		for (bool fail_issue : { false, true }) {
			Fixture f;
			auto range = actual_closed_prefix(f);
			f.driver.issue_fail = fail_issue;
			f.driver.retain_fail = !fail_issue;
			const RenderingDevice::NativeDisposalBudget grant{ 32, 64, UINT64_MAX };
			RenderingDevice::NativeDisposalUsage usage;
			NativeHeightRange::Snapshot snapshot;
			bool admitted = false;
			CHECK(selected_service(f, range, false, grant, usage, snapshot, admitted) == FAILED);
			CHECK(admitted);
			CHECK_MESSAGE(snapshot.has_recorded_effect, "NHR_R2_01_ERROR: admitted native failure cannot erase the actual prefix");
			CHECK(snapshot.recording_closed);
			CHECK(snapshot.mapping_closed);
			CHECK(snapshot.issue_closed);
			CHECK(snapshot.graph_members == 2);
			CHECK(snapshot.sealed_members == 2);
			CHECK_MESSAGE(snapshot.issued_sources == 1, "NHR_R2_01_ERROR: admitted native failure retains its original issue report");
			CHECK(snapshot.retained_sources == (fail_issue ? 1u : 0u));
			CHECK(snapshot.completed_sources == 0);
			CHECK(snapshot.issue_error == (fail_issue ? FAILED : OK));
			CHECK(snapshot.retention_error == (fail_issue ? OK : FAILED));
			CHECK(snapshot.completion_error == OK);
			CHECK(snapshot.return_error == OK);
			CHECK_FALSE(snapshot.native_complete);
			CHECK_FALSE(snapshot.prefix_settled);
			CHECK_FALSE(snapshot.observers_returned);
			CHECK(f.driver.issues == 1);
			CHECK(f.driver.polls == 0);
			CHECK(usage.polls == 0);
			CHECK(f.driver.releases == 0);
			// The projection itself has no driver, usage, reducer or cursor effect.
			const auto issues = f.driver.issues, retains = f.driver.retains;
			const auto passes = usage.passes, retain_cursor = range->retain_cursor, poll_cursor = range->poll_cursor;
			const auto phase = f.rd.native_disposal_continuation.phase;
			f.rd._native_height_range_snapshot(range, snapshot);
			CHECK(f.driver.issues == issues);
			CHECK(f.driver.retains == retains);
			CHECK(f.driver.polls == 0);
			CHECK(usage.passes == passes);
			CHECK(range->retain_cursor == retain_cursor);
			CHECK(range->poll_cursor == poll_cursor);
			CHECK(f.rd.native_disposal_continuation.phase == phase);
			CHECK_FALSE(range->native_complete);
			CHECK_FALSE(range->prefix_settled);
			f.rd.draw_graph.finalize();
		}
#endif
	}

	static void reader_requested_returns_are_bounded_and_fair() {
		Fixture f;
		Ref<NativeHeightRange> idle, pending, empty;
		REQUIRE(f.rd._native_height_range_prepare(idle) == OK);
		pending = sealed(f);
		pending->reader_scope = true;
		REQUIRE(f.rd._native_height_range_prepare(empty) == OK);
		empty->reader_scope = true;
		CHECK(f.rd._native_height_reader_request_return(empty) == ERR_INVALID_PARAMETER);
		bool closed = false;
		REQUIRE(f.rd._native_height_reader_finish(empty, closed) == OK);
		REQUIRE(closed);
		REQUIRE(f.rd._native_height_reader_request_return(empty) == OK);
		REQUIRE(f.rd._native_height_reader_request_return(pending) == OK);
		CHECK(f.rd._native_height_reader_request_return(idle) == ERR_INVALID_PARAMETER);
		RenderingDevice::NativeDisposalUsage usage;
		CHECK(f.rd._native_height_reader_returns_step({}, usage) == ERR_BUSY);
		CHECK(f.rd.native_height_reader_return_cursor == 0);
		CHECK(usage.passes == 0);
		const RenderingDevice::NativeDisposalBudget one{ 1, 2, UINT64_MAX };
		CHECK(f.rd._native_height_reader_returns_step(one, usage) == ERR_BUSY);
		CHECK(usage.passes == 1);
		CHECK(f.rd.native_height_reader_return_cursor == 1);
		usage = {};
		CHECK(f.rd._native_height_reader_returns_step(one, usage) == ERR_BUSY);
		CHECK(!pending->observers_returned);
		CHECK(!pending->native_complete);
		CHECK(f.rd.native_height_reader_return_cursor == 2);
		usage = {};
		CHECK(f.rd._native_height_reader_returns_step(one, usage) == OK);
		CHECK(empty->observers_returned);
		CHECK(f.rd.native_height_ranges.size() == 2);
		CHECK(f.driver.polls == 0);
		CHECK(f.driver.issues == 0);
		CHECK(!idle->recording_closed);
		// Removing an earlier unrelated owner row preserves the next original slot.
		REQUIRE(f.rd._native_height_range_close(idle) == OK);
		const RenderingDevice::NativeDisposalBudget all{ UINT32_MAX, UINT32_MAX, UINT64_MAX };
		usage = {};
		REQUIRE(f.rd._native_height_range_return(idle, all, usage) == OK);
		CHECK(f.rd.native_height_reader_return_cursor == 1);
		usage = {};
		CHECK(f.rd._native_height_reader_returns_step(one, usage) == ERR_BUSY);
		CHECK(!pending->observers_returned);
		CHECK(!pending->native_complete);
	}
	static void reader_return_pump_separates_record_and_observer_errors() {
		const RenderingDevice::NativeDisposalBudget budget{ 1, 2, UINT64_MAX };
		{
			Fixture f;
			auto reader = sealed(f);
			reader->reader_scope = true;
			reader->append_error = ERR_CANT_CREATE;
			REQUIRE(f.rd._native_height_reader_request_return(reader) == OK);
			RenderingDevice::NativeDisposalUsage usage;
			CHECK(f.rd._native_height_reader_returns_step(budget, usage) == ERR_BUSY);
			CHECK(reader->append_error == ERR_CANT_CREATE);
			CHECK(!reader->observers_returned);
			// Original observe prioritizes append_error; the return pump must
			// still expose a simultaneous real observer failure.
			reader->completion_error = FAILED;
			usage = {};
			CHECK(f.rd._native_height_reader_returns_step(budget, usage) == FAILED);
			CHECK(reader->append_error == ERR_CANT_CREATE);
			CHECK(!reader->observers_returned);
		}
		{
			Fixture f;
			Ref<NativeHeightRange> reader;
			REQUIRE(f.rd._native_height_range_prepare(reader) == OK);
			reader->reader_scope = true;
			reader->append_error = ERR_CANT_CREATE;
			bool closed = false;
			REQUIRE(f.rd._native_height_reader_finish(reader, closed) == ERR_CANT_CREATE);
			REQUIRE(closed);
			REQUIRE(f.rd._native_height_reader_request_return(reader) == OK);
			RenderingDevice::NativeDisposalUsage usage;
			CHECK(f.rd._native_height_reader_returns_step(budget, usage) == OK);
			CHECK(reader->observers_returned);
			CHECK(reader->append_error == ERR_CANT_CREATE);
			CHECK(!reader->native_complete);
		}
		{
			Fixture f;
			auto reader = sealed(f);
			reader->reader_scope = true;
			reader->append_error = ERR_CANT_CREATE;
			reader->uncaptured_prefix = true;
			REQUIRE(f.rd._native_height_reader_request_return(reader) == OK);
			RenderingDevice::NativeDisposalUsage usage;
			CHECK(f.rd._native_height_reader_returns_step(budget, usage) == ERR_INVALID_DATA);
			CHECK(!reader->observers_returned);
			CHECK(reader->append_error == ERR_CANT_CREATE);
		}
	}
	static void reader_scope_refuses_forged_and_stale_admission() {
		Fixture f;
		Ref<NativeHeightRange> producer, reader;
		REQUIRE(f.rd._native_height_range_prepare(producer) == OK);
		CHECK(f.rd._native_height_reader_prepare(producer, RID(), 0, reader) == ERR_INVALID_DATA);
		CHECK(reader.is_null());
		producer->recording_closed = true;
		CHECK(f.rd._native_height_reader_prepare(producer, RID(), UINT32_MAX, reader) == ERR_INVALID_DATA);
		CHECK(producer->active_readers == 0);
		CHECK(f.rd._native_height_reader_validate_sample(producer) == ERR_INVALID_PARAMETER);
		RenderingDevice::ComputeListID list = 123;
		CHECK(f.rd._native_height_reader_begin(producer, list) == ERR_BUSY);
		CHECK(list == RenderingDevice::ComputeListID(RenderingDevice::INVALID_ID));
		bool closed = true;
		CHECK(f.rd._native_height_reader_finish(producer, closed) == ERR_INVALID_PARAMETER);
		CHECK(!closed);
		// An open reader scope cannot be guessed closed by finalization.
		REQUIRE(f.rd._native_height_range_prepare(reader) == OK);
		reader->reader_scope = true;
		CHECK(f.rd._native_height_ranges_prepare_finalization() == ERR_BUSY);
		CHECK(!reader->recording_closed);
		REQUIRE(f.rd._native_height_reader_finish(reader, closed) == OK);
		CHECK(closed);
		CHECK(f.rd._native_height_ranges_prepare_finalization() == OK);
	}
	static void worker_preflight_refusal_guard() {
		using Sink = RendererRD::HeightCaptureCheckedSink;
		Fixture f;
		f.rd.transfer_worker_pool.resize(1);
		f.rd.transfer_worker_operation_used_by_draw.resize(1);
		f.rd.transfer_worker_pool_size = 1;
		f.rd.transfer_worker_pool[0] = memnew(RenderingDevice::TransferWorker);
		auto *worker = f.rd.transfer_worker_pool[0];
		worker->index = 0;
		worker->operations_counter = 1;
		worker->operations_processed = 0;
		Sink sink(f.rd, 2, 0);
		Ref<NativeHeightRange> range;
		REQUIRE(sink.prepare_native_range(range) == OK);
		worker->operations_processed = 1;
		CHECK(f.rd._check_transfer_worker_operation(0, 1) == OK);
		CHECK(f.rd.transfer_worker_operation_used_by_draw[0] == 1);
		worker->operations_processed = 0;
		f.rd.transfer_worker_operation_used_by_draw[0] = 0;
		REQUIRE(f.rd._native_height_range_enter_checked_call(range, sink) == OK);
		CHECK(f.rd._check_transfer_worker_operation(0, 1) == ERR_PARAMETER_RANGE_ERROR);
		CHECK(range->checked_transfer_count == 0);
		CHECK(range->worker_count == 0);
		CHECK(worker->height_watcher_count == 0);
		f.rd.native_height_active_checked_range.unref();
		CHECK(worker->thread_mutex.try_lock());
		worker->thread_mutex.unlock();
		CHECK(worker->operations_mutex.try_lock());
		worker->operations_mutex.unlock();
		sink.unwind();
		range.unref();
		memdelete(worker);
		f.rd.transfer_worker_pool[0] = nullptr;
		f.rd.transfer_worker_pool.clear();
		f.rd.transfer_worker_operation_used_by_draw.clear();
		f.rd.transfer_worker_pool_size = 0;

		// The standalone clear recipe has no capture sink and keeps the legacy
		// worker check available for its ordinary caller.
		Ref<NativeHeightRange> clear_range;
		REQUIRE(f.rd._native_height_range_prepare(clear_range) == OK);
		f.rd.transfer_worker_pool.resize(1);
		f.rd.transfer_worker_operation_used_by_draw.resize(1);
		f.rd.transfer_worker_pool_size = 1;
		f.rd.transfer_worker_pool[0] = memnew(RenderingDevice::TransferWorker);
		worker = f.rd.transfer_worker_pool[0];
		worker->index = 0;
		worker->operations_counter = 1;
		worker->operations_processed = 0;
		CHECK(f.rd._check_transfer_worker_operation(0, 1) == OK);
		memdelete(worker);
		f.rd.transfer_worker_pool[0] = nullptr;
		f.rd.transfer_worker_pool.clear();
		f.rd.transfer_worker_operation_used_by_draw.clear();
		f.rd.transfer_worker_pool_size = 0;
	}
	static void worker_source_custody() {
		using Sink = RendererRD::HeightCaptureCheckedSink;
		Fixture f;
		f.rd.transfer_worker_pool.resize(1);
		f.rd.transfer_worker_operation_used_by_draw.resize(1);
		f.rd.transfer_worker_pool_size = 1;
		auto *worker = memnew(RenderingDevice::TransferWorker);
		f.rd.transfer_worker_pool[0] = worker;
		worker->operations_counter = 4;
		worker->recording = true;
		Sink sink(f.rd, 2, 2);
		Ref<NativeHeightRange> range;
		REQUIRE(sink.prepare_native_range(range) == OK);
		// An open range alone must never capture an ordinary worker call.
		CHECK(f.rd._check_transfer_worker_operation(0, 2) == OK);
		CHECK(range->worker_count == 0);
		REQUIRE(f.rd._native_height_range_enter_checked_call(range, sink) == OK);
		f.driver.quote_fail = true;
		CHECK(f.rd._check_transfer_worker_operation(0, 2) == FAILED);
		CHECK(range->worker_count == 0);
		CHECK(worker->height_watcher_count == 0);
		f.driver.quote_fail = false;
		f.driver.quote_excess = true;
		CHECK(f.rd._check_transfer_worker_operation(0, 2) == ERR_PARAMETER_RANGE_ERROR);
		CHECK(range->worker_count == 0);
		f.driver.quote_excess = false;
		CHECK(f.rd._check_transfer_worker_operation(0, 2) == OK);
		CHECK(range->worker_count == 1);
		CHECK(worker->height_observer_pending);
		f.rd.native_height_active_checked_range.unref();
		// A later reused report cannot satisfy an operation outside its exact interval.
		CHECK(f.rd._native_height_join_worker(worker, 5, 8, report()) == ERR_BUSY);
		CHECK(range->source_count == 0);
		RDD::QueueCallReport retry;
		retry.error = ERR_BUSY;
		CHECK(f.rd._native_height_join_worker(worker, 1, 4, retry) == ERR_BUSY);
		CHECK(range->source_count == 0);
		f.driver.retain_busy = true;
		worker->recording = false;
		worker->submitted = true;
		worker->submission_serial = 901;
		worker->operations_submitted = 4;
		worker->last_submission = report();
		worker->last_completion.completed = true;
		worker->last_completion.completed_serial = 901;
		CHECK(f.rd._native_height_join_worker(worker, 1, 4, worker->last_submission) == ERR_BUSY);
		CHECK(range->source_count == 1);
		CHECK(range->worker_edge_count == 1);
		CHECK(range->workers[0].attached);
		f.rd.native_height_active_checked_range = range;
		CHECK(f.rd._check_transfer_worker_operation(0, 4) == OK);
		CHECK(range->workers[0].attached);
		CHECK(range->worker_edge_count == 1);
		f.rd.native_height_active_checked_range.unref();
		CHECK_FALSE(f.rd._wait_for_transfer_worker(worker));
		CHECK(f.driver.recycles == 0);
		CHECK(worker->operations_processed == 0);
		CHECK(worker->submitted);
		f.driver.retain_busy = false;
		CHECK(f.rd._wait_for_transfer_worker(worker));
		CHECK(f.driver.recycles == 1);
		CHECK(range->sources[0].retained);
		CHECK(worker->operations_processed == 4);
		CHECK(worker->height_watcher_count == 0);
		// Synthetic graph member isolates reducer behavior; it is not GPU proof.
		range->graph_count = 1;
		range->graphs[0].state = NativeHeightRange::MemberState::IssueAttached;
		range->graphs[0].issue_attempted = true;
		range->recording_closed = true;
		range->has_recorded_effect = true;
		NativeHeightRange::Snapshot snapshot;
		CHECK(observe(f, range, snapshot) == ERR_BUSY);
		CHECK_FALSE(snapshot.native_complete);
		f.driver.completion.completed = true;
		f.driver.completion.completed_serial = 901;
		CHECK(observe(f, range, snapshot) == OK);
		CHECK(snapshot.native_complete);
		CHECK(give_back(f, range) == OK);
		CHECK(f.driver.releases == 1);
		memdelete(worker);
		f.rd.transfer_worker_pool.clear();
		f.rd.transfer_worker_pool_size = 0;
	}
	static void checked_transfer_receipt_controls() {
		using Sink = RendererRD::HeightCaptureCheckedSink;
		using Effect = RenderingDevice::NativeAppendEffect;
		// Mechanical tuple authentication only: test-owned worker state and raw
		// records below are deliberate controls, not a real VA/GPU acceptance.
		// Every success receipt is issued by the original checker under its locks.
		for (uint32_t fault = 0; fault < 9; ++fault) {
			CAPTURE(fault);
			Fixture f;
			RenderingDevice::TransferWorker worker;
			worker.index = 0;
			worker.operations_counter = 4;
			worker.operations_processed = fault == 0 ? 4 : 0;
			worker.recording = true;
			f.rd.transfer_worker_pool.resize(1);
			f.rd.transfer_worker_pool[0] = &worker;
			f.rd.transfer_worker_operation_used_by_draw.resize(1);
			f.rd.transfer_worker_pool_size = 1;
			Sink sink(f.rd, 2, 2);
			Ref<NativeHeightRange> range;
			REQUIRE(sink.prepare_native_range(range) == OK);
			REQUIRE(sink.enter_checked_call() == OK);
			if (fault != 3) {
				REQUIRE(f.rd._check_transfer_worker_operation(0, 2) == OK);
				CHECK(range->checked_transfer_count == 1);
				CHECK(range->checked_transfers[0].worker == 0);
				CHECK(range->checked_transfers[0].operation == 2);
				CHECK(range->checked_transfers[0].processed == (fault == 0));
				CHECK(range->worker_count == (fault == 0 ? 0u : 1u));
			}
			if (fault == 4 || fault == 6) {
				REQUIRE(f.rd._check_transfer_worker_operation(0, 4) == OK);
				CHECK(range->checked_transfer_count == 2);
				// The two exact operations share one original pending dependency.
				CHECK(range->worker_count == 1);
				CHECK(range->workers[0].operation == 4);
				CHECK(f.rd._check_transfer_worker_operation(0, 4) == ERR_PARAMETER_RANGE_ERROR);
				CHECK(range->checked_transfer_count == 2);
				CHECK(worker.height_watcher_count == 1);
			}
			sink.business_count = 1;
			auto &record = sink.business[0];
			record.operation = Sink::Operation::VertexArray;
			record.bind.capture_complete = true;
			record.bind.state_accepted = true;
			record.bind.transfer_count = record.transfer_count = sink.transfer_count = 1;
			sink.transfers[0] = { 0, fault == 2 ? 3u : 2u };
			if (fault == 5 || fault == 6) {
				record.bind.transfer_count = record.transfer_count = sink.transfer_count = 2;
				sink.transfers[1] = { 0, 2 };
				if (fault == 6) { sink.transfers[0].operation = 4; }
			}
			if (fault == 8) { record.bind.effect = Effect::Unknown; }
			const Error enrolled = f.rd._native_height_range_enroll_original_record(range, sink, 0, false);
			const bool covered = fault <= 1 || fault == 7;
			CHECK(enrolled == (covered ? OK : ERR_INVALID_DATA));
			CHECK(range->uncaptured_prefix == !covered);
			if (covered) {
				CHECK(range->capture_transfers == 1);
			} else {
				sink.reject(enrolled);
			}
			if (fault == 7) {
				// An actual original join attempt with an unknown report cannot be
				// repaired by a later known report, even though the tuple matched.
				auto unknown = report();
				unknown.calls.clear();
				CHECK(f.rd._native_height_join_worker(&worker, 1, 4, unknown) == ERR_INVALID_DATA);
				CHECK(f.rd._native_height_join_worker(&worker, 1, 4, report()) == ERR_INVALID_DATA);
				CHECK(range->uncaptured_prefix);
				CHECK(range->source_count == 0);
				CHECK_FALSE(range->workers[0].attached);
			}
			CHECK(sink.unwind() == (covered ? OK : ERR_INVALID_DATA));
			CHECK(range->recording_closed);
			CHECK(f.driver.issues == 0); // No subsequent draw or issue repairs a fault.
			CHECK(f.driver.polls == 0);
			if (!covered || fault == 7) {
				CHECK(give_back(f, range) == ERR_BUSY);
				CHECK_FALSE(range->native_complete);
				CHECK_FALSE(range->observers_returned);
			} else if (fault == 0) {
				CHECK(give_back(f, range) == OK); // Actual processed check, empty prefix.
				CHECK_FALSE(range->native_complete);
			}
			CHECK(f.driver.releases == 0);
			f.rd.transfer_worker_pool.clear();
			f.rd.transfer_worker_operation_used_by_draw.clear();
			f.rd.transfer_worker_pool_size = 0;
		}
	}
	static void worker_invalid_reports() {
		for (uint32_t variant = 0; variant < 3; ++variant) {
			Fixture f;
			auto range = sealed(f);
			RenderingDevice::TransferWorker worker;
			worker.height_watcher_count = 1;
			worker.height_observer_pending = true;
			worker.height_watchers[0] = { range, 0 };
			range->worker_count = 1;
			range->workers[0] = { 0, 2, false, false };
			auto bad = report();
			if (variant == 0) {
				bad.calls.clear();
			} else if (variant == 1) {
				bad.error = FAILED; // Preserve this genuine issued prefix.
			} else {
				bad.calls.write[0].completion_serial = 0;
			}
			CHECK(f.rd._native_height_join_worker(&worker, 1, 4, bad) != OK);
			CHECK(worker.height_observer_pending);
			CHECK_FALSE(range->workers[0].attached);
			CHECK(range->source_count == (variant == 1 ? 1 : 0));
			CHECK(f.driver.retains == (variant == 1 ? 1 : 0));
			CHECK(f.rd._native_height_join_worker(&worker, 1, 4, bad) != OK);
			CHECK(range->source_count == (variant == 1 ? 1 : 0));
			NativeHeightRange::Snapshot snapshot;
			CHECK(observe(f, range, snapshot) != OK);
			CHECK_FALSE(snapshot.native_complete);
			CHECK_FALSE(snapshot.issue_closed);
			CHECK(give_back(f, range) == ERR_BUSY);
		}
	}
	static Error observe(Fixture &f, const Ref<NativeHeightRange> &range, NativeHeightRange::Snapshot &out) {
		RenderingDevice::NativeDisposalBudget budget{ 64, 64, UINT64_MAX };
		RenderingDevice::NativeDisposalUsage usage;
		return RendererRD::HeightCaptureCheckedSink::observe_native_range(f.rd, range, budget, usage, out);
	}
	static Error give_back(Fixture &f, const Ref<NativeHeightRange> &range) {
		RenderingDevice::NativeDisposalBudget budget{ 64, 64, UINT64_MAX };
		RenderingDevice::NativeDisposalUsage usage;
		return RendererRD::HeightCaptureCheckedSink::return_native_range(f.rd, range, budget, usage);
	}

public:
	static void producer_reader_lifetime_controls() {
		Fixture f;
		f.driver.depth_resources = true;
		RendererRD::HeightCaptureCheckedSink sink(f.rd, 2, 0);
		using Resources = std::remove_pointer_t<decltype(sink.quoted_resources.ptr())>;
		// Real original resource factories/owner, CPU driver callbacks only.
		Ref<Resources> resources;
		REQUIRE(Resources::allocate(f.rd, 8, 4, resources) == OK);
		Ref<NativeHeightRange> producer;
		REQUIRE(sink.prepare_native_range(producer) == OK);
		// Model enrollment of that same original checked Begin result. This is
		// reducer input, not an executed renderer pass or quote admission test.
		sink.quoted_resources = resources;
		sink.business_count = 1;
		sink.business[0].operation = RendererRD::HeightCaptureCheckedSink::Operation::Begin;
		sink.business[0].resource = resources->framebuffer;
		sink.business[0].begin.error = ERR_CANT_CREATE;
		CHECK(f.rd._native_height_range_enroll_original_record(producer, sink, 0, false) == ERR_CANT_CREATE);
		CHECK(producer->producer_resources.ptr() == resources.ptr());
		sink.reject(ERR_CANT_CREATE);
		sink.unwind();
		// Existing suite's sealed reducer fixture, using actual observer issue
		// and return methods below. No GPU completion is inferred from it.
		producer->append_error = OK;
		producer->graph_count = 1;
		producer->has_recorded_effect = true;
		producer->graphs[0].frame = 0;
		producer->graphs[0].frame_epoch = 1;
		producer->graphs[0].state = NativeHeightRange::MemberState::Sealed;
		producer->graphs[0].slot = 2;
		producer->graphs[0].command_buffer = RDD::CommandBufferID(45);
		// Test-owned descriptor metadata points at the actual original texture
		// tracker; the real reader prepare method authenticates this exact set.
		RenderingDevice::UniformSet set;
		set.driver_id = RDD::UniformSetID(23);
		set.shader_set = 0;
		set.draw_trackers.push_back(f.rd.texture_owner.get_or_null(resources->depth)->draw_tracker);
		set.draw_trackers_usage.push_back(RenderingDeviceGraph::RESOURCE_USAGE_TEXTURE_SAMPLE);
		RID sampled = f.rd.uniform_set_owner.make_rid(set);
		Ref<NativeHeightRange> first, second;
		REQUIRE(f.rd._native_height_reader_prepare(producer, sampled, 0, first) == OK);
		REQUIRE(f.rd._native_height_reader_prepare(producer, sampled, 0, second) == OK);
		CHECK(producer->active_readers == 2);
		CHECK(first->producer == producer);
		CHECK(second->producer == producer);
		// Wrong sampled usage cannot add a third reader or publish a range.
		f.rd.uniform_set_owner.get_or_null(sampled)->draw_trackers_usage.write[0] = RenderingDeviceGraph::RESOURCE_USAGE_TEXTURE_BUFFER_READ_WRITE;
		Ref<NativeHeightRange> wrong;
		CHECK(f.rd._native_height_reader_prepare(producer, sampled, 0, wrong) == ERR_INVALID_DATA);
		CHECK(wrong.is_null());
		CHECK(producer->active_readers == 2);
		f.rd.uniform_set_owner.get_or_null(sampled)->draw_trackers_usage.write[0] = RenderingDeviceGraph::RESOURCE_USAGE_TEXTURE_SAMPLE;
		for (const auto &reader : { first, second }) {
			reader->recording_closed = true;
			reader->has_recorded_effect = true;
			reader->graph_count = 1;
			reader->graphs[0] = producer->graphs[0];
			f.rd.frames[0].native_height_ranges.push_back(reader);
		}
		CHECK(issue(f, 2, 45, OK, report()) == OK);
		CHECK(f.rd._native_height_retain_frame_sources(0) == OK);
		CHECK(f.driver.retains == 3);
		f.driver.completion.completed = true;
		f.driver.completion.completed_serial = 901;
		f.driver.completion.resolved_issue = RDD::IssueFact::KnownAccepted;
		NativeHeightRange::Snapshot snapshot;
		REQUIRE(observe(f, producer, snapshot) == OK);
		CHECK(snapshot.active_readers == 2);
		CHECK(give_back(f, producer) == ERR_BUSY);
		CHECK(producer->producer_resources.ptr() == resources.ptr());
		CHECK(f.driver.releases == 0);
		CHECK(resources->retire_step() == ERR_BUSY);
		CHECK(f.driver.texture_frees == 0);
		CHECK(f.rd._native_height_reader_prepare(producer, sampled, 0, wrong) == ERR_INVALID_DATA);
		// Completion alone is not the reader owner's explicit return request.
		REQUIRE(observe(f, second, snapshot) == OK);
		CHECK(give_back(f, second) == ERR_BUSY);
		REQUIRE(f.rd._native_height_reader_request_return(second) == OK);
		REQUIRE(give_back(f, second) == OK); // Out-of-order completion/return.
		CHECK(producer->active_readers == 1);
		const uint32_t released = f.driver.releases;
		CHECK(give_back(f, second) == OK);
		CHECK(f.driver.releases == released);
		CHECK(producer->active_readers == 1);
		CHECK(give_back(f, producer) == ERR_BUSY);
		// Unknown prefix must retain the remaining reader and original source.
		first->uncaptured_prefix = true;
		REQUIRE(f.rd._native_height_reader_request_return(first) == OK);
		CHECK(give_back(f, first) == ERR_BUSY);
		CHECK(producer->active_readers == 1);
		CHECK(resources->retire_step() == ERR_BUSY);
		CHECK(f.driver.texture_frees == 0);
		// Test-only repair of the deliberate Unknown control for safe teardown.
		first->uncaptured_prefix = false;
		REQUIRE(observe(f, first, snapshot) == OK);
		REQUIRE(give_back(f, first) == OK);
		CHECK(producer->active_readers == 0);
		CHECK(producer->producer_resources.ptr() == resources.ptr());
		REQUIRE(give_back(f, producer) == OK);
		CHECK(producer->producer_resources.is_null());
		CHECK(f.driver.releases == 3);
		CHECK(first->producer.is_null());
		CHECK(second->producer.is_null());
		f.rd.uniform_set_owner.free(sampled); // Test-owned descriptor metadata.
		sink.quoted_resources.unref();
		// Real original owner returns only after the last original observer.
		CHECK(resources->retire_step() == ERR_BUSY);
		f.rd._free_pending_resources(0);
		for (uint32_t i = 0; i < 4; ++i) {
			resources->retire_step();
			f.rd._free_pending_resources(0);
		}
		CHECK(resources->state == Resources::State::RELEASED);
		CHECK(f.driver.texture_frees == 1);
		resources.unref();
		for (const auto &entry : f.rd.framebuffer_formats) f.driver.render_pass_free(entry.value.render_pass);
		f.rd.framebuffer_formats.clear();
		f.rd.framebuffer_format_cache.clear();
	}
	static void actual_reader_graph_membership_controls() {
		Fixture f;
		class GraphDriver : public TransferTestDriverBase {} driver;
		RenderingDeviceGraph graph;
		graph.initialize(&driver, [](RDD *, VectorView<RDD::AttachmentLoadOp>, VectorView<RDD::AttachmentStoreOp>, void *) { return RDD::RenderPassID(); }, 1, RDD::CommandQueueFamilyID(1), 0);
		graph.begin(true);
		graph.add_compute_list_begin();
		const auto dispatch = graph.add_compute_list_dispatch(1, 1, 1);
		const auto close = graph.add_compute_list_end();
		CHECK(graph.is_instruction_in_closed_list(dispatch, close));
		graph.add_compute_list_begin();
		const auto second_dispatch = graph.add_compute_list_dispatch(1, 1, 1);
		const auto second_close = graph.add_compute_list_end();
		CHECK_FALSE(graph.is_instruction_in_closed_list(dispatch, second_close));
		CHECK_FALSE(graph.is_instruction_in_closed_list(second_dispatch, close));
		CHECK(graph.is_instruction_in_closed_list(second_dispatch, second_close));
		auto forged = dispatch;
		++forged.graph_instance;
		CHECK_FALSE(graph.is_instruction_in_closed_list(forged, close));
		// Enrollment validates the original RD graph, frame and actual handle.
		f.rd.draw_graph.initialize(&driver, [](RDD *, VectorView<RDD::AttachmentLoadOp>, VectorView<RDD::AttachmentStoreOp>, void *) { return RDD::RenderPassID(); }, 1, RDD::CommandQueueFamilyID(1), 0);
		f.rd.draw_graph.begin(true);
		Ref<NativeHeightRange> reader;
		REQUIRE(f.rd._native_height_range_prepare(reader) == OK);
		reader->reader_scope = true;
		reader->capture_frame = 0;
		reader->capture_epoch = 1;
		f.rd.draw_graph.add_compute_list_begin();
		const auto original = f.rd.draw_graph.add_compute_list_dispatch(1, 1, 1);
		REQUIRE(f.rd._native_height_reader_enroll(reader, original) == OK);
		CHECK(reader->graph_count == 1);
		reader->reader_owns_list = true;
		reader->reader_dispatches = 1;
		f.rd.native_height_active_checked_range = reader;
		f.rd.compute_list.active = true;
		const auto original_close = f.rd._compute_list_end_recorded();
		CHECK(original_close.is_valid());
		CHECK(reader->recording_closed);
		CHECK_FALSE(reader->uncaptured_prefix);
		CHECK(reader->graph_count == 2);
		CHECK(reader->append_error == OK);
		Ref<NativeHeightRange> foreign_reader;
		REQUIRE(f.rd._native_height_range_prepare(foreign_reader) == OK);
		foreign_reader->reader_scope = true;
		foreign_reader->capture_frame = 0;
		foreign_reader->capture_epoch = 1;
		CHECK(f.rd._native_height_reader_enroll(foreign_reader, dispatch) == ERR_INVALID_DATA);
		CHECK(foreign_reader->uncaptured_prefix);
		CHECK(foreign_reader->graph_count == 0);
		// Actual End cleanup must quarantine a mismatched member without
		// overwriting the earlier original business failure.
		Ref<NativeHeightRange> failed;
		REQUIRE(f.rd._native_height_range_prepare(failed) == OK);
		failed->reader_scope = true;
		failed->capture_frame = 0;
		failed->capture_epoch = 1;
		failed->reader_owns_list = true;
		failed->reader_dispatches = 1;
		failed->append_error = ERR_CANT_CREATE;
		failed->graphs[0].handle = dispatch; // Actual foreign-graph instruction.
		failed->graph_count = 1;
		f.rd.draw_graph.add_compute_list_begin();
		f.rd.native_height_active_checked_range = failed;
		f.rd.compute_list.active = true;
		CHECK(f.rd._compute_list_end_recorded().is_valid());
		CHECK(failed->recording_closed);
		CHECK(failed->uncaptured_prefix);
		CHECK(failed->append_error == ERR_CANT_CREATE);
		CHECK(failed->graph_count == 2);
		f.rd.draw_graph.finalize();
	}
	static void reader_first_error_is_sticky() {
		Fixture f;
		Ref<NativeHeightRange> reader;
		REQUIRE(f.rd._native_height_range_prepare(reader) == OK);
		reader->reader_scope = true;
		reader->append_error = ERR_CANT_CREATE;
		f.rd.native_height_active_checked_range = reader;
		f.rd.compute_list_dispatch_indirect(RenderingDevice::INVALID_ID, RID(), 0);
		CHECK(reader->append_error == ERR_CANT_CREATE);
		f.rd.compute_list_add_barrier(RenderingDevice::INVALID_ID);
		CHECK(reader->append_error == ERR_CANT_CREATE);
		CHECK(f.rd._native_height_reader_enroll(reader, {}) == ERR_INVALID_DATA);
		CHECK(reader->append_error == ERR_CANT_CREATE);
		CHECK(reader->uncaptured_prefix);
		reader->reader_owns_list = true;
		bool closed = false;
		CHECK(f.rd._native_height_reader_finish(reader, closed) == ERR_CANT_CREATE);
		CHECK_FALSE(closed);
		CHECK(reader->uncaptured_prefix);
		CHECK(reader->append_error == ERR_CANT_CREATE);
		f.rd.native_height_active_checked_range.unref();
	}

	static void one_pass_reader_return_resumes_paid_entry() {
		Fixture f;
		auto reader = sealed(f);
		reader->reader_scope = true;
		CHECK(issue(f, 2, 45, OK, report()) == OK);
		REQUIRE(f.rd._native_height_retain_frame_sources(0) == OK);
		REQUIRE(f.rd._native_height_reader_request_return(reader) == OK);
		f.driver.completion.completed = true;
		f.driver.completion.completed_serial = 901;
		f.driver.completion.resolved_issue = RDD::IssueFact::KnownAccepted;
		const RenderingDevice::NativeDisposalBudget one{ 1, 1, UINT64_MAX };
		uint32_t total_passes = 0, total_receipts = 0;
		for (uint32_t turn = 0; turn < 12 && !reader->observers_returned; ++turn) {
			RenderingDevice::NativeDisposalUsage used;
			const uint32_t polls = f.driver.polls, releases = f.driver.releases;
			const Error result = f.rd._native_height_reader_returns_step(one, used);
			CHECK((result == OK || result == ERR_BUSY));
			const uint32_t receipts = used.polls + used.frees + used.issues + used.retains + used.releases + used.recycles;
			CHECK(used.passes <= 1);
			CHECK(receipts <= 1);
			CHECK((f.driver.polls - polls) + (f.driver.releases - releases) == receipts);
			total_passes += used.passes;
			total_receipts += receipts;
			if (turn == 0) {
				CHECK(used.passes == 1); // Original entry WORK was actually paid.
				CHECK(receipts == 0);
				CHECK(f.driver.polls == 0);
				RenderingDevice::NativeDisposalUsage zero_usage;
				const auto cursor = f.rd.native_height_reader_return_cursor;
				CHECK(f.rd._native_height_reader_returns_step({}, zero_usage) == ERR_BUSY);
				CHECK(zero_usage.passes == 0);
				CHECK(f.rd.native_height_reader_return_cursor == cursor);
			}
		}
		CHECK(reader->observers_returned);
		CHECK(f.driver.polls == 1);
		CHECK(f.driver.releases == 1);
		CHECK(total_passes == 3); // WORK, actual poll, actual observer release.
		CHECK(total_receipts == 2);
	}
	static void one_pass_current_producer_observation() {
		Fixture f;
		f.driver.depth_resources = true;
		RendererRD::HeightCaptureCheckedSink sink(f.rd, 2, 0);
		using Resources = std::remove_pointer_t<decltype(sink.quoted_resources.ptr())>;
		Ref<Resources> resources;
		REQUIRE(Resources::allocate(f.rd, 8, 4, resources) == OK);
		auto producer = sealed(f); // Existing reducer membership; issue/retain/poll are real callbacks.
		// Preserve the exact original allocated resource Ref as the checked Begin
		// enrollment does. This is a reducer test, not renderer admission evidence.
		producer->producer_resources = resources;
		Ref<NativeHeightRange> idle;
		REQUIRE(f.rd._native_height_range_prepare(idle) == OK);
		idle->recording_closed = true; // Closed no-effect producer still pays directory WORK per inspection.
		REQUIRE(issue(f, 2, 45, OK, report()) == OK);
		REQUIRE(f.rd._native_height_retain_frame_sources(0) == OK);
		f.driver.completion.completed_serial = 901;
		f.driver.completion.resolved_issue = RDD::IssueFact::KnownAccepted;
		f.driver.completion.completed = false;
		const RenderingDevice::NativeDisposalBudget one{ 1, 1, UINT64_MAX };
		uint32_t total_passes = 0, total_receipts = 0;
		for (uint32_t turn = 0; turn < 12 && !producer->native_complete; ++turn) {
			RenderingDevice::NativeDisposalUsage used;
			const uint32_t polls = f.driver.polls, releases = f.driver.releases;
			CHECK(f.rd._native_height_reader_returns_step(one, used) == ERR_BUSY);
			const uint32_t receipts = used.polls + used.frees + used.issues + used.retains + used.releases + used.recycles;
			CHECK(used.passes <= 1);
			CHECK(receipts <= 1);
			CHECK(f.driver.polls - polls == receipts);
			CHECK(f.driver.releases == releases);
			total_passes += used.passes;
			total_receipts += receipts;
			CHECK_FALSE(producer->owner_requested_return);
			CHECK_FALSE(producer->observers_returned);
			CHECK(producer->sources[0].retained);
			CHECK_FALSE(producer->sources[0].observer_returned);
			CHECK(producer->producer_resources.ptr() == resources.ptr());
			CHECK(f.driver.texture_frees == 0);
			if (f.driver.polls == 1) f.driver.completion.completed = true;
		}
		CHECK(producer->native_complete);
		CHECK(f.driver.polls == 2); // Incomplete observation resumes without repaying row WORK.
		CHECK(total_receipts == 2);
		CHECK(total_passes == 5); // Producer WORK + two polls + two idle-row inspections.
		// Completion closes the paid episode. Every subsequent idle directory
		// inspection pays WORK again, including this same completed producer.
		for (uint32_t turn = 0; turn < 4; ++turn) {
			RenderingDevice::NativeDisposalUsage used;
			CHECK(f.rd._native_height_reader_returns_step(one, used) == ERR_BUSY);
			CHECK(used.passes == 1);
			CHECK(used.polls == 0);
			CHECK(used.releases == 0);
		}
		CHECK(f.driver.releases == 0);
		// Settle through the existing explicit observe for safe old-source RED
		// teardown, after all bounded-service progress assertions above.
		NativeHeightRange::Snapshot cleanup_snapshot;
		f.driver.completion.completed = true;
		REQUIRE(observe(f, producer, cleanup_snapshot) == OK);
		REQUIRE(give_back(f, producer) == OK); // Explicit owner return only for teardown.
		CHECK(producer->producer_resources.is_null());
		CHECK(f.driver.releases == 1);
		CHECK(resources->retire_step() == ERR_BUSY);
		f.rd._free_pending_resources(0);
		for (uint32_t i = 0; i < 4; ++i) {
			resources->retire_step();
			f.rd._free_pending_resources(0);
		}
		CHECK(resources->state == Resources::State::RELEASED);
		CHECK(f.driver.texture_frees == 1);
		resources.unref();
		for (const auto &entry : f.rd.framebuffer_formats) f.driver.render_pass_free(entry.value.render_pass);
		f.rd.framebuffer_formats.clear();
		f.rd.framebuffer_format_cache.clear();
	}

	static void checked_sink_failure_terminal_controls() {
		using Sink = RendererRD::HeightCaptureCheckedSink;
		using Effect = RenderingDevice::NativeAppendEffect;
		{
			Fixture f;
			Sink sink(f.rd, 2, 0);
			Ref<NativeHeightRange> range;
			REQUIRE(sink.prepare_native_range(range) == OK);
			CHECK(f.rd._native_height_range_close_checked_capture(range, sink) == ERR_INVALID_PARAMETER);
			CHECK_FALSE(range->recording_closed);
			// The original checked Begin refused before effect. Its error survives
			// explicit unwind, but an empty physical prefix has nothing to poll.
			sink.business_count = 1;
			sink.begin_attempted = true;
			sink.business[0].operation = Sink::Operation::Begin;
			sink.business[0].begin.error = ERR_CANT_CREATE;
			CHECK(f.rd._native_height_range_enroll_original_record(range, sink, 0, false) == ERR_CANT_CREATE);
			sink.reject(ERR_CANT_CREATE);
			CHECK(sink.unwind() == ERR_CANT_CREATE);
			CHECK(range->recording_closed);
			CHECK_FALSE(range->has_recorded_effect);
			CHECK_FALSE(range->uncaptured_prefix);
			CHECK(give_back(f, range) == OK);
			CHECK_FALSE(range->native_complete);
			CHECK(f.driver.polls == 0);
		}
		{
			Fixture f;
			Sink sink(f.rd, 2, 0);
			Ref<NativeHeightRange> range;
			REQUIRE(sink.prepare_native_range(range) == OK);
			sink.business_count = 1;
			sink.business[0].operation = Sink::Operation::Begin;
			sink.business[0].begin.error = ERR_CANT_CREATE;
			REQUIRE(f.rd._native_height_range_enroll_original_record(range, sink, 0, false) == ERR_CANT_CREATE);
			sink.reject(ERR_CANT_CREATE);
			sink.cleanup_attempted = true;
			// Sticky business error does not refuse the independent cleanup call.
			CHECK(sink.enter_checked_call() == OK);
			sink.cleanup.draw.effect = Effect::Unknown;
			sink.cleanup.draw.error = FAILED;
			CHECK(f.rd._native_height_range_enroll_original_record(range, sink, 0, true) == ERR_CANT_CREATE);
			sink.restoration_unknown = true;
			CHECK(sink.unwind() == ERR_CANT_CREATE);
			CHECK(range->recording_closed);
			CHECK(range->has_recorded_effect);
			CHECK(range->uncaptured_prefix);
			CHECK(range->append_error == ERR_CANT_CREATE);
			CHECK(give_back(f, range) == ERR_BUSY);
			CHECK_FALSE(range->prefix_settled);
			CHECK(f.driver.releases == 0);
		}
		{
			// A physically settled original source does not make an unknown
			// cleanup prefix returnable. Keep the source positive so this tests
			// the terminal distinction rather than the empty-range fast path.
			Fixture f;
			auto range = sealed(f);
			range->recording_closed = false;
			CHECK(issue(f, 2, 45, OK, report()) == OK);
			CHECK(f.rd._native_height_retain_frame_sources(0) == OK);
			f.driver.completion.completed = true;
			f.driver.completion.completed_serial = 901;
			NativeHeightRange::Snapshot settled;
			CHECK(observe(f, range, settled) == ERR_BUSY);
			CHECK(settled.completed_sources == 1);
			CHECK_FALSE(settled.native_complete); // The checked recording is still open.
			Sink sink(f.rd, 2, 0);
			sink.native_range = range;
			range->capture_sink = &sink;
			range->capture_frame = 0;
			range->capture_epoch = f.rd.frames[0].qualification_epoch;
			sink.terminal_requested = true;
			sink.cleanup_attempted = true;
			sink.restoration_unknown = true;
			sink.cleanup.draw.effect = Effect::Unknown;
			sink.cleanup.draw.error = FAILED;
			CHECK(f.rd._native_height_range_enroll_original_record(range, sink, 0, true) == FAILED);
			CHECK(f.rd._native_height_range_close_checked_capture(range, sink) == FAILED);
			CHECK(range->uncaptured_prefix);
			range->native_complete = false;
			range->prefix_settled = false;
			NativeHeightRange::Snapshot out;
			CHECK(observe(f, range, out) == FAILED); // Preserve the first cleanup error.
			CHECK_FALSE(out.native_complete);
			CHECK(give_back(f, range) == ERR_BUSY);
			CHECK_FALSE(range->observers_returned);
			CHECK(f.driver.releases == 0);
		}
	}

	static void checked_sink_provenance_and_missing_members() {
		using Sink = RendererRD::HeightCaptureCheckedSink;
		using Effect = RenderingDevice::NativeAppendEffect;
		for (int fault = 0; fault < 4; ++fault) {
			CAPTURE(fault);
			Fixture f;
			Sink sink(f.rd, 2, 1);
			Ref<NativeHeightRange> range;
			REQUIRE(sink.prepare_native_range(range) == OK);
			sink.business_count = 1;
			sink.business[0].operation = Sink::Operation::UniformSet;
			sink.business[0].bind.capture_complete = true;
			sink.business[0].bind.state_accepted = true;
			if (fault == 0) {
				// A copied valid-looking handle from a different graph is not a
				// member. contains_record must reject it before source mapping.
				sink.business[0].bind.effect = Effect::Recorded;
				sink.business[0].bind.bind_handle = { 99, 1, 3, 0, 0 };
			} else if (fault == 1) {
				++f.rd.frames[0].qualification_epoch;
			} else if (fault == 2) {
				REQUIRE(f.rd._native_height_range_enroll_original_record(range, sink, 0, false) == OK);
				CHECK(range->graph_count == 0); // Deferred bind adds no graph row.
			} else {
				sink.business[0].bind.transfer_count = 1;
				sink.business[0].transfer_count = 1;
				sink.transfer_count = 1;
			}
			CHECK(f.rd._native_height_range_enroll_original_record(range, sink, 0, false) != OK);
			sink.reject(range->append_error);
			sink.unwind();
			CHECK(range->uncaptured_prefix);
			CHECK(give_back(f, range) == ERR_BUSY);
			CHECK_FALSE(range->native_complete);
			CHECK(f.driver.polls == 0);
		}
	}
	[[noreturn]] static void actual_finalize_child(const String &mode) {
		Driver driver;
		driver.finalize_sentinel = true;
		RenderingDevice *const previous_singleton = RenderingDevice::get_singleton();
		auto *rd = memnew(RenderingDevice);
		rd->driver = &driver;
		rd->is_main_instance = true;
		rd->render_thread_id = Thread::get_caller_id();
		rd->frames.resize(1);
		auto &frame = rd->frames[0];
		frame.qualification_epoch = 1;
		frame.primary_command_buffer_begun = true;
		Ref<NativeHeightRange> range;
		const auto require = child_require;
		require(RenderingDevice::get_singleton() == (previous_singleton ? previous_singleton : rd));
		require(rd->_native_height_range_prepare(range) == OK);
		// Exact synthetic sealed row for CPU lifecycle control only. The independent
		// Metal fixture already proves real graph/issue/callback membership.
		range->graph_count = 1;
		auto &member = range->graphs[0];
		member.frame = 0;
		member.frame_epoch = frame.qualification_epoch;
		member.state = NativeHeightRange::MemberState::Sealed;
		member.slot = 0;
		member.command_buffer = RDD::CommandBufferID(45);
		range->recording_closed = true;
		range->has_recorded_effect = true;
		frame.native_height_ranges.push_back(range);
		const auto original = report();
		require(rd->_native_height_issue_slot(0, 1, 0, member.command_buffer, OK, original) == OK);
		frame.chain_reports.push_back(original);
		// This frame owns an issued no-fence call, no open/unsent command buffer.
		// Do not set finalization_started/drained or cached native_complete here.
		frame.primary_command_buffer_begun = false;
		driver.completion.completed = true;
		driver.completion.completed_serial = 901;
		driver.release_busy = true;
		std::fprintf(stderr, "NHR_FINALIZE_BEFORE\n");
		std::fflush(stderr);
		rd->finalize(); // ACTUAL public entry; reaches original frame/worker drain.
		require(rd->native_height_finalization_started);
		require(rd->native_height_finalization_drained);
		require(!rd->native_finalization_returned && rd->driver == &driver);
		require(rd->native_height_ranges.size() == 1 && range->native_complete);
		require(!range->observers_returned && range->return_cursor == 0);
		require(driver.releases == 1 && driver.recycles == 1);
		const auto first_polls = driver.polls;
		const auto first_issues = driver.issues;
		const auto first_command_ends = driver.command_ends;
		const auto first_segment_ends = driver.segment_ends;
		rd->finalize(); // ACTUAL repeat; still blocked before graph/resource teardown.
		require(!rd->native_finalization_returned && rd->driver == &driver);
		require(rd->native_height_ranges.size() == 1 && !range->observers_returned);
		require(range->return_cursor == 0 && driver.releases == 2);
		require(driver.polls == first_polls && driver.issues == first_issues);
		require(driver.command_ends == first_command_ends && driver.segment_ends == first_segment_ends);
		require(driver.recycles == 1);
		std::fprintf(stderr, "NHR_FINALIZE_BUSY_RETRY releases=2 recycle=1 owner_live=1 returned=0\n");
		std::fflush(stderr);
		if (mode == "last_ref") {
			std::fprintf(stderr, "NHR_BEFORE_LAST_REF\n");
			std::fflush(stderr);
			memdelete(rd); // ACTUAL destructor: finalize BUSY, then LAST_REF _Exit78.
			std::_Exit(88); // A returned destructor is an invalid positive result.
		}
		std::_Exit(0); // Isolated blocked owner: deliberately no unsafe detach/free.
	}

	static void wrong_slot_buffer_and_empty_busy() {
		Fixture f;
		auto range = sealed(f);
		// Deleting exact slot/buffer matching makes these controls fail.
		CHECK(issue(f, 1, 45, OK, report()) == OK);
		CHECK(issue(f, 2, 46, OK, report()) == OK);
		CHECK(range->source_count == 0);
		CHECK_FALSE(range->graphs[0].issue_attempted);
		CHECK(issue(f, 2, 45, ERR_BUSY, {}) == ERR_BUSY);
		CHECK(range->source_count == 0);
		CHECK_FALSE(range->graphs[0].issue_attempted);
		CHECK(f.driver.retains == 0);
		CHECK(issue(f, 2, 45, OK, report()) == OK);
		CHECK(range->source_count == 1);
		CHECK(range->graphs[0].issue_attempted);
		CHECK(f.rd._native_height_retain_frame_sources(0) == OK);
		CHECK(f.driver.retains == 1);
	}
	static void callback_identity_and_unknown_refinement() {
		Fixture f;
		auto range = sealed(f);
		auto actual = report();
		actual.calls.write[0].issue = RDD::IssueFact::IssuedUnknown;
		CHECK(issue(f, 2, 45, OK, actual) == OK);
		CHECK(f.rd._native_height_retain_frame_sources(0) == OK);
		NativeHeightRange::Snapshot out;
		f.driver.completion.completed = true;
		f.driver.completion.completed_serial = 902;
		f.driver.completion.resolved_issue = RDD::IssueFact::KnownAccepted;
		CHECK(observe(f, range, out) == ERR_BUSY);
		CHECK_FALSE(out.native_complete);
		CHECK(give_back(f, range) == ERR_BUSY);
		CHECK(f.driver.releases == 0);
		f.driver.completion.completed_serial = 901;
		f.driver.completion.resolved_issue = RDD::IssueFact::NotIssued;
		CHECK(observe(f, range, out) == ERR_BUSY);
		CHECK_FALSE(out.native_complete);
		f.driver.completion.resolved_issue = RDD::IssueFact::KnownAccepted;
		CHECK(observe(f, range, out) == OK);
		CHECK(out.native_complete);
		f.driver.release_busy = true;
		CHECK(give_back(f, range) == ERR_BUSY);
		CHECK(range->return_cursor == 0);
		f.driver.release_busy = false;
		CHECK(give_back(f, range) == OK);
		const uint32_t polls = f.driver.polls;
		const uint32_t releases = f.driver.releases;
		CHECK(give_back(f, range) == OK);
		CHECK(observe(f, range, out) == OK);
		CHECK(out.native_complete);
		CHECK(out.observers_returned);
		CHECK(f.driver.polls == polls);
		CHECK(f.driver.releases == releases);
	}
	static void retain_busy_and_two_independent_cells() {
		Fixture f;
		auto first = sealed(f);
		auto second = sealed(f);
		f.driver.retain_busy = true;
		CHECK(issue(f, 2, 45, OK, report()) == ERR_BUSY);
		CHECK(first->source_count == 1);
		CHECK(second->source_count == 1);
		CHECK(f.rd._native_height_retain_frame_sources(0) == ERR_BUSY);
		CHECK_FALSE(first->sources[0].retained);
		CHECK_FALSE(second->sources[0].retained);
		f.driver.retain_busy = false;
		CHECK(f.rd._native_height_retain_frame_sources(0) == OK);
		CHECK(first->sources[0].retained);
		CHECK(second->sources[0].retained);
		const uint32_t retains = f.driver.retains;
		CHECK(f.rd._native_height_retain_frame_sources(0) == OK);
		CHECK(f.driver.retains == retains);
		f.driver.completion.completed = true;
		f.driver.completion.completed_serial = 901;
		NativeHeightRange::Snapshot out;
		CHECK(observe(f, first, out) == OK);
		CHECK(give_back(f, first) == OK);
		CHECK(first->sources[0].observer_returned);
		CHECK_FALSE(second->sources[0].observer_returned);
		CHECK(observe(f, second, out) == OK);
		CHECK(out.native_complete);
		CHECK(give_back(f, second) == OK);
		CHECK(f.driver.releases == 2);
	}
	static void old_epoch_and_empty_close() {
		Fixture f;
		auto range = sealed(f);
		++f.rd.frames[0].qualification_epoch;
		CHECK(issue(f, 2, 45, OK, report()) == OK);
		CHECK(range->source_count == 0);
		CHECK_FALSE(range->graphs[0].issue_attempted);
		Ref<NativeHeightRange> empty;
		CHECK(f.rd._native_height_range_prepare(empty) == OK);
		CHECK(f.rd._native_height_range_close(empty) == OK);
		NativeHeightRange::Snapshot out;
		CHECK(observe(f, empty, out) == OK);
		CHECK(out.recording_closed);
		CHECK_FALSE(out.has_recorded_effect);
		CHECK_FALSE(out.native_complete);
		CHECK(give_back(f, empty) == OK);
		CHECK(f.driver.retains == 0);
		CHECK(f.driver.releases == 0);
	}
	static void actual_issue_hooks_and_retain_barriers() {
		for (bool bounded : { false, true }) {
			Fixture f;
			auto range = sealed(f);
			range->graphs[0].slot = 0;
			f.rd.frames[0].command_buffer = RDD::CommandBufferID(45);
			f.driver.retain_busy = true;
			if (bounded) {
				// Start at the original already-sealed continuation's actual issue
				// phase; no test enrollment hook is installed in production code.
				f.rd.native_disposal_active = true;
				auto &cursor = f.rd.native_disposal_continuation;
				cursor.serial = 1;
				cursor.phase = 2;
				cursor.slot_count = 1;
				RenderingDevice::NativeDisposalBudget budget{ 64, 64, UINT64_MAX };
				RenderingDevice::NativeDisposalUsage usage;
				CHECK(f.rd._native_disposal_maintenance_step(budget, usage) == ERR_BUSY);
				CHECK(cursor.phase == 2);
				CHECK(cursor.slot == 1);
			} else {
				CHECK(f.rd.execute_chained_cmds(false, {}, {}));
			}
			CHECK(f.driver.issues == 1);
			CHECK(range->source_count == 1);
			CHECK(range->graphs[0].issue_attempted);
			CHECK_FALSE(range->sources[0].retained);
			CHECK((bounded ? f.rd._native_disposal_stall_step(0) : f.rd._stall_for_frame(0, true)) == ERR_BUSY);
			CHECK(f.driver.recycles == 0);
			CHECK(f.driver.polls == 0);
			f.driver.retain_busy = false;
			CHECK(f.rd._native_height_retain_frame_sources(0) == OK);
			CHECK(range->sources[0].retained);
			CHECK(f.driver.issues == 1);
			f.driver.completion.completed = true;
			f.driver.completion.completed_serial = 901;
			NativeHeightRange::Snapshot out;
			CHECK(observe(f, range, out) == OK);
			CHECK(out.native_complete);
			CHECK(give_back(f, range) == OK);
		}
	}
	static void sealed_flush_retry_blocks_getter_append() {
		Fixture f;
		auto range = sealed(f);
		range->graphs[0].slot = 0;
		auto &frame = f.rd.frames[0];
		frame.command_buffer = RDD::CommandBufferID(45);
		frame.qualification_graph_closed = frame.qualification_segment_closed = true;
		f.driver.retain_busy = true;
		CHECK(f.rd._execute_frame(false) == OK);
		CHECK(f.driver.issues == 1);
		CHECK(frame.fence_signaled);
		CHECK(f.rd._flush_and_stall_for_all_frames(false) == ERR_BUSY);
		CHECK(f.rd._recording_error() == ERR_BUSY);
		// This public synchronous getter refuses at its original recording guard
		// before creating a temporary buffer or appending another readback.
		ERR_PRINT_OFF;
		CHECK(f.rd.buffer_get_data(RID()).is_empty());
		ERR_PRINT_ON;
		CHECK(f.rd._end_frame() == OK);
		CHECK(f.rd._execute_frame(false) == OK);
		CHECK(f.driver.issues == 1);
		CHECK(f.driver.command_ends == 0);
		CHECK(f.driver.segment_ends == 0);
		CHECK(f.driver.recycles == 0);
	}
	static void finite_capacity_and_foreign_range_refusal() {
		Fixture f;
		LocalVector<Ref<NativeHeightRange>> prepared;
		prepared.resize(RenderingDevice::NATIVE_HEIGHT_RANGE_CAPACITY);
		for (uint32_t i = 0; i < RenderingDevice::NATIVE_HEIGHT_RANGE_CAPACITY; ++i) {
			CHECK(f.rd._native_height_range_prepare(prepared[i]) == OK);
			CHECK(prepared[i].is_valid());
		}
		Ref<NativeHeightRange> exhausted;
		CHECK(f.rd._native_height_range_prepare(exhausted) == ERR_BUSY);
		CHECK(exhausted.is_null());
		Fixture foreign_fixture;
		Ref<NativeHeightRange> foreign;
		REQUIRE(foreign_fixture.rd._native_height_range_prepare(foreign) == OK);
		NativeHeightRange::Snapshot out;
		RenderingDevice::NativeDisposalUsage foreign_usage;
		CHECK(RendererRD::HeightCaptureCheckedSink::observe_native_range(f.rd, foreign, RenderingDevice::NativeDisposalBudget{ 64, 64, UINT64_MAX }, foreign_usage, out) == ERR_INVALID_PARAMETER);
		CHECK_FALSE(out.native_complete);
	}
	static void post_drain_finalization_busy_cursor() {
		Fixture f;
		auto range = sealed(f);
		CHECK(issue(f, 2, 45, OK, report()) == OK);
		CHECK(f.rd._native_height_retain_frame_sources(0) == OK);
		f.driver.completion.completed = true;
		f.driver.completion.completed_serial = 901;
		NativeHeightRange::Snapshot out;
		CHECK(observe(f, range, out) == OK);
		CHECK(out.native_complete);
		f.rd.native_height_finalization_started = true;
		f.rd.native_height_finalization_drained = true;
		f.driver.release_busy = true;
		CHECK(f.rd._native_height_ranges_prepare_finalization() == ERR_BUSY);
		CHECK(f.rd.native_height_ranges.size() == 1);
		CHECK_FALSE(range->observers_returned);
		const uint32_t issues = f.driver.issues;
		const uint32_t polls = f.driver.polls;
		f.driver.release_busy = false;
		CHECK(f.rd._native_height_ranges_prepare_finalization() == OK);
		CHECK(f.rd.native_height_ranges.is_empty());
		CHECK(range->observers_returned);
		CHECK(f.driver.issues == issues);
		CHECK(f.driver.polls == polls);
		CHECK(f.driver.releases == 2);
		// A retry of this post-drain phase never re-enters graph end or issue.
		CHECK(f.rd._native_height_ranges_prepare_finalization() == OK);
		CHECK(f.driver.issues == issues);
		CHECK(f.driver.polls == polls);
	}
	static void finalization_does_not_close_live_checked_sink() {
		using Sink = RendererRD::HeightCaptureCheckedSink;
		for (bool pending_result : { false, true }) {
			CAPTURE(pending_result);
			Fixture f;
			Sink sink(f.rd, 2, 0);
			Ref<NativeHeightRange> range;
			REQUIRE(sink.prepare_native_range(range) == OK);
			if (pending_result) {
				// Model a checked call which has returned an unknown prefix but
				// has not enrolled yet. Its empty range arrays are not no-effect.
				sink.business_count = 1;
				sink.business[0].begin.effect = RenderingDevice::NativeAppendEffect::Unknown;
				sink.restoration_unknown = true;
				sink.reject(ERR_CANT_CREATE);
			}
			for (int retry = 0; retry < 2; ++retry) {
				CHECK(f.rd._native_height_ranges_prepare_finalization() == ERR_BUSY);
				CHECK_FALSE(range->recording_closed);
				CHECK_FALSE(range->observers_returned);
				CHECK(f.rd.native_height_ranges.size() == 1);
				CHECK(f.rd.frames[0].native_height_ranges.size() == 1);
				CHECK(f.driver.polls == 0);
				CHECK(f.driver.releases == 0);
			}
			sink.unwind(); // Only the original terminal path may freeze custody.
			CHECK(range->recording_closed);
			if (pending_result) {
				CHECK(range->uncaptured_prefix);
				CHECK(f.rd._native_height_ranges_prepare_finalization() != OK);
				CHECK(f.rd.native_height_ranges.size() == 1);
				CHECK_FALSE(range->observers_returned);
			} else {
				CHECK(f.rd._native_height_ranges_prepare_finalization() == OK);
				CHECK(f.rd.native_height_ranges.is_empty());
				CHECK(f.rd.frames[0].native_height_ranges.is_empty());
				CHECK(range->observers_returned);
				CHECK_FALSE(range->native_complete);
			}
		}
	}
	static void preflight_zero_effect_restrictions() {
		Fixture f;
		Ref<NativeHeightRange> range;
		CHECK(f.rd._native_height_range_prepare(range) == OK);
		NativeDisposalWitness depth, framebuffer;
		CHECK(f.rd._native_height_range_record_depth_clear(range, depth, framebuffer, 1.0f) == ERR_INVALID_PARAMETER);
		CHECK_FALSE(range->has_recorded_effect);
		f.rd.draw_list.active = true;
		Ref<NativeHeightRange> blocked;
		CHECK(f.rd._native_height_range_prepare(blocked) == ERR_BUSY);
		f.rd.draw_list.active = false;
		f.rd.frames[0].qualification_graph_closed = true;
		CHECK(f.rd._native_height_range_prepare(blocked) == ERR_BUSY);
		f.rd.frames[0].qualification_graph_closed = false;
		CHECK(f.rd._native_height_range_close(range) == OK);
		CHECK(f.rd._native_height_ranges_prepare_finalization() == OK);
	}

	static void known_no_effect_failure_settles_but_keeps_error() {
		Fixture f;
		Ref<NativeHeightRange> range;
		REQUIRE(f.rd._native_height_range_prepare(range) == OK);
		// Model the original checked begin's known pre-effect refusal. No graph
		// handle/source is fabricated and the cell's sticky failure survives.
		range->recording_started = true;
		range->append_error = ERR_CANT_CREATE;
		f.rd.frames[0].native_height_ranges.push_back(range);
		CHECK(f.rd._native_height_range_close(range) == ERR_CANT_CREATE);
		NativeHeightRange::Snapshot out;
		CHECK(observe(f, range, out) == ERR_CANT_CREATE);
		CHECK(out.append_error == ERR_CANT_CREATE);
		CHECK_FALSE(out.has_recorded_effect);
		CHECK_FALSE(out.native_complete);
		CHECK(f.rd._native_height_ranges_prepare_finalization() == OK);
		CHECK(f.rd.native_height_ranges.is_empty());
		CHECK(f.rd.frames[0].native_height_ranges.is_empty());
		CHECK(observe(f, range, out) == ERR_CANT_CREATE);
		CHECK(out.append_error == ERR_CANT_CREATE);
		CHECK_FALSE(out.native_complete);
		CHECK(f.driver.polls == 0);
		CHECK(f.driver.retains == 0);
		CHECK(f.driver.releases == 0);
		Ref<NativeHeightRange> unknown;
		REQUIRE(f.rd._native_height_range_prepare(unknown) == OK);
		unknown->uncaptured_prefix = true;
		unknown->append_error = ERR_INVALID_DATA;
		CHECK(f.rd._native_height_ranges_prepare_finalization() != OK);
		CHECK(f.rd.native_height_ranges.size() == 1);
		CHECK_FALSE(unknown->observers_returned);
	}
	static void failed_source_blocks_finalization() {
		Fixture f;
		auto range = sealed(f);
		CHECK(issue(f, 2, 45, OK, report()) == OK);
		CHECK(f.rd._native_height_retain_frame_sources(0) == OK);
		f.driver.completion.completed = true;
		f.driver.completion.completed_serial = 901;
		f.driver.completion.native_error = true;
		f.driver.completion.native_code = -8;
		NativeHeightRange::Snapshot out;
		CHECK(observe(f, range, out) == FAILED);
		CHECK_FALSE(out.native_complete);
		CHECK(out.completion_error == FAILED);
		CHECK(f.rd._native_height_ranges_prepare_finalization() != OK);
		CHECK(f.rd.driver == &f.driver);
		CHECK_FALSE(f.rd.native_finalization_returned);
		CHECK(f.rd.native_height_ranges.size() == 1);
		CHECK(f.driver.releases == 0);
		// There is no manufactured "device lost => complete" escape hatch.
		// Destructive-finalize failure is tested in a subprocess, never in this
		// doctest process: original frame wait CRASH_NOW / LAST_REF fail-stop.
	}
	static void settled_prefix_keeps_business_error_but_returns_sources() {
		Fixture f;
		auto range = sealed(f);
		CHECK(issue(f, 2, 45, OK, report()) == OK);
		CHECK(f.rd._native_height_retain_frame_sources(0) == OK);
		f.driver.completion.completed = true;
		f.driver.completion.completed_serial = 901;
		NativeHeightRange::Snapshot out;
		// The checked sink may have captured a real native prefix before a
		// later business operation failed. Keep that sticky error, but allow
		// mechanical observer retirement once every source is settled.
		range->append_error = ERR_CANT_CREATE;
		CHECK(f.rd._native_height_range_close(range) == ERR_CANT_CREATE);
		CHECK(observe(f, range, out) == ERR_CANT_CREATE);
		CHECK(out.prefix_settled);
		CHECK_FALSE(out.native_complete);
		CHECK(out.append_error == ERR_CANT_CREATE);
		CHECK(f.rd._native_height_ranges_prepare_finalization() == OK);
		CHECK(f.rd.native_height_ranges.is_empty());
		CHECK(range->observers_returned);
		CHECK(f.driver.releases == 1);
	}
	static void failed_prefix_release_is_sticky_and_not_replayed() {
		Fixture f;
		auto range = sealed(f);
		CHECK(issue(f, 2, 45, OK, report()) == OK);
		CHECK(f.rd._native_height_retain_frame_sources(0) == OK);
		f.driver.completion.completed = true;
		f.driver.completion.completed_serial = 901;
		NativeHeightRange::Snapshot out;
		range->append_error = ERR_CANT_CREATE;
		CHECK(f.rd._native_height_range_close(range) == ERR_CANT_CREATE);
		CHECK(observe(f, range, out) == ERR_CANT_CREATE);
		f.driver.release_fail = true;
		CHECK(give_back(f, range) == FAILED);
		CHECK(f.driver.releases == 1);
		f.driver.release_fail = false;
		CHECK(give_back(f, range) == FAILED);
		CHECK(f.driver.releases == 1);
		CHECK(f.rd._native_height_ranges_prepare_finalization() == ERR_CANT_CREATE);
		CHECK(observe(f, range, out) == ERR_CANT_CREATE);
		CHECK(out.return_error == FAILED);
		CHECK(f.driver.releases == 1);
		CHECK_FALSE(range->observers_returned);
		CHECK(f.rd.native_height_ranges.size() == 1);
	}
	static void business_failure_does_not_settle_unsafe_prefix() {
		enum class Fault { Mapping, Issue, Retention, Completion, Uncaptured, Unresolved, WrongSerial };
		for (Fault fault : { Fault::Mapping, Fault::Issue, Fault::Retention, Fault::Completion, Fault::Uncaptured, Fault::Unresolved, Fault::WrongSerial }) {
			CAPTURE(int(fault));
			Fixture f;
			auto range = sealed(f); // Synthetic reducer member; no new issuer.
			auto original = report();
			if (fault == Fault::Unresolved) {
				original.calls.write[0].issue = RDD::IssueFact::IssuedUnknown;
			}
			REQUIRE(issue(f, 2, 45, OK, original) == OK);
			f.driver.completion.completed = true;
			f.driver.completion.completed_serial = fault == Fault::WrongSerial ? 902 : 901;
			f.driver.completion.resolved_issue = fault == Fault::Unresolved ? RDD::IssueFact::IssuedUnknown : RDD::IssueFact::KnownAccepted;
			if (fault == Fault::Mapping) {
				range->mapping_error = ERR_INVALID_DATA;
			} else if (fault == Fault::Issue) {
				range->issue_error = FAILED;
			} else if (fault == Fault::Retention) {
				range->retention_error = FAILED;
			} else if (fault == Fault::Completion) {
				f.driver.completion.native_error = true;
			} else if (fault == Fault::Uncaptured) {
				range->uncaptured_prefix = true;
			}
			range->append_error = ERR_CANT_CREATE;
			NativeHeightRange::Snapshot out;
			CHECK(observe(f, range, out) == ERR_CANT_CREATE);
			CHECK(out.append_error == ERR_CANT_CREATE);
			CHECK_FALSE(out.prefix_settled);
			CHECK_FALSE(out.native_complete);
			CHECK(give_back(f, range) == ERR_BUSY);
			CHECK(f.rd._native_height_ranges_prepare_finalization() != OK);
			CHECK_FALSE(range->observers_returned);
			CHECK(f.rd.native_height_ranges.size() == 1);
			CHECK(f.rd.frames[0].native_height_ranges.size() == 1);
			CHECK(f.driver.releases == 0);
		}
	}
};

TEST_CASE("[RenderingDevice][NativeHeightRange] exact slot buffer and empty BUSY") {
	TestNativeHeightRangeAccess::wrong_slot_buffer_and_empty_busy();
}
TEST_CASE("[RenderingDevice][NativeHeightRange] checked sink failed begin and unknown cleanup remain distinct") {
	TestNativeHeightRangeAccess::checked_sink_failure_terminal_controls();
}
TEST_CASE("[RenderingDevice][NativeHeightRange] checked sink provenance duplicate and unsupported transfer quarantine") {
	TestNativeHeightRangeAccess::checked_sink_provenance_and_missing_members();
}

TEST_CASE("[RenderingDevice][NativeHeightRange] original callback identity and observer return") {
	TestNativeHeightRangeAccess::callback_identity_and_unknown_refinement();
}
TEST_CASE("[RenderingDevice][NativeHeightRange] native failure preserves finalization owner") {
	TestNativeHeightRangeAccess::failed_source_blocks_finalization();
}

TEST_CASE("[RenderingDevice][NativeHeightRange] settled native prefix returns despite sticky business error") {
	TestNativeHeightRangeAccess::settled_prefix_keeps_business_error_but_returns_sources();
}

TEST_CASE("[RenderingDevice][NativeHeightRange] failed observer return is sticky and not replayed") {
	TestNativeHeightRangeAccess::failed_prefix_release_is_sticky_and_not_replayed();
}

TEST_CASE("[RenderingDevice][NativeHeightRange] business failure cannot settle unsafe native prefix") {
	TestNativeHeightRangeAccess::business_failure_does_not_settle_unsafe_prefix();
}

TEST_CASE("[RenderingDevice][NativeHeightRange] retain BUSY and shared source observers") {
	TestNativeHeightRangeAccess::retain_busy_and_two_independent_cells();
}
TEST_CASE("[RenderingDevice][NativeHeightRange] old frame epoch and empty closure") {
	TestNativeHeightRangeAccess::old_epoch_and_empty_close();
}

TEST_CASE("[RenderingDevice][NativeHeightRange] known no-effect failure settles without native success") {
	TestNativeHeightRangeAccess::known_no_effect_failure_settles_but_keeps_error();
}

TEST_CASE("[RenderingDevice][NativeHeightRange] ordinary and bounded actual issue hooks retain before recycle") {
	TestNativeHeightRangeAccess::actual_issue_hooks_and_retain_barriers();
}
TEST_CASE("[RenderingDevice][NativeHeightRange] sealed synchronous getter retry cannot append or issue again") {
	TestNativeHeightRangeAccess::sealed_flush_retry_blocks_getter_append();
}

TEST_CASE("[RenderingDevice][NativeHeightRange] finite capacity and foreign range refusal") {
	TestNativeHeightRangeAccess::finite_capacity_and_foreign_range_refusal();
}
TEST_CASE("[RenderingDevice][NativeHeightRange] preflight refuses invalid witnesses and active recording") {
	TestNativeHeightRangeAccess::preflight_zero_effect_restrictions();
}

TEST_CASE("[RenderingDevice][NativeHeightRange] post-drain finalization BUSY resumes without reissue") {
	TestNativeHeightRangeAccess::post_drain_finalization_busy_cursor();
}
TEST_CASE("[RenderingDevice][NativeHeightRange] finalization cannot guess live checked sink closed or empty") {
	TestNativeHeightRangeAccess::finalization_does_not_close_live_checked_sink();
}

TEST_CASE("[RenderingDevice][NativeHeightRange] active checked sink refuses raw worker dependency before locks") {
	TestNativeHeightRangeAccess::worker_preflight_refusal_guard();
}

// The normal full suite performs no destructive work. Only the isolated runner
// enables this mode; all process exits below are confined to its child host.
TEST_CASE("[RenderingDevice][NativeHeightRange] actual finalize child") {
	const String mode = OS::get_singleton()->get_environment("GODOT_NHR_FINALIZE_CHILD");
	if (mode == "busy" || mode == "last_ref") {
		TestNativeHeightRangeAccess::actual_finalize_child(mode);
	}
}

TEST_CASE("[RenderingDevice][NativeHeightRange] exact worker source retains before recycle and completes through original observer") {
	TestNativeHeightRangeAccess::worker_source_custody();
}

TEST_CASE("[RenderingDevice][NativeHeightRange] invalid worker reports retain known prefix without completing or replaying") {
	TestNativeHeightRangeAccess::worker_invalid_reports();
}

TEST_CASE("[RenderingDevice][NativeHeightRange] exact checked transfer receipts reject missing duplicate hidden and unknown membership") {
	TestNativeHeightRangeAccess::checked_transfer_receipt_controls();
}

TEST_CASE("[RenderingDevice][NativeHeightRange][ReaderCustody] reader_requested_returns_are_bounded_and_fair") {
	TestNativeHeightRangeAccess::reader_requested_returns_are_bounded_and_fair();
}

TEST_CASE("[RenderingDevice][NativeHeightRange][ReaderCustody] reader_return_pump_separates_record_and_observer_errors") {
	TestNativeHeightRangeAccess::reader_return_pump_separates_record_and_observer_errors();
}

TEST_CASE("[RenderingDevice][NativeHeightRange][ReaderCustody] reader_scope_refuses_forged_and_stale_admission") {
	TestNativeHeightRangeAccess::reader_scope_refuses_forged_and_stale_admission();
}

TEST_CASE("[RenderingDevice][NativeHeightRange][ReaderCustody] producer_reader_lifetime_controls") {
	TestNativeHeightRangeAccess::producer_reader_lifetime_controls();
}

TEST_CASE("[RenderingDevice][NativeHeightRange][ReaderCustody] actual_reader_graph_membership_controls") {
	TestNativeHeightRangeAccess::actual_reader_graph_membership_controls();
}

TEST_CASE("[RenderingDevice][NativeHeightRange][ReaderCustody] reader_first_error_is_sticky") {
	TestNativeHeightRangeAccess::reader_first_error_is_sticky();
}

TEST_CASE("[RenderingDevice][NativeHeightRange][OwnerBudget] one pass resumes paid row into actual poll and release") {
	TestNativeHeightRangeAccess::one_pass_reader_return_resumes_paid_entry();
}
TEST_CASE("[RenderingDevice][NativeHeightRange][OwnerBudget] one pass observes Current producer without revoking resources") {
	TestNativeHeightRangeAccess::one_pass_current_producer_observation();
}

TEST_CASE("[RenderingDevice][NativeHeightRange][ExactPaidOwner] no following draw issues original closed prefix and resumes retain Busy") { TestNativeHeightRangeAccess::exact_paid_no_following_draw(); }
TEST_CASE("[RenderingDevice][NativeHeightRange][ExactPaidOwner] foreign prefix and empty expired exhausted grant preserve original custody") { TestNativeHeightRangeAccess::exact_paid_refusal_controls(); }

TEST_CASE("[RenderingDevice][NativeHeightRange][ExactPaidOwner] settled business failure preserves actual release error and post-return snapshot") { TestNativeHeightRangeAccess::exact_paid_sticky_prefix_return(); }
TEST_CASE("[RenderingDevice][NativeHeightRange][ExactPaidOwner] explicit one-pass grants resume and exhausted duplicate cannot spend twice") { TestNativeHeightRangeAccess::exact_paid_one_pass_grants(); }

TEST_CASE("[RenderingDevice][NativeHeightRange][ExactPaidOwner][R2_01] admitted early issue and retain errors project original prefix without poll") { TestNativeHeightRangeAccess::exact_paid_early_error_snapshot(); }
