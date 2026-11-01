/**************************************************************************/
/*  physics_server_3d_wrap_mt.cpp                                         */
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

#include "physics_server_3d_wrap_mt.h"

#include "core/object/callable_mp.h"
#include "core/os/os.h"

PhysicsServer3DWrapMT::Scope::Scope(const PhysicsServer3DWrapMT *p_wrapper, ScopeKind p_kind) :
		wrapper(const_cast<PhysicsServer3DWrapMT *>(p_wrapper)), kind(p_kind) {
	task_id = WorkerThreadPool::get_singleton()->get_caller_task_id();
	if (kind == ScopeKind::ROOT || kind == ScopeKind::RELEASE) {
		const bool release_ancestor = kind == ScopeKind::RELEASE && wrapper->_has_release_ancestor();
		MutexLock lock(wrapper->control_mutex);
		if (wrapper->release_sealed || wrapper->stop_state == StopState::CLOSED || wrapper->pump_exited) {
			return;
		}
		if (wrapper->stop_state != StopState::OPEN && !release_ancestor) {
			return;
		}
		wrapper->publishers++;
		ticket = true;
		main_root = Thread::is_main_thread();
		if (main_root) {
			wrapper->main_depth++;
		}
	}
	admitted = true;
	previous = scope_top;
	scope_top = this;
#ifdef TESTS_ENABLED
	if (kind == ScopeKind::ROOT) {
		wrapper->_test_point(TestPoint::ROOT_ADMITTED);
	}
#endif
}

PhysicsServer3DWrapMT::Scope::~Scope() {
	if (!admitted) {
		return;
	}
	DEV_ASSERT(scope_top == this);
	scope_top = previous;
	if (ticket) {
		{
			MutexLock lock(wrapper->control_mutex);
			DEV_ASSERT(wrapper->publishers > 0);
			wrapper->publishers--;
			if (main_root) {
				DEV_ASSERT(wrapper->main_depth > 0);
				wrapper->main_depth--;
			}
		}
		// Pump and foreign epilogues cannot acquire Main shutdown authority.
		if (main_root) {
			wrapper->_service_stop();
		}
	}
}

bool PhysicsServer3DWrapMT::_has_release_ancestor(bool p_include_finish) const {
	const WorkerThreadPool::TaskID task = WorkerThreadPool::get_singleton()->get_caller_task_id();
	for (Scope *scope = scope_top; scope; scope = scope->previous) {
		if (scope->wrapper == this && scope->task_id == task && (p_include_finish || scope->kind != ScopeKind::FINISH)) {
			return true;
		}
	}
	return false;
}

bool PhysicsServer3DWrapMT::_should_queue() const {
	const WorkerThreadPool::TaskID task = WorkerThreadPool::get_singleton()->get_caller_task_id();
	MutexLock lock(control_mutex);
	// RUN yield() can execute another task on this OS thread. It can safely
	// borrow dispatch only before PARK is acknowledged: the suspended pump
	// cannot grant Main authority until that task has returned. Queuing a
	// synchronous getter here would instead wait for its own suspended pump.
	const bool is_pump = task != WorkerThreadPool::INVALID_TASK_ID && task == server_task_id;
	const bool borrowed_running_thread = Thread::get_caller_id() == server_thread && !parked && !main_dispatch_authority;
	const bool is_owner = create_thread ? (is_pump || borrowed_running_thread) : Thread::is_main_thread();
	return !is_owner && !(Thread::is_main_thread() && main_dispatch_authority);
}

void PhysicsServer3DWrapMT::_warn_sync(const char *p_method) const {
#ifdef DEBUG_SYNC
	print_line("sync on: " + String(p_method));
#endif
#ifdef DEBUG_ENABLED
	if (Thread::is_main_thread() && Engine::get_singleton()->notify_frame_server_synced()) {
#ifdef DEV_ENABLED
		WARN_PRINT("Call to " + String(p_method) + " causing PhysicsServer3D synchronizations on every frame. This significantly affects performance.");
#endif
	}
#endif
}

