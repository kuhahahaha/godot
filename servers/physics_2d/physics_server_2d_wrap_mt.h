/**************************************************************************/
/*  physics_server_2d_wrap_mt.h                                           */
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

#pragma once

#include "core/object/worker_thread_pool.h"
#include "core/os/thread.h"
#include "core/templates/command_queue_mt.h"
#include "servers/physics_2d/physics_server_2d.h"

#ifdef DEBUG_ENABLED
#include "core/config/engine.h"
#endif

#define P2D_RID(m_type) \
	RID m_type##_create() override { \
		Scope scope(this); \
		ERR_FAIL_COND_V_MSG(!scope, RID(), "Physics server is stopping."); \
		return physics_server_2d->m_type##_create(); \
	}

#define P2D_FUNC1(m_name, m_a1) \
	void m_name(m_a1 p1) override { \
		Scope scope(this); \
		ERR_FAIL_COND_MSG(!scope, "Physics server is stopping."); \
		_dispatch_async(&PhysicsServer2D::m_name, p1); \
	}

#define P2D_FUNC2(m_name, m_a1, m_a2) \
	void m_name(m_a1 p1, m_a2 p2) override { \
		Scope scope(this); \
		ERR_FAIL_COND_MSG(!scope, "Physics server is stopping."); \
		_dispatch_async(&PhysicsServer2D::m_name, p1, p2); \
	}

#define P2D_FUNC3(m_name, m_a1, m_a2, m_a3) \
	void m_name(m_a1 p1, m_a2 p2, m_a3 p3) override { \
		Scope scope(this); \
		ERR_FAIL_COND_MSG(!scope, "Physics server is stopping."); \
		_dispatch_async(&PhysicsServer2D::m_name, p1, p2, p3); \
	}

#define P2D_FUNC4(m_name, m_a1, m_a2, m_a3, m_a4) \
	void m_name(m_a1 p1, m_a2 p2, m_a3 p3, m_a4 p4) override { \
		Scope scope(this); \
		ERR_FAIL_COND_MSG(!scope, "Physics server is stopping."); \
		_dispatch_async(&PhysicsServer2D::m_name, p1, p2, p3, p4); \
	}

#define P2D_FUNC5(m_name, m_a1, m_a2, m_a3, m_a4, m_a5) \
	void m_name(m_a1 p1, m_a2 p2, m_a3 p3, m_a4 p4, m_a5 p5) override { \
		Scope scope(this); \
		ERR_FAIL_COND_MSG(!scope, "Physics server is stopping."); \
		_dispatch_async(&PhysicsServer2D::m_name, p1, p2, p3, p4, p5); \
	}

#define P2D_FUNC6(m_name, m_a1, m_a2, m_a3, m_a4, m_a5, m_a6) \
	void m_name(m_a1 p1, m_a2 p2, m_a3 p3, m_a4 p4, m_a5 p5, m_a6 p6) override { \
		Scope scope(this); \
		ERR_FAIL_COND_MSG(!scope, "Physics server is stopping."); \
		_dispatch_async(&PhysicsServer2D::m_name, p1, p2, p3, p4, p5, p6); \
	}

