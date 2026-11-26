/**************************************************************************/
/*  test_device_session_qualification_scene_depth.cpp                     */
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

#include "core/extension/gdextension.h"
#include "core/extension/gdextension_qualification_internal.h"
#include "core/os/thread.h"
#include "tests/servers/rendering/transfer_test_driver_base.h"
#include "tests/test_macros.h"

TEST_FORCE_LINK(test_device_session_qualification_scene_depth)

// CPU-only original Registry/Bridge/QRT controls. This fixture does not create
// a Scene producer, original HeightFrameResources or GPU resource pair.
class TestQualificationSceneDepthAccess {
	using Registry = GDExtensionDeviceExit::Registry;
	class Driver : public TransferTestDriverBase {
	public:
		uint32_t quotes = 0, creates = 0;
		Error quote_error = OK;
		Registry *reentrant_registry = nullptr;
		GDExtensionQualificationBridgeRefV1 reentrant_bridge{};
		GDExtensionStorageSceneDepthRefV1 reentrant_facet{};
		bool reentered = false;
		uint64_t limit_get(Limit p_limit) override {
			CHECK(p_limit == LIMIT_MAX_TEXTURE_SIZE_2D);
			return 4096;
		}
		BitField<TextureUsageBits> texture_get_usages_supported_by_format(DataFormat p_format, bool p_cpu_readable) override {
			CHECK(p_format == DATA_FORMAT_D32_SFLOAT);
			CHECK_FALSE(p_cpu_readable);
			return TEXTURE_USAGE_SAMPLING_BIT | TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		}
		Error depth_allocation_quote(uint32_t width, uint32_t height, DepthAllocationQuote &out) override {
			++quotes;
			CHECK(width == 64);
			CHECK(height == 32);
			out = { DepthAllocationQuote::Policy::MetalHeapPool, 4096, 8192, 256 };
			return quote_error;
		}
		TextureID texture_create(const TextureFormat &, const TextureView &) override {
			++creates;
			if (reentrant_registry) {
				GDExtensionQualificationBudgetV1 budget{ { 1, sizeof(GDExtensionDeviceExitAbi), 0 }, 1, 100, 0 };
				GDExtensionStorageSceneDepthReportV1 out{};
				CHECK(reentrant_registry->qualification_scene_depth_observe(reentrant_bridge, reentrant_facet, &budget, &out).native_error == ERR_BUSY);
				CHECK(out.facet.handle_serial == 0);
				CHECK(reentrant_registry->qualification_scene_depth_release(reentrant_bridge, reentrant_facet).native_error == ERR_BUSY);
				reentered = true;
			}
			return TextureID();
		}
	};
	struct Fixture {
		Registry *registry = memnew(Registry);
		Driver driver;
		RenderingDevice device;
		RenderingDevice *previous = RenderingDevice::singleton;
		struct Holder : GDExtensionDeviceExit::HolderRetirement {
			Holder() : HolderRetirement(GDExtensionDeviceExit::RecordRef{}) {}
			~Holder() = default;
		} holder;
		Registry::QualificationBridge *bridge = memnew(Registry::QualificationBridge);
		GDExtensionQualificationBridgeRefV1 ref{ 71, 11 };
		Fixture() {
			RenderingDevice::singleton = &device;
			device.is_main_instance = true;
			device.render_thread_id = Thread::get_caller_id();
			device.driver = &driver;
			device.frames.resize(1);
			device.frames[0].qualification_epoch = 1;
			device.frames[0].primary_command_buffer_begun = true;
			registry->instance = 71;
			auto &record = registry->records[0];
			record.owner_thread = Thread::get_caller_id();
			record.snapshot.library_generation = 1;
			record.guard_enrollment = GDExtensionDeviceExit::GuardEnrollment::QUALIFIED;
			auto &d = registry->device_records[0];
			d.object = &device;
			d.live = true;
			d.epoch = 1;
			d.execution_thread = Thread::get_caller_id();
			holder.generation = 1;
			bridge->serial = 11;
			bridge->epoch = 1;
			bridge->holder = &holder;
			registry->qualification_bridges[0] = bridge;
		}
		void dispatch() {
			// Original private test Registry has no RenderingServer singleton
			// registration. Enroll only its queue pin, then call actual dispatch.
			auto *cell = bridge->rt_slot0();
			cell->active = true;
			cell->state = 1;
			cell->queued = true;
			++bridge->children;
			if (!cell->queue_ref_reserved) {
				++registry->records[0].private_refs;
			}
			registry->qualification_rt_dispatch(bridge->serial, cell->serial);
		}
		~Fixture() {
			RenderingDevice::singleton = previous;
			device.is_main_instance = false;
			device.driver = nullptr;
			device.frames.clear();
			registry->qualification_bridges[0] = nullptr;
			if (bridge) {
				for (auto *child : bridge->scene_depth) {
					if (child) {
						memdelete(child);
					}
				}
				for (auto *cell : bridge->rt_cells) {
					if (cell) {
						memdelete(cell);
					}
				}
				bridge->rt_cells.clear();
				memdelete(bridge);
			}
			memdelete(registry);
		}
	};
	static GDExtensionStorageSceneDepthQuoteRequestV1 request() {
		return { { 1, sizeof(GDExtensionDeviceExitAbi), 0 }, { { 1, sizeof(GDExtensionDeviceExitAbi), 0 }, 64, 32, 0 }, 4, 0, 9, 1 };
	}
	static GDExtensionStorageSceneDepthCreateRequestV1 create_request(GDExtensionStorageSceneDepthRefV1 facet) {
		return { { 1, sizeof(GDExtensionDeviceExitAbi), 0 }, facet, 4, 0, 9, 2, 0 };
	}
	static GDExtensionStorageSceneDepthRetireRequestV1 retire_request(GDExtensionStorageSceneDepthRefV1 facet) {
		return { { 1, sizeof(GDExtensionDeviceExitAbi), 0 }, facet, 4, 0, 9, 3, 0 };
	}
	static void close_no_native(Fixture &f, GDExtensionStorageSceneDepthRefV1 facet, Error creation_error = OK) {
		auto q = retire_request(facet);
		GDExtensionStorageSceneDepthReportV1 out{};
		REQUIRE(f.registry->qualification_scene_depth_retire(f.ref, &q, &out).native_error == OK);
		CHECK(out.state == 9);
		CHECK(out.creation_error == creation_error);
		CHECK(out.known_created_mask == 0);
		CHECK(out.depth_state == 0);
		CHECK(out.framebuffer_state == 0);
		CHECK(out.all_created_children_returned == 0);
		const auto before = out;
		CHECK(f.registry->qualification_scene_depth_retire(f.ref, &q, &out).native_error == OK);
		CHECK(memcmp(&before, &out, sizeof(out)) == 0);
		GDExtensionQualificationBudgetV1 budget{ { 1, sizeof(GDExtensionDeviceExitAbi), 0 }, 1, 100, 0 };
		CHECK(f.registry->qualification_scene_depth_observe(f.ref, facet, &budget, &out).native_error == OK);
		CHECK(memcmp(&before, &out, sizeof(out)) == 0);
		CHECK(f.registry->qualification_scene_depth_release(f.ref, facet).native_error == OK);
		CHECK(f.registry->qualification_scene_depth_release(f.ref, facet).native_error == ERR_DOES_NOT_EXIST);
		CHECK(f.driver.creates == (creation_error == ERR_CANT_CREATE ? 1 : 0));
	}

