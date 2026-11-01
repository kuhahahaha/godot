/**************************************************************************/
/*  test_wrapper_stop.cpp                                                 */
/**************************************************************************/

#include "tests/test_macros.h"

TEST_FORCE_LINK(test_wrapper_stop)

#ifndef PHYSICS_3D_DISABLED

#include "servers/physics_3d/physics_server_3d_dummy.h"
#include "servers/physics_3d/physics_server_3d_wrap_mt.h"

#include <atomic>

namespace TestPhysicsWrapperStop {

// The production wrapper, queue and WorkerThreadPool remain real. Only backend
// work is recorded: these cases qualify wrapper ordering, not a physics solver.
// No SceneTree tag: creating a second live server would replace its singleton.
class RecordingBackend : public PhysicsServer3DDummy {
public:
	enum RequestFrom { NONE,
		STEP,
		SET_ACTIVE,
		QUERY };
	PhysicsServer3DWrapMT *wrapper = nullptr;
	RequestFrom request_from = NONE;
	std::atomic<int> calls{ 0 };
	std::atomic<int> owner_depth{ 0 };
	std::atomic<int> finish_count{ 0 };
	std::atomic<int> end_sync_count{ 0 };
	std::atomic<bool> finished_in_owner{ false };
	std::atomic<bool> request_returned_before_finish{ false };
	Thread::ID init_thread = Thread::UNASSIGNED_ID;
	Thread::ID finish_thread = Thread::UNASSIGNED_ID;

