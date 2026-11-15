/**************************************************************************/
/*  test_render_capture_lifetime.cpp                                      */
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

#include "servers/rendering/render_capture_test_hooks.h"
#include "tests/test_macros.h"

#include "modules/modules_enabled.gen.h"

TEST_FORCE_LINK(test_render_capture_lifetime)

#if defined(TESTS_ENABLED) && defined(MODULE_SKINNED_MESH_CAPTURE_TESTS_ENABLED)
#include "modules/skinned_mesh_capture_tests/skinned_mesh_capture_test_observer.h"

namespace TestSkinnedMeshCaptureTestProbe {
TEST_CASE("[P01] Lifetime") {
	// A recorded prefix remains owned after capture failure until its actual
	// readback and native references complete; parent verdict is irrelevant.
	SkinnedMeshCaptureTestCollectorLifetime lifetime;
	CHECK(lifetime.accept(7));
	const uint64_t lifetime_token = lifetime.record_token(7);
	CHECK(lifetime_token != 0);
	lifetime.close();
	lifetime.poison_capture();
	CHECK_FALSE(lifetime.can_retire());
	CHECK_FALSE(lifetime.complete_readback(8));
	CHECK_FALSE(lifetime.can_retire());
	CHECK(lifetime.complete_readback_token(7, lifetime_token));
	CHECK_FALSE(lifetime.can_retire());
	lifetime.release_native_references();
	CHECK(lifetime.can_retire());
	CHECK_FALSE(lifetime.accept(9));

	SkinnedMeshCaptureTestCollectorLifetime slots;
	CHECK(slots.accept(11));
	CHECK(slots.record_slot(11, 3));
	CHECK(slots.record_slot(11, 9));
	CHECK_FALSE(slots.record_slot(11, 9));
	CHECK_FALSE(slots.complete_slot(11, 8));
	CHECK(slots.complete_slot(11, 3));
	CHECK_FALSE(slots.can_retire());
	CHECK(slots.complete_slot(11, 9));
	CHECK_FALSE(slots.can_retire());
	slots.close();
	CHECK_FALSE(slots.can_retire());
	slots.release_native_references();
	CHECK_FALSE(slots.complete_slot(11, 9));
	CHECK_FALSE(slots.complete_slot(10, 3));
	CHECK_FALSE(slots.record_slot(11, 64));
	CHECK(slots.can_retire());

	SkinnedMeshCaptureTestCollectorLifetime no_work;
	no_work.close();
	no_work.release_native_references();
	CHECK_FALSE(no_work.can_retire());
	CHECK_FALSE(no_work.accept(1));
	CHECK_FALSE(no_work.can_retire());

	SkinnedMeshCaptureTestAllocationIdentity identity;
	const uint64_t first = identity.acquire(4, 77);
	CHECK(first != 0);
	CHECK(identity.resolve(4, 77) == first);
	CHECK(identity.resolve(9, 77) == 0);
	identity.release(4, 77);
	CHECK(identity.resolve(4, 77) == 0);
	const uint64_t reused = identity.acquire(4, 77);
	CHECK(reused != first);
	CHECK(identity.resolve(4, 77) == reused);
	const uint64_t alias = identity.acquire_alias(4, 77);
	CHECK(alias == reused);
	CHECK(identity.release_alias(4, 77));
	CHECK(identity.resolve(4, 77) == reused);
	CHECK(identity.release_alias(4, 77));
	CHECK(identity.resolve(4, 77) == 0);

	SkinnedMeshCaptureTestWritePrefix prefix;
	CHECK(prefix.begin(4, 77));
	CHECK(prefix.record(0, 16));
	CHECK_FALSE(prefix.record_failure(16));
	CHECK(prefix.recorded_bytes() == 16);
	CHECK(prefix.has_failed_prefix());
	CHECK_FALSE(prefix.record(16, 16));

	SkinnedMeshCaptureTestCollectorLifetime tokens;
	CHECK(tokens.accept(30));
	const uint64_t token = tokens.record_token(30);
	CHECK(token != 0);
	CHECK_FALSE(tokens.complete_readback_token(30, token + 1));
	CHECK(tokens.complete_readback_token(30, token));
	CHECK_FALSE(tokens.complete_readback_token(30, token));
	const uint64_t slot_token = tokens.record_slot_token(30, 2);
	CHECK(slot_token != 0);
	CHECK_FALSE(tokens.complete_slot_token(30, 2, slot_token + 1));
	CHECK(tokens.complete_slot_token(30, 2, slot_token));

	SkinnedMeshCaptureTestCollectorLifetime failed;
	CHECK(failed.accept(22));
	const uint64_t failed_token = failed.record_token(22);
	CHECK(failed_token != 0);
	failed.device_failure();
	CHECK(failed.is_capture_poisoned());
	CHECK_FALSE(failed.record(22));
	failed.close();
	CHECK_FALSE(failed.can_retire());
	CHECK(failed.complete_readback_token(22, failed_token));
	failed.release_native_references();
	CHECK(failed.can_retire());

	skinned_mesh_capture_test_hook_reset();
	skinned_mesh_capture_test_hook_begin(42, 99);
	RenderCaptureTestHooks::observe_buffer_write(5, 0, 16, true);
	RenderCaptureTestHooks::observe_buffer_write(5, 16, 16, false);
	RenderCaptureTestHooks::observe_readback_queued(5, 32);
	RenderCaptureTestHooks::observe_readback_completed(5, 32);
	const SkinnedMeshCaptureTestHookSnapshot hook = skinned_mesh_capture_test_hook_snapshot();
	CHECK(hook.write_begin_count == 2);
	CHECK(hook.write_bytes == 16);
	CHECK(hook.write_failure_count == 1);
	CHECK(hook.readback_queued_count == 1);
	CHECK(hook.readback_completed_count == 1);
	const std::vector<SkinnedMeshCaptureTestHookEvent> events = skinned_mesh_capture_test_hook_events();
	REQUIRE(events.size() == 4);
	CHECK(events.front().epoch == 42);
	CHECK(events.front().allocation_serial == 0);
	CHECK(events.front().session_serial_hint == 99);
	CHECK(events.front().sequence == 1);
}
} //namespace TestSkinnedMeshCaptureTestProbe
#endif