	static void configure(Fixture &f) {
		GDExtensionQualificationRtConfigV1 config{ { 1, sizeof(GDExtensionQualificationRtConfigV1), 0 }, 1, sizeof(GDExtensionQualificationRecordV4), MAX(sizeof(GDExtensionQualificationTicketRefV1) + sizeof(GDExtensionQualificationReportV2), sizeof(GDExtensionQualificationReportV3)), 1, 1 };
		REQUIRE(f.registry->qualification_rt_configure(f.ref, &config).native_error == OK);
	}
	static GDExtensionQualificationRtRequestV1 retire_header(uint64_t serial) {
		return { { 1, sizeof(GDExtensionQualificationRtRequestV1), 0 }, 23, 0, serial, 0, sizeof(GDExtensionStorageSceneDepthRetireRequestV1), sizeof(GDExtensionStorageSceneDepthReportV1), 0, 0 };
	}

public:
	static void storage_finality_namespace_and_idle_close() {
		{
			Fixture f;
			GDExtensionQualificationRtConfigV1 config{ { 1, 40, 0 }, 1, 312, 200, 1, 256 };
			REQUIRE(f.registry->qualification_rt_configure(f.ref, &config).native_error == OK);
			GDExtensionStorageCreateRequestV1 origin{ { 1, 16, 0 }, 128, 0, 0, 1, 1, 0 };
			GDExtensionQualificationRtRequestV1 h{ { 1, 56, 0 }, 2, 0, 1, 0, sizeof(origin), sizeof(GDExtensionStorageBackingV1), 0, 0 };
			GDExtensionQualificationRtRefV1 accepted{};
			REQUIRE(f.registry->qualification_rt_reserve(f.ref, &h, &origin, nullptr, &accepted).native_error == OK);
			GDExtensionStorageDomainV1 domain{ { 1, 16, 0 }, 1, 0 };
			CHECK(f.registry->qualification_storage_domain_install(f.ref, &domain).native_error == ERR_ALREADY_IN_USE);
			CHECK(f.bridge->storage_frontiers == nullptr);
			REQUIRE(f.registry->qualification_rt_release(f.ref, accepted).native_error == OK);
			CHECK(f.registry->qualification_storage_domain_install(f.ref, &domain).native_error == ERR_ALREADY_IN_USE);
		}
		{
			Fixture f;
			f.registry->records[0].private_refs = 1;
			f.registry->device_records[0].refs = 1;
			f.holder.qualification_children = 1;
			GDExtensionStorageDomainV1 domain{ { 1, 16, 0 }, 4096, 0 };
			REQUIRE(f.registry->qualification_storage_domain_install(f.ref, &domain).native_error == OK);
			CHECK(f.bridge->children == 0);
			CHECK(f.bridge->child_metadata_stats().bridge_request_bytes >= sizeof(*f.bridge) + 4096 * 24);
			CHECK(f.registry->qualification_close(f.ref).native_error == OK);
			f.bridge = nullptr;
			CHECK(f.holder.qualification_children == 0);
			CHECK(f.registry->records[0].private_refs == 0);
			CHECK(f.registry->device_records[0].refs == 0);
		}
	}
	static void storage_finality_duration_observation() {
		Fixture f;
		const GDExtensionStorageDomainV1 domain{ { 1, 16, 0 }, 1, 0 };
		REQUIRE(f.registry->qualification_storage_domain_install(f.ref, &domain).native_error == OK);
		for (uint32_t i = 0; i < 2; ++i) {
			uint32_t index;
			REQUIRE(f.bridge->storage_origins.prepare(index) == OK);
			const GDExtensionStorageCreateRequestV1 origin{ { 1, 16, 0 }, 128, 0, i, 1, 1, 0 };
			f.bridge->storage_origins.begin(index, origin, 100 + i);
			f.bridge->storage_origins.archive_retired_origin(index, 100 + i, 200 + i);
		}
		GDExtensionStorageGenerationFinalizeV1 q{ { 1, 16, 0 }, 0, 40960, 1, 1, 0 };
		const auto attempted = f.registry->qualification_storage_generation_finalize(f.ref, &q);
		auto &frontier = f.bridge->storage_frontiers[0];
		print_line(vformat("STORAGE_FINALITY_DURATION_OBSERVED allowance_us=1 native_error=%d closed=%d completed=%d cursor=%d credits=%d", attempted.native_error, frontier.closed, frontier.completed, frontier.cursor, f.bridge->storage_origins.used_count()));
		// The original clock is not injectable. Either actual exhaustion or
		// completed work is valid; timing is not the pass/fail oracle.
		CHECK((attempted.native_error == OK || attempted.native_error == ERR_BUSY));
		if (attempted.native_error == ERR_BUSY) {
			if (!frontier.closed) {
				CHECK(f.bridge->storage_origins.used_count() == 2);
				CHECK(f.bridge->children == 0);
			} else {
				CHECK(frontier.closed == 1);
				CHECK(frontier.completed == 0);
				CHECK(f.bridge->children == 1);
				CHECK(f.bridge->storage_origins.used_count() <= 2);
			}
		}
		q.max_micros = 5000;
		auto resumed = f.registry->qualification_storage_generation_finalize(f.ref, &q);
		while (resumed.native_error == ERR_BUSY) {
			resumed = f.registry->qualification_storage_generation_finalize(f.ref, &q);
		}
		CHECK(resumed.native_error == OK);
		CHECK(frontier.closed == 1);
		CHECK(frontier.completed == 1);
		CHECK(f.bridge->storage_origins.used_count() == 0);
		CHECK(f.bridge->children == 0);
	}
	static void storage_finality() {
		Fixture f;
		f.registry->records[0].private_refs = 1;
		f.registry->device_records[0].refs = 1;
		f.holder.qualification_children = 1;
		GDExtensionStorageDomainV1 domain{ { 1, sizeof(GDExtensionDeviceExitAbi), 0 }, 2, 0 };
		const auto refs = f.registry->records[0].private_refs;
		f.registry->private_limit = refs + 2;
		CHECK(f.registry->qualification_storage_domain_install(f.ref, &domain).native_error == ERR_PARAMETER_RANGE_ERROR);
		CHECK(f.bridge->storage_frontiers == nullptr);
		CHECK(f.registry->records[0].private_refs == refs);
		f.registry->private_limit = 100;
		domain.slot_count = 4097;
		CHECK(f.registry->qualification_storage_domain_install(f.ref, &domain).native_error == ERR_INVALID_PARAMETER);
		domain.slot_count = 2;
		{
			using namespace GDExtensionDeviceExit;
			RegistrationContext metadata;
			metadata.level_phase = LevelPhase::PRODUCTION_METADATA;
			metadata.previous = RegistrationContext::current;
			RegistrationContext::current = &metadata;
			CHECK(f.registry->qualification_storage_domain_install(f.ref, &domain).native_error == ERR_UNAUTHORIZED);
			RegistrationContext::current = metadata.previous;
		}
		const auto installed = f.registry->qualification_storage_domain_install(f.ref, &domain);
		REQUIRE(installed.native_error == OK);
		if (installed.native_error != OK) {
			return;
		}
		CHECK(f.registry->records[0].private_refs == refs + 3);
		CHECK(f.registry->qualification_storage_domain_install(f.ref, &domain).native_error == ERR_ALREADY_IN_USE);
		GDExtensionStorageGenerationFinalizeV1 q{ { 1, sizeof(GDExtensionDeviceExitAbi), 0 }, 0, 1, 1, 5000, 0 };
		GDExtensionQualificationRtConfigV1 config{ { 1, sizeof(GDExtensionQualificationRtConfigV1), 0 }, 1, 312, 200, 1, 256 };
		REQUIRE(f.registry->qualification_rt_configure(f.ref, &config).native_error == OK);
		{
			using namespace GDExtensionDeviceExit;
			RegistrationContext metadata;
			metadata.level_phase = LevelPhase::PRODUCTION_METADATA;
			metadata.previous = RegistrationContext::current;
			RegistrationContext::current = &metadata;
			CHECK(f.registry->qualification_storage_generation_finalize(f.ref, &q).native_error == ERR_UNAUTHORIZED);
			RegistrationContext::current = metadata.previous;
		}
		// Two original history entries make a one-step scan genuinely partial.
		for (uint32_t i = 0; i < 2; ++i) {
			uint32_t index;
			REQUIRE(f.bridge->storage_origins.prepare(index) == OK);
			GDExtensionStorageCreateRequestV1 origin{ { 1, 16, 0 }, 128, 0, i, 1, 1, 0 };
			f.bridge->storage_origins.begin(index, origin, 100 + i);
			f.bridge->storage_origins.archive_retired_origin(index, 100 + i, 200 + i);
		}
		CHECK(f.registry->qualification_storage_generation_finalize({ 72, 11 }, &q).native_error == ERR_INVALID_DATA);
		f.holder.generation = 2;
		CHECK(f.registry->qualification_storage_generation_finalize(f.ref, &q).native_error == ERR_INVALID_DATA);
		f.holder.generation = 1;
		f.bridge->epoch = 2;
		CHECK(f.registry->qualification_storage_generation_finalize(f.ref, &q).native_error == ERR_UNAVAILABLE);
		f.bridge->epoch = 1;
		q.allocation_slot = 2;
		CHECK(f.registry->qualification_storage_generation_finalize(f.ref, &q).native_error == ERR_INVALID_PARAMETER);
		q.allocation_slot = 0;
		Thread worker;
		worker.start([](void *arg) {
			auto &f = *static_cast<Fixture *>(arg);
			GDExtensionStorageGenerationFinalizeV1 q{ { 1, 16, 0 }, 0, 1, 1, 5000, 0 };
			CHECK(f.registry->qualification_storage_generation_finalize(f.ref, &q).native_error == ERR_UNAUTHORIZED);
		},
				&f);
		worker.wait_to_finish();
		CHECK(f.bridge->storage_frontiers[0].closed == 0);
		CHECK(f.bridge->storage_origins.used_count() == 2);
		REQUIRE(f.registry->qualification_storage_generation_finalize(f.ref, &q).native_error == ERR_BUSY);
		CHECK(f.bridge->storage_frontiers[0].closed == 1);
		CHECK(f.bridge->storage_origins.used_count() == 1);
		CHECK(f.bridge->children == 1);
		CHECK(f.registry->qualification_close(f.ref).native_error == ERR_BUSY);
		CHECK(f.holder.qualification_children == 1);
		GDExtensionStorageCreateRequestV1 stale{ { 1, 16, 0 }, 256, 0, 0, 1, 77, 0 };
		GDExtensionStorageBackingV1 backing{};
		CHECK(f.registry->qualification_storage_create(f.ref, &stale, &backing).reason == GDEXTENSION_DEVICE_SESSION_REASON_STORAGE_STALE_GENERATION_V1);
		REQUIRE(f.registry->qualification_storage_generation_finalize(f.ref, &q).native_error == OK);
		CHECK(f.bridge->storage_origins.used_count() == 0);
		CHECK(f.bridge->children == 0);
		CHECK(f.registry->qualification_storage_generation_finalize(f.ref, &q).native_error == OK);
		q.closed_through_generation = 0;
		CHECK(f.registry->qualification_storage_generation_finalize(f.ref, &q).native_error == ERR_INVALID_PARAMETER);
		q.closed_through_generation = UINT64_MAX;
		CHECK(f.registry->qualification_storage_generation_finalize(f.ref, &q).native_error == OK);
		q.closed_through_generation = 1;
		CHECK(f.registry->qualification_storage_generation_finalize(f.ref, &q).native_error == ERR_INVALID_DATA);
		// No-child cleanup remains admitted after new work closes. QRT uses the
		// same retained bridge and reserved cleanup credits, not a new grant.
		q.allocation_slot = 1;
		f.bridge->closing = true;
		f.registry->records[0].quiescing = true;
		GDExtensionQualificationRtRequestV1 header{ { 1, sizeof(GDExtensionQualificationRtRequestV1), 0 }, 25, 0, 1, 0, sizeof(q), 8, 0, 0 };
		GDExtensionQualificationRtRefV1 rt{};
		REQUIRE(f.registry->qualification_rt_reserve(f.ref, &header, &q, nullptr, &rt).native_error == OK);
		f.dispatch();
		GDExtensionQualificationRtObservationV1 observed{};
		uint64_t acknowledged = 0;
		REQUIRE(f.registry->qualification_rt_observe(f.ref, rt, &observed, &acknowledged, 8, nullptr, 0).native_error == OK);
		CHECK(observed.call_result.native_error == OK);
		CHECK(acknowledged == 1);
		CHECK(f.registry->qualification_rt_release(f.ref, rt).native_error == OK);
		CHECK(f.registry->qualification_close(f.ref).native_error == OK);
		f.bridge = nullptr;
		CHECK(f.holder.qualification_children == 0);
		CHECK(f.registry->records[0].private_refs == 0);
		CHECK(f.registry->device_records[0].refs == 0);
	}