	void request(RequestFrom p_from) {
		owner_depth.fetch_add(1);
		if (request_from == p_from) {
			request_from = NONE;
			wrapper->finish();
			wrapper->finish();
			request_returned_before_finish.store(finish_count.load() == 0);
		}
		owner_depth.fetch_sub(1);
	}
	void init() override {
		calls.fetch_add(1);
		init_thread = Thread::get_caller_id();
	}
	void step(real_t p_step) override {
		calls.fetch_add(1);
		request(STEP);
	}
	void set_active(bool p_active) override {
		calls.fetch_add(1);
		request(SET_ACTIVE);
	}
	void sync() override { calls.fetch_add(1); }
	void flush_queries() override {
		calls.fetch_add(1);
		request(QUERY);
	}
	void end_sync() override {
		calls.fetch_add(1);
		end_sync_count.fetch_add(1);
	}
	void finish() override {
		calls.fetch_add(1);
		finish_count.fetch_add(1);
		finished_in_owner.store(finished_in_owner.load() || owner_depth.load() != 0);
		finish_thread = Thread::get_caller_id();
	}
	RID sphere_shape_create() override {
		calls.fetch_add(1);
		return RID::from_uint64(0xBAD);
	}
	ShapeType shape_get_type(RID p_shape) const override {
		const_cast<RecordingBackend *>(this)->calls.fetch_add(1);
		return SHAPE_SPHERE;
	}
	JointType joint_get_type(RID p_joint) const override {
		const_cast<RecordingBackend *>(this)->calls.fetch_add(1);
		return JOINT_TYPE_HINGE;
	}
	bool joint_is_disabled_collisions_between_bodies(RID p_joint) const override {
		const_cast<RecordingBackend *>(this)->calls.fetch_add(1);
		return false;
	}
	PhysicsDirectSpaceState3D *space_get_direct_state(RID p_space) override {
		calls.fetch_add(1);
		return nullptr;
	}
	Vector<Vector3> space_get_contacts(RID p_space) const override {
		const_cast<RecordingBackend *>(this)->calls.fetch_add(1);
		Vector<Vector3> result;
		result.push_back(Vector3(91, 92, 93));
		return result;
	}
	int space_get_contact_count(RID p_space) const override {
		const_cast<RecordingBackend *>(this)->calls.fetch_add(1);
		return 91;
	}
	bool body_test_motion(RID p_body, const MotionParameters &p_parameters, MotionResult *r_result = nullptr) override {
		calls.fetch_add(1);
		if (r_result) {
			r_result->travel = Vector3(91, 92, 93);
		}
		return true;
	}
	PhysicsDirectBodyState3D *body_get_direct_state(RID p_body) override {
		calls.fetch_add(1);
		return nullptr;
	}
	bool is_flushing_queries() const override {
		const_cast<RecordingBackend *>(this)->calls.fetch_add(1);
		return true;
	}
	int get_process_info(ProcessInfo p_info) override {
		calls.fetch_add(1);
		return 91;
	}
};

TEST_CASE("[PhysicsWrapperStop] inline normal-owner finish waits for owner return") {
	REQUIRE(PhysicsServer3D::get_singleton() == nullptr);
	auto *backend = memnew(RecordingBackend);
	auto *wrapper = memnew(PhysicsServer3DWrapMT(backend, false));
	backend->wrapper = wrapper;
	wrapper->init();
	backend->request_from = RecordingBackend::STEP;
	wrapper->step(1.0 / 60.0);
	CHECK(backend->request_returned_before_finish.load());
	CHECK_FALSE(backend->finished_in_owner.load());
	CHECK(backend->finish_count.load() == 1);
	CHECK(backend->end_sync_count.load() == 0);
	wrapper->finish();
	CHECK(backend->finish_count.load() == 1);
	memdelete(wrapper);
}

TEST_CASE("[PhysicsWrapperStop] inline generated-owner finish waits for owner return") {
	REQUIRE(PhysicsServer3D::get_singleton() == nullptr);
	auto *backend = memnew(RecordingBackend);
	auto *wrapper = memnew(PhysicsServer3DWrapMT(backend, false));
	backend->wrapper = wrapper;
	wrapper->init();
	backend->request_from = RecordingBackend::SET_ACTIVE;
	wrapper->set_active(true);
	CHECK(backend->request_returned_before_finish.load());
	CHECK_FALSE(backend->finished_in_owner.load());
	CHECK(backend->finish_count.load() == 1);
	memdelete(wrapper);
}

TEST_CASE("[PhysicsWrapperStop] inline query finish ends active sync once after traversal") {
	REQUIRE(PhysicsServer3D::get_singleton() == nullptr);
	auto *backend = memnew(RecordingBackend);
	auto *wrapper = memnew(PhysicsServer3DWrapMT(backend, false));
	backend->wrapper = wrapper;
	wrapper->init();
	wrapper->sync();
	backend->request_from = RecordingBackend::QUERY;
	wrapper->flush_queries();
	CHECK(backend->request_returned_before_finish.load());
	CHECK_FALSE(backend->finished_in_owner.load());
	CHECK(backend->finish_count.load() == 1);
	CHECK(backend->end_sync_count.load() == 1);
	wrapper->end_sync();
	wrapper->finish();
	CHECK(backend->finish_count.load() == 1);
	CHECK(backend->end_sync_count.load() == 1);
	memdelete(wrapper);
}

TEST_CASE("[PhysicsWrapperStop] inline stopped generated and custom entries preserve outputs") {
	REQUIRE(PhysicsServer3D::get_singleton() == nullptr);
	auto *backend = memnew(RecordingBackend);
	auto *wrapper = memnew(PhysicsServer3DWrapMT(backend, false));
	backend->wrapper = wrapper;
	wrapper->init();
	wrapper->finish();
	const int calls_after_finish = backend->calls.load();
	PhysicsServer3D::MotionParameters parameters;
	PhysicsServer3D::MotionResult result;
	result.travel = Vector3(3, 5, 7);
	// Stopped root calls intentionally diagnose. Their results and backend call
	// counter are asserted individually; lifecycle repetitions stay silent.
	ERR_PRINT_OFF;
	CHECK(wrapper->sphere_shape_create() == RID());
	CHECK(wrapper->shape_get_type(RID()) == PhysicsServer3D::SHAPE_CUSTOM);
	CHECK(wrapper->joint_get_type(RID()) == PhysicsServer3D::JOINT_TYPE_PIN);
	CHECK(wrapper->joint_is_disabled_collisions_between_bodies(RID()));
	wrapper->set_active(true);
	CHECK(wrapper->space_get_direct_state(RID()) == nullptr);
	CHECK(wrapper->space_get_contacts(RID()).is_empty());
	CHECK(wrapper->space_get_contact_count(RID()) == 0);
	CHECK_FALSE(wrapper->body_test_motion(RID(), parameters, &result));
	CHECK(result.travel == Vector3(3, 5, 7));
	CHECK(wrapper->body_get_direct_state(RID()) == nullptr);
	CHECK_FALSE(wrapper->is_flushing_queries());
	CHECK(wrapper->get_process_info(PhysicsServer3D::INFO_ACTIVE_OBJECTS) == 0);
	ERR_PRINT_ON;
	wrapper->step(1.0 / 60.0);
	wrapper->sync();
	wrapper->flush_queries();
	wrapper->end_sync();
	wrapper->finish();
	CHECK(backend->calls.load() == calls_after_finish);
	memdelete(wrapper);
}

TEST_CASE("[PhysicsWrapperStop] ordinary inline lifecycle control") {
	REQUIRE(PhysicsServer3D::get_singleton() == nullptr);
	auto *backend = memnew(RecordingBackend);
	auto *wrapper = memnew(PhysicsServer3DWrapMT(backend, false));
	backend->wrapper = wrapper;
	wrapper->init();
	wrapper->step(1.0 / 60.0);
	wrapper->sync();
	wrapper->flush_queries();
	wrapper->end_sync();
	wrapper->finish();
	CHECK(backend->calls.load() == 6);
	CHECK(backend->finish_count.load() == 1);
	CHECK(backend->end_sync_count.load() == 1);
	CHECK_FALSE(backend->finished_in_owner.load());
	CHECK(backend->finish_thread == Thread::MAIN_ID);
	memdelete(wrapper);
}

#ifdef THREADS_ENABLED
TEST_CASE("[PhysicsWrapperStop] ordinary threaded lifecycle preserves finish affinity") {
	REQUIRE(PhysicsServer3D::get_singleton() == nullptr);
	auto *backend = memnew(RecordingBackend);
	auto *wrapper = memnew(PhysicsServer3DWrapMT(backend, true));
	backend->wrapper = wrapper;
	wrapper->init();
	wrapper->step(1.0 / 60.0);
	wrapper->sync();
	wrapper->flush_queries();
	wrapper->end_sync();
	wrapper->finish();
	CHECK(backend->calls.load() == 6);
	CHECK(backend->finish_count.load() == 1);
	CHECK(backend->end_sync_count.load() == 1);
	CHECK(backend->init_thread != Thread::MAIN_ID);
	CHECK(backend->finish_thread == backend->init_thread);
	memdelete(wrapper);
}
#endif

#ifdef THREADS_ENABLED

class CleanupBackend : public PhysicsServer3DDummy {
public:
	int setter_count = 0;
	mutable int getter_count = 0;
	int finish_count = 0;
	mutable bool finished_before_getter = false;
	int finish_count_when_cleanup_returned = -1;
	void init() override {}
	void finish() override { finish_count++; }
	void shape_set_data(RID p_shape, const Variant &p_data) override { setter_count++; }
	Variant shape_get_data(RID p_shape) const override {
		getter_count++;
		finished_before_getter = finish_count != 0;
		return 123;
	}
};

class FinishFromPayloadCleanup : public RefCounted {
public:
	PhysicsServer3DWrapMT *wrapper = nullptr;
	CleanupBackend *backend = nullptr;
	~FinishFromPayloadCleanup() {
		wrapper->finish();
		backend->finish_count_when_cleanup_returned = backend->finish_count;
	}
};

TEST_CASE("[PhysicsWrapperStop] direct getter preflush payload cleanup defers finish through getter") {
	REQUIRE(PhysicsServer3D::get_singleton() == nullptr);
	auto *backend = memnew(CleanupBackend);
	auto *wrapper = memnew(PhysicsServer3DWrapMT(backend, false));
	wrapper->init();
	struct ProducerContext {
		PhysicsServer3DWrapMT *wrapper;
		CleanupBackend *backend;
	} context{ wrapper, backend };
	Thread producer;
	producer.start([](void *p_userdata) {
		auto *context = static_cast<ProducerContext *>(p_userdata);
		Ref<FinishFromPayloadCleanup> payload;
		payload.instantiate();
		payload->wrapper = context->wrapper;
		payload->backend = context->backend;
		// The nonthreaded wrapper still queues a foreign async setter. After
		// this thread returns, only the real queued Variant owns the payload.
		context->wrapper->shape_set_data(RID(), Variant(payload));
	},
			&context);
	producer.wait_to_finish();
	REQUIRE(backend->setter_count == 0);
	REQUIRE(backend->finish_count == 0);
	const Variant result = wrapper->shape_get_data(RID());
	CHECK(int(result) == 123);
	CHECK(backend->setter_count == 1);
	CHECK(backend->getter_count == 1);
	CHECK(backend->finish_count_when_cleanup_returned == 0);
	CHECK_FALSE(backend->finished_before_getter);
	CHECK(backend->finish_count == 1);
	wrapper->finish();
	memdelete(wrapper);
}

#endif // THREADS_ENABLED

} // namespace TestPhysicsWrapperStop

#endif // PHYSICS_3D_DISABLED