#define P2D_FUNC1RC_DEFAULT(m_r, m_name, m_a1, m_default) \
	m_r m_name(m_a1 p1) const override { \
		Scope scope(this); \
		ERR_FAIL_COND_V_MSG(!scope, m_default, "Physics server is stopping."); \
		return _dispatch_return<m_r>(#m_name, &PhysicsServer2D::m_name, p1); \
	}

#define P2D_FUNC2RC_DEFAULT(m_r, m_name, m_a1, m_a2, m_default) \
	m_r m_name(m_a1 p1, m_a2 p2) const override { \
		Scope scope(this); \
		ERR_FAIL_COND_V_MSG(!scope, m_default, "Physics server is stopping."); \
		return _dispatch_return<m_r>(#m_name, &PhysicsServer2D::m_name, p1, p2); \
	}

#define P2D_FUNC2S(m_name, m_a1, m_a2) \
	void m_name(m_a1 p1, m_a2 p2) override { \
		Scope scope(this); \
		ERR_FAIL_COND_MSG(!scope, "Physics server is stopping."); \
		_dispatch_sync(#m_name, &PhysicsServer2D::m_name, p1, p2); \
	}

class PhysicsServer2DWrapMT : public PhysicsServer2D {
	GDSOFTCLASS(PhysicsServer2DWrapMT, PhysicsServer2D);

#ifdef TESTS_ENABLED
	friend class TestPhysicsWrapper2DR1Access;
	enum class TestPoint { ROOT_ADMITTED,
		BEFORE_BARRIER,
		BARRIER_ENTER,
		AFTER_BARRIER,
		DRAIN_AFTER_FLUSH,
		BOUNDARY_ENTRY };
	void (*test_callback)(void *, TestPoint) = nullptr;
	void *test_context = nullptr;
	void _test_point(TestPoint p_point) {
		if (test_callback) {
			test_callback(test_context, p_point);
		}
	}
#endif

	mutable PhysicsServer2D *physics_server_2d = nullptr;

	mutable CommandQueueMT command_queue;

	Thread::ID server_thread = Thread::UNASSIGNED_ID;
	WorkerThreadPool::TaskID server_task_id = WorkerThreadPool::INVALID_TASK_ID;
	bool create_thread = false;
	SafeFlag doing_sync;

	enum class StopState { OPEN,
		REQUESTED,
		DRAINING,
		CLOSED };
	enum class ControlAction { RUN,
		PARK,
		FINISH_ONLY };
	enum class ScopeKind { ROOT,
		RELEASE,
		OWNER,
		DRAIN,
		FINISH };
	class Scope {
		friend class PhysicsServer2DWrapMT;
		PhysicsServer2DWrapMT *wrapper;
		Scope *previous = nullptr;
		ScopeKind kind;
		WorkerThreadPool::TaskID task_id = WorkerThreadPool::INVALID_TASK_ID;
		bool admitted = false;
		bool ticket = false;
		bool main_root = false;

	public:
		Scope(const PhysicsServer2DWrapMT *p_wrapper, ScopeKind p_kind = ScopeKind::ROOT);
		~Scope();
		explicit operator bool() const { return admitted; }
	};
	inline static thread_local Scope *scope_top = nullptr;
	mutable BinaryMutex control_mutex;
	StopState stop_state = StopState::OPEN;
	ControlAction control_action = ControlAction::RUN;
	uint64_t publishers = 0;
	uint32_t main_depth = 0;
	uint64_t requested_serial = 0;
	uint64_t acknowledged_serial = 0;
	uint64_t released_serial = 0;
	bool parked = false;
	bool pump_exited = false;
	bool release_sealed = false;
	bool initialized = false;
	bool sync_in_progress = false;
	bool backend_sync_active = false;
	bool sync_lease = false;
	bool main_dispatch_authority = false;
	bool query_active = false;

	bool _has_release_ancestor(bool p_include_finish = false) const;
	bool _should_queue() const;
	void _notify_pump();
	void _request_finish();
	void _service_stop();
	void _acquire_park();
	void _drain_publishers();
	void _warn_sync(const char *p_method) const;

	template <typename M, typename... Args>
	void _dispatch_async(M p_method, Args &&...p_args) {
		if (_should_queue()) {
			command_queue.push(physics_server_2d, p_method, std::forward<Args>(p_args)...);
		} else {
			Scope owner(this, ScopeKind::OWNER);
			command_queue.flush_if_pending();
			(physics_server_2d->*p_method)(std::forward<Args>(p_args)...);
		}
	}
	template <typename M, typename... Args>
	void _dispatch_sync(const char *p_name, M p_method, Args &&...p_args) {
		if (_should_queue()) {
			command_queue.push_and_sync(physics_server_2d, p_method, std::forward<Args>(p_args)...);
			_warn_sync(p_name);
		} else {
			Scope owner(this, ScopeKind::OWNER);
			command_queue.flush_if_pending();
			(physics_server_2d->*p_method)(std::forward<Args>(p_args)...);
		}
	}
	template <typename R, typename M, typename... Args>
	R _dispatch_return(const char *p_name, M p_method, Args &&...p_args) const {
		auto *self = const_cast<PhysicsServer2DWrapMT *>(this);
		if (_should_queue()) {
			R result{};
			command_queue.push_and_ret(physics_server_2d, p_method, &result, std::forward<Args>(p_args)...);
			_warn_sync(p_name);
			return result;
		}
		Scope owner(this, ScopeKind::OWNER);
		self->command_queue.flush_if_pending();
		return (physics_server_2d->*p_method)(std::forward<Args>(p_args)...);
	}

	void _assign_mt_ids(WorkerThreadPool::TaskID p_pump_task_id);
	void _thread_loop();
	void _thread_sync();

public:
	//FUNC1RID(shape,ShapeType); todo fix
	P2D_RID(world_boundary_shape)
	P2D_RID(separation_ray_shape)
	P2D_RID(segment_shape)
	P2D_RID(circle_shape)
	P2D_RID(rectangle_shape)
	P2D_RID(capsule_shape)
	P2D_RID(convex_polygon_shape)
	P2D_RID(concave_polygon_shape)

	P2D_FUNC2(shape_set_data, RID, const Variant &);
	P2D_FUNC2(shape_set_custom_solver_bias, RID, real_t);

	P2D_FUNC1RC_DEFAULT(ShapeType, shape_get_type, RID, SHAPE_CUSTOM);
	P2D_FUNC1RC_DEFAULT(Variant, shape_get_data, RID, Variant());
	P2D_FUNC1RC_DEFAULT(real_t, shape_get_custom_solver_bias, RID, 0);

	//these work well, but should be used from the main thread only
	bool shape_collide(RID p_shape_A, const Transform2D &p_xform_A, const Vector2 &p_motion_A, RID p_shape_B, const Transform2D &p_xform_B, const Vector2 &p_motion_B, Vector2 *r_results, int p_result_max, int &r_result_count) override {
		Scope scope(this);
		ERR_FAIL_COND_V_MSG(!scope, false, "Physics server is stopping.");
		ERR_FAIL_COND_V(!Thread::is_main_thread(), false);
		return physics_server_2d->shape_collide(p_shape_A, p_xform_A, p_motion_A, p_shape_B, p_xform_B, p_motion_B, r_results, p_result_max, r_result_count);
	}

	/* SPACE API */

	P2D_RID(space);
	P2D_FUNC2(space_set_active, RID, bool);
	P2D_FUNC1RC_DEFAULT(bool, space_is_active, RID, false);

	P2D_FUNC3(space_set_param, RID, SpaceParameter, real_t);
	P2D_FUNC2RC_DEFAULT(real_t, space_get_param, RID, SpaceParameter, 0);

	// this function only works on physics process, errors and returns null otherwise
	PhysicsDirectSpaceState2D *space_get_direct_state(RID p_space) override {
		Scope scope(this);
		ERR_FAIL_COND_V_MSG(!scope, nullptr, "Physics server is stopping.");
		ERR_FAIL_COND_V(!Thread::is_main_thread(), nullptr);
		return physics_server_2d->space_get_direct_state(p_space);
	}

	P2D_FUNC2(space_set_debug_contacts, RID, int);
	virtual Vector<Vector2> space_get_contacts(RID p_space) const override {
		Scope scope(this);
		ERR_FAIL_COND_V_MSG(!scope, Vector<Vector2>(), "Physics server is stopping.");
		ERR_FAIL_COND_V(!Thread::is_main_thread(), Vector<Vector2>());
		return physics_server_2d->space_get_contacts(p_space);
	}

	virtual int space_get_contact_count(RID p_space) const override {
		Scope scope(this);
		ERR_FAIL_COND_V_MSG(!scope, 0, "Physics server is stopping.");
		ERR_FAIL_COND_V(!Thread::is_main_thread(), 0);
		return physics_server_2d->space_get_contact_count(p_space);
	}

	/* AREA API */

	//FUNC0RID(area);
	P2D_RID(area);

	P2D_FUNC2(area_set_space, RID, RID);
	P2D_FUNC1RC_DEFAULT(RID, area_get_space, RID, RID());

	P2D_FUNC4(area_add_shape, RID, RID, const Transform2D &, bool);
	P2D_FUNC3(area_set_shape, RID, int, RID);
	P2D_FUNC3(area_set_shape_transform, RID, int, const Transform2D &);
	P2D_FUNC3(area_set_shape_disabled, RID, int, bool);

	P2D_FUNC1RC_DEFAULT(int, area_get_shape_count, RID, -1);
	P2D_FUNC2RC_DEFAULT(RID, area_get_shape, RID, int, RID());
	P2D_FUNC2RC_DEFAULT(Transform2D, area_get_shape_transform, RID, int, Transform2D());
	P2D_FUNC2(area_remove_shape, RID, int);
	P2D_FUNC1(area_clear_shapes, RID);

	P2D_FUNC2(area_attach_object_instance_id, RID, ObjectID);
	P2D_FUNC1RC_DEFAULT(ObjectID, area_get_object_instance_id, RID, ObjectID());

	P2D_FUNC2(area_attach_canvas_instance_id, RID, ObjectID);
	P2D_FUNC1RC_DEFAULT(ObjectID, area_get_canvas_instance_id, RID, ObjectID());

	P2D_FUNC3(area_set_param, RID, AreaParameter, const Variant &);
	P2D_FUNC2(area_set_transform, RID, const Transform2D &);

	P2D_FUNC2RC_DEFAULT(Variant, area_get_param, RID, AreaParameter, Variant());
	P2D_FUNC1RC_DEFAULT(Transform2D, area_get_transform, RID, Transform2D());

	P2D_FUNC2(area_set_collision_layer, RID, uint32_t);
	P2D_FUNC1RC_DEFAULT(uint32_t, area_get_collision_layer, RID, 0);

	P2D_FUNC2(area_set_collision_mask, RID, uint32_t);
	P2D_FUNC1RC_DEFAULT(uint32_t, area_get_collision_mask, RID, 0);

	P2D_FUNC2(area_set_monitorable, RID, bool);
	P2D_FUNC2(area_set_pickable, RID, bool);

	P2D_FUNC2(area_set_monitor_callback, RID, const Callable &);
	P2D_FUNC2(area_set_area_monitor_callback, RID, const Callable &);

	/* BODY API */

	//FUNC2RID(body,BodyMode,bool);
	P2D_RID(body)

	P2D_FUNC2(body_set_space, RID, RID);
	P2D_FUNC1RC_DEFAULT(RID, body_get_space, RID, RID());

	P2D_FUNC2(body_set_mode, RID, BodyMode);
	P2D_FUNC1RC_DEFAULT(BodyMode, body_get_mode, RID, BODY_MODE_STATIC);

	P2D_FUNC4(body_add_shape, RID, RID, const Transform2D &, bool);
	P2D_FUNC3(body_set_shape, RID, int, RID);
	P2D_FUNC3(body_set_shape_transform, RID, int, const Transform2D &);

	P2D_FUNC1RC_DEFAULT(int, body_get_shape_count, RID, -1);
	P2D_FUNC2RC_DEFAULT(Transform2D, body_get_shape_transform, RID, int, Transform2D());
	P2D_FUNC2RC_DEFAULT(RID, body_get_shape, RID, int, RID());

	P2D_FUNC3(body_set_shape_disabled, RID, int, bool);
	P2D_FUNC5(body_set_shape_as_one_way_collision, RID, int, bool, real_t, const Vector2 &);

	P2D_FUNC2(body_remove_shape, RID, int);
	P2D_FUNC1(body_clear_shapes, RID);

	P2D_FUNC2(body_attach_object_instance_id, RID, ObjectID);
	P2D_FUNC1RC_DEFAULT(ObjectID, body_get_object_instance_id, RID, ObjectID());

	P2D_FUNC2(body_attach_canvas_instance_id, RID, ObjectID);
	P2D_FUNC1RC_DEFAULT(ObjectID, body_get_canvas_instance_id, RID, ObjectID());

	P2D_FUNC2(body_set_continuous_collision_detection_mode, RID, CCDMode);
	P2D_FUNC1RC_DEFAULT(CCDMode, body_get_continuous_collision_detection_mode, RID, CCD_MODE_DISABLED);

	P2D_FUNC2(body_set_collision_layer, RID, uint32_t);
	P2D_FUNC1RC_DEFAULT(uint32_t, body_get_collision_layer, RID, 0);

	P2D_FUNC2(body_set_collision_mask, RID, uint32_t);
	P2D_FUNC1RC_DEFAULT(uint32_t, body_get_collision_mask, RID, 0);

	P2D_FUNC2(body_set_collision_priority, RID, real_t);
	P2D_FUNC1RC_DEFAULT(real_t, body_get_collision_priority, RID, 0);

	P2D_FUNC3(body_set_param, RID, BodyParameter, const Variant &);
	P2D_FUNC2RC_DEFAULT(Variant, body_get_param, RID, BodyParameter, 0);

	P2D_FUNC1(body_reset_mass_properties, RID);

	P2D_FUNC3(body_set_state, RID, BodyState, const Variant &);
	P2D_FUNC2RC_DEFAULT(Variant, body_get_state, RID, BodyState, Variant());

	P2D_FUNC2(body_apply_central_impulse, RID, const Vector2 &);
	P2D_FUNC2(body_apply_torque_impulse, RID, real_t);
	P2D_FUNC3(body_apply_impulse, RID, const Vector2 &, const Vector2 &);

	P2D_FUNC2(body_apply_central_force, RID, const Vector2 &);
	P2D_FUNC3(body_apply_force, RID, const Vector2 &, const Vector2 &);
	P2D_FUNC2(body_apply_torque, RID, real_t);

	P2D_FUNC2(body_add_constant_central_force, RID, const Vector2 &);
	P2D_FUNC3(body_add_constant_force, RID, const Vector2 &, const Vector2 &);
	P2D_FUNC2(body_add_constant_torque, RID, real_t);

	P2D_FUNC2(body_set_constant_force, RID, const Vector2 &);
	P2D_FUNC1RC_DEFAULT(Vector2, body_get_constant_force, RID, Vector2());

	P2D_FUNC2(body_set_constant_torque, RID, real_t);
	P2D_FUNC1RC_DEFAULT(real_t, body_get_constant_torque, RID, 0);

	P2D_FUNC2(body_set_axis_velocity, RID, const Vector2 &);

	P2D_FUNC2(body_add_collision_exception, RID, RID);
	P2D_FUNC2(body_remove_collision_exception, RID, RID);
	P2D_FUNC2S(body_get_collision_exceptions, RID, List<RID> *);

	P2D_FUNC2(body_set_max_contacts_reported, RID, int);
	P2D_FUNC1RC_DEFAULT(int, body_get_max_contacts_reported, RID, -1);

	P2D_FUNC2(body_set_contacts_reported_depth_threshold, RID, real_t);
	P2D_FUNC1RC_DEFAULT(real_t, body_get_contacts_reported_depth_threshold, RID, 0);

	P2D_FUNC2(body_set_omit_force_integration, RID, bool);
	P2D_FUNC1RC_DEFAULT(bool, body_is_omitting_force_integration, RID, false);

	P2D_FUNC2(body_set_state_sync_callback, RID, const Callable &);
	P2D_FUNC3(body_set_force_integration_callback, RID, const Callable &, const Variant &);

	bool body_collide_shape(RID p_body, int p_body_shape, RID p_shape, const Transform2D &p_shape_xform, const Vector2 &p_motion, Vector2 *r_results, int p_result_max, int &r_result_count) override {
		Scope scope(this);
		ERR_FAIL_COND_V_MSG(!scope, false, "Physics server is stopping.");
		return physics_server_2d->body_collide_shape(p_body, p_body_shape, p_shape, p_shape_xform, p_motion, r_results, p_result_max, r_result_count);
	}

	P2D_FUNC2(body_set_pickable, RID, bool);

	bool body_test_motion(RID p_body, const MotionParameters &p_parameters, MotionResult *r_result = nullptr) override {
		Scope scope(this);
		ERR_FAIL_COND_V_MSG(!scope, false, "Physics server is stopping.");
		ERR_FAIL_COND_V(!Thread::is_main_thread(), false);
		return physics_server_2d->body_test_motion(p_body, p_parameters, r_result);
	}

	// this function only works on physics process, errors and returns null otherwise
	PhysicsDirectBodyState2D *body_get_direct_state(RID p_body) override {
		Scope scope(this);
		ERR_FAIL_COND_V_MSG(!scope, nullptr, "Physics server is stopping.");
		ERR_FAIL_COND_V(!Thread::is_main_thread(), nullptr);
		return physics_server_2d->body_get_direct_state(p_body);
	}

	/* JOINT API */

	P2D_RID(joint)

	P2D_FUNC1(joint_clear, RID)

	P2D_FUNC3(joint_set_param, RID, JointParam, real_t);
	P2D_FUNC2RC_DEFAULT(real_t, joint_get_param, RID, JointParam, -1);

	P2D_FUNC2(joint_disable_collisions_between_bodies, RID, const bool);
	P2D_FUNC1RC_DEFAULT(bool, joint_is_disabled_collisions_between_bodies, RID, true);

	///FUNC3RID(pin_joint,const Vector2&,RID,RID);
	///FUNC5RID(groove_joint,const Vector2&,const Vector2&,const Vector2&,RID,RID);
	///FUNC4RID(damped_spring_joint,const Vector2&,const Vector2&,RID,RID);

	//TODO need to convert this to FUNCRID, but it's a hassle..

	P2D_FUNC4(joint_make_pin, RID, const Vector2 &, RID, RID);
	P2D_FUNC6(joint_make_groove, RID, const Vector2 &, const Vector2 &, const Vector2 &, RID, RID);
	P2D_FUNC5(joint_make_damped_spring, RID, const Vector2 &, const Vector2 &, RID, RID);

	P2D_FUNC3(pin_joint_set_param, RID, PinJointParam, real_t);
	P2D_FUNC2RC_DEFAULT(real_t, pin_joint_get_param, RID, PinJointParam, 0);

	P2D_FUNC3(pin_joint_set_flag, RID, PinJointFlag, bool);
	P2D_FUNC2RC_DEFAULT(bool, pin_joint_get_flag, RID, PinJointFlag, false);

	P2D_FUNC3(damped_spring_joint_set_param, RID, DampedSpringParam, real_t);
	P2D_FUNC2RC_DEFAULT(real_t, damped_spring_joint_get_param, RID, DampedSpringParam, 0);

	P2D_FUNC1RC_DEFAULT(JointType, joint_get_type, RID, JOINT_TYPE_PIN);

	/* MISC */

	void free_rid(RID p_rid) override;
	P2D_FUNC1(set_active, bool);

	virtual void init() override;
	virtual void step(real_t p_step) override;
	virtual void sync() override;
	virtual void end_sync() override;
	virtual void flush_queries() override;
	virtual void finish() override;

	virtual bool is_flushing_queries() const override {
		Scope scope(this);
		ERR_FAIL_COND_V_MSG(!scope, false, "Physics server is stopping.");
		MutexLock lock(control_mutex);
		return query_active;
	}

	int get_process_info(ProcessInfo p_info) override {
		Scope scope(this);
		ERR_FAIL_COND_V_MSG(!scope, 0, "Physics server is stopping.");
		return physics_server_2d->get_process_info(p_info);
	}

	PhysicsServer2DWrapMT(PhysicsServer2D *p_contained, bool p_create_thread);
	~PhysicsServer2DWrapMT();
};

#undef P2D_RID
#undef P2D_FUNC1
#undef P2D_FUNC2
#undef P2D_FUNC3
#undef P2D_FUNC4
#undef P2D_FUNC5
#undef P2D_FUNC6
#undef P2D_FUNC1RC_DEFAULT
#undef P2D_FUNC2RC_DEFAULT
#undef P2D_FUNC2S