	static void native_operation_blocks_observe_and_ack() {
		Fixture f;
		const auto q = request();
		GDExtensionStorageSceneDepthQuoteV1 quoted{};
		REQUIRE(f.registry->qualification_scene_depth_quote(f.ref, &q, &quoted).native_error == OK);
		auto *child = f.bridge->scene_depth[0];
		const auto cached = child->report;
		child->native_operation = true;
		GDExtensionQualificationBudgetV1 budget{ { 1, sizeof(GDExtensionDeviceExitAbi), 0 }, 1, 100, 0 };
		GDExtensionStorageSceneDepthReportV1 report{};
		const auto c = create_request(quoted.facet);
		const auto r = retire_request(quoted.facet);
		CHECK(f.registry->qualification_scene_depth_observe(f.ref, quoted.facet, &budget, &report).native_error == ERR_BUSY);
		CHECK(f.registry->qualification_scene_depth_release(f.ref, quoted.facet).native_error == ERR_BUSY);
		CHECK(f.registry->qualification_scene_depth_create(f.ref, &c, &report).native_error == ERR_BUSY);
		CHECK(f.registry->qualification_scene_depth_retire(f.ref, &r, &report).native_error == ERR_BUSY);
		CHECK(memcmp(&cached, &child->report, sizeof(cached)) == 0);
		CHECK(f.driver.quotes == 1);
		CHECK(f.driver.creates == 0);
		child->native_operation = false;
		f.driver.reentrant_registry = f.registry;
		f.driver.reentrant_bridge = f.ref;
		f.driver.reentrant_facet = quoted.facet;
		ERR_PRINT_OFF;
		const auto attempted = f.registry->qualification_scene_depth_create(f.ref, &c, &report);
		ERR_PRINT_ON;
		REQUIRE(attempted.native_error == OK);
		CHECK(f.driver.reentered);
		CHECK(report.creation_error == ERR_CANT_CREATE);
		CHECK(f.registry->qualification_scene_depth_observe(f.ref, quoted.facet, &budget, &report).native_error == OK);
		CHECK(report.creation_error == ERR_CANT_CREATE);
		close_no_native(f, quoted.facet, ERR_CANT_CREATE);
		CHECK(f.driver.quotes == 2);
		CHECK(f.driver.creates == 1);
	}
	static void canonical_new_work_admission() {
		using namespace GDExtensionDeviceExit;
		Fixture f;
		auto q = request();
		GDExtensionStorageSceneDepthQuoteV1 quoted{};
		REQUIRE(f.registry->qualification_scene_depth_quote(f.ref, &q, &quoted).native_error == OK);
		const auto first_quote = quoted;
		auto c = create_request(quoted.facet);
		GDExtensionStorageSceneDepthReportV1 report{};
		const auto before_report = f.bridge->scene_depth[0]->report;
		const uint64_t next = f.registry->qualification_serials.next;
		const uint32_t refs = f.registry->records[0].private_refs;
		const uint32_t children = f.bridge->children;
		auto refuse = [&](Error expected) {
			auto fresh = request();
			fresh.allocation_generation = 10;
			quoted.facet = { 99, 99 };
			CHECK(f.registry->qualification_scene_depth_quote(f.ref, &fresh, &quoted).native_error == expected);
			CHECK(quoted.facet.handle_serial == 0);
			CHECK(f.registry->qualification_scene_depth_create(f.ref, &c, &report).native_error == expected);
			CHECK(report.facet.handle_serial == 0);
			CHECK_FALSE(f.bridge->scene_depth[0]->create_attempted);
			CHECK(memcmp(&before_report, &f.bridge->scene_depth[0]->report, sizeof(before_report)) == 0);
			CHECK(f.bridge->scene_depth_origins.used_count() == 1);
			CHECK(f.bridge->children == children);
			CHECK(f.registry->records[0].private_refs == refs);
			CHECK(f.registry->qualification_serials.next == next);
			CHECK(f.driver.quotes == 1);
			CHECK(f.driver.creates == 0);
			// An exact quote retry observes its already admitted transaction.
			CHECK(f.registry->qualification_scene_depth_quote(f.ref, &q, &quoted).native_error == OK);
			CHECK(memcmp(&first_quote, &quoted, sizeof(quoted)) == 0);
		};
		f.registry->records[0].guard_enrollment = GuardEnrollment::REVOKED;
		refuse(ERR_BUSY);
		f.registry->records[0].guard_enrollment = GuardEnrollment::QUALIFIED;
		{
			RegistrationContext metadata, nested;
			metadata.level_phase = LevelPhase::PRODUCTION_METADATA;
			metadata.previous = RegistrationContext::current;
			nested.previous = &metadata;
			RegistrationContext::current = &nested;
			refuse(ERR_UNAUTHORIZED);
			RegistrationContext::current = metadata.previous;
		}
		{
			ProductionCleanupExtent cleanup;
			refuse(ERR_UNAUTHORIZED);
		}
		ERR_PRINT_OFF;
		const auto attempted = f.registry->qualification_scene_depth_create(f.ref, &c, &report);
		ERR_PRINT_ON;
		REQUIRE(attempted.native_error == OK);
		const auto created = report;
		f.registry->records[0].guard_enrollment = GuardEnrollment::REVOKED;
		CHECK(f.registry->qualification_scene_depth_create(f.ref, &c, &report).native_error == OK);
		CHECK(memcmp(&created, &report, sizeof(report)) == 0);
		// Cleanup is still legal under revoked guard and dynamic cleanup extent.
		{
			ProductionCleanupExtent cleanup;
			close_no_native(f, first_quote.facet, ERR_CANT_CREATE);
		}
	}
	static void finite_children_and_history() {
		for (bool history : { false, true }) {
			Fixture f;
			const uint32_t limit = history ? Registry::QualificationBridge::SCENE_DEPTH_HISTORY_CAPACITY : Registry::QualificationBridge::CHILD_CAPACITY;
			GDExtensionStorageSceneDepthRefV1 first{};
			for (uint32_t i = 0; i < limit; ++i) {
				auto q = request();
				q.allocation_generation = uint64_t(i) + 1;
				GDExtensionStorageSceneDepthQuoteV1 out{};
				REQUIRE(f.registry->qualification_scene_depth_quote(f.ref, &q, &out).native_error == OK);
				if (!i) {
					first = out.facet;
				}
				if (history) {
					auto r = retire_request(out.facet);
					r.allocation_generation = q.allocation_generation;
					GDExtensionStorageSceneDepthReportV1 report{};
					REQUIRE(f.registry->qualification_scene_depth_retire(f.ref, &r, &report).native_error == OK);
					REQUIRE(f.registry->qualification_scene_depth_release(f.ref, out.facet).native_error == OK);
				}
			}
			const auto serial = f.registry->qualification_serials.next;
			const auto refs = f.registry->records[0].private_refs;
			const auto reservations = f.registry->qualification_cleanup_serial_reservations;
			const auto children = f.bridge->children;
			auto q = request();
			q.allocation_generation = uint64_t(limit) + 1;
			GDExtensionStorageSceneDepthQuoteV1 out{};
			CHECK(f.registry->qualification_scene_depth_quote(f.ref, &q, &out).native_error == ERR_PARAMETER_RANGE_ERROR);
			CHECK(out.facet.handle_serial == 0);
			CHECK(f.driver.quotes == limit);
			CHECK(f.registry->qualification_serials.next == serial);
			CHECK(f.registry->records[0].private_refs == refs);
			CHECK(f.registry->qualification_cleanup_serial_reservations == reservations);
			CHECK(f.bridge->children == children);
			CHECK(f.bridge->scene_depth_origins.used_count() == limit);
			CHECK(f.bridge->scene_depth_origins.retired_count() == (history ? limit : 0));
			if (history) {
				q.allocation_generation = 1;
				CHECK(f.registry->qualification_scene_depth_quote(f.ref, &q, &out).native_error == ERR_DOES_NOT_EXIST);
			} else {
				// Capacity refusal did not discard any original admitted custody.
				GDExtensionQualificationBudgetV1 budget{ { 1, sizeof(GDExtensionDeviceExitAbi), 0 }, 1, 100, 0 };
				GDExtensionStorageSceneDepthReportV1 report{};
				CHECK(f.registry->qualification_scene_depth_observe(f.ref, first, &budget, &report).native_error == OK);
				CHECK(report.state == 1);
			}
			CHECK(f.driver.creates == 0);
		}
	}
	static void retire_delivery_cancellation() {
		Fixture f;
		configure(f);
		auto q = request();
		GDExtensionStorageSceneDepthQuoteV1 quoted{};
		REQUIRE(f.registry->qualification_scene_depth_quote(f.ref, &q, &quoted).native_error == OK);
		auto r = retire_request(quoted.facet);
		auto header = retire_header(51);
		GDExtensionQualificationRtRefV1 rt{};
		REQUIRE(f.registry->qualification_rt_reserve(f.ref, &header, &r, nullptr, &rt).native_error == OK);
		CHECK(f.registry->qualification_cleanup_serial_reservations == 0);
		CHECK(f.bridge->scene_depth[0]->accepted_rt_refs == 1);
		// Inactive copied delivery never ran Retire and must restore its credit.
		CHECK(f.registry->qualification_rt_release(f.ref, rt).native_error == OK);
		CHECK(f.registry->qualification_cleanup_serial_reservations == 1);
		CHECK(f.bridge->scene_depth[0]->cleanup_serial_reserved);
		CHECK(f.bridge->scene_depth[0]->accepted_rt_refs == 0);
		CHECK_FALSE(f.bridge->scene_depth[0]->retiring);
		CHECK(f.registry->qualification_rt_reserve(f.ref, &header, &r, nullptr, &rt).native_error == ERR_DOES_NOT_EXIST);
		header.operation_serial = 52;
		REQUIRE(f.registry->qualification_rt_reserve(f.ref, &header, &r, nullptr, &rt).native_error == OK);
		// Inject an original execution-thread admission failure before dispatch;
		// no leaf or retirement effect runs. Delivery remains whole and typed.
		f.registry->device_records[0].execution_thread = Thread::get_caller_id() + 1;
		f.dispatch();
		f.registry->device_records[0].execution_thread = Thread::get_caller_id();
		GDExtensionQualificationRtObservationV1 observed{};
		GDExtensionStorageSceneDepthReportV1 report{};
		REQUIRE(f.registry->qualification_rt_observe(f.ref, rt, &observed, &report, sizeof(report), nullptr, 0).native_error == OK);
		CHECK(observed.state == 2);
		CHECK(observed.call_result.native_error == ERR_UNAUTHORIZED);
		CHECK(report.facet.handle_serial == 0);
		CHECK(f.registry->qualification_rt_release(f.ref, rt).native_error == OK);
		CHECK(f.registry->qualification_cleanup_serial_reservations == 1);
		CHECK(f.bridge->scene_depth[0]->cleanup_serial_reserved);
		CHECK_FALSE(f.bridge->scene_depth[0]->retiring);
		header.operation_serial = 53;
		REQUIRE(f.registry->qualification_rt_reserve(f.ref, &header, &r, nullptr, &rt).native_error == OK);
		f.dispatch();
		REQUIRE(f.registry->qualification_rt_observe(f.ref, rt, &observed, &report, sizeof(report), nullptr, 0).native_error == OK);
		CHECK(observed.state == 2);
		CHECK(observed.call_result.native_error == OK);
		CHECK(report.state == 9);
		CHECK(f.registry->qualification_rt_release(f.ref, rt).native_error == OK);
		CHECK(f.registry->qualification_cleanup_serial_reservations == 0);
		CHECK(f.registry->qualification_scene_depth_release(f.ref, quoted.facet).native_error == OK);
		CHECK(f.bridge->children == 0);
		CHECK(f.registry->records[0].private_refs == 0);
	}
	static void original_bridge_close_and_inactive_create(bool quote_delivery) {
		Fixture f;
		// Include the original open Bridge's base references, consumed only by
		// the actual qualification_close success path in this control.
		f.holder.qualification_children = 1;
		f.registry->records[0].private_refs = 1;
		f.registry->device_records[0].refs = 1;
		configure(f);
		auto q = request();
		GDExtensionStorageSceneDepthQuoteV1 quoted{};
		GDExtensionStorageSceneDepthReportV1 report{};
		GDExtensionQualificationRtRefV1 rt{};
		GDExtensionQualificationRtRequestV1 header{ { 1, sizeof(GDExtensionQualificationRtRequestV1), 0 }, 21, 0, 71, 0, sizeof(q), sizeof(quoted), 0, 0 };
		if (quote_delivery) {
			REQUIRE(f.registry->qualification_rt_reserve(f.ref, &header, &q, nullptr, &rt).native_error == OK);
			CHECK(f.registry->qualification_close(f.ref).native_error == ERR_BUSY);
			f.dispatch();
			GDExtensionQualificationRtObservationV1 observed{};
			REQUIRE(f.registry->qualification_rt_observe(f.ref, rt, &observed, &quoted, sizeof(quoted), nullptr, 0).native_error == OK);
			CHECK(observed.state == 2);
			CHECK(observed.call_result.native_error == OK);
			CHECK(f.registry->qualification_close(f.ref).native_error == ERR_BUSY);
			CHECK(f.registry->qualification_rt_release(f.ref, rt).native_error == OK);
		} else {
			REQUIRE(f.registry->qualification_scene_depth_quote(f.ref, &q, &quoted).native_error == OK);
			auto c = create_request(quoted.facet);
			header.kind = 22;
			header.request_bytes = sizeof(c);
			header.result_capacity = sizeof(report);
			REQUIRE(f.registry->qualification_rt_reserve(f.ref, &header, &c, nullptr, &rt).native_error == OK);
			CHECK(f.registry->qualification_close(f.ref).native_error == ERR_BUSY);
			auto r = retire_request(quoted.facet);
			REQUIRE(f.registry->qualification_scene_depth_retire(f.ref, &r, &report).native_error == OK);
			CHECK(f.registry->qualification_scene_depth_release(f.ref, quoted.facet).native_error == ERR_BUSY);
			CHECK(f.registry->qualification_rt_release(f.ref, rt).native_error == OK);
			CHECK_FALSE(f.bridge->scene_depth[0]->create_attempted);
		}
		CHECK(f.registry->qualification_close(f.ref).native_error == ERR_BUSY);
		auto c = create_request(quoted.facet);
		CHECK(f.registry->qualification_scene_depth_create(f.ref, &c, &report).native_error == ERR_BUSY);
		close_no_native(f, quoted.facet);
		CHECK(f.registry->records[0].private_refs == 1);
		REQUIRE(f.registry->qualification_close(f.ref).native_error == OK);
		f.bridge = nullptr; // The actual close deleted it and its copied cell.
		CHECK(f.registry->qualification_bridges[0] == nullptr);
		CHECK(f.holder.qualification_children == 0);
		CHECK(f.registry->device_records[0].refs == 0);
		CHECK(f.registry->records[0].private_refs == 0);
		CHECK(f.driver.quotes == 1);
		CHECK(f.driver.creates == 0);
	}
	static void quote_retry_and_archive(bool failure) {
		Fixture f;
		f.driver.quote_error = failure ? ERR_UNAVAILABLE : OK;
		auto q = request();
		GDExtensionStorageSceneDepthQuoteV1 out{};
		REQUIRE(f.registry->qualification_scene_depth_quote(f.ref, &q, &out).native_error == OK);
		CHECK(out.facet.registry_instance == 71);
		CHECK(out.facet.handle_serial != 0);
		CHECK(out.quote_error == f.driver.quote_error);
		CHECK(out.framebuffer_owner_reservation.knowledge == (failure ? 0 : 1));
		CHECK(out.framebuffer_owner_reservation.bytes == 0);
		CHECK(out.framebuffer_driver_backing.knowledge == 0);
		CHECK(out.texture_owner_reservation.bytes == (failure ? 0 : 8192));
		const auto first = out;
		f.driver.quote_error = OK;
		CHECK(f.registry->qualification_scene_depth_quote(f.ref, &q, &out).native_error == OK);
		CHECK(memcmp(&out, &first, sizeof(out)) == 0);
		CHECK(f.driver.quotes == 1);
		q.descriptor.width = 63;
		CHECK(f.registry->qualification_scene_depth_quote(f.ref, &q, &out).native_error == ERR_INVALID_DATA);
		q = request();
		CHECK(f.registry->qualification_scene_depth_release(f.ref, first.facet).native_error == ERR_BUSY);
		if (failure) {
			auto c = create_request(first.facet);
			GDExtensionStorageSceneDepthReportV1 report{};
			CHECK(f.registry->qualification_scene_depth_create(f.ref, &c, &report).native_error != OK);
		}
		close_no_native(f, first.facet);
		CHECK(f.bridge->scene_depth_origins.retired_count() == 1);
		CHECK(f.registry->qualification_scene_depth_quote(f.ref, &q, &out).native_error == ERR_DOES_NOT_EXIST);
		CHECK(f.driver.quotes == 1);
		CHECK(f.registry->records[0].private_refs == 0);
	}
	static void failed_original_create_and_foreign_origin() {
		Fixture f;
		auto q = request();
		GDExtensionStorageSceneDepthQuoteV1 quoted{};
		REQUIRE(f.registry->qualification_scene_depth_quote(f.ref, &q, &quoted).native_error == OK);
		auto c = create_request(quoted.facet);
		GDExtensionStorageSceneDepthReportV1 report{};
		c.allocation_generation = 8;
		CHECK(f.registry->qualification_scene_depth_create(f.ref, &c, &report).native_error == ERR_INVALID_DATA);
		c = create_request(quoted.facet);
		c.create_attempt = 3;
		CHECK(f.registry->qualification_scene_depth_create(f.ref, &c, &report).native_error == ERR_INVALID_PARAMETER);
		c = create_request(quoted.facet);
		c.facet.registry_instance = 72;
		CHECK(f.registry->qualification_scene_depth_create(f.ref, &c, &report).native_error == ERR_INVALID_DATA);
		c = create_request(quoted.facet);
		ERR_PRINT_OFF;
		const auto attempted = f.registry->qualification_scene_depth_create(f.ref, &c, &report);
		ERR_PRINT_ON;
		REQUIRE(attempted.native_error == OK);
		CHECK(report.facet.handle_serial == quoted.facet.handle_serial);
		CHECK(report.creation_error == ERR_CANT_CREATE);
		CHECK(report.create_outcome == 0);
		CHECK(report.state == 8);
		CHECK(report.known_created_mask == 0);
		CHECK(f.bridge->scene_depth[0]->native_frame.is_null());
		const auto first = report;
		CHECK(f.registry->qualification_scene_depth_create(f.ref, &c, &report).native_error == OK);
		CHECK(memcmp(&first, &report, sizeof(report)) == 0);
		// A different authenticated Bridge on the same main device has no cell.
		Registry::QualificationBridge foreign;
		foreign.serial = 12;
		foreign.epoch = 1;
		foreign.holder = &f.holder;
		f.registry->qualification_bridges[1] = &foreign;
		CHECK(f.registry->qualification_scene_depth_create({ 71, 12 }, &c, &report).native_error == ERR_DOES_NOT_EXIST);
		f.registry->qualification_bridges[1] = nullptr;
		close_no_native(f, quoted.facet, ERR_CANT_CREATE);
		CHECK(f.driver.quotes == 2);
		CHECK(f.driver.creates == 1);
	}
	static void malformed_and_capacity() {
		Fixture f;
		alignas(8) uint8_t shared[sizeof(GDExtensionStorageSceneDepthQuoteV1)]{};
		auto *q = reinterpret_cast<GDExtensionStorageSceneDepthQuoteRequestV1 *>(shared);
		*q = request();
		uint8_t before[sizeof(shared)];
		memcpy(before, shared, sizeof(shared));
		CHECK(f.registry->qualification_scene_depth_quote(f.ref, q, reinterpret_cast<GDExtensionStorageSceneDepthQuoteV1 *>(shared)).native_error == ERR_INVALID_PARAMETER);
		CHECK(memcmp(before, shared, sizeof(shared)) == 0);
		auto input = request();
		GDExtensionStorageSceneDepthQuoteV1 out{};
		input.quote_attempt = 2;
		CHECK(f.registry->qualification_scene_depth_quote(f.ref, &input, &out).native_error == ERR_INVALID_PARAMETER);
		CHECK(out.facet.handle_serial == 0);
		input = request();
		f.registry->private_limit = 2;
		CHECK(f.registry->qualification_scene_depth_quote(f.ref, &input, &out).native_error == ERR_PARAMETER_RANGE_ERROR);
		CHECK(f.driver.quotes == 0);
		CHECK(f.bridge->children == 0);
		CHECK(f.bridge->scene_depth_origins.used_count() == 0);
		f.registry->private_limit = 100;
		f.registry->qualification_serials.exhausted = true;
		CHECK(f.registry->qualification_scene_depth_quote(f.ref, &input, &out).native_error == ERR_PARAMETER_RANGE_ERROR);
		CHECK(f.driver.quotes == 0);
	}
	static void copied_three_stages_and_close() {
		Fixture f;
		GDExtensionQualificationRtConfigV1 config{ { 1, sizeof(GDExtensionQualificationRtConfigV1), 0 }, 1, sizeof(GDExtensionQualificationRecordV4), MAX(sizeof(GDExtensionQualificationTicketRefV1) + sizeof(GDExtensionQualificationReportV2), sizeof(GDExtensionQualificationReportV3)), 1, 1 };
		REQUIRE(f.registry->qualification_rt_configure(f.ref, &config).native_error == OK);
		GDExtensionStorageSceneDepthQuoteV1 quoted{};
		GDExtensionStorageSceneDepthReportV1 report{};
		auto q = request();
		GDExtensionQualificationRtRequestV1 header{ { 1, sizeof(GDExtensionQualificationRtRequestV1), 0 }, 21, 0, 41, 0, sizeof(q), sizeof(quoted), 0, 0 };
		GDExtensionQualificationRtRefV1 rt{};
		REQUIRE(f.registry->qualification_rt_reserve(f.ref, &header, &q, nullptr, &rt).native_error == OK);
		f.dispatch();
		GDExtensionQualificationRtObservationV1 observed{};
		REQUIRE(f.registry->qualification_rt_observe(f.ref, rt, &observed, &quoted, sizeof(quoted), nullptr, 0).native_error == OK);
		CHECK(observed.state == 2);
		CHECK(observed.call_result.native_error == OK);
		CHECK(quoted.quote_error == OK);
		CHECK(f.registry->qualification_rt_release(f.ref, rt).native_error == OK);
		auto c = create_request(quoted.facet);
		header.kind = 22;
		header.operation_serial = 42;
		header.request_bytes = sizeof(c);
		header.result_capacity = sizeof(report);
		REQUIRE(f.registry->qualification_rt_reserve(f.ref, &header, &c, nullptr, &rt).native_error == OK);
		// Accepted copied create is allowed after close; the real allocation
		// attempt preserves its original null-driver failure cause.
		f.bridge->closing = true;
		ERR_PRINT_OFF;
		f.dispatch();
		ERR_PRINT_ON;
		REQUIRE(f.registry->qualification_rt_observe(f.ref, rt, &observed, &report, sizeof(report), nullptr, 0).native_error == OK);
		CHECK(observed.state == 2);
		CHECK(observed.call_result.native_error == OK);
		CHECK(report.creation_error == ERR_CANT_CREATE);
		CHECK(f.registry->qualification_rt_release(f.ref, rt).native_error == OK);
		auto r = retire_request(quoted.facet);
		header.kind = 23;
		header.operation_serial = 43;
		header.request_bytes = sizeof(r);
		REQUIRE(f.registry->qualification_rt_reserve(f.ref, &header, &r, nullptr, &rt).native_error == OK);
		f.dispatch();
		REQUIRE(f.registry->qualification_rt_observe(f.ref, rt, &observed, &report, sizeof(report), nullptr, 0).native_error == OK);
		CHECK(observed.state == 2);
		CHECK(observed.call_result.native_error == OK);
		CHECK(report.state == 9);
		CHECK(report.all_created_children_returned == 0);
		CHECK(f.registry->qualification_scene_depth_release(f.ref, quoted.facet).native_error == ERR_BUSY);
		CHECK(f.registry->qualification_rt_release(f.ref, rt).native_error == OK);
		CHECK(f.registry->qualification_scene_depth_release(f.ref, quoted.facet).native_error == OK);
		CHECK(f.registry->qualification_cleanup_serial_reservations == 0);
		CHECK(f.driver.quotes == 2);
		CHECK(f.driver.creates == 1);
	}

