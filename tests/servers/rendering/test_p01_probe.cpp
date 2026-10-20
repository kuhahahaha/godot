/**************************************************************************/
/*  test_p01_probe.cpp                                          */
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

#include "tests/test_macros.h"
#include "modules/modules_enabled.gen.h"

TEST_FORCE_LINK(test_p01_probe)

#ifdef MODULE_P01_PROBE_ENABLED
#include "modules/p01_probe/p01_observer.h"

namespace TestP01Probe {
TEST_CASE("[P01] Lifetime") {
	// A recorded prefix remains owned after capture failure until its actual
	// readback and native references complete; parent verdict is irrelevant.
	P01CollectorLifetime lifetime;
	CHECK(lifetime.accept(7));
	CHECK(lifetime.record(7));
	lifetime.close();
	lifetime.poison_capture();
	CHECK_FALSE(lifetime.can_retire());
	CHECK_FALSE(lifetime.complete_readback(8));
	CHECK_FALSE(lifetime.can_retire());
	CHECK(lifetime.complete_readback(7));
	CHECK_FALSE(lifetime.can_retire());
	lifetime.release_native_references();
	CHECK(lifetime.can_retire());
	CHECK_FALSE(lifetime.accept(9));

	P01CollectorLifetime slots;
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

	P01CollectorLifetime no_work;
	no_work.close();
	no_work.release_native_references();
	CHECK_FALSE(no_work.can_retire());
	CHECK_FALSE(no_work.accept(1));
	CHECK_FALSE(no_work.can_retire());

	P01AllocationIdentity identity;
	const uint64_t first = identity.acquire(4, 77);
	CHECK(first != 0);
	CHECK(identity.resolve(4, 77) == first);
	CHECK(identity.resolve(9, 77) == 0);
	identity.release(4, 77);
	CHECK(identity.resolve(4, 77) == 0);
	const uint64_t reused = identity.acquire(4, 77);
	CHECK(reused != first);
	CHECK(identity.resolve(4, 77) == reused);

	P01WritePrefix prefix;
	CHECK(prefix.begin(4, 77));
	CHECK(prefix.record(0, 16));
	CHECK_FALSE(prefix.record_failure(16));
	CHECK(prefix.recorded_bytes() == 16);
	CHECK(prefix.has_failed_prefix());
	CHECK_FALSE(prefix.record(16, 16));
}
} // namespace TestP01Probe
#endif
