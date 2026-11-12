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
			auto *cell = bridge->rt;
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
				if (bridge->rt) {
					memdelete(bridge->rt);
				}
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
			const uint32_t limit = history ? Registry::QualificationBridge::STORAGE_HISTORY_CAPACITY : Registry::QualificationBridge::CHILD_CAPACITY;
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