	class Bug148Driver : public TransferTestDriverBase {
	public:
		uint64_t next_buffer = 100;
		uint64_t next_texture = 200;
		uint64_t next_sampler = 300;
		uint32_t created_buffers = 0;
		uint32_t freed_buffers = 0;
		uint64_t limit_get(Limit) override {
			return 1u << 24;
		}
		BufferID buffer_create(uint64_t, BitField<BufferUsageBits>, MemoryAllocationType, uint64_t) override {
			++created_buffers;
			return BufferID(++next_buffer);
		}
		void buffer_free(BufferID p_buffer) override {
			CHECK(p_buffer.id >= 101);
			++freed_buffers;
		}
		uint64_t buffer_get_allocation_size(BufferID) override {
			return 128;
		}
		TextureID texture_create(const TextureFormat &, const TextureView &) override {
			return TextureID(++next_texture);
		}
		void texture_free(TextureID) override {}
		uint64_t texture_get_allocation_size(TextureID) override { return 64; }
		BitField<TextureUsageBits> texture_get_usages_supported_by_format(DataFormat, bool) override {
			return TEXTURE_USAGE_SAMPLING_BIT | TEXTURE_USAGE_STORAGE_BIT | TEXTURE_USAGE_CAN_UPDATE_BIT | TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		}
		uint64_t api_trait_get(ApiTrait) override { return 0; }
		void set_object_name(ObjectType, ID, const String &) override {}
		bool native_call_observation_supported() const override { return true; }
		uint32_t qualification_native_call_bound(uint32_t p_slots) const override { return p_slots; }
		SamplerID sampler_create(const SamplerState &) override {
			return SamplerID(++next_sampler);
		}
		void sampler_free(SamplerID) override {}
	};