void PhysicsServer3DWrapMT::_assign_mt_ids(WorkerThreadPool::TaskID p_pump_task_id) {
	MutexLock lock(control_mutex);
	server_thread = Thread::get_caller_id();
	server_task_id = p_pump_task_id;
}

void PhysicsServer3DWrapMT::_notify_pump() {
	WorkerThreadPool::TaskID task;
	{
		MutexLock lock(control_mutex);
		task = pump_exited ? WorkerThreadPool::INVALID_TASK_ID : server_task_id;
	}
	if (task != WorkerThreadPool::INVALID_TASK_ID) {
		WorkerThreadPool::get_singleton()->notify_yield_over(task);
	}
}

void PhysicsServer3DWrapMT::_thread_loop() {
	for (;;) {
		bool flush = false;
		bool finish_backend = false;
		bool wait_parked = false;
		{
			MutexLock lock(control_mutex);
			if (control_action == ControlAction::FINISH_ONLY) {
				finish_backend = true;
				server_thread = Thread::get_caller_id();
			} else if (control_action == ControlAction::PARK) {
				// This check occurs only after the preceding whole flush scope
				// has returned, including callback-capable payload destruction.
				parked = true;
				acknowledged_serial = requested_serial;
				wait_parked = true;
			} else {
				flush = !doing_sync.is_set();
			}
		}
		if (finish_backend) {
			{
				Scope finishing(this, ScopeKind::FINISH);
				physics_server_3d->finish();
			}
			MutexLock lock(control_mutex);
			pump_exited = true;
			return; // No normal queue flush after FINISH_ONLY.
		}
		if (wait_parked) {
			// A cooperative pool yield could run an unrelated synchronous
			// getter on the parked pump stack and prevent its eventual resume.
			// Keep this lease wait non-cooperative and bounded; notifications
			// still wake RUN waits and control is checked at most 1 ms later.
			OS::get_singleton()->delay_usec(1000);
			continue;
		}
		if (flush) {
			{
				Scope owner(this, ScopeKind::OWNER);
				command_queue.flush_all();
			}
			// Recheck immediately after the whole flush, before any yield.
			MutexLock lock(control_mutex);
			if (control_action != ControlAction::RUN) {
				continue;
			}
		}
		WorkerThreadPool::get_singleton()->yield();
		// Read control before consulting doing_sync on every wake.
	}
}

void PhysicsServer3DWrapMT::_thread_sync() {
#ifdef TESTS_ENABLED
	_test_point(TestPoint::BARRIER_ENTER);
#endif
	doing_sync.set();
}

void PhysicsServer3DWrapMT::_acquire_park() {
	uint64_t serial;
	bool overflow = false;
	{
		MutexLock lock(control_mutex);
		if (sync_lease) {
			return;
		}
		if (control_action != ControlAction::PARK) {
			overflow = requested_serial == UINT64_MAX;
			if (!overflow) {
				requested_serial++;
				control_action = ControlAction::PARK;
			}
		}
		serial = requested_serial;
	}
	CRASH_COND_MSG(overflow, "Physics wrapper park serial exhausted.");
	_notify_pump();
	for (;;) {
		{
			MutexLock lock(control_mutex);
			if (parked && acknowledged_serial == serial) {
				sync_lease = true;
				main_dispatch_authority = true;
				return;
			}
		}
		OS::get_singleton()->delay_usec(1000);
	}
}

void PhysicsServer3DWrapMT::_request_finish() {
	bool wake = false;
	{
		MutexLock lock(control_mutex);
		if (stop_state == StopState::OPEN) {
			stop_state = StopState::REQUESTED;
			wake = true;
		}
	}
	// A request is not PARK. In particular a normal FIFO barrier may still
	// be outstanding, and must be allowed to complete before PARK is sent.
	if (wake) {
		_notify_pump();
	}
}

void PhysicsServer3DWrapMT::_drain_publishers() {
	for (;;) {
		command_queue.flush_all();
#ifdef TESTS_ENABLED
		_test_point(TestPoint::DRAIN_AFTER_FLUSH);
#endif
		bool empty;
		{
			MutexLock lock(control_mutex);
			empty = publishers == 0;
		}
		if (!empty) {
			OS::get_singleton()->delay_usec(1000);
			continue;
		}
		// A publisher may have staged after the preceding empty flush but
		// before dropping its ticket. Flush that accepted asynchronous tail.
		command_queue.flush_all();
		{
			MutexLock lock(control_mutex);
			if (publishers == 0) {
				return;
			}
		}
	}
}

