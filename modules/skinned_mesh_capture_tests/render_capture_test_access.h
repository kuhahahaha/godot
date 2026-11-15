/**************************************************************************/
/*  render_capture_test_access.h                                          */
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
#include "servers/rendering/renderer_rd/storage_rd/mesh_storage.h"
#include "servers/rendering/renderer_scene_cull.h"
#include "servers/rendering/rendering_device.h"
// Engine-private access: no public methods or script binding. Only the bounded
// capture implementation and explicit CPU fixtures may inspect original owners.
class RenderCaptureTestAccess {
	friend class SkinnedMeshCaptureTestRDPool;
	friend class SkinnedMeshCaptureTestNativeSource;
	friend class SkinnedMeshCaptureTestRDPoolTestAccess;
	friend class SkinnedMeshCaptureTestWriteCensusTestAccess;
	static const auto &draw_list(RenderingDevice *p_device) { return p_device->draw_list; }
	static const auto &compute_list(RenderingDevice *p_device) { return p_device->compute_list; }
	static const auto &raytracing_list(RenderingDevice *p_device) { return p_device->raytracing_list; }
	static auto &capture_test_allocation_registry(RenderingDevice *p_device) { return p_device->capture_test_allocation_registry; }
	static auto &capture_test_resource_registry(RenderingDevice *p_device) { return p_device->capture_test_resource_registry; }
	static const auto &capture_test_coverage_failed(RenderingDevice *p_device) { return p_device->capture_test_coverage_failed; }
	static const auto *texture(RenderingDevice *p_device, RID p_rid) { return p_device->texture_owner.get_or_null(p_rid); }
	static const auto *shader(RenderingDevice *p_device, RID p_rid) { return p_device->shader_owner.get_or_null(p_rid); }
	static const auto *compute_pipeline(RenderingDevice *p_device, RID p_rid) { return p_device->compute_pipeline_owner.get_or_null(p_rid); }
	static auto &uniform_set_owner(RenderingDevice *p_device) { return p_device->uniform_set_owner; }
	static auto &storage_buffer_owner(RenderingDevice *p_device) { return p_device->storage_buffer_owner; }
	using Buffer = RenderingDevice::Buffer;
	using UniformSet = RenderingDevice::UniformSet;
	static const auto *buffer(RenderingDevice *p_device, RID p_rid) { return p_device->_get_buffer_from_owner(p_rid); }
	static void record_writable_set(RenderingDevice *p_device, RID p_set, uint32_t p_operation) { p_device->_capture_test_record_writable_set(p_set, p_operation); }
	static void record_buffer_usage(RenderingDevice *p_device, uint64_t p_driver, RenderingDeviceGraph::ResourceUsage p_usage, uint32_t p_operation) { p_device->_capture_test_record_buffer_usage(p_driver, p_usage, p_operation); }
	static auto *scene_singleton() { return RendererSceneCull::singleton; }
	static const auto *instance(RendererSceneCull *p_scene, RID p_rid) { return p_scene->instance_owner.get_or_null(p_rid); }
	static const auto *mesh(RendererRD::MeshStorage *p_storage, RID p_rid) { return p_storage->mesh_owner.get_or_null(p_rid); }
	static const auto *mesh_instance(RendererRD::MeshStorage *p_storage, RID p_rid) { return p_storage->mesh_instance_owner.get_or_null(p_rid); }
	static const auto *skeleton(RendererRD::MeshStorage *p_storage, RID p_rid) { return p_storage->skeleton_owner.get_or_null(p_rid); }
};