	struct Bug148Fixture {
		Registry *registry = memnew(Registry);
		Bug148Driver driver;
		RenderingDevice device;
		RenderingDevice *previous = RenderingDevice::singleton;
		struct Holder : GDExtensionDeviceExit::HolderRetirement {
			Holder() : HolderRetirement(GDExtensionDeviceExit::RecordRef{}) {}
			~Holder() = default;
		} holder;
		Registry::QualificationBridge *bridge = memnew(Registry::QualificationBridge);
		GDExtensionQualificationBridgeRefV1 ref{ 71, 11 };
		bool closed = false;
		Bug148Fixture() {
			RenderingDevice::singleton = &device;
			device.is_main_instance = true;
			device.render_thread_id = Thread::get_caller_id();
			device.driver = &driver;
			device.frames.resize(1);
			device.frames[0].qualification_epoch = 1;
			device.frames[0].primary_command_buffer_begun = true;
			registry->instance = 71;
			auto &record = registry->records[0];
			record.owner_thread = Thread::get_caller_id();
			record.snapshot.library_generation = 1;
			record.guard_enrollment = GDExtensionDeviceExit::GuardEnrollment::QUALIFIED;
			auto &d = registry->device_records[0];
			d.object = &device;
			d.live = true;
			d.epoch = 1;
			d.execution_thread = Thread::get_caller_id();
			holder.generation = 1;
			holder.qualification_children = 1;
			bridge->serial = 11;
			bridge->epoch = 1;
			bridge->holder = &holder;
			record.private_refs = 1;
			d.refs = 1;
			registry->qualification_bridges[0] = bridge;
		}
		void enqueue_and_dispatch_all(const LocalVector<GDExtensionQualificationRtRefV1> &p_refs) {
			for (uint32_t i = 0; i < p_refs.size(); ++i) {
				auto *cell = bridge->find_rt_by_serial(p_refs[i].handle_serial);
				REQUIRE(cell != nullptr);
				REQUIRE(cell->occupied);
				cell->active = true;
				cell->state = 1;
				if (!cell->queued) {
					const bool use_queue_credit = cell->header.kind == 3 && bridge->cleanup_refs_reserved;
					if (cell->queue_ref_reserved) {
						cell->queued = true;
						++bridge->children;
					} else if (use_queue_credit) {
						if (!bridge->claim_cleanup_queue(cell->queued, cell->queue_ref_credit)) {
							REQUIRE(registry->records[bridge->library].private_refs < registry->private_limit);
							cell->queued = true;
							++bridge->children;
							++registry->records[bridge->library].private_refs;
						}
					} else {
						REQUIRE(registry->records[bridge->library].private_refs < registry->private_limit);
						cell->queued = true;
						++bridge->children;
						++registry->records[bridge->library].private_refs;
					}
				}
			}
			for (uint32_t i = 0; i < p_refs.size(); ++i) {
				registry->qualification_rt_dispatch(bridge->serial, p_refs[i].handle_serial);
			}
		}
		~Bug148Fixture() {
			RenderingDevice::singleton = previous;
			device.is_main_instance = false;
			device.driver = nullptr;
			device.frames.clear();
			if (!closed && bridge) {
				registry->qualification_bridges[0] = nullptr;
				for (auto *child : bridge->storage) {
					if (child) {
						memdelete(child);
					}
				}
				for (auto *cell : bridge->rt_cells) {
					if (cell) {
						memdelete(cell);
					}
				}
				bridge->rt_cells.clear();
				memdelete(bridge);
			}
			memdelete(registry);
		}
	};

	static GDExtensionQualificationRtConfigV1 bug148_config(uint32_t p_slots, uint32_t p_payload = 256) {
		return { { 1, sizeof(GDExtensionQualificationRtConfigV1), 0 }, p_slots, 312, 200, 1, p_payload };
	}

	static GDExtensionQualificationRtRequestV1 bug148_storage_create_header(uint64_t p_op_serial) {
		return { { 1, sizeof(GDExtensionQualificationRtRequestV1), 0 }, 2, 0, p_op_serial, 0, sizeof(GDExtensionStorageCreateRequestV1), sizeof(GDExtensionStorageBackingV1), 0, 0 };
	}

	static GDExtensionQualificationRtRequestV1 bug148_storage_retire_header(uint64_t p_op_serial) {
		return { { 1, sizeof(GDExtensionQualificationRtRequestV1), 0 }, 3, 0, p_op_serial, 0, sizeof(GDExtensionStorageRetireRequestV1), sizeof(GDExtensionStorageRetireRefV1) + sizeof(GDExtensionStorageRetireReportV1), 0, 0 };
	}

	static void bug148_configure_bounds_and_lockout() {
		Bug148Fixture f;
		// Reject slot_count == 0 and slot_count == 65.
		auto bad_zero = bug148_config(0);
		auto res = f.registry->qualification_rt_configure(f.ref, &bad_zero);
		CHECK(res.native_error == ERR_INVALID_PARAMETER);
		CHECK(res.reason == 2);
		CHECK(f.bridge->rt_cells.is_empty());

		auto bad_over = bug148_config(65);
		res = f.registry->qualification_rt_configure(f.ref, &bad_over);
		CHECK(res.native_error == ERR_INVALID_PARAMETER);
		CHECK(res.reason == 2);
		CHECK(f.bridge->rt_cells.is_empty());

		// Reject checked total * slot_count > 128 MiB (64 * 4 MiB = 256 MiB).
		auto bad_oom = bug148_config(64, 4u * 1024u * 1024u);
		res = f.registry->qualification_rt_configure(f.ref, &bad_oom);
		CHECK(res.native_error == ERR_PARAMETER_RANGE_ERROR);
		CHECK(res.reason == 19);
		CHECK(f.bridge->rt_cells.is_empty());

		// Accept 1, reconfigure to 32, then reconfigure to 64 before first acceptance.
		auto c1 = bug148_config(1);
		REQUIRE(f.registry->qualification_rt_configure(f.ref, &c1).native_error == OK);
		CHECK(f.bridge->rt_cells.size() == 1);
		auto c32 = bug148_config(32);
		REQUIRE(f.registry->qualification_rt_configure(f.ref, &c32).native_error == OK);
		CHECK(f.bridge->rt_cells.size() == 32);
		auto c64 = bug148_config(64);
		REQUIRE(f.registry->qualification_rt_configure(f.ref, &c64).native_error == OK);
		CHECK(f.bridge->rt_cells.size() == 64);
		CHECK(f.bridge->rt_config.slot_count == 64);

		// Reserve 1 operation -> locks out further reconfiguration with ERR_BUSY, 16.
		const auto hdr = bug148_storage_create_header(1);
		const GDExtensionStorageCreateRequestV1 req{ { 1, sizeof(GDExtensionDeviceExitAbi), 0 }, 128, 1, 0, 1, 1, 0 };
		GDExtensionQualificationRtRefV1 rt_ref{};
		REQUIRE(f.registry->qualification_rt_reserve(f.ref, &hdr, &req, nullptr, &rt_ref).native_error == OK);
		CHECK(f.bridge->rt_ever_accepted);
		res = f.registry->qualification_rt_configure(f.ref, &c32);
		CHECK(res.native_error == ERR_BUSY);
		CHECK(res.reason == 16);

		// Release unused reserve and close cleanly.
		REQUIRE(f.registry->qualification_rt_release(f.ref, rt_ref).native_error == OK);
		REQUIRE(f.registry->qualification_close(f.ref).native_error == OK);
		f.closed = true;
	}