void PhysicsServer3DWrapMT::_service_stop() {
	if (!Thread::is_main_thread()) {
		return;
	}
	{
		MutexLock lock(control_mutex);
		if (stop_state != StopState::REQUESTED || main_depth != 0) {
			return;
		}
		if (!initialized) {
			release_sealed = true;
			stop_state = StopState::CLOSED;
			return;
		}
		stop_state = StopState::DRAINING;
	}
	if (create_thread) {
		_acquire_park();
	}
	{
		Scope drain(this, ScopeKind::DRAIN);
		_drain_publishers();
		bool end_backend_sync;
		{
			MutexLock lock(control_mutex);
			end_backend_sync = backend_sync_active;
			backend_sync_active = false;
		}
		if (end_backend_sync) {
			physics_server_3d->end_sync();
			// end_sync can release holders whose cleanup produces more frees.
			_drain_publishers();
		}
		for (;;) {
			{
				MutexLock lock(control_mutex);
				if (publishers == 0) {
					// Ticket zero and release sealing share one linearization
					// point. No cleanup can reopen publication after transfer.
					release_sealed = true;
					main_dispatch_authority = false;
					sync_lease = false;
					doing_sync.clear();
					control_action = ControlAction::FINISH_ONLY;
					break;
				}
			}
			_drain_publishers();
		}
	}
	if (create_thread) {
		_notify_pump();
		WorkerThreadPool::TaskID task;
		{
			MutexLock lock(control_mutex);
			task = server_task_id;
		}
		const Error error = WorkerThreadPool::get_singleton()->wait_for_task_completion(task);
		ERR_FAIL_COND_MSG(error != OK, "Physics pump task did not join.");
		command_queue.set_pump_task_id(WorkerThreadPool::INVALID_TASK_ID);
	} else {
		Scope finishing(this, ScopeKind::FINISH);
		physics_server_3d->finish();
	}
	{
		MutexLock lock(control_mutex);
		server_task_id = WorkerThreadPool::INVALID_TASK_ID;
		server_thread = Thread::MAIN_ID;
		stop_state = StopState::CLOSED;
	}
}

void PhysicsServer3DWrapMT::step(real_t p_step) {
	Scope scope(this);
	if (!scope) {
		return;
	}
	if (create_thread) {
		command_queue.push(physics_server_3d, &PhysicsServer3D::step, p_step);
	} else {
		Scope owner(this, ScopeKind::OWNER);
		physics_server_3d->step(p_step);
	}
}

void PhysicsServer3DWrapMT::sync() {
	_service_stop();
#ifdef TESTS_ENABLED
	_test_point(TestPoint::BOUNDARY_ENTRY);
#endif
	Scope scope(this);
	if (!scope) {
		// A foreign request can race the entry service and admission.
		_service_stop();
		return;
	}
	ERR_FAIL_COND(!Thread::is_main_thread());
	bool invalid_sync;
	{
		MutexLock lock(control_mutex);
		invalid_sync = sync_in_progress || backend_sync_active;
		if (!invalid_sync) {
			sync_in_progress = true;
		}
	}
	ERR_FAIL_COND_MSG(invalid_sync, "Physics sync is already active.");
	if (create_thread) {
// Revision 3: do not publish PARK until this exact FIFO barrier has
// completed its dispatched payload cleanup and returned to Main.
#ifdef TESTS_ENABLED
		_test_point(TestPoint::BEFORE_BARRIER);
#endif
		command_queue.push_and_sync(this, &PhysicsServer3DWrapMT::_thread_sync);
#ifdef TESTS_ENABLED
		_test_point(TestPoint::AFTER_BARRIER);
#endif
		_acquire_park();
	} else {
		Scope owner(this, ScopeKind::OWNER);
		command_queue.flush_all();
	}
	{
		MutexLock lock(control_mutex);
		backend_sync_active = true;
	}
	physics_server_3d->sync();
	{
		MutexLock lock(control_mutex);
		sync_in_progress = false;
	}
}

