#pragma once
#include "modules/p01_probe/p01_native_source.h"
#include "modules/p01_probe/p01_observer.h"
#include "tests/test_macros.h"
TEST_CASE("[P01] Draw observation rejects missing RD and never fabricates a selected role") {
	CHECK(P01NativeSource::draw_record_count() == 0);
	CHECK_FALSE(P01NativeSource::observe_draw(RID(), 0, RID(), 0, 0, 0));
	CHECK(P01NativeSource::draw_observations().empty());
	P01RDPool pool;
	P01NativeSourceObservation observation;
	CHECK(P01NativeSource::capture_selected(RID(), RID(), RID(), 1, true, pool, 0, observation) == ERR_INVALID_DATA);
}

class P01DrawSelectionTestAccess {
public:
 static void check_selection() {
  P01DrawObservation target;
  target.epoch = 1; target.frame = 2; target.viewport = 3; target.mesh_instance = 4; target.sequence = 5;
  auto neighbor = target; neighbor.mesh_instance = 6; neighbor.sequence = 7;
  uint64_t selected = 0;
  CHECK(P01NativeSource::select_matching_draw({target, neighbor}, 4, 1, 2, 3, 0, selected) == OK);
  CHECK(selected == 5);
  auto second = target; second.sequence = 8;
  CHECK(P01NativeSource::select_matching_draw({target, neighbor, second}, 4, 1, 2, 3, 0, selected) == ERR_INVALID_DATA);
  CHECK(P01NativeSource::select_matching_draw({target, neighbor, second}, 4, 1, 2, 3, 8, selected) == OK);
  CHECK(selected == 8);
  CHECK(P01NativeSource::select_matching_draw({neighbor}, 4, 1, 2, 3, 0, selected) == ERR_INVALID_DATA);
  CHECK(P01NativeSource::select_matching_draw({target}, 4, 1, 2, 99, 0, selected) == ERR_INVALID_DATA);
 }
};
TEST_CASE("[P01] Viewport selection ignores unrelated meshes but rejects ambiguous target draws") {
 P01DrawSelectionTestAccess::check_selection();
}

#include <thread>
TEST_CASE("[P01] Exact recording scopes exclude foreign and nested publishers") {
 p01_hook_reset(); p01_hook_begin(6000);
 uint64_t first = 0;
 {
  P01RecordingScope operation; first = operation.identity(); REQUIRE(first != 0);
  std::thread other([] { p01_observe_buffer_copy(1, 2, 0, 0, 36, 3, 4, 5); }); other.join();
  CHECK(p01_hook_events().back().recording_token == 0);
  { P01RecordingScope nested; p01_observe_buffer_copy(1, 2, 0, 0, 36, 3, 4, 5);
    CHECK(p01_hook_events().back().recording_token == nested.identity()); CHECK(nested.identity() != first); }
  const auto marker = p01_observe_draw_record(3, 4, 5);
  p01_observe_buffer_copy(1, 2, 0, 0, 36, 3, 4, 5);
  CHECK(marker.kind == 16); CHECK(marker.sequence == 3); CHECK(marker.epoch == 6000);
  CHECK(p01_hook_events().back().recording_token == first);
  CHECK(marker.sequence != p01_hook_events().back().sequence);
 }
 p01_observe_buffer_copy(1, 2, 0, 0, 36, 3, 4, 5);
 CHECK(p01_hook_events().back().recording_token == 0);
 p01_hook_reset();
}