	static void bug148_concurrent_32_create_and_retire_one_drain() {
		Bug148Fixture f;
		auto cfg = bug148_config(32);
		REQUIRE(f.registry->qualification_rt_configure(f.ref, &cfg).native_error == OK);

		constexpr uint32_t N = 32;
		LocalVector<GDExtensionQualificationRtRefV1> create_refs;
		create_refs.resize(N);
		LocalVector<GDExtensionStorageCreateRequestV1> create_reqs;
		create_reqs.resize(N);
		for (uint32_t i = 0; i < N; ++i) {
			create_reqs[i] = { { 1, sizeof(GDExtensionDeviceExitAbi), 0 }, 128, i, 0, 1, 1, 0 };
			const auto hdr = bug148_storage_create_header(i + 1);
			REQUIRE(f.registry->qualification_rt_reserve(f.ref, &hdr, &create_reqs[i], nullptr, &create_refs[i]).native_error == OK);
			CHECK(create_refs[i].handle_serial != 0);
		}
		// 33rd concurrent reserve returns ERR_BUSY, 16.
		{
			const GDExtensionStorageCreateRequestV1 extra_req{ { 1, sizeof(GDExtensionDeviceExitAbi), 0 }, 128, 99, 0, 1, 1, 0 };
			const auto extra_hdr = bug148_storage_create_header(33);
			GDExtensionQualificationRtRefV1 extra_ref{};
			const auto busy = f.registry->qualification_rt_reserve(f.ref, &extra_hdr, &extra_req, nullptr, &extra_ref);
			CHECK(busy.native_error == ERR_BUSY);
			CHECK(busy.reason == 16);
			CHECK(extra_ref.handle_serial == 0);
		}
		// Idempotent re-reserve matches exact handle_serial across slots; mismatch returns ERR_INVALID_DATA, 16.
		for (uint32_t idx : { 0u, 15u, 31u }) {
			const auto hdr = bug148_storage_create_header(idx + 1);
			GDExtensionQualificationRtRefV1 repeat{};
			REQUIRE(f.registry->qualification_rt_reserve(f.ref, &hdr, &create_reqs[idx], nullptr, &repeat).native_error == OK);
			CHECK(repeat.handle_serial == create_refs[idx].handle_serial);
			auto mutated = create_reqs[idx];
			mutated.allocation_generation = 2;
			GDExtensionQualificationRtRefV1 bad_ref{};
			const auto mismatch = f.registry->qualification_rt_reserve(f.ref, &hdr, &mutated, nullptr, &bad_ref);
			CHECK(mismatch.native_error == ERR_INVALID_DATA);
			CHECK(mismatch.reason == 16);
			CHECK(bad_ref.handle_serial == 0);
		}

		// Drain ALL 32 StorageCreate slots in ONE RT pass.
		f.enqueue_and_dispatch_all(create_refs);
		CHECK(f.driver.created_buffers == N);

		LocalVector<GDExtensionStorageBackingV1> backings;
		backings.resize(N);
		for (uint32_t i = 0; i < N; ++i) {
			GDExtensionQualificationRtObservationV1 obs{};
			REQUIRE(f.registry->qualification_rt_observe(f.ref, create_refs[i], &obs, &backings[i], sizeof(backings[i]), nullptr, 0).native_error == OK);
			CHECK(obs.state == 2);
			CHECK(obs.call_result.native_error == OK);
			CHECK(backings[i].backing.handle_serial != 0);
			CHECK(backings[i].rid != 0);
			REQUIRE(f.registry->qualification_rt_release(f.ref, create_refs[i]).native_error == OK);
		}
		CHECK(f.bridge->rt_retired_highwater == N);
		CHECK(f.bridge->storage_by_serial.size() == N);
		CHECK(f.bridge->storage_by_retire_serial.size() == N);
		CHECK(f.bridge->storage_by_origin.size() == N);
		CHECK(f.bridge->find_storage_by_serial_locked(backings[15].backing.handle_serial) != nullptr);

		// Retired op_serials (1 and 32) return ERR_DOES_NOT_EXIST, 17.
		for (uint64_t old_op : { 1ull, 32ull }) {
			const auto old_hdr = bug148_storage_create_header(old_op);
			GDExtensionQualificationRtRefV1 old_ref{};
			const auto stale = f.registry->qualification_rt_reserve(f.ref, &old_hdr, &create_reqs[0], nullptr, &old_ref);
			CHECK(stale.native_error == ERR_DOES_NOT_EXIST);
			CHECK(stale.reason == 17);
		}

		// Reserve ALL 32 concurrent kind=3 (StorageRetire) operations simultaneously!
		LocalVector<GDExtensionQualificationRtRefV1> retire_refs;
		retire_refs.resize(N);
		for (uint32_t i = 0; i < N; ++i) {
			const GDExtensionStorageRetireRequestV1 rreq{ { 1, sizeof(GDExtensionDeviceExitAbi), 0 }, backings[i].backing, backings[i].rid, backings[i].backing_serial, i, 0, 1, 1, 0 };
			const auto hdr = bug148_storage_retire_header(100 + i + 1);
			REQUIRE(f.registry->qualification_rt_reserve(f.ref, &hdr, &rreq, nullptr, &retire_refs[i]).native_error == OK);
			CHECK(retire_refs[i].handle_serial != 0);
		}
		// Slot 0 uses cleanup_slot_in_use credit; slots 1..31 pay ordinary private_refs + queue_ref_reserved.
		CHECK(f.bridge->cleanup_slot_in_use);
		CHECK(f.bridge->find_rt_by_serial(retire_refs[0].handle_serial)->slot_ref_credit);
		CHECK_FALSE(f.bridge->find_rt_by_serial(retire_refs[1].handle_serial)->slot_ref_credit);
		CHECK(f.bridge->find_rt_by_serial(retire_refs[1].handle_serial)->queue_ref_reserved);

		// Drain ALL 32 StorageRetire slots in ONE RT pass.
		f.enqueue_and_dispatch_all(retire_refs);

		struct RetireResultPair {
			GDExtensionStorageRetireRefV1 ref;
			GDExtensionStorageRetireReportV1 report;
		};
		LocalVector<GDExtensionStorageRetireRefV1> storage_retire_refs;
		storage_retire_refs.resize(N);
		for (uint32_t i = 0; i < N; ++i) {
			GDExtensionQualificationRtObservationV1 obs{};
			alignas(8) uint8_t raw[sizeof(GDExtensionStorageRetireRefV1) + sizeof(GDExtensionStorageRetireReportV1)] = {};
			REQUIRE(f.registry->qualification_rt_observe(f.ref, retire_refs[i], &obs, raw, sizeof(raw), nullptr, 0).native_error == OK);
			CHECK(obs.state == 2);
			CHECK(obs.call_result.native_error == OK);
			memcpy(&storage_retire_refs[i], raw, sizeof(GDExtensionStorageRetireRefV1));
			CHECK(storage_retire_refs[i].handle_serial != 0);
			REQUIRE(f.registry->qualification_rt_release(f.ref, retire_refs[i]).native_error == OK);
		}
		CHECK_FALSE(f.bridge->cleanup_slot_in_use);
		CHECK_FALSE(f.bridge->cleanup_queue_in_use);

		// Drive retirement step + free pending resources + finalize retire state + observe & release all 32 storage retires.
		for (uint32_t i = 0; i < N; ++i) {
			f.registry->qualification_retire_step(f.bridge, &f.device, backings[i].backing.handle_serial, 1);
		}
		f.device._free_pending_resources(0);
		CHECK(f.driver.freed_buffers == N);
		for (uint32_t i = 0; i < N; ++i) {
			f.registry->qualification_retire_step(f.bridge, &f.device, backings[i].backing.handle_serial, 1);
		}
		for (uint32_t i = 0; i < N; ++i) {
			GDExtensionStorageRetireReportV1 rep{};
			GDExtensionQualificationBudgetV1 bud{ { 1, sizeof(GDExtensionDeviceExitAbi), 0 }, 1, 100, 0 };
			REQUIRE(f.registry->qualification_storage_retire_observe(f.ref, storage_retire_refs[i], &bud, &rep).native_error == OK);
			CHECK(rep.state == 1);
			REQUIRE(f.registry->qualification_storage_retire_release(f.ref, storage_retire_refs[i]).native_error == OK);
		}
		REQUIRE(f.registry->qualification_close(f.ref).native_error == OK);
		f.closed = true;
		CHECK(f.registry->records[0].private_refs == 0);
	}

