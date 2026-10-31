/**************************************************************************/
/*  test_qualification_physical.cpp                                       */
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

#include "core/crypto/crypto_core.h"
#include "core/extension/gdextension_qualification_internal.h"
#include "core/object/callable_mp.h"
#include "core/os/os.h"
#include "core/os/semaphore.h"
#include "core/os/thread.h"
#include "servers/rendering/rendering_device.h"
#include "servers/rendering/rendering_server.h"
#include "tests/servers/rendering/transfer_test_driver_base.h"
#include "tests/test_macros.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>
#if defined(METAL_ENABLED) && defined(MACOS_ENABLED)
#include "drivers/metal/rendering_context_driver_metal.h"
#include "drivers/metal/rendering_device_driver_metal3.h"
#endif
TEST_FORCE_LINK(test_qualification_physical)
extern thread_local void (*test_uniform_set_after_rid_free)(RenderingDevice *);
class TestQualificationPhysicalAccess {
public:
	// These tests use synthetic cells; this helper scopes only the cell already
	// attached to the fixture RID and does not assert a production owner event.
	static void free_qualified_fixture(RenderingDevice &device, RID rid) {
		auto *buffer = device.storage_buffer_owner.get_or_null(rid);
		auto *shader = device.shader_owner.get_or_null(rid);
		auto *pipeline = device.compute_pipeline_owner.get_or_null(rid);
		auto *texture = device.texture_owner.get_or_null(rid);
		auto *sampler = device.sampler_owner.get_or_null(rid);
		GDExtensionDeviceExit::QualificationPhysicalReturn *cell = nullptr;
		if (buffer && buffer->qualification_return) {
			cell = buffer->qualification_return;
		} else if (shader && shader->qualification_return) {
			cell = shader->qualification_return;
		} else if (pipeline && pipeline->qualification_return) {
			cell = pipeline->qualification_return;
		} else if (texture && texture->qualification_return) {
			cell = texture->qualification_return;
		} else if (sampler && sampler->qualification_return) {
			cell = sampler->qualification_return;
		}
		auto *previous = device.qualification_retiring_cell;
		device.qualification_retiring_cell = cell;
		device.free_rid(rid);
		device.qualification_retiring_cell = previous;
	}
	struct InvalidationState {
		RID old_rid;
		RID replacement;
		int callbacks = 0;
		bool old_valid_during_callback = true;
		RenderingDevice *device = nullptr;
	};
	inline static thread_local InvalidationState *invalidation_state = nullptr;
	static void replace_freed_uniform_set(RenderingDevice *device) {
		test_uniform_set_after_rid_free = nullptr;
		RenderingDevice::UniformSet replacement;
		invalidation_state->replacement = device->uniform_set_owner.make_rid(replacement);
	}
	static void original_invalidation(void *userdata) {
		auto *state = static_cast<InvalidationState *>(userdata);
		++state->callbacks;
		state->old_valid_during_callback = state->device->uniform_set_owner.owns(state->old_rid);
	}
	static void invalidation_after_slot_reuse(RenderingDevice &device) {
		InvalidationState state;
		state.device = &device;
		RenderingDevice::UniformSet original;
		original.invalidated_callback = original_invalidation;
		original.invalidated_callback_userdata = &state;
		state.old_rid = device.uniform_set_owner.make_rid(original);
		device.render_thread_id = Thread::get_caller_id();
		invalidation_state = &state;
		test_uniform_set_after_rid_free = replace_freed_uniform_set;
		device.free_rid(state.old_rid);
		test_uniform_set_after_rid_free = nullptr;
		invalidation_state = nullptr;
		CHECK(state.replacement.is_valid());
		CHECK(uint32_t(state.replacement.get_id()) == uint32_t(state.old_rid.get_id()));
		CHECK(state.callbacks == 1);
		CHECK_FALSE(state.old_valid_during_callback);
		CHECK(device.uniform_set_owner.owns(state.replacement));
		device.uniform_set_owner.free(state.replacement);
	}
	struct FramebufferInvalidationState {
		RenderingDevice *device = nullptr;
		RID rid;
		RID replacement;
		int callbacks = 0;
		bool valid_during_callback = true;
		Thread::ID callback_thread = Thread::UNASSIGNED_ID;
	};
	static void framebuffer_invalidation_reenter(void *userdata) {
		auto *state = static_cast<FramebufferInvalidationState *>(userdata);
		state->callbacks++;
		state->callback_thread = Thread::get_caller_id();
		state->valid_during_callback = state->device->framebuffer_is_valid(state->rid);
		if (state->callbacks == 1 && state->valid_during_callback) {
			state->device->free_rid(state->rid);
		} else if (state->callbacks == 1) {
			RenderingDevice::Framebuffer replacement{};
			state->replacement = state->device->framebuffer_owner.make_rid(replacement);
		}
	}
	static void framebuffer_invalidation_once(RenderingDevice &device) {
		FramebufferInvalidationState state;
		state.device = &device;
		RenderingDevice::Framebuffer framebuffer{};
		framebuffer.invalidated_callback = framebuffer_invalidation_reenter;
		framebuffer.invalidated_callback_userdata = &state;
		state.rid = device.framebuffer_owner.make_rid(framebuffer);
		device.render_thread_id = Thread::get_caller_id();
		ERR_PRINT_OFF;
		device.free_rid(state.rid);
		ERR_PRINT_ON;
		CHECK(state.callbacks == 1);
		CHECK_FALSE(state.valid_during_callback);
		CHECK(state.callback_thread == Thread::get_caller_id());
		CHECK_FALSE(device.framebuffer_owner.owns(state.rid));
		CHECK(device.frames[0].framebuffers_to_dispose_of.size() == 1);
		CHECK(state.replacement.is_valid());
		CHECK(uint32_t(state.replacement.get_id()) == uint32_t(state.rid.get_id()));
		CHECK(device.framebuffer_owner.owns(state.replacement));
		if (state.replacement.is_valid()) {
			device.framebuffer_owner.free(state.replacement);
		}
	}
	struct CascadeInvalidationState {
		RenderingDevice *device = nullptr;
		RID parent;
		int callbacks = 0;
		bool reenter = false;
	};
	static void cascade_invalidation(void *userdata) {
		auto *state = static_cast<CascadeInvalidationState *>(userdata);
		state->callbacks++;
		if (state->reenter) {
			state->device->free_rid(state->parent);
			if (!state->device->texture_owner.owns(state->parent)) {
				std::fputs("RD_CASCADE_PARENT_FREED_BEFORE_OUTER_RETURN\n", stderr);
				std::fflush(stderr);
				std::_Exit(86);
			}
		}
	}
	static void cascade_reentry(RenderingDevice &device, bool reenter) {
		CascadeInvalidationState state;
		state.device = &device;
		state.reenter = reenter;
		RenderingDevice::Texture texture{};
		state.parent = device.texture_owner.make_rid(texture);
		RenderingDevice::Framebuffer framebuffer{};
		framebuffer.invalidated_callback = cascade_invalidation;
		framebuffer.invalidated_callback_userdata = &state;
		RID child = device.framebuffer_owner.make_rid(framebuffer);
		device.render_thread_id = Thread::get_caller_id();
		device._add_dependency(child, state.parent);
		device.free_rid(state.parent);
		CHECK(state.callbacks == 1);
		CHECK_FALSE(device.texture_owner.owns(state.parent));
		CHECK_FALSE(device.framebuffer_owner.owns(child));
		CHECK(device.frames[0].textures_to_dispose_of.size() == 1);
		CHECK(device.frames[0].framebuffers_to_dispose_of.size() == 1);
		CHECK(device.resource_frees_in_progress.is_empty());
	}
	struct SiblingInvalidationState {
		RenderingDevice *device = nullptr;
		RID first;
		RID second;
		int callbacks = 0;
	};
	static void sibling_invalidation(void *userdata) {
		auto *state = static_cast<SiblingInvalidationState *>(userdata);
		state->callbacks++;
		if (state->callbacks == 1) {
			RID sibling = state->device->framebuffer_is_valid(state->first) ? state->first : state->second;
			if (state->device->framebuffer_is_valid(sibling)) {
				state->device->free_rid(sibling);
			}
		}
	}
	static void cascade_sibling_reentry(RenderingDevice &device) {
		SiblingInvalidationState state;
		state.device = &device;
		RenderingDevice::Texture texture{};
		RID parent = device.texture_owner.make_rid(texture);
		RenderingDevice::Framebuffer framebuffer{};
		framebuffer.invalidated_callback = sibling_invalidation;
		framebuffer.invalidated_callback_userdata = &state;
		state.first = device.framebuffer_owner.make_rid(framebuffer);
		state.second = device.framebuffer_owner.make_rid(framebuffer);
		device.render_thread_id = Thread::get_caller_id();
		device._add_dependency(state.first, parent);
		device._add_dependency(state.second, parent);
		device.free_rid(parent);
		CHECK(state.callbacks == 2);
		CHECK_FALSE(device.texture_owner.owns(parent));
		CHECK_FALSE(device.framebuffer_owner.owns(state.first));
		CHECK_FALSE(device.framebuffer_owner.owns(state.second));
		CHECK(device.frames[0].textures_to_dispose_of.size() == 1);
		CHECK(device.frames[0].framebuffers_to_dispose_of.size() == 2);
		CHECK(device.resource_frees_in_progress.is_empty());
	}
	struct LateBusyState {
		RenderingDevice *device = nullptr;
		RID child;
		int callbacks = 0;
	};
	static void mark_parent_busy(void *userdata) {
		auto *state = static_cast<LateBusyState *>(userdata);
		state->callbacks++;
		state->device->texture_owner.get_or_null(state->child)->qualification_users = 1;
	}
	static void cascade_late_busy_retry(RenderingDevice &device) {
		LateBusyState state;
		state.device = &device;
		RenderingDevice::Texture texture{};
		RID parent = device.texture_owner.make_rid(texture);
		state.child = device.texture_owner.make_rid(texture);
		RenderingDevice::Framebuffer framebuffer{};
		framebuffer.invalidated_callback = mark_parent_busy;
		framebuffer.invalidated_callback_userdata = &state;
		RID grandchild = device.framebuffer_owner.make_rid(framebuffer);
		device.render_thread_id = Thread::get_caller_id();
		device._add_dependency(state.child, parent);
		device._add_dependency(grandchild, state.child);
		ERR_PRINT_OFF;
		device.free_rid(parent);
		ERR_PRINT_ON;
		CHECK(state.callbacks == 1);
		CHECK(device.texture_owner.owns(parent));
		CHECK(device.texture_owner.owns(state.child));
		CHECK_FALSE(device.framebuffer_owner.owns(grandchild));
		const HashSet<RID> *children = device.dependency_map.getptr(parent);
		CHECK((children && children->has(state.child)));
		CHECK(device.resource_frees_in_progress.is_empty());
		device.texture_owner.get_or_null(state.child)->qualification_users = 0;
		if (device.texture_owner.owns(parent)) {
			device.free_rid(parent);
		} else {
			device.free_rid(state.child);
		}
		CHECK_FALSE(device.texture_owner.owns(parent));
		CHECK_FALSE(device.texture_owner.owns(state.child));
		CHECK(device.frames[0].textures_to_dispose_of.size() == 2);
		CHECK(device.frames[0].framebuffers_to_dispose_of.size() == 1);
		CHECK(device.resource_frees_in_progress.is_empty());
	}
	static void cascade_refused_pipeline_enqueue(RenderingDevice &device) {
		GDExtensionDeviceExit::QualificationPhysicalReturn cell;
		RenderingDevice::Shader shader{};
		RID parent = device.shader_owner.make_rid(shader);
		RenderingDevice::ComputePipeline pipeline{};
		pipeline.shader = parent;
		pipeline.qualification_return = &cell;
		RID child = device.compute_pipeline_owner.make_rid(pipeline);
		cell.rid = child.get_id();
		device.qualification_retiring_cell = &cell;
		device.render_thread_id = Thread::get_caller_id();
		device._add_dependency(child, parent);
		ERR_PRINT_OFF;
		device.free_rid(parent);
		ERR_PRINT_ON;
		CHECK(device.shader_owner.owns(parent));
		CHECK(device.compute_pipeline_owner.owns(child));
		const HashSet<RID> *children = device.dependency_map.getptr(parent);
		CHECK((children && children->has(child)));
		CHECK_FALSE(cell.enqueued);
		CHECK(device.resource_frees_in_progress.is_empty());
		// Close this synthetic cell to exercise a subsequent queue admission.
		// No native return or owner acknowledgment is asserted here.
		cell.use_closed.store(true);
		if (device.shader_owner.owns(parent)) {
			device.free_rid(parent);
		} else {
			device.free_rid(child);
		}
		CHECK_FALSE(device.shader_owner.owns(parent));
		CHECK_FALSE(device.compute_pipeline_owner.owns(child));
		CHECK(cell.enqueued);
		CHECK(cell.free_event == 0);
	}
	static void pipeline_owner_and_enqueue_guard(RenderingDevice &device, int mode) {
		GDExtensionDeviceExit::QualificationPhysicalReturn cell;
		GDExtensionDeviceExit::QualificationPhysicalReturn other_cell;
		GDExtensionDeviceExit::QualificationTicket ticket;
		cell.use_closed.store(true);
		if (mode == 1) {
			cell.uses.push_back(&ticket); // Synthetic unretired ticket only.
		} else if (mode == 2) {
			cell.dependent_children.store(1);
		} else if (mode == 3) {
			cell.enqueued = true;
		} else if (mode == 4) {
			device.frames[0].qualification_epoch = 0;
		} else if (mode == 5) {
			device.next_qualification_free_event = 0;
		} else if (mode == 6) {
			cell.reserved_event = 42;
		}
		RenderingDevice::Shader shader{};
		RID parent = device.shader_owner.make_rid(shader);
		RenderingDevice::ComputePipeline pipeline{};
		pipeline.shader = parent;
		pipeline.qualification_return = &cell;
		RID child = device.compute_pipeline_owner.make_rid(pipeline);
		cell.rid = child.get_id();
		device.qualification_retiring_cell = mode <= 2 ? &other_cell : &cell;
		device.render_thread_id = Thread::get_caller_id();
		device._add_dependency(child, parent);
		ERR_PRINT_OFF;
		device.free_rid(parent);
		ERR_PRINT_ON;
		CHECK(device.shader_owner.owns(parent));
		CHECK(device.compute_pipeline_owner.owns(child));
		const HashSet<RID> *children = device.dependency_map.getptr(parent);
		CHECK((children && children->has(child)));
		CHECK(cell.enqueued == (mode == 3));
		CHECK(device.resource_frees_in_progress.is_empty());
		// Synthetic fixture cleanup does not attest to any native return.
		cell.uses.clear();
		cell.dependent_children.store(0);
		if (device.compute_pipeline_owner.owns(child)) {
			device.compute_pipeline_owner.get_or_null(child)->qualification_return = nullptr;
		}
		if (device.shader_owner.owns(parent)) {
			device.free_rid(parent);
		} else if (device.compute_pipeline_owner.owns(child)) {
			device.free_rid(child);
		}
	}
	static void pipeline_direct_owner_guard(RenderingDevice &device, int mode) {
		using namespace GDExtensionDeviceExit;
		auto *bridge = memnew(Registry::QualificationBridge);
		auto *unit = memnew(Registry::QualificationBridge::NativeUnit);
		QualificationTicket ticket;
		bridge->units[0] = unit;
		unit->serial = 31;
		unit->retiring = true;
		unit->backing.known_mask = 3;
		unit->report.report_revision = 1;
		unit->pipeline.use_closed.store(true);
		unit->shader.use_closed.store(true);
		if (mode == 1) {
			unit->pipeline.uses.push_back(&ticket);
		} else {
			unit->pipeline.dependent_children.store(1);
		}
		RenderingDevice::Shader shader{};
		shader.qualification_return = &unit->shader;
		RID parent = device.shader_owner.make_rid(shader);
		RenderingDevice::ComputePipeline pipeline{};
		pipeline.shader = parent;
		pipeline.qualification_return = &unit->pipeline;
		RID child = device.compute_pipeline_owner.make_rid(pipeline);
		unit->shader.rid = parent.get_id();
		unit->pipeline.rid = child.get_id();
		device.render_thread_id = Thread::get_caller_id();
		device._add_dependency(child, parent);
		device.qualification_retiring = false;
		device.draw_graph.initialize(device.driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
		device.draw_graph.begin();
		Registry::get_singleton().qualification_retire_step(bridge, &device, 31, 2);
		CHECK(device.shader_owner.owns(parent));
		CHECK(device.compute_pipeline_owner.owns(child));
		CHECK_FALSE(unit->shader.enqueued);
		CHECK_FALSE(unit->pipeline.enqueued);
		CHECK(unit->report.reason == 4);
		CHECK(unit->report.report_revision == 2);
		unit->pipeline.uses.clear();
		unit->pipeline.dependent_children.store(0);
		Registry::get_singleton().qualification_retire_step(bridge, &device, 31, 2);
		CHECK_FALSE(device.compute_pipeline_owner.owns(child));
		CHECK(unit->pipeline.enqueued);
		CHECK(unit->pipeline.free_event == 0);
		CHECK(device.shader_owner.owns(parent));
		CHECK_FALSE(device.qualification_retiring);
		CHECK(device.qualification_retiring_cell == nullptr);
		if (device.shader_owner.owns(parent)) {
			device.shader_owner.get_or_null(parent)->qualification_return = nullptr;
		}
		if (device.compute_pipeline_owner.owns(child)) {
			device.compute_pipeline_owner.get_or_null(child)->qualification_return = nullptr;
		}
		if (device.shader_owner.owns(parent)) {
			device.free_rid(parent);
		} else if (device.compute_pipeline_owner.owns(child)) {
			device.free_rid(child);
		}
		device.frames[0].compute_pipelines_to_dispose_of.clear();
		memdelete(unit);
		memdelete(bridge);
	}
	static void pipeline_scoped_target(RenderingDevice &device, bool wrong_rid) {
		GDExtensionDeviceExit::QualificationPhysicalReturn cell;
		GDExtensionDeviceExit::QualificationPhysicalReturn other;
		cell.use_closed.store(true);
		RenderingDevice::ComputePipeline pipeline{};
		pipeline.qualification_return = &cell;
		RID rid = device.compute_pipeline_owner.make_rid(pipeline);
		cell.rid = wrong_rid ? 0 : rid.get_id();
		device.render_thread_id = Thread::get_caller_id();
		device.qualification_retiring_cell = wrong_rid ? &cell : &other;
		ERR_PRINT_OFF;
		device.free_rid(rid);
		ERR_PRINT_ON;
		CHECK(device.compute_pipeline_owner.owns(rid));
		CHECK_FALSE(cell.enqueued);
		cell.rid = rid.get_id();
		device.qualification_retiring_cell = &cell;
		device.free_rid(rid);
		CHECK_FALSE(device.compute_pipeline_owner.owns(rid));
		CHECK(cell.enqueued);
		CHECK(cell.free_event == 0);
		device.qualification_retiring_cell = nullptr;
		device.frames[0].compute_pipelines_to_dispose_of.clear();
	}
	static void pipeline_scope_restore(RenderingDevice &device) {
		using namespace GDExtensionDeviceExit;
		QualificationPhysicalReturn outer;
		auto *bridge = memnew(Registry::QualificationBridge);
		auto *unit = memnew(Registry::QualificationBridge::NativeUnit);
		bridge->units[0] = unit;
		unit->serial = 31;
		unit->retiring = true;
		unit->backing.known_mask = 2;
		unit->report.report_revision = 1;
		unit->pipeline.use_closed.store(true);
		RenderingDevice::ComputePipeline pipeline{};
		pipeline.qualification_return = &unit->pipeline;
		RID rid = device.compute_pipeline_owner.make_rid(pipeline);
		unit->pipeline.rid = rid.get_id();
		device.render_thread_id = Thread::get_caller_id();
		device.qualification_retiring = true;
		device.qualification_retiring_cell = &outer;
		device.draw_graph.initialize(device.driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
		device.draw_graph.begin();
		Registry::get_singleton().qualification_retire_step(bridge, &device, 31, 2);
		CHECK_FALSE(device.compute_pipeline_owner.owns(rid));
		CHECK(unit->pipeline.enqueued);
		CHECK(unit->pipeline.free_event == 0);
		CHECK(device.qualification_retiring);
		CHECK(device.qualification_retiring_cell == &outer);
		device.qualification_retiring_cell = nullptr;
		device.frames[0].compute_pipelines_to_dispose_of.clear();
		memdelete(unit);
		memdelete(bridge);
	}
	template <typename D>
	static void owner_driver_reentry(RenderingDevice &device, D &driver) {
		using namespace GDExtensionDeviceExit;
		auto *bridge = memnew(Registry::QualificationBridge);
		auto *unit = memnew(Registry::QualificationBridge::NativeUnit);
		QualificationPhysicalReturn foreign_cell;
		QualificationTicket pending;
		bridge->units[0] = unit;
		unit->serial = 31;
		unit->retiring = true;
		unit->backing.known_mask = 1;
		unit->report.report_revision = 1;
		unit->shader.use_closed.store(true);
		RenderingDevice::Shader shader{};
		shader.driver_id = RenderingDeviceDriver::ShaderID(19);
		shader.qualification_return = &unit->shader;
		RID original = device.shader_owner.make_rid(shader);
		unit->shader.rid = original.get_id();
		RenderingDevice::Shader foreign_parent_shader{};
		RID foreign_parent = device.shader_owner.make_rid(foreign_parent_shader);
		RenderingDevice::ComputePipeline pipeline{};
		pipeline.shader = foreign_parent;
		pipeline.qualification_return = &foreign_cell;
		RID foreign = device.compute_pipeline_owner.make_rid(pipeline);
		foreign_cell.rid = foreign.get_id();
		foreign_cell.use_closed.store(true);
		foreign_cell.uses.push_back(&pending);
		device.render_thread_id = Thread::get_caller_id();
		device._add_dependency(foreign, foreign_parent);
		device.qualification_retiring = false;
		device.qualification_retiring_cell = nullptr;
		driver.polls = 1;
		driver.reentry_device = &device;
		driver.reentry_target = foreign;
		device.draw_graph.initialize(&driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
		device.draw_graph.begin();
		ERR_PRINT_OFF;
		Registry::get_singleton().qualification_retire_step(bridge, &device, 31, 2);
		ERR_PRINT_ON;
		CHECK(driver.reentry_checks == 2);
		CHECK_FALSE(device.shader_owner.owns(original));
		CHECK(unit->shader.enqueued);
		CHECK(unit->shader.free_event == 0);
		CHECK(device.compute_pipeline_owner.owns(foreign));
		CHECK_FALSE(foreign_cell.enqueued);
		const HashSet<RID> *children = device.dependency_map.getptr(foreign_parent);
		CHECK((children && children->has(foreign)));
		CHECK_FALSE(device.qualification_retiring);
		CHECK(device.qualification_retiring_cell == nullptr);
		// Release only synthetic fixture state; no physical completion is inferred.
		driver.reentry_device = nullptr;
		foreign_cell.uses.clear();
		if (device.compute_pipeline_owner.owns(foreign)) {
			device.compute_pipeline_owner.get_or_null(foreign)->qualification_return = nullptr;
		}
		if (device.shader_owner.owns(foreign_parent)) {
			device.free_rid(foreign_parent);
		}
		device.frames[0].shaders_to_dispose_of.clear();
		device.frames[0].compute_pipelines_to_dispose_of.clear();
		memdelete(unit);
		memdelete(bridge);
	}
	struct BusyDependentState {
		RenderingDevice *device = nullptr;
		RID parent;
		RID dependent;
		int callbacks = 0;
	};
	static void add_busy_dependent(void *userdata) {
		auto *state = static_cast<BusyDependentState *>(userdata);
		state->callbacks++;
		state->device->texture_owner.get_or_null(state->dependent)->qualification_users = 1;
		state->device->_add_dependency(state->dependent, state->parent);
	}
	static void cascade_busy_dependent_retry(RenderingDevice &device) {
		BusyDependentState state;
		state.device = &device;
		RenderingDevice::Texture texture{};
		state.parent = device.texture_owner.make_rid(texture);
		state.dependent = device.texture_owner.make_rid(texture);
		RenderingDevice::Framebuffer framebuffer{};
		framebuffer.invalidated_callback = add_busy_dependent;
		framebuffer.invalidated_callback_userdata = &state;
		RID first = device.framebuffer_owner.make_rid(framebuffer);
		device.render_thread_id = Thread::get_caller_id();
		device._add_dependency(first, state.parent);
		ERR_PRINT_OFF;
		device.free_rid(state.parent);
		ERR_PRINT_ON;
		CHECK(state.callbacks == 1);
		CHECK(device.texture_owner.owns(state.parent));
		CHECK(device.texture_owner.owns(state.dependent));
		CHECK_FALSE(device.framebuffer_owner.owns(first));
		const HashSet<RID> *children = device.dependency_map.getptr(state.parent);
		CHECK(children != nullptr);
		CHECK((children && children->has(state.dependent)));
		CHECK(device.resource_frees_in_progress.is_empty());
		device.texture_owner.get_or_null(state.dependent)->qualification_users = 0;
		device.free_rid(state.parent);
		CHECK_FALSE(device.texture_owner.owns(state.parent));
		CHECK_FALSE(device.texture_owner.owns(state.dependent));
		CHECK(device.frames[0].textures_to_dispose_of.size() == 2);
		CHECK(device.frames[0].framebuffers_to_dispose_of.size() == 1);
		CHECK(device.resource_frees_in_progress.is_empty());
	}
	template <typename D>
	static auto create_qualified_sampler(D &device, const RenderingDevice::SamplerState &state, GDExtensionDeviceExit::QualificationPhysicalReturn *cell, int) -> decltype(device._qualification_sampler_create(state, cell)) {
		return device._qualification_sampler_create(state, cell);
	}
	template <typename D>
	static RID create_qualified_sampler(D &, const RenderingDevice::SamplerState &, GDExtensionDeviceExit::QualificationPhysicalReturn *, long) { return RID(); }
	template <typename B>
	static auto fixture_sampler_child(B *bridge, int) -> decltype((void)sizeof(typename B::Sampler), static_cast<GDExtensionDeviceExit::QualificationPhysicalReturn *>(nullptr)) {
		auto *child = memnew(typename B::Sampler);
		child->serial = 31;
		child->object_serial = 43;
		child->physical.bridge = bridge->serial;
		child->physical.child = 31;
		child->physical.serial = 43;
		bridge->samplers[0] = child;
		return &child->physical;
	}
	template <typename B>
	static GDExtensionDeviceExit::QualificationPhysicalReturn *fixture_sampler_child(B *, long) { return nullptr; }
	template <typename B>
	static auto fixture_sampler_publish(B *bridge, RID rid, int) -> decltype((void)sizeof(typename B::Sampler)) {
		bridge->samplers[0]->rid = rid.get_id();
		bridge->samplers[0]->creating = false;
	}
	template <typename B>
	static void fixture_sampler_publish(B *, RID, long) {}
	template <typename B>
	static auto fixture_sampler_state(B *bridge, uint32_t mode, bool enabled, int) -> decltype((void)sizeof(typename B::Sampler)) {
		auto *child = bridge->samplers[0];
		if (mode == 1) {
			child->creating = enabled;
		} else if (mode == 2) {
			child->retiring = enabled;
		} else if (mode == 3) {
			child->acknowledged = enabled;
		} else if (mode == 4) {
			if (enabled) {
				++child->rid;
			} else {
				--child->rid;
			}
		} else if (mode == 5) {
			if (enabled) {
				++child->object_serial;
			} else {
				--child->object_serial;
			}
		}
	}
	template <typename B>
	static void fixture_sampler_state(B *, uint32_t, bool, long) {}
	template <typename B>
	static auto fixture_sampler_clear(B *bridge, int) -> decltype((void)sizeof(typename B::Sampler)) {
		memdelete(bridge->samplers[0]);
		bridge->samplers[0] = nullptr;
	}
	template <typename B>
	static void fixture_sampler_clear(B *, long) {}
	static void sampler_ticket_pin(RenderingDevice &device) {
		using namespace GDExtensionDeviceExit;
		// The bridge child is test-only enrollment; no production grant is issued.
		auto &registry = Registry::get_singleton();
		auto *bridge = memnew(Registry::QualificationBridge);
		bridge->serial = UINT64_MAX;
		auto *cell = fixture_sampler_child(bridge, 0);
		CHECK_MESSAGE(cell != nullptr, "The original bridge must retain a sampler child.");
		if (!cell) {
			memdelete(bridge);
			return;
		}
		uint32_t slot = 0;
		{
			MutexLock lock(registry.mutex);
			for (; slot < Registry::RECORD_CAPACITY && registry.qualification_bridges[slot]; ++slot) {
			}
			if (slot < Registry::RECORD_CAPACITY) {
				registry.qualification_bridges[slot] = bridge;
			}
		}
		CHECK(slot < Registry::RECORD_CAPACITY);
		if (slot == Registry::RECORD_CAPACITY) {
			fixture_sampler_clear(bridge, 0);
			memdelete(bridge);
			return;
		}
		RID rid = create_qualified_sampler(device, RenderingDevice::SamplerState(), cell, 0);
		CHECK(rid.is_valid());
		device.render_thread_id = Thread::get_caller_id();
		if (rid.is_valid()) {
			fixture_sampler_publish(bridge, rid, 0);
			QualificationTicket ticket;
			ticket.serial = 23;
			ticket.bridge_serial = bridge->serial;
			ticket.physically_retired = true;
			LocalVector<RID> resources;
			for (uint32_t i = 0; i < 129; ++i) {
				resources.push_back(rid);
			}
			Error pinned;
			auto reject = [&]() {Error status;{MutexLock registry_lock(registry.mutex);MutexLock rd_lock(device._thread_safe_);status=device._qualification_pin_resources(&ticket,resources);}CHECK(status==ERR_UNAUTHORIZED);CHECK(ticket.resource_pins.is_empty());CHECK(ticket.physical_uses.is_empty());CHECK(cell->uses.is_empty());CHECK(ticket.physically_retired.load()); };
			QualificationPhysicalReturn impostor;
			impostor.bridge = bridge->serial;
			impostor.child = 31;
			impostor.serial = 43;
			impostor.rid = rid.get_id();
			auto *sampler = device.sampler_owner.get_or_null(rid);
			sampler->qualification_return = &impostor;
			reject();
			sampler->qualification_return = cell;
			for (uint32_t mode = 1; mode <= 5; ++mode) {
				fixture_sampler_state(bridge, mode, true, 0);
				reject();
				fixture_sampler_state(bridge, mode, false, 0);
			}
			++cell->child;
			reject();
			--cell->child;
			++cell->rid;
			reject();
			--cell->rid;
			--ticket.bridge_serial;
			reject();
			++ticket.bridge_serial;
			cell->use_closed = true;
			reject();
			cell->use_closed = false;
			{
				MutexLock registry_lock(registry.mutex);
				MutexLock rd_lock(device._thread_safe_);
				pinned = device._qualification_pin_resources(&ticket, resources);
			}
			CHECK_MESSAGE(pinned == OK, "An original sampler child must be admitted once even when repeated in a batch.");
			if (pinned == OK) {
				CHECK(ticket.resource_pins.size() == 1);
				CHECK(ticket.physical_uses.size() == 1);
				CHECK(cell->uses.size() == 1);
				CHECK_FALSE(ticket.physically_retired.load());
				cell->use_closed = true;
				Error retry;
				{
					MutexLock registry_lock(registry.mutex);
					MutexLock rd_lock(device._thread_safe_);
					retry = device._qualification_pin_resources(&ticket, resources);
				}
				CHECK(retry == ERR_UNAUTHORIZED);
				CHECK(ticket.resource_pins.size() == 1);
				device.qualification_retiring = true;
				ERR_PRINT_OFF;
				free_qualified_fixture(device, rid);
				ERR_PRINT_ON;
				CHECK(device.sampler_owner.owns(rid));
				CHECK_FALSE(cell->enqueued);
				{
					MutexLock registry_lock(registry.mutex);
					MutexLock rd_lock(device._thread_safe_);
					device._qualification_unpin_resources(&ticket);
				}
				CHECK(ticket.resource_pins.is_empty());
				CHECK(cell->uses.size() == 1);
				// Fixture cleanup is not a ticket acknowledgment receipt.
				cell->uses.clear();
				ticket.physical_uses.clear();
			}
			cell->use_closed = true;
			device.qualification_retiring = true;
			free_qualified_fixture(device, rid);
			device._free_pending_resources(0);
			CHECK(cell->free_event != 0);
		}
		{
			MutexLock lock(registry.mutex);
			registry.qualification_bridges[slot] = nullptr;
		}
		fixture_sampler_clear(bridge, 0);
		memdelete(bridge);
	}
	static void sampler_original_return(RenderingDevice &device) {
		using namespace GDExtensionDeviceExit;
		QualificationPhysicalReturn cell;
		cell.bridge = 11;
		cell.child = 31;
		cell.serial = 43;
		RID rid = create_qualified_sampler(device, RenderingDevice::SamplerState(), &cell, 0);
		CHECK_MESSAGE(rid.is_valid(), "The original qualified sampler owner must produce a real sampler RID.");
		if (rid.is_null()) {
			return;
		}
		CHECK(cell.rid == rid.get_id());
		CHECK(cell.serial == 43);
		CHECK(cell.free_event == 0);
		device.render_thread_id = Thread::get_caller_id();
		device.qualification_retiring = false;
		ERR_PRINT_OFF;
		free_qualified_fixture(device, rid);
		ERR_PRINT_ON;
		CHECK(device.sampler_owner.owns(rid));
		CHECK_FALSE(cell.enqueued);
		cell.use_closed = true;
		device.qualification_retiring = true;
		free_qualified_fixture(device, rid);
		CHECK_FALSE(device.sampler_owner.owns(rid));
		CHECK(cell.enqueued);
		CHECK(cell.free_event == 0);
		device._free_pending_resources(0);
		CHECK(cell.free_event != 0);
		CHECK(cell.frame_epoch == 71);
	}
	static void sampler_legacy_and_parent_refusal(RenderingDevice &device) {
		using namespace GDExtensionDeviceExit;
		QualificationPhysicalReturn cell;
		cell.bridge = 11;
		cell.child = 31;
		cell.serial = 43;
		RID rid = create_qualified_sampler(device, RenderingDevice::SamplerState(), &cell, 0);
		REQUIRE(rid.is_valid());
		device.render_thread_id = Thread::get_caller_id();
		RenderingDevice::Shader shader;
		RID shader_rid = device.shader_owner.make_rid(shader);
		RenderingDevice::Uniform uniform;
		uniform.uniform_type = RenderingDevice::UNIFORM_TYPE_SAMPLER;
		uniform.append_id(rid);
		ERR_PRINT_OFF;
		const uint64_t raw = device.get_driver_resource(RenderingDevice::DRIVER_RESOURCE_SAMPLER, rid);
		RID set = device.uniform_set_create(VectorView<RenderingDevice::Uniform>(&uniform, 1), shader_rid, 0);
		ERR_PRINT_ON;
		CHECK(raw == 0);
		CHECK(set.is_null());
		CHECK_FALSE(cell.enqueued);
		device.shader_owner.free(shader_rid);
		RenderingDevice::Buffer buffer;
		buffer.driver_id = RenderingDeviceDriver::BufferID(19);
		buffer.draw_tracker = RDG::resource_tracker_create();
		RID parent = device.storage_buffer_owner.make_rid(buffer);
		device._add_dependency(rid, parent);
		cell.use_closed = true;
		device.qualification_retiring = true;
		ERR_PRINT_OFF;
		device.free_rid(parent);
		ERR_PRINT_ON;
		CHECK(device.storage_buffer_owner.owns(parent));
		CHECK(device.sampler_owner.owns(rid));
		CHECK_FALSE(cell.enqueued);
		free_qualified_fixture(device, rid);
		device.free_rid(parent);
		device._free_pending_resources(0);
		CHECK(cell.free_event != 0);
	}
	static void ordinary_sampler_lifecycle(RenderingDevice &device) {
		device.render_thread_id = Thread::get_caller_id();
		RID rid = device.sampler_create(RenderingDevice::SamplerState());
		REQUIRE(rid.is_valid());
		CHECK(device.get_driver_resource(RenderingDevice::DRIVER_RESOURCE_SAMPLER, rid) == 37);
		GDExtensionDeviceExit::QualificationTicket ticket;
		ticket.serial = 23;
		ticket.bridge_serial = 11;
		ticket.physically_retired = true;
		LocalVector<RID> resources;
		resources.push_back(rid);
		auto &registry = GDExtensionDeviceExit::Registry::get_singleton();
		Error pinned;
		{
			MutexLock registry_lock(registry.mutex);
			MutexLock rd_lock(device._thread_safe_);
			pinned = device._qualification_pin_resources(&ticket, resources);
		}
		CHECK(pinned == ERR_UNAUTHORIZED);
		CHECK(ticket.resource_pins.is_empty());
		CHECK(ticket.physical_uses.is_empty());
		if (pinned == OK) {
			MutexLock registry_lock(registry.mutex);
			MutexLock rd_lock(device._thread_safe_);
			device._qualification_unpin_resources(&ticket);
		}
		free_qualified_fixture(device, rid);
		CHECK_FALSE(device.sampler_owner.owns(rid));
		device._free_pending_resources(0);
	}
	static void sampler_refuses_suspended_recording(RenderingDevice &device) {
		using namespace GDExtensionDeviceExit;
		QualificationPhysicalReturn cell;
		cell.bridge = 11;
		cell.child = 31;
		cell.serial = 43;
		device.frame_wait_error = ERR_BUSY;
		ERR_PRINT_OFF;
		RID rid = create_qualified_sampler(device, RenderingDevice::SamplerState(), &cell, 0);
		ERR_PRINT_ON;
		CHECK(rid.is_null());
		CHECK(cell.rid == 0);
		device.frame_wait_error = OK;
		if (rid.is_valid()) {
			cell.use_closed = true;
			device.qualification_retiring = true;
			device.render_thread_id = Thread::get_caller_id();
			free_qualified_fixture(device, rid);
			device._free_pending_resources(0);
		}
	}
	template <typename D>
	static auto create_qualified_texture(D &device, const RenderingDevice::TextureFormat &format, GDExtensionDeviceExit::QualificationPhysicalReturn *cell, int) -> decltype(device._qualification_texture_create(format, RenderingDevice::TextureView(), cell)) {
		return device._qualification_texture_create(format, RenderingDevice::TextureView(), cell);
	}
	template <typename D>
	static RID create_qualified_texture(D &, const RenderingDevice::TextureFormat &, GDExtensionDeviceExit::QualificationPhysicalReturn *, long) { return RID(); }
	static void texture_original_return(RenderingDevice &device) {
		using namespace GDExtensionDeviceExit;
		QualificationPhysicalReturn cell;
		cell.bridge = 11;
		cell.child = 31;
		cell.serial = 43;
		RenderingDevice::TextureFormat format;
		format.format = RenderingDevice::DATA_FORMAT_R32G32B32A32_SFLOAT;
		format.texture_type = RenderingDevice::TEXTURE_TYPE_2D;
		format.width = format.height = 2;
		format.depth = format.array_layers = format.mipmaps = 1;
		format.samples = RenderingDevice::TEXTURE_SAMPLES_1;
		format.usage_bits = RenderingDevice::TEXTURE_USAGE_SAMPLING_BIT | RenderingDevice::TEXTURE_USAGE_CAN_UPDATE_BIT;
		RID rid = create_qualified_texture(device, format, &cell, 0);
		CHECK_MESSAGE(rid.is_valid(), "The original qualified texture owner must produce a real texture RID.");
		if (rid.is_null()) {
			return;
		}
		CHECK(cell.rid == rid.get_id());
		CHECK(cell.serial == 43);
		CHECK(cell.free_event == 0);
		device.render_thread_id = Thread::get_caller_id();
		device.qualification_retiring = false;
		ERR_PRINT_OFF;
		free_qualified_fixture(device, rid);
		ERR_PRINT_ON;
		CHECK(device.texture_owner.owns(rid));
		CHECK_FALSE(cell.enqueued);
		cell.use_closed = true;
		device.qualification_retiring = true;
		free_qualified_fixture(device, rid);
		CHECK_FALSE(device.texture_owner.owns(rid));
		CHECK(cell.enqueued);
		CHECK(cell.free_event == 0);
		device._free_pending_resources(0);
		CHECK(cell.free_event != 0);
		CHECK(cell.frame_epoch == 71);
	}
	static void texture_rejects_unowned_legacy_operations(RenderingDevice &device, RenderingDeviceDriver &driver) {
		using namespace GDExtensionDeviceExit;
		QualificationPhysicalReturn cell;
		cell.bridge = 11;
		cell.child = 31;
		cell.serial = 43;
		RenderingDevice::TextureFormat format;
		format.format = RenderingDevice::DATA_FORMAT_R32G32B32A32_SFLOAT;
		format.texture_type = RenderingDevice::TEXTURE_TYPE_2D;
		format.width = format.height = 2;
		format.depth = format.array_layers = format.mipmaps = 1;
		format.samples = RenderingDevice::TEXTURE_SAMPLES_1;
		format.usage_bits = RenderingDevice::TEXTURE_USAGE_SAMPLING_BIT | RenderingDevice::TEXTURE_USAGE_CAN_UPDATE_BIT;
		RID rid = create_qualified_texture(device, format, &cell, 0);
		REQUIRE(rid.is_valid());
		device.render_thread_id = Thread::get_caller_id();
		ERR_PRINT_OFF;
		RID shared = device.texture_create_shared(RenderingDevice::TextureView(), rid);
		RID slice = device.texture_create_shared_from_slice(RenderingDevice::TextureView(), rid, 0, 0, 1, RenderingDevice::TEXTURE_SLICE_2D, 1);
		Vector<uint8_t> empty;
		Error update = device.texture_update(rid, 0, empty);
		Error clear = device.texture_clear(rid, Color(), 0, 1, 0, 1);
		Error copy = device.texture_copy(rid, rid, Vector3(), Vector3(), Vector3(1, 1, 1), 0, 0, 0, 0);
		Error resolve = device.texture_resolve_multisample(rid, rid);
		ERR_PRINT_ON;
		CHECK(shared.is_null());
		CHECK(slice.is_null());
		CHECK(update == ERR_UNAUTHORIZED);
		CHECK(clear == ERR_UNAUTHORIZED);
		CHECK(copy == ERR_UNAUTHORIZED);
		CHECK(resolve == ERR_UNAUTHORIZED);
		CHECK(device.texture_owner.owns(rid));
		CHECK_FALSE(cell.enqueued);
		cell.use_closed = true;
		device.qualification_retiring = true;
		free_qualified_fixture(device, rid);
		device._free_pending_resources(0);
		CHECK(cell.free_event != 0);
	}
	static void texture_rejects_driver_callback_before_prefix(RenderingDevice &device, RenderingDeviceDriver &driver) {
		using namespace GDExtensionDeviceExit;
		QualificationPhysicalReturn cell;
		cell.bridge = 11;
		cell.child = 31;
		cell.serial = 43;
		RenderingDevice::TextureFormat format;
		format.format = RenderingDevice::DATA_FORMAT_R32G32B32A32_SFLOAT;
		format.texture_type = RenderingDevice::TEXTURE_TYPE_2D;
		format.width = format.height = 2;
		format.depth = format.array_layers = format.mipmaps = 1;
		format.samples = RenderingDevice::TEXTURE_SAMPLES_1;
		format.usage_bits = RenderingDevice::TEXTURE_USAGE_SAMPLING_BIT | RenderingDevice::TEXTURE_USAGE_CAN_UPDATE_BIT;
		RID rid = create_qualified_texture(device, format, &cell, 0);
		REQUIRE(rid.is_valid());
		device.render_thread_id = Thread::get_caller_id();
		RenderingDevice::Buffer normal;
		normal.driver_id = RenderingDeviceDriver::BufferID(19);
		RID normal_rid = device.storage_buffer_owner.make_rid(normal);
		device.draw_graph.initialize(&driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
		device.draw_graph.begin();
		for (bool prefix : { false, true }) {
			Vector<RenderingDevice::CallbackResource> resources;
			if (prefix) {
				RenderingDevice::CallbackResource buffer;
				buffer.rid = normal_rid;
				buffer.type = RenderingDevice::CALLBACK_RESOURCE_TYPE_BUFFER;
				buffer.usage = RenderingDevice::CALLBACK_RESOURCE_USAGE_STORAGE_BUFFER_READ;
				resources.push_back(buffer);
			}
			RenderingDevice::CallbackResource texture;
			texture.rid = rid;
			texture.type = RenderingDevice::CALLBACK_RESOURCE_TYPE_TEXTURE;
			texture.usage = RenderingDevice::CALLBACK_RESOURCE_USAGE_TEXTURE_SAMPLE;
			resources.push_back(texture);
			Error result = device.driver_callback_add(nullptr, nullptr, resources);
			CHECK(result == ERR_UNAUTHORIZED);
			CHECK_FALSE(device.draw_graph.has_driver_callbacks());
			CHECK(device.storage_buffer_owner.get_or_null(normal_rid)->draw_tracker == nullptr);
			device.draw_graph.begin();
			auto *ordinary = device.storage_buffer_owner.get_or_null(normal_rid);
			if (ordinary->draw_tracker) {
				RDG::resource_tracker_free(ordinary->draw_tracker);
				ordinary->draw_tracker = nullptr;
			}
		}
		device.storage_buffer_owner.free(normal_rid);
		cell.use_closed = true;
		device.qualification_retiring = true;
		free_qualified_fixture(device, rid);
		device._free_pending_resources(0);
		CHECK(cell.free_event != 0);
	}
	static void texture_worker_refusal_preserves_dependencies(RenderingDevice &device) {
		using namespace GDExtensionDeviceExit;
		QualificationPhysicalReturn cell;
		cell.bridge = 11;
		cell.child = 31;
		cell.serial = 43;
		RenderingDevice::TextureFormat format;
		format.format = RenderingDevice::DATA_FORMAT_R32G32B32A32_SFLOAT;
		format.texture_type = RenderingDevice::TEXTURE_TYPE_2D;
		format.width = format.height = 2;
		format.depth = format.array_layers = format.mipmaps = 1;
		format.samples = RenderingDevice::TEXTURE_SAMPLES_1;
		format.usage_bits = RenderingDevice::TEXTURE_USAGE_SAMPLING_BIT | RenderingDevice::TEXTURE_USAGE_CAN_UPDATE_BIT;
		RID rid = create_qualified_texture(device, format, &cell, 0);
		REQUIRE(rid.is_valid());
		device.render_thread_id = Thread::get_caller_id();
		RenderingDevice::Buffer buffer;
		buffer.driver_id = RenderingDeviceDriver::BufferID(19);
		buffer.draw_tracker = RDG::resource_tracker_create();
		RID dependent = device.storage_buffer_owner.make_rid(buffer);
		device._add_dependency(dependent, rid);
		auto *texture = device.texture_owner.get_or_null(rid);
		texture->transfer_worker_index = 0;
		cell.use_closed = true;
		device.qualification_retiring = true;
		ERR_PRINT_OFF;
		free_qualified_fixture(device, rid);
		ERR_PRINT_ON;
		CHECK(device.texture_owner.owns(rid));
		CHECK(device.storage_buffer_owner.owns(dependent));
		CHECK_FALSE(cell.enqueued);
		CHECK(cell.free_event == 0);
		texture = device.texture_owner.get_or_null(rid);
		texture->transfer_worker_index = -1;
		free_qualified_fixture(device, rid);
		device._free_pending_resources(0);
		CHECK(cell.free_event != 0);
	}
	static void texture_ticket_pin(RenderingDevice &device) {
		using namespace GDExtensionDeviceExit;
		auto &registry = Registry::get_singleton();
		auto *bridge = memnew(Registry::QualificationBridge);
		auto *child = memnew(Registry::QualificationBridge::Texture);
		bridge->serial = UINT64_MAX;
		child->serial = 31;
		child->object_serial = 43;
		child->physical.bridge = bridge->serial;
		child->physical.child = 31;
		child->physical.serial = 43;
		bridge->textures[0] = child;
		uint32_t slot = 0;
		{
			MutexLock lock(registry.mutex);
			for (; slot < Registry::RECORD_CAPACITY && registry.qualification_bridges[slot]; ++slot) {
			}
			if (slot < Registry::RECORD_CAPACITY) {
				registry.qualification_bridges[slot] = bridge;
			}
		}
		CHECK(slot < Registry::RECORD_CAPACITY);
		if (slot == Registry::RECORD_CAPACITY) {
			memdelete(child);
			memdelete(bridge);
			return;
		}
		auto &cell = child->physical;
		RenderingDevice::TextureFormat format;
		format.format = RenderingDevice::DATA_FORMAT_R32G32B32A32_SFLOAT;
		format.texture_type = RenderingDevice::TEXTURE_TYPE_2D;
		format.width = format.height = 2;
		format.depth = format.array_layers = format.mipmaps = 1;
		format.samples = RenderingDevice::TEXTURE_SAMPLES_1;
		format.usage_bits = RenderingDevice::TEXTURE_USAGE_SAMPLING_BIT | RenderingDevice::TEXTURE_USAGE_CAN_UPDATE_BIT;
		RID rid = create_qualified_texture(device, format, &cell, 0);
		CHECK(rid.is_valid());
		device.render_thread_id = Thread::get_caller_id();
		if (rid.is_valid()) {
			child->rid = rid.get_id();
			child->creating = false;
			QualificationTicket ticket;
			ticket.serial = 23;
			ticket.bridge_serial = bridge->serial;
			ticket.physically_retired = true;
			LocalVector<RID> resources;
			resources.push_back(rid);
			Error pinned;
			auto reject = [&]() {Error status;{MutexLock registry_lock(registry.mutex);MutexLock rd_lock(device._thread_safe_);status=device._qualification_pin_resources(&ticket,resources);}CHECK(status==ERR_UNAUTHORIZED);CHECK(ticket.resource_pins.is_empty());CHECK(ticket.physical_uses.is_empty());CHECK(cell.uses.is_empty());CHECK(ticket.physically_retired.load()); };
			QualificationPhysicalReturn impostor;
			impostor.bridge = bridge->serial;
			impostor.child = child->serial;
			impostor.serial = child->object_serial;
			impostor.rid = child->rid;
			auto *texture = device.texture_owner.get_or_null(rid);
			texture->qualification_return = &impostor;
			reject();
			texture->qualification_return = &cell;
			child->creating = true;
			reject();
			child->creating = false;
			child->retiring = true;
			reject();
			child->retiring = false;
			child->acknowledged = true;
			reject();
			child->acknowledged = false;
			++child->rid;
			reject();
			--child->rid;
			++child->object_serial;
			reject();
			--child->object_serial;
			++cell.child;
			reject();
			--cell.child;
			cell.use_closed = true;
			reject();
			cell.use_closed = false;
			--ticket.bridge_serial;
			reject();
			++ticket.bridge_serial;
			LocalVector<RID> repeated_resources;
			for (uint32_t i = 0; i < 129; ++i) {
				repeated_resources.push_back(rid);
			}
			{
				MutexLock registry_lock(registry.mutex);
				MutexLock rd_lock(device._thread_safe_);
				pinned = device._qualification_pin_resources(&ticket, repeated_resources);
			}
			CHECK(pinned == OK);
			if (pinned != OK) {
				MutexLock registry_lock(registry.mutex);
				MutexLock rd_lock(device._thread_safe_);
				pinned = device._qualification_pin_resources(&ticket, resources);
			}
			if (pinned == OK) {
				CHECK_FALSE(ticket.physically_retired.load());
				CHECK(ticket.resource_pins.size() == 1);
				CHECK(ticket.physical_uses.size() == 1);
				CHECK(cell.uses.size() == 1);
				cell.use_closed = true;
				Error repeated;
				{
					MutexLock registry_lock(registry.mutex);
					MutexLock rd_lock(device._thread_safe_);
					repeated = device._qualification_pin_resources(&ticket, resources);
				}
				CHECK(repeated == ERR_UNAUTHORIZED);
				CHECK(ticket.resource_pins.size() == 1);
				CHECK(ticket.physical_uses.size() == 1);
				CHECK(cell.uses.size() == 1);
				device.qualification_retiring = true;
				ERR_PRINT_OFF;
				free_qualified_fixture(device, rid);
				ERR_PRINT_ON;
				CHECK(device.texture_owner.owns(rid));
				CHECK_FALSE(cell.enqueued);
				{
					MutexLock registry_lock(registry.mutex);
					MutexLock rd_lock(device._thread_safe_);
					device._qualification_unpin_resources(&ticket);
				}
				CHECK(ticket.resource_pins.is_empty());
				CHECK(cell.uses.size() == 1);
				// The neighbor cleanup here is a synthetic fixture, not a release receipt.
				cell.uses.clear();
				ticket.physical_uses.clear();
			}
			cell.use_closed = true;
			device.qualification_retiring = true;
			free_qualified_fixture(device, rid);
			device._free_pending_resources(0);
			CHECK(cell.free_event != 0);
		}
		{
			MutexLock lock(registry.mutex);
			registry.qualification_bridges[slot] = nullptr;
		}
		memdelete(child);
		memdelete(bridge);
	}

	static void unregistered_texture_cell_cannot_pin(RenderingDevice &device) {
		using namespace GDExtensionDeviceExit;
		QualificationPhysicalReturn cell;
		cell.bridge = 11;
		cell.child = 31;
		cell.serial = 43;
		RenderingDevice::TextureFormat format;
		format.format = RenderingDevice::DATA_FORMAT_R32G32B32A32_SFLOAT;
		format.texture_type = RenderingDevice::TEXTURE_TYPE_2D;
		format.width = format.height = 2;
		format.depth = format.array_layers = format.mipmaps = 1;
		format.samples = RenderingDevice::TEXTURE_SAMPLES_1;
		format.usage_bits = RenderingDevice::TEXTURE_USAGE_SAMPLING_BIT | RenderingDevice::TEXTURE_USAGE_CAN_UPDATE_BIT;
		RID rid = create_qualified_texture(device, format, &cell, 0);
		REQUIRE(rid.is_valid());
		device.render_thread_id = Thread::get_caller_id();
		QualificationTicket ticket;
		ticket.serial = 23;
		ticket.bridge_serial = 11;
		ticket.physically_retired = true;
		LocalVector<RID> resources;
		resources.push_back(rid);
		auto &registry = Registry::get_singleton();
		Error pinned;
		{
			MutexLock registry_lock(registry.mutex);
			MutexLock rd_lock(device._thread_safe_);
			pinned = device._qualification_pin_resources(&ticket, resources);
		}
		CHECK(pinned == ERR_UNAUTHORIZED);
		if (pinned == OK) {
			MutexLock registry_lock(registry.mutex);
			MutexLock rd_lock(device._thread_safe_);
			device._qualification_unpin_resources(&ticket);
			cell.uses.clear();
			ticket.physical_uses.clear();
		}
		CHECK(ticket.resource_pins.is_empty());
		CHECK(ticket.physical_uses.is_empty());
		cell.use_closed = true;
		device.qualification_retiring = true;
		free_qualified_fixture(device, rid);
		device._free_pending_resources(0);
		CHECK(cell.free_event != 0);
	}
	static void ordinary_texture_cannot_be_qualification_pin(RenderingDevice &device) {
		using namespace GDExtensionDeviceExit;
		RenderingDevice::TextureFormat format;
		format.format = RenderingDevice::DATA_FORMAT_R32G32B32A32_SFLOAT;
		format.texture_type = RenderingDevice::TEXTURE_TYPE_2D;
		format.width = format.height = 2;
		format.depth = format.array_layers = format.mipmaps = 1;
		format.samples = RenderingDevice::TEXTURE_SAMPLES_1;
		format.usage_bits = RenderingDevice::TEXTURE_USAGE_SAMPLING_BIT | RenderingDevice::TEXTURE_USAGE_CAN_UPDATE_BIT;
		RID rid = device.texture_create(format, RenderingDevice::TextureView());
		REQUIRE(rid.is_valid());
		device.render_thread_id = Thread::get_caller_id();
		QualificationTicket ticket;
		ticket.serial = 23;
		ticket.bridge_serial = 11;
		ticket.physically_retired = true;
		LocalVector<RID> resources;
		resources.push_back(rid);
		auto &registry = Registry::get_singleton();
		Error pinned;
		{
			MutexLock registry_lock(registry.mutex);
			MutexLock rd_lock(device._thread_safe_);
			pinned = device._qualification_pin_resources(&ticket, resources);
		}
		CHECK(pinned == ERR_UNAUTHORIZED);
		if (pinned == OK) {
			MutexLock registry_lock(registry.mutex);
			MutexLock rd_lock(device._thread_safe_);
			device._qualification_unpin_resources(&ticket);
		}
		CHECK(ticket.resource_pins.is_empty());
		CHECK(ticket.physical_uses.is_empty());
		free_qualified_fixture(device, rid);
		device._free_pending_resources(0);
	}
	template <typename B>
	static auto fixture_texture_child(B *bridge, int) -> decltype((void)sizeof(typename B::Texture), static_cast<GDExtensionDeviceExit::QualificationPhysicalReturn *>(nullptr)) {
		auto *child = memnew(typename B::Texture);
		child->serial = 31;
		child->physical.bridge = 11;
		child->physical.child = 31;
		bridge->textures[0] = child;
		return &child->physical;
	}
	template <typename B>
	static GDExtensionDeviceExit::QualificationPhysicalReturn *fixture_texture_child(B *, long) { return nullptr; }
	template <typename B>
	static auto clear_fixture_texture_child(B *bridge, int) -> decltype((void)sizeof(typename B::Texture)) {
		memdelete(bridge->textures[0]);
		bridge->textures[0] = nullptr;
	}
	template <typename B>
	static void clear_fixture_texture_child(B *, long) {}
	static void texture_ticket_reverse_unlink(RenderingDevice &device, bool sampler = false) {
		using namespace GDExtensionDeviceExit;
		auto *registry = memnew(Registry);
		auto *previous = RenderingDevice::singleton;
		RenderingDevice::singleton = &device;
		device.is_main_instance = true;
		device.render_thread_id = Thread::get_caller_id();
		registry->instance = 71;
		auto &record = registry->records[0];
		record.owner_thread = Thread::get_caller_id();
		record.snapshot.library_generation = 1;
		record.private_refs = 4;
		auto &d = registry->device_records[0];
		d.object = &device;
		d.live = true;
		d.epoch = 1;
		struct Holder : HolderRetirement {
			Holder() : HolderRetirement(RecordRef{}) {}
			~Holder() = default;
		} holder;
		holder.generation = 1;
		auto *bridge = memnew(Registry::QualificationBridge);
		bridge->serial = 11;
		bridge->epoch = 1;
		bridge->holder = &holder;
		bridge->children = 3;
		registry->qualification_bridges[0] = bridge;
		auto *cell = sampler ? fixture_sampler_child(bridge, 0) : fixture_texture_child(bridge, 0);
		CHECK_MESSAGE(cell != nullptr, "The original bridge must retain the selected child.");
		if (cell) {
			auto *first = memnew(QualificationTicket);
			auto *other = memnew(QualificationTicket);
			first->serial = 23;
			other->serial = 24;
			first->bridge_serial = other->bridge_serial = 11;
			first->physically_retired = true;
			other->physically_retired = false;
			bridge->tickets[0] = first;
			bridge->tickets[1] = other;
			first->physical_uses.push_back(cell);
			other->physical_uses.push_back(cell);
			cell->uses.push_back(first);
			cell->uses.push_back(other);
			const auto result = registry->qualification_release({ 71, 11 }, { 71, 23 });
			CHECK(result.native_error == OK);
			if (result.native_error == OK) {
				CHECK(first->physical_uses.is_empty());
				CHECK(cell->uses.size() == 1);
				CHECK(cell->uses[0] == other);
				CHECK(other->physical_uses.size() == 1);
			}
			// Fixture teardown does not claim retirement of the synthetic neighbor.
			cell->uses.clear();
			first->physical_uses.clear();
			other->physical_uses.clear();
			bridge->tickets[0] = bridge->tickets[1] = nullptr;
			memdelete(first);
			memdelete(other);
			if (sampler) {
				fixture_sampler_clear(bridge, 0);
			} else {
				clear_fixture_texture_child(bridge, 0);
			}
		}
		registry->qualification_bridges[0] = nullptr;
		RenderingDevice::singleton = previous;
		device.is_main_instance = false;
		memdelete(bridge);
		memdelete(registry);
	}
	static void method_children_keep_bridge_cleanup_credits() {
		using namespace GDExtensionDeviceExit;
		// Fixture-only children verify that pending original method resources are
		// included in the same bridge credit lifetime; no producer is implied.
		for (uint32_t mode = 0; mode < 2; ++mode) {
			auto *registry = memnew(Registry);
			registry->records[0].private_refs = 3;
			auto *bridge = memnew(Registry::QualificationBridge);
			bridge->serial = 11;
			bridge->cleanup_refs_reserved = true;
			auto *cell = mode ? fixture_sampler_child(bridge, 0) : fixture_texture_child(bridge, 0);
			REQUIRE(cell != nullptr);
			{
				MutexLock lock(registry->mutex);
				registry->qualification_trim_cleanup_ref_credits_locked(bridge);
			}
			CHECK_MESSAGE(bridge->cleanup_refs_reserved, "A live method child must retain its bridge cleanup credits.");
			CHECK(registry->records[0].private_refs == 3);
			if (mode) {
				bridge->samplers[0]->acknowledged = true;
			} else {
				bridge->textures[0]->acknowledged = true;
			}
			if (bridge->cleanup_refs_reserved) {
				MutexLock lock(registry->mutex);
				registry->qualification_trim_cleanup_ref_credits_locked(bridge);
			}
			CHECK_FALSE(bridge->cleanup_refs_reserved);
			CHECK(registry->records[0].private_refs == 1);
			if (mode) {
				fixture_sampler_clear(bridge, 0);
			} else {
				clear_fixture_texture_child(bridge, 0);
			}
			memdelete(bridge);
			memdelete(registry);
		}
	}
	static void texture_worker_descendant_does_not_livelock(RenderingDevice &device) {
		using namespace GDExtensionDeviceExit;
		QualificationPhysicalReturn cell;
		cell.bridge = 11;
		cell.child = 31;
		cell.serial = 43;
		RenderingDevice::TextureFormat format;
		format.format = RenderingDevice::DATA_FORMAT_R32G32B32A32_SFLOAT;
		format.texture_type = RenderingDevice::TEXTURE_TYPE_2D;
		format.width = format.height = 2;
		format.depth = format.array_layers = format.mipmaps = 1;
		format.samples = RenderingDevice::TEXTURE_SAMPLES_1;
		format.usage_bits = RenderingDevice::TEXTURE_USAGE_SAMPLING_BIT | RenderingDevice::TEXTURE_USAGE_CAN_UPDATE_BIT;
		RID rid = create_qualified_texture(device, format, &cell, 0);
		REQUIRE(rid.is_valid());
		device.render_thread_id = Thread::get_caller_id();
		RenderingDevice::Buffer buffer;
		buffer.driver_id = RenderingDeviceDriver::BufferID(19);
		buffer.draw_tracker = RDG::resource_tracker_create();
		RID parent = device.storage_buffer_owner.make_rid(buffer);
		device._add_dependency(rid, parent);
		auto *texture = device.texture_owner.get_or_null(rid);
		texture->transfer_worker_index = 0;
		cell.use_closed = true;
		device.qualification_retiring = true;
		ERR_PRINT_OFF;
		device.free_rid(parent);
		ERR_PRINT_ON;
		CHECK(device.texture_owner.owns(rid));
		CHECK(device.storage_buffer_owner.owns(parent));
		CHECK_FALSE(cell.enqueued);
		CHECK(cell.free_event == 0);
		texture->transfer_worker_index = -1;
		ERR_PRINT_OFF;
		device.free_rid(parent);
		ERR_PRINT_ON;
		CHECK(device.texture_owner.owns(rid));
		CHECK(device.storage_buffer_owner.owns(parent));
		CHECK_FALSE(cell.enqueued);
		if (device.texture_owner.owns(rid)) {
			free_qualified_fixture(device, rid);
		}
		if (device.storage_buffer_owner.owns(parent)) {
			device.free_rid(parent);
		}
		device._free_pending_resources(0);
		CHECK(cell.free_event != 0);
	}
	static void ticket_reverse_unlink(RenderingDevice &device, RenderingDeviceDriver &driver) {
		using namespace GDExtensionDeviceExit;
		auto *registry = memnew(Registry);
		auto *previous = RenderingDevice::singleton;
		RenderingDevice::singleton = &device;
		device.is_main_instance = true;
		device.render_thread_id = Thread::get_caller_id();
		registry->instance = 71;
		auto &record = registry->records[0];
		record.owner_thread = Thread::get_caller_id();
		record.snapshot.library_generation = 1;
		record.private_refs = 5;
		auto &d = registry->device_records[0];
		d.object = &device;
		d.live = true;
		d.epoch = 1;
		struct Holder : HolderRetirement {
			Holder() : HolderRetirement(RecordRef{}) {}
			~Holder() = default;
		} holder;
		holder.generation = 1;
		auto *bridge = memnew(Registry::QualificationBridge);
		bridge->serial = 11;
		bridge->epoch = 1;
		bridge->holder = &holder;
		bridge->children = 4;
		registry->qualification_bridges[0] = bridge;
		auto *storage = memnew(Registry::QualificationBridge::Storage);
		storage->serial = 31;
		storage->physical.bridge = 11;
		storage->physical.child = 31;
		bridge->storage[0] = storage;
		RID rid = device._qualification_storage_create(128, &storage->physical);
		REQUIRE(rid.is_valid());
		device.draw_graph.initialize(&driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
		device.draw_graph.begin();
		auto *unit = memnew(Registry::QualificationBridge::NativeUnit);
		unit->serial = 32;
		unit->shader.bridge = 11;
		unit->shader.child = 32;
		bridge->units[0] = unit;
		auto *first = memnew(QualificationTicket);
		auto *other = memnew(QualificationTicket);
		first->serial = 23;
		other->serial = 24;
		first->bridge_serial = other->bridge_serial = 11;
		first->physically_retired = true;
		other->physically_retired = false;
		bridge->tickets[0] = first;
		bridge->tickets[1] = other;
		first->physical_uses.push_back(&storage->physical);
		first->physical_uses.push_back(&unit->shader);
		other->physical_uses.push_back(&storage->physical);
		storage->physical.uses.push_back(first);
		storage->physical.uses.push_back(other);
		unit->shader.uses.push_back(first);
		const GDExtensionQualificationBridgeRefV1 ref{ 71, 11 };
		const GDExtensionQualificationTicketRefV1 ticket{ 71, 23 };
		first->accepted_rt_refs = 1;
		CHECK(registry->qualification_release(ref, ticket).native_error == ERR_BUSY);
		first->accepted_rt_refs = 0;
		first->frame_refs = 1;
		CHECK(registry->qualification_release(ref, ticket).native_error == ERR_BUSY);
		first->frame_refs = 0;
		first->queued = true;
		CHECK(registry->qualification_release(ref, ticket).native_error == ERR_BUSY);
		first->queued = false;
#ifdef THREADS_ENABLED
		std::atomic<bool> locked = false, done = false;
		std::thread worker([&] {device._thread_safe_.lock();locked=true;while(!done.load()){std::this_thread::yield();
}device._thread_safe_.unlock(); });
		while (!locked.load()) {
			std::this_thread::yield();
		}
		auto blocked = registry->qualification_release(ref, ticket);
		CHECK(blocked.native_error == ERR_BUSY);
		CHECK(blocked.reason == 19);
		CHECK_FALSE(first->acknowledged);
		CHECK(storage->physical.uses.size() == 2);
		CHECK(first->physical_uses.size() == 2);
		CHECK(unit->shader.uses.size() == 1);
		done = true;
		worker.join();
#endif
		if (!first->acknowledged) {
			REQUIRE(registry->qualification_release(ref, ticket).native_error == OK);
			CHECK(first->physical_uses.is_empty());
			CHECK(storage->physical.uses.size() == 1);
			CHECK(storage->physical.uses[0] == other);
			CHECK(unit->shader.uses.is_empty());
			CHECK(other->physical_uses.size() == 1);
			CHECK(bridge->children == 3);
			CHECK(record.private_refs == 4);
			CHECK(registry->qualification_release(ref, ticket).reason == 17);
			storage->retiring = true;
			storage->physical.use_closed = true;
			registry->qualification_retire_step(bridge, &device, 31, 1);
			CHECK(device.storage_buffer_owner.owns(rid));
			CHECK_FALSE(storage->physical.enqueued);
			CHECK(storage->report.reason == 3);
			other->physically_retired = true;
			registry->qualification_retire_step(bridge, &device, 31, 1);
			CHECK_FALSE(device.storage_buffer_owner.owns(rid));
			CHECK(storage->physical.enqueued);
			device._free_pending_resources(0);
			CHECK(storage->physical.free_event != 0);
		}
		registry->qualification_bridges[0] = nullptr;
		RenderingDevice::singleton = previous;
		device.is_main_instance = false;
		memdelete(first);
		memdelete(other);
		memdelete(storage);
		memdelete(unit);
		memdelete(bridge);
		memdelete(registry);
	}
	static void foreign_bridge_pin_is_rejected(RenderingDevice &device) {
		using namespace GDExtensionDeviceExit;
		auto *registry = memnew(Registry);
		auto *bridge = memnew(Registry::QualificationBridge);
		auto *foreign = memnew(Registry::QualificationBridge);
		bridge->serial = 11;
		foreign->serial = 12;
		registry->qualification_bridges[0] = bridge;
		registry->qualification_bridges[1] = foreign;
		auto *storage = memnew(Registry::QualificationBridge::Storage);
		storage->serial = 31;
		storage->physical.bridge = 12;
		storage->physical.child = 31;
		foreign->storage[0] = storage;
		RID rid = device._qualification_storage_create(128, &storage->physical);
		REQUIRE(rid.is_valid());
		QualificationTicket ticket;
		ticket.serial = 23;
		ticket.bridge_serial = 11;
		ticket.physically_retired = true;
		LocalVector<RID> resources;
		resources.push_back(rid);
		Error result;
		{
			MutexLock registry_lock(registry->mutex);
			MutexLock rd_lock(device._thread_safe_);
			result = device._qualification_pin_resources(&ticket, resources);
		}
		CHECK(result == ERR_UNAUTHORIZED);
		CHECK(ticket.resource_pins.is_empty());
		CHECK(ticket.physical_uses.is_empty());
		CHECK(storage->physical.uses.is_empty());
		CHECK(ticket.physically_retired.load());
		storage->physical.use_closed = true;
		device._free_internal(rid);
		device._free_pending_resources(0);
		registry->qualification_bridges[0] = registry->qualification_bridges[1] = nullptr;
		memdelete(storage);
		memdelete(foreign);
		memdelete(bridge);
		memdelete(registry);
	}
	static void ticket_empty_release_during_rd_contention(RenderingDevice &device) {
#ifdef THREADS_ENABLED
		using namespace GDExtensionDeviceExit;
		auto *registry = memnew(Registry);
		auto *previous = RenderingDevice::singleton;
		RenderingDevice::singleton = &device;
		device.is_main_instance = true;
		registry->instance = 71;
		auto &record = registry->records[0];
		record.owner_thread = Thread::get_caller_id();
		record.snapshot.library_generation = 1;
		record.private_refs = 2;
		auto &d = registry->device_records[0];
		d.object = &device;
		d.live = true;
		d.epoch = 1;
		struct Holder : HolderRetirement {
			Holder() : HolderRetirement(RecordRef{}) {}
			~Holder() = default;
		} holder;
		holder.generation = 1;
		auto *bridge = memnew(Registry::QualificationBridge);
		bridge->serial = 11;
		bridge->epoch = 1;
		bridge->holder = &holder;
		bridge->children = 1;
		registry->qualification_bridges[0] = bridge;
		auto *ticket = memnew(QualificationTicket);
		ticket->serial = 23;
		ticket->bridge_serial = 11;
		ticket->physically_retired = true;
		bridge->tickets[0] = ticket;
		std::atomic<bool> locked = false, done = false;
		std::thread worker([&] {device._thread_safe_.lock();locked=true;while(!done.load()){std::this_thread::yield();
}device._thread_safe_.unlock(); });
		while (!locked.load()) {
			std::this_thread::yield();
		}
		const auto result = registry->qualification_release({ 71, 11 }, { 71, 23 });
		CHECK(result.native_error == OK);
		CHECK(ticket->acknowledged);
		CHECK(bridge->children == 0);
		done = true;
		worker.join();
		registry->qualification_bridges[0] = nullptr;
		RenderingDevice::singleton = previous;
		device.is_main_instance = false;
		memdelete(ticket);
		memdelete(bridge);
		memdelete(registry);
#endif
	}
	static void ticket_relock_busy(RenderingDevice &device, RenderingDeviceDriver &driver, std::atomic<bool> &start, std::atomic<bool> &locked) {
#ifdef THREADS_ENABLED
		using namespace GDExtensionDeviceExit;
		auto *registry = memnew(Registry);
		auto *previous = RenderingDevice::singleton;
		RenderingDevice::singleton = &device;
		device.is_main_instance = true;
		device.driver = &driver;
		registry->instance = 71;
		auto &record = registry->records[0];
		record.owner_thread = Thread::get_caller_id();
		record.snapshot.library_generation = 1;
		record.private_refs = 3;
		auto &d = registry->device_records[0];
		d.object = &device;
		d.live = true;
		d.epoch = 1;
		struct Holder : HolderRetirement {
			Holder() : HolderRetirement(RecordRef{}) {}
			~Holder() = default;
		} holder;
		holder.generation = 1;
		auto *bridge = memnew(Registry::QualificationBridge);
		bridge->serial = 11;
		bridge->epoch = 1;
		bridge->holder = &holder;
		bridge->children = 2;
		registry->qualification_bridges[0] = bridge;
		auto *storage = memnew(Registry::QualificationBridge::Storage);
		storage->serial = 31;
		storage->physical.bridge = 11;
		storage->physical.child = 31;
		bridge->storage[0] = storage;
		auto *ticket = memnew(QualificationTicket);
		ticket->serial = 23;
		ticket->bridge_serial = 11;
		ticket->physically_retired = true;
		bridge->tickets[0] = ticket;
		ticket->physical_uses.push_back(&storage->physical);
		storage->physical.uses.push_back(ticket);
		RenderingDeviceDriver::NativeCallFact source;
		source.serial = 101;
		ticket->natives.push_back({ source, 0, 0, 0, 0, true, false });
		std::atomic<bool> done = false;
		std::thread worker([&] {while(!start.load() && !done.load()){std::this_thread::yield();
}if(done.load()){return;
}device._thread_safe_.lock();locked=true;while(!done.load()){std::this_thread::yield();
}device._thread_safe_.unlock(); });
		const auto blocked = registry->qualification_release({ 71, 11 }, { 71, 23 });
		CHECK(blocked.native_error == ERR_BUSY);
		CHECK(blocked.reason == 19);
		CHECK(ticket->natives[0].owner_released);
		CHECK_FALSE(ticket->acknowledged);
		CHECK(ticket->physical_uses.size() == 1);
		CHECK(storage->physical.uses.size() == 1);
		done = true;
		worker.join();
		if (!ticket->acknowledged) {
			CHECK(registry->qualification_release({ 71, 11 }, { 71, 23 }).native_error == OK);
			CHECK(ticket->physical_uses.is_empty());
			CHECK(storage->physical.uses.is_empty());
		}
		registry->qualification_bridges[0] = nullptr;
		RenderingDevice::singleton = previous;
		device.driver = nullptr;
		device.is_main_instance = false;
		memdelete(ticket);
		memdelete(storage);
		memdelete(bridge);
		memdelete(registry);
#endif
	}
	static void ticket_native_release_retry(RenderingDevice &device, RenderingDeviceDriver &driver) {
		using namespace GDExtensionDeviceExit;
		auto *registry = memnew(Registry);
		auto *previous = RenderingDevice::singleton;
		RenderingDevice::singleton = &device;
		device.is_main_instance = true;
		device.driver = &driver;
		registry->instance = 71;
		auto &record = registry->records[0];
		record.owner_thread = Thread::get_caller_id();
		record.snapshot.library_generation = 1;
		record.private_refs = 3;
		auto &d = registry->device_records[0];
		d.object = &device;
		d.live = true;
		d.epoch = 1;
		struct Holder : HolderRetirement {
			Holder() : HolderRetirement(RecordRef{}) {}
			~Holder() = default;
		} holder;
		holder.generation = 1;
		auto *bridge = memnew(Registry::QualificationBridge);
		bridge->serial = 11;
		bridge->epoch = 1;
		bridge->holder = &holder;
		bridge->children = 2;
		registry->qualification_bridges[0] = bridge;
		auto *storage = memnew(Registry::QualificationBridge::Storage);
		storage->serial = 31;
		storage->physical.bridge = 11;
		storage->physical.child = 31;
		bridge->storage[0] = storage;
		auto *ticket = memnew(QualificationTicket);
		ticket->serial = 23;
		ticket->bridge_serial = 11;
		ticket->physically_retired = true;
		bridge->tickets[0] = ticket;
		ticket->physical_uses.push_back(&storage->physical);
		storage->physical.uses.push_back(ticket);
		RenderingDeviceDriver::NativeCallFact a, b;
		a.serial = 101;
		b.serial = 102;
		ticket->natives.push_back({ a, 0, 0, 0, 0, true, false });
		ticket->natives.push_back({ b, 0, 0, 0, 0, true, false });
		auto first = registry->qualification_release({ 71, 11 }, { 71, 23 });
		CHECK(first.native_error == ERR_BUSY);
		CHECK(first.reason == 19);
		CHECK(ticket->natives[0].owner_released);
		CHECK_FALSE(ticket->natives[1].owner_released);
		CHECK_FALSE(ticket->acknowledged);
		CHECK(ticket->physical_uses.size() == 1);
		CHECK(storage->physical.uses.size() == 1);
		REQUIRE(registry->qualification_release({ 71, 11 }, { 71, 23 }).native_error == OK);
		CHECK(ticket->natives[1].owner_released);
		CHECK(ticket->physical_uses.is_empty());
		CHECK(storage->physical.uses.is_empty());
		registry->qualification_bridges[0] = nullptr;
		RenderingDevice::singleton = previous;
		device.driver = nullptr;
		device.is_main_instance = false;
		memdelete(ticket);
		memdelete(storage);
		memdelete(bridge);
		memdelete(registry);
	}
	static void ticket_bad_reverse_edges(RenderingDevice &device, uint32_t mode) {
		using namespace GDExtensionDeviceExit;
		auto *registry = memnew(Registry);
		auto *previous = RenderingDevice::singleton;
		RenderingDevice::singleton = &device;
		device.is_main_instance = true;
		registry->instance = 71;
		auto &record = registry->records[0];
		record.owner_thread = Thread::get_caller_id();
		record.snapshot.library_generation = 1;
		record.private_refs = 3;
		auto &d = registry->device_records[0];
		d.object = &device;
		d.live = true;
		d.epoch = 1;
		struct Holder : HolderRetirement {
			Holder() : HolderRetirement(RecordRef{}) {}
			~Holder() = default;
		} holder;
		holder.generation = 1;
		auto *bridge = memnew(Registry::QualificationBridge);
		bridge->serial = 11;
		bridge->epoch = 1;
		bridge->holder = &holder;
		bridge->children = 2;
		registry->qualification_bridges[0] = bridge;
		auto *storage = memnew(Registry::QualificationBridge::Storage);
		storage->serial = 31;
		storage->physical.bridge = 11;
		storage->physical.child = 31;
		bridge->storage[0] = storage;
		auto *ticket = memnew(QualificationTicket);
		ticket->serial = 23;
		ticket->bridge_serial = 11;
		ticket->physically_retired = true;
		bridge->tickets[0] = ticket;
		ticket->physical_uses.push_back(&storage->physical);
		storage->physical.uses.push_back(ticket);
		if (mode == 0) {
			ticket->physical_uses.push_back(&storage->physical);
		}
		if (mode == 1) {
			storage->physical.uses.push_back(ticket);
		}
		if (mode == 2) {
			storage->physical.uses.clear();
		}
		if (mode == 3) {
			ticket->physical_uses.push_back(reinterpret_cast<QualificationPhysicalReturn *>(uintptr_t(1)));
		}
		if (mode == 4) {
			ticket->physical_uses.clear();
		}
		const auto result = registry->qualification_release({ 71, 11 }, { 71, 23 });
		CHECK(result.native_error == ERR_INVALID_DATA);
		CHECK_FALSE(ticket->acknowledged);
		CHECK(bridge->children == 2);
		CHECK(record.private_refs == 3);
		CHECK(ticket->physical_uses.size() == (mode == 0 || mode == 3 ? 2 : mode == 4 ? 0
																					  : 1));
		CHECK(storage->physical.uses.size() == (mode == 1 ? 2 : mode == 2 ? 0
																		  : 1));
		registry->qualification_bridges[0] = nullptr;
		RenderingDevice::singleton = previous;
		device.is_main_instance = false;
		memdelete(ticket);
		memdelete(storage);
		memdelete(bridge);
		memdelete(registry);
	}
	static void storage_qrt_retire_reservation_survives_exhaustion(RenderingDevice &device, RenderingDeviceDriver &driver) {
		using namespace GDExtensionDeviceExit;
		// Fixture enrollment and sequence seeding are test-only. Real Registry entrypoints run below.
		auto *registry = memnew(Registry);
		auto *previous = RenderingDevice::singleton;
		RenderingDevice::singleton = &device;
		device.is_main_instance = true;
		device.render_thread_id = Thread::get_caller_id();
		registry->instance = 71;
		auto &record = registry->records[0];
		record.owner_thread = Thread::get_caller_id();
		record.snapshot.library_generation = 1;
		record.guard_enrollment = GuardEnrollment::QUALIFIED;
		record.private_refs = 1;
		auto &d = registry->device_records[0];
		d.object = &device;
		d.live = true;
		d.epoch = 1;
		d.execution_thread = Thread::get_caller_id();
		d.refs = 1;
		struct Holder : HolderRetirement {
			Holder() : HolderRetirement(RecordRef{}) {}
			~Holder() = default;
		} holder;
		holder.generation = 1;
		holder.qualification_children = 1;
		auto *bridge = memnew(Registry::QualificationBridge);
		bridge->serial = 11;
		bridge->epoch = 1;
		bridge->holder = &holder;
		registry->qualification_bridges[0] = bridge;
		const GDExtensionQualificationBridgeRefV1 ref{ 71, 11 };
		device.draw_graph.initialize(&driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
		device.draw_graph.begin();
		const GDExtensionQualificationRtConfigV1 config{ { 1, 40, 0 }, 1, 312, 200, 1, 256 };
		REQUIRE(registry->qualification_rt_configure(ref, &config).native_error == OK);
		const GDExtensionStorageCreateRequestV1 create{ { 1, 16, 0 }, 128, 3, 1, 41, 1, 0 };
		GDExtensionStorageBackingV1 backing = {};
		REQUIRE(registry->qualification_storage_create(ref, &create, &backing).native_error == OK);
		const GDExtensionStorageRetireRequestV1 retire{ { 1, 16, 0 }, backing.backing, backing.rid, backing.backing_serial, 3, 1, 41, 2, 0 };
		const GDExtensionQualificationRtRequestV1 header{ { 1, 56, 0 }, 3, 0, 1, 0, sizeof(retire), sizeof(GDExtensionStorageRetireRefV1) + sizeof(GDExtensionStorageRetireReportV1), 0, 0 };
		// Model the final unissued identity; a protected cleanup reservation must
		// prevent other admissions from consuming it before this request.
		registry->qualification_serials.next = UINT64_MAX;
		registry->qualification_serials.exhausted = false;
		GDExtensionQualificationRtRefV1 rt = {};
		const auto reserved = registry->qualification_rt_reserve(ref, &header, &retire, nullptr, &rt);
		CHECK_MESSAGE(reserved.native_error == OK, "An already accepted storage child must retain a QRT cleanup identity.");
		if (reserved.native_error == OK) {
			REQUIRE(rt.handle_serial != 0);
		}
		// Test teardown only: restore an unused range before releasing the
		// unactivated cell; exhausted retry and stale-ref semantics are separate.
		registry->qualification_serials.next = 101;
		registry->qualification_serials.exhausted = false;
		if (reserved.native_error == OK) {
			REQUIRE(registry->qualification_rt_release(ref, rt).native_error == OK);
		}
		GDExtensionStorageRetireRefV1 retired = {};
		GDExtensionStorageRetireReportV1 report = {};
		REQUIRE(registry->qualification_storage_retire(ref, &retire, &retired, &report).native_error == OK);
		device._free_pending_resources(0);
		auto *child = bridge->storage[0];
		REQUIRE(child != nullptr);
		registry->qualification_retire_step(bridge, &device, child->serial, 1);
		const GDExtensionQualificationBudgetV1 budget{ { 1, 16, 0 }, 1, 1000, 0 };
		REQUIRE(registry->qualification_storage_retire_observe(ref, retired, &budget, &report).native_error == OK);
		CHECK(report.state == 1);
		REQUIRE(registry->qualification_storage_retire_release(ref, retired).native_error == OK);
		REQUIRE(registry->qualification_close(ref).native_error == OK);
		CHECK(holder.qualification_children == 0);
		RenderingDevice::singleton = previous;
		device.is_main_instance = false;
		memdelete(registry);
	}
	static void qrt_cleanup_capacity_blocks_unrelated_admission(RenderingDevice &device, RenderingDeviceDriver &driver) {
		using namespace GDExtensionDeviceExit;
		auto *registry = memnew(Registry);
		auto *previous = RenderingDevice::singleton;
		RenderingDevice::singleton = &device;
		device.is_main_instance = true;
		device.render_thread_id = Thread::get_caller_id();
		registry->instance = 71;
		auto &record = registry->records[0];
		record.owner_thread = Thread::get_caller_id();
		record.snapshot.library_generation = 1;
		record.guard_enrollment = GuardEnrollment::QUALIFIED;
		record.private_refs = 1;
		auto &d = registry->device_records[0];
		d.object = &device;
		d.live = true;
		d.epoch = 1;
		d.execution_thread = Thread::get_caller_id();
		d.refs = 1;
		struct Holder : HolderRetirement {
			Holder() : HolderRetirement(RecordRef{}) {}
			~Holder() = default;
		} holder;
		holder.generation = 1;
		holder.qualification_children = 1;
		auto *bridge = memnew(Registry::QualificationBridge);
		bridge->serial = 11;
		bridge->epoch = 1;
		bridge->holder = &holder;
		registry->qualification_bridges[0] = bridge;
		const GDExtensionQualificationBridgeRefV1 ref{ 71, 11 };
		device.draw_graph.initialize(&driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
		device.draw_graph.begin();
		const GDExtensionStorageCreateRequestV1 create{ { 1, 16, 0 }, 128, 3, 1, 41, 1, 0 };
		GDExtensionStorageBackingV1 backing = {};
		REQUIRE(registry->qualification_storage_create(ref, &create, &backing).native_error == OK);
		// Configuration after creation must not leave this accepted child unprotected.
		const GDExtensionQualificationRtConfigV1 config{ { 1, 40, 0 }, 1, 312, 200, 1, 256 };
		REQUIRE(registry->qualification_rt_configure(ref, &config).native_error == OK);
		registry->qualification_serials.next = UINT64_MAX;
		registry->qualification_serials.exhausted = false;
		const uint64_t unrelated_request = backing.rid;
		const GDExtensionQualificationRtRequestV1 unrelated{ { 1, 56, 0 }, 1, 0, 1, 0, sizeof(unrelated_request), 48, 0, 0 };
		GDExtensionQualificationRtRefV1 unrelated_ref = {};
		auto blocked = registry->qualification_rt_reserve(ref, &unrelated, &unrelated_request, nullptr, &unrelated_ref);
		CHECK_MESSAGE(blocked.native_error == ERR_PARAMETER_RANGE_ERROR, "Unrelated admission must not consume an accepted child's final cleanup capacity.");
		if (blocked.native_error == OK) {
			REQUIRE(registry->qualification_rt_release(ref, unrelated_ref).native_error == OK);
		} else {
			const GDExtensionStorageRetireRequestV1 retire{ { 1, 16, 0 }, backing.backing, backing.rid, backing.backing_serial, 3, 1, 41, 2, 0 };
			const GDExtensionQualificationRtRequestV1 cleanup{ { 1, 56, 0 }, 3, 0, 1, 0, sizeof(retire), sizeof(GDExtensionStorageRetireRefV1) + sizeof(GDExtensionStorageRetireReportV1), 0, 0 };
			GDExtensionQualificationRtRefV1 live = {}, repeat = {};
			REQUIRE(registry->qualification_rt_reserve(ref, &cleanup, &retire, nullptr, &live).native_error == OK);
			CHECK(live.handle_serial == UINT64_MAX);
			CHECK(registry->qualification_rt_release(ref, live).native_error == ERR_BUSY);
			REQUIRE(registry->qualification_rt_reserve(ref, &cleanup, &retire, nullptr, &repeat).native_error == OK);
			CHECK(repeat.handle_serial == live.handle_serial);
			// Fixture teardown only; this is not a production sequence reset.
			registry->qualification_serials.next = 101;
			registry->qualification_serials.exhausted = false;
			REQUIRE(registry->qualification_rt_release(ref, live).native_error == OK);
		}
		// Fixture teardown only after an intentionally saturated issuer.
		registry->qualification_serials.next = 101;
		registry->qualification_serials.exhausted = false;
		const GDExtensionStorageRetireRequestV1 retire{ { 1, 16, 0 }, backing.backing, backing.rid, backing.backing_serial, 3, 1, 41, 2, 0 };
		GDExtensionStorageRetireRefV1 retired = {};
		GDExtensionStorageRetireReportV1 report = {};
		REQUIRE(registry->qualification_storage_retire(ref, &retire, &retired, &report).native_error == OK);
		device._free_pending_resources(0);
		auto *child = bridge->storage[0];
		REQUIRE(child != nullptr);
		registry->qualification_retire_step(bridge, &device, child->serial, 1);
		const GDExtensionQualificationBudgetV1 budget{ { 1, 16, 0 }, 1, 1000, 0 };
		REQUIRE(registry->qualification_storage_retire_observe(ref, retired, &budget, &report).native_error == OK);
		CHECK(report.state == 1);
		REQUIRE(registry->qualification_storage_retire_release(ref, retired).native_error == OK);
		REQUIRE(registry->qualification_close(ref).native_error == OK);
		RenderingDevice::singleton = previous;
		device.is_main_instance = false;
		memdelete(registry);
	}
	static void qrt_create_error_has_same_delivery_before_and_after_driver(RenderingDevice &device, RenderingDeviceDriver &driver, bool &fail_create, int &creates, bool post_driver, uint32_t existing_count = 0) {
		using namespace GDExtensionDeviceExit;
		auto *registry = memnew(Registry);
		auto *previous = RenderingDevice::singleton;
		RenderingDevice::singleton = &device;
		device.is_main_instance = true;
		device.render_thread_id = Thread::get_caller_id();
		registry->instance = 71;
		auto &record = registry->records[0];
		record.owner_thread = Thread::get_caller_id();
		record.snapshot.library_generation = 1;
		record.guard_enrollment = GuardEnrollment::QUALIFIED;
		record.private_refs = 1;
		auto &d = registry->device_records[0];
		d.object = &device;
		d.live = true;
		d.epoch = 1;
		d.execution_thread = Thread::get_caller_id();
		d.refs = 1;
		struct Holder : HolderRetirement {
			Holder() : HolderRetirement(RecordRef{}) {}
			~Holder() = default;
		} holder;
		holder.generation = 1;
		holder.qualification_children = 1;
		auto *bridge = memnew(Registry::QualificationBridge);
		bridge->serial = 11;
		bridge->epoch = 1;
		bridge->holder = &holder;
		registry->qualification_bridges[0] = bridge;
		const GDExtensionQualificationBridgeRefV1 ref{ 71, 11 };
		device.draw_graph.initialize(&driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
		device.draw_graph.begin();
		GDExtensionStorageBackingV1 existing_backing[2] = {};
		REQUIRE(existing_count <= 2);
		for (uint32_t i = 0; i < existing_count; ++i) {
			const GDExtensionStorageCreateRequestV1 existing_create{ { 1, 16, 0 }, 128, uint32_t(4 + i), 1, uint64_t(42 + i), 1, 0 };
			REQUIRE(registry->qualification_storage_create(ref, &existing_create, &existing_backing[i]).native_error == OK);
		}
		const GDExtensionQualificationRtConfigV1 config{ { 1, 40, 0 }, 1, 312, 200, 1, 256 };
		REQUIRE(registry->qualification_rt_configure(ref, &config).native_error == OK);
		const GDExtensionStorageCreateRequestV1 create{ { 1, 16, 0 }, 128, 3, 1, 41, 1, 0 };
		const GDExtensionQualificationRtRequestV1 header{ { 1, 56, 0 }, 2, 0, existing_count ? UINT64_MAX - existing_count : 1, 0, sizeof(create), sizeof(GDExtensionStorageBackingV1), 0, 0 };
		GDExtensionQualificationRtRefV1 rt = {};
		REQUIRE(registry->qualification_rt_reserve(ref, &header, &create, nullptr, &rt).native_error == OK);
		const int before = creates;
		fail_create = post_driver;
		device.frame_wait_error = post_driver ? OK : FAILED;
		bridge->rt->active = true;
		bridge->rt->state = 1;
		bridge->rt->queued = true;
		++bridge->children;
		++record.private_refs;
		registry->qualification_rt_dispatch(bridge->serial, rt.handle_serial);
		device.frame_wait_error = OK;
		fail_create = false;
		GDExtensionQualificationRtObservationV1 observed = {};
		alignas(8) uint8_t bytes[sizeof(GDExtensionStorageBackingV1)] = {};
		REQUIRE(registry->qualification_rt_observe(ref, rt, &observed, bytes, sizeof(bytes), nullptr, 0).native_error == OK);
		CHECK(observed.kind == 2);
		CHECK(observed.operation_serial == header.operation_serial);
		CHECK(observed.state == 2);
		CHECK(observed.result_bytes == sizeof(bytes));
		CHECK(observed.call_result.native_error == FAILED);
		CHECK(observed.call_result.reason == 16);
		for (uint8_t byte : bytes) {
			CHECK(byte == 0);
		}
		CHECK(creates == before + (post_driver ? 1 : 0));
		CHECK(bridge->storage[existing_count] == nullptr);
		REQUIRE(registry->qualification_rt_release(ref, rt).native_error == OK);
		GDExtensionQualificationRtRefV1 stale = {};
		CHECK(registry->qualification_rt_reserve(ref, &header, &create, nullptr, &stale).native_error == ERR_DOES_NOT_EXIST);
		for (uint32_t i = 0; i < existing_count; ++i) {
			const auto &backing = existing_backing[i];
			const GDExtensionStorageRetireRequestV1 retire{ { 1, 16, 0 }, backing.backing, backing.rid, backing.backing_serial, uint32_t(4 + i), 1, uint64_t(42 + i), 2, 0 };
			const GDExtensionQualificationRtRequestV1 retire_header{ { 1, 56, 0 }, 3, 0, UINT64_MAX - existing_count + i + 1, 0, sizeof(retire), sizeof(GDExtensionStorageRetireRefV1) + sizeof(GDExtensionStorageRetireReportV1), 0, 0 };
			GDExtensionQualificationRtRefV1 retire_rt = {};
			REQUIRE(registry->qualification_rt_reserve(ref, &retire_header, &retire, nullptr, &retire_rt).native_error == OK);
			bridge->rt->active = true;
			bridge->rt->state = 1;
			bridge->rt->queued = true;
			++bridge->children;
			++record.private_refs;
			registry->qualification_rt_dispatch(bridge->serial, retire_rt.handle_serial);
			alignas(8) uint8_t retire_bytes[sizeof(GDExtensionStorageRetireRefV1) + sizeof(GDExtensionStorageRetireReportV1)] = {};
			GDExtensionQualificationRtObservationV1 retire_observed = {};
			REQUIRE(registry->qualification_rt_observe(ref, retire_rt, &retire_observed, retire_bytes, sizeof(retire_bytes), nullptr, 0).native_error == OK);
			CHECK(retire_observed.operation_serial == retire_header.operation_serial);
			CHECK(retire_observed.state == 2);
			CHECK(retire_observed.call_result.native_error == OK);
			GDExtensionStorageRetireRefV1 retired = {};
			memcpy(&retired, retire_bytes, sizeof(retired));
			REQUIRE(retired.registry_instance == 71);
			REQUIRE(retired.handle_serial != 0);
			REQUIRE(registry->qualification_rt_release(ref, retire_rt).native_error == OK);
			device._free_pending_resources(0);
			auto *child = bridge->storage[i];
			REQUIRE(child != nullptr);
			registry->qualification_retire_step(bridge, &device, child->serial, 1);
			const GDExtensionQualificationBudgetV1 budget{ { 1, 16, 0 }, 1, 1000, 0 };
			GDExtensionStorageRetireReportV1 report = {};
			REQUIRE(registry->qualification_storage_retire_observe(ref, retired, &budget, &report).native_error == OK);
			CHECK(report.state == 1);
			REQUIRE(registry->qualification_storage_retire_release(ref, retired).native_error == OK);
		}
		REQUIRE(registry->qualification_close(ref).native_error == OK);
		RenderingDevice::singleton = previous;
		device.is_main_instance = false;
		memdelete(registry);
	}
	static void qrt_last_serial_delivers_original_retire(RenderingDevice &device, RenderingDeviceDriver &driver, bool fail_first = false) {
		using namespace GDExtensionDeviceExit;
		auto *registry = memnew(Registry);
		auto *previous = RenderingDevice::singleton;
		RenderingDevice::singleton = &device;
		device.is_main_instance = true;
		device.render_thread_id = Thread::get_caller_id();
		registry->instance = 71;
		auto &record = registry->records[0];
		record.owner_thread = Thread::get_caller_id();
		record.snapshot.library_generation = 1;
		record.guard_enrollment = GuardEnrollment::QUALIFIED;
		record.private_refs = 1;
		auto &d = registry->device_records[0];
		d.object = &device;
		d.live = true;
		d.epoch = 1;
		d.execution_thread = Thread::get_caller_id();
		d.refs = 1;
		struct Holder : HolderRetirement {
			Holder() : HolderRetirement(RecordRef{}) {}
			~Holder() = default;
		} holder;
		holder.generation = 1;
		holder.qualification_children = 1;
		auto *bridge = memnew(Registry::QualificationBridge);
		bridge->serial = 11;
		bridge->epoch = 1;
		bridge->holder = &holder;
		registry->qualification_bridges[0] = bridge;
		const GDExtensionQualificationBridgeRefV1 ref{ 71, 11 };
		device.draw_graph.initialize(&driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
		device.draw_graph.begin();
		const GDExtensionStorageCreateRequestV1 create{ { 1, 16, 0 }, 128, 3, 1, 41, 1, 0 };
		GDExtensionStorageBackingV1 backing = {};
		REQUIRE(registry->qualification_storage_create(ref, &create, &backing).native_error == OK);
		const GDExtensionQualificationRtConfigV1 config{ { 1, 40, 0 }, 1, 312, 200, 1, 256 };
		REQUIRE(registry->qualification_rt_configure(ref, &config).native_error == OK);
		const GDExtensionStorageRetireRequestV1 retire{ { 1, 16, 0 }, backing.backing, backing.rid, backing.backing_serial, 3, 1, 41, 2, 0 };
		const GDExtensionQualificationRtRequestV1 header{ { 1, 56, 0 }, 3, 0, 1, 0, sizeof(retire), sizeof(GDExtensionStorageRetireRefV1) + sizeof(GDExtensionStorageRetireReportV1), 0, 0 };
		registry->qualification_serials.next = UINT64_MAX;
		registry->qualification_serials.exhausted = false;
		GDExtensionQualificationRtRefV1 rt = {};
		REQUIRE(registry->qualification_rt_reserve(ref, &header, &retire, nullptr, &rt).native_error == OK);
		CHECK(rt.handle_serial == UINT64_MAX);
		alignas(8) uint8_t bytes[sizeof(GDExtensionStorageRetireRefV1) + sizeof(GDExtensionStorageRetireReportV1)] = {};
		GDExtensionQualificationRtObservationV1 observed = {};
		if (fail_first) {
			// The original dispatch fails admission before calling retirement. The
			// queue pin is a fixture enrollment, not a fabricated dispatch result.
			d.execution_thread = Thread::UNASSIGNED_ID;
			bridge->rt->active = true;
			bridge->rt->state = 1;
			bridge->rt->queued = true;
			++bridge->children;
			++record.private_refs;
			registry->qualification_rt_dispatch(bridge->serial, rt.handle_serial);
			REQUIRE(registry->qualification_rt_observe(ref, rt, &observed, bytes, sizeof(bytes), nullptr, 0).native_error == OK);
			CHECK(observed.state == 2);
			CHECK(observed.call_result.native_error == ERR_UNAUTHORIZED);
			auto *child = bridge->storage[0];
			REQUIRE(child != nullptr);
			CHECK_FALSE(child->retiring);
			CHECK_FALSE(child->physical.enqueued);
			d.execution_thread = Thread::get_caller_id();
			CHECK(registry->qualification_rt_release(ref, rt).native_error == ERR_BUSY);
			child->retiring = true;
			CHECK(registry->qualification_rt_activate(ref, rt).native_error == ERR_BUSY);
			CHECK(bridge->rt->observed);
			child->retiring = false;
			child->physical.use_closed = true;
			CHECK(registry->qualification_rt_activate(ref, rt).native_error == ERR_BUSY);
			CHECK(bridge->rt->observed);
			child->physical.use_closed = false;
			const uint32_t old_limit = registry->private_limit;
			registry->private_limit = record.private_refs;
			bridge->cleanup_queue_in_use = true;
			const auto resumed = registry->qualification_rt_activate(ref, rt);
			bridge->cleanup_queue_in_use = false;
			registry->private_limit = old_limit;
			CHECK_MESSAGE(resumed.native_error == OK, "An observed no-effect cleanup failure must retry on the same original reference.");
			if (resumed.native_error != OK) {
				// Fixture-only teardown after the failing arm.
				registry->qualification_serials.next = 101;
				registry->qualification_serials.exhausted = false;
				REQUIRE(registry->qualification_rt_release(ref, rt).native_error == OK);
				GDExtensionStorageRetireRefV1 retired = {};
				GDExtensionStorageRetireReportV1 report = {};
				REQUIRE(registry->qualification_storage_retire(ref, &retire, &retired, &report).native_error == OK);
				device._free_pending_resources(0);
				registry->qualification_retire_step(bridge, &device, child->serial, 1);
				const GDExtensionQualificationBudgetV1 budget{ { 1, 16, 0 }, 1, 1000, 0 };
				REQUIRE(registry->qualification_storage_retire_observe(ref, retired, &budget, &report).native_error == OK);
				REQUIRE(registry->qualification_storage_retire_release(ref, retired).native_error == OK);
				REQUIRE(registry->qualification_close(ref).native_error == OK);
				RenderingDevice::singleton = previous;
				device.is_main_instance = false;
				memdelete(registry);
				return;
			}
		}
		// Fixture-only dispatch enrollment; the following call executes the original
		// typed dispatch and physical-free path, not a synthetic success reply.
		bridge->rt->active = true;
		bridge->rt->state = 1;
		bridge->rt->queued = true;
		++bridge->children;
		++record.private_refs;
		registry->qualification_rt_dispatch(bridge->serial, rt.handle_serial);
		REQUIRE(registry->qualification_rt_observe(ref, rt, &observed, bytes, sizeof(bytes), nullptr, 0).native_error == OK);
		CHECK(observed.state == 2);
		CHECK(observed.call_result.native_error == OK);
		CHECK(observed.slot_generation == rt.handle_serial);
		CHECK(registry->qualification_rt_activate(ref, rt).native_error == ERR_BUSY);
		GDExtensionStorageRetireRefV1 retired = {};
		memcpy(&retired, bytes, sizeof(retired));
		REQUIRE(retired.registry_instance == 71);
		REQUIRE(retired.handle_serial != 0);
		REQUIRE(registry->qualification_rt_release(ref, rt).native_error == OK);
		device._free_pending_resources(0);
		auto *child = bridge->storage[0];
		REQUIRE(child != nullptr);
		registry->qualification_retire_step(bridge, &device, child->serial, 1);
		const GDExtensionQualificationBudgetV1 budget{ { 1, 16, 0 }, 1, 1000, 0 };
		GDExtensionStorageRetireReportV1 report = {};
		REQUIRE(registry->qualification_storage_retire_observe(ref, retired, &budget, &report).native_error == OK);
		CHECK(report.state == 1);
		REQUIRE(registry->qualification_storage_retire_release(ref, retired).native_error == OK);
		REQUIRE(registry->qualification_close(ref).native_error == OK);
		RenderingDevice::singleton = previous;
		device.is_main_instance = false;
		memdelete(registry);
	}
	static void qrt_cleanup_release_preserves_retry(RenderingDevice &device, RenderingDeviceDriver &driver, bool exhaust) {
		using namespace GDExtensionDeviceExit;
		// Private fixture enrollment and sequence seeding do not issue production authority.
		auto *registry = memnew(Registry);
		auto *previous = RenderingDevice::singleton;
		RenderingDevice::singleton = &device;
		device.is_main_instance = true;
		device.render_thread_id = Thread::get_caller_id();
		registry->instance = 71;
		auto &record = registry->records[0];
		record.owner_thread = Thread::get_caller_id();
		record.snapshot.library_generation = 1;
		record.guard_enrollment = GuardEnrollment::QUALIFIED;
		record.private_refs = 1;
		auto &d = registry->device_records[0];
		d.object = &device;
		d.live = true;
		d.epoch = 1;
		d.execution_thread = Thread::get_caller_id();
		d.refs = 1;
		struct Holder : HolderRetirement {
			Holder() : HolderRetirement(RecordRef{}) {}
			~Holder() = default;
		} holder;
		holder.generation = 1;
		holder.qualification_children = 1;
		auto *bridge = memnew(Registry::QualificationBridge);
		bridge->serial = 11;
		bridge->epoch = 1;
		bridge->holder = &holder;
		registry->qualification_bridges[0] = bridge;
		const GDExtensionQualificationBridgeRefV1 ref{ 71, 11 };
		device.draw_graph.initialize(&driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
		device.draw_graph.begin();
		const GDExtensionQualificationRtConfigV1 config{ { 1, 40, 0 }, 1, 312, 200, 1, 256 };
		REQUIRE(registry->qualification_rt_configure(ref, &config).native_error == OK);
		const GDExtensionStorageCreateRequestV1 create{ { 1, 16, 0 }, 128, 3, 1, 41, 1, 0 };
		GDExtensionStorageBackingV1 backing = {};
		REQUIRE(registry->qualification_storage_create(ref, &create, &backing).native_error == OK);
		const GDExtensionStorageRetireRequestV1 retire{ { 1, 16, 0 }, backing.backing, backing.rid, backing.backing_serial, 3, 1, 41, 2, 0 };
		GDExtensionQualificationRtRequestV1 header{ { 1, 56, 0 }, 3, 0, 1, 0, sizeof(retire), sizeof(GDExtensionStorageRetireRefV1) + sizeof(GDExtensionStorageRetireReportV1), 0, 0 };
		registry->qualification_serials.next = exhaust ? UINT64_MAX : 100;
		registry->qualification_serials.exhausted = false;
		GDExtensionQualificationRtRefV1 old_ref = {}, live = {};
		bool have_old = false;
		for (uint64_t attempt = 1; attempt <= 3; ++attempt) {
			header.operation_serial = attempt;
			GDExtensionQualificationRtRefV1 issued = {};
			const auto reserve = registry->qualification_rt_reserve(ref, &header, &retire, nullptr, &issued);
			CHECK_MESSAGE(reserve.native_error == OK, "A live storage child must retain an available cleanup route after an unactivated release.");
			if (reserve.native_error != OK) {
				break;
			}
			CHECK(issued.handle_serial != 0);
			live = issued;
			if (have_old) {
				CHECK(issued.handle_serial != old_ref.handle_serial);
				const auto stale = registry->qualification_rt_release(ref, old_ref);
				CHECK(stale.native_error != OK);
			}
			GDExtensionQualificationRtRefV1 exact = {};
			const auto repeated = registry->qualification_rt_reserve(ref, &header, &retire, nullptr, &exact);
			CHECK(repeated.native_error == OK);
			CHECK(exact.handle_serial == issued.handle_serial);
			const auto released = registry->qualification_rt_release(ref, issued);
			if (released.native_error != OK) {
				CHECK(released.native_error == ERR_BUSY);
				GDExtensionQualificationRtRefV1 retained = {};
				const auto retry = registry->qualification_rt_reserve(ref, &header, &retire, nullptr, &retained);
				CHECK(retry.native_error == OK);
				CHECK(retained.handle_serial == issued.handle_serial);
				break;
			}
			old_ref = issued;
			have_old = true;
			live = {};
		}
		// Test teardown only; the accepted QRT ref was never activated or sent to a driver.
		registry->qualification_serials.next = 101;
		registry->qualification_serials.exhausted = false;
		if (live.handle_serial) {
			auto released = registry->qualification_rt_release(ref, live);
			if (released.native_error != OK) {
				auto *cell = bridge->rt;
				REQUIRE((cell && cell->occupied && !cell->active && !cell->queued && !cell->executing && !cell->ticket));
				cell->occupied = false;
				--bridge->children;
				--record.private_refs; // fixture-only teardown
			}
		}
		GDExtensionStorageRetireRefV1 retired = {};
		GDExtensionStorageRetireReportV1 report = {};
		REQUIRE(registry->qualification_storage_retire(ref, &retire, &retired, &report).native_error == OK);
		device._free_pending_resources(0);
		auto *child = bridge->storage[0];
		REQUIRE(child != nullptr);
		registry->qualification_retire_step(bridge, &device, child->serial, 1);
		const GDExtensionQualificationBudgetV1 budget{ { 1, 16, 0 }, 1, 1000, 0 };
		REQUIRE(registry->qualification_storage_retire_observe(ref, retired, &budget, &report).native_error == OK);
		CHECK(report.state == 1);
		REQUIRE(registry->qualification_storage_retire_release(ref, retired).native_error == OK);
		REQUIRE(registry->qualification_close(ref).native_error == OK);
		CHECK(holder.qualification_children == 0);
		RenderingDevice::singleton = previous;
		device.is_main_instance = false;
		memdelete(registry);
	}
	static void qrt_conflicting_retire_waits_for_original_owner(RenderingDevice &device, RenderingDeviceDriver &driver) {
		using namespace GDExtensionDeviceExit;
		auto *registry = memnew(Registry);
		auto *previous = RenderingDevice::singleton;
		RenderingDevice::singleton = &device;
		device.is_main_instance = true;
		device.render_thread_id = Thread::get_caller_id();
		registry->instance = 71;
		auto &record = registry->records[0];
		record.owner_thread = Thread::get_caller_id();
		record.snapshot.library_generation = 1;
		record.guard_enrollment = GuardEnrollment::QUALIFIED;
		record.private_refs = 1;
		auto &d = registry->device_records[0];
		d.object = &device;
		d.live = true;
		d.epoch = 1;
		d.execution_thread = Thread::get_caller_id();
		d.refs = 1;
		struct Holder : HolderRetirement {
			Holder() : HolderRetirement(RecordRef{}) {}
			~Holder() = default;
		} holder;
		holder.generation = 1;
		holder.qualification_children = 1;
		auto *bridge = memnew(Registry::QualificationBridge);
		bridge->serial = 11;
		bridge->epoch = 1;
		bridge->holder = &holder;
		registry->qualification_bridges[0] = bridge;
		const GDExtensionQualificationBridgeRefV1 ref{ 71, 11 };
		device.draw_graph.initialize(&driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
		device.draw_graph.begin();
		GDExtensionStorageBackingV1 backing[2] = {};
		for (uint32_t i = 0; i < 2; ++i) {
			GDExtensionStorageCreateRequestV1 q{ { 1, 16, 0 }, 128, 3 + i, 1, 41, 1, 0 };
			REQUIRE(registry->qualification_storage_create(ref, &q, &backing[i]).native_error == OK);
		}
		const GDExtensionQualificationRtConfigV1 config{ { 1, 40, 0 }, 1, 312, 200, 1, 256 };
		REQUIRE(registry->qualification_rt_configure(ref, &config).native_error == OK);
		GDExtensionStorageRetireRequestV1 request[2] = { { { 1, 16, 0 }, backing[0].backing, backing[0].rid, backing[0].backing_serial, 3, 1, 41, 2, 0 }, { { 1, 16, 0 }, backing[1].backing, backing[1].rid, backing[1].backing_serial, 4, 1, 41, 2, 0 } };
		GDExtensionQualificationRtRequestV1 header{ { 1, 56, 0 }, 3, 0, 1, 0, sizeof(request[0]), sizeof(GDExtensionStorageRetireRefV1) + sizeof(GDExtensionStorageRetireReportV1), 0, 0 };
		registry->qualification_serials.next = UINT64_MAX - 1;
		registry->qualification_serials.exhausted = false;
		GDExtensionQualificationRtRefV1 first = {}, second = {};
		REQUIRE(registry->qualification_rt_reserve(ref, &header, &request[0], nullptr, &first).native_error == OK);
		auto direct = request[0];
		direct.retire_attempt = 3;
		GDExtensionStorageRetireRefV1 direct_ref = {};
		GDExtensionStorageRetireReportV1 report = {};
		REQUIRE(registry->qualification_storage_retire(ref, &direct, &direct_ref, &report).native_error == OK);
		// Fixture-only queue enrollment; dispatch and both owner retirements are real.
		bridge->rt->active = true;
		bridge->rt->state = 1;
		bridge->rt->queued = true;
		++bridge->children;
		++record.private_refs;
		registry->qualification_rt_dispatch(bridge->serial, first.handle_serial);
		alignas(8) uint8_t bytes[sizeof(GDExtensionStorageRetireRefV1) + sizeof(GDExtensionStorageRetireReportV1)] = {};
		GDExtensionQualificationRtObservationV1 observed = {};
		REQUIRE(registry->qualification_rt_observe(ref, first, &observed, bytes, sizeof(bytes), nullptr, 0).native_error == OK);
		CHECK(observed.state == 2);
		CHECK(observed.call_result.native_error == ERR_INVALID_DATA);
		CHECK(registry->qualification_rt_activate(ref, first).native_error == ERR_BUSY);
		CHECK(registry->qualification_rt_release(ref, first).native_error == ERR_BUSY);
		header.operation_serial = 2;
		CHECK(registry->qualification_rt_reserve(ref, &header, &request[1], nullptr, &second).native_error == ERR_BUSY);
		CHECK(second.handle_serial == 0);
		device._free_pending_resources(0);
		auto *a = bridge->storage[0];
		auto *b = bridge->storage[1];
		REQUIRE(a != nullptr);
		REQUIRE(b != nullptr);
		registry->qualification_retire_step(bridge, &device, a->serial, 1);
		const GDExtensionQualificationBudgetV1 budget{ { 1, 16, 0 }, 1, 1000, 0 };
		REQUIRE(registry->qualification_storage_retire_observe(ref, direct_ref, &budget, &report).native_error == OK);
		CHECK(report.state == 1);
		REQUIRE(registry->qualification_storage_retire_release(ref, direct_ref).native_error == OK);
		REQUIRE(registry->qualification_rt_release(ref, first).native_error == OK);
		REQUIRE(registry->qualification_rt_reserve(ref, &header, &request[1], nullptr, &second).native_error == OK);
		CHECK(second.handle_serial == UINT64_MAX);
		CHECK(registry->qualification_rt_release(ref, first).native_error != OK);
		bridge->rt->active = true;
		bridge->rt->state = 1;
		bridge->rt->queued = true;
		++bridge->children;
		++record.private_refs;
		registry->qualification_rt_dispatch(bridge->serial, second.handle_serial);
		REQUIRE(registry->qualification_rt_observe(ref, second, &observed, bytes, sizeof(bytes), nullptr, 0).native_error == OK);
		CHECK(observed.call_result.native_error == OK);
		GDExtensionStorageRetireRefV1 second_ref = {};
		memcpy(&second_ref, bytes, sizeof(second_ref));
		REQUIRE(second_ref.handle_serial != 0);
		REQUIRE(registry->qualification_rt_release(ref, second).native_error == OK);
		device._free_pending_resources(0);
		registry->qualification_retire_step(bridge, &device, b->serial, 1);
		REQUIRE(registry->qualification_storage_retire_observe(ref, second_ref, &budget, &report).native_error == OK);
		CHECK(report.state == 1);
		REQUIRE(registry->qualification_storage_retire_release(ref, second_ref).native_error == OK);
		REQUIRE(registry->qualification_close(ref).native_error == OK);
		RenderingDevice::singleton = previous;
		device.is_main_instance = false;
		memdelete(registry);
	}
	static void qrt_cleanup_survives_private_capacity(RenderingDevice &device, RenderingDeviceDriver &driver) {
		using namespace GDExtensionDeviceExit;
		auto *registry = memnew(Registry);
		auto *previous = RenderingDevice::singleton;
		RenderingDevice::singleton = &device;
		device.is_main_instance = true;
		device.render_thread_id = Thread::get_caller_id();
		registry->instance = 71;
		registry->private_limit = 4;
		auto &record = registry->records[0];
		record.owner_thread = Thread::get_caller_id();
		record.snapshot.library_generation = 1;
		record.guard_enrollment = GuardEnrollment::QUALIFIED;
		record.private_refs = 1;
		auto &d = registry->device_records[0];
		d.object = &device;
		d.live = true;
		d.epoch = 1;
		d.execution_thread = Thread::get_caller_id();
		d.refs = 1;
		struct Holder : HolderRetirement {
			Holder() : HolderRetirement(RecordRef{}) {}
			~Holder() = default;
		} holder;
		holder.generation = 1;
		holder.qualification_children = 1;
		auto *bridge = memnew(Registry::QualificationBridge);
		bridge->serial = 11;
		bridge->epoch = 1;
		bridge->holder = &holder;
		registry->qualification_bridges[0] = bridge;
		const GDExtensionQualificationBridgeRefV1 ref{ 71, 11 };
		device.draw_graph.initialize(&driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
		device.draw_graph.begin();
		GDExtensionStorageBackingV1 backing[3] = {};
		bool made[3] = {};
		for (uint32_t i = 0; i < 3; ++i) {
			GDExtensionStorageCreateRequestV1 q{ { 1, 16, 0 }, 128, 3 + i, 1, 41, 1, 0 };
			const auto r = registry->qualification_storage_create(ref, &q, &backing[i]);
			made[i] = r.native_error == OK;
			if (!made[i]) {
				CHECK(backing[i].rid == 0);
			}
		}
		REQUIRE(made[0]);
		const GDExtensionQualificationRtConfigV1 config{ { 1, 40, 0 }, 1, 312, 200, 1, 256 };
		REQUIRE(registry->qualification_rt_configure(ref, &config).native_error == OK);
		const GDExtensionStorageRetireRequestV1 first{ { 1, 16, 0 }, backing[0].backing, backing[0].rid, backing[0].backing_serial, 3, 1, 41, 2, 0 };
		const GDExtensionQualificationRtRequestV1 header{ { 1, 56, 0 }, 3, 0, 1, 0, sizeof(first), sizeof(GDExtensionStorageRetireRefV1) + sizeof(GDExtensionStorageRetireReportV1), 0, 0 };
		GDExtensionQualificationRtRefV1 rt = {};
		const auto admitted = registry->qualification_rt_reserve(ref, &header, &first, nullptr, &rt);
		CHECK_MESSAGE(admitted.native_error == OK, "A live child must retain its original QRT cleanup reference at the private-reference limit.");
		GDExtensionStorageRetireRefV1 qrt_retired = {};
		if (admitted.native_error == OK) {
			CHECK(record.private_refs == registry->private_limit);
			auto *cell = bridge->rt;
			cell->active = true;
			cell->state = 1;
			// Exercise the original queue-credit claim and refused-queue rollback at
			// saturation, then dispatch the same retained cell through the real path.
			REQUIRE(bridge->claim_cleanup_queue(cell->queued, cell->queue_ref_credit));
			CHECK(record.private_refs == registry->private_limit);
			bool other_queued = false, other_credit = false;
			CHECK_FALSE(bridge->claim_cleanup_queue(other_queued, other_credit));
			bridge->release_cleanup_queue(cell->queued, cell->queue_ref_credit);
			CHECK_FALSE(bridge->cleanup_queue_in_use);
			CHECK(record.private_refs == registry->private_limit);
			REQUIRE(bridge->claim_cleanup_queue(cell->queued, cell->queue_ref_credit));
			registry->qualification_rt_dispatch(bridge->serial, rt.handle_serial);
			CHECK_FALSE(bridge->cleanup_queue_in_use);
			CHECK_FALSE(cell->queued);
			CHECK(record.private_refs == registry->private_limit);
			alignas(8) uint8_t bytes[sizeof(GDExtensionStorageRetireRefV1) + sizeof(GDExtensionStorageRetireReportV1)] = {};
			GDExtensionQualificationRtObservationV1 observed = {};
			REQUIRE(registry->qualification_rt_observe(ref, rt, &observed, bytes, sizeof(bytes), nullptr, 0).native_error == OK);
			CHECK(observed.state == 2);
			CHECK(observed.call_result.native_error == OK);
			memcpy(&qrt_retired, bytes, sizeof(qrt_retired));
			REQUIRE(qrt_retired.handle_serial != 0);
			REQUIRE(registry->qualification_rt_release(ref, rt).native_error == OK);
		}
		// Fixture teardown only: restore room for the original direct retirement.
		registry->private_limit = 100;
		for (uint32_t i = 0; i < 3; ++i) {
			if (made[i]) {
				GDExtensionStorageRetireRequestV1 q{ { 1, 16, 0 }, backing[i].backing, backing[i].rid, backing[i].backing_serial, 3 + i, 1, 41, 2, 0 };
				GDExtensionStorageRetireRefV1 retired = {};
				GDExtensionStorageRetireReportV1 report = {};
				REQUIRE(registry->qualification_storage_retire(ref, &q, &retired, &report).native_error == OK);
				if (i == 0 && qrt_retired.handle_serial) {
					CHECK(retired.handle_serial == qrt_retired.handle_serial);
				}
				device._free_pending_resources(0);
				Registry::QualificationBridge::Storage *child = nullptr;
				for (auto *entry : bridge->storage) {
					if (entry && entry->serial == backing[i].backing.handle_serial) {
						child = entry;
						break;
					}
				}
				REQUIRE(child != nullptr);
				registry->qualification_retire_step(bridge, &device, child->serial, 1);
				const GDExtensionQualificationBudgetV1 budget{ { 1, 16, 0 }, 1, 1000, 0 };
				REQUIRE(registry->qualification_storage_retire_observe(ref, retired, &budget, &report).native_error == OK);
				CHECK(report.state == 1);
				REQUIRE(registry->qualification_storage_retire_release(ref, retired).native_error == OK);
			}
		}
		CHECK_MESSAGE(record.private_refs == 1, "Returned child and released QRT slot must return the bridge cleanup credits.");
		REQUIRE(registry->qualification_close(ref).native_error == OK);
		RenderingDevice::singleton = previous;
		device.is_main_instance = false;
		memdelete(registry);
	}
	static void qrt_slot_last_returns_private_credits(RenderingDevice &device, RenderingDeviceDriver &driver) {
		using namespace GDExtensionDeviceExit;
		auto *registry = memnew(Registry);
		auto *previous = RenderingDevice::singleton;
		RenderingDevice::singleton = &device;
		device.is_main_instance = true;
		device.render_thread_id = Thread::get_caller_id();
		registry->instance = 71;
		registry->private_limit = 4;
		auto &record = registry->records[0];
		record.owner_thread = Thread::get_caller_id();
		record.snapshot.library_generation = 1;
		record.guard_enrollment = GuardEnrollment::QUALIFIED;
		record.private_refs = 1;
		auto &d = registry->device_records[0];
		d.object = &device;
		d.live = true;
		d.epoch = 1;
		d.execution_thread = Thread::get_caller_id();
		d.refs = 1;
		struct Holder : HolderRetirement {
			Holder() : HolderRetirement(RecordRef{}) {}
			~Holder() = default;
		} holder;
		holder.generation = 1;
		holder.qualification_children = 1;
		auto *bridge = memnew(Registry::QualificationBridge);
		bridge->serial = 11;
		bridge->epoch = 1;
		bridge->holder = &holder;
		registry->qualification_bridges[0] = bridge;
		const GDExtensionQualificationBridgeRefV1 ref{ 71, 11 };
		device.draw_graph.initialize(&driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
		device.draw_graph.begin();
		GDExtensionStorageCreateRequestV1 create{ { 1, 16, 0 }, 128, 3, 1, 41, 1, 0 };
		GDExtensionStorageBackingV1 backing = {};
		REQUIRE(registry->qualification_storage_create(ref, &create, &backing).native_error == OK);
		const GDExtensionQualificationRtConfigV1 config{ { 1, 40, 0 }, 1, 312, 200, 1, 256 };
		REQUIRE(registry->qualification_rt_configure(ref, &config).native_error == OK);
		GDExtensionStorageRetireRequestV1 retire{ { 1, 16, 0 }, backing.backing, backing.rid, backing.backing_serial, 3, 1, 41, 2, 0 };
		GDExtensionQualificationRtRequestV1 header{ { 1, 56, 0 }, 3, 0, 1, 0, sizeof(retire), sizeof(GDExtensionStorageRetireRefV1) + sizeof(GDExtensionStorageRetireReportV1), 0, 0 };
		GDExtensionQualificationRtRefV1 rt = {};
		REQUIRE(registry->qualification_rt_reserve(ref, &header, &retire, nullptr, &rt).native_error == OK);
		GDExtensionStorageRetireRefV1 retired = {};
		GDExtensionStorageRetireReportV1 report = {};
		REQUIRE(registry->qualification_storage_retire(ref, &retire, &retired, &report).native_error == OK);
		device._free_pending_resources(0);
		auto *child = bridge->storage[0];
		REQUIRE(child != nullptr);
		registry->qualification_retire_step(bridge, &device, child->serial, 1);
		const GDExtensionQualificationBudgetV1 budget{ { 1, 16, 0 }, 1, 1000, 0 };
		REQUIRE(registry->qualification_storage_retire_observe(ref, retired, &budget, &report).native_error == OK);
		CHECK(report.state == 1);
		REQUIRE(registry->qualification_storage_retire_release(ref, retired).native_error == OK);
		CHECK(record.private_refs == 3);
		CHECK(bridge->cleanup_slot_in_use);
		REQUIRE(registry->qualification_rt_release(ref, rt).native_error == OK);
		CHECK_FALSE(bridge->cleanup_slot_in_use);
		CHECK_MESSAGE(record.private_refs == 1, "The last QRT slot return must release the two now-unused cleanup credits.");
		const uint64_t query = backing.rid;
		GDExtensionQualificationRtRequestV1 query_header{ { 1, 56, 0 }, 1, 0, 2, 0, sizeof(query), 48, 0, 0 };
		GDExtensionQualificationRtRefV1 query_ref = {};
		REQUIRE(registry->qualification_rt_reserve(ref, &query_header, &query, nullptr, &query_ref).native_error == OK);
		CHECK(record.private_refs == 3);
		REQUIRE(registry->qualification_rt_release(ref, query_ref).native_error == OK);
		REQUIRE(registry->qualification_close(ref).native_error == OK);
		RenderingDevice::singleton = previous;
		device.is_main_instance = false;
		memdelete(registry);
	}
	static void prepared_ticket_cleanup_capacity(RenderingDevice &device, RenderingDeviceDriver &driver) {
		using namespace GDExtensionDeviceExit;
		// The Registry and buffer are fixture-enrolled; preparation and release use
		// the original API. No native dispatch or completion is invented here.
		auto *registry = memnew(Registry);
		auto *previous = RenderingDevice::singleton;
		RenderingDevice::singleton = &device;
		device.is_main_instance = true;
		device.render_thread_id = Thread::get_caller_id();
		registry->instance = 71;
		registry->private_limit = 2;
		auto &record = registry->records[0];
		record.owner_thread = Thread::get_caller_id();
		record.snapshot.library_generation = 1;
		record.guard_enrollment = GuardEnrollment::QUALIFIED;
		record.private_refs = 1;
		auto &d = registry->device_records[0];
		d.object = &device;
		d.live = true;
		d.epoch = 1;
		d.execution_thread = Thread::get_caller_id();
		d.refs = 1;
		struct Holder : HolderRetirement {
			Holder() : HolderRetirement(RecordRef{}) {}
			~Holder() = default;
		} holder;
		holder.generation = 1;
		holder.qualification_children = 1;
		auto *bridge = memnew(Registry::QualificationBridge);
		bridge->serial = 11;
		bridge->epoch = 1;
		bridge->holder = &holder;
		registry->qualification_bridges[0] = bridge;
		device.draw_graph.initialize(&driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
		device.draw_graph.begin();
		RenderingDevice::Buffer buffer;
		buffer.driver_id = RenderingDeviceDriver::BufferID(1);
		buffer.backing_serial = 41;
		buffer.size = 128;
		buffer.usage = RenderingDeviceDriver::BUFFER_USAGE_TRANSFER_TO_BIT;
		RID rid = device.storage_buffer_owner.make_rid(buffer);
		GDExtensionQualificationUploadPlanV1 plan = {};
		plan.abi = { 1, 16, 0 };
		plan.role = 7;
		plan.owner_kind = GDEXTENSION_QUALIFICATION_UPLOAD_OWNER_E09_V1;
		plan.rid = rid.get_id();
		plan.backing_serial = 41;
		plan.byte_size = 32;
		GDExtensionQualificationUploadPrepareV1 prepare = {};
		prepare.abi = { 1, 16, 0 };
		prepare.record_generation = 37;
		prepare.request_nonce = 39;
		prepare.library_generation = 1;
		prepare.qualification_session_serial = 7;
		prepare.parent_serial = 31;
		prepare.reservation_serial = 97;
		prepare.attempt = prepare.upload_count = prepare.max_segments = 1;
		prepare.expected_result_bytes = 16;
		const GDExtensionQualificationBridgeRefV1 ref{ 71, 11 };
		GDExtensionQualificationTicketRefV1 ticket = {};
		GDExtensionQualificationReportV1 report = {};
		auto refused = registry->qualification_prepare_uploads(ref, &prepare, &plan, 1, &ticket, &report);
		CHECK_MESSAGE(refused.native_error == ERR_PARAMETER_RANGE_ERROR, "A ticket must not consume the last reference without cleanup capacity.");
		if (refused.native_error == OK) {
			CHECK(registry->qualification_release(ref, ticket).native_error == OK);
			ticket = {};
			++prepare.request_nonce;
		}
		CHECK(record.private_refs == 1);
		CHECK(bridge->children == 0);
		registry->private_limit = 4;
		auto accepted = registry->qualification_prepare_uploads(ref, &prepare, &plan, 1, &ticket, &report);
		CHECK(accepted.native_error == OK);
		if (accepted.native_error == OK) {
			CHECK(record.private_refs == 4);
			auto *original = bridge->tickets[refused.native_error == OK ? 1 : 0];
			REQUIRE(original != nullptr);
			// Controlled stop state and queue enrollment; the real service executes
			// the original ticket reducer. This does not exercise server dispatch.
			original->stop_requested = true;
			original->admission_closed = true;
			original->physically_retired = false;
			CHECK(registry->qualification_release(ref, ticket).native_error == ERR_BUSY);
			{
				MutexLock lock(registry->mutex);
				CHECK(bridge->claim_cleanup_queue(original->queued, original->queue_ref_credit));
			}
			CHECK(record.private_refs == 4);
			registry->qualification_service(bridge->serial, original->serial, 3, 1, 5000);
			CHECK_FALSE(original->queued);
			CHECK_FALSE(bridge->cleanup_queue_in_use);
			CHECK(original->physically_retired.load());
			CHECK(original->report.state == 7);
			CHECK(registry->qualification_release(ref, ticket).native_error == OK);
			CHECK(record.private_refs == 1);
		}
		CHECK(registry->qualification_close(ref).native_error == OK);
		device.storage_buffer_owner.free(rid);
		RenderingDevice::singleton = previous;
		device.is_main_instance = false;
		memdelete(registry);
	}
	static void saturated_ticket_transport(RenderingDevice &device, RenderingDeviceDriver &driver, bool record_work, bool contend = false, bool full_queue = false) {
		using namespace GDExtensionDeviceExit;
		// Singleton fixture cleanup is safe only when server dispatch is synchronous.
		REQUIRE(Thread::is_main_thread());
		REQUIRE(RenderingServer::get_singleton() != nullptr);
		REQUIRE(RenderingServer::get_singleton()->is_on_render_thread());
		auto &registry = Registry::get_singleton();
		uint32_t library = 0, native = 0, bridge_slot = 0;
		struct Holder : HolderRetirement {
			Holder() : HolderRetirement(RecordRef{}) {}
			~Holder() = default;
		} holder;
		holder.generation = 1;
		holder.qualification_children = 1;
		Registry::DeviceRecord saved_device;
		uint64_t saved_owner = 0, saved_generation = 0;
		GuardEnrollment saved_guard = GuardEnrollment::UNAVAILABLE;
		uint32_t saved_refs = 0, saved_limit = 0;
		RenderingDevice *previous = nullptr;
		Registry::QualificationBridge *bridge = nullptr;
		{
			MutexLock lock(registry.mutex);
			bool isolated = true;
			for (uint32_t i = 0; i < Registry::RECORD_CAPACITY; ++i) {
				if (registry.records[i].occupied || registry.qualification_bridges[i]) {
					isolated = false;
					break;
				}
			}
			REQUIRE(isolated);
			for (; library < Registry::RECORD_CAPACITY && registry.records[library].occupied; ++library) {
			}
			for (; native < Registry::RECORD_CAPACITY && registry.device_records[native].live; ++native) {
			}
			for (; bridge_slot < Registry::RECORD_CAPACITY && registry.qualification_bridges[bridge_slot]; ++bridge_slot) {
			}
			REQUIRE(library < Registry::RECORD_CAPACITY);
			REQUIRE(native < Registry::RECORD_CAPACITY);
			REQUIRE(bridge_slot < Registry::RECORD_CAPACITY);
			REQUIRE(registry.qualification_serial_capacity(4));
			auto &record = registry.records[library];
			auto &d = registry.device_records[native];
			saved_device = d;
			saved_owner = record.owner_thread;
			saved_generation = record.snapshot.library_generation;
			saved_guard = record.guard_enrollment;
			saved_refs = record.private_refs;
			saved_limit = registry.private_limit;
			previous = RenderingDevice::singleton;
			RenderingDevice::singleton = &device;
			device.is_main_instance = true;
			device.render_thread_id = Thread::get_caller_id();
			record.owner_thread = Thread::get_caller_id();
			record.snapshot.library_generation = 1;
			record.guard_enrollment = GuardEnrollment::QUALIFIED;
			record.private_refs = 1;
			registry.private_limit = 5;
			d.object = &device;
			d.live = true;
			d.epoch = 1;
			d.execution_thread = Thread::get_caller_id();
			d.refs = 1;
			bridge = memnew(Registry::QualificationBridge);
			registry.qualification_serials.take(bridge->serial);
			bridge->library = library;
			bridge->device = native;
			bridge->epoch = 1;
			bridge->holder = &holder;
			registry.qualification_bridges[bridge_slot] = bridge;
		}
		auto &record = registry.records[library];
		device.draw_graph.initialize(&driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
		device.draw_graph.begin();
		RenderingDevice::Buffer buffer;
		buffer.driver_id = RenderingDeviceDriver::BufferID(1);
		buffer.backing_serial = 41;
		buffer.size = 128;
		buffer.usage = RenderingDeviceDriver::BUFFER_USAGE_TRANSFER_TO_BIT;
		RID rid = device.storage_buffer_owner.make_rid(buffer);
		GDExtensionQualificationUploadPlanV1 plan = {};
		plan.abi = { 1, 16, 0 };
		plan.role = 7;
		plan.owner_kind = GDEXTENSION_QUALIFICATION_UPLOAD_OWNER_E09_V1;
		plan.rid = rid.get_id();
		plan.backing_serial = 41;
		plan.byte_size = 32;
		GDExtensionQualificationUploadPrepareV1 prepare = {};
		prepare.abi = { 1, 16, 0 };
		prepare.record_generation = 37;
		prepare.request_nonce = 39;
		prepare.library_generation = 1;
		prepare.qualification_session_serial = 7;
		prepare.parent_serial = 31;
		prepare.reservation_serial = 97;
		prepare.attempt = prepare.upload_count = prepare.max_segments = 1;
		prepare.expected_result_bytes = 16;
		GDExtensionQualificationBridgeRefV1 ref{ registry.instance, bridge->serial };
		GDExtensionQualificationTicketRefV1 ticket = {};
		GDExtensionQualificationReportV1 report = {};
		auto prepared = registry.qualification_prepare_uploads(ref, &prepare, &plan, 1, &ticket, &report);
		CHECK(prepared.native_error == OK);
		GDExtensionQualificationRtConfigV1 config{ { 1, 40, 0 }, 1, 312, 200, 1, 256 };
		CHECK(registry.qualification_rt_configure(ref, &config).native_error == OK);
		GDExtensionQualificationUploadRequestV1 upload = {};
		upload.abi = { 1, 16, 0 };
		upload.record_generation = 37;
		upload.parent_serial = 31;
		upload.rid = rid.get_id();
		upload.backing_serial = 41;
		upload.byte_size = 32;
		upload.role = 7;
		upload.owner_kind = plan.owner_kind;
		upload.expected_revision = report.revision;
		GDExtensionQualificationRecordV1 request = {};
		request.abi = { 1, 16, 0 };
		request.record_generation = 37;
		request.request_nonce = 39;
		request.library_generation = 1;
		request.qualification_session_serial = 7;
		request.attempt = 1;
		request.range_end = request.sample_count = 4;
		request.body_groups_x = 1;
		request.readback_size = 16;
		// Record admission requires completed uploads. Seed this metadata only for
		// the controlled record branch; it is not evidence of an actual upload.
		if (record_work && bridge->tickets[0]) {
			auto &u = bridge->tickets[0]->uploads[0];
			u.attempted = true;
			u.progress.consumed = 32;
			u.report.state = 1;
		}
		alignas(8) uint8_t payload[48] = {};
		memcpy(payload, &ticket, sizeof(ticket));
		GDExtensionQualificationRtRequestV1 header{ { 1, 56, 0 }, record_work ? 9u : 8u, 0, 1, record_work ? 16u : uint64_t(sizeof(payload)), record_work ? uint32_t(sizeof(request)) : uint32_t(sizeof(upload)), record_work ? uint32_t(sizeof(GDExtensionQualificationReportV1)) : uint32_t(sizeof(GDExtensionQualificationUploadReportV1)), record_work ? 0u : 1u, 0 };
		GDExtensionQualificationRtRefV1 rt = {};
		const void *request_ptr = record_work ? static_cast<const void *>(&request) : static_cast<const void *>(&upload);
		auto reserved = registry.qualification_rt_reserve(ref, &header, request_ptr, payload, &rt);
		CHECK(reserved.native_error == OK);
		CHECK(record.private_refs == 5);
		if (!full_queue) {
			bool other_queued = false, other_credit = false;
			if (contend) {
				MutexLock lock(registry.mutex);
				CHECK(bridge->claim_cleanup_queue(other_queued, other_credit));
			}
			auto stopped = registry.qualification_cancel(ref, ticket);
			CHECK(stopped.native_error == OK);
			auto activated = registry.qualification_rt_activate(ref, rt);
			CHECK(activated.native_error == OK);
			GDExtensionQualificationRtObservationV1 observation = {};
			alignas(8) uint8_t returned[200] = {};
			GDExtensionQualificationUploadSegmentV1 segment = {};
			CHECK(registry.qualification_rt_observe(ref, rt, &observation, returned, header.result_capacity, record_work ? nullptr : &segment, header.segment_capacity).native_error == OK);
			if (contend && other_credit) {
				CHECK(observation.state == 1);
				CHECK(registry.qualification_release(ref, ticket).native_error == ERR_BUSY);
				{
					MutexLock lock(registry.mutex);
					bridge->release_cleanup_queue(other_queued, other_credit);
				}
				CHECK(registry.qualification_rt_observe(ref, rt, &observation, returned, header.result_capacity, record_work ? nullptr : &segment, header.segment_capacity).native_error == OK);
			}
			CHECK_MESSAGE(observation.state == 2, "An already accepted ticket operation must reach the real server even when private references are saturated.");
			if (observation.state == 2) {
				CHECK(observation.call_result.native_error == ERR_BUSY);
				CHECK(registry.qualification_rt_release(ref, rt).native_error == OK);
				if (contend) {
					const GDExtensionQualificationBudgetV1 budget{ { 1, 16, 0 }, 1, 1000, 0 };
					CHECK(registry.qualification_observe(ref, ticket, &budget, &report).native_error == OK);
				}
				CHECK(registry.qualification_release(ref, ticket).native_error == OK);
				CHECK(record.private_refs == 1);
			}
		}
#ifdef THREADS_ENABLED
		else if (bridge->rt && bridge->tickets[0]) {
			// Fill the real bounded server queue while the main render thread waits.
			// The worker retries ticket and RT service only after each actual drain.
			struct Filler : Object {
				uint32_t calls = 0;
				void run() { ++calls; }
			} filler;
			const Callable fill_call = callable_mp(&filler, &Filler::run);
			Semaphore first, first_drain, second, second_drain, third, third_drain;
			GDExtensionDeviceSessionResultV1 cancel_result = {}, activate_result = {}, ticket_pending = {}, rt_pending = {}, ticket_retry = {}, rt_retry = {}, rt_final = {}, rt_release = {}, ticket_release = {};
			GDExtensionQualificationBudgetV1 budget{ { 1, 16, 0 }, 1, 1000, 0 };
			GDExtensionQualificationReportV1 ticket_report = {};
			GDExtensionQualificationRtObservationV1 initial = {}, retry = {}, done = {};
			GDExtensionQualificationUploadReportV1 output = {};
			GDExtensionQualificationUploadSegmentV1 segment = {};
			bool initially_queued = true, credit_in_use = true;
			uint32_t initial_refs = 0;
			std::thread owner([&] {
				{
					MutexLock lock(registry.mutex);
					record.owner_thread = Thread::get_caller_id();
				}
				for (uint32_t i = 0; i < 4096; ++i) {
					RenderingServer::get_singleton()->call_on_render_thread(fill_call);
				}
				cancel_result = registry.qualification_cancel(ref, ticket);
				activate_result = registry.qualification_rt_activate(ref, rt);
				ticket_pending = registry.qualification_observe(ref, ticket, &budget, &ticket_report);
				rt_pending = registry.qualification_rt_observe(ref, rt, &initial, &output, sizeof(output), &segment, 1);
				{
					MutexLock lock(registry.mutex);
					initially_queued = bridge->rt->queued || bridge->tickets[0]->queued;
					credit_in_use = bridge->cleanup_queue_in_use;
					initial_refs = record.private_refs;
				}
				first.post();
				first_drain.wait();
				ticket_retry = registry.qualification_observe(ref, ticket, &budget, &ticket_report);
				second.post();
				second_drain.wait();
				rt_retry = registry.qualification_rt_observe(ref, rt, &retry, &output, sizeof(output), &segment, 1);
				third.post();
				third_drain.wait();
				rt_final = registry.qualification_rt_observe(ref, rt, &done, &output, sizeof(output), &segment, 1);
				rt_release = registry.qualification_rt_release(ref, rt);
				ticket_release = registry.qualification_release(ref, ticket);
			});
			first.wait();
			RenderingServer::get_singleton()->sync();
			first_drain.post();
			second.wait();
			RenderingServer::get_singleton()->sync();
			second_drain.post();
			third.wait();
			RenderingServer::get_singleton()->sync();
			third_drain.post();
			owner.join();
			RenderingServer::get_singleton()->sync();
			CHECK(cancel_result.native_error == OK);
			CHECK(activate_result.native_error == OK);
			CHECK(ticket_pending.native_error == OK);
			CHECK(rt_pending.native_error == OK);
			CHECK(initial.state == 1);
			CHECK_FALSE(initially_queued);
			CHECK_FALSE(credit_in_use);
			CHECK(initial_refs == 5);
			CHECK(filler.calls == 4096);
			CHECK(ticket_retry.native_error == OK);
			CHECK(rt_retry.native_error == OK);
			CHECK(retry.state == 1);
			CHECK(rt_final.native_error == OK);
			CHECK(done.state == 2);
			CHECK(done.call_result.native_error == ERR_BUSY);
			CHECK(rt_release.native_error == OK);
			CHECK(ticket_release.native_error == OK);
			CHECK(record.private_refs == 1);
		}
#endif
		// Fixture-only removal after a failed assertion; never publish fake results.
		{
			MutexLock lock(registry.mutex);
			registry.qualification_bridges[bridge_slot] = nullptr;
			record.owner_thread = saved_owner;
			record.snapshot.library_generation = saved_generation;
			record.guard_enrollment = saved_guard;
			record.private_refs = saved_refs;
			registry.private_limit = saved_limit;
			registry.device_records[native] = saved_device;
			RenderingDevice::singleton = previous;
			device.is_main_instance = false;
		}
		for (auto *entry : bridge->tickets) {
			if (entry) {
				memdelete(entry);
			}
		}
		if (bridge->rt) {
			memdelete(bridge->rt);
		}
		memdelete(bridge);
		device.storage_buffer_owner.free(rid);
	}
	static void retired_prepared_upload(RenderingDevice &device, RenderingDeviceDriver &driver) {
		using namespace GDExtensionDeviceExit;
		// Original cells are fixture-enrolled; calls below exercise the original
		// API lifecycle. This CPU control grants no production/native authority.
		auto *registry = memnew(Registry);
		auto *previous = RenderingDevice::singleton;
		RenderingDevice::singleton = &device;
		device.is_main_instance = true;
		device.render_thread_id = Thread::get_caller_id();
		registry->instance = 71;
		auto &record = registry->records[0];
		record.owner_thread = Thread::get_caller_id();
		record.snapshot.library_generation = 1;
		record.guard_enrollment = GuardEnrollment::QUALIFIED;
		record.private_refs = 1;
		auto &d = registry->device_records[0];
		d.object = &device;
		d.live = true;
		d.epoch = 1;
		d.execution_thread = Thread::get_caller_id();
		d.refs = 1;
		struct Holder : HolderRetirement {
			Holder() : HolderRetirement(RecordRef{}) {}
			~Holder() = default;
		} holder;
		holder.generation = 1;
		holder.qualification_children = 1;
		auto *bridge = memnew(Registry::QualificationBridge);
		bridge->serial = 11;
		bridge->epoch = 1;
		bridge->holder = &holder;
		registry->qualification_bridges[0] = bridge;
		const GDExtensionQualificationBridgeRefV1 ref{ 71, 11 };
		device.draw_graph.initialize(&driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
		device.draw_graph.begin();
		GDExtensionStorageCreateRequestV1 create{ { 1, 16, 0 }, 128, 3, 1, 41, 1, 0 };
		GDExtensionStorageBackingV1 backing = {};
		REQUIRE(registry->qualification_storage_create(ref, &create, &backing).native_error == OK);
		uint8_t bytes[32] = {};
		GDExtensionQualificationUploadPlanV1 plan = {};
		plan.abi = { 1, 16, 0 };
		plan.role = 7;
		plan.owner_kind = GDEXTENSION_QUALIFICATION_UPLOAD_OWNER_E05_V1;
		plan.rid = backing.rid;
		plan.backing_serial = backing.backing_serial;
		plan.byte_size = sizeof(bytes);
		plan.e05_backing = backing.backing;
		REQUIRE(CryptoCore::sha256(bytes, sizeof(bytes), plan.payload_digest) == OK);
		GDExtensionQualificationUploadPrepareV1 prepare = {};
		prepare.abi = { 1, 16, 0 };
		prepare.record_generation = 37;
		prepare.request_nonce = 39;
		prepare.library_generation = 1;
		prepare.qualification_session_serial = 7;
		prepare.parent_serial = 31;
		prepare.reservation_serial = 97;
		prepare.attempt = prepare.upload_count = prepare.max_segments = 1;
		prepare.expected_result_bytes = 16;
		GDExtensionQualificationTicketRefV1 ticket_ref = {};
		GDExtensionQualificationReportV1 report = {};
		REQUIRE(registry->qualification_prepare_uploads(ref, &prepare, &plan, 1, &ticket_ref, &report).native_error == OK);
		auto *ticket = bridge->tickets[0];
		REQUIRE(ticket != nullptr);
		LocalVector<RID> pins;
		pins.push_back(RID::from_uint64(backing.rid));
		REQUIRE(device._qualification_pin_resources(ticket, pins) == OK);
		CHECK(ticket->physical_uses.size() == 1);
		registry->qualification_ticket_publish(ticket, &device);
		CHECK(ticket->physically_retired.load());
		CHECK(ticket->resource_pins.is_empty());
		CHECK_FALSE(ticket->admission_closed.load());
		GDExtensionQualificationUploadRequestV1 upload = {};
		upload.abi = { 1, 16, 0 };
		upload.record_generation = 37;
		upload.parent_serial = 31;
		upload.rid = plan.rid;
		upload.backing_serial = plan.backing_serial;
		upload.byte_size = sizeof(bytes);
		upload.role = plan.role;
		upload.owner_kind = plan.owner_kind;
		upload.e05_backing = plan.e05_backing;
		upload.expected_revision = ticket->report.revision;
		memcpy(upload.payload_digest, plan.payload_digest, 32);
		GDExtensionQualificationRtConfigV1 config{ { 1, 40, 0 }, 1, 312, 200, 1, 256 };
		REQUIRE(registry->qualification_rt_configure(ref, &config).native_error == OK);
		alignas(8) uint8_t payload[48] = {};
		memcpy(payload, &ticket_ref, 16);
		memcpy(payload + 16, bytes, sizeof(bytes));
		GDExtensionQualificationRtRequestV1 header{ { 1, 56, 0 }, 8, 0, 1, sizeof(payload), sizeof(upload), sizeof(GDExtensionQualificationUploadReportV1), 1, 0 };
		GDExtensionQualificationRtRefV1 rt = {};
		REQUIRE(registry->qualification_rt_reserve(ref, &header, &upload, payload, &rt).native_error == OK);
		CHECK(ticket->accepted_rt_refs == 1);
		REQUIRE(registry->qualification_upload_destination(bridge, &device, plan) == OK);
		GDExtensionStorageRetireRequestV1 retire{ { 1, 16, 0 }, backing.backing, backing.rid, backing.backing_serial, 3, 1, 41, 2, 0 };
		GDExtensionStorageRetireRefV1 retired = {};
		GDExtensionStorageRetireReportV1 retired_report = {};
		REQUIRE(registry->qualification_storage_retire(ref, &retire, &retired, &retired_report).native_error == OK);
		CHECK_FALSE(device.storage_buffer_owner.owns(RID::from_uint64(backing.rid)));
		GDExtensionQualificationUploadSegmentV1 segment = {};
		GDExtensionQualificationUploadReportV1 upload_report = {};
		auto refused = registry->qualification_upload(ref, ticket_ref, &upload, bytes, sizeof(bytes), &segment, 1, &upload_report);
		CHECK(refused.native_error == ERR_INVALID_DATA);
		CHECK(refused.reason == 16);
		CHECK(upload_report.revision == 0);
		CHECK(ticket->graphs.is_empty());
		CHECK(ticket->natives.is_empty());
		CHECK(device.draw_graph.qualification_command_count() == 0);
		CHECK(registry->qualification_release(ref, ticket_ref).native_error == ERR_BUSY);
		REQUIRE(registry->qualification_rt_release(ref, rt).native_error == OK);
		CHECK(ticket->accepted_rt_refs == 0);
		REQUIRE(registry->qualification_release(ref, ticket_ref).native_error == OK);
		CHECK(registry->qualification_release(ref, ticket_ref).reason == 17);
		device._free_pending_resources(0);
		// Drive the original synchronous maintenance step on this fixture Registry;
		// the global RenderingServer scheduler belongs to the global Registry.
		registry->qualification_retire_step(bridge, &device, bridge->storage[0]->serial, 1);
		const GDExtensionQualificationBudgetV1 budget{ { 1, 16, 0 }, 1, 1000, 0 };
		REQUIRE(registry->qualification_storage_retire_observe(ref, retired, &budget, &retired_report).native_error == OK);
		CHECK(retired_report.state == 1);
		REQUIRE(registry->qualification_storage_retire_release(ref, retired).native_error == OK);
		CHECK(bridge->children == 0);
		CHECK(record.private_refs == 1);
		REQUIRE(registry->qualification_close(ref).native_error == OK);
		CHECK(registry->qualification_bridges[0] == nullptr);
		CHECK(holder.qualification_children == 0);
		RenderingDevice::singleton = previous;
		device.is_main_instance = false;
		memdelete(registry);
	}
	static void retire_window(RenderingDevice &device, RenderingDeviceDriver &driver, uint32_t mode) {
		using namespace GDExtensionDeviceExit;
		auto *bridge = memnew(Registry::QualificationBridge);
		auto *child = memnew(Registry::QualificationBridge::Storage);
		bridge->storage[0] = child;
		child->serial = 31;
		child->retiring = true;
		child->physical.bridge = 11;
		child->physical.child = 31;
		RID rid = device._qualification_storage_create(128, &child->physical);
		REQUIRE(rid.is_valid());
		child->physical.use_closed = true;
		device.draw_graph.initialize(&driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
		device.draw_graph.begin();
		if (mode == 1) {
			device.draw_list.active = true;
		}
		if (mode == 2) {
			device.compute_list.active = true;
		}
		if (mode == 3) {
			device.raytracing_list.active = true;
		}
		if (mode == 4) {
			device.draw_graph.add_driver_callback(nullptr, nullptr, {}, {});
		}
		Registry::get_singleton().qualification_retire_step(bridge, &device, 31, 1);
		CHECK(device.storage_buffer_owner.owns(rid));
		CHECK_FALSE(child->physical.enqueued);
		CHECK(child->report.state == 0);
		device.draw_list.active = false;
		device.compute_list.active = false;
		device.raytracing_list.active = false;
		device.draw_graph.begin();
		Registry::get_singleton().qualification_retire_step(bridge, &device, 31, 1);
		CHECK_FALSE(device.storage_buffer_owner.owns(rid));
		CHECK(child->physical.enqueued);
		CHECK(child->physical.free_event == 0);
		// This control checks enqueue safety only. Keep the queued return cell alive
		// through the original driver's disposal, then delete the original child.
		device._free_pending_resources(0);
		CHECK(child->physical.free_event != 0);
		memdelete(child);
		memdelete(bridge);
	}
	static void retire_after_service_error(RenderingDevice &device, RenderingDeviceDriver &driver) {
		using namespace GDExtensionDeviceExit;
		auto *bridge = memnew(Registry::QualificationBridge);
		auto *child = memnew(Registry::QualificationBridge::Storage);
		bridge->storage[0] = child;
		child->serial = 31;
		child->retiring = true;
		child->physical.bridge = 11;
		child->physical.child = 31;
		RID rid = device._qualification_storage_create(128, &child->physical);
		REQUIRE(rid.is_valid());
		child->physical.use_closed = true;
		device.draw_graph.initialize(&driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
		device.draw_graph.begin();
		auto &registry = Registry::get_singleton();
		registry.qualification_retire_step(bridge, &device, 31, 1);
		REQUIRE(child->physical.enqueued);
		REQUIRE(child->physical.free_event == 0);
		// Model the original service's published non-BUSY maintenance error after
		// enqueue; the actual buffer remains owned by the original dispose queue.
		child->maintenance_error = ERR_CANT_ACQUIRE_RESOURCE;
		child->report.state = 2;
		child->report.reason = 5;
		child->report.native_error = ERR_CANT_ACQUIRE_RESOURCE;
		registry.qualification_retire_step(bridge, &device, 31, 1);
		CHECK(child->report.state == 2);
		CHECK(child->report.return_status == 0);
		CHECK(child->report.reason == 5);
		CHECK(child->report.native_error == ERR_CANT_ACQUIRE_RESOURCE);
		device._free_pending_resources(0);
		REQUIRE(child->physical.free_event != 0);
		registry.qualification_retire_step(bridge, &device, 31, 1);
		CHECK(child->report.event_seq == child->physical.free_event);
		CHECK(child->report.state == 2);
		CHECK(child->report.return_status == 0);
		CHECK(child->report.reason == 5);
		CHECK(child->report.native_error == ERR_CANT_ACQUIRE_RESOURCE);
		memdelete(child);
		memdelete(bridge);
	}
	static void unknown_does_not_enqueue(RenderingDevice &device, RenderingDeviceDriver &driver) {
		using namespace GDExtensionDeviceExit;
		auto *bridge = memnew(Registry::QualificationBridge);
		auto *child = memnew(Registry::QualificationBridge::Storage);
		bridge->storage[0] = child;
		child->serial = 31;
		child->retiring = true;
		child->physical.bridge = 11;
		child->physical.child = 31;
		child->physical.use_closed = true;
		RID rid = device._qualification_storage_create(128, &child->physical);
		REQUIRE(rid.is_valid());
		device.draw_graph.initialize(&driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
		device.draw_graph.begin();
		child->maintenance_error = FAILED;
		child->report.state = 2;
		child->report.reason = 5;
		child->report.native_error = FAILED;
		Registry::get_singleton().qualification_retire_step(bridge, &device, 31, 1);
		CHECK(device.storage_buffer_owner.owns(rid));
		CHECK_FALSE(child->physical.enqueued);
		CHECK(child->report.state == 2);
		CHECK(child->report.reason == 5);
		// Explicit fixture cleanup follows the assertion and is not an owner ack.
		if (device.storage_buffer_owner.owns(rid)) {
			device._free_internal(rid);
		}
		device._free_pending_resources(0);
		memdelete(child);
		memdelete(bridge);
	}
	static void service_poll_error(RenderingDevice &device, RenderingDeviceDriver &driver) {
		using namespace GDExtensionDeviceExit;
		auto *registry = memnew(Registry);
		auto *previous = RenderingDevice::singleton;
		RenderingDevice::singleton = &device;
		device.is_main_instance = true;
		device.render_thread_id = Thread::get_caller_id();
		registry->instance = 71;
		auto &record = registry->records[0];
		record.owner_thread = Thread::get_caller_id();
		record.snapshot.library_generation = 1;
		record.guard_enrollment = GuardEnrollment::QUALIFIED;
		record.private_refs = 3;
		auto &d = registry->device_records[0];
		d.object = &device;
		d.live = true;
		d.epoch = 1;
		d.execution_thread = Thread::get_caller_id();
		d.refs = 1;
		struct Holder : HolderRetirement {
			Holder() : HolderRetirement(RecordRef{}) {}
			~Holder() = default;
		} holder;
		holder.generation = 1;
		auto *bridge = memnew(Registry::QualificationBridge);
		bridge->serial = 11;
		bridge->epoch = 1;
		bridge->holder = &holder;
		bridge->children = 2;
		registry->qualification_bridges[0] = bridge;
		auto *child = memnew(Registry::QualificationBridge::Storage);
		bridge->storage[0] = child;
		child->serial = 31;
		child->retire_serial = 33;
		child->retiring = child->queued = true;
		child->physical.bridge = 11;
		child->physical.child = 31;
		child->physical.use_closed = true;
		RID rid = device._qualification_storage_create(128, &child->physical);
		REQUIRE(rid.is_valid());
		device.draw_graph.initialize(&driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
		device.draw_graph.begin();
		RenderingDeviceDriver::QueueCallReport chain;
		RenderingDeviceDriver::NativeCallFact call;
		call.serial = call.completion_serial = 51;
		chain.calls.push_back(call);
		device.frames[0].chain_reports.push_back(chain);
		registry->qualification_service(11, 31, 1, 1, 5000);
		CHECK_FALSE(child->queued);
		CHECK(child->physical.enqueued);
		CHECK(child->report.state == 2);
		CHECK(child->report.reason == 5);
		CHECK(child->report.native_error == FAILED);
		CHECK(device.frame_wait_native_code.load() == -21);
		CHECK(registry->qualification_storage_retire_release({ 71, 11 }, { 71, 33 }).native_error == ERR_BUSY);
		registry->qualification_retire_step(bridge, &device, 31, 1);
		CHECK(child->report.state == 2);
		CHECK(child->report.reason == 5);
		device._free_pending_resources(0);
		REQUIRE(child->physical.free_event != 0);
		registry->qualification_retire_step(bridge, &device, 31, 1);
		CHECK(child->report.state == 2);
		CHECK(child->report.return_status == 0);
		CHECK(child->report.native_error == FAILED);
		CHECK(registry->qualification_storage_retire_release({ 71, 11 }, { 71, 33 }).native_error == ERR_BUSY);
		device.frames[0].chain_reports.clear();
		registry->qualification_bridges[0] = nullptr;
		RenderingDevice::singleton = previous;
		device.is_main_instance = false;
		memdelete(child);
		memdelete(bridge);
		memdelete(registry);
	}
	static void service_error_before_shader_enqueue(RenderingDevice &device, RenderingDeviceDriver &driver, bool earlier_error, bool saturated, bool close_before) {
		using namespace GDExtensionDeviceExit;
		auto *registry = memnew(Registry);
		auto *previous = RenderingDevice::singleton;
		RenderingDevice::singleton = &device;
		device.is_main_instance = true;
		device.render_thread_id = Thread::get_caller_id();
		registry->instance = 71;
		auto &record = registry->records[0];
		record.owner_thread = Thread::get_caller_id();
		record.snapshot.library_generation = 1;
		record.guard_enrollment = GuardEnrollment::QUALIFIED;
		record.private_refs = 3;
		auto &d = registry->device_records[0];
		d.object = &device;
		d.live = true;
		d.epoch = 1;
		d.execution_thread = Thread::get_caller_id();
		d.refs = 1;
		struct Holder : HolderRetirement {
			Holder() : HolderRetirement(RecordRef{}) {}
			~Holder() = default;
		} holder;
		holder.generation = 1;
		auto *bridge = memnew(Registry::QualificationBridge);
		bridge->serial = 11;
		bridge->epoch = 1;
		bridge->holder = &holder;
		bridge->children = 2;
		registry->qualification_bridges[0] = bridge;
		auto *child = memnew(Registry::QualificationBridge::NativeUnit);
		bridge->units[0] = child;
		child->serial = 31;
		child->retire_serial = 33;
		child->retiring = child->queued = true;
		child->backing.known_mask = 1;
		child->report.report_revision = saturated ? UINT64_MAX : 1;
		if (earlier_error) {
			child->maintenance_error = ERR_CANT_ACQUIRE_RESOURCE;
			child->report.state = 2;
			child->report.reason = 5;
			child->report.native_error = ERR_CANT_ACQUIRE_RESOURCE;
		}
		child->shader.bridge = 11;
		child->shader.child = 31;
		child->shader.serial = 43;
		child->shader.use_closed = true;
		RenderingDevice::Shader shader;
		shader.driver_id = RenderingDeviceDriver::ShaderID(19);
		shader.qualification_return = &child->shader;
		RID rid = device.shader_owner.make_rid(shader);
		child->shader.rid = rid.get_id();
		device.draw_graph.initialize(&driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
		device.draw_graph.begin();
		RenderingDeviceDriver::QueueCallReport chain;
		RenderingDeviceDriver::NativeCallFact call;
		call.serial = call.completion_serial = 51;
		chain.calls.push_back(call);
		device.frames[0].chain_reports.push_back(chain);
		const auto original = child->report;
		if (close_before) {
			CHECK(registry->qualification_close({ 71, 11 }).native_error == ERR_BUSY);
			CHECK(child->queued);
			CHECK(bridge->children == 2);
		}
		registry->qualification_service(11, 31, 2, 1, 5000);
		CHECK(device.frame_wait_native_code.load() == -21);
		CHECK(child->maintenance_error == (earlier_error ? ERR_CANT_ACQUIRE_RESOURCE : FAILED));
		if (saturated) {
			CHECK(memcmp(&original, &child->report, sizeof(original)) == 0);
		} else {
			CHECK(child->report.state == 2);
			CHECK(child->report.native_error == (earlier_error ? ERR_CANT_ACQUIRE_RESOURCE : FAILED));
		}
		CHECK(device.shader_owner.owns(rid));
		CHECK_FALSE(child->shader.enqueued);
		CHECK(child->report.return_status == 0);
		CHECK(registry->qualification_native_unit_retire_release({ 71, 11 }, { 71, 33 }).native_error == ERR_BUSY);
		CHECK(registry->qualification_close({ 71, 11 }).native_error == ERR_BUSY);
		// The test fixture performs explicit cleanup after observing the service result.
		if (device.shader_owner.owns(rid)) {
			device._free_internal(rid);
		}
		device._free_pending_resources(0);
		device.frames[0].chain_reports.clear();
		registry->qualification_bridges[0] = nullptr;
		RenderingDevice::singleton = previous;
		device.is_main_instance = false;
		memdelete(child);
		memdelete(bridge);
		memdelete(registry);
	}
	static void native_revision_exhausted(RenderingDevice &device, RenderingDeviceDriver &driver) {
		using namespace GDExtensionDeviceExit;
		auto *bridge = memnew(Registry::QualificationBridge);
		auto *child = memnew(Registry::QualificationBridge::NativeUnit);
		bridge->units[0] = child;
		child->serial = 31;
		child->retiring = true;
		child->backing.known_mask = 1;
		child->report.report_revision = UINT64_MAX;
		child->report.reason = 1;
		child->shader.bridge = 11;
		child->shader.child = 31;
		child->shader.serial = 43;
		child->shader.use_closed = true;
		RenderingDevice::Shader shader;
		shader.driver_id = RenderingDeviceDriver::ShaderID(19);
		shader.qualification_return = &child->shader;
		RID rid = device.shader_owner.make_rid(shader);
		child->shader.rid = rid.get_id();
		device.draw_graph.initialize(&driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
		device.draw_graph.begin();
		const auto original = child->report;
		Registry::get_singleton().qualification_retire_step(bridge, &device, 31, 2);
		CHECK(device.shader_owner.owns(rid));
		CHECK_FALSE(child->shader.enqueued);
		CHECK(memcmp(&original, &child->report, sizeof(original)) == 0);
		if (device.shader_owner.owns(rid)) {
			device._free_internal(rid);
		}
		device._free_pending_resources(0);
		memdelete(child);
		memdelete(bridge);
	}
	static void unknown_while_dependent(RenderingDevice &device, RenderingDeviceDriver &driver, bool native, bool unresolved, uint32_t old_reason) {
		using namespace GDExtensionDeviceExit;
		auto *bridge = memnew(Registry::QualificationBridge);
		auto *storage = memnew(Registry::QualificationBridge::Storage);
		auto *unit = memnew(Registry::QualificationBridge::NativeUnit);
		bridge->storage[0] = storage;
		bridge->units[0] = unit;
		storage->serial = unit->serial = 31;
		storage->retiring = unit->retiring = true;
		storage->physical.bridge = unit->shader.bridge = 11;
		storage->physical.child = unit->shader.child = 31;
		storage->physical.use_closed = unit->shader.use_closed = true;
		RID rid;
		if (native) {
			unit->backing.known_mask = 1;
			unit->backing.possible_mask = unresolved ? 2 : 0;
			unit->shader.serial = 43;
			unit->report.report_revision = 1;
			unit->report.state = 2;
			unit->report.reason = old_reason;
			RenderingDevice::Shader shader;
			shader.driver_id = RenderingDeviceDriver::ShaderID(19);
			shader.qualification_return = &unit->shader;
			rid = device.shader_owner.make_rid(shader);
			REQUIRE(rid.is_valid());
			unit->shader.rid = rid.get_id();
			unit->shader.dependent_children = 1;
		} else {
			rid = device._qualification_storage_create(128, &storage->physical);
			REQUIRE(rid.is_valid());
			storage->report.state = 2;
			storage->report.reason = old_reason;
			storage->physical.dependent_children = 1;
		}
		device.draw_graph.initialize(&driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
		device.draw_graph.begin();
		auto &registry = Registry::get_singleton();
		registry.qualification_retire_step(bridge, &device, 31, native ? 2 : 1);
		if (native) {
			CHECK(unit->report.state == 2);
			CHECK(unit->report.reason == old_reason);
			CHECK(unit->report.return_status == 0);
			CHECK_FALSE(unit->shader.enqueued);
			unit->shader.dependent_children = 0;
			unit->backing.possible_mask = 0;
		} else {
			CHECK(storage->report.state == 2);
			CHECK(storage->report.reason == old_reason);
			CHECK(storage->report.return_status == 0);
			CHECK_FALSE(storage->physical.enqueued);
			storage->physical.dependent_children = 0;
		}
		registry.qualification_retire_step(bridge, &device, 31, native ? 2 : 1);
		if (native) {
			CHECK(unit->shader.enqueued);
		} else {
			CHECK(storage->physical.enqueued);
		}
		device._free_pending_resources(0);
		registry.qualification_retire_step(bridge, &device, 31, native ? 2 : 1);
		if (native) {
			CHECK(unit->shader.free_event != 0);
			CHECK(unit->report.state == 1);
			CHECK(unit->report.return_status == 1);
			CHECK(unit->report.reason == 0);
		} else {
			CHECK(storage->physical.free_event != 0);
			CHECK(storage->report.state == 1);
			CHECK(storage->report.return_status == 1);
			CHECK(storage->report.reason == 0);
		}
		// Keep fixture-owned return cells alive until any failed-path RID is disposed.
		if (native && device.shader_owner.owns(rid)) {
			device._free_internal(rid);
		}
		if (!native && device.storage_buffer_owner.owns(rid)) {
			device._free_internal(rid);
		}
		device._free_pending_resources(0);
		memdelete(storage);
		memdelete(unit);
		memdelete(bridge);
	}
	static void pipeline_identity_precedes_shader_pending(RenderingDevice &device, RenderingDeviceDriver &driver) {
		using namespace GDExtensionDeviceExit;
		auto *bridge = memnew(Registry::QualificationBridge);
		auto *child = memnew(Registry::QualificationBridge::NativeUnit);
		bridge->units[0] = child;
		child->serial = 31;
		child->retiring = true;
		child->backing.known_mask = 3;
		child->report.report_revision = 1;
		child->pipeline.rid = UINT64_MAX;
		child->pipeline.serial = 47;
		child->pipeline.bridge = 11;
		child->pipeline.child = 31;
		child->pipeline.use_closed = true;
		child->shader.serial = 43;
		child->shader.bridge = 11;
		child->shader.child = 31;
		child->shader.use_closed = true;
		child->shader.dependent_children = 1;
		RenderingDevice::Shader shader;
		shader.driver_id = RenderingDeviceDriver::ShaderID(19);
		shader.qualification_return = &child->shader;
		RID rid = device.shader_owner.make_rid(shader);
		REQUIRE(rid.is_valid());
		child->shader.rid = rid.get_id();
		device.draw_graph.initialize(&driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
		device.draw_graph.begin();
		Registry::get_singleton().qualification_retire_step(bridge, &device, 31, 2);
		CHECK(child->report.state == 2);
		CHECK(child->report.reason == 6);
		CHECK(child->report.return_status == 0);
		CHECK_FALSE(child->shader.enqueued);
		CHECK_FALSE(child->pipeline.enqueued);
		// The missing pipeline is an intentionally invalid fixture identity, not a native child.
		child->shader.dependent_children = 0;
		if (device.shader_owner.owns(rid)) {
			device._free_internal(rid);
		}
		device._free_pending_resources(0);
		memdelete(child);
		memdelete(bridge);
	}
	static void unknown_pipeline_keeps_shader(RenderingDevice &device, RenderingDeviceDriver &driver, int mode) {
		const bool possible = mode == 1;
		using namespace GDExtensionDeviceExit;
		auto *bridge = memnew(Registry::QualificationBridge);
		auto *child = memnew(Registry::QualificationBridge::NativeUnit);
		bridge->units[0] = child;
		child->serial = 31;
		child->retiring = true;
		child->backing.known_mask = possible ? 1 : 3;
		child->backing.possible_mask = possible ? 2 : 0;
		child->report.report_revision = 1;
		child->pipeline.rid = mode == 0 ? UINT64_MAX : 0;
		child->pipeline.serial = possible ? 0 : 47;
		child->pipeline.bridge = 11;
		child->pipeline.child = 31;
		child->pipeline.use_closed = true;
		child->shader.serial = 43;
		child->shader.bridge = 11;
		child->shader.child = 31;
		child->shader.use_closed = true;
		RenderingDevice::Shader shader;
		shader.driver_id = RenderingDeviceDriver::ShaderID(19);
		shader.qualification_return = &child->shader;
		RID rid = device.shader_owner.make_rid(shader);
		REQUIRE(rid.is_valid());
		child->shader.rid = rid.get_id();
		device.draw_graph.initialize(&driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
		device.draw_graph.begin();
		Registry::get_singleton().qualification_retire_step(bridge, &device, 31, 2);
		CHECK(child->report.state == 2);
		CHECK(child->report.reason == (possible ? 5 : 6));
		CHECK(child->report.return_status == 0);
		CHECK(device.shader_owner.owns(rid));
		CHECK_FALSE(child->shader.enqueued);
		CHECK(child->shader.free_event == 0);
		CHECK(child->report.freed_mask == 0);
		// Independent fixture cleanup does not assert that the unknown pipeline returned.
		if (device.shader_owner.owns(rid)) {
			device._free_internal(rid);
		}
		device._free_pending_resources(0);
		memdelete(child);
		memdelete(bridge);
	}
	static void mismatched_pipeline_event_is_not_returned(RenderingDevice &device, RenderingDeviceDriver &driver) {
		using namespace GDExtensionDeviceExit;
		auto *bridge = memnew(Registry::QualificationBridge);
		auto *child = memnew(Registry::QualificationBridge::NativeUnit);
		bridge->units[0] = child;
		child->serial = 31;
		child->retiring = true;
		child->backing.known_mask = 2;
		child->report.report_revision = 1;
		// A deliberately inconsistent fixture event is not evidence of a driver free.
		child->pipeline.rid = 0;
		child->pipeline.serial = 47;
		child->pipeline.enqueued = true;
		child->pipeline.frame_slot = 0;
		child->pipeline.frame_epoch = 71;
		child->pipeline.free_event = 59;
		device.draw_graph.initialize(&driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
		device.draw_graph.begin();
		Registry::get_singleton().qualification_retire_step(bridge, &device, 31, 2);
		CHECK(child->report.state == 2);
		CHECK(child->report.reason == 6);
		CHECK(child->report.return_status == 0);
		memdelete(child);
		memdelete(bridge);
	}
	static void native_nonerror_unknown_can_settle(RenderingDevice &device, RenderingDeviceDriver &driver) {
		using namespace GDExtensionDeviceExit;
		auto *bridge = memnew(Registry::QualificationBridge);
		auto *child = memnew(Registry::QualificationBridge::NativeUnit);
		bridge->units[0] = child;
		child->serial = 31;
		child->retiring = true;
		child->backing.known_mask = 1;
		child->shader.rid = 41;
		child->shader.serial = 43;
		child->shader.enqueued = true;
		child->shader.frame_slot = 0;
		child->shader.frame_epoch = 71;
		child->shader.free_event = 47;
		child->report.report_revision = 1;
		child->report.state = 2;
		child->report.reason = 5;
		device.draw_graph.initialize(&driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
		device.draw_graph.begin();
		Registry::get_singleton().qualification_retire_step(bridge, &device, 31, 2);
		CHECK(child->report.state == 1);
		CHECK(child->report.return_status == 1);
		CHECK(child->report.native_error == OK);
		CHECK(child->report.shader_free_event_seq == 47);
		CHECK(child->report.report_revision == 2);
		memdelete(child);
		memdelete(bridge);
	}
	static void native_retire_after_service_error(RenderingDevice &device, RenderingDeviceDriver &driver) {
		using namespace GDExtensionDeviceExit;
		auto *bridge = memnew(Registry::QualificationBridge);
		auto *child = memnew(Registry::QualificationBridge::NativeUnit);
		bridge->units[0] = child;
		child->serial = 31;
		child->retiring = true;
		child->backing.known_mask = 1;
		child->shader.rid = 41;
		child->shader.serial = 43;
		child->shader.enqueued = true;
		child->shader.frame_slot = 0;
		child->shader.frame_epoch = 71;
		child->report.report_revision = 1;
		child->maintenance_error = ERR_CANT_ACQUIRE_RESOURCE;
		child->report.state = 2;
		child->report.reason = 5;
		child->report.native_error = ERR_CANT_ACQUIRE_RESOURCE;
		device.draw_graph.initialize(&driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
		device.draw_graph.begin();
		auto &registry = Registry::get_singleton();
		registry.qualification_retire_step(bridge, &device, 31, 2);
		CHECK(child->report.state == 2);
		CHECK(child->report.return_status == 0);
		CHECK(child->report.reason == 5);
		CHECK(child->report.native_error == ERR_CANT_ACQUIRE_RESOURCE);
		// Isolated report reducer control: actual driver disposal is covered by
		// the storage fixture above, not asserted by this synthetic event.
		child->shader.free_event = 47;
		registry.qualification_retire_step(bridge, &device, 31, 2);
		CHECK(child->report.freed_mask == 1);
		CHECK(child->report.shader_free_event_seq == 47);
		CHECK(child->report.state == 2);
		CHECK(child->report.return_status == 0);
		CHECK(child->report.reason == 5);
		CHECK(child->report.native_error == ERR_CANT_ACQUIRE_RESOURCE);
		memdelete(child);
		memdelete(bridge);
	}
	static void storage_serial_drain_at_exhaustion(RenderingDevice &device, RenderingDeviceDriver &driver, uint32_t count) {
		using namespace GDExtensionDeviceExit;
		// Fixture-only registry enrollment and counter seeding; neither creates production authority.
		auto *registry = memnew(Registry);
		auto *previous = RenderingDevice::singleton;
		RenderingDevice::singleton = &device;
		device.is_main_instance = true;
		device.render_thread_id = Thread::get_caller_id();
		registry->instance = 71;
		auto &record = registry->records[0];
		record.owner_thread = Thread::get_caller_id();
		record.snapshot.library_generation = 1;
		record.guard_enrollment = GuardEnrollment::QUALIFIED;
		record.private_refs = 1;
		auto &d = registry->device_records[0];
		d.object = &device;
		d.live = true;
		d.epoch = 1;
		d.execution_thread = Thread::get_caller_id();
		d.refs = 1;
		struct Holder : HolderRetirement {
			Holder() : HolderRetirement(RecordRef{}) {}
			~Holder() = default;
		} holder;
		holder.generation = 1;
		holder.qualification_children = 1;
		auto *bridge = memnew(Registry::QualificationBridge);
		bridge->serial = 11;
		bridge->epoch = 1;
		bridge->holder = &holder;
		registry->qualification_bridges[0] = bridge;
		const GDExtensionQualificationBridgeRefV1 ref{ 71, 11 };
		device.draw_graph.initialize(&driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
		device.draw_graph.begin();
		registry->qualification_serials.next = UINT64_MAX - (count == 1 ? 1 : 4);
		GDExtensionStorageBackingV1 backing[2] = {};
		bool made[2] = {};
		for (uint32_t i = 0; i < count; ++i) {
			GDExtensionStorageCreateRequestV1 create{ { 1, 16, 0 }, 128, 3 + i, 1, 41, 1, 0 };
			const auto result = registry->qualification_storage_create(ref, &create, &backing[i]);
			made[i] = result.native_error == OK;
			if (!made[i]) {
				CHECK(backing[i].rid == 0);
				CHECK(backing[i].backing.handle_serial == 0);
			}
		}
		if (count == 2) {
			REQUIRE(made[0]);
			auto *child = bridge->storage[0];
			REQUIRE(child != nullptr);
			const GDExtensionStorageRetireRefV1 unpublished{ 71, child->retire_serial };
			const GDExtensionQualificationBudgetV1 budget{ { 1, 16, 0 }, 1, 1000, 0 };
			GDExtensionStorageRetireReportV1 hidden = {};
			CHECK(registry->qualification_storage_retire_observe(ref, unpublished, &budget, &hidden).native_error == ERR_DOES_NOT_EXIST);
			CHECK(registry->qualification_storage_retire_release(ref, unpublished).native_error == ERR_DOES_NOT_EXIST);
			// Simulate other original admissions consuming all remaining serials.
			registry->qualification_serials.exhausted = true;
		}
		for (uint32_t i = 0; i < count; ++i) {
			if (made[i]) {
				GDExtensionStorageRetireRequestV1 retire{ { 1, 16, 0 }, backing[i].backing, backing[i].rid, backing[i].backing_serial, 3 + i, 1, 41, 2, 0 };
				GDExtensionStorageRetireRefV1 retired = {};
				GDExtensionStorageRetireReportV1 report = {};
				auto result = registry->qualification_storage_retire(ref, &retire, &retired, &report);
				CHECK_MESSAGE(result.native_error == OK, "A successfully created backing must retain its original retirement capacity.");
				if (result.native_error != OK) {
					// Test teardown only: restore a separate unused sequence range so the
					// real fixture backing is physically disposed even on the failing arm.
					registry->qualification_serials.next = 101;
					registry->qualification_serials.exhausted = false;
					result = registry->qualification_storage_retire(ref, &retire, &retired, &report);
				}
				REQUIRE(result.native_error == OK);
				device._free_pending_resources(0);
				Registry::QualificationBridge::Storage *child = nullptr;
				for (auto *entry : bridge->storage) {
					if (entry && entry->serial == backing[i].backing.handle_serial) {
						child = entry;
						break;
					}
				}
				REQUIRE(child != nullptr);
				registry->qualification_retire_step(bridge, &device, child->serial, 1);
				const GDExtensionQualificationBudgetV1 budget{ { 1, 16, 0 }, 1, 1000, 0 };
				REQUIRE(registry->qualification_storage_retire_observe(ref, retired, &budget, &report).native_error == OK);
				CHECK(report.state == 1);
				REQUIRE(registry->qualification_storage_retire_release(ref, retired).native_error == OK);
			}
		}
		REQUIRE(registry->qualification_close(ref).native_error == OK);
		CHECK(holder.qualification_children == 0);
		RenderingDevice::singleton = previous;
		device.is_main_instance = false;
		memdelete(registry);
	}
	static void storage_free_event_drain_at_exhaustion(RenderingDevice &device, RenderingDeviceDriver &driver) {
		using namespace GDExtensionDeviceExit;
		// Fixture-only registry enrollment; the real original create/retire/observe calls run below.
		auto *registry = memnew(Registry);
		auto *previous = RenderingDevice::singleton;
		RenderingDevice::singleton = &device;
		device.is_main_instance = true;
		device.render_thread_id = Thread::get_caller_id();
		registry->instance = 71;
		auto &record = registry->records[0];
		record.owner_thread = Thread::get_caller_id();
		record.snapshot.library_generation = 1;
		record.guard_enrollment = GuardEnrollment::QUALIFIED;
		record.private_refs = 1;
		auto &d = registry->device_records[0];
		d.object = &device;
		d.live = true;
		d.epoch = 1;
		d.execution_thread = Thread::get_caller_id();
		d.refs = 1;
		struct Holder : HolderRetirement {
			Holder() : HolderRetirement(RecordRef{}) {}
			~Holder() = default;
		} holder;
		holder.generation = 1;
		holder.qualification_children = 1;
		auto *bridge = memnew(Registry::QualificationBridge);
		bridge->serial = 11;
		bridge->epoch = 1;
		bridge->holder = &holder;
		registry->qualification_bridges[0] = bridge;
		const GDExtensionQualificationBridgeRefV1 ref{ 71, 11 };
		device.draw_graph.initialize(&driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
		device.draw_graph.begin();
		device.next_qualification_free_event = UINT64_MAX;
		GDExtensionStorageBackingV1 backing[2] = {};
		bool made[2] = {};
		for (uint32_t i = 0; i < 2; ++i) {
			GDExtensionStorageCreateRequestV1 create{ { 1, 16, 0 }, 128, 3 + i, 1, 41, 1, 0 };
			const auto result = registry->qualification_storage_create(ref, &create, &backing[i]);
			made[i] = result.native_error == OK;
			if (!made[i]) {
				CHECK(backing[i].rid == 0);
				CHECK(backing[i].backing.handle_serial == 0);
			}
		}
		REQUIRE(made[0]);
		// The last event slot belongs to the accepted backing, so an unrelated
		// unreserved enqueue must not consume it before the backing retires.
		QualificationPhysicalReturn other;
		other.use_closed = true;
		device.qualification_retiring = true;
		const Error other_result = device._qualification_enqueue(&other);
		CHECK(other_result == ERR_BUSY);
		CHECK_FALSE(other.enqueued);
		if (other_result == OK) {
			device.next_qualification_free_event = UINT64_MAX; // fixture teardown only
		}
		for (uint32_t i = 0; i < 2; ++i) {
			if (made[i]) {
				GDExtensionStorageRetireRequestV1 retire{ { 1, 16, 0 }, backing[i].backing, backing[i].rid, backing[i].backing_serial, 3 + i, 1, 41, 2, 0 };
				GDExtensionStorageRetireRefV1 retired = {};
				GDExtensionStorageRetireReportV1 report = {};
				REQUIRE(registry->qualification_storage_retire(ref, &retire, &retired, &report).native_error == OK);
				device._free_pending_resources(0);
				Registry::QualificationBridge::Storage *child = nullptr;
				for (auto *entry : bridge->storage) {
					if (entry && entry->serial == backing[i].backing.handle_serial) {
						child = entry;
						break;
					}
				}
				REQUIRE(child != nullptr);
				registry->qualification_retire_step(bridge, &device, child->serial, 1);
				const GDExtensionQualificationBudgetV1 budget{ { 1, 16, 0 }, 1, 1000, 0 };
				// This fixture uses a private Registry, so inspecting its pending report
				// must not dispatch work through the process singleton's render server.
				report = child->report;
				CHECK_MESSAGE(report.state == 1, "Every accepted backing must have capacity for its real physical free event.");
				if (report.state != 1) {
					// Test teardown only: restore unused event numbers and drive the original physical free.
					device.next_qualification_free_event = 101;
					registry->qualification_retire_step(bridge, &device, child->serial, 1);
					device._free_pending_resources(0);
					registry->qualification_retire_step(bridge, &device, child->serial, 1);
					report = child->report;
				}
				REQUIRE(report.state == 1);
				REQUIRE(registry->qualification_storage_retire_observe(ref, retired, &budget, &report).native_error == OK);
				REQUIRE(registry->qualification_storage_retire_release(ref, retired).native_error == OK);
			}
		}
		REQUIRE(registry->qualification_close(ref).native_error == OK);
		CHECK(holder.qualification_children == 0);
		RenderingDevice::singleton = previous;
		device.is_main_instance = false;
		memdelete(registry);
	}
	static void storage_event_capacity_returns_after_create_failure(RenderingDevice &device, bool &fail_create) {
		using namespace GDExtensionDeviceExit;
		device.next_qualification_free_event = UINT64_MAX;
		QualificationPhysicalReturn failed;
		failed.bridge = 11;
		failed.child = 13;
		fail_create = true;
		RID missing = device._qualification_storage_create(128, &failed);
		CHECK(missing.is_null());
		CHECK_FALSE(failed.event_capacity_reserved);
		CHECK(device.qualification_free_event_reservations == 0);
		QualificationPhysicalReturn live;
		live.bridge = 11;
		live.child = 14;
		fail_create = false;
		RID rid = device._qualification_storage_create(128, &live);
		REQUIRE(rid.is_valid());
		CHECK(live.event_capacity_reserved);
		CHECK(device.qualification_free_event_reservations == 1);
		live.use_closed = true;
		device._free_internal(rid);
		device._free_pending_resources(0);
		CHECK(live.free_event == UINT64_MAX);
		CHECK(device.qualification_free_event_reservations == 0);
	}
	static void native_retire_identity_survives_exhaustion(RenderingDevice &device, RenderingDeviceDriver &driver, int &polls) {
		using namespace GDExtensionDeviceExit;
		// Synthetic enrollment and counter seeding; the original retire entrypoint
		// and the real frame disposer are exercised below.
		auto *registry = memnew(Registry);
		auto *previous = RenderingDevice::singleton;
		RenderingDevice::singleton = &device;
		device.is_main_instance = true;
		device.render_thread_id = Thread::get_caller_id();
		registry->instance = 71;
		auto &record = registry->records[0];
		record.owner_thread = Thread::get_caller_id();
		record.snapshot.library_generation = 1;
		record.guard_enrollment = GuardEnrollment::QUALIFIED;
		record.private_refs = 2;
		auto &d = registry->device_records[0];
		d.object = &device;
		d.live = true;
		d.epoch = 1;
		d.execution_thread = Thread::get_caller_id();
		d.refs = 1;
		struct Holder : HolderRetirement {
			Holder() : HolderRetirement(RecordRef{}) {}
			~Holder() = default;
		} holder;
		holder.generation = 1;
		auto *bridge = memnew(Registry::QualificationBridge);
		bridge->serial = 11;
		bridge->epoch = 1;
		bridge->holder = &holder;
		bridge->children = 1;
		registry->qualification_bridges[0] = bridge;
		auto *unit = memnew(Registry::QualificationBridge::NativeUnit);
		bridge->units[0] = unit;
		unit->serial = 31;
		unit->retire_serial = 33;
		unit->dependency_serial = 34;
		unit->creating = false;
		unit->backing.unit = { 71, 31 };
		unit->backing.known_mask = 1;
		unit->shader.bridge = 11;
		unit->shader.child = 31;
		unit->shader.serial = 43;
		RenderingDevice::Shader shader;
		shader.driver_id = RenderingDeviceDriver::ShaderID(19);
		shader.qualification_return = &unit->shader;
		RID rid = device.shader_owner.make_rid(shader);
		unit->shader.rid = rid.get_id();
		unit->backing.shader_rid = rid.get_id();
		unit->backing.shader_serial = 43;
		device.draw_graph.initialize(&driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RenderingDeviceDriver::CommandQueueFamilyID(1), 0);
		device.draw_graph.begin();
		const GDExtensionQualificationBudgetV1 budget{ { 1, 16, 0 }, 1, 1000, 0 };
		GDExtensionNativeUnitRetireReportV1 hidden = {};
		CHECK(registry->qualification_native_unit_retire_observe({ 71, 11 }, { 71, 33 }, &budget, &hidden).native_error == ERR_DOES_NOT_EXIST);
		CHECK(registry->qualification_native_unit_retire_release({ 71, 11 }, { 71, 33 }).native_error == ERR_DOES_NOT_EXIST);
		registry->qualification_serials.exhausted = true;
		polls = 1;
		GDExtensionNativeUnitRetireRequestV1 request{ { 1, 16, 0 }, { 71, 31 }, 43, 0 };
		GDExtensionNativeUnitRetireRefV1 retired = {};
		GDExtensionNativeUnitRetireReportV1 report = {};
		auto result = registry->qualification_native_unit_retire({ 71, 11 }, &request, &retired, &report);
		CHECK_MESSAGE(result.native_error == OK, "An accepted native unit must retain retirement identity after unrelated exhaustion.");
		if (result.native_error != OK) {
			registry->qualification_serials.next = 101;
			registry->qualification_serials.exhausted = false;
			result = registry->qualification_native_unit_retire({ 71, 11 }, &request, &retired, &report);
		}
		REQUIRE(result.native_error == OK);
		device._free_pending_resources(0);
		registry->qualification_retire_step(bridge, &device, 31, 2);
		CHECK(unit->shader.free_event != 0);
		CHECK(unit->report.state == 1);
		registry->qualification_bridges[0] = nullptr;
		RenderingDevice::singleton = previous;
		device.is_main_instance = false;
		memdelete(unit);
		memdelete(bridge);
		memdelete(registry);
	}
	static void resource_event_capacity_interleave(RenderingDevice &device, bool storage_first) {
		using namespace GDExtensionDeviceExit;
		device.next_qualification_free_event = UINT64_MAX - 2;
		device.render_thread_id = Thread::get_caller_id();
		QualificationPhysicalReturn buffer, texture, sampler;
		buffer.bridge = texture.bridge = sampler.bridge = 11;
		buffer.child = 13;
		texture.child = 31;
		sampler.child = 32;
		texture.serial = 43;
		sampler.serial = 44;
		RenderingDevice::TextureFormat format;
		format.format = RenderingDevice::DATA_FORMAT_R32G32B32A32_SFLOAT;
		format.texture_type = RenderingDevice::TEXTURE_TYPE_2D;
		format.width = format.height = 2;
		format.depth = format.array_layers = format.mipmaps = 1;
		format.samples = RenderingDevice::TEXTURE_SAMPLES_1;
		format.usage_bits = RenderingDevice::TEXTURE_USAGE_SAMPLING_BIT | RenderingDevice::TEXTURE_USAGE_CAN_UPDATE_BIT;
		RID buffer_rid, texture_rid, sampler_rid;
		if (storage_first) {
			buffer_rid = device._qualification_storage_create(128, &buffer);
		}
		texture_rid = create_qualified_texture(device, format, &texture, 0);
		sampler_rid = create_qualified_sampler(device, RenderingDevice::SamplerState(), &sampler, 0);
		if (!storage_first) {
			buffer_rid = device._qualification_storage_create(128, &buffer);
		}
		REQUIRE(buffer_rid.is_valid());
		REQUIRE(texture_rid.is_valid());
		REQUIRE(sampler_rid.is_valid());
		CHECK(buffer.event_capacity_reserved);
		CHECK(texture.event_capacity_reserved);
		CHECK(sampler.event_capacity_reserved);
		CHECK(device.qualification_free_event_reservations == 3);
		buffer.use_closed = texture.use_closed = sampler.use_closed = true;
		device.qualification_retiring = true;
		free_qualified_fixture(device, sampler_rid);
		free_qualified_fixture(device, buffer_rid);
		free_qualified_fixture(device, texture_rid);
		device._free_pending_resources(0);
		CHECK(buffer.free_event != 0);
		CHECK(texture.free_event != 0);
		CHECK(sampler.free_event != 0);
		CHECK(device.qualification_free_event_reservations == 0);
	}
	static void attach(RenderingDevice &device, RenderingDeviceDriver &driver) {
		device.driver = &driver;
		device.frames.resize(1);
		device.frames[0].qualification_epoch = 71;
		device.qualification_retiring = true;
	}
	static RID create(RenderingDevice &device, GDExtensionDeviceExit::QualificationPhysicalReturn &cell) {
		cell.bridge = 11;
		cell.child = 13;
		return device._qualification_storage_create(128, &cell);
	}
	static bool marked(RenderingDevice &device, RID rid, GDExtensionDeviceExit::QualificationPhysicalReturn &cell) {
		auto *b = device.storage_buffer_owner.get_or_null(rid);
		return b && b->qualification_return == &cell && cell.serial == b->backing_serial && cell.rid == rid.get_id();
	}
	static void enqueue(RenderingDevice &device, RID rid, GDExtensionDeviceExit::QualificationPhysicalReturn &cell) {
		cell.use_closed = true;
		device._free_internal(rid);
	}
	static uint64_t event(const GDExtensionDeviceExit::QualificationPhysicalReturn &cell) { return cell.free_event; }
	static void dispose(RenderingDevice &device) { device._free_pending_resources(0); }
	static RID dependent(RenderingDevice &device, GDExtensionDeviceExit::QualificationPhysicalReturn &cell) {
		RenderingDevice::UniformSet set;
		set.driver_id = RenderingDeviceDriver::UniformSetID(23);
		set.qualification_dependencies.push_back(&cell);
		cell.dependent_children.fetch_add(1);
		return device.uniform_set_owner.make_rid(set);
	}
	static uint32_t dependents(const GDExtensionDeviceExit::QualificationPhysicalReturn &cell) { return cell.dependent_children.load(); }
	static void reserve_unused_workers(RenderingDevice &device) { device.transfer_worker_pool.resize(4); }
	static Error worker_try(RenderingDevice &device) { return device._submit_transfer_workers({}, true); }
	static BinaryMutex &worker_mutex(RenderingDevice &device) { return device.transfer_worker_pool_mutex; }
	static void detach(RenderingDevice &device) {
		device.frames.clear();
		device.driver = nullptr;
	}
};
namespace TestQualificationPhysical {
class Driver : public TransferTestDriverBase {
public:
	GDExtensionDeviceExit::QualificationPhysicalReturn *cell = nullptr;
	int creates = 0, frees = 0, sets_freed = 0, texture_creates = 0, texture_frees = 0, texture_shared_calls = 0, sampler_creates = 0, sampler_frees = 0;
	bool fail_poll = false;
	bool fail_native_once = false;
	bool fail_buffer_create = false;
	std::atomic<bool> *release_start = nullptr, *release_locked = nullptr;
	int polls = 0;
	RenderingDevice *reentry_device = nullptr;
	RID reentry_target;
	mutable int reentry_checks = 0;
	int shaders_freed = 0;
	int release_one = 0, release_two = 0;
	Error native_call_release(uint64_t serial) override {
		if (serial == 101) {
			++release_one;
			if (release_start && release_locked) {
				release_start->store(true);
				const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
				while (!release_locked->load() && std::chrono::steady_clock::now() < deadline) {
					std::this_thread::yield();
				}
				if (!release_locked->load()) {
					return ERR_BUSY;
				}
			}
			return OK;
		}
		if (serial == 102) {
			++release_two;
			if (fail_native_once && release_two == 1) {
				return ERR_BUSY;
			}
			return OK;
		}
		return ERR_DOES_NOT_EXIST;
	}
	TextureID texture_create(const TextureFormat &, const TextureView &) override {
		++texture_creates;
		return TextureID(29);
	}
	TextureID texture_create_shared(TextureID, const TextureView &) override {
		++texture_shared_calls;
		return TextureID();
	}
	TextureID texture_create_shared_from_slice(TextureID, const TextureView &, TextureSliceType, uint32_t, uint32_t, uint32_t, uint32_t) override {
		++texture_shared_calls;
		return TextureID();
	}
	void texture_free(TextureID id) override {
		CHECK(id == TextureID(29));
		++texture_frees;
	}
	SamplerID sampler_create(const SamplerState &) override {
		++sampler_creates;
		return SamplerID(37);
	}
	void sampler_free(SamplerID id) override {
		CHECK(id == SamplerID(37));
		++sampler_frees;
	}
	uint64_t texture_get_allocation_size(TextureID) override { return 64; }
	BitField<TextureUsageBits> texture_get_usages_supported_by_format(DataFormat, bool) override { return TEXTURE_USAGE_SAMPLING_BIT | TEXTURE_USAGE_CAN_UPDATE_BIT; }
	uint64_t limit_get(Limit) override { return 4096; }
	uint64_t api_trait_get(ApiTrait) override { return 0; }
	void set_object_name(ObjectType, ID, const String &) override {}
	uint64_t get_resource_native_handle(DriverResource type, ID id) override {
		CHECK(type == DRIVER_RESOURCE_SAMPLER);
		return id.id;
	}
	bool native_call_observation_supported() const override { return true; }
	FencePollResult native_call_poll(uint64_t serial) override {
		FencePollResult result;
		if (fail_poll) {
			++polls;
			result.completed_serial = serial;
			result.native_error = true;
			result.native_code = -21;
		}
		return result;
	}
	bool shader_retire_ready_nonblocking(ShaderID) const override {
		if (reentry_device && ++reentry_checks == 2) {
			reentry_device->free_rid(reentry_target);
		}
		return polls > 0;
	}
	void shader_free(ShaderID id) override {
		CHECK(id == ShaderID(19));
		++shaders_freed;
	}
	uint32_t qualification_native_call_bound(uint32_t slots) const override { return slots; }
	void uniform_set_free(UniformSetID id) override {
		CHECK(id == UniformSetID(23));
		CHECK(TestQualificationPhysicalAccess::dependents(*cell) == 1);
		sets_freed++;
	}
	BufferID buffer_create(uint64_t size, BitField<BufferUsageBits>, MemoryAllocationType, uint64_t) override {
		CHECK(size == 128);
		creates++;
		return fail_buffer_create ? BufferID() : BufferID(19);
	}
	void buffer_free(BufferID id) override {
		CHECK(id == BufferID(19));
		if (cell) {
			CHECK(TestQualificationPhysicalAccess::event(*cell) == 0);
		}
		frees++;
	}
};
TEST_CASE("[RenderingDevice][Qualification] invalidation retains original callback when uniform set slot is reused") {
	RenderingDevice device;
	Driver driver;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::invalidation_after_slot_reuse(device);
	TestQualificationPhysicalAccess::detach(device);
}
TEST_CASE("[RenderingDevice] framebuffer invalidation is synchronous and reentry does not duplicate disposal") {
	RenderingDevice device;
	Driver driver;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::framebuffer_invalidation_once(device);
	TestQualificationPhysicalAccess::detach(device);
}
TEST_CASE("[RenderingDevice] parent disposal remains active through child invalidation") {
	RenderingDevice device;
	Driver driver;
	TestQualificationPhysicalAccess::attach(device, driver);
	const bool reenter = OS::get_singleton()->get_environment("GODOT_TEST_RD_CASCADE_REENTRY") == "race";
	TestQualificationPhysicalAccess::cascade_reentry(device, reenter);
	TestQualificationPhysicalAccess::detach(device);
}
TEST_CASE("[RenderingDevice] child invalidation can retire a sibling during parent disposal") {
	RenderingDevice device;
	Driver driver;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::cascade_sibling_reentry(device);
	TestQualificationPhysicalAccess::detach(device);
}
TEST_CASE("[RenderingDevice] late busy dependent preserves parent and edge until retry") {
	RenderingDevice device;
	Driver driver;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::cascade_late_busy_retry(device);
	TestQualificationPhysicalAccess::detach(device);
}
TEST_CASE("[RenderingDevice] refused pipeline enqueue preserves shader parent and edge") {
	RenderingDevice device;
	Driver driver;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::cascade_refused_pipeline_enqueue(device);
	TestQualificationPhysicalAccess::detach(device);
}
TEST_CASE("[RenderingDevice] pipeline enqueue guards reject closed-cell blocked states") {
	for (int mode = 3; mode <= 6; mode++) {
		RenderingDevice device;
		Driver driver;
		TestQualificationPhysicalAccess::attach(device, driver);
		TestQualificationPhysicalAccess::pipeline_owner_and_enqueue_guard(device, mode);
		TestQualificationPhysicalAccess::detach(device);
	}
}
TEST_CASE("[RenderingDevice] pipeline pending owner use preserves parent") {
	RenderingDevice device;
	Driver driver;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::pipeline_owner_and_enqueue_guard(device, 1);
	TestQualificationPhysicalAccess::detach(device);
}
TEST_CASE("[RenderingDevice] pipeline native dependent preserves parent") {
	RenderingDevice device;
	Driver driver;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::pipeline_owner_and_enqueue_guard(device, 2);
	TestQualificationPhysicalAccess::detach(device);
}
TEST_CASE("[RenderingDevice] native unit retirement respects pending pipeline owner conditions") {
	for (int mode = 1; mode <= 2; mode++) {
		RenderingDevice device;
		Driver driver;
		TestQualificationPhysicalAccess::attach(device, driver);
		TestQualificationPhysicalAccess::pipeline_direct_owner_guard(device, mode);
		TestQualificationPhysicalAccess::detach(device);
	}
}
TEST_CASE("[RenderingDevice] qualification retirement is scoped to the original pipeline cell and RID") {
	for (int mode = 0; mode < 2; mode++) {
		RenderingDevice device;
		Driver driver;
		TestQualificationPhysicalAccess::attach(device, driver);
		TestQualificationPhysicalAccess::pipeline_scoped_target(device, mode != 0);
		TestQualificationPhysicalAccess::detach(device);
	}
}
TEST_CASE("[RenderingDevice] native unit retirement restores an outer retirement scope") {
	RenderingDevice device;
	Driver driver;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::pipeline_scope_restore(device);
	TestQualificationPhysicalAccess::detach(device);
}
TEST_CASE("[RenderingDevice] original owner scope rejects a foreign pipeline on driver reentry") {
	RenderingDevice device;
	Driver driver;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::owner_driver_reentry(device, driver);
	TestQualificationPhysicalAccess::detach(device);
}
TEST_CASE("[RenderingDevice] busy dependent preserves its parent until retry") {
	RenderingDevice device;
	Driver driver;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::cascade_busy_dependent_retry(device);
	TestQualificationPhysicalAccess::detach(device);
}
TEST_CASE("[RenderingDevice][Qualification] buffer return follows exact physical free and survives RID erase") {
	RenderingDevice device;
	Driver driver;
	GDExtensionDeviceExit::QualificationPhysicalReturn cell;
	driver.cell = &cell;
	TestQualificationPhysicalAccess::attach(device, driver);
	RID rid = TestQualificationPhysicalAccess::create(device, cell);
	CHECK(rid.is_valid());
	CHECK(TestQualificationPhysicalAccess::marked(device, rid, cell));
	TestQualificationPhysicalAccess::enqueue(device, rid, cell);
	CHECK_FALSE(TestQualificationPhysicalAccess::marked(device, rid, cell));
	CHECK(TestQualificationPhysicalAccess::event(cell) == 0);
	CHECK(driver.frees == 0);
	TestQualificationPhysicalAccess::dispose(device);
	CHECK(driver.frees == 1);
	const auto event = TestQualificationPhysicalAccess::event(cell);
	CHECK(event != 0);
	TestQualificationPhysicalAccess::dispose(device);
	CHECK(driver.frees == 1);
	CHECK(TestQualificationPhysicalAccess::event(cell) == event);
	TestQualificationPhysicalAccess::detach(device);
}
TEST_CASE("[RenderingDevice][Qualification] dependent descriptor pin survives RID erase until driver return") {
	RenderingDevice device;
	Driver driver;
	GDExtensionDeviceExit::QualificationPhysicalReturn cell;
	driver.cell = &cell;
	TestQualificationPhysicalAccess::attach(device, driver);
	RID set = TestQualificationPhysicalAccess::dependent(device, cell);
	device.free_rid(set);
	CHECK(TestQualificationPhysicalAccess::dependents(cell) == 1);
	CHECK(driver.sets_freed == 0);
	TestQualificationPhysicalAccess::dispose(device);
	CHECK(driver.sets_freed == 1);
	CHECK(TestQualificationPhysicalAccess::dependents(cell) == 0);
	TestQualificationPhysicalAccess::detach(device);
}
TEST_CASE("[RenderingDevice][Qualification] unsupported checked descriptor return has no driver effect") {
	TransferTestDriverBase driver;
	const auto result = driver.qualification_uniform_set_free(RenderingDeviceDriver::UniformSetID(23));
	CHECK(result.state == RenderingDeviceDriver::QualificationDescriptorReturnState::Unavailable);
	CHECK(result.native_code == 0);
}
TEST_CASE("[Qualification] retire keeps exact original RID live while lists or callbacks are active") {
	for (uint32_t mode = 1; mode <= 4; ++mode) {
		RenderingDevice device;
		Driver driver;
		TestQualificationPhysicalAccess::attach(device, driver);
		TestQualificationPhysicalAccess::retire_window(device, driver, mode);
		CHECK(driver.frees == 1);
		TestQualificationPhysicalAccess::detach(device);
	}
}
#ifdef THREADS_ENABLED
TEST_CASE("[RenderingDevice][Qualification] contended transfer lock refuses maintenance before driver effect") {
	RenderingDevice device;
	Driver driver;
	TestQualificationPhysicalAccess::attach(device, driver);
	auto &mutex = TestQualificationPhysicalAccess::worker_mutex(device);
	mutex.lock();
	Error result = OK;
	std::thread worker([&] { result = TestQualificationPhysicalAccess::worker_try(device); });
	worker.join();
	CHECK(result == ERR_BUSY);
	mutex.unlock();
	TestQualificationPhysicalAccess::reserve_unused_workers(device);
	CHECK(TestQualificationPhysicalAccess::worker_try(device) == OK);
	TestQualificationPhysicalAccess::detach(device);
}
#endif

} //namespace TestQualificationPhysical

TEST_CASE("[Qualification][E05] accepted storage retains retirement identities near registry exhaustion") {
	for (uint32_t count = 1; count <= 2; ++count) {
		RenderingDevice device;
		TestQualificationPhysical::Driver driver;
		TestQualificationPhysicalAccess::attach(device, driver);
		TestQualificationPhysicalAccess::storage_serial_drain_at_exhaustion(device, driver, count);
		CHECK(driver.creates == driver.frees);
		TestQualificationPhysicalAccess::detach(device);
	}
}

TEST_CASE("[Qualification][E05] accepted storage retains physical event capacity at exhaustion") {
	RenderingDevice device;
	TestQualificationPhysical::Driver driver;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::storage_free_event_drain_at_exhaustion(device, driver);
	CHECK(driver.creates == driver.frees);
	TestQualificationPhysicalAccess::detach(device);
}

TEST_CASE("[Qualification][E05] failed storage creation releases reserved event capacity") {
	RenderingDevice device;
	TestQualificationPhysical::Driver driver;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::storage_event_capacity_returns_after_create_failure(device, driver.fail_buffer_create);
	CHECK(driver.creates == 2);
	CHECK(driver.frees == 1);
	TestQualificationPhysicalAccess::detach(device);
}

TEST_CASE("[Qualification][E05] QRT create error bytes do not identify driver attempt") {
	for (bool post_driver : { false, true }) {
		RenderingDevice device;
		TestQualificationPhysical::Driver driver;
		TestQualificationPhysicalAccess::attach(device, driver);
		TestQualificationPhysicalAccess::qrt_create_error_has_same_delivery_before_and_after_driver(device, driver, driver.fail_buffer_create, driver.creates, post_driver);
		CHECK(driver.creates == (post_driver ? 1 : 0));
		CHECK(driver.frees == 0);
		TestQualificationPhysicalAccess::detach(device);
	}
}

TEST_CASE("[Qualification][E05] failed create at caller limit leaves final serial for existing retirement") {
	for (bool post_driver : { false, true }) {
		RenderingDevice device;
		TestQualificationPhysical::Driver driver;
		TestQualificationPhysicalAccess::attach(device, driver);
		TestQualificationPhysicalAccess::qrt_create_error_has_same_delivery_before_and_after_driver(device, driver, driver.fail_buffer_create, driver.creates, post_driver, 1);
		CHECK(driver.creates == (post_driver ? 2 : 1));
		CHECK(driver.frees == 1);
		TestQualificationPhysicalAccess::detach(device);
	}
}

TEST_CASE("[Qualification][E05] failed create reserves two final caller serials for published parts") {
	for (bool post_driver : { false, true }) {
		RenderingDevice device;
		TestQualificationPhysical::Driver driver;
		TestQualificationPhysicalAccess::attach(device, driver);
		TestQualificationPhysicalAccess::qrt_create_error_has_same_delivery_before_and_after_driver(device, driver, driver.fail_buffer_create, driver.creates, post_driver, 2);
		CHECK(driver.creates == (post_driver ? 3 : 2));
		CHECK(driver.frees == 2);
		TestQualificationPhysicalAccess::detach(device);
	}
}

TEST_CASE("[Qualification][E05] event capacity protects previously accepted resource kinds") {
	for (bool storage_first : { false, true }) {
		RenderingDevice device;
		TestQualificationPhysical::Driver driver;
		TestQualificationPhysicalAccess::attach(device, driver);
		TestQualificationPhysicalAccess::resource_event_capacity_interleave(device, storage_first);
		CHECK(driver.frees == 1);
		CHECK(driver.texture_frees == 1);
		CHECK(driver.sampler_frees == 1);
		TestQualificationPhysicalAccess::detach(device);
	}
}

TEST_CASE("[Qualification] accepted native unit retains retirement identity after sequence exhaustion") {
	RenderingDevice device;
	TestQualificationPhysical::Driver driver;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::native_retire_identity_survives_exhaustion(device, driver, driver.polls);
	CHECK(driver.shaders_freed == 1);
	TestQualificationPhysicalAccess::detach(device);
}

TEST_CASE("[Qualification] retired E05 backing rejects same prepared upload and preserves RT release and bridge teardown") {
	RenderingDevice device;
	TestQualificationPhysical::Driver driver;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::retired_prepared_upload(device, driver);
	CHECK(driver.creates == 1);
	CHECK(driver.frees == 1);
	TestQualificationPhysicalAccess::detach(device);
}

TEST_CASE("[Qualification] storage retire retains unknown after maintenance failure and physical free") {
	RenderingDevice device;
	TestQualificationPhysical::Driver driver;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::retire_after_service_error(device, driver);
	CHECK(driver.creates == 1);
	CHECK(driver.frees == 1);
	TestQualificationPhysicalAccess::detach(device);
}

TEST_CASE("[Qualification] native unit retire reducer retains maintenance unknown on observed event") {
	RenderingDevice device;
	TestQualificationPhysical::Driver driver;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::native_retire_after_service_error(device, driver);
	TestQualificationPhysicalAccess::detach(device);
}

TEST_CASE("[Qualification] service native poll error remains unknown after storage free") {
	RenderingDevice device;
	TestQualificationPhysical::Driver driver;
	driver.fail_poll = true;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::service_poll_error(device, driver);
	CHECK(driver.creates == 1);
	CHECK(driver.frees == 1);
	TestQualificationPhysicalAccess::detach(device);
}

TEST_CASE("[Qualification] storage maintenance unknown prevents new retire enqueue") {
	RenderingDevice device;
	TestQualificationPhysical::Driver driver;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::unknown_does_not_enqueue(device, driver);
	CHECK(driver.creates == 1);
	CHECK(driver.frees == 1);
	TestQualificationPhysicalAccess::detach(device);
}

TEST_CASE("[Qualification] service poll error precedes new native shader retirement") {
	RenderingDevice device;
	TestQualificationPhysical::Driver driver;
	driver.fail_poll = true;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::service_error_before_shader_enqueue(device, driver, false, false, false);
	CHECK(driver.polls == 1);
	CHECK(driver.shaders_freed == 1);
	TestQualificationPhysicalAccess::detach(device);
}

TEST_CASE("[Qualification] service preserves first native retire maintenance error") {
	RenderingDevice device;
	TestQualificationPhysical::Driver driver;
	driver.fail_poll = true;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::service_error_before_shader_enqueue(device, driver, true, false, false);
	CHECK(driver.polls == 1);
	CHECK(driver.shaders_freed == 1);
	TestQualificationPhysicalAccess::detach(device);
}

TEST_CASE("[Qualification] native unit exhausted retire revision freezes report and prevents new enqueue") {
	RenderingDevice device;
	TestQualificationPhysical::Driver driver;
	driver.polls = 1;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::native_revision_exhausted(device, driver);
	CHECK(driver.shaders_freed == 1);
	TestQualificationPhysicalAccess::detach(device);
}
TEST_CASE("[Qualification] native unit unknown without maintenance error accepts original later event") {
	RenderingDevice device;
	TestQualificationPhysical::Driver driver;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::native_nonerror_unknown_can_settle(device, driver);
	TestQualificationPhysicalAccess::detach(device);
}

TEST_CASE("[Qualification] exhausted native report keeps service error private and close busy") {
	RenderingDevice device;
	TestQualificationPhysical::Driver driver;
	driver.fail_poll = true;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::service_error_before_shader_enqueue(device, driver, false, true, false);
	CHECK(driver.polls == 1);
	CHECK(driver.shaders_freed == 1);
	TestQualificationPhysicalAccess::detach(device);
}
TEST_CASE("[Qualification] queued native service retains maintenance error across close") {
	RenderingDevice device;
	TestQualificationPhysical::Driver driver;
	driver.fail_poll = true;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::service_error_before_shader_enqueue(device, driver, false, false, true);
	CHECK(driver.polls == 1);
	CHECK(driver.shaders_freed == 1);
	TestQualificationPhysicalAccess::detach(device);
}

TEST_CASE("[Qualification] queued native close retains preexisting maintenance error") {
	RenderingDevice device;
	TestQualificationPhysical::Driver driver;
	driver.fail_poll = true;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::service_error_before_shader_enqueue(device, driver, true, false, true);
	CHECK(driver.polls == 1);
	CHECK(driver.shaders_freed == 1);
	TestQualificationPhysicalAccess::detach(device);
}

TEST_CASE("[Qualification] ticket ack removes only its original reverse uses after RD contention") {
	RenderingDevice device;
	TestQualificationPhysical::Driver driver;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::ticket_reverse_unlink(device, driver);
	CHECK(driver.creates == 1);
	CHECK(driver.frees == 1);
	TestQualificationPhysicalAccess::detach(device);
}

TEST_CASE("[Qualification] ticket ack rejects damaged or duplicate reverse edges without partial unlink") {
	for (uint32_t mode = 0; mode < 5; ++mode) {
		RenderingDevice device;
		TestQualificationPhysicalAccess::ticket_bad_reverse_edges(device, mode);
	}
}

TEST_CASE("[Qualification] ticket native release retry retains reverse edges until all observers return") {
	RenderingDevice device;
	TestQualificationPhysical::Driver driver;
	driver.fail_native_once = true;
	TestQualificationPhysicalAccess::ticket_native_release_retry(device, driver);
	CHECK(driver.release_one == 1);
	CHECK(driver.release_two == 2);
}

#ifdef THREADS_ENABLED
TEST_CASE("[Qualification] empty ticket ack preserves progress during RD contention") {
	RenderingDevice device;
	TestQualificationPhysicalAccess::ticket_empty_release_during_rd_contention(device);
}
TEST_CASE("[Qualification] ticket observer return survives later RD contention without replay") {
	RenderingDevice device;
	TestQualificationPhysical::Driver driver;
	std::atomic<bool> start = false, locked = false;
	driver.release_start = &start;
	driver.release_locked = &locked;
	TestQualificationPhysicalAccess::ticket_relock_busy(device, driver, start, locked);
	CHECK(driver.release_one == 1);
}
#endif

TEST_CASE("[Qualification] foreign bridge physical child cannot acquire ticket reverse use") {
	RenderingDevice device;
	TestQualificationPhysical::Driver driver;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::foreign_bridge_pin_is_rejected(device);
	CHECK(driver.creates == 1);
	CHECK(driver.frees == 1);
	TestQualificationPhysicalAccess::detach(device);
}

TEST_CASE("[Qualification] non-error Unknown remains well-formed while a dependent blocks retirement") {
	for (int variant = 0; variant < 6; ++variant) {
		const bool native = variant % 3 != 0;
		RenderingDevice device;
		TestQualificationPhysical::Driver driver;
		driver.polls = 1;
		TestQualificationPhysicalAccess::attach(device, driver);
		TestQualificationPhysicalAccess::unknown_while_dependent(device, driver, native, variant % 3 == 2, variant < 3 ? 6 : 5);
		CHECK((native ? driver.shaders_freed == 1 : driver.frees == 1));
		TestQualificationPhysicalAccess::detach(device);
	}
}

TEST_CASE("[Qualification] unit pipeline identity failure is not masked by shader pending") {
	RenderingDevice device;
	TestQualificationPhysical::Driver driver;
	driver.polls = 1;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::pipeline_identity_precedes_shader_pending(device, driver);
	CHECK(driver.shaders_freed == 1);
	TestQualificationPhysicalAccess::detach(device);
}

TEST_CASE("[Qualification] unknown pipeline preserves its original shader") {
	for (int mode = 0; mode < 3; ++mode) {
		RenderingDevice device;
		TestQualificationPhysical::Driver driver;
		driver.polls = 1;
		TestQualificationPhysicalAccess::attach(device, driver);
		TestQualificationPhysicalAccess::unknown_pipeline_keeps_shader(device, driver, mode);
		CHECK(driver.shaders_freed == 1);
		TestQualificationPhysicalAccess::detach(device);
	}
}

TEST_CASE("[Qualification] mismatched pipeline cell cannot publish Returned from an event") {
	RenderingDevice device;
	TestQualificationPhysical::Driver driver;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::mismatched_pipeline_event_is_not_returned(device, driver);
	TestQualificationPhysicalAccess::detach(device);
}

TEST_CASE("[Qualification][E05] QRT cleanup release preserves retry and stale-ref safety") {
	RenderingDevice device;
	TestQualificationPhysical::Driver driver;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::qrt_cleanup_release_preserves_retry(device, driver, true);
	CHECK(driver.creates == driver.frees);
	TestQualificationPhysicalAccess::detach(device);
}

TEST_CASE("[Qualification][E05] QRT cleanup retries reject old refs with available serials") {
	RenderingDevice device;
	TestQualificationPhysical::Driver driver;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::qrt_cleanup_release_preserves_retry(device, driver, false);
	CHECK(driver.creates == driver.frees);
	TestQualificationPhysicalAccess::detach(device);
}

TEST_CASE("[Qualification][E05] QRT cleanup capacity excludes unrelated admissions") {
	RenderingDevice device;
	TestQualificationPhysical::Driver driver;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::qrt_cleanup_capacity_blocks_unrelated_admission(device, driver);
	CHECK(driver.creates == driver.frees);
	TestQualificationPhysicalAccess::detach(device);
}

TEST_CASE("[Qualification][E05] last QRT serial delivers original retirement before release") {
	RenderingDevice device;
	TestQualificationPhysical::Driver driver;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::qrt_last_serial_delivers_original_retire(device, driver);
	CHECK(driver.creates == driver.frees);
	TestQualificationPhysicalAccess::detach(device);
}

TEST_CASE("[Qualification][E05] last QRT serial retries observed no-effect dispatch failure") {
	RenderingDevice device;
	TestQualificationPhysical::Driver driver;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::qrt_last_serial_delivers_original_retire(device, driver, true);
	CHECK(driver.creates == driver.frees);
	TestQualificationPhysicalAccess::detach(device);
}

TEST_CASE("[Qualification][E05] conflicting cleanup waits for original owner before next child") {
	RenderingDevice device;
	TestQualificationPhysical::Driver driver;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::qrt_conflicting_retire_waits_for_original_owner(device, driver);
	CHECK(driver.creates == 2);
	CHECK(driver.frees == 2);
	TestQualificationPhysicalAccess::detach(device);
}

TEST_CASE("[Qualification][E05] accepted storage retains cleanup at private reference capacity") {
	RenderingDevice device;
	TestQualificationPhysical::Driver driver;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::qrt_cleanup_survives_private_capacity(device, driver);
	CHECK(driver.creates == driver.frees);
	TestQualificationPhysicalAccess::detach(device);
}

TEST_CASE("[Qualification][E05] last cleanup slot returns reserved private references") {
	RenderingDevice device;
	TestQualificationPhysical::Driver driver;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::qrt_slot_last_returns_private_credits(device, driver);
	CHECK(driver.creates == 1);
	CHECK(driver.frees == 1);
	TestQualificationPhysicalAccess::detach(device);
}

TEST_CASE("[Qualification][E05] prepared ticket requires cleanup capacity") {
	RenderingDevice device;
	TestQualificationPhysical::Driver driver;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::prepared_ticket_cleanup_capacity(device, driver);
	TestQualificationPhysicalAccess::detach(device);
}

TEST_CASE("[Qualification][E05] saturated ticket transport drains after cancellation") {
	RenderingDevice device;
	TestQualificationPhysical::Driver driver;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::saturated_ticket_transport(device, driver, false);
	TestQualificationPhysicalAccess::detach(device);
}

TEST_CASE("[Qualification][E05] saturated ticket record transport drains after cancellation") {
	RenderingDevice device;
	TestQualificationPhysical::Driver driver;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::saturated_ticket_transport(device, driver, true);
	TestQualificationPhysicalAccess::detach(device);
}

TEST_CASE("[Qualification][E05] ticket transport retries after shared queue credit returns") {
	RenderingDevice device;
	TestQualificationPhysical::Driver driver;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::saturated_ticket_transport(device, driver, false, true);
	TestQualificationPhysicalAccess::detach(device);
}

#ifdef THREADS_ENABLED
TEST_CASE("[Qualification][E05] full server queue retries ticket cleanup and transport") {
	RenderingDevice device;
	TestQualificationPhysical::Driver driver;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::saturated_ticket_transport(device, driver, false, false, true);
	TestQualificationPhysicalAccess::detach(device);
}
#endif

TEST_CASE("[Qualification][E05] accepted storage retains QRT cleanup at registry exhaustion") {
	RenderingDevice device;
	TestQualificationPhysical::Driver driver;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::storage_qrt_retire_reservation_survives_exhaustion(device, driver);
	CHECK(driver.creates == driver.frees);
	TestQualificationPhysicalAccess::detach(device);
}

TEST_CASE("[Qualification][T1] original texture owner reports its real driver return") {
	RenderingDevice device;
	TestQualificationPhysical::Driver driver;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::texture_original_return(device);
	CHECK(driver.texture_creates == 1);
	CHECK(driver.texture_frees == 1);
	TestQualificationPhysicalAccess::detach(device);
}

TEST_CASE("[Qualification][T1] original sampler owner reports its real driver return") {
	RenderingDevice device;
	TestQualificationPhysical::Driver driver;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::sampler_original_return(device);
	CHECK(driver.sampler_creates == 1);
	CHECK(driver.sampler_frees == 1);
	TestQualificationPhysicalAccess::detach(device);
}

TEST_CASE("[Qualification][T1] original sampler is retained by its ticket while in use") {
	RenderingDevice device;
	TestQualificationPhysical::Driver driver;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::sampler_ticket_pin(device);
	CHECK(driver.sampler_creates == 1);
	CHECK(driver.sampler_frees == 1);
	TestQualificationPhysicalAccess::detach(device);
}

TEST_CASE("[Qualification][T1] marked sampler rejects legacy access and implicit parent return") {
	RenderingDevice device;
	TestQualificationPhysical::Driver driver;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::sampler_legacy_and_parent_refusal(device);
	CHECK(driver.sampler_creates == 1);
	CHECK(driver.sampler_frees == 1);
	CHECK(driver.frees == 1);
	TestQualificationPhysicalAccess::detach(device);
}

TEST_CASE("[Qualification][T1] ordinary sampler retains its original lifecycle") {
	RenderingDevice device;
	TestQualificationPhysical::Driver driver;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::ordinary_sampler_lifecycle(device);
	CHECK(driver.sampler_creates == 1);
	CHECK(driver.sampler_frees == 1);
	TestQualificationPhysicalAccess::detach(device);
}

TEST_CASE("[Qualification][T1] sampler creation refuses suspended recording before driver effects") {
	RenderingDevice device;
	TestQualificationPhysical::Driver driver;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::sampler_refuses_suspended_recording(device);
	CHECK(driver.sampler_creates == 0);
	TestQualificationPhysicalAccess::detach(device);
}

TEST_CASE("[Qualification][T1] marked texture refuses legacy sharing and upload") {
	RenderingDevice device;
	TestQualificationPhysical::Driver driver;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::texture_rejects_unowned_legacy_operations(device, driver);
	CHECK(driver.texture_shared_calls == 0);
	CHECK(driver.texture_frees == 1);
	TestQualificationPhysicalAccess::detach(device);
}

TEST_CASE("[Qualification][T1] marked texture rejects driver callbacks before an ordinary prefix") {
	RenderingDevice device;
	TestQualificationPhysical::Driver driver;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::texture_rejects_driver_callback_before_prefix(device, driver);
	CHECK(driver.texture_frees == 1);
	TestQualificationPhysicalAccess::detach(device);
}

TEST_CASE("[Qualification][T1] worker refusal preserves texture dependencies") {
	RenderingDevice device;
	TestQualificationPhysical::Driver driver;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::texture_worker_refusal_preserves_dependencies(device);
	CHECK(driver.texture_frees == 1);
	CHECK(driver.frees == 1);
	TestQualificationPhysicalAccess::detach(device);
}

TEST_CASE("[Qualification][T1] original texture is retained by its ticket while in use") {
	RenderingDevice device;
	TestQualificationPhysical::Driver driver;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::texture_ticket_pin(device);
	CHECK(driver.texture_frees == 1);
	TestQualificationPhysicalAccess::detach(device);
}

TEST_CASE("[Qualification][T1] ordinary texture cannot become a qualification resource") {
	RenderingDevice device;
	TestQualificationPhysical::Driver driver;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::ordinary_texture_cannot_be_qualification_pin(device);
	CHECK(driver.texture_frees == 1);
	TestQualificationPhysicalAccess::detach(device);
}

#if defined(METAL_ENABLED) && defined(MACOS_ENABLED)
TEST_CASE("[Qualification][T1] Metal original texture owner physically returns an empty RGBA32F texture") {
	NS::SharedPtr<MTL::Device> available = NS::TransferPtr(MTL::CreateSystemDefaultDevice());
	REQUIRE(available.get() != nullptr);
	RenderingContextDriverMetal context;
	REQUIRE(context.initialize() == OK);
	MTL3::RenderingDeviceDriverMetal driver(&context);
	REQUIRE(driver.initialize(0, 2) == OK);
	INFO("Native Metal device: ", available->name()->utf8String());
	RenderingDevice device;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::texture_original_return(device);
	TestQualificationPhysicalAccess::detach(device);
}
TEST_CASE("[Qualification][T1] Metal original sampler owner physically returns a sampler") {
	NS::SharedPtr<MTL::Device> available = NS::TransferPtr(MTL::CreateSystemDefaultDevice());
	REQUIRE(available.get() != nullptr);
	RenderingContextDriverMetal context;
	REQUIRE(context.initialize() == OK);
	MTL3::RenderingDeviceDriverMetal driver(&context);
	REQUIRE(driver.initialize(0, 2) == OK);
	INFO("Native Metal device: ", available->name()->utf8String());
	RenderingDevice device;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::sampler_original_return(device);
	TestQualificationPhysicalAccess::detach(device);
}
#endif

TEST_CASE("[Qualification][T1] texture ticket acknowledgment preserves the neighboring original use") {
	RenderingDevice device;
	TestQualificationPhysical::Driver driver;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::texture_ticket_reverse_unlink(device);
	TestQualificationPhysicalAccess::detach(device);
}

TEST_CASE("[Qualification][T1] sampler ticket acknowledgment preserves the neighboring original use") {
	RenderingDevice device;
	TestQualificationPhysical::Driver driver;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::texture_ticket_reverse_unlink(device, true);
	TestQualificationPhysicalAccess::detach(device);
}

TEST_CASE("[Qualification][T1] live method children retain bridge cleanup credits") {
	TestQualificationPhysicalAccess::method_children_keep_bridge_cleanup_credits();
}

TEST_CASE("[Qualification][T1] a same-numbered but unregistered texture cell is not authority") {
	RenderingDevice device;
	TestQualificationPhysical::Driver driver;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::unregistered_texture_cell_cannot_pin(device);
	CHECK(driver.texture_frees == 1);
	TestQualificationPhysicalAccess::detach(device);
}

TEST_CASE("[Qualification][T1] parent release refuses an unsettled texture descendant") {
	RenderingDevice device;
	TestQualificationPhysical::Driver driver;
	TestQualificationPhysicalAccess::attach(device, driver);
	TestQualificationPhysicalAccess::texture_worker_descendant_does_not_livelock(device);
	CHECK(driver.texture_frees == 1);
	CHECK(driver.frees == 1);
	TestQualificationPhysicalAccess::detach(device);
}
