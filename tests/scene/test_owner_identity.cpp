#include "tests/test_macros.h"

TEST_FORCE_LINK(test_owner_identity)

#include "core/object/ref_counted_test_hooks.h"
#include "core/os/memory_allocation_test_hooks.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#ifndef _3D_DISABLED
#include "scene/resources/3d/world_3d.h"
#endif
#include "servers/audio/audio_server.h"
#include "servers/audio/effects/audio_effect_spectrum_analyzer.h"

#include <thread>

namespace TestOwnerIdentity {

struct ObservationScope {
	uint64_t reference_events = 0;
	uint64_t allocations = 0;

	ObservationScope() {
		RefCountedReleaseTest::channel = { [](void *context, RefCountedReleaseTest::Phase, const RefCountedReleaseTest::Observation &) {
			static_cast<ObservationScope *>(context)->reference_events++;
		}, this };
		MemoryAllocationTest::attempts = 0;
		MemoryAllocationTest::enabled = true;
	}
	void stop() {
		allocations = MemoryAllocationTest::attempts;
		MemoryAllocationTest::enabled = false;
		RefCountedReleaseTest::channel = {};
	}
	~ObservationScope() { stop(); }
};

#ifndef _3D_DISABLED
TEST_CASE("[SceneTree][OwnerIdentity] viewport observes its assigned world without retaining it") {
	Window *root = SceneTree::get_singleton()->get_root();
	SubViewport *viewport = memnew(SubViewport);
	root->add_child(viewport);
	CHECK(viewport->get_world_3d_instance_id().is_null());
	CHECK(viewport->find_world_3d().is_valid());
	Ref<World3D> world;
	world.instantiate();
	viewport->set_world_3d(world);
	const ObjectID expected = world->get_instance_id();
	const int references = world->get_reference_count();
	ObjectID observed;
	ObservationScope scope;
	for (int iteration = 0; iteration < 16; iteration++) {
		observed = viewport->get_world_3d_instance_id();
	}
	scope.stop();
	CHECK(observed == expected);
	CHECK(scope.reference_events == 0);
	CHECK(scope.allocations == 0);
	CHECK(world->get_reference_count() == references);
	ObservationScope ref_control;
	{ Ref<World3D> retained = viewport->get_world_3d(); }
	ref_control.stop();
	CHECK(ref_control.reference_events > 0);
	CHECK(uint64_t(int64_t(viewport->call(SNAME("get_world_3d_instance_id")))) == uint64_t(expected));
#ifdef DEBUG_ENABLED
	ObjectID foreign_result = expected;
	ERR_PRINT_OFF;
	std::thread worker([&]() { foreign_result = viewport->get_world_3d_instance_id(); });
	worker.join();
	ERR_PRINT_ON;
	CHECK(foreign_result.is_null());
#endif
	root->remove_child(viewport);
	viewport->set_use_own_world_3d(true);
	CHECK(viewport->get_world_3d_instance_id() == expected);
	CHECK(viewport->find_world_3d()->get_instance_id() != expected);
	viewport->set_world_3d(Ref<World3D>());
	CHECK(viewport->get_world_3d_instance_id().is_null());
	memdelete(viewport);
}
#endif

TEST_CASE("[Audio][OwnerIdentity] bus effect observation does not retain its resource") {
	AudioServer *server = AudioServer::get_singleton();
	REQUIRE(server != nullptr);
	Ref<AudioEffectSpectrumAnalyzer> effect;
	effect.instantiate();
	const int effect_index = server->get_bus_effect_count(0);
	server->add_bus_effect(0, effect);
	const ObjectID expected = effect->get_instance_id();
	const int references = effect->get_reference_count();
	ObjectID observed;
	ObservationScope scope;
	for (int iteration = 0; iteration < 16; iteration++) {
		observed = server->get_bus_effect_instance_id(0, effect_index);
	}
	scope.stop();
	CHECK(observed == expected);
	CHECK(scope.reference_events == 0);
	CHECK(scope.allocations == 0);
	CHECK(effect->get_reference_count() == references);
	ObservationScope ref_control;
	{ Ref<AudioEffect> retained = server->get_bus_effect(0, effect_index); }
	ref_control.stop();
	CHECK(ref_control.reference_events > 0);
	CHECK(uint64_t(int64_t(server->call(SNAME("get_bus_effect_instance_id"), 0, effect_index))) == uint64_t(expected));
	ERR_PRINT_OFF;
	CHECK(server->get_bus_effect_instance_id(-1, 0).is_null());
	CHECK(server->get_bus_effect_instance_id(server->get_bus_count(), 0).is_null());
	CHECK(server->get_bus_effect_instance_id(0, -1).is_null());
	CHECK(server->get_bus_effect_instance_id(0, server->get_bus_effect_count(0)).is_null());
	ERR_PRINT_ON;
	server->remove_bus_effect(0, effect_index);
	ERR_PRINT_OFF;
	CHECK(server->get_bus_effect_instance_id(0, effect_index).is_null());
	ERR_PRINT_ON;
}

}