void PhysicsServer3DWrapMT::flush_queries() {
	_service_stop();
#ifdef TESTS_ENABLED
	_test_point(TestPoint::BOUNDARY_ENTRY);
#endif
	bool nested;
	{
		MutexLock lock(control_mutex);
		nested = query_active;
	}
	ERR_FAIL_COND_MSG(nested, "Physics queries are already being flushed.");
	Scope scope(this);
	if (!scope) {
		// A foreign request can race the entry service and admission.
		_service_stop();
		return;
	}
	ERR_FAIL_COND(!Thread::is_main_thread());
	bool owns_backend;
	{
		MutexLock lock(control_mutex);
		owns_backend = !create_thread || sync_lease;
		if (owns_backend) {
			query_active = true;
		}
	}
	ERR_FAIL_COND_MSG(!owns_backend, "Physics query flush requires an active sync phase.");
	physics_server_3d->flush_queries();
	{
		MutexLock lock(control_mutex);
		query_active = false;
	}
}

void PhysicsServer3DWrapMT::end_sync() {
	_service_stop();
#ifdef TESTS_ENABLED
	_test_point(TestPoint::BOUNDARY_ENTRY);
#endif
	Scope scope(this);
	if (!scope) {
		// A foreign request can race the entry service and admission.
		_service_stop();
		return;
	}
	ERR_FAIL_COND(!Thread::is_main_thread());
	{
		MutexLock lock(control_mutex);
		if (!backend_sync_active) {
			return;
		}
		backend_sync_active = false;
	}
	physics_server_3d->end_sync();
	bool resume = false;
	{
		MutexLock lock(control_mutex);
		if (create_thread && stop_state == StopState::OPEN) {
			doing_sync.clear();
			released_serial = requested_serial;
			sync_lease = false;
			main_dispatch_authority = false;
			parked = false;
			control_action = ControlAction::RUN;
			resume = true;
		}
	}
	if (resume) {
		_notify_pump();
	}
}

void PhysicsServer3DWrapMT::init() {
	_service_stop();
	Scope scope(this);
	if (!scope) {
		// A foreign request can race the entry service and admission.
		_service_stop();
		return;
	}
	ERR_FAIL_COND(!Thread::is_main_thread());
	{
		MutexLock lock(control_mutex);
		if (initialized) {
			return;
		}
		initialized = true;
	}
	if (create_thread) {
		const WorkerThreadPool::TaskID task = WorkerThreadPool::get_singleton()->add_task(callable_mp(this, &PhysicsServer3DWrapMT::_thread_loop), true, "Physics server 3D pump task", true);
		{
			MutexLock lock(control_mutex);
			server_task_id = task;
		}
		command_queue.set_pump_task_id(task);
		command_queue.push(this, &PhysicsServer3DWrapMT::_assign_mt_ids, task);
		command_queue.push_and_sync(physics_server_3d, &PhysicsServer3D::init);
	} else {
		{
			MutexLock lock(control_mutex);
			server_thread = Thread::MAIN_ID;
		}
		Scope owner(this, ScopeKind::OWNER);
		physics_server_3d->init();
	}
}

void PhysicsServer3DWrapMT::finish() {
	_request_finish();
	_service_stop();
}

void PhysicsServer3DWrapMT::free_rid(RID p_rid) {
	const bool cleanup = _has_release_ancestor(true);
	Scope release(this, ScopeKind::RELEASE);
	if (!release) {
		if (!cleanup) {
			ERR_PRINT("Physics server is stopping.");
		}
		return;
	}
	_dispatch_async(&PhysicsServer3D::free_rid, p_rid);
}

PhysicsServer3DWrapMT::PhysicsServer3DWrapMT(PhysicsServer3D *p_contained, bool p_create_thread) {
	physics_server_3d = p_contained;
	create_thread = p_create_thread;
}

PhysicsServer3DWrapMT::~PhysicsServer3DWrapMT() {
	memdelete(physics_server_3d);
}