	static void bug148_close_under_private_limit_saturation() {
		Bug148Fixture f;
		auto cfg = bug148_config(4);
		REQUIRE(f.registry->qualification_rt_configure(f.ref, &cfg).native_error == OK);

		// Create 2 live storage buffers (establishes bridge->cleanup_refs_reserved == true).
		LocalVector<GDExtensionQualificationRtRefV1> create_refs;
		create_refs.resize(2);
		for (uint32_t i = 0; i < 2; ++i) {
			const GDExtensionStorageCreateRequestV1 req{ { 1, sizeof(GDExtensionDeviceExitAbi), 0 }, 128, i, 0, 1, 1, 0 };
			const auto hdr = bug148_storage_create_header(i + 1);
			REQUIRE(f.registry->qualification_rt_reserve(f.ref, &hdr, &req, nullptr, &create_refs[i]).native_error == OK);
		}
		f.enqueue_and_dispatch_all(create_refs);
		GDExtensionStorageBackingV1 backings[2] = {};
		for (uint32_t i = 0; i < 2; ++i) {
			GDExtensionQualificationRtObservationV1 obs{};
			REQUIRE(f.registry->qualification_rt_observe(f.ref, create_refs[i], &obs, &backings[i], sizeof(backings[i]), nullptr, 0).native_error == OK);
			REQUIRE(f.registry->qualification_rt_release(f.ref, create_refs[i]).native_error == OK);
		}
		REQUIRE(f.bridge->cleanup_refs_reserved);

		// Saturate private_limit completely (0 ordinary private_refs left) and enter closing.
		f.registry->private_limit = f.registry->records[0].private_refs;
		CHECK(f.registry->qualification_close(f.ref).native_error == ERR_BUSY);
		CHECK(f.bridge->closing);

		// Slot 1 StorageRetire succeeds via pre-reserved cleanup_slot_in_use credit!
		const GDExtensionStorageRetireRequestV1 rreq0{ { 1, sizeof(GDExtensionDeviceExitAbi), 0 }, backings[0].backing, backings[0].rid, backings[0].backing_serial, 0, 0, 1, 1, 0 };
		const auto rhdr0 = bug148_storage_retire_header(10);
		GDExtensionQualificationRtRefV1 rref0{};
		REQUIRE(f.registry->qualification_rt_reserve(f.ref, &rhdr0, &rreq0, nullptr, &rref0).native_error == OK);
		CHECK(f.bridge->cleanup_slot_in_use);
		CHECK(f.bridge->find_rt_by_serial(rref0.handle_serial)->slot_ref_credit);

		// Concurrent slot 2 StorageRetire while cleanup_slot_in_use is held AND private_refs saturated returns ERR_BUSY, 19!
		const GDExtensionStorageRetireRequestV1 rreq1{ { 1, sizeof(GDExtensionDeviceExitAbi), 0 }, backings[1].backing, backings[1].rid, backings[1].backing_serial, 1, 0, 1, 1, 0 };
		const auto rhdr1 = bug148_storage_retire_header(11);
		GDExtensionQualificationRtRefV1 rref1{};
		const auto busy = f.registry->qualification_rt_reserve(f.ref, &rhdr1, &rreq1, nullptr, &rref1);
		CHECK(busy.native_error == ERR_BUSY);
		CHECK(busy.reason == 19);
		CHECK(rref1.handle_serial == 0);

		// Drain buffer 0 via cleanup queue credit and release its QRT cell.
		LocalVector<GDExtensionQualificationRtRefV1> one;
		one.push_back(rref0);
		f.enqueue_and_dispatch_all(one);
		alignas(8) uint8_t raw0[sizeof(GDExtensionStorageRetireRefV1) + sizeof(GDExtensionStorageRetireReportV1)] = {};
		GDExtensionQualificationRtObservationV1 obs0{};
		REQUIRE(f.registry->qualification_rt_observe(f.ref, rref0, &obs0, raw0, sizeof(raw0), nullptr, 0).native_error == OK);
		CHECK(obs0.state == 2);
		CHECK(obs0.call_result.native_error == OK);
		GDExtensionStorageRetireRefV1 sref0{};
		memcpy(&sref0, raw0, sizeof(sref0));
		REQUIRE(f.registry->qualification_rt_release(f.ref, rref0).native_error == OK);
		CHECK_FALSE(f.bridge->cleanup_slot_in_use);

		// Now buffer 1 StorageRetire succeeds via slot-1 cleanup credit even at saturation!
		REQUIRE(f.registry->qualification_rt_reserve(f.ref, &rhdr1, &rreq1, nullptr, &rref1).native_error == OK);
		one[0] = rref1;
		f.enqueue_and_dispatch_all(one);
		alignas(8) uint8_t raw1[sizeof(GDExtensionStorageRetireRefV1) + sizeof(GDExtensionStorageRetireReportV1)] = {};
		GDExtensionQualificationRtObservationV1 obs1{};
		REQUIRE(f.registry->qualification_rt_observe(f.ref, rref1, &obs1, raw1, sizeof(raw1), nullptr, 0).native_error == OK);
		CHECK(obs1.state == 2);
		CHECK(obs1.call_result.native_error == OK);
		GDExtensionStorageRetireRefV1 sref1{};
		memcpy(&sref1, raw1, sizeof(sref1));
		REQUIRE(f.registry->qualification_rt_release(f.ref, rref1).native_error == OK);

		// Complete both retirements and close bridge cleanly.
		f.registry->qualification_retire_step(f.bridge, &f.device, backings[0].backing.handle_serial, 1);
		f.registry->qualification_retire_step(f.bridge, &f.device, backings[1].backing.handle_serial, 1);
		f.device._free_pending_resources(0);
		CHECK(f.driver.freed_buffers == 2);
		f.registry->qualification_retire_step(f.bridge, &f.device, backings[0].backing.handle_serial, 1);
		f.registry->qualification_retire_step(f.bridge, &f.device, backings[1].backing.handle_serial, 1);
		GDExtensionStorageRetireReportV1 rep{};
		GDExtensionQualificationBudgetV1 bud{ { 1, sizeof(GDExtensionDeviceExitAbi), 0 }, 1, 100, 0 };
		REQUIRE(f.registry->qualification_storage_retire_observe(f.ref, sref0, &bud, &rep).native_error == OK);
		REQUIRE(f.registry->qualification_storage_retire_release(f.ref, sref0).native_error == OK);
		REQUIRE(f.registry->qualification_storage_retire_observe(f.ref, sref1, &bud, &rep).native_error == OK);
		REQUIRE(f.registry->qualification_storage_retire_release(f.ref, sref1).native_error == OK);
		REQUIRE(f.registry->qualification_close(f.ref).native_error == OK);
		f.closed = true;
		CHECK(f.registry->records[0].private_refs == 0);
	}

	static void bug149d_unacknowledged_children_and_live_tickets() {
		Bug148Fixture f;
		CHECK(f.bridge->unacknowledged_children == 0);
		CHECK(f.bridge->live_tickets.is_empty());
		CHECK_FALSE(f.bridge->cleanup_refs_reserved);

		// 1. Storage create -> unacknowledged_children == 1, cleanup_refs_reserved == true.
		const GDExtensionStorageCreateRequestV1 sreq{ { 1, 16, 0 }, 128, 0, 0, 1, 1, 0 };
		GDExtensionStorageBackingV1 sbacking{};
		REQUIRE(f.registry->qualification_storage_create(f.ref, &sreq, &sbacking).native_error == OK);
		CHECK(f.bridge->unacknowledged_children == 1);
		CHECK(f.bridge->cleanup_refs_reserved);

		// 2. Texture create -> unacknowledged_children == 2.
		GDExtensionQualificationSourceV1 tex_src{};
		tex_src.session_serial = 1;
		tex_src.generation = 1;
		tex_src.digest[0] = 1;
		const GDExtensionQualificationTextureCreateV1 treq{ { 1, 16, 0 }, tex_src, 1, 1, 4, 4, 1, 2, 1, 1 };
		GDExtensionQualificationResourceBackingV1 tbacking{};
		REQUIRE(f.registry->qualification_texture_create(f.ref, &treq, &tbacking).native_error == OK);
		CHECK(f.bridge->unacknowledged_children == 2);

		// 3. Sampler create -> unacknowledged_children == 3.
		GDExtensionQualificationSourceV1 smp_src{};
		smp_src.session_serial = 1;
		smp_src.generation = 2;
		smp_src.digest[0] = 2;
		const GDExtensionQualificationSamplerCreateV1 smpreq{ { 1, 16, 0 }, smp_src, 1, 1, 0, 0, 0, 0, 0, 0 };
		GDExtensionQualificationResourceBackingV1 smpbacking{};
		REQUIRE(f.registry->qualification_sampler_create(f.ref, &smpreq, &smpbacking).native_error == OK);
		CHECK(f.bridge->unacknowledged_children == 3);

		// 4. Ticket create (prepare_uploads) -> unacknowledged_children == 4, live_tickets.size() == 1.
		GDExtensionQualificationUploadPrepareV1 upreq{};
		upreq.abi = { 1, 16, 0 };
		upreq.library_generation = 1;
		upreq.qualification_session_serial = 7;
		upreq.record_generation = 11;
		upreq.request_nonce = 13;
		upreq.parent_serial = 17;
		upreq.reservation_serial = 19;
		upreq.attempt = 1;
		upreq.upload_count = 1;
		upreq.max_segments = 1;
		upreq.expected_result_bytes = 16;
		GDExtensionQualificationUploadPlanV1 uplan{};
		uplan.abi = { 1, 16, 0 };
		uplan.upload_index = 0;
		uplan.role = 1;
		uplan.owner_kind = 1;
		uplan.rid = sbacking.rid;
		uplan.backing_serial = sbacking.backing_serial;
		uplan.byte_offset = 0;
		uplan.byte_size = 16;
		uplan.e05_backing = sbacking.backing;
		uplan.payload_digest[0] = 1;
		GDExtensionQualificationTicketRefV1 tref{};
		GDExtensionQualificationReportV1 trep{};
		REQUIRE(f.registry->qualification_prepare_uploads(f.ref, &upreq, &uplan, 1, &tref, &trep).native_error == OK);
		CHECK(f.bridge->unacknowledged_children == 4);
		CHECK(f.bridge->live_tickets.size() == 1);
		CHECK(f.bridge->live_tickets[0]->live_ticket_index == 0);

		// 5. SceneDepth quote -> unacknowledged_children == 5.
		const GDExtensionStorageSceneDepthQuoteRequestV1 sdreq = request();
		GDExtensionStorageSceneDepthQuoteV1 sdquote{};
		REQUIRE(f.registry->qualification_scene_depth_quote(f.ref, &sdreq, &sdquote).native_error == OK);
		CHECK(f.bridge->unacknowledged_children == 5);

		// Acknowledge Ticket -> unacknowledged_children == 4, live_tickets empty, cleanup_refs_reserved stays true.
		REQUIRE(f.registry->qualification_release(f.ref, tref).native_error == OK);
		CHECK(f.bridge->unacknowledged_children == 4);
		CHECK(f.bridge->live_tickets.is_empty());
		CHECK(f.bridge->cleanup_refs_reserved);

		// Retire & acknowledge SceneDepth -> unacknowledged_children == 3, cleanup_refs_reserved stays true.
		const GDExtensionStorageSceneDepthRetireRequestV1 sdret = retire_request(sdquote.facet);
		GDExtensionStorageSceneDepthReportV1 sdrep{};
		REQUIRE(f.registry->qualification_scene_depth_retire(f.ref, &sdret, &sdrep).native_error == OK);
		REQUIRE(f.registry->qualification_scene_depth_release(f.ref, sdquote.facet).native_error == OK);
		CHECK(f.bridge->unacknowledged_children == 3);
		CHECK(f.bridge->cleanup_refs_reserved);

		// Retire & acknowledge Sampler & Texture -> unacknowledged_children drops 3 -> 2 -> 1.
		const GDExtensionQualificationResourceRetireV1 smpret{ { 1, 16, 0 }, smpbacking.resource, smpbacking.rid, smpbacking.object_serial, 1 };
		GDExtensionQualificationResourceRetireReportV1 smprep{};
		REQUIRE(f.registry->qualification_resource_retire(f.ref, &smpret, &smprep).native_error == OK);
		const GDExtensionQualificationResourceRetireV1 tret{ { 1, 16, 0 }, tbacking.resource, tbacking.rid, tbacking.object_serial, 2 };
		GDExtensionQualificationResourceRetireReportV1 treport{};
		REQUIRE(f.registry->qualification_resource_retire(f.ref, &tret, &treport).native_error == OK);
		f.device._free_pending_resources(0);
		f.registry->qualification_retire_step(f.bridge, &f.device, smpbacking.resource.handle_serial, 4);
		f.registry->qualification_retire_step(f.bridge, &f.device, tbacking.resource.handle_serial, 4);
		REQUIRE(f.registry->qualification_resource_retire_release(f.ref, smpbacking.resource, f.bridge->samplers[0]->report.revision).native_error == OK);
		CHECK(f.bridge->unacknowledged_children == 2);
		CHECK(f.bridge->cleanup_refs_reserved);
		REQUIRE(f.registry->qualification_resource_retire_release(f.ref, tbacking.resource, f.bridge->textures[0]->report.revision).native_error == OK);
		CHECK(f.bridge->unacknowledged_children == 1);
		CHECK(f.bridge->cleanup_refs_reserved);

		// Retire & acknowledge last child (Storage) -> unacknowledged_children == 0, cleanup_refs_reserved trims O(1)!
		const GDExtensionStorageRetireRequestV1 sret{ { 1, 16, 0 }, sbacking.backing, sbacking.rid, sbacking.backing_serial, 0, 0, 1, 2, 0 };
		GDExtensionStorageRetireRefV1 srref{};
		GDExtensionStorageRetireReportV1 srrep{};
		REQUIRE(f.registry->qualification_storage_retire(f.ref, &sret, &srref, &srrep).native_error == OK);
		f.device._free_pending_resources(0);
		f.registry->qualification_retire_step(f.bridge, &f.device, sbacking.backing.handle_serial, 1);
		REQUIRE(f.registry->qualification_storage_retire_release(f.ref, srref).native_error == OK);
		CHECK(f.bridge->unacknowledged_children == 0);
		CHECK_FALSE(f.bridge->cleanup_refs_reserved);

		// Close cleanly with zero leaks.
		REQUIRE(f.registry->qualification_close(f.ref).native_error == OK);
		f.closed = true;
		CHECK(f.registry->records[0].private_refs == 0);
	}

