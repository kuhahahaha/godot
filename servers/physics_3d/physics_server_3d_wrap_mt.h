/**************************************************************************/
/*  physics_server_3d_wrap_mt.h                                           */
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
#include "servers/physics_3d/physics_server_3d.h"

#ifdef DEBUG_ENABLED
#include "core/config/engine.h"
#endif

#define P3D_RID(m_type) \
	RID m_type##_create() override { \
		Scope scope(this); \
		ERR_FAIL_COND_V_MSG(!scope, RID(), "Physics server is stopping."); \
		return physics_server_3d->m_type##_create(); \
	}

#define P3D_FUNC1(m_name, m_a1) \
	void m_name(m_a1 p1) override { \
		Scope scope(this); \
		ERR_FAIL_COND_MSG(!scope, "Physics server is stopping."); \
		_dispatch_async(&PhysicsServer3D::m_name, p1); \
	}

#define P3D_FUNC2(m_name, m_a1, m_a2) \
	void m_name(m_a1 p1, m_a2 p2) override { \
		Scope scope(this); \
		ERR_FAIL_COND_MSG(!scope, "Physics server is stopping."); \
		_dispatch_async(&PhysicsServer3D::m_name, p1, p2); \
	}

#define P3D_FUNC3(m_name, m_a1, m_a2, m_a3) \
	void m_name(m_a1 p1, m_a2 p2, m_a3 p3) override { \
		Scope scope(this); \
		ERR_FAIL_COND_MSG(!scope, "Physics server is stopping."); \
		_dispatch_async(&PhysicsServer3D::m_name, p1, p2, p3); \
	}

#define P3D_FUNC4(m_name, m_a1, m_a2, m_a3, m_a4) \
	void m_name(m_a1 p1, m_a2 p2, m_a3 p3, m_a4 p4) override { \
		Scope scope(this); \
		ERR_FAIL_COND_MSG(!scope, "Physics server is stopping."); \
		_dispatch_async(&PhysicsServer3D::m_name, p1, p2, p3, p4); \
	}

#define P3D_FUNC5(m_name, m_a1, m_a2, m_a3, m_a4, m_a5) \
	void m_name(m_a1 p1, m_a2 p2, m_a3 p3, m_a4 p4, m_a5 p5) override { \
		Scope scope(this); \
		ERR_FAIL_COND_MSG(!scope, "Physics server is stopping."); \
		_dispatch_async(&PhysicsServer3D::m_name, p1, p2, p3, p4, p5); \
	}

#define P3D_FUNC7(m_name, m_a1, m_a2, m_a3, m_a4, m_a5, m_a6, m_a7) \
	void m_name(m_a1 p1, m_a2 p2, m_a3 p3, m_a4 p4, m_a5 p5, m_a6 p6, m_a7 p7) override { \
		Scope scope(this); \
		ERR_FAIL_COND_MSG(!scope, "Physics server is stopping."); \
		_dispatch_async(&PhysicsServer3D::m_name, p1, p2, p3, p4, p5, p6, p7); \
	}

