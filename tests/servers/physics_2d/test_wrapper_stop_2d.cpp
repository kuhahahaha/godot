/**************************************************************************/
/*  test_wrapper_stop_2d.cpp                                                 */
/**************************************************************************/

#include "tests/test_macros.h"

TEST_FORCE_LINK(test_wrapper_stop_2d)

#ifndef PHYSICS_2D_DISABLED

#include "servers/physics_2d/physics_server_2d_dummy.h"
#include "servers/physics_2d/physics_server_2d_wrap_mt.h"

#include <atomic>

namespace TestPhysicsWrapper2DStop {

// The production wrapper, queue and WorkerThreadPool remain real. Only backend
// work is recorded: these cases qualify wrapper ordering, not a physics solver.
// No SceneTree tag: creating a second live server would replace its singleton.
class RecordingBackend : public PhysicsServer2DDummy {
public:
	enum RequestFrom { NONE,
		STEP,
		SET_ACTIVE,
		QUERY };
	PhysicsServer2DWrapMT *wrapper = nullptr;
	RequestFrom request_from = NONE;
	std::atomic<int> calls{ 0 };
	std::atomic<int> groove_count{ 0 };
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
	RID circle_shape_create() override {
		calls.fetch_add(1);
		return RID::from_uint64(0xBAD);
	}
	ShapeType shape_get_type(RID p_shape) const override {
		const_cast<RecordingBackend *>(this)->calls.fetch_add(1);
		return SHAPE_CIRCLE;
	}
	JointType joint_get_type(RID p_joint) const override {
		const_cast<RecordingBackend *>(this)->calls.fetch_add(1);
		return JOINT_TYPE_GROOVE;
	}
	bool joint_is_disabled_collisions_between_bodies(RID p_joint) const override {
		const_cast<RecordingBackend *>(this)->calls.fetch_add(1);
		return false;
	}
	bool shape_collide(RID, const Transform2D &, const Vector2 &, RID, const Transform2D &, const Vector2 &, Vector2 *r_results, int p_result_max, int &r_result_count) override {
		calls.fetch_add(1);
		if (r_results && p_result_max > 0) {
			r_results[0] = Vector2(91, 92);
		}
		r_result_count = 99;
		return true;
	}
	bool body_collide_shape(RID, int, RID, const Transform2D &, const Vector2 &, Vector2 *r_results, int p_result_max, int &r_result_count) override {
		calls.fetch_add(1);
		if (r_results && p_result_max > 0) {
			r_results[0] = Vector2(91, 92);
		}
		r_result_count = 99;
		return true;
	}
	int area_get_shape_count(RID) const override {
		const_cast<RecordingBackend *>(this)->calls.fetch_add(1);
		return 91;
	}
	int body_get_shape_count(RID) const override {
		const_cast<RecordingBackend *>(this)->calls.fetch_add(1);
		return 92;
	}
	int body_get_max_contacts_reported(RID) const override {
		const_cast<RecordingBackend *>(this)->calls.fetch_add(1);
		return 93;
	}
	real_t joint_get_param(RID, JointParam) const override {
		const_cast<RecordingBackend *>(this)->calls.fetch_add(1);
		return 94;
	}
	Variant body_get_param(RID, BodyParameter) const override {
		const_cast<RecordingBackend *>(this)->calls.fetch_add(1);
		return 95;
	}
	void body_get_collision_exceptions(RID, List<RID> *r_exceptions) override {
		calls.fetch_add(1);
		r_exceptions->push_back(RID::from_uint64(99));
	}
	void joint_make_groove(RID, const Vector2 &, const Vector2 &, const Vector2 &, RID, RID) override {
		calls.fetch_add(1);
		groove_count.fetch_add(1);
	}
	PhysicsDirectSpaceState2D *space_get_direct_state(RID p_space) override {
		calls.fetch_add(1);
		return nullptr;
	}
	Vector<Vector2> space_get_contacts(RID p_space) const override {
		const_cast<RecordingBackend *>(this)->calls.fetch_add(1);
		Vector<Vector2> result;
		result.push_back(Vector2(91, 92));
		return result;
	}
	int space_get_contact_count(RID p_space) const override {
		const_cast<RecordingBackend *>(this)->calls.fetch_add(1);
		return 91;
	}
	bool body_test_motion(RID p_body, const MotionParameters &p_parameters, MotionResult *r_result = nullptr) override {
		calls.fetch_add(1);
		if (r_result) {
			r_result->travel = Vector2(91, 92);
		}
		return true;
	}
	PhysicsDirectBodyState2D *body_get_direct_state(RID p_body) override {
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

TEST_CASE("[PhysicsWrapper2DStop] inline normal-owner finish waits for owner return") {
	REQUIRE(PhysicsServer2D::get_singleton() == nullptr);
	auto *backend = memnew(RecordingBackend);
	auto *wrapper = memnew(PhysicsServer2DWrapMT(backend, false));
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

TEST_CASE("[PhysicsWrapper2DStop] inline generated-owner finish waits for owner return") {
	REQUIRE(PhysicsServer2D::get_singleton() == nullptr);
	auto *backend = memnew(RecordingBackend);
	auto *wrapper = memnew(PhysicsServer2DWrapMT(backend, false));
	backend->wrapper = wrapper;
	wrapper->init();
	backend->request_from = RecordingBackend::SET_ACTIVE;
	wrapper->set_active(true);
	CHECK(backend->request_returned_before_finish.load());
	CHECK_FALSE(backend->finished_in_owner.load());
	CHECK(backend->finish_count.load() == 1);
	memdelete(wrapper);
}

TEST_CASE("[PhysicsWrapper2DStop] inline query finish ends active sync once after traversal") {
	REQUIRE(PhysicsServer2D::get_singleton() == nullptr);
	auto *backend = memnew(RecordingBackend);
	auto *wrapper = memnew(PhysicsServer2DWrapMT(backend, false));
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

TEST_CASE("[PhysicsWrapper2DStop] inline stopped generated and custom entries preserve outputs") {
	REQUIRE(PhysicsServer2D::get_singleton() == nullptr);
	auto *backend = memnew(RecordingBackend);
	auto *wrapper = memnew(PhysicsServer2DWrapMT(backend, false));
	backend->wrapper = wrapper;
	wrapper->init();
	wrapper->finish();
	const int calls_after_finish = backend->calls.load();
	PhysicsServer2D::MotionParameters parameters;
	PhysicsServer2D::MotionResult result;
	result.travel = Vector2(3, 5);
	// Stopped root calls intentionally diagnose. Their results and backend call
	// counter are asserted individually; lifecycle repetitions stay silent.
	ERR_PRINT_OFF;
	CHECK(wrapper->circle_shape_create() == RID());
	CHECK(wrapper->shape_get_type(RID()) == PhysicsServer2D::SHAPE_CUSTOM);
	CHECK(wrapper->joint_get_type(RID()) == PhysicsServer2D::JOINT_TYPE_PIN);
	CHECK(wrapper->joint_is_disabled_collisions_between_bodies(RID()));
	CHECK(wrapper->area_get_shape_count(RID()) == -1);
	CHECK(wrapper->body_get_shape_count(RID()) == -1);
	CHECK(wrapper->body_get_max_contacts_reported(RID()) == -1);
	CHECK(wrapper->joint_get_param(RID(), PhysicsServer2D::JOINT_PARAM_BIAS) == real_t(-1));
	const Variant body_param = wrapper->body_get_param(RID(), PhysicsServer2D::BODY_PARAM_MASS);
	CHECK(body_param.get_type() == Variant::INT);
	CHECK(int64_t(body_param) == 0);
	List<RID> exceptions;
	exceptions.push_back(RID::from_uint64(17));
	wrapper->body_get_collision_exceptions(RID(), &exceptions);
	CHECK(exceptions.size() == 1);
	CHECK(exceptions.front()->get() == RID::from_uint64(17));
	wrapper->joint_make_groove(RID(), Vector2(), Vector2(), Vector2(), RID(), RID());
	CHECK(backend->groove_count.load() == 0);
	Vector2 collisions[2] = { Vector2(3, 5), Vector2(7, 11) };
	int result_count = 17;
	CHECK_FALSE(wrapper->shape_collide(RID(), Transform2D(), Vector2(), RID(), Transform2D(), Vector2(), collisions, 1, result_count));
	CHECK(result_count == 17);
	CHECK(collisions[0] == Vector2(3, 5));
	CHECK(collisions[1] == Vector2(7, 11));
	CHECK_FALSE(wrapper->body_collide_shape(RID(), 0, RID(), Transform2D(), Vector2(), collisions, 1, result_count));
	CHECK(result_count == 17);
	CHECK(collisions[0] == Vector2(3, 5));
	CHECK(collisions[1] == Vector2(7, 11));
	wrapper->set_active(true);
	CHECK(wrapper->space_get_direct_state(RID()) == nullptr);
	CHECK(wrapper->space_get_contacts(RID()).is_empty());
	CHECK(wrapper->space_get_contact_count(RID()) == 0);
	CHECK_FALSE(wrapper->body_test_motion(RID(), parameters, &result));
	CHECK(result.travel == Vector2(3, 5));
	CHECK(wrapper->body_get_direct_state(RID()) == nullptr);
	CHECK_FALSE(wrapper->is_flushing_queries());
	CHECK(wrapper->get_process_info(PhysicsServer2D::INFO_ACTIVE_OBJECTS) == 0);
	ERR_PRINT_ON;
	wrapper->step(1.0 / 60.0);
	wrapper->sync();
	wrapper->flush_queries();
	wrapper->end_sync();
	wrapper->finish();
	CHECK(backend->calls.load() == calls_after_finish);
	memdelete(wrapper);
}

TEST_CASE("[PhysicsWrapper2DStop] ordinary inline lifecycle control") {
	REQUIRE(PhysicsServer2D::get_singleton() == nullptr);
	auto *backend = memnew(RecordingBackend);
	auto *wrapper = memnew(PhysicsServer2DWrapMT(backend, false));
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
TEST_CASE("[PhysicsWrapper2DStop] ordinary threaded lifecycle preserves finish affinity") {
	REQUIRE(PhysicsServer2D::get_singleton() == nullptr);
	auto *backend = memnew(RecordingBackend);
	auto *wrapper = memnew(PhysicsServer2DWrapMT(backend, true));
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

class CleanupBackend : public PhysicsServer2DDummy {
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
	PhysicsServer2DWrapMT *wrapper = nullptr;
	CleanupBackend *backend = nullptr;
	~FinishFromPayloadCleanup() {
		wrapper->finish();
		backend->finish_count_when_cleanup_returned = backend->finish_count;
	}
};

TEST_CASE("[PhysicsWrapper2DStop] direct getter preflush payload cleanup defers finish through getter") {
	REQUIRE(PhysicsServer2D::get_singleton() == nullptr);
	auto *backend = memnew(CleanupBackend);
	auto *wrapper = memnew(PhysicsServer2DWrapMT(backend, false));
	wrapper->init();
	struct ProducerContext {
		PhysicsServer2DWrapMT *wrapper;
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

#ifdef THREADS_ENABLED
TEST_CASE("[PhysicsWrapper2DStop] ordinary foreign body-collide caller policy control") {
	REQUIRE(PhysicsServer2D::get_singleton() == nullptr);
	auto *backend = memnew(RecordingBackend);
	auto *wrapper = memnew(PhysicsServer2DWrapMT(backend, false));
	backend->wrapper = wrapper;
	wrapper->init();
	struct Context {
		PhysicsServer2DWrapMT *wrapper;
		bool result = false;
		int count = 0;
		Vector2 point;
	} context{ wrapper };
	Thread foreign;
	foreign.start([](void *p_data) {
		auto *context = static_cast<Context *>(p_data);
		context->result = context->wrapper->body_collide_shape(RID(), 0, RID(), Transform2D(), Vector2(), &context->point, 1, context->count);
	},
			&context);
	foreign.wait_to_finish();
	CHECK(context.result);
	CHECK(context.count == 99);
	CHECK(context.point == Vector2(91, 92));
	wrapper->joint_make_groove(RID(), Vector2(), Vector2(), Vector2(), RID(), RID());
	CHECK(backend->groove_count.load() == 1);
	wrapper->finish();
	memdelete(wrapper);
}
#endif

} // namespace TestPhysicsWrapper2DStop

#endif // PHYSICS_2D_DISABLED