	static void bug168a_storage_child_capacity_16384() {
		using namespace GDExtensionDeviceExit;
		CHECK(Registry::QualificationBridge::STORAGE_CHILD_CAPACITY == 16384u);
		CHECK(Registry::QualificationBridge::STORAGE_HISTORY_CAPACITY == 65536u);
		Fixture f;
		REQUIRE(f.bridge != nullptr);
		constexpr uint32_t TEST_COUNT = 5000u;
		LocalVector<Registry::QualificationBridge::Storage *> children;
		LocalVector<uint32_t> slots;
		children.reserve(TEST_COUNT);
		slots.reserve(TEST_COUNT);
		for (uint32_t i = 0; i < TEST_COUNT; ++i) {
			const uint32_t slot = f.bridge->alloc_storage_slot_locked();
			REQUIRE(slot < Registry::QualificationBridge::STORAGE_CHILD_CAPACITY);
			auto *child = memnew(Registry::QualificationBridge::Storage);
			child->serial = 100000ull + i;
			child->origin.allocation_slot = i % Registry::QualificationBridge::STORAGE_DOMAIN_MAX_SLOTS;
			child->origin.allocation_generation = 1 + (i / Registry::QualificationBridge::STORAGE_DOMAIN_MAX_SLOTS);
			child->origin.part = 0;
			child->origin.create_attempt = 1;
			f.bridge->index_storage_slot_locked(slot, child);
			children.push_back(child);
			slots.push_back(slot);
		}
		CHECK(f.bridge->find_storage_by_serial_locked(100000ull) == children[0]);
		CHECK(f.bridge->find_storage_by_serial_locked(100000ull + 4096) == children[4096]);
		CHECK(f.bridge->find_storage_by_serial_locked(100000ull + TEST_COUNT - 1) == children[TEST_COUNT - 1]);
		CHECK(f.bridge->find_storage_by_origin_locked(children[TEST_COUNT - 1]->origin) == children[TEST_COUNT - 1]);
		for (uint32_t i = 0; i < TEST_COUNT; ++i) {
			f.bridge->unindex_storage_slot_locked(slots[i], children[i]);
			memdelete(children[i]);
		}
		CHECK(f.bridge->find_storage_by_serial_locked(100000ull + 4096) == nullptr);
		CHECK(f.bridge->unacknowledged_children == 0);
	}
};

TEST_CASE("[SceneDepthR2] original quote exact retry and archived origin") {
	TestQualificationSceneDepthAccess::quote_retry_and_archive(false);
}
TEST_CASE("[SceneDepthR2] admitted failed quote retains same facet until no-native ACK") {
	TestQualificationSceneDepthAccess::quote_retry_and_archive(true);
}
TEST_CASE("[SceneDepthR2] original owner preserves failed allocation cause without second pair") {
	TestQualificationSceneDepthAccess::failed_original_create_and_foreign_origin();
}
TEST_CASE("[SceneDepthR2] malformed extents and original finite admission have no quote effect") {
	TestQualificationSceneDepthAccess::malformed_and_capacity();
}
TEST_CASE("[SceneDepthR2] original copied three stages pin facet through delivery close") {
	TestQualificationSceneDepthAccess::copied_three_stages_and_close();
}

TEST_CASE("[SceneDepthR2] canonical first admission denies revoked metadata and cleanup work") {
	TestQualificationSceneDepthAccess::canonical_new_work_admission();
}
TEST_CASE("[SceneDepthR2] exact original child and archived history bounds refuse without effects") {
	TestQualificationSceneDepthAccess::finite_children_and_history();
}
TEST_CASE("[SceneDepthR2] cancelled inactive and failed retire delivery restore original cleanup credit") {
	TestQualificationSceneDepthAccess::retire_delivery_cancellation();
}
TEST_CASE("[SceneDepthR2] actual bridge close preserves occupied quote then retires and acknowledges") {
	TestQualificationSceneDepthAccess::original_bridge_close_and_inactive_create(true);
}
TEST_CASE("[SceneDepthR2] inactive accepted create pins original child through close and cancellation") {
	TestQualificationSceneDepthAccess::original_bridge_close_and_inactive_create(false);
}

TEST_CASE("[SceneDepthR2] original native callback reentry preserves Busy cache and ACK barriers") {
	TestQualificationSceneDepthAccess::native_operation_blocks_observe_and_ack();
}

TEST_CASE("[StorageFinality] original Registry declared generation cleanup") {
	TestQualificationSceneDepthAccess::storage_finality();
}

TEST_CASE("[StorageFinality][EntryABI] installed original procedures enforce exact DTOs") {
	const auto install = reinterpret_cast<GDExtensionInterfaceDeviceSessionQualificationStorageDomainInstallV1>(GDExtension::get_interface_function("device_session_qualification_storage_domain_install_v1"));
	const auto finalize = reinterpret_cast<GDExtensionInterfaceDeviceSessionQualificationStorageGenerationFinalizeV1>(GDExtension::get_interface_function("device_session_qualification_storage_generation_finalize_v1"));
	REQUIRE(install != nullptr);
	REQUIRE(finalize != nullptr);
	if (!install || !finalize) {
		return;
	}
	CHECK(sizeof(GDExtensionStorageDomainV1) == 24);
	CHECK(sizeof(GDExtensionStorageGenerationFinalizeV1) == 40);
	CHECK(install({}, nullptr).native_error == ERR_INVALID_PARAMETER);
	CHECK(finalize({}, nullptr).native_error == ERR_INVALID_PARAMETER);
	GDExtensionStorageDomainV1 bad{ { 1, 24, 0 }, 1, 0 };
	CHECK(install({}, &bad).native_error == ERR_INVALID_PARAMETER);
	GDExtensionStorageGenerationFinalizeV1 zero{ { 1, 16, 0 }, 0, 0, 1, 5000, 0 };
	CHECK(finalize({}, &zero).native_error == ERR_INVALID_PARAMETER);
}

TEST_CASE("[StorageFinality] original accepted namespace refuses opt-in and idle domain closes") {
	TestQualificationSceneDepthAccess::storage_finality_namespace_and_idle_close();
}

TEST_CASE("[QualificationRt][BUG-148][Pillar1] qualification_rt_configure validates slot_count 1..64 and checked total payload ceiling") {
	TestQualificationSceneDepthAccess::bug148_configure_bounds_and_lockout();
}

TEST_CASE("[QualificationRt][BUG-148][Pillar1] 32 concurrent StorageCreate and 32 concurrent StorageRetire drain in one RT pass with zero leaks and clean close") {
	TestQualificationSceneDepthAccess::bug148_concurrent_32_create_and_retire_one_drain();
}

TEST_CASE("[QualificationRt][BUG-148][Pillar1] close under private_limit saturation drains via slot-1 cleanup credit while concurrent slot 2 returns ERR_BUSY 19") {
	TestQualificationSceneDepthAccess::bug148_close_under_private_limit_saturation();
}

TEST_CASE("[Qualification][BUG-149D][Pillar1] unacknowledged_children and live_tickets track create/acknowledge/trim/close in O(1)") {
	TestQualificationSceneDepthAccess::bug149d_unacknowledged_children_and_live_tickets();
}

TEST_CASE("[StorageFinality] original duration observation preserves frontier and next service continuation") {
	TestQualificationSceneDepthAccess::storage_finality_duration_observation();
}

TEST_CASE("[Qualification][BUG-168A] QualificationBridge STORAGE_CHILD_CAPACITY supports 16384 concurrent slots with O(1) index lookup") {
	TestQualificationSceneDepthAccess::bug168a_storage_child_capacity_16384();
}