#define P3D_FUNC1RC_DEFAULT(m_r, m_name, m_a1, m_default) \
	m_r m_name(m_a1 p1) const override { \
		Scope scope(this); \
		ERR_FAIL_COND_V_MSG(!scope, m_default, "Physics server is stopping."); \
		return _dispatch_return<m_r>(#m_name, &PhysicsServer3D::m_name, p1); \
	}

#define P3D_FUNC1RC(m_r, m_name, m_a1) \
	P3D_FUNC1RC_DEFAULT(m_r, m_name, m_a1, m_r())

#define P3D_FUNC2RC_DEFAULT(m_r, m_name, m_a1, m_a2, m_default) \
	m_r m_name(m_a1 p1, m_a2 p2) const override { \
		Scope scope(this); \
		ERR_FAIL_COND_V_MSG(!scope, m_default, "Physics server is stopping."); \
		return _dispatch_return<m_r>(#m_name, &PhysicsServer3D::m_name, p1, p2); \
	}

#define P3D_FUNC2RC(m_r, m_name, m_a1, m_a2) \
	P3D_FUNC2RC_DEFAULT(m_r, m_name, m_a1, m_a2, m_r())

#define P3D_FUNC3RC_DEFAULT(m_r, m_name, m_a1, m_a2, m_a3, m_default) \
	m_r m_name(m_a1 p1, m_a2 p2, m_a3 p3) const override { \
		Scope scope(this); \
		ERR_FAIL_COND_V_MSG(!scope, m_default, "Physics server is stopping."); \
		return _dispatch_return<m_r>(#m_name, &PhysicsServer3D::m_name, p1, p2, p3); \
	}

#define P3D_FUNC3RC(m_r, m_name, m_a1, m_a2, m_a3) \
	P3D_FUNC3RC_DEFAULT(m_r, m_name, m_a1, m_a2, m_a3, m_r())

#define P3D_FUNC2S(m_name, m_a1, m_a2) \
	void m_name(m_a1 p1, m_a2 p2) override { \
		Scope scope(this); \
		ERR_FAIL_COND_MSG(!scope, "Physics server is stopping."); \
		_dispatch_sync(#m_name, &PhysicsServer3D::m_name, p1, p2); \
	}

class PhysicsServer3DWrapMT : public PhysicsServer3D {
	GDSOFTCLASS(PhysicsServer3DWrapMT, PhysicsServer3D);

#ifdef TESTS_ENABLED
	friend class TestPhysicsQueryRetirementAccess;
	friend class TestPhysicsWrapperR1Access;
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

	mutable PhysicsServer3D *physics_server_3d = nullptr;

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
		friend class PhysicsServer3DWrapMT;
		PhysicsServer3DWrapMT *wrapper;
		Scope *previous = nullptr;
		ScopeKind kind;
		WorkerThreadPool::TaskID task_id = WorkerThreadPool::INVALID_TASK_ID;
		bool admitted = false;
		bool ticket = false;
		bool main_root = false;

	public:
		Scope(const PhysicsServer3DWrapMT *p_wrapper, ScopeKind p_kind = ScopeKind::ROOT);
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
			command_queue.push(physics_server_3d, p_method, std::forward<Args>(p_args)...);
		} else {
			Scope owner(this, ScopeKind::OWNER);
			command_queue.flush_if_pending();
			(physics_server_3d->*p_method)(std::forward<Args>(p_args)...);
		}
	}
	template <typename M, typename... Args>
	void _dispatch_sync(const char *p_name, M p_method, Args &&...p_args) {
		if (_should_queue()) {
			command_queue.push_and_sync(physics_server_3d, p_method, std::forward<Args>(p_args)...);
			_warn_sync(p_name);
		} else {
			Scope owner(this, ScopeKind::OWNER);
			command_queue.flush_if_pending();
			(physics_server_3d->*p_method)(std::forward<Args>(p_args)...);
		}
	}
	template <typename R, typename M, typename... Args>
	R _dispatch_return(const char *p_name, M p_method, Args &&...p_args) const {
		auto *self = const_cast<PhysicsServer3DWrapMT *>(this);
		if (_should_queue()) {
			R result{};
			command_queue.push_and_ret(physics_server_3d, p_method, &result, std::forward<Args>(p_args)...);
			_warn_sync(p_name);
			return result;
		}
		Scope owner(this, ScopeKind::OWNER);
		self->command_queue.flush_if_pending();
		return (physics_server_3d->*p_method)(std::forward<Args>(p_args)...);
	}

	void _assign_mt_ids(WorkerThreadPool::TaskID p_pump_task_id);
	void _thread_loop();
	void _thread_sync();

public:
	//FUNC1RID(shape,ShapeType); todo fix
	P3D_RID(world_boundary_shape)
	P3D_RID(separation_ray_shape)
	P3D_RID(sphere_shape)
	P3D_RID(box_shape)
	P3D_RID(capsule_shape)
	P3D_RID(cylinder_shape)
	P3D_RID(convex_polygon_shape)
	P3D_RID(concave_polygon_shape)
	P3D_RID(heightmap_shape)
	P3D_RID(custom_shape)

	P3D_FUNC2(shape_set_data, RID, const Variant &);
	P3D_FUNC2(shape_set_custom_solver_bias, RID, real_t);

	P3D_FUNC2(shape_set_margin, RID, real_t)
	P3D_FUNC1RC(real_t, shape_get_margin, RID)

	P3D_FUNC1RC_DEFAULT(ShapeType, shape_get_type, RID, SHAPE_CUSTOM);
	P3D_FUNC1RC(Variant, shape_get_data, RID);
	P3D_FUNC1RC(real_t, shape_get_custom_solver_bias, RID);
#if 0
	//these work well, but should be used from the main thread only
	bool shape_collide(RID p_shape_A, const Transform &p_xform_A, const Vector3 &p_motion_A, RID p_shape_B, const Transform &p_xform_B, const Vector3 &p_motion_B, Vector3 *r_results, int p_result_max, int &r_result_count) {
		ERR_FAIL_COND_V(!Thread::is_main_thread(), false);
		return physics_server_3d->shape_collide(p_shape_A, p_xform_A, p_motion_A, p_shape_B, p_xform_B, p_motion_B, r_results, p_result_max, r_result_count);
	}
#endif
	/* SPACE API */

	P3D_RID(space);
	P3D_FUNC2(space_set_active, RID, bool);
	P3D_FUNC1RC(bool, space_is_active, RID);

	P3D_FUNC3(space_set_param, RID, SpaceParameter, real_t);
	P3D_FUNC2RC(real_t, space_get_param, RID, SpaceParameter);

	// this function only works on physics process, errors and returns null otherwise
	PhysicsDirectSpaceState3D *space_get_direct_state(RID p_space) override {
		Scope scope(this);
		ERR_FAIL_COND_V_MSG(!scope, nullptr, "Physics server is stopping.");
		ERR_FAIL_COND_V(!Thread::is_main_thread(), nullptr);
		return physics_server_3d->space_get_direct_state(p_space);
	}

	P3D_FUNC2(space_set_debug_contacts, RID, int);
	virtual Vector<Vector3> space_get_contacts(RID p_space) const override {
		Scope scope(this);
		ERR_FAIL_COND_V_MSG(!scope, Vector<Vector3>(), "Physics server is stopping.");
		ERR_FAIL_COND_V(!Thread::is_main_thread(), Vector<Vector3>());
		return physics_server_3d->space_get_contacts(p_space);
	}

	virtual int space_get_contact_count(RID p_space) const override {
		Scope scope(this);
		ERR_FAIL_COND_V_MSG(!scope, 0, "Physics server is stopping.");
		ERR_FAIL_COND_V(!Thread::is_main_thread(), 0);
		return physics_server_3d->space_get_contact_count(p_space);
	}

	/* AREA API */

	//FUNC0RID(area);
	P3D_RID(area);

	P3D_FUNC2(area_set_space, RID, RID);
	P3D_FUNC1RC(RID, area_get_space, RID);

	P3D_FUNC4(area_add_shape, RID, RID, const Transform3D &, bool);
	P3D_FUNC3(area_set_shape, RID, int, RID);
	P3D_FUNC3(area_set_shape_transform, RID, int, const Transform3D &);
	P3D_FUNC3(area_set_shape_disabled, RID, int, bool);

	P3D_FUNC1RC(int, area_get_shape_count, RID);
	P3D_FUNC2RC(RID, area_get_shape, RID, int);
	P3D_FUNC2RC(Transform3D, area_get_shape_transform, RID, int);
	P3D_FUNC2(area_remove_shape, RID, int);
	P3D_FUNC1(area_clear_shapes, RID);

	P3D_FUNC2(area_attach_object_instance_id, RID, ObjectID);
	P3D_FUNC1RC(ObjectID, area_get_object_instance_id, RID);

	P3D_FUNC3(area_set_param, RID, AreaParameter, const Variant &);
	P3D_FUNC2(area_set_transform, RID, const Transform3D &);

	P3D_FUNC2RC(Variant, area_get_param, RID, AreaParameter);
	P3D_FUNC1RC(Transform3D, area_get_transform, RID);

	P3D_FUNC2(area_set_collision_layer, RID, uint32_t);
	P3D_FUNC1RC(uint32_t, area_get_collision_layer, RID);

	P3D_FUNC2(area_set_collision_mask, RID, uint32_t);
	P3D_FUNC1RC(uint32_t, area_get_collision_mask, RID);

	P3D_FUNC2(area_set_monitorable, RID, bool);
	P3D_FUNC2(area_set_ray_pickable, RID, bool);

	P3D_FUNC2(area_set_monitor_callback, RID, const Callable &);
	P3D_FUNC2(area_set_area_monitor_callback, RID, const Callable &);

	/* BODY API */

	//FUNC2RID(body,BodyMode,bool);
	P3D_RID(body)

	P3D_FUNC2(body_set_space, RID, RID);
	P3D_FUNC1RC(RID, body_get_space, RID);

	P3D_FUNC2(body_set_mode, RID, BodyMode);
	P3D_FUNC1RC(BodyMode, body_get_mode, RID);

	P3D_FUNC4(body_add_shape, RID, RID, const Transform3D &, bool);
	P3D_FUNC3(body_set_shape, RID, int, RID);
	P3D_FUNC3(body_set_shape_transform, RID, int, const Transform3D &);

	P3D_FUNC1RC(int, body_get_shape_count, RID);
	P3D_FUNC2RC(Transform3D, body_get_shape_transform, RID, int);
	P3D_FUNC2RC(RID, body_get_shape, RID, int);

	P3D_FUNC3(body_set_shape_disabled, RID, int, bool);

	P3D_FUNC2(body_remove_shape, RID, int);
	P3D_FUNC1(body_clear_shapes, RID);

	P3D_FUNC2(body_attach_object_instance_id, RID, ObjectID);
	P3D_FUNC1RC(ObjectID, body_get_object_instance_id, RID);

	P3D_FUNC2(body_set_enable_continuous_collision_detection, RID, bool);
	P3D_FUNC1RC(bool, body_is_continuous_collision_detection_enabled, RID);

	P3D_FUNC2(body_set_collision_layer, RID, uint32_t);
	P3D_FUNC1RC(uint32_t, body_get_collision_layer, RID);

	P3D_FUNC2(body_set_collision_mask, RID, uint32_t);
	P3D_FUNC1RC(uint32_t, body_get_collision_mask, RID);

	P3D_FUNC2(body_set_collision_priority, RID, real_t);
	P3D_FUNC1RC(real_t, body_get_collision_priority, RID);

	P3D_FUNC2(body_set_user_flags, RID, uint32_t);
	P3D_FUNC1RC(uint32_t, body_get_user_flags, RID);

	P3D_FUNC3(body_set_param, RID, BodyParameter, const Variant &);
	P3D_FUNC2RC(Variant, body_get_param, RID, BodyParameter);

	P3D_FUNC1(body_reset_mass_properties, RID);

	P3D_FUNC3(body_set_state, RID, BodyState, const Variant &);
	P3D_FUNC2RC(Variant, body_get_state, RID, BodyState);

	P3D_FUNC2(body_apply_torque_impulse, RID, const Vector3 &);
	P3D_FUNC2(body_apply_central_impulse, RID, const Vector3 &);
	P3D_FUNC3(body_apply_impulse, RID, const Vector3 &, const Vector3 &);

	P3D_FUNC2(body_apply_central_force, RID, const Vector3 &);
	P3D_FUNC3(body_apply_force, RID, const Vector3 &, const Vector3 &);
	P3D_FUNC2(body_apply_torque, RID, const Vector3 &);

	P3D_FUNC3(soft_body_apply_point_impulse, RID, int, const Vector3 &);
	P3D_FUNC3(soft_body_apply_point_force, RID, int, const Vector3 &);
	P3D_FUNC2(soft_body_apply_central_impulse, RID, const Vector3 &);
	P3D_FUNC2(soft_body_apply_central_force, RID, const Vector3 &);

	P3D_FUNC2(body_add_constant_central_force, RID, const Vector3 &);
	P3D_FUNC3(body_add_constant_force, RID, const Vector3 &, const Vector3 &);
	P3D_FUNC2(body_add_constant_torque, RID, const Vector3 &);

	P3D_FUNC2(body_set_constant_force, RID, const Vector3 &);
	P3D_FUNC1RC(Vector3, body_get_constant_force, RID);

	P3D_FUNC2(body_set_constant_torque, RID, const Vector3 &);
	P3D_FUNC1RC(Vector3, body_get_constant_torque, RID);

	P3D_FUNC2(body_set_axis_velocity, RID, const Vector3 &);

	P3D_FUNC3(body_set_axis_lock, RID, BodyAxis, bool);
	P3D_FUNC2RC(bool, body_is_axis_locked, RID, BodyAxis);

	P3D_FUNC2(body_add_collision_exception, RID, RID);
	P3D_FUNC2(body_remove_collision_exception, RID, RID);
	P3D_FUNC2S(body_get_collision_exceptions, RID, List<RID> *);

	P3D_FUNC2(body_set_max_contacts_reported, RID, int);
	P3D_FUNC1RC(int, body_get_max_contacts_reported, RID);

	P3D_FUNC2(body_set_contacts_reported_depth_threshold, RID, real_t);
	P3D_FUNC1RC(real_t, body_get_contacts_reported_depth_threshold, RID);

	P3D_FUNC2(body_set_omit_force_integration, RID, bool);
	P3D_FUNC1RC(bool, body_is_omitting_force_integration, RID);

	P3D_FUNC2(body_set_state_sync_callback, RID, const Callable &);
	P3D_FUNC3(body_set_force_integration_callback, RID, const Callable &, const Variant &);

	P3D_FUNC2(body_set_ray_pickable, RID, bool);

	bool body_test_motion(RID p_body, const MotionParameters &p_parameters, MotionResult *r_result = nullptr) override {
		Scope scope(this);
		ERR_FAIL_COND_V_MSG(!scope, false, "Physics server is stopping.");
		ERR_FAIL_COND_V(!Thread::is_main_thread(), false);
		return physics_server_3d->body_test_motion(p_body, p_parameters, r_result);
	}

	// this function only works on physics process, errors and returns null otherwise
	PhysicsDirectBodyState3D *body_get_direct_state(RID p_body) override {
		Scope scope(this);
		ERR_FAIL_COND_V_MSG(!scope, nullptr, "Physics server is stopping.");
		ERR_FAIL_COND_V(!Thread::is_main_thread(), nullptr);
		return physics_server_3d->body_get_direct_state(p_body);
	}

	/* SOFT BODY API */

	P3D_RID(soft_body)

	P3D_FUNC2(soft_body_update_rendering_server, RID, RequiredParam<PhysicsServer3DRenderingServerHandler>)

	P3D_FUNC2(soft_body_set_space, RID, RID)
	P3D_FUNC1RC(RID, soft_body_get_space, RID)

	P3D_FUNC2(soft_body_set_ray_pickable, RID, bool);

	P3D_FUNC2(soft_body_set_collision_layer, RID, uint32_t)
	P3D_FUNC1RC(uint32_t, soft_body_get_collision_layer, RID)

	P3D_FUNC2(soft_body_set_collision_mask, RID, uint32_t)
	P3D_FUNC1RC(uint32_t, soft_body_get_collision_mask, RID)

	P3D_FUNC2(soft_body_add_collision_exception, RID, RID)
	P3D_FUNC2(soft_body_remove_collision_exception, RID, RID)
	P3D_FUNC2S(soft_body_get_collision_exceptions, RID, List<RID> *)

	P3D_FUNC3(soft_body_set_state, RID, BodyState, const Variant &);
	P3D_FUNC2RC(Variant, soft_body_get_state, RID, BodyState);

	P3D_FUNC2(soft_body_set_transform, RID, const Transform3D &);

	P3D_FUNC2(soft_body_set_simulation_precision, RID, int);
	P3D_FUNC1RC(int, soft_body_get_simulation_precision, RID);

	P3D_FUNC2(soft_body_set_total_mass, RID, real_t);
	P3D_FUNC1RC(real_t, soft_body_get_total_mass, RID);

	P3D_FUNC2(soft_body_set_linear_stiffness, RID, real_t);
	P3D_FUNC1RC(real_t, soft_body_get_linear_stiffness, RID);

	P3D_FUNC2(soft_body_set_shrinking_factor, RID, real_t);
	P3D_FUNC1RC(real_t, soft_body_get_shrinking_factor, RID);

	P3D_FUNC2(soft_body_set_pressure_coefficient, RID, real_t);
	P3D_FUNC1RC(real_t, soft_body_get_pressure_coefficient, RID);

	P3D_FUNC2(soft_body_set_damping_coefficient, RID, real_t);
	P3D_FUNC1RC(real_t, soft_body_get_damping_coefficient, RID);

	P3D_FUNC2(soft_body_set_drag_coefficient, RID, real_t);
	P3D_FUNC1RC(real_t, soft_body_get_drag_coefficient, RID);

	P3D_FUNC2(soft_body_set_mesh, RID, RID);

	P3D_FUNC1RC(AABB, soft_body_get_bounds, RID);

	P3D_FUNC3(soft_body_move_point, RID, int, const Vector3 &);
	P3D_FUNC2RC(Vector3, soft_body_get_point_global_position, RID, int);

	P3D_FUNC1(soft_body_remove_all_pinned_points, RID);
	P3D_FUNC3(soft_body_pin_point, RID, int, bool);
	P3D_FUNC2RC(bool, soft_body_is_point_pinned, RID, int);

	/* JOINT API */

	P3D_RID(joint)

	P3D_FUNC1(joint_clear, RID)

	P3D_FUNC5(joint_make_pin, RID, RID, const Vector3 &, RID, const Vector3 &)

	P3D_FUNC3(pin_joint_set_param, RID, PinJointParam, real_t)
	P3D_FUNC2RC(real_t, pin_joint_get_param, RID, PinJointParam)

	P3D_FUNC2(pin_joint_set_local_a, RID, const Vector3 &)
	P3D_FUNC1RC(Vector3, pin_joint_get_local_a, RID)

	P3D_FUNC2(pin_joint_set_local_b, RID, const Vector3 &)
	P3D_FUNC1RC(Vector3, pin_joint_get_local_b, RID)

	P3D_FUNC5(joint_make_hinge, RID, RID, const Transform3D &, RID, const Transform3D &)
	P3D_FUNC7(joint_make_hinge_simple, RID, RID, const Vector3 &, const Vector3 &, RID, const Vector3 &, const Vector3 &)

	P3D_FUNC3(hinge_joint_set_param, RID, HingeJointParam, real_t)
	P3D_FUNC2RC(real_t, hinge_joint_get_param, RID, HingeJointParam)

	P3D_FUNC3(hinge_joint_set_flag, RID, HingeJointFlag, bool)
	P3D_FUNC2RC(bool, hinge_joint_get_flag, RID, HingeJointFlag)

	P3D_FUNC5(joint_make_slider, RID, RID, const Transform3D &, RID, const Transform3D &)

	P3D_FUNC3(slider_joint_set_param, RID, SliderJointParam, real_t)
	P3D_FUNC2RC(real_t, slider_joint_get_param, RID, SliderJointParam)

	P3D_FUNC5(joint_make_cone_twist, RID, RID, const Transform3D &, RID, const Transform3D &)

	P3D_FUNC3(cone_twist_joint_set_param, RID, ConeTwistJointParam, real_t)
	P3D_FUNC2RC(real_t, cone_twist_joint_get_param, RID, ConeTwistJointParam)

	P3D_FUNC5(joint_make_generic_6dof, RID, RID, const Transform3D &, RID, const Transform3D &)

	P3D_FUNC4(generic_6dof_joint_set_param, RID, Vector3::Axis, G6DOFJointAxisParam, real_t)
	P3D_FUNC3RC(real_t, generic_6dof_joint_get_param, RID, Vector3::Axis, G6DOFJointAxisParam)

	P3D_FUNC4(generic_6dof_joint_set_flag, RID, Vector3::Axis, G6DOFJointAxisFlag, bool)
	P3D_FUNC3RC(bool, generic_6dof_joint_get_flag, RID, Vector3::Axis, G6DOFJointAxisFlag)

	P3D_FUNC1RC_DEFAULT(JointType, joint_get_type, RID, JOINT_TYPE_PIN);

	P3D_FUNC2(joint_set_solver_priority, RID, int);
	P3D_FUNC1RC(int, joint_get_solver_priority, RID);

	P3D_FUNC2(joint_disable_collisions_between_bodies, RID, bool);
	P3D_FUNC1RC_DEFAULT(bool, joint_is_disabled_collisions_between_bodies, RID, true);

	/* MISC */

	void free_rid(RID p_rid) override;
	P3D_FUNC1(set_active, bool);

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
		return physics_server_3d->get_process_info(p_info);
	}

	PhysicsServer3DWrapMT(PhysicsServer3D *p_contained, bool p_create_thread);
	~PhysicsServer3DWrapMT();
};

#undef P3D_RID
#undef P3D_FUNC1
#undef P3D_FUNC2
#undef P3D_FUNC3
#undef P3D_FUNC4
#undef P3D_FUNC5
#undef P3D_FUNC7
#undef P3D_FUNC1RC_DEFAULT
#undef P3D_FUNC1RC
#undef P3D_FUNC2RC_DEFAULT
#undef P3D_FUNC2RC
#undef P3D_FUNC3RC_DEFAULT
#undef P3D_FUNC3RC
#undef P3D_FUNC2S
