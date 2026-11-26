/**************************************************************************/
/*  rendering_device_driver_metal.cpp                                     */
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

/**************************************************************************/
/*                                                                        */
/* Portions of this code were derived from MoltenVK.                      */
/*                                                                        */
/* Copyright (c) 2015-2023 The Brenwill Workshop Ltd.                     */
/* (http://www.brenwill.com)                                              */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

#include "rendering_device_driver_metal.h"

#include "core/extension/gdextension_library_exit_guard.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/os/os.h"

#ifdef MACOS_ENABLED
#include <malloc/malloc.h>
#endif

thread_local LocalVector<RenderingDeviceDriverMetal::CompletionRecord *> *RenderingDeviceDriverMetal::active_completion_records = nullptr;
thread_local uint64_t RenderingDeviceDriverMetal::active_native_serial = 0;
thread_local uint32_t RenderingDeviceDriverMetal::active_native_serial_count = 0;
thread_local uint32_t *RenderingDeviceDriverMetal::active_native_issue_count = nullptr;
uint64_t RenderingDeviceDriverMetal::reserve_native_call_serials(uint32_t count) {
	if (!count) {
		return 0;
	}
	uint64_t current = next_native_call_serial.load();
	while (current) {
		if (uint64_t(count - 1) > UINT64_MAX - current) {
			return 0;
		}
		const uint64_t next = current + count;
		if (next_native_call_serial.compare_exchange_weak(current, next)) {
			return current;
		}
	}
	return 0;
}
RenderingDeviceDriver::FencePollResult RenderingDeviceDriverMetal::native_call_poll(uint64_t serial) {
	FencePollResult out;
	if (!serial) {
		return out;
	}
	if (native_call_blocking_allowed()) {
		completion_records_mutex.lock();
	} else if (!completion_records_mutex.try_lock()) {
		return out;
	}
	for (auto *record : completion_records) {
		if (record->serial == serial) {
			out.completed_serial = record->serial;
			if (record->completed.load()) {
				out.completed = !record->failed.load();
				out.native_error = record->failed.load();
				out.native_code = record->native_code.load();
				// The exact registered command's completion callback is an authoritative
				// acceptance resolution for Metal's void commit, unlike a quiet tail fence.
				if (out.completed) {
					out.resolved_issue = IssueFact::KnownAccepted;
				}
			}
			break;
		}
	}
	completion_records_mutex.unlock();
	return out;
}
void RenderingDeviceDriverMetal::publish_command_buffer_status(CompletionRecord *p_record, MTL::CommandBuffer *p_command_buffer) {
	if (!p_record || !p_command_buffer) {
		return;
	}
	const MTL::CommandBufferStatus status = p_command_buffer->status();
	if (status != MTL::CommandBufferStatusCompleted && status != MTL::CommandBufferStatusError) {
		return;
	}
	p_record->native_code.store(static_cast<int64_t>(status), std::memory_order_relaxed);
	p_record->failed.store(status != MTL::CommandBufferStatusCompleted, std::memory_order_relaxed);
	p_record->completed.store(true, std::memory_order_release);
}

void RenderingDeviceDriverMetal::release_completion_record(CompletionRecord *p_record) {
	if (p_record && p_record->live_refs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
		memdelete(p_record);
	}
}

RenderingDeviceDriver::FencePollResult RenderingDeviceDriverMetal::native_call_wait(uint64_t serial) {
	NS::SharedPtr<MTL::CommandBuffer> command;
	uint32_t active_records = 0;
	{
		MutexLock lock(completion_records_mutex);
		active_records = completion_records.size();
		for (auto *record : completion_records) {
			if (record->serial == serial) {
				command = record->command_buffer;
				break;
			}
		}
	}
	if (!command) {
#ifndef TESTS_ENABLED
		static std::atomic<uint32_t> missing_diag_count{ 0 };
		const uint32_t diag_n = missing_diag_count.fetch_add(1, std::memory_order_relaxed);
		if (diag_n < 8 || (diag_n % 256) == 0) {
			print_error(String("[BUG-119B] Metal native_call_wait missing completion record: serial=") + itos(int64_t(serial)) +
					" active_records=" + itos(int64_t(active_records)) + " occ=" + itos(int64_t(diag_n + 1)));
		}
#else
		(void)active_records;
#endif
		FencePollResult missing;
		missing.completed = false;
		missing.native_error = true;
		missing.completed_serial = 0;
		missing.native_code = ERR_DOES_NOT_EXIST;
		missing.resolved_issue = IssueFact::NotIssued;
		return missing;
	}
	command->waitUntilCompleted();
	FencePollResult out;
	{
		MutexLock lock(completion_records_mutex);
		for (auto *record : completion_records) {
			if (record->serial == serial) {
				publish_command_buffer_status(record, command.get());
				const bool done = record->completed.load(std::memory_order_acquire);
				out.completed_serial = serial;
				out.native_error = record->failed.load(std::memory_order_relaxed);
				out.completed = done && !out.native_error;
				out.native_code = record->native_code.load(std::memory_order_relaxed);
				if (out.completed) {
					out.resolved_issue = IssueFact::KnownAccepted;
				}
				return out;
			}
		}
	}
	const MTL::CommandBufferStatus status = command->status();
	out.completed_serial = serial;
	out.native_code = static_cast<int64_t>(status);
	out.native_error = (status != MTL::CommandBufferStatusCompleted);
	out.completed = (status == MTL::CommandBufferStatusCompleted);
	if (out.completed) {
		out.resolved_issue = IssueFact::KnownAccepted;
	}
	return out;
}
Error RenderingDeviceDriverMetal::native_call_recycle(uint64_t serial) {
	if (!serial) {
		return ERR_INVALID_PARAMETER;
	}
	if (!completion_records_mutex.try_lock()) {
		return ERR_BUSY;
	}
	for (uint32_t i = 0; i < completion_records.size(); ++i) {
		auto *record = completion_records[i];
		if (record->serial != serial) {
			continue;
		}
		if (!record->completed.load() || record->failed.load()) {
			completion_records_mutex.unlock();
			return ERR_BUSY;
		}
		// The original fence owns these pointers until its checked recycle. Calls
		// without a fence are retired by the original frame's successful observation.
		if (!record->fence_owned.load()) {
			record->retired.store(true);
			if (!record->observers) {
				completion_records.remove_at(i);
				release_completion_record(record);
			}
		}
		completion_records_mutex.unlock();
		return OK;
	}
	completion_records_mutex.unlock();
	return ERR_DOES_NOT_EXIST;
}

Error RenderingDeviceDriverMetal::native_call_recycle_wait(uint64_t serial) {
	if (!serial) {
		return ERR_INVALID_PARAMETER;
	}
	MutexLock lock(completion_records_mutex);
	for (uint32_t i = 0; i < completion_records.size(); ++i) {
		auto *record = completion_records[i];
		if (record->serial != serial) {
			continue;
		}
		if (!record->completed.load() || record->failed.load()) {
			return ERR_BUSY;
		}
		if (!record->fence_owned.load()) {
			record->retired.store(true);
			if (!record->observers) {
				completion_records.remove_at(i);
				release_completion_record(record);
			}
		}
		return OK;
	}
	return ERR_DOES_NOT_EXIST;
}

Error RenderingDeviceDriverMetal::native_call_retain(uint64_t serial) {
	if (!serial) {
		return ERR_INVALID_PARAMETER;
	}
	if (native_call_blocking_allowed()) {
		completion_records_mutex.lock();
	} else if (!completion_records_mutex.try_lock()) {
		return ERR_BUSY;
	}
	for (auto *record : completion_records) {
		if (record->serial == serial) {
			if (record->observers == UINT32_MAX) {
				completion_records_mutex.unlock();
				return ERR_PARAMETER_RANGE_ERROR;
			}
			++record->observers;
			completion_records_mutex.unlock();
			return OK;
		}
	}
	completion_records_mutex.unlock();
	return ERR_DOES_NOT_EXIST;
}
Error RenderingDeviceDriverMetal::native_call_release(uint64_t serial) {
	if (!serial) {
		return ERR_INVALID_PARAMETER;
	}
	if (native_call_blocking_allowed()) {
		completion_records_mutex.lock();
	} else if (!completion_records_mutex.try_lock()) {
		return ERR_BUSY;
	}
	for (uint32_t i = 0; i < completion_records.size(); ++i) {
		auto *record = completion_records[i];
		if (record->serial != serial) {
			continue;
		}
		if (!record->observers) {
			completion_records_mutex.unlock();
			return ERR_DOES_NOT_EXIST;
		}
		--record->observers;
		if (!record->observers && record->retired.load()) {
			completion_records.remove_at(i);
			release_completion_record(record);
		}
		completion_records_mutex.unlock();
		return OK;
	}
	completion_records_mutex.unlock();
	return ERR_DOES_NOT_EXIST;
}

RenderingDeviceDriverMetal::CompletionRecord *RenderingDeviceDriverMetal::register_completion_record(MTL::CommandBuffer *p_command_buffer, uint64_t p_serial, bool p_collection_locked, bool p_independent_source) {
	const bool batch_source = active_completion_records && !p_independent_source;
	const uint64_t serial = batch_source ? (active_native_serial_count ? (--active_native_serial_count, active_native_serial++) : 0) : reserve_native_call_serials(1);
	if (!serial || (!batch_source && admit_native_source(serial, p_independent_source) != OK)) {
		return nullptr;
	}

	uint64_t registry = 0, epoch = 0;
	session_loss_identity(registry, epoch);
	p_command_buffer->addCompletedHandler([registry, epoch](MTL::CommandBuffer *completed) {
		NS::Error *error = completed->error();
		if (error && error->code() == MTL::CommandBufferErrorDeviceRemoved) {
			notify_session_device_lost(registry, epoch);
		}
	});
	CompletionRecord *record = memnew(CompletionRecord);
	record->serial = serial;
	record->command_buffer = NS::RetainPtr(p_command_buffer);
	p_command_buffer->addCompletedHandler([record](MTL::CommandBuffer *p_completed) {
		publish_command_buffer_status(record, p_completed);
		release_completion_record(record);
	});
	if (!p_collection_locked) {
		completion_records_mutex.lock();
	}
	completion_records.push_back(record);
	if (batch_source) {
		active_completion_records->push_back(record);
	}
	if (!p_collection_locked) {
		completion_records_mutex.unlock();
	}
	return record;
}

bool RenderingDeviceDriverMetal::completion_records_complete(const LocalVector<CompletionRecord *> &p_records) const {
	MutexLock lock(const_cast<Mutex &>(completion_records_mutex));
	for (CompletionRecord *record : p_records) {
		if (!record->completed.load() || record->failed.load()) {
			return false;
		}
	}
	return true;
}

bool RenderingDeviceDriverMetal::completion_records_failed(const LocalVector<CompletionRecord *> &p_records, int64_t &r_native_code) const {
	MutexLock lock(const_cast<Mutex &>(completion_records_mutex));
	for (CompletionRecord *record : p_records) {
		if (record->completed.load() && record->failed.load()) {
			r_native_code = record->native_code.load();
			return true;
		}
	}
	return false;
}

void RenderingDeviceDriverMetal::retire_completion_records(const LocalVector<CompletionRecord *> &p_records) {
	MutexLock lock(completion_records_mutex);
	for (CompletionRecord *record : p_records) {
		if (!record->completed.load() || record->retired.exchange(true)) {
			continue;
		}
		record->fence_owned.store(false);
		if (record->observers) {
			continue;
		}
		for (uint32_t i = 0; i < completion_records.size(); i++) {
			if (completion_records[i] == record) {
				completion_records.remove_at(i);
				break;
			}
		}
		release_completion_record(record);
	}
}

void RenderingDeviceDriverMetal::drain_completion_records() {
	LocalVector<CompletionRecord *> records;
	{
		MutexLock lock(completion_records_mutex);
		for (auto *record : completion_records) {
			GDExtensionDeviceExit::require_close_success(record->observers ? ERR_BUSY : OK, GDExtensionDeviceExit::CloseRoute::LAST_REF);
		}
		records = std::move(completion_records);
	}
	// Wait outside the collection lock and before the derived driver releases
	// command-buffer wrappers and their scratch resources. live_refs keeps each
	// record alive until both this drain and Metal's async completion handler finish.
	for (CompletionRecord *record : records) {
		record->command_buffer->waitUntilCompleted();
		publish_command_buffer_status(record, record->command_buffer.get());
		release_completion_record(record);
	}
}

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/error/error_macros.h"
#include "core/io/marshalls.h"
#include "core/os/os.h"
#include "core/string/ustring.h"
#include "core/templates/hash_map.h"
#include "drivers/apple/foundation_helpers.h"
#include "drivers/metal/pixel_formats.h"
#include "drivers/metal/rendering_context_driver_metal.h"
#include "drivers/metal/rendering_shader_container_metal.h"

#include <Metal/Metal.hpp>
#include <objc/message.h>
#include <objc/objc.h>
#include <objc/runtime.h>
#include <os/log.h>
#include <os/signpost.h>

#include <algorithm>

#ifndef MTLGPUAddress
typedef uint64_t MTLGPUAddress;
#endif

static bool class_conforms_to_protocol_recursive(Class p_class, Protocol *p_protocol) {
	Class current = p_class;
	while (current != nil) {
		if (class_conformsToProtocol(current, p_protocol)) {
			return true;
		}
		current = class_getSuperclass(current);
	}
	return false;
}

#pragma mark - Logging

extern os_log_t LOG_DRIVER;
// Used for dynamic tracing.
extern os_log_t LOG_INTERVALS;

/*****************/
/**** GENERIC ****/
/*****************/

// RDD::CompareOperator == VkCompareOp.
static_assert(ENUM_MEMBERS_EQUAL(RDD::COMPARE_OP_NEVER, MTL::CompareFunctionNever));
static_assert(ENUM_MEMBERS_EQUAL(RDD::COMPARE_OP_LESS, MTL::CompareFunctionLess));
static_assert(ENUM_MEMBERS_EQUAL(RDD::COMPARE_OP_EQUAL, MTL::CompareFunctionEqual));
static_assert(ENUM_MEMBERS_EQUAL(RDD::COMPARE_OP_LESS_OR_EQUAL, MTL::CompareFunctionLessEqual));
static_assert(ENUM_MEMBERS_EQUAL(RDD::COMPARE_OP_GREATER, MTL::CompareFunctionGreater));
static_assert(ENUM_MEMBERS_EQUAL(RDD::COMPARE_OP_NOT_EQUAL, MTL::CompareFunctionNotEqual));
static_assert(ENUM_MEMBERS_EQUAL(RDD::COMPARE_OP_GREATER_OR_EQUAL, MTL::CompareFunctionGreaterEqual));
static_assert(ENUM_MEMBERS_EQUAL(RDD::COMPARE_OP_ALWAYS, MTL::CompareFunctionAlways));

/*****************/
/**** BUFFERS ****/
/*****************/

RDD::BufferID RenderingDeviceDriverMetal::buffer_create(uint64_t p_size, BitField<BufferUsageBits> p_usage, MemoryAllocationType p_allocation_type, uint64_t p_frames_drawn) {
	const uint64_t original_size = p_size;
	if (p_usage.has_flag(BUFFER_USAGE_DYNAMIC_PERSISTENT_BIT)) {
		p_size = round_up_to_alignment(p_size, 16u) * _frame_count;
	}

	MTL::ResourceOptions options = 0;
	switch (p_allocation_type) {
		case MEMORY_ALLOCATION_TYPE_CPU:
			options = base_hazard_tracking | MTL::ResourceStorageModeShared;
			break;
		case MEMORY_ALLOCATION_TYPE_GPU:
			if (p_usage.has_flag(BUFFER_USAGE_DYNAMIC_PERSISTENT_BIT)) {
				options = MTL::ResourceHazardTrackingModeUntracked | MTL::ResourceStorageModeShared | MTL::ResourceCPUCacheModeWriteCombined;
			} else {
				options = base_hazard_tracking | MTL::ResourceStorageModePrivate;
			}
			break;
		case MEMORY_ALLOCATION_TYPE_GPU_MAPPABLE:
			options = base_hazard_tracking | MTL::ResourceStorageModeShared;
			break;
	}

	MetalBuffer buffer = allocator->new_buffer(p_size, options);
	ERR_FAIL_NULL_V_MSG(buffer.buffer.get(), BufferID(), "Can't create buffer of size: " + itos(p_size));

	BufferInfo *buf_info;
	if (p_usage.has_flag(BUFFER_USAGE_DYNAMIC_PERSISTENT_BIT)) {
		static_assert(sizeof(MetalBufferDynamicInfo) <= sizeof(VersatileResource));
		MetalBufferDynamicInfo *dyn_buffer = VersatileResource::allocate<MetalBufferDynamicInfo>(resources_allocator);
		buf_info = dyn_buffer;
#ifdef DEBUG_ENABLED
		dyn_buffer->last_frame_mapped = p_frames_drawn - 1ul;
#endif
		dyn_buffer->set_frame_index(0u);
		dyn_buffer->size_bytes = round_up_to_alignment(original_size, 16u);
	} else {
		buf_info = VersatileResource::allocate<BufferInfo>(resources_allocator);
	}
	*static_cast<MetalBuffer *>(buf_info) = buffer;
	buf_info->logical_size = original_size;

	return BufferID(buf_info);
}

bool RenderingDeviceDriverMetal::buffer_set_texel_format(BufferID p_buffer, DataFormat p_format) {
	// Nothing to do.
	return true;
}

void RenderingDeviceDriverMetal::buffer_free(BufferID p_buffer) {
	NS::SharedPtr<NS::AutoreleasePool> pool = NS::TransferPtr(NS::AutoreleasePool::alloc()->init());
	BufferInfo *buf_info = (BufferInfo *)p_buffer.id;
	if (!buf_info) {
		return;
	}

	allocator->free_buffer(*buf_info);

	if (buf_info->is_dynamic()) {
		VersatileResource::free(resources_allocator, (MetalBufferDynamicInfo *)buf_info);
	} else {
		VersatileResource::free(resources_allocator, buf_info);
	}
}

uint64_t RenderingDeviceDriverMetal::buffer_get_allocation_size(BufferID p_buffer) {
	const BufferInfo *buf_info = (const BufferInfo *)p_buffer.id;
	return buf_info->buffer.get()->allocatedSize();
}

uint8_t *RenderingDeviceDriverMetal::buffer_map(BufferID p_buffer) {
	const BufferInfo *buf_info = (const BufferInfo *)p_buffer.id;
	ERR_FAIL_COND_V_MSG(buf_info->buffer.get()->storageMode() != MTL::StorageModeShared, nullptr, "Unable to map private buffers");
	return (uint8_t *)buf_info->buffer.get()->contents();
}

void RenderingDeviceDriverMetal::buffer_unmap(BufferID p_buffer) {
	// Nothing to do.
}

uint8_t *RenderingDeviceDriverMetal::buffer_persistent_map_advance(BufferID p_buffer, uint64_t p_frames_drawn) {
	MetalBufferDynamicInfo *buf_info = (MetalBufferDynamicInfo *)p_buffer.id;
	ERR_FAIL_COND_V_MSG(!buf_info->is_dynamic(), nullptr, "Buffer must have BUFFER_USAGE_DYNAMIC_PERSISTENT_BIT. Use buffer_map() instead.");
#ifdef DEBUG_ENABLED
	ERR_FAIL_COND_V_MSG(buf_info->last_frame_mapped == p_frames_drawn, nullptr, "Buffers with BUFFER_USAGE_DYNAMIC_PERSISTENT_BIT must only be mapped once per frame. Otherwise there could be race conditions with the GPU. Amalgamate all data uploading into one map(), use an extra buffer or remove the bit.");
	buf_info->last_frame_mapped = p_frames_drawn;
#endif
	return (uint8_t *)buf_info->buffer.get()->contents() + buf_info->next_frame_index(_frame_count) * buf_info->size_bytes;
}

uint64_t RenderingDeviceDriverMetal::buffer_get_dynamic_offsets(Span<BufferID> p_buffers) {
	uint64_t mask = 0u;
	uint64_t shift = 0u;

	for (const BufferID &buf : p_buffers) {
		const BufferInfo *buf_info = (const BufferInfo *)buf.id;
		if (!buf_info->is_dynamic()) {
			continue;
		}
		mask |= buf_info->frame_index() << shift;
		// We can encode the frame index in 2 bits since frame_count won't be > 4.
		shift += 2UL;
	}

	return mask;
}

uint64_t RenderingDeviceDriverMetal::buffer_get_device_address(BufferID p_buffer) {
	if (__builtin_available(iOS 16.0, macOS 13.0, *)) {
		const BufferInfo *buf_info = (const BufferInfo *)p_buffer.id;
		return buf_info->buffer.get()->gpuAddress();
	} else {
#if DEV_ENABLED
		WARN_PRINT_ONCE("buffer_get_device_address is not supported on this OS version.");
#endif
		return 0;
	}
}

#pragma mark - Texture

#pragma mark - Format Conversions

static const MTL::TextureType TEXTURE_TYPE[RDD::TEXTURE_TYPE_MAX] = {
	MTL::TextureType1D,
	MTL::TextureType2D,
	MTL::TextureType3D,
	MTL::TextureTypeCube,
	MTL::TextureType1DArray,
	MTL::TextureType2DArray,
	MTL::TextureTypeCubeArray,
};

bool RenderingDeviceDriverMetal::is_valid_linear(const TextureFormat &p_format) const {
	MTLFormatType ft = pixel_formats->getFormatType(p_format.format);

	return p_format.texture_type == TEXTURE_TYPE_2D // Linear textures must be 2D textures.
			&& ft != MTLFormatType::DepthStencil && ft != MTLFormatType::Compressed // Linear textures must not be depth/stencil or compressed formats.)
			&& p_format.mipmaps == 1 // Linear textures must have 1 mipmap level.
			&& p_format.array_layers == 1 // Linear textures must have 1 array layer.
			&& p_format.samples == TEXTURE_SAMPLES_1; // Linear textures must have 1 sample.
}

NS::SharedPtr<MTL::TextureDescriptor> RenderingDeviceDriverMetal::_depth_allocation_descriptor(uint32_t p_width, uint32_t p_height) const {
	NS::SharedPtr<MTL::TextureDescriptor> desc = NS::TransferPtr(MTL::TextureDescriptor::alloc()->init());
	desc->setTextureType(MTL::TextureType2D);
	desc->setPixelFormat((MTL::PixelFormat)pixel_formats->getMTLPixelFormat(DATA_FORMAT_D32_SFLOAT));
	desc->setWidth(p_width);
	desc->setHeight(p_height);
	desc->setDepth(1);
	desc->setMipmapLevelCount(1);
	desc->setResourceOptions(base_hazard_tracking | MTL::ResourceCPUCacheModeDefaultCache | MTL::ResourceStorageModePrivate);
	desc->setUsage(MTL::TextureUsageShaderRead | MTL::TextureUsageRenderTarget);
	return desc;
}

Error RenderingDeviceDriverMetal::depth_allocation_quote(uint32_t p_width, uint32_t p_height, DepthAllocationQuote &r_quote) {
	r_quote = {};
	// Match the original RD texture-create limit before descriptor construction
	// or a native size query; oversized descriptors are not native probes.
	const uint64_t max_size = limit_get(LIMIT_MAX_TEXTURE_SIZE_2D);
	if (!p_width || !p_height || p_width > max_size || p_height > max_size) {
		return ERR_INVALID_PARAMETER;
	}
	if (!allocator || !pixel_formats ||
			!flags::any(pixel_formats->getCapabilities((MTL::PixelFormat)pixel_formats->getMTLPixelFormat(DATA_FORMAT_D32_SFLOAT)), kMTLFmtCapsDSAtt)) {
		return ERR_UNAVAILABLE;
	}
	const auto desc = _depth_allocation_descriptor(p_width, p_height);
	MetalAllocator::TextureAllocationQuote quote;
	if (!allocator->texture_allocation_quote(desc.get(), quote)) {
		return ERR_UNAVAILABLE;
	}
	r_quote.policy = quote.dedicated ? DepthAllocationQuote::Policy::MetalHeapDedicated : DepthAllocationQuote::Policy::MetalHeapPool;
	r_quote.native_requirement = quote.native_requirement;
	r_quote.owner_reservation = quote.owner_reservation;
	r_quote.alignment = quote.alignment;
	return OK;
}

RDD::TextureID RenderingDeviceDriverMetal::texture_create(const TextureFormat &p_format, const TextureView &p_view) {
	NS::SharedPtr<MTL::TextureDescriptor> desc = NS::TransferPtr(MTL::TextureDescriptor::alloc()->init());
	desc->setTextureType(TEXTURE_TYPE[p_format.texture_type]);

	PixelFormats &formats = *pixel_formats;
	desc->setPixelFormat((MTL::PixelFormat)formats.getMTLPixelFormat(p_format.format));
	MTLFmtCaps format_caps = formats.getCapabilities(desc->pixelFormat());

	desc->setWidth(p_format.width);
	desc->setHeight(p_format.height);
	desc->setDepth(p_format.depth);
	desc->setMipmapLevelCount(p_format.mipmaps);

	if (p_format.texture_type == TEXTURE_TYPE_1D_ARRAY ||
			p_format.texture_type == TEXTURE_TYPE_2D_ARRAY) {
		desc->setArrayLength(p_format.array_layers);
	} else if (p_format.texture_type == TEXTURE_TYPE_CUBE_ARRAY) {
		desc->setArrayLength(p_format.array_layers / 6);
	}

	// TODO(sgc): Evaluate lossy texture support (perhaps as a project option?)
	//  https://developer.apple.com/videos/play/tech-talks/10876?time=459
	// desc->setCompressionType(MTL::TextureCompressionTypeLossy);

	if (p_format.samples > TEXTURE_SAMPLES_1) {
		SampleCount supported = (*device_properties).find_nearest_supported_sample_count(p_format.samples);

		if (supported > SampleCount1) {
			bool ok = p_format.texture_type == TEXTURE_TYPE_2D || p_format.texture_type == TEXTURE_TYPE_2D_ARRAY;
			if (ok) {
				switch (p_format.texture_type) {
					case TEXTURE_TYPE_2D:
						desc->setTextureType(MTL::TextureType2DMultisample);
						break;
					case TEXTURE_TYPE_2D_ARRAY:
						desc->setTextureType(MTL::TextureType2DMultisampleArray);
						break;
					default:
						break;
				}
				desc->setSampleCount((NS::UInteger)supported);
				if (p_format.mipmaps > 1) {
					// For a buffer-backed or multi-sample texture, the value must be 1.
					WARN_PRINT("mipmaps == 1 for multi-sample textures");
					desc->setMipmapLevelCount(1);
				}
			} else {
				WARN_PRINT("Unsupported multi-sample texture type; disabling multi-sample");
			}
		}
	}

	static const MTL::TextureSwizzle COMPONENT_SWIZZLE[TEXTURE_SWIZZLE_MAX] = {
		static_cast<MTL::TextureSwizzle>(255), // IDENTITY
		MTL::TextureSwizzleZero,
		MTL::TextureSwizzleOne,
		MTL::TextureSwizzleRed,
		MTL::TextureSwizzleGreen,
		MTL::TextureSwizzleBlue,
		MTL::TextureSwizzleAlpha,
	};

	MTL::TextureSwizzleChannels swizzle = MTL::TextureSwizzleChannels::Make(
			p_view.swizzle_r != TEXTURE_SWIZZLE_IDENTITY ? COMPONENT_SWIZZLE[p_view.swizzle_r] : MTL::TextureSwizzleRed,
			p_view.swizzle_g != TEXTURE_SWIZZLE_IDENTITY ? COMPONENT_SWIZZLE[p_view.swizzle_g] : MTL::TextureSwizzleGreen,
			p_view.swizzle_b != TEXTURE_SWIZZLE_IDENTITY ? COMPONENT_SWIZZLE[p_view.swizzle_b] : MTL::TextureSwizzleBlue,
			p_view.swizzle_a != TEXTURE_SWIZZLE_IDENTITY ? COMPONENT_SWIZZLE[p_view.swizzle_a] : MTL::TextureSwizzleAlpha);

	// Represents a swizzle operation that is a no-op.
	static MTL::TextureSwizzleChannels IDENTITY_SWIZZLE = MTL::TextureSwizzleChannels::Default();

	bool no_swizzle = memcmp(&IDENTITY_SWIZZLE, &swizzle, sizeof(MTL::TextureSwizzleChannels)) == 0;
	if (!no_swizzle) {
		desc->setSwizzle(swizzle);
	}

	// Usage.

	MTL::ResourceOptions options = 0;
	bool is_linear = false;
#if defined(VISIONOS_ENABLED)
	const bool supports_memoryless = true;
#else
	GODOT_CLANG_WARNING_PUSH_AND_IGNORE("-Wdeprecated-declarations")
	const bool supports_memoryless = (*device_properties).features.highestFamily >= MTL::GPUFamilyApple2 && (*device_properties).features.highestFamily < MTL::GPUFamilyMac1;
	GODOT_CLANG_WARNING_POP
#endif
	if (supports_memoryless && p_format.usage_bits & TEXTURE_USAGE_TRANSIENT_BIT) {
		options = base_hazard_tracking | MTL::ResourceStorageModeMemoryless;
		desc->setStorageMode(MTL::StorageModeMemoryless);
	} else {
		options = base_hazard_tracking | MTL::ResourceCPUCacheModeDefaultCache;
		if (p_format.usage_bits & TEXTURE_USAGE_CPU_READ_BIT) {
			options |= MTL::ResourceStorageModeShared;
			// The user has indicated they want to read from the texture on the CPU,
			// so we'll see if we can use a linear format.
			// A linear format is a texture that is backed by a buffer,
			// which allows for CPU access to the texture data via a pointer.
			is_linear = is_valid_linear(p_format);
		} else {
			options |= MTL::ResourceStorageModePrivate;
		}
	}
	desc->setResourceOptions(options);

	MTL::TextureUsage usage = desc->usage();
	if (p_format.usage_bits & TEXTURE_USAGE_SAMPLING_BIT) {
		usage |= MTL::TextureUsageShaderRead;
	}

	if (p_format.usage_bits & TEXTURE_USAGE_STORAGE_BIT) {
		usage |= MTL::TextureUsageShaderWrite;
	}

	bool can_be_attachment = flags::any(format_caps, (kMTLFmtCapsColorAtt | kMTLFmtCapsDSAtt));

	if (flags::any(p_format.usage_bits, TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) &&
			can_be_attachment) {
		usage |= MTL::TextureUsageRenderTarget;
	}

	if (p_format.usage_bits & TEXTURE_USAGE_INPUT_ATTACHMENT_BIT) {
		usage |= MTL::TextureUsageShaderRead;
	}

	if (p_format.usage_bits & TEXTURE_USAGE_STORAGE_ATOMIC_BIT) {
		ERR_FAIL_COND_V_MSG((format_caps & kMTLFmtCapsAtomic) == 0, RDD::TextureID(), "Atomic operations on this texture format are not supported.");
		ERR_FAIL_COND_V_MSG(!device_properties->features.supports_native_image_atomics, RDD::TextureID(), "Atomic operations on textures are not supported on this OS version. Check SUPPORTS_IMAGE_ATOMIC_32_BIT.");
		// If supports_native_image_atomics is true, this condition should always succeed, as it is set the same.
		if (__builtin_available(macOS 14.0, iOS 17.0, tvOS 17.0, *)) {
			usage |= MTL::TextureUsageShaderAtomic;
		}
	}

	if (p_format.usage_bits & TEXTURE_USAGE_VRS_ATTACHMENT_BIT) {
		ERR_FAIL_V_MSG(RDD::TextureID(), "unsupported: TEXTURE_USAGE_VRS_ATTACHMENT_BIT");
	}

	if (flags::any(p_format.usage_bits, TEXTURE_USAGE_CAN_UPDATE_BIT | TEXTURE_USAGE_CAN_COPY_TO_BIT) &&
			can_be_attachment && no_swizzle) {
		// Per MoltenVK, can be cleared as a render attachment.
		usage |= MTL::TextureUsageRenderTarget;
	}
	if (p_format.usage_bits & TEXTURE_USAGE_CAN_COPY_FROM_BIT) {
		// Covered by blits.
	}

	// Create texture views with a different component layout.
	if (!p_format.shareable_formats.is_empty()) {
		usage |= MTL::TextureUsagePixelFormatView;
	}

	desc->setUsage(usage);

	// Quote and actual creation use the same original standalone D32 recipe.
	// All other descriptors retain the general construction above.
	if (p_format.texture_type == TEXTURE_TYPE_2D && p_format.format == DATA_FORMAT_D32_SFLOAT &&
			p_format.depth == 1 && p_format.array_layers == 1 && p_format.mipmaps == 1 && p_format.samples == TEXTURE_SAMPLES_1 &&
			p_format.usage_bits == (TEXTURE_USAGE_SAMPLING_BIT | TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) && p_format.shareable_formats.is_empty() && no_swizzle && can_be_attachment) {
		desc = _depth_allocation_descriptor(p_format.width, p_format.height);
	}

	// Allocate memory.

	TextureInfo *tex_info = VersatileResource::allocate<TextureInfo>(resources_allocator);
	if (is_linear) {
		// Linear textures are restricted to 2D textures, a single mipmap level and a single array layer.
		MTL::PixelFormat pixel_format = desc->pixelFormat();
		size_t row_alignment = get_texel_buffer_alignment_for_format(p_format.format);
		size_t bytes_per_row = formats.getBytesPerRow(pixel_format, p_format.width);
		bytes_per_row = round_up_to_alignment(bytes_per_row, row_alignment);
		size_t bytes_per_layer = formats.getBytesPerLayer(pixel_format, bytes_per_row, p_format.height);
		size_t byte_count = bytes_per_layer * p_format.depth * p_format.array_layers;

		tex_info->linear_backing = allocator->new_buffer(byte_count, options);
		if (tex_info->linear_backing.buffer) {
			tex_info->texture = NS::TransferPtr(tex_info->linear_backing.buffer->newTexture(desc.get(), 0, bytes_per_row));
		}
	} else {
		*static_cast<MetalTexture *>(tex_info) = allocator->new_texture(desc.get());
	}
	if (!tex_info->texture) {
		allocator->free_buffer(tex_info->linear_backing);
		VersatileResource::free(resources_allocator, tex_info);
		ERR_FAIL_V_MSG(TextureID(), "Unable to create texture.");
	}

	return TextureID(tex_info);
}

RDD::TextureID RenderingDeviceDriverMetal::texture_create_from_extension(uint64_t p_native_texture, TextureType p_type, DataFormat p_format, uint32_t p_array_layers, bool p_depth_stencil, uint32_t p_mipmaps) {
	MTL::Texture *res = reinterpret_cast<MTL::Texture *>(p_native_texture);

	TextureInfo *tex_info = VersatileResource::allocate<TextureInfo>(resources_allocator);

	id native_obj = (id)(void *)p_native_texture;
	if (class_conforms_to_protocol_recursive(object_getClass(native_obj), objc_getProtocol("MTLRasterizationRateMap"))) {
		// A rate map is not a texture. Do not create a view, track residency, or set a label.
		tex_info->rasterization_rate_map = true;
		tex_info->texture = NS::RetainPtr(reinterpret_cast<MTL::Texture *>(p_native_texture));
		return TextureID(tex_info);
	}

	tex_info->imported = true;

	// If the requested format is different, we need to create a view.
	MTL::PixelFormat format = (MTL::PixelFormat)pixel_formats->getMTLPixelFormat(p_format);
	if (res->pixelFormat() != format) {
		MTL::TextureSwizzleChannels swizzle = MTL::TextureSwizzleChannels::Default();
		MTL::Texture *view = res->newTextureView(format, res->textureType(), NS::Range::Make(0, res->mipmapLevelCount()), NS::Range::Make(0, p_array_layers), swizzle);
		if (view == nullptr) {
			VersatileResource::free(resources_allocator, tex_info);
			ERR_FAIL_V_MSG(TextureID(), "Unable to create texture view.");
		}
		tex_info->texture = NS::TransferPtr(view);
	} else {
		tex_info->texture = NS::RetainPtr(res);
	}

	{
		MutexLock lock(imported_textures_mutex);
		imported_textures.push_back(tex_info->texture.get());
	}

	if (main_residency_set) {
		main_residency_set->addAllocation(tex_info->texture.get());
		main_residency_set->commit();
	}

	return TextureID(tex_info);
}

RDD::TextureID RenderingDeviceDriverMetal::texture_create_shared(TextureID p_original_texture, const TextureView &p_view) {
	TextureInfo *src_info = (TextureInfo *)p_original_texture.id;
	ERR_FAIL_COND_V_MSG(src_info->rasterization_rate_map, TextureID(), "Cannot create a shared texture from a rasterization rate map.");

	MTL::Texture *src_texture = src_info->texture.get();

	NS::UInteger slices = src_texture->arrayLength();
	if (src_texture->textureType() == MTL::TextureTypeCube) {
		// Metal expects Cube textures to have a slice count of 6.
		slices = 6;
	} else if (src_texture->textureType() == MTL::TextureTypeCubeArray) {
		// Metal expects Cube Array textures to have 6 slices per layer.
		slices *= 6;
	}

#if DEV_ENABLED
	if (src_texture->sampleCount() > 1) {
		// TODO(sgc): is it ok to create a shared texture from a multi-sample texture?
		WARN_PRINT("Is it safe to create a shared texture from multi-sample texture?");
	}
#endif

	MTL::PixelFormat format = (MTL::PixelFormat)pixel_formats->getMTLPixelFormat(p_view.format);

	static const MTL::TextureSwizzle component_swizzle[TEXTURE_SWIZZLE_MAX] = {
		static_cast<MTL::TextureSwizzle>(255), // IDENTITY
		MTL::TextureSwizzleZero,
		MTL::TextureSwizzleOne,
		MTL::TextureSwizzleRed,
		MTL::TextureSwizzleGreen,
		MTL::TextureSwizzleBlue,
		MTL::TextureSwizzleAlpha,
	};

#define SWIZZLE(C, CHAN) (p_view.swizzle_##C != TEXTURE_SWIZZLE_IDENTITY ? component_swizzle[p_view.swizzle_##C] : MTL::TextureSwizzle##CHAN)
	MTL::TextureSwizzleChannels swizzle = MTL::TextureSwizzleChannels::Make(SWIZZLE(r, Red), SWIZZLE(g, Green), SWIZZLE(b, Blue), SWIZZLE(a, Alpha));
#undef SWIZZLE
	MTL::Texture *obj = src_texture->newTextureView(format, src_texture->textureType(), NS::Range::Make(0, src_texture->mipmapLevelCount()), NS::Range::Make(0, slices), swizzle);
	ERR_FAIL_NULL_V_MSG(obj, TextureID(), "Unable to create shared texture");

	TextureInfo *tex_info = VersatileResource::allocate<TextureInfo>(resources_allocator);
	tex_info->texture = NS::TransferPtr(obj);
	return TextureID(tex_info);
}

RDD::TextureID RenderingDeviceDriverMetal::texture_create_shared_from_slice(TextureID p_original_texture, const TextureView &p_view, TextureSliceType p_slice_type, uint32_t p_layer, uint32_t p_layers, uint32_t p_mipmap, uint32_t p_mipmaps) {
	TextureInfo *src_info = (TextureInfo *)p_original_texture.id;
	ERR_FAIL_COND_V_MSG(src_info->rasterization_rate_map, TextureID(), "Cannot create a shared texture from a rasterization rate map.");

	MTL::Texture *src_texture = src_info->texture.get();

	static const MTL::TextureType VIEW_TYPES[] = {
		MTL::TextureType1D, // MTLTextureType1D
		MTL::TextureType1D, // MTLTextureType1DArray
		MTL::TextureType2D, // MTLTextureType2D
		MTL::TextureType2D, // MTLTextureType2DArray
		MTL::TextureType2D, // MTLTextureType2DMultisample
		MTL::TextureType2D, // MTLTextureTypeCube
		MTL::TextureType2D, // MTLTextureTypeCubeArray
		MTL::TextureType2D, // MTLTextureType3D
		MTL::TextureType2D, // MTLTextureType2DMultisampleArray
	};

	MTL::TextureType textureType = VIEW_TYPES[src_texture->textureType()];
	switch (p_slice_type) {
		case TEXTURE_SLICE_2D: {
			textureType = MTL::TextureType2D;
		} break;
		case TEXTURE_SLICE_3D: {
			textureType = MTL::TextureType3D;
		} break;
		case TEXTURE_SLICE_CUBEMAP: {
			textureType = MTL::TextureTypeCube;
		} break;
		case TEXTURE_SLICE_2D_ARRAY: {
			textureType = MTL::TextureType2DArray;
		} break;
		case TEXTURE_SLICE_MAX: {
			ERR_FAIL_V_MSG(TextureID(), "Invalid texture slice type");
		} break;
	}

	MTL::PixelFormat format = (MTL::PixelFormat)pixel_formats->getMTLPixelFormat(p_view.format);

	static const MTL::TextureSwizzle component_swizzle[TEXTURE_SWIZZLE_MAX] = {
		static_cast<MTL::TextureSwizzle>(255), // IDENTITY
		MTL::TextureSwizzleZero,
		MTL::TextureSwizzleOne,
		MTL::TextureSwizzleRed,
		MTL::TextureSwizzleGreen,
		MTL::TextureSwizzleBlue,
		MTL::TextureSwizzleAlpha,
	};

#define SWIZZLE(C, CHAN) (p_view.swizzle_##C != TEXTURE_SWIZZLE_IDENTITY ? component_swizzle[p_view.swizzle_##C] : MTL::TextureSwizzle##CHAN)
	MTL::TextureSwizzleChannels swizzle = MTL::TextureSwizzleChannels::Make(SWIZZLE(r, Red), SWIZZLE(g, Green), SWIZZLE(b, Blue), SWIZZLE(a, Alpha));
#undef SWIZZLE
	MTL::Texture *obj = src_texture->newTextureView(format, textureType, NS::Range::Make(p_mipmap, p_mipmaps), NS::Range::Make(p_layer, p_layers), swizzle);
	ERR_FAIL_NULL_V_MSG(obj, TextureID(), "Unable to create shared texture");

	TextureInfo *tex_info = VersatileResource::allocate<TextureInfo>(resources_allocator);
	tex_info->texture = NS::TransferPtr(obj);
	return TextureID(tex_info);
}

void RenderingDeviceDriverMetal::texture_free(TextureID p_texture) {
	NS::SharedPtr<NS::AutoreleasePool> pool = NS::TransferPtr(NS::AutoreleasePool::alloc()->init());
	TextureInfo *tex_info = (TextureInfo *)p_texture.id;
	if (tex_info->imported) {
		MutexLock lock(imported_textures_mutex);
		imported_textures.erase(tex_info->texture.get());

		if (main_residency_set) {
			main_residency_set->removeAllocation(tex_info->texture.get());
			main_residency_set->commit();
		}
	}
	allocator->free_texture(*tex_info);
	allocator->free_buffer(tex_info->linear_backing);
	VersatileResource::free(resources_allocator, tex_info);
}

uint64_t RenderingDeviceDriverMetal::texture_get_allocation_size(TextureID p_texture) {
	TextureInfo *tex_info = (TextureInfo *)p_texture.id;
	if (tex_info->rasterization_rate_map) {
		return 0;
	}
	return tex_info->texture.get()->allocatedSize();
}

void RenderingDeviceDriverMetal::texture_get_copyable_layout(TextureID p_texture, const TextureSubresource &p_subresource, TextureCopyableLayout *r_layout) {
	TextureInfo *tex_info = (TextureInfo *)p_texture.id;
	ERR_FAIL_COND_MSG(tex_info->rasterization_rate_map, "Cannot get copyable layout for rasterization rate map.");

	MTL::Texture *obj = tex_info->texture.get();

	PixelFormats &pf = *pixel_formats;
	DataFormat format = pf.getDataFormat(obj->pixelFormat());

	uint32_t w = MAX(1u, obj->width() >> p_subresource.mipmap);
	uint32_t h = MAX(1u, obj->height() >> p_subresource.mipmap);
	uint32_t d = MAX(1u, obj->depth() >> p_subresource.mipmap);

	uint32_t bw = 0, bh = 0;
	get_compressed_image_format_block_dimensions(format, bw, bh);

	uint32_t sbw = 0, sbh = 0;
	*r_layout = {};
	r_layout->size = get_image_format_required_size(format, w, h, d, 1, &sbw, &sbh);
	r_layout->row_pitch = r_layout->size / ((sbh / bh) * d);
}

Vector<uint8_t> RenderingDeviceDriverMetal::texture_get_data(TextureID p_texture, uint32_t p_layer) {
	TextureInfo *tex_info = (TextureInfo *)p_texture.id;
	ERR_FAIL_COND_V_MSG(tex_info->rasterization_rate_map, Vector<uint8_t>(), "Cannot get data for a rasterization rate map.");

	MTL::Texture *obj = tex_info->texture.get();
	ERR_FAIL_COND_V_MSG(obj->storageMode() != MTL::StorageModeShared, Vector<uint8_t>(), "Texture must be created with TEXTURE_USAGE_CPU_READ_BIT set.");

	MTL::Buffer *buf = obj->buffer();
	if (buf) {
		ERR_FAIL_COND_V_MSG(p_layer > 0, Vector<uint8_t>(), "A linear texture has a single layer.");
		ERR_FAIL_COND_V_MSG(obj->mipmapLevelCount() > 1, Vector<uint8_t>(), "A linear texture has a single mipmap level.");
		Vector<uint8_t> image_data;
		image_data.resize_uninitialized(buf->length());
		memcpy(image_data.ptrw(), buf->contents(), buf->length());
		return image_data;
	}

	DataFormat tex_format = pixel_formats->getDataFormat(obj->pixelFormat());
	uint32_t tex_w = obj->width();
	uint32_t tex_h = obj->height();
	uint32_t tex_d = obj->depth();
	uint32_t tex_mipmaps = obj->mipmapLevelCount();

	// Must iteratively copy the texture data to a buffer.

	uint32_t tight_mip_size = get_image_format_required_size(tex_format, tex_w, tex_h, tex_d, tex_mipmaps);

	Vector<uint8_t> image_data;
	image_data.resize(tight_mip_size);

	uint32_t pixel_size = get_image_format_pixel_size(tex_format);
	uint32_t blockw = 0, blockh = 0;
	get_compressed_image_format_block_dimensions(tex_format, blockw, blockh);

	uint8_t *dest_ptr = image_data.ptrw();

	for (uint32_t mm_i = 0; mm_i < tex_mipmaps; mm_i++) {
		uint32_t bw = STEPIFY(tex_w, blockw);
		uint32_t bh = STEPIFY(tex_h, blockh);

		uint32_t bytes_per_row = get_compressed_image_format_pixels_shifted(tex_format, bw * pixel_size);
		uint32_t bytes_per_img = bytes_per_row * bh;
		uint32_t mip_size = bytes_per_img * tex_d;

		obj->getBytes(dest_ptr, bytes_per_row, bytes_per_img, MTL::Region(0, 0, 0, bw, bh, tex_d), mm_i, p_layer);

		dest_ptr += mip_size;

		// Next mipmap level.
		tex_w = MAX(blockw, tex_w >> 1);
		tex_h = MAX(blockh, tex_h >> 1);
		tex_d = MAX(1u, tex_d >> 1);
	}

	// Ensure that the destination pointer is at the end of the image data.
	DEV_ASSERT(dest_ptr - image_data.ptr() == image_data.size());

	return image_data;
}

BitField<RDD::TextureUsageBits> RenderingDeviceDriverMetal::texture_get_usages_supported_by_format(DataFormat p_format, bool p_cpu_readable) {
	PixelFormats &pf = *pixel_formats;
	if (pf.getMTLPixelFormat(p_format) == MTL::PixelFormatInvalid) {
		return 0;
	}

	MTLFmtCaps caps = pf.getCapabilities(p_format);

	// Everything supported by default makes an all-or-nothing check easier for the caller.
	BitField<RDD::TextureUsageBits> supported = INT64_MAX;
	supported.clear_flag(TEXTURE_USAGE_VRS_ATTACHMENT_BIT); // No VRS support for Metal.

	if (!flags::any(caps, kMTLFmtCapsColorAtt)) {
		supported.clear_flag(TEXTURE_USAGE_COLOR_ATTACHMENT_BIT);
	}
	if (!flags::any(caps, kMTLFmtCapsDSAtt)) {
		supported.clear_flag(TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
	}
	if (!flags::any(caps, kMTLFmtCapsRead)) {
		supported.clear_flag(TEXTURE_USAGE_SAMPLING_BIT);
	}
	if (!flags::any(caps, kMTLFmtCapsAtomic)) {
		supported.clear_flag(TEXTURE_USAGE_STORAGE_ATOMIC_BIT);
	}

	return supported;
}

bool RenderingDeviceDriverMetal::texture_can_make_shared_with_format(TextureID p_texture, DataFormat p_format, bool &r_raw_reinterpretation) {
	r_raw_reinterpretation = false;
	return true;
}

#pragma mark - Sampler

static const MTL::CompareFunction COMPARE_OPERATORS[RDD::COMPARE_OP_MAX] = {
	MTL::CompareFunctionNever,
	MTL::CompareFunctionLess,
	MTL::CompareFunctionEqual,
	MTL::CompareFunctionLessEqual,
	MTL::CompareFunctionGreater,
	MTL::CompareFunctionNotEqual,
	MTL::CompareFunctionGreaterEqual,
	MTL::CompareFunctionAlways,
};

static const MTL::StencilOperation STENCIL_OPERATIONS[RDD::STENCIL_OP_MAX] = {
	MTL::StencilOperationKeep,
	MTL::StencilOperationZero,
	MTL::StencilOperationReplace,
	MTL::StencilOperationIncrementClamp,
	MTL::StencilOperationDecrementClamp,
	MTL::StencilOperationInvert,
	MTL::StencilOperationIncrementWrap,
	MTL::StencilOperationDecrementWrap,
};

static const MTL::BlendFactor BLEND_FACTORS[RDD::BLEND_FACTOR_MAX] = {
	MTL::BlendFactorZero,
	MTL::BlendFactorOne,
	MTL::BlendFactorSourceColor,
	MTL::BlendFactorOneMinusSourceColor,
	MTL::BlendFactorDestinationColor,
	MTL::BlendFactorOneMinusDestinationColor,
	MTL::BlendFactorSourceAlpha,
	MTL::BlendFactorOneMinusSourceAlpha,
	MTL::BlendFactorDestinationAlpha,
	MTL::BlendFactorOneMinusDestinationAlpha,
	MTL::BlendFactorBlendColor,
	MTL::BlendFactorOneMinusBlendColor,
	MTL::BlendFactorBlendAlpha,
	MTL::BlendFactorOneMinusBlendAlpha,
	MTL::BlendFactorSourceAlphaSaturated,
	MTL::BlendFactorSource1Color,
	MTL::BlendFactorOneMinusSource1Color,
	MTL::BlendFactorSource1Alpha,
	MTL::BlendFactorOneMinusSource1Alpha,
};
static const MTL::BlendOperation BLEND_OPERATIONS[RDD::BLEND_OP_MAX] = {
	MTL::BlendOperationAdd,
	MTL::BlendOperationSubtract,
	MTL::BlendOperationReverseSubtract,
	MTL::BlendOperationMin,
	MTL::BlendOperationMax,
};

static const MTL::SamplerAddressMode ADDRESS_MODES[RDD::SAMPLER_REPEAT_MODE_MAX] = {
	MTL::SamplerAddressModeRepeat,
	MTL::SamplerAddressModeMirrorRepeat,
	MTL::SamplerAddressModeClampToEdge,
	MTL::SamplerAddressModeClampToBorderColor,
	MTL::SamplerAddressModeMirrorClampToEdge,
};

static const MTL::SamplerBorderColor SAMPLER_BORDER_COLORS[RDD::SAMPLER_BORDER_COLOR_MAX] = {
	MTL::SamplerBorderColorTransparentBlack,
	MTL::SamplerBorderColorTransparentBlack,
	MTL::SamplerBorderColorOpaqueBlack,
	MTL::SamplerBorderColorOpaqueBlack,
	MTL::SamplerBorderColorOpaqueWhite,
	MTL::SamplerBorderColorOpaqueWhite,
};

RDD::SamplerID RenderingDeviceDriverMetal::sampler_create(const SamplerState &p_state) {
	NS::SharedPtr<MTL::SamplerDescriptor> desc = NS::TransferPtr(MTL::SamplerDescriptor::alloc()->init());
	desc->setSupportArgumentBuffers(true);

	desc->setMagFilter(p_state.mag_filter == SAMPLER_FILTER_LINEAR ? MTL::SamplerMinMagFilterLinear : MTL::SamplerMinMagFilterNearest);
	desc->setMinFilter(p_state.min_filter == SAMPLER_FILTER_LINEAR ? MTL::SamplerMinMagFilterLinear : MTL::SamplerMinMagFilterNearest);
	desc->setMipFilter(p_state.mip_filter == SAMPLER_FILTER_LINEAR ? MTL::SamplerMipFilterLinear : MTL::SamplerMipFilterNearest);

	MTL::SamplerAddressMode address_u = ADDRESS_MODES[p_state.repeat_u];
	MTL::SamplerAddressMode address_v = ADDRESS_MODES[p_state.repeat_v];
	MTL::SamplerAddressMode address_w = ADDRESS_MODES[p_state.repeat_w];

	if (!device_properties->features.supports_border_color) {
		// Default to clamp to edge if border color is not supported.
		if (address_u == MTL::SamplerAddressModeClampToBorderColor) {
			address_u = MTL::SamplerAddressModeClampToEdge;
		}
		if (address_v == MTL::SamplerAddressModeClampToBorderColor) {
			address_v = MTL::SamplerAddressModeClampToEdge;
		}
		if (address_w == MTL::SamplerAddressModeClampToBorderColor) {
			address_w = MTL::SamplerAddressModeClampToEdge;
		}
	}

	desc->setSAddressMode(address_u);
	desc->setTAddressMode(address_v);
	desc->setRAddressMode(address_w);

	if (p_state.use_anisotropy) {
		desc->setMaxAnisotropy(p_state.anisotropy_max);
	}

	desc->setCompareFunction(COMPARE_OPERATORS[p_state.compare_op]);

	desc->setLodMinClamp(p_state.min_lod);
	desc->setLodMaxClamp(p_state.max_lod);

	if (device_properties->features.supports_border_color) {
		desc->setBorderColor(SAMPLER_BORDER_COLORS[p_state.border_color]);
	}

	desc->setNormalizedCoordinates(!p_state.unnormalized_uvw);

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 260000 || __IPHONE_OS_VERSION_MAX_ALLOWED >= 260000 || __TV_OS_VERSION_MAX_ALLOWED >= 260000 || __VISION_OS_VERSION_MAX_ALLOWED >= 260000
	if (p_state.lod_bias != 0.0) {
		if (__builtin_available(macOS 26.0, iOS 26.0, tvOS 26.0, visionOS 26.0, *)) {
			desc->setLodBias(p_state.lod_bias);
		}
	}
#endif

	MTL::SamplerState *obj = device->newSamplerState(desc.get());
	ERR_FAIL_NULL_V_MSG(obj, SamplerID(), "newSamplerState failed");
	return SamplerID(reinterpret_cast<uint64_t>(obj));
}

void RenderingDeviceDriverMetal::sampler_free(SamplerID p_sampler) {
	MTL::SamplerState *obj = reinterpret_cast<MTL::SamplerState *>(p_sampler.id);
	obj->release();
}

bool RenderingDeviceDriverMetal::sampler_is_format_supported_for_filter(DataFormat p_format, SamplerFilter p_filter) {
	switch (p_filter) {
		case SAMPLER_FILTER_NEAREST:
			return true;
		case SAMPLER_FILTER_LINEAR: {
			MTLFmtCaps caps = pixel_formats->getCapabilities(p_format);
			return flags::any(caps, kMTLFmtCapsFilter);
		}
	}
}

#pragma mark - Vertex Array

RDD::VertexFormatID RenderingDeviceDriverMetal::vertex_format_create(Span<VertexAttribute> p_vertex_attribs, const VertexAttributeBindingsMap &p_vertex_bindings) {
	MTL::VertexDescriptor *desc = MTL::VertexDescriptor::vertexDescriptor();

	for (const VertexAttributeBindingsMap::KV &kv : p_vertex_bindings) {
		uint32_t idx = get_metal_buffer_index_for_vertex_attribute_binding(kv.key);
		MTL::VertexBufferLayoutDescriptor *ld = desc->layouts()->object(idx);
		if (kv.value.stride != 0) {
			ld->setStepFunction(kv.value.frequency == VERTEX_FREQUENCY_VERTEX ? MTL::VertexStepFunctionPerVertex : MTL::VertexStepFunctionPerInstance);
			ld->setStepRate(1);
			ld->setStride(kv.value.stride);
		} else {
			ld->setStepFunction(MTL::VertexStepFunctionConstant);
			ld->setStepRate(0);
			ld->setStride(0);
		}
		DEV_ASSERT(ld->stride() == desc->layouts()->object(idx)->stride());
	}

	for (const VertexAttribute &vf : p_vertex_attribs) {
		MTL::VertexAttributeDescriptor *attr = desc->attributes()->object(vf.location);
		attr->setFormat((MTL::VertexFormat)pixel_formats->getMTLVertexFormat(vf.format));
		attr->setOffset(vf.offset);
		uint32_t idx = get_metal_buffer_index_for_vertex_attribute_binding(vf.binding);
		attr->setBufferIndex(idx);
		if (vf.stride == 0) {
			// Constant attribute, so we must determine the stride to satisfy Metal API.
			uint32_t stride = desc->layouts()->object(idx)->stride();
			desc->layouts()->object(idx)->setStride(std::max(stride, vf.offset + pixel_formats->getBytesPerBlock(vf.format)));
		}
	}

	desc->retain();
	return VertexFormatID(reinterpret_cast<uint64_t>(desc));
}

void RenderingDeviceDriverMetal::vertex_format_free(VertexFormatID p_vertex_format) {
	{
		MutexLock lock(pipeline_state_cache_mutex);
		LocalVector<RenderPipelineCacheKey> stale_keys;
		for (const KeyValue<RenderPipelineCacheKey, RenderPipelineCacheEntry> &kv : render_pso_cache) {
			if (kv.key.vertex_format_id == p_vertex_format.id) {
				stale_keys.push_back(kv.key);
			}
		}
		for (const RenderPipelineCacheKey &key : stale_keys) {
			render_pso_cache.erase(key);
		}
	}
	MTL::VertexDescriptor *obj = reinterpret_cast<MTL::VertexDescriptor *>(p_vertex_format.id);
	obj->release();
}

#pragma mark - Barriers

void RenderingDeviceDriverMetal::command_pipeline_barrier(
		CommandBufferID p_cmd_buffer,
		BitField<PipelineStageBits> p_src_stages,
		BitField<PipelineStageBits> p_dst_stages,
		VectorView<MemoryAccessBarrier> p_memory_barriers,
		VectorView<BufferBarrier> p_buffer_barriers,
		VectorView<TextureBarrier> p_texture_barriers,
		VectorView<AccelerationStructureBarrier> p_acceleration_structure_barriers) {
	MDCommandBufferBase *obj = (MDCommandBufferBase *)(p_cmd_buffer.id);
	obj->pipeline_barrier(p_src_stages, p_dst_stages, p_memory_barriers, p_buffer_barriers, p_texture_barriers, p_acceleration_structure_barriers);
}

#pragma mark - Queues

RDD::CommandQueueFamilyID RenderingDeviceDriverMetal::command_queue_family_get(BitField<CommandQueueFamilyBits> p_cmd_queue_family_bits, RenderingContextDriver::SurfaceID p_surface) {
	if (p_cmd_queue_family_bits.has_flag(COMMAND_QUEUE_FAMILY_GRAPHICS_BIT) || (p_surface != 0)) {
		return CommandQueueFamilyID(COMMAND_QUEUE_FAMILY_GRAPHICS_BIT);
	} else if (p_cmd_queue_family_bits.has_flag(COMMAND_QUEUE_FAMILY_COMPUTE_BIT)) {
		return CommandQueueFamilyID(COMMAND_QUEUE_FAMILY_COMPUTE_BIT);
	} else if (p_cmd_queue_family_bits.has_flag(COMMAND_QUEUE_FAMILY_TRANSFER_BIT)) {
		return CommandQueueFamilyID(COMMAND_QUEUE_FAMILY_TRANSFER_BIT);
	} else {
		return CommandQueueFamilyID();
	}
}

#pragma mark - Command Buffers

bool RenderingDeviceDriverMetal::command_buffer_begin(CommandBufferID p_cmd_buffer) {
	MDCommandBufferBase *obj = (MDCommandBufferBase *)(p_cmd_buffer.id);
	obj->begin();
	return true;
}

bool RenderingDeviceDriverMetal::command_buffer_begin_secondary(CommandBufferID p_cmd_buffer, RenderPassID p_render_pass, uint32_t p_subpass, FramebufferID p_framebuffer) {
	ERR_FAIL_V_MSG(false, "not implemented");
}

void RenderingDeviceDriverMetal::command_buffer_end(CommandBufferID p_cmd_buffer) {
	MDCommandBufferBase *obj = (MDCommandBufferBase *)(p_cmd_buffer.id);
	obj->end();
}

void RenderingDeviceDriverMetal::command_buffer_execute_secondary(CommandBufferID p_cmd_buffer, VectorView<CommandBufferID> p_secondary_cmd_buffers) {
	ERR_FAIL_MSG("not implemented");
}

#pragma mark - Swap Chain

void RenderingDeviceDriverMetal::_swap_chain_release(SwapChain *p_swap_chain) {
	_swap_chain_release_buffers(p_swap_chain);

	if (p_swap_chain->render_pass.id != 0) {
		render_pass_free(p_swap_chain->render_pass);
		p_swap_chain->render_pass = RenderPassID();
	}
}

void RenderingDeviceDriverMetal::_swap_chain_release_buffers(SwapChain *p_swap_chain) {
}

RDD::SwapChainID RenderingDeviceDriverMetal::swap_chain_create(RenderingContextDriver::SurfaceID p_surface) {
	const RenderingContextDriverMetal::Surface *surface = (RenderingContextDriverMetal::Surface *)(p_surface);
	if (sync_mode != HazardTracking) {
		if (__builtin_available(macOS 26.0, iOS 26.0, tvOS 26.0, visionOS 26.0, *)) {
			add_residency_set_to_main_queue(surface->get_residency_set());
		}
	}

	SwapChain *swap_chain = memnew(SwapChain);
	swap_chain->surface = p_surface;
	return SwapChainID(swap_chain);
}

RDD::RenderPassID RenderingDeviceDriverMetal::_swap_chain_create_render_pass(RDD::DataFormat p_format) {
	// Create the render pass that will be used to draw to the swap chain's framebuffers.
	RDD::Attachment attachment;
	attachment.format = p_format;
	attachment.samples = RDD::TEXTURE_SAMPLES_1;
	attachment.load_op = RDD::ATTACHMENT_LOAD_OP_CLEAR;
	attachment.store_op = RDD::ATTACHMENT_STORE_OP_STORE;

	RDD::Subpass subpass;
	RDD::AttachmentReference color_ref;
	color_ref.attachment = 0;
	color_ref.aspect.set_flag(RDD::TEXTURE_ASPECT_COLOR_BIT);
	subpass.color_references.push_back(color_ref);

	return render_pass_create(attachment, subpass, {}, 1, RDD::AttachmentReference());
}

Error RenderingDeviceDriverMetal::swap_chain_resize(CommandQueueID p_cmd_queue, SwapChainID p_swap_chain, uint32_t p_desired_framebuffer_count) {
	DEV_ASSERT(p_cmd_queue.id != 0);
	DEV_ASSERT(p_swap_chain.id != 0);

	SwapChain *swap_chain = (SwapChain *)(p_swap_chain.id);
	RenderingContextDriverMetal::Surface *surface = (RenderingContextDriverMetal::Surface *)(swap_chain->surface);

	DataFormat new_data_format = DATA_FORMAT_MAX;
	ColorSpace new_color_space = COLOR_SPACE_MAX;
	RETURN_IF_ERROR(surface->resize(p_desired_framebuffer_count, new_data_format, new_color_space));

	if (new_data_format != swap_chain->data_format) {
		_swap_chain_release(swap_chain);
		swap_chain->render_pass = _swap_chain_create_render_pass(new_data_format);
		if (!swap_chain->render_pass) {
			return ERR_INVALID_DATA;
		}
	}

	swap_chain->data_format = new_data_format;
	swap_chain->color_space = new_color_space;

	// Once everything's been created correctly, indicate the surface no longer needs to be resized.
	context_driver->surface_set_needs_resize(swap_chain->surface, false);

	return OK;
}

RDD::FramebufferID RenderingDeviceDriverMetal::swap_chain_acquire_framebuffer(CommandQueueID p_cmd_queue, SwapChainID p_swap_chain, bool &r_resize_required) {
	DEV_ASSERT(p_cmd_queue.id != 0);
	DEV_ASSERT(p_swap_chain.id != 0);

	SwapChain *swap_chain = (SwapChain *)(p_swap_chain.id);
	if (context_driver->surface_get_needs_resize(swap_chain->surface)) {
		r_resize_required = true;
		return FramebufferID();
	}

	RenderingContextDriverMetal::Surface *metal_surface = (RenderingContextDriverMetal::Surface *)(swap_chain->surface);
	return metal_surface->acquire_next_frame_buffer();
}

RDD::RenderPassID RenderingDeviceDriverMetal::swap_chain_get_render_pass(SwapChainID p_swap_chain) {
	const SwapChain *swap_chain = (const SwapChain *)(p_swap_chain.id);
	return swap_chain->render_pass;
}

RDD::DataFormat RenderingDeviceDriverMetal::swap_chain_get_format(SwapChainID p_swap_chain) {
	const SwapChain *swap_chain = (const SwapChain *)(p_swap_chain.id);
	return swap_chain->data_format;
}

RDD::ColorSpace RenderingDeviceDriverMetal::swap_chain_get_color_space(SwapChainID p_swap_chain) {
	const SwapChain *swap_chain = (const SwapChain *)(p_swap_chain.id);
	return swap_chain->color_space;
}

bool RenderingDeviceDriverMetal::swap_chain_get_hdr_output_supported(SwapChainID p_swap_chain) {
	// Our minimum supported version of metal requires support for EDR.
	return true;
}

void RenderingDeviceDriverMetal::swap_chain_set_max_fps(SwapChainID p_swap_chain, int p_max_fps) {
	SwapChain *swap_chain = (SwapChain *)(p_swap_chain.id);
	RenderingContextDriverMetal::Surface *metal_surface = (RenderingContextDriverMetal::Surface *)(swap_chain->surface);
	metal_surface->set_max_fps(p_max_fps);
}

void RenderingDeviceDriverMetal::swap_chain_free(SwapChainID p_swap_chain) {
	SwapChain *swap_chain = (SwapChain *)(p_swap_chain.id);
	if (sync_mode != HazardTracking) {
		if (__builtin_available(macOS 26.0, iOS 26.0, tvOS 26.0, visionOS 26.0, *)) {
			RenderingContextDriverMetal::Surface *surface = (RenderingContextDriverMetal::Surface *)(swap_chain->surface);
			remove_residency_set_to_main_queue(surface->get_residency_set());
		}
	}
	_swap_chain_release(swap_chain);
	memdelete(swap_chain);
}

#pragma mark - Frame buffer

RDD::FramebufferID RenderingDeviceDriverMetal::framebuffer_create(RenderPassID p_render_pass, VectorView<TextureID> p_attachments, uint32_t p_width, uint32_t p_height) {
	MDRenderPass *pass = (MDRenderPass *)(p_render_pass.id);

	Vector<MTL::Texture *> textures;
	textures.resize(p_attachments.size());
	MTL::RasterizationRateMap *rasterization_rate_map = nullptr;

	for (uint32_t i = 0; i < p_attachments.size(); i += 1) {
		const MDAttachment &a = pass->attachments[i];
		TextureInfo *tex_info = (TextureInfo *)p_attachments[i].id;
		if (tex_info->rasterization_rate_map) {
			rasterization_rate_map = reinterpret_cast<MTL::RasterizationRateMap *>(tex_info->texture.get());
			textures.write[i] = nullptr;
		} else {
			textures.write[i] = tex_info->texture.get();
		}
		if (a.samples > 1) {
			MTL::Texture *tex = textures[i];
			if (tex != nullptr && tex->sampleCount() != a.samples) {
#if DEV_ENABLED
				WARN_PRINT("Mismatched sample count for attachment " + itos(i) + "; expected " + itos(a.samples) + ", got " + itos(tex->sampleCount()));
#endif
			}
		}
	}

	MDFrameBufferTexture *fb = memnew(MDFrameBufferTexture(textures, Size2i(p_width, p_height)));
	fb->rasterization_rate_map = rasterization_rate_map;
	return FramebufferID(fb);
}

void RenderingDeviceDriverMetal::framebuffer_free(FramebufferID p_framebuffer) {
	MDFrameBuffer *obj = (MDFrameBuffer *)(p_framebuffer.id);
	if (obj) {
		memdelete(obj);
	}
}

#pragma mark - Shader

void RenderingDeviceDriverMetal::evict_compute_pso_for_library(const SHA256Digest &p_library_key) {
	NS::SharedPtr<NS::AutoreleasePool> pool = NS::TransferPtr(NS::AutoreleasePool::alloc()->init());
	MutexLock lock(pipeline_state_cache_mutex);
	if (compute_shader_owners_by_library.has(p_library_key)) {
		return;
	}
	if (LocalVector<ComputePipelineCacheKey> *keys = compute_pso_keys_by_library.getptr(p_library_key)) {
		uint64_t evicted = 0;
		for (const ComputePipelineCacheKey &key : *keys) {
			if (compute_pso_cache.erase(key)) {
				++evicted;
			}
		}
		compute_pso_keys_by_library.erase(p_library_key);
		if (evicted) {
			total_compute_pso_cache_evictions.fetch_add(evicted, std::memory_order_relaxed);
		}
	}
}

void RenderingDeviceDriverMetal::shader_cache_free_entry(ShaderCacheEntry *p_entry) {
	SHA256Digest evicted_key;
	bool should_evict_compute_pso = false;
	{
		MutexLock lock(_shader_cache_lock);
		// Only remove the map slot if it still refers to this exact entry. A concurrent
		// creation for the same hash may have replaced it, in which case that newer entry
		// (and its library) must be left untouched.
		if (ShaderCacheEntry **pentry = _shader_cache.getptr(p_entry->key); pentry != nullptr && *pentry == p_entry) {
			evicted_key = p_entry->key;
			should_evict_compute_pso = true;
			_shader_cache.erase(p_entry->key);
		}
		memdelete(p_entry);
	}
	if (should_evict_compute_pso) {
		evict_compute_pso_for_library(evicted_key);
	}
}

std::optional<std::shared_ptr<MDLibrary>> RenderingDeviceDriverMetal::shader_cache_get_library(const SHA256Digest &key) {
	MutexLock lock(_shader_cache_lock);

	if (ShaderCacheEntry **p = _shader_cache.getptr(key); p != nullptr) {
		if (std::shared_ptr<MDLibrary> lib = (*p)->library.lock()) {
			return lib;
		}
		// Library was released; remove stale cache entry and recreate.
		_shader_cache.erase(key);
	}

	return std::nullopt;
}

void RenderingDeviceDriverMetal::shader_cache_set_entry(const SHA256Digest &key, ShaderCacheEntry *p_entry) {
	MutexLock lock(_shader_cache_lock);
	_shader_cache[key] = p_entry;
}

template <typename T, typename U>
struct is_layout_compatible
		: std::bool_constant<
				  sizeof(T) == sizeof(U) &&
				  alignof(T) == alignof(U) &&
				  std::is_trivially_copyable_v<T> &&
				  std::is_trivially_copyable_v<U>> {};
static_assert(is_layout_compatible<UniformInfo::Indexes, RenderingShaderContainerMetal::UniformData::Indexes>::value, "UniformInfo::Indexes layout does not match RenderingShaderContainerMetal::UniformData::Indexes layout");

API_AVAILABLE(macos(11.0), ios(14.0), tvos(14.0))
static void update_uniform_info(const RenderingShaderContainerMetal::UniformData &p_data, UniformInfo &r_ui) {
	r_ui.active_stages = p_data.active_stages;
	r_ui.dataType = static_cast<MTL::DataType>(p_data.data_type);
	memcpy(&r_ui.slot, &p_data.slot, sizeof(UniformInfo::Indexes));
	memcpy(&r_ui.arg_buffer, &p_data.arg_buffer, sizeof(UniformInfo::Indexes));
	r_ui.access = static_cast<MTL::BindingAccess>(p_data.access);
	r_ui.usage = static_cast<MTL::ResourceUsage>(p_data.usage);
	r_ui.textureType = static_cast<MTL::TextureType>(p_data.texture_type);
	r_ui.imageFormat = p_data.image_format;
	r_ui.arrayLength = p_data.array_length;
	r_ui.isMultisampled = p_data.is_multisampled;
}

RDD::ShaderID RenderingDeviceDriverMetal::shader_create_from_container(const Ref<RenderingShaderContainer> &p_shader_container, const Vector<ImmutableSampler> &p_immutable_samplers) {
	Ref<RenderingShaderContainerMetal> shader_container = p_shader_container;
	using RSCM = RenderingShaderContainerMetal;

	if (shader_container->is_invalid()) {
		WARN_PRINT("Metal shader container is invalid and will be recompiled.");
		return RDD::ShaderID();
	}

	CharString shader_name = shader_container->shader_name;
	RSCM::HeaderData &mtl_reflection_data = shader_container->mtl_reflection_data;
	Vector<RenderingShaderContainer::Shader> &shaders = shader_container->shaders;
	Vector<RSCM::StageData> &mtl_shaders = shader_container->mtl_shaders;

	ERR_FAIL_COND_V(!mtl_reflection_data.has_valid_profile(), RDD::ShaderID());

	// We need to regenerate the shader if the cache is moved to an incompatible device or argument buffer support differs.
	ERR_FAIL_COND_V_MSG(!device_properties->features.argument_buffers_supported() && mtl_reflection_data.uses_argument_buffers(),
			RDD::ShaderID(),
			"Shader was compiled with argument buffers enabled, but this device does not support them");

	ERR_FAIL_COND_V_MSG(device_properties->features.msl_max_version < mtl_reflection_data.msl_version,
			RDD::ShaderID(),
			"Shader was compiled for a newer version of Metal");

	MTL::GPUFamily compiled_gpu_family = static_cast<MTL::GPUFamily>(mtl_reflection_data.profile.gpu);
	ERR_FAIL_COND_V_MSG(device_properties->features.highestFamily < compiled_gpu_family,
			RDD::ShaderID(),
			"Shader was generated for a newer Apple GPU");

	ERR_FAIL_COND_V(mtl_reflection_data.profile.platform != device_profile.platform, RDD::ShaderID());
	ERR_FAIL_COND_V(uint32_t(mtl_reflection_data.profile.platform) > uint32_t(MetalDeviceProfile::Platform::visionOS), RDD::ShaderID());
	ERR_FAIL_COND_V(uint32_t(mtl_reflection_data.profile.gpu) < 1001 || uint32_t(mtl_reflection_data.profile.gpu) > 1009, RDD::ShaderID());
	ERR_FAIL_COND_V(shaders.size() != mtl_shaders.size() || shaders.is_empty(), RDD::ShaderID());

	HashMap<RDD::ShaderStage, std::shared_ptr<MDLibrary>> libraries;

	PipelineType pipeline_type = PIPELINE_TYPE_RASTERIZATION;
	Vector<uint8_t> decompressed_code;
	for (uint32_t shader_index = 0; shader_index < shaders.size(); shader_index++) {
		NS::SharedPtr<NS::AutoreleasePool> pool = NS::TransferPtr(NS::AutoreleasePool::alloc()->init());
		const RenderingShaderContainer::Shader &shader = shaders[shader_index];
		const RSCM::StageData &shader_data = mtl_shaders[shader_index];

		if (shader.shader_stage == RDD::ShaderStage::SHADER_STAGE_COMPUTE) {
			pipeline_type = PIPELINE_TYPE_COMPUTE;
		}
		ERR_FAIL_COND_V(shader_data.source_size == 0 || shader_data.compile_path_version != 1 || shader_data.supports_fast_math > 1 || shader_data.is_position_invariant > 1, RDD::ShaderID());
		const bool effective_disable = ShaderCompilePolicy::effective_disable(shader_container->get_compile_flags(), shader_data.supports_fast_math);
		ERR_FAIL_COND_V(shader_data.effective_disable_fast_math != uint32_t(effective_disable), RDD::ShaderID());
		const uint64_t payload_size = uint64_t(shader_data.source_size) + shader_data.library_size;
		ERR_FAIL_COND_V(payload_size != (shader.code_decompressed_size ? shader.code_decompressed_size : uint64_t(shader.code_compressed_bytes.size())), RDD::ShaderID());
		ERR_FAIL_COND_V(shader.code_compression_flags & ~uint32_t(RenderingShaderContainer::COMPRESSION_FLAG_ZSTD), RDD::ShaderID());
		ERR_FAIL_COND_V(shader.code_compression_flags && !shader.code_decompressed_size, RDD::ShaderID());
		ERR_FAIL_COND_V(shader_data.library_size > 0 && (mtl_reflection_data.os_min_version.is_null() || mtl_reflection_data.os_min_version > device_properties->os_version), RDD::ShaderID());

		if (shader.code_decompressed_size > 0) {
			decompressed_code.resize(shader.code_decompressed_size);
			bool decompressed = shader_container->decompress_code(shader.code_compressed_bytes.ptr(), shader.code_compressed_bytes.size(), shader.code_compression_flags, decompressed_code.ptrw(), decompressed_code.size());
			ERR_FAIL_COND_V_MSG(!decompressed, RDD::ShaderID(), vformat("Failed to decompress code on shader stage %s.", String(RDD::SHADER_STAGE_NAMES[shader.shader_stage])));
		} else {
			decompressed_code = shader.code_compressed_bytes;
		}

		SHA256Digest source_digest(reinterpret_cast<const char *>(decompressed_code.ptr()), shader_data.source_size);
		ERR_FAIL_COND_V(memcmp(source_digest.data, shader_data.hash.data, sizeof(source_digest.data)) != 0, RDD::ShaderID());
		ShaderCompilePolicy::LibraryKeyInput key_input;
		key_input.compile_path_version = shader_data.compile_path_version;
		key_input.stage = shader.shader_stage;
		key_input.requested_flags = static_cast<uint32_t>(shader_container->get_compile_flags());
		key_input.effective_disable = effective_disable;
		key_input.msl_version = mtl_reflection_data.msl_version;
		key_input.position_invariance = shader_data.is_position_invariant;
		key_input.enable_logging = mtl_reflection_data.needs_debug_logging();
		key_input.compiled_platform = uint32_t(mtl_reflection_data.profile.platform);
		key_input.compiled_gpu_family = uint32_t(mtl_reflection_data.profile.gpu);
		key_input.profile_min_os_version = mtl_reflection_data.profile.min_os_version;
		key_input.binary_os_min_version = shader_data.library_size ? uint32_t(mtl_reflection_data.os_min_version) : UINT32_MAX;
		key_input.actual_platform = uint32_t(device_profile.platform);
		key_input.actual_gpu_family = uint32_t(device_profile.gpu);
		key_input.actual_os_version = device_properties->os_version;
		key_input.origin = shader_data.library_size ? 1 : 0;
		key_input.library_size = shader_data.library_size;
		if (shader_data.library_size) {
			SHA256Digest metallib_digest(reinterpret_cast<const char *>(decompressed_code.ptr() + shader_data.source_size), shader_data.library_size);
			memcpy(key_input.metallib_sha256, metallib_digest.data, sizeof(key_input.metallib_sha256));
		}
		NS::SharedPtr<MTL::CompileOptions> options;
		if (!shader_data.library_size) {
			options = make_shader_compile_options(mtl_reflection_data.msl_version, mtl_reflection_data.needs_debug_logging(), shader_data.is_position_invariant, effective_disable);
		}
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 150000 || __IPHONE_OS_VERSION_MAX_ALLOWED >= 180000 || __TV_OS_VERSION_MAX_ALLOWED >= 180000 || defined(VISIONOS_ENABLED)
		if (__builtin_available(macOS 15.0, iOS 18.0, tvOS 18.0, visionOS 2.0, *)) {
			if (options) {
				key_input.math_floating_point_functions = uint32_t(options->mathFloatingPointFunctions());
			}
		}
#endif
		const PackedByteArray key_bytes = ShaderCompilePolicy::library_key_with_source(key_input, decompressed_code.ptr(), shader_data.source_size);
		ERR_FAIL_COND_V(key_bytes.size() != 32, RDD::ShaderID());
		const SHA256Digest library_key(reinterpret_cast<const char *>(key_bytes.ptr()));
#ifdef TESTS_ENABLED
		Dictionary audit;
		audit["shader_name"] = String::utf8(shader_name.get_data());
		audit["stage"] = int(shader.shader_stage);
		audit["source_digest"] = String::hex_encode_buffer(source_digest.data, sizeof(source_digest.data));
		audit["library_key"] = String::hex_encode_buffer(key_bytes.ptr(), key_bytes.size());
		audit["required_flags"] = key_input.requested_flags;
		audit["effective_disable"] = effective_disable;
		audit["compile_path"] = shader_data.library_size ? "precompiled_metallib" : "runtime_source";
		audit["phase"] = "library_lookup";
		audit["cache_hit"] = false;
		if (shader_data.library_size) {
			audit["metallib_digest"] = String::hex_encode_buffer(key_input.metallib_sha256, 32);
			audit["library_size"] = shader_data.library_size;
			audit["container_digest"] = shader_container->get_loaded_container_digest();
			audit["container_size"] = shader_container->get_loaded_container_size();
		}
#endif
		if (std::optional<std::shared_ptr<MDLibrary>> lib = shader_cache_get_library(library_key); lib) {
#ifdef TESTS_ENABLED
			audit["cache_hit"] = true;
			const uint64_t creation_seq = (*lib)->audit_creation_seq();
			audit["creation_seq"] = creation_seq ? Variant(creation_seq) : Variant();
			ShaderCompileAudit::emit(audit);
#endif
			libraries[shader.shader_stage] = std::move(*lib);
			continue;
		}
#ifdef TESTS_ENABLED
		ShaderCompileAudit::emit(audit);
		if (options) {
			audit["phase"] = "runtime_options";
			audit["actual_options"] = observe_shader_compile_options(options.get());
			ShaderCompileAudit::emit(audit);
		}
#endif
		ShaderCacheEntry *cd = memnew(ShaderCacheEntry(*this, library_key));
#ifdef TESTS_ENABLED
		cd->audit_context = audit;
#endif
		cd->name = shader_name;
		cd->stage = shader.shader_stage;

		std::shared_ptr<MDLibrary> library;
		if (shader_data.library_size > 0) {
			if (mtl_reflection_data.os_min_version > device_properties->os_version) {
				memdelete(cd);
				ERR_FAIL_V_MSG(RDD::ShaderID(), "Metal shader binary was generated for a newer target OS");
			}
#ifdef TESTS_ENABLED
			audit["phase"] = "binary_load";
			cd->creation_seq = ShaderCompileAudit::emit(audit);
#endif
#if DEV_ENABLED
			NS::SharedPtr<NS::String> source = NS::TransferPtr(NS::String::alloc()->init((void *)decompressed_code.ptr(), shader_data.source_size, NS::UTF8StringEncoding));
#endif
			dispatch_data_t binary = dispatch_data_create(decompressed_code.ptr() + shader_data.source_size, shader_data.library_size, dispatch_get_main_queue(), DISPATCH_DATA_DESTRUCTOR_DEFAULT);
			library = MDLibrary::create(cd, device,
#if DEV_ENABLED
					source.get(),
#endif
					binary);
			dispatch_release(binary);
		} else if (_shader_load_strategy == ShaderLoadStrategy::LAZY) {
			// Retain compact zstd-compressed MSL bytes instead of an eagerly decompressed NS::String
			// (BUG-188B). Decompressed transiently inside MDLazyLibrary::_load_locked().
			const uint32_t decomp_size = shader.code_decompressed_size ? shader.code_decompressed_size : uint32_t(shader.code_compressed_bytes.size());
			library = MDLibrary::create_lazy_compressed(cd, device, shader.code_compressed_bytes, decomp_size, shader_data.source_size, shader.code_compression_flags, options.get());
		} else {
			NS::SharedPtr<NS::String> source = NS::TransferPtr(NS::String::alloc()->init((void *)decompressed_code.ptr(), shader_data.source_size, NS::UTF8StringEncoding));
			library = MDLibrary::create(cd, device, source.get(), options.get(), _shader_load_strategy);
		}

		shader_cache_set_entry(library_key, cd);
		libraries[shader.shader_stage] = library;
	}

	ShaderReflection refl = shader_container->get_shader_reflection();
	RSCM::MetalShaderReflection mtl_refl = shader_container->get_metal_shader_reflection();

	Vector<UniformSet> uniform_sets;
	uint32_t uniform_sets_count = mtl_refl.uniform_sets.size();
	uniform_sets.resize(uniform_sets_count);

	DynamicOffsetLayout dynamic_offset_layout;
	uint8_t dynamic_offset = 0;

	// Create sets.
	for (uint32_t i = 0; i < uniform_sets_count; i++) {
		UniformSet &set = uniform_sets.write[i];
		const Vector<ShaderUniform> &refl_set = refl.uniform_sets.ptr()[i];
		const Vector<RSCM::UniformData> &mtl_set = mtl_refl.uniform_sets.ptr()[i];
		uint32_t set_size = mtl_set.size();
		set.uniforms.resize(set_size);

		uint8_t dynamic_count = 0;
		bool has_storage_buffer = false;

		LocalVector<UniformInfo>::Iterator iter = set.uniforms.begin();
		for (uint32_t j = 0; j < set_size; j++) {
			const ShaderUniform &uniform = refl_set.ptr()[j];
			has_storage_buffer |= uniform.type == UNIFORM_TYPE_STORAGE_BUFFER || uniform.type == UNIFORM_TYPE_STORAGE_BUFFER_DYNAMIC;
			const RSCM::UniformData &bind = mtl_set.ptr()[j];

			switch (uniform.type) {
				case UNIFORM_TYPE_STORAGE_BUFFER_DYNAMIC:
				case UNIFORM_TYPE_UNIFORM_BUFFER_DYNAMIC: {
					set.dynamic_uniforms.push_back(j);
					dynamic_count++;
				} break;
				default: {
				} break;
			}

			UniformInfo &ui = *iter;
			++iter;
			update_uniform_info(bind, ui);
			ui.binding = uniform.binding;

			if (ui.arg_buffer.texture == UINT32_MAX && ui.arg_buffer.buffer == UINT32_MAX && ui.arg_buffer.sampler == UINT32_MAX) {
				// No bindings.
				continue;
			}
#define VAL(x) (x == UINT32_MAX ? 0 : x)
			const uint64_t end = uint64_t(std::max({ VAL(ui.arg_buffer.texture), VAL(ui.arg_buffer.buffer), VAL(ui.arg_buffer.sampler) })) + std::max(ui.arrayLength, 1u);
			ERR_FAIL_COND_V_MSG(end > uint64_t(INT32_MAX) / sizeof(uint64_t), ShaderID(), "Metal argument layout exceeds the allocation limit.");
			set.buffer_size = std::max(set.buffer_size, uint32_t(end * sizeof(uint64_t)));
#undef VAL
		}
		// Layout depends only on the reflected set, never active-stage usage or
		// OpArrayLength presence. A no-length shader can create a set for another
		// compatible shader that does use length(). The tail shares ownership of
		// the ordinary argument allocation (or its per-command dynamic copy).
		if (has_storage_buffer) {
			ERR_FAIL_COND_V_MSG(!set.buffer_sizes.initialize(set.buffer_size / sizeof(uint64_t)), ShaderID(), "Metal buffer-size table exceeds the argument allocation limit.");
			set.buffer_size = set.buffer_sizes.allocation_size;
		}

		if (dynamic_count > 0) {
			dynamic_offset_layout.set_offset_count(i, dynamic_offset, dynamic_count);
			dynamic_offset += dynamic_count;
		}
	}

	MDShader *shader = nullptr;
	if (pipeline_type == PIPELINE_TYPE_COMPUTE) {
		MDComputeShader *cs = new MDComputeShader(
				shader_name,
				uniform_sets,
				mtl_reflection_data.uses_argument_buffers(),
				libraries[RDD::ShaderStage::SHADER_STAGE_COMPUTE]);

		cs->local = MTL::Size(refl.compute_local_size[0], refl.compute_local_size[1], refl.compute_local_size[2]);
		cs->has_specialization_constants = !refl.specialization_constants.is_empty();
		if (cs->kernel && cs->kernel->get_cache_key() != nullptr) {
			cs->library_key = *cs->kernel->get_cache_key();
			cs->has_library_key = true;
			MutexLock lock(pipeline_state_cache_mutex);
			uint32_t &owners = compute_shader_owners_by_library[cs->library_key];
			++owners;
		}
		shader = cs;
	} else {
		MDRenderShader *rs = new MDRenderShader(
				shader_name,
				uniform_sets,
				mtl_reflection_data.needs_view_mask_buffer(),
				mtl_reflection_data.uses_argument_buffers(),
				libraries[RDD::ShaderStage::SHADER_STAGE_VERTEX],
				libraries[RDD::ShaderStage::SHADER_STAGE_FRAGMENT]);
		shader = rs;
	}

	shader->push_constants.stages = refl.push_constant_stages;
	shader->push_constants.size = refl.push_constant_size;
	shader->push_constants.binding = mtl_reflection_data.push_constant_binding;
	shader->dynamic_offset_layout = dynamic_offset_layout;

	return RDD::ShaderID(shader);
}

bool RenderingDeviceDriverMetal::shader_retire_ready_nonblocking(ShaderID p_shader) const {
	if (!p_shader) {
		return false;
	}
	const auto *shader = reinterpret_cast<MDShader *>(p_shader.id);
	if (const auto *compute_shader = dynamic_cast<const MDComputeShader *>(shader)) {
		MutexLock lock(pipeline_state_cache_mutex);
		return compute_shader->cached_unspecialized_pso.get() != nullptr ||
				(compute_shader->kernel && compute_shader->kernel->retire_ready_nonblocking());
	}
	if (const auto *render_shader = dynamic_cast<const MDRenderShader *>(shader)) {
		MutexLock lock(pipeline_state_cache_mutex);
		return render_shader->modules_destroyed ||
				(render_shader->vert && render_shader->frag &&
						render_shader->vert->retire_ready_nonblocking() && render_shader->frag->retire_ready_nonblocking());
	}
	return false;
}

void RenderingDeviceDriverMetal::shader_free(ShaderID p_shader) {
	NS::SharedPtr<NS::AutoreleasePool> pool = NS::TransferPtr(NS::AutoreleasePool::alloc()->init());
	MDShader *obj = (MDShader *)p_shader.id;
	SHA256Digest evicted_compute_key;
	bool should_evict_compute_pso = false;
	{
		MutexLock lock(pipeline_state_cache_mutex);
		LocalVector<RenderPipelineCacheKey> stale_render_keys;
		for (const KeyValue<RenderPipelineCacheKey, RenderPipelineCacheEntry> &kv : render_pso_cache) {
			if (kv.key.shader_id == p_shader.id) {
				stale_render_keys.push_back(kv.key);
			}
		}
		for (const RenderPipelineCacheKey &key : stale_render_keys) {
			render_pso_cache.erase(key);
		}
		if (MDComputeShader *cs = dynamic_cast<MDComputeShader *>(obj); cs != nullptr && cs->has_library_key) {
			if (uint32_t *owners = compute_shader_owners_by_library.getptr(cs->library_key)) {
				if (*owners <= 1u) {
					compute_shader_owners_by_library.erase(cs->library_key);
					evicted_compute_key = cs->library_key;
					should_evict_compute_pso = true;
				} else {
					--(*owners);
				}
			}
		}
	}
	if (should_evict_compute_pso) {
		evict_compute_pso_for_library(evicted_compute_key);
	}
	delete obj;
}

void RenderingDeviceDriverMetal::shader_destroy_modules(ShaderID p_shader) {
	if (!p_shader) {
		return;
	}
	NS::SharedPtr<NS::AutoreleasePool> pool = NS::TransferPtr(NS::AutoreleasePool::alloc()->init());
	MDShader *shader = reinterpret_cast<MDShader *>(p_shader.id);
	if (MDComputeShader *cs = dynamic_cast<MDComputeShader *>(shader); cs != nullptr) {
		std::shared_ptr<MDLibrary> dropped_kernel;
		{
			MutexLock lock(pipeline_state_cache_mutex);
			dropped_kernel = std::move(cs->kernel);
		}
		if (dropped_kernel) {
			dropped_kernel.reset();
			total_compute_libraries_released.fetch_add(1, std::memory_order_relaxed);
		}
	} else if (MDRenderShader *rs = dynamic_cast<MDRenderShader *>(shader); rs != nullptr) {
		std::shared_ptr<MDLibrary> dropped_vert;
		std::shared_ptr<MDLibrary> dropped_frag;
		{
			MutexLock lock(pipeline_state_cache_mutex);
			dropped_vert = std::move(rs->vert);
			dropped_frag = std::move(rs->frag);
			rs->modules_destroyed = true;
		}
		dropped_vert.reset();
		dropped_frag.reset();
	}
}

/*********************/
/**** UNIFORM SET ****/
/*********************/

RDD::UniformSetID RenderingDeviceDriverMetal::uniform_set_create(VectorView<BoundUniform> p_uniforms, ShaderID p_shader, uint32_t p_set_index, int p_linear_pool_index) {
	//p_linear_pool_index = -1; // TODO:? Linear pools not implemented or not supported by API backend.
	last_uniform_set_create_error = OK;

	MDShader *shader = (MDShader *)(p_shader.id);
	ERR_FAIL_INDEX_V_MSG(p_set_index, shader->sets.size(), UniformSetID(), "Set index out of range");
	const UniformSet &shader_set = shader->sets.get(p_set_index);
	ERR_FAIL_COND_V(p_uniforms.size() != shader_set.uniforms.size(), UniformSetID());
	MDUniformSet *set = memnew(MDUniformSet);
	// Determine if there are any dynamic uniforms in this set.
	bool is_dynamic = !shader_set.dynamic_uniforms.is_empty();

	Vector<uint8_t> arg_buffer_data;

	if (device_properties->features.argument_buffers_supported()) {
		if (arg_buffer_data.resize(shader_set.buffer_size) != OK) {
			memdelete(set);
			ERR_FAIL_V(UniformSetID());
		}
		arg_buffer_data.fill(0);

		// If argument buffers are enabled, we have already verified availability, so we can skip the runtime check.
		GODOT_CLANG_WARNING_PUSH_AND_IGNORE("-Wunguarded-availability-new")
		uint64_t *ptr = (uint64_t *)arg_buffer_data.ptrw();

		HashMap<MTL::Resource *, StageResourceUsage, HashMapHasherDefault> bound_resources;
		auto add_usage = [&bound_resources](MTL::Resource *res, BitField<RDD::ShaderStage> stage, MTL::ResourceUsage usage) {
			StageResourceUsage *sru = bound_resources.getptr(res);
			if (sru == nullptr) {
				sru = &bound_resources.insert(res, ResourceUnused)->value;
			}
			if (stage.has_flag(RDD::SHADER_STAGE_VERTEX_BIT)) {
				*sru |= stage_resource_usage(RDD::SHADER_STAGE_VERTEX, usage);
			}
			if (stage.has_flag(RDD::SHADER_STAGE_FRAGMENT_BIT)) {
				*sru |= stage_resource_usage(RDD::SHADER_STAGE_FRAGMENT, usage);
			}
			if (stage.has_flag(RDD::SHADER_STAGE_COMPUTE_BIT)) {
				*sru |= stage_resource_usage(RDD::SHADER_STAGE_COMPUTE, usage);
			}
		};
#define ADD_USAGE(res, stage, usage) \
	if (sync_mode == HazardTracking) { \
		add_usage(res, stage, usage); \
	}

		// Ensure the argument buffer exists for this set as some shader pipelines may
		// have been generated with argument buffers enabled.
		for (uint32_t i = 0; i < p_uniforms.size(); i += 1) {
			const BoundUniform &uniform = p_uniforms[i];
			const UniformInfo &ui = shader_set.uniforms[i];
			const UniformInfo::Indexes &idx = ui.arg_buffer;
			if (shader_set.buffer_sizes.resource_count != 0 &&
					(uniform.type == UNIFORM_TYPE_STORAGE_BUFFER || uniform.type == UNIFORM_TYPE_UNIFORM_BUFFER || uniform.is_dynamic())) {
				const BufferInfo *buffer = (const BufferInfo *)uniform.ids[0].id;
				if (!shader_set.buffer_sizes.write_length(ptr, idx.buffer, buffer->logical_size)) {
					memdelete(set);
					ERR_FAIL_V_MSG(UniformSetID(), "Metal descriptor byte length exceeds the size-table ABI.");
				}
			}

			switch (uniform.type) {
				case UNIFORM_TYPE_SAMPLER: {
					size_t count = uniform.ids.size();
					for (size_t j = 0; j < count; j += 1) {
						MTL::SamplerState *sampler = reinterpret_cast<MTL::SamplerState *>(uniform.ids[j].id);
						*(MTL::ResourceID *)(ptr + idx.sampler + j) = sampler->gpuResourceID();
					}
				} break;
				case UNIFORM_TYPE_SAMPLER_WITH_TEXTURE: {
					uint32_t count = uniform.ids.size() / 2;
					for (uint32_t j = 0; j < count; j += 1) {
						MTL::SamplerState *sampler = reinterpret_cast<MTL::SamplerState *>(uniform.ids[j * 2 + 0].id);
						MTL::Texture *texture = ((TextureInfo *)uniform.ids[j * 2 + 1].id)->texture.get();
						*(MTL::ResourceID *)(ptr + idx.texture + j) = texture->gpuResourceID();
						*(MTL::ResourceID *)(ptr + idx.sampler + j) = sampler->gpuResourceID();

						ADD_USAGE(texture, ui.active_stages, ui.usage);
					}
				} break;
				case UNIFORM_TYPE_TEXTURE: {
					size_t count = uniform.ids.size();
					for (size_t j = 0; j < count; j += 1) {
						MTL::Texture *texture = ((TextureInfo *)uniform.ids[j].id)->texture.get();
						*(MTL::ResourceID *)(ptr + idx.texture + j) = texture->gpuResourceID();

						ADD_USAGE(texture, ui.active_stages, ui.usage);
					}
				} break;
				case UNIFORM_TYPE_IMAGE: {
					size_t count = uniform.ids.size();
					for (size_t j = 0; j < count; j += 1) {
						MTL::Texture *texture = ((TextureInfo *)uniform.ids[j].id)->texture.get();
						*(MTL::ResourceID *)(ptr + idx.texture + j) = texture->gpuResourceID();
						ADD_USAGE(texture, ui.active_stages, ui.usage);

						if (idx.buffer != UINT32_MAX) {
							// Emulated atomic image access.
							MTL::Texture *parent = texture->parentTexture();
							MTL::Buffer *buffer = (parent ? parent : texture)->buffer();
							*(MTLGPUAddress *)(ptr + idx.buffer + j) = buffer->gpuAddress();

							ADD_USAGE(buffer, ui.active_stages, ui.usage);
						}
					}
				} break;
				case UNIFORM_TYPE_TEXTURE_BUFFER: {
					ERR_PRINT("not implemented: UNIFORM_TYPE_TEXTURE_BUFFER");
				} break;
				case UNIFORM_TYPE_SAMPLER_WITH_TEXTURE_BUFFER: {
					ERR_PRINT("not implemented: UNIFORM_TYPE_SAMPLER_WITH_TEXTURE_BUFFER");
				} break;
				case UNIFORM_TYPE_IMAGE_BUFFER: {
					CRASH_NOW_MSG("not implemented: UNIFORM_TYPE_IMAGE_BUFFER");
				} break;
				case UNIFORM_TYPE_STORAGE_BUFFER:
				case UNIFORM_TYPE_UNIFORM_BUFFER: {
					const BufferInfo *buffer = (const BufferInfo *)uniform.ids[0].id;
					*(MTLGPUAddress *)(ptr + idx.buffer) = buffer->buffer.get()->gpuAddress();

					ADD_USAGE(buffer->buffer.get(), ui.active_stages, ui.usage);
				} break;
				case UNIFORM_TYPE_INPUT_ATTACHMENT: {
					size_t count = uniform.ids.size();
					for (size_t j = 0; j < count; j += 1) {
						MTL::Texture *texture = ((TextureInfo *)uniform.ids[j].id)->texture.get();
						*(MTL::ResourceID *)(ptr + idx.texture + j) = texture->gpuResourceID();

						ADD_USAGE(texture, ui.active_stages, ui.usage);
					}
				} break;
				case UNIFORM_TYPE_UNIFORM_BUFFER_DYNAMIC:
				case UNIFORM_TYPE_STORAGE_BUFFER_DYNAMIC: {
					// Encode the base GPU address (frame 0); it will be updated at bind time.
					const MetalBufferDynamicInfo *buffer = (const MetalBufferDynamicInfo *)uniform.ids[0].id;
					*(MTLGPUAddress *)(ptr + idx.buffer) = buffer->buffer.get()->gpuAddress();

					ADD_USAGE(buffer->buffer.get(), ui.active_stages, ui.usage);
				} break;
				default: {
					DEV_ASSERT(false);
				}
			}
		}

#undef ADD_USAGE

		if (sync_mode == HazardTracking) {
			for (const KeyValue<MTL::Resource *, StageResourceUsage> &keyval : bound_resources) {
				ResourceVector *resources = set->usage_to_resources.getptr(keyval.value);
				if (resources == nullptr) {
					resources = &set->usage_to_resources.insert(keyval.value, ResourceVector())->value;
				}
				int64_t pos = resources->span().bisect(keyval.key, true);
				if (pos == resources->size() || (*resources)[pos] != keyval.key) {
					resources->insert(pos, keyval.key);
				}
			}
		}

		if (!is_dynamic) {
			const bool unified_shared = device->hasUnifiedMemory();
			set->arg_buffer = allocator->new_buffer(shader_set.buffer_size, base_hazard_tracking | (unified_shared ? MTL::ResourceStorageModeShared : MTL::ResourceStorageModePrivate));
			if (!set->arg_buffer.buffer) {
				memdelete(set);
				ERR_FAIL_V_MSG(UniformSetID(), "Failed to allocate Metal argument buffer.");
			}
			if (shader_set.buffer_sizes.resource_count != 0 &&
					!shader_set.buffer_sizes.write_address(arg_buffer_data.ptrw(), set->arg_buffer.buffer->gpuAddress())) {
				allocator->free_buffer(set->arg_buffer);
				memdelete(set);
				ERR_FAIL_V_MSG(UniformSetID(), "Metal buffer-size table address overflow.");
			}
#if DEV_ENABLED
			char label[64];
			snprintf(label, sizeof(label), "Uniform Set %u", p_set_index);
			set->arg_buffer.buffer->setLabel(NS::String::string(label, NS::UTF8StringEncoding));
#endif
			if (unified_shared) {
				memcpy(set->arg_buffer.buffer->contents(), arg_buffer_data.ptr(), arg_buffer_data.size());
			} else {
				const Error copy_error = _copy_queue_copy_to_buffer(arg_buffer_data, set->arg_buffer.buffer.get());
				if (copy_error != OK) {
					// Refusal happens before this destination is encoded. Any earlier
					// flushed prefix remains in the original Frame completion reports.
					last_uniform_set_create_error = copy_error;
					allocator->free_buffer(set->arg_buffer);
					memdelete(set);
					return UniformSetID();
				}
			}
		} else {
			// Store the arg buffer data for dynamic uniform sets.
			// It will be copied and updated at bind time.
			set->arg_buffer_data = arg_buffer_data;
		}

		GODOT_CLANG_WARNING_POP
	}
	Vector<BoundUniform> bound_uniforms;
	bound_uniforms.resize(p_uniforms.size());
	for (uint32_t i = 0; i < p_uniforms.size(); i += 1) {
		bound_uniforms.write[i] = p_uniforms[i];
	}
	set->uniforms = bound_uniforms;

	return UniformSetID(set);
}

void RenderingDeviceDriverMetal::uniform_set_free(UniformSetID p_uniform_set) {
	MDUniformSet *obj = (MDUniformSet *)p_uniform_set.id;
	allocator->free_buffer(obj->arg_buffer);
	memdelete(obj);
}

uint32_t RenderingDeviceDriverMetal::uniform_sets_get_dynamic_offsets(VectorView<UniformSetID> p_uniform_sets, ShaderID p_shader, uint32_t p_first_set_index, uint32_t p_set_count) const {
	const MDShader *shader = (const MDShader *)p_shader.id;
	const DynamicOffsetLayout layout = shader->dynamic_offset_layout;

	if (layout.is_empty()) {
		return 0u;
	}

	uint32_t mask = 0u;

	for (uint32_t i = 0; i < p_set_count; i++) {
		const uint32_t index = p_first_set_index + i;
		uint32_t shift = layout.get_offset_index_shift(index);
		const uint32_t count = layout.get_count(index);
		DEV_ASSERT(shader->sets[index].dynamic_uniforms.size() == count);
		if (count == 0) {
			continue;
		}

		const MDUniformSet *usi = (const MDUniformSet *)p_uniform_sets[i].id;
		for (uint32_t uniform_index : shader->sets[index].dynamic_uniforms) {
			const RDD::BoundUniform &uniform = usi->uniforms[uniform_index];
			DEV_ASSERT(uniform.is_dynamic());
			const MetalBufferDynamicInfo *buf_info = (const MetalBufferDynamicInfo *)uniform.ids[0].id;
			mask |= buf_info->frame_index() << shift;
			shift += 4u;
		}
	}

	return mask;
}

void RenderingDeviceDriverMetal::command_uniform_set_prepare_for_use(CommandBufferID p_cmd_buffer, UniformSetID p_uniform_set, ShaderID p_shader, uint32_t p_set_index) {
}

#pragma mark - Transfer

void RenderingDeviceDriverMetal::command_clear_buffer(CommandBufferID p_cmd_buffer, BufferID p_buffer, uint64_t p_offset, uint64_t p_size) {
	MDCommandBufferBase *cmd = (MDCommandBufferBase *)(p_cmd_buffer.id);
	cmd->clear_buffer(p_buffer, p_offset, p_size);
}

void RenderingDeviceDriverMetal::command_copy_buffer(CommandBufferID p_cmd_buffer, BufferID p_src_buffer, BufferID p_dst_buffer, VectorView<BufferCopyRegion> p_regions) {
	MDCommandBufferBase *cmd = (MDCommandBufferBase *)(p_cmd_buffer.id);
	cmd->copy_buffer(p_src_buffer, p_dst_buffer, p_regions);
}

void RenderingDeviceDriverMetal::command_copy_texture(CommandBufferID p_cmd_buffer, TextureID p_src_texture, TextureLayout p_src_texture_layout, TextureID p_dst_texture, TextureLayout p_dst_texture_layout, VectorView<TextureCopyRegion> p_regions) {
	MDCommandBufferBase *cmd = (MDCommandBufferBase *)(p_cmd_buffer.id);
	cmd->copy_texture(p_src_texture, p_dst_texture, p_regions);
}

void RenderingDeviceDriverMetal::command_resolve_texture(CommandBufferID p_cmd_buffer, TextureID p_src_texture, TextureLayout p_src_texture_layout, uint32_t p_src_layer, uint32_t p_src_mipmap, TextureID p_dst_texture, TextureLayout p_dst_texture_layout, uint32_t p_dst_layer, uint32_t p_dst_mipmap) {
	MDCommandBufferBase *cb = (MDCommandBufferBase *)(p_cmd_buffer.id);
	cb->resolve_texture(p_src_texture, p_src_texture_layout, p_src_layer, p_src_mipmap, p_dst_texture, p_dst_texture_layout, p_dst_layer, p_dst_mipmap);
}

void RenderingDeviceDriverMetal::command_clear_color_texture(CommandBufferID p_cmd_buffer, TextureID p_texture, TextureLayout p_texture_layout, const Color &p_color, const TextureSubresourceRange &p_subresources) {
	MDCommandBufferBase *cb = (MDCommandBufferBase *)(p_cmd_buffer.id);
	cb->clear_color_texture(p_texture, p_texture_layout, p_color, p_subresources);
}

void RenderingDeviceDriverMetal::command_clear_depth_stencil_texture(CommandBufferID p_cmd_buffer, TextureID p_texture, TextureLayout p_texture_layout, float p_depth, uint8_t p_stencil, const TextureSubresourceRange &p_subresources) {
	MDCommandBufferBase *cb = (MDCommandBufferBase *)(p_cmd_buffer.id);
	cb->clear_depth_stencil_texture(p_texture, p_texture_layout, p_depth, p_stencil, p_subresources);
}

void RenderingDeviceDriverMetal::command_copy_buffer_to_texture(CommandBufferID p_cmd_buffer, BufferID p_src_buffer, TextureID p_dst_texture, TextureLayout p_dst_texture_layout, VectorView<BufferTextureCopyRegion> p_regions) {
	MDCommandBufferBase *cmd = (MDCommandBufferBase *)(p_cmd_buffer.id);
	cmd->copy_buffer_to_texture(p_src_buffer, p_dst_texture, p_regions);
}

void RenderingDeviceDriverMetal::command_copy_texture_to_buffer(CommandBufferID p_cmd_buffer, TextureID p_src_texture, TextureLayout p_src_texture_layout, BufferID p_dst_buffer, VectorView<BufferTextureCopyRegion> p_regions) {
	MDCommandBufferBase *cmd = (MDCommandBufferBase *)(p_cmd_buffer.id);
	cmd->copy_texture_to_buffer(p_src_texture, p_dst_buffer, p_regions);
}

#pragma mark - Pipeline

void RenderingDeviceDriverMetal::pipeline_free(PipelineID p_pipeline_id) {
	NS::SharedPtr<NS::AutoreleasePool> pool = NS::TransferPtr(NS::AutoreleasePool::alloc()->init());
	MDPipeline *obj = (MDPipeline *)(p_pipeline_id.id);
	delete obj;
}

// ----- BINDING -----

void RenderingDeviceDriverMetal::command_bind_push_constants(CommandBufferID p_cmd_buffer, ShaderID p_shader, uint32_t p_dst_first_index, VectorView<uint32_t> p_data) {
	MDCommandBufferBase *cb = (MDCommandBufferBase *)(p_cmd_buffer.id);
	cb->encode_push_constant_data(p_shader, p_data);
}

// ----- CACHE -----

String RenderingDeviceDriverMetal::_pipeline_get_cache_path() const {
	String path = OS::get_singleton()->get_user_data_dir() + "/metal/pipelines";
	path += "." + context_device.name.validate_filename().replace_char(' ', '_').to_lower();
	if (Engine::get_singleton()->is_editor_hint()) {
		path += ".editor";
	}
	path += ".cache";

	return path;
}

String RenderingDeviceDriverMetal::_pipeline_get_compute_cache_path() const {
	String path = OS::get_singleton()->get_user_data_dir() + "/metal/compute_pipelines";
	path += "." + context_device.name.validate_filename().replace_char(' ', '_').to_lower();
	if (Engine::get_singleton()->is_editor_hint()) {
		path += ".editor";
	}
	path += ".cache";

	return path;
}

struct MetalArchiveReleaseJob {
	RenderingDeviceDriverMetal *driver = nullptr;
	NS::SharedPtr<MTL::BinaryArchive> archive;
	String cache_path;
	uint32_t archive_count = 0;
};

NS::SharedPtr<MTL::BinaryArchive> RenderingDeviceDriverMetal::_acquire_pipeline_archive() {
	if (!archive_active.load(std::memory_order_acquire)) {
		return nullptr;
	}
	MutexLock lock(archive_mutex);
	if (archive) {
		archive_activity_this_frame = true;
	}
	return archive;
}

bool RenderingDeviceDriverMetal::_can_add_to_pipeline_archive(MTL::BinaryArchive *p_archive, bool p_is_compute) {
	MutexLock lock(archive_mutex);
	if (archive.get() != p_archive) {
		return false;
	}
	if (pipeline_cache_mode == PipelineCacheMode::RELEASE_AFTER_WARMUP) {
		if (archive_added_pipelines >= ARCHIVE_WARMUP_MAX_PIPELINES) {
			return false;
		}
		if (p_is_compute && archive_added_compute_pipelines >= ARCHIVE_WARMUP_MAX_COMPUTE_PIPELINES) {
			return false;
		}
	}
	return true;
}

void RenderingDeviceDriverMetal::_record_pipeline_archive_addition(MTL::BinaryArchive *p_archive, bool p_is_compute) {
	MutexLock lock(archive_mutex);
	if (archive.get() == p_archive) {
		archive_count += 1;
		archive_added_pipelines += 1;
		if (p_is_compute) {
			archive_added_compute_pipelines += 1;
		}
		archive_dirty = true;
	}
}

void RenderingDeviceDriverMetal::_serialize_archive_to_path(MTL::BinaryArchive *p_archive, const String &p_cache_path, uint32_t p_archive_count) {
	if (!p_archive || p_cache_path.is_empty()) {
		return;
	}
	const bool enforce_cap = _is_archive_cap_enforced();
	if (enforce_cap && p_archive_count > ARCHIVE_MAX_ENTRIES) {
		WARN_PRINT(vformat("Metal binary archive serialize skipped (%d entries exceeds %d cap).", p_archive_count, ARCHIVE_MAX_ENTRIES));
		return;
	}
	std::unique_lock<std::shared_mutex> serialize_lock(archive_serialize_mutex);
	++archive_serialize_calls;
	NS::SharedPtr<NS::AutoreleasePool> pool = NS::TransferPtr(NS::AutoreleasePool::alloc()->init());
	DirAccess::make_dir_recursive_absolute(p_cache_path.get_base_dir());
	String tmp_path = p_cache_path + ".tmp";
	CharString tmp_utf8 = tmp_path.utf8();
	NS::String *ns_tmp = NS::String::string(tmp_utf8.get_data(), NS::UTF8StringEncoding);
	NS::URL *target = NS::URL::fileURLWithPath(ns_tmp);
	NS::Error *error = nullptr;
	if (p_archive->serializeToURL(target, &error)) {
		if (enforce_cap) {
			const int64_t serialized_size = FileAccess::get_size(tmp_path);
			if (serialized_size >= 0 && static_cast<uint64_t>(serialized_size) > archive_max_bytes) {
				WARN_PRINT(vformat("Metal binary archive serialize skipped ('%s' is %.1f MiB, exceeds %s MiB cap).",
						p_cache_path, serialized_size / (1024.0 * 1024.0), String::num_uint64(archive_max_bytes / (1024ull * 1024ull))));
				DirAccess::remove_absolute(tmp_path);
				if (FileAccess::exists(p_cache_path)) {
					const int64_t existing_size = FileAccess::get_size(p_cache_path);
					if (existing_size >= 0 && static_cast<uint64_t>(existing_size) > archive_max_bytes) {
						DirAccess::remove_absolute(p_cache_path);
					}
				}
				return;
			}
		}
		DirAccess::rename_absolute(tmp_path, p_cache_path);
	} else {
		if (error != nullptr) {
			print_verbose(String("Metal binary archive serialize skipped: ") + error->localizedDescription()->utf8String());
		}
		DirAccess::remove_absolute(tmp_path);
	}
}

void RenderingDeviceDriverMetal::_serialize_compute_archive_to_path(MTL::BinaryArchive *p_archive, const String &p_cache_path) {
	if (!p_archive || p_cache_path.is_empty()) {
		return;
	}
	std::unique_lock<std::shared_mutex> serialize_lock(compute_archive_serialize_mutex);
	NS::SharedPtr<NS::AutoreleasePool> pool = NS::TransferPtr(NS::AutoreleasePool::alloc()->init());
	DirAccess::make_dir_recursive_absolute(p_cache_path.get_base_dir());
	String tmp_path = p_cache_path + ".tmp";
	CharString tmp_utf8 = tmp_path.utf8();
	NS::String *ns_tmp = NS::String::string(tmp_utf8.get_data(), NS::UTF8StringEncoding);
	NS::URL *target = NS::URL::fileURLWithPath(ns_tmp);
	NS::Error *error = nullptr;
	if (p_archive->serializeToURL(target, &error)) {
		DirAccess::rename_absolute(tmp_path, p_cache_path);
	} else if (error != nullptr) {
		print_verbose(String("Metal compute binary archive serialize skipped: ") + error->localizedDescription()->utf8String());
	}
}

void RenderingDeviceDriverMetal::_flush_compute_archive_task_func(void *p_userdata) {
	MetalArchiveReleaseJob *job = static_cast<MetalArchiveReleaseJob *>(p_userdata);
	{
		NS::SharedPtr<NS::AutoreleasePool> pool = NS::TransferPtr(NS::AutoreleasePool::alloc()->init());
		if (job->archive) {
			if (!job->driver->archive_tearing_down.load(std::memory_order_acquire)) {
				job->driver->_serialize_compute_archive_to_path(job->archive.get(), job->cache_path);
			}
			job->archive.reset();
		}
	}
	memdelete(job);
}

void RenderingDeviceDriverMetal::_wait_compute_archive_flush_task() {
	if (compute_archive_flush_task != WorkerThreadPool::INVALID_TASK_ID) {
		if (WorkerThreadPool::get_singleton() != nullptr) {
			WorkerThreadPool::get_singleton()->wait_for_task_completion(compute_archive_flush_task);
		}
		compute_archive_flush_task = WorkerThreadPool::INVALID_TASK_ID;
	}
}

NS::SharedPtr<MTL::BinaryArchive> RenderingDeviceDriverMetal::_ensure_compute_archive(bool p_enable_disk) {
	if (pipeline_cache_mode == PipelineCacheMode::DISABLED || archive_tearing_down.load(std::memory_order_acquire) || device == nullptr) {
		return nullptr;
	}
	if (pipeline_cache_mode == PipelineCacheMode::RELEASE_AFTER_WARMUP && !p_enable_disk &&
			compute_archive_released_after_warmup.load(std::memory_order_acquire)) {
		return nullptr;
	}
	{
		MutexLock lock(compute_archive_mutex);
		if (pipeline_cache_mode == PipelineCacheMode::RELEASE_AFTER_WARMUP && !p_enable_disk &&
				compute_archive_released_after_warmup.load(std::memory_order_acquire)) {
			return nullptr;
		}
		if (compute_archive && (!p_enable_disk || compute_archive_disk_backed)) {
			compute_archive_activity_this_frame = true;
			return compute_archive;
		}
	}

	NS::SharedPtr<NS::AutoreleasePool> pool = NS::TransferPtr(NS::AutoreleasePool::alloc()->init());
	NS::SharedPtr<MTL::BinaryArchiveDescriptor> desc = NS::TransferPtr(MTL::BinaryArchiveDescriptor::alloc()->init());
	NS::SharedPtr<MTL::BinaryArchive> loaded_archive;
	NS::Error *error = nullptr;
	bool loaded_from_disk = false;

	if (p_enable_disk && OS::get_singleton() != nullptr) {
		String cache_path = _pipeline_get_compute_cache_path();
		if (FileAccess::exists(cache_path)) {
			bool skip_oversized = false;
			Ref<FileAccess> existing = FileAccess::open(cache_path, FileAccess::READ);
			if (existing.is_valid() && existing->get_length() > COMPUTE_ARCHIVE_MAX_DISK_BYTES) {
				print_verbose(vformat("Metal compute binary archive on disk (%d MiB) exceeds cap; starting fresh archive.", int(existing->get_length() / (1024 * 1024))));
				skip_oversized = true;
			}
			if (!skip_oversized) {
				CharString path_utf8 = cache_path.utf8();
				NS::String *ns_path = NS::String::string(path_utf8.get_data(), NS::UTF8StringEncoding);
				desc->setUrl(NS::URL::fileURLWithPath(ns_path));
				loaded_archive = NS::TransferPtr(device->newBinaryArchive(desc.get(), &error));
				if (loaded_archive && error == nullptr) {
					loaded_from_disk = true;
				} else {
					loaded_archive = nullptr;
					error = nullptr;
					desc->setUrl(nullptr);
				}
			}
		}
	}
	if (!loaded_archive) {
		loaded_archive = NS::TransferPtr(device->newBinaryArchive(desc.get(), &error));
	}
	if (!loaded_archive) {
		return nullptr;
	}

	NS::SharedPtr<MTL::BinaryArchive> replaced;
	{
		MutexLock lock(compute_archive_mutex);
		if (archive_tearing_down.load(std::memory_order_acquire)) {
			return nullptr;
		}
		if (pipeline_cache_mode == PipelineCacheMode::RELEASE_AFTER_WARMUP && !p_enable_disk &&
				compute_archive_released_after_warmup.load(std::memory_order_acquire)) {
			return nullptr;
		}
		if (compute_archive && (!p_enable_disk || compute_archive_disk_backed)) {
			compute_archive_activity_this_frame = true;
			return compute_archive;
		}
		replaced = std::move(compute_archive);
		compute_archive = loaded_archive;
		compute_archive_known_keys.clear();
		compute_archive_added_pipelines = 0;
		compute_archive_idle_frames = 0;
		compute_archive_open_frames = 0;
		compute_archive_activity_this_frame = !p_enable_disk;
		compute_archive_dirty = false;
		compute_archive_loaded_from_disk = loaded_from_disk;
		compute_archive_disk_backed = p_enable_disk;
		compute_archive_released_after_warmup.store(false, std::memory_order_release);
		compute_archive_active.store(true, std::memory_order_release);
	}
	replaced.reset();
	return loaded_archive;
}

void RenderingDeviceDriverMetal::_maybe_flush_compute_archive() {
	if (pipeline_cache_mode == PipelineCacheMode::DISABLED || archive_tearing_down.load(std::memory_order_acquire)) {
		return;
	}
	if (pipeline_cache_mode == PipelineCacheMode::RELEASE_AFTER_WARMUP) {
		if (!compute_archive_active.load(std::memory_order_acquire)) {
			return;
		}
		NS::SharedPtr<MTL::BinaryArchive> to_release;
		bool need_serialize = false;
		{
			MutexLock lock(compute_archive_mutex);
			if (!compute_archive) {
				compute_archive_active.store(false, std::memory_order_release);
				return;
			}
			++compute_archive_open_frames;
			if (compute_archive_activity_this_frame) {
				compute_archive_activity_this_frame = false;
				compute_archive_idle_frames = 0;
			} else {
				++compute_archive_idle_frames;
			}
			const bool idle_ready = compute_archive_idle_frames >= COMPUTE_ARCHIVE_FLUSH_IDLE_FRAMES;
			const bool ceiling_reached = compute_archive_open_frames >= COMPUTE_ARCHIVE_WARMUP_MAX_FRAMES;
			const bool pipeline_cap_reached = compute_archive_added_pipelines >= COMPUTE_ARCHIVE_MAX_PIPELINES;
			if (!idle_ready && !ceiling_reached && !pipeline_cap_reached) {
				return;
			}
			to_release = std::move(compute_archive);
			need_serialize = compute_archive_disk_backed && compute_archive_dirty;
			compute_archive_known_keys.clear();
			compute_archive_added_pipelines = 0;
			compute_archive_idle_frames = 0;
			compute_archive_open_frames = 0;
			compute_archive_activity_this_frame = false;
			compute_archive_dirty = false;
			compute_archive_loaded_from_disk = false;
			compute_archive_disk_backed = false;
			compute_archive_released_after_warmup.store(true, std::memory_order_release);
			compute_archive_active.store(false, std::memory_order_release);
		}
		if (!to_release) {
			return;
		}
		print_verbose("Metal: releasing compute binary pipeline archive after warmup to reclaim host memory.");
		if (need_serialize && !archive_tearing_down.load(std::memory_order_acquire)) {
			String cache_path = _pipeline_get_compute_cache_path();
			if (WorkerThreadPool *wtp = WorkerThreadPool::get_singleton()) {
				_wait_compute_archive_flush_task();
				MetalArchiveReleaseJob *job = memnew(MetalArchiveReleaseJob);
				job->driver = this;
				job->archive = std::move(to_release);
				job->cache_path = cache_path;
				compute_archive_flush_task = wtp->add_native_task(&_flush_compute_archive_task_func, job, false, "MetalComputeArchiveRelease");
				return;
			}
			_serialize_compute_archive_to_path(to_release.get(), cache_path);
		}
		NS::SharedPtr<NS::AutoreleasePool> pool = NS::TransferPtr(NS::AutoreleasePool::alloc()->init());
		to_release.reset();
		return;
	}
	NS::SharedPtr<MTL::BinaryArchive> to_flush;
	{
		MutexLock lock(compute_archive_mutex);
		if (!compute_archive || !compute_archive_disk_backed || !compute_archive_dirty) {
			return;
		}
		++compute_archive_idle_frames;
		if (compute_archive_idle_frames < COMPUTE_ARCHIVE_FLUSH_IDLE_FRAMES) {
			return;
		}
		to_flush = compute_archive;
		compute_archive_dirty = false;
		compute_archive_idle_frames = 0;
	}
	if (!to_flush) {
		return;
	}
	String cache_path = _pipeline_get_compute_cache_path();
	if (WorkerThreadPool *wtp = WorkerThreadPool::get_singleton()) {
		if (compute_archive_flush_task != WorkerThreadPool::INVALID_TASK_ID) {
			if (!wtp->is_task_completed(compute_archive_flush_task)) {
				MutexLock lock(compute_archive_mutex);
				compute_archive_dirty = true;
				return;
			}
			wtp->wait_for_task_completion(compute_archive_flush_task);
			compute_archive_flush_task = WorkerThreadPool::INVALID_TASK_ID;
		}
		MetalArchiveReleaseJob *job = memnew(MetalArchiveReleaseJob);
		job->driver = this;
		job->archive = std::move(to_flush);
		job->cache_path = cache_path;
		compute_archive_flush_task = wtp->add_native_task(&_flush_compute_archive_task_func, job, false, "MetalComputeArchiveFlush");
		return;
	}
	_serialize_compute_archive_to_path(to_flush.get(), cache_path);
}

void RenderingDeviceDriverMetal::_release_archive_task_func(void *p_userdata) {
	MetalArchiveReleaseJob *job = static_cast<MetalArchiveReleaseJob *>(p_userdata);
	{
		NS::SharedPtr<NS::AutoreleasePool> pool = NS::TransferPtr(NS::AutoreleasePool::alloc()->init());
		if (job->archive) {
			if (!job->driver->archive_tearing_down.load(std::memory_order_acquire)) {
				job->driver->_serialize_archive_to_path(job->archive.get(), job->cache_path, job->archive_count);
			}
			job->archive.reset();
		}
	}
	memdelete(job);
}

void RenderingDeviceDriverMetal::_wait_archive_release_task() {
	if (archive_release_task != WorkerThreadPool::INVALID_TASK_ID) {
		if (WorkerThreadPool::get_singleton() != nullptr) {
			WorkerThreadPool::get_singleton()->wait_for_task_completion(archive_release_task);
		}
		archive_release_task = WorkerThreadPool::INVALID_TASK_ID;
	}
}

void RenderingDeviceDriverMetal::_maybe_release_pipeline_archive(uint32_t p_frames_drawn) {
	if (pipeline_cache_mode != PipelineCacheMode::RELEASE_AFTER_WARMUP ||
			!archive_active.load(std::memory_order_acquire)) {
		return;
	}

	NS::SharedPtr<MTL::BinaryArchive> to_release;
	bool need_serialize = false;
	uint32_t current_archive_count = 0;
	{
		MutexLock lock(archive_mutex);
		if (!archive) {
			archive_active.store(false, std::memory_order_release);
			return;
		}
		++archive_open_frames;
		if (archive_activity_this_frame) {
			archive_activity_this_frame = false;
			archive_idle_frames = 0;
		} else {
			++archive_idle_frames;
		}
		const bool idle_ready = archive_idle_frames >= archive_warmup_idle_frames && p_frames_drawn >= archive_warmup_min_frames;
		const bool ceiling_reached = archive_open_frames >= ARCHIVE_WARMUP_MAX_FRAMES;
		const bool pipeline_cap_reached = archive_added_pipelines >= ARCHIVE_WARMUP_MAX_PIPELINES;
		if (!idle_ready && !ceiling_reached && !pipeline_cap_reached) {
			return;
		}
		to_release = std::move(archive);
		need_serialize = archive_dirty;
		current_archive_count = archive_count;
		archive_count = 0;
		archive_added_pipelines = 0;
		archive_added_compute_pipelines = 0;
		archive_dirty = false;
		archive_activity_this_frame = false;
		archive_idle_frames = 0;
		archive_open_frames = 0;
		archive_active.store(false, std::memory_order_release);
	}

	if (!to_release) {
		return;
	}

	print_verbose("Metal: releasing binary pipeline archive after warmup to reclaim host memory.");
	if (need_serialize && !archive_tearing_down.load(std::memory_order_acquire)) {
		String cache_path = _pipeline_get_cache_path();
		if (WorkerThreadPool *wtp = WorkerThreadPool::get_singleton()) {
			_wait_archive_release_task();
			MetalArchiveReleaseJob *job = memnew(MetalArchiveReleaseJob);
			job->driver = this;
			job->archive = std::move(to_release);
			job->cache_path = cache_path;
			job->archive_count = current_archive_count;
			archive_release_task = wtp->add_native_task(&_release_archive_task_func, job, false, "MetalArchiveRelease");
			return;
		}
		_serialize_archive_to_path(to_release.get(), cache_path, current_archive_count);
	}

	NS::SharedPtr<NS::AutoreleasePool> pool = NS::TransferPtr(NS::AutoreleasePool::alloc()->init());
	to_release.reset();
}

bool RenderingDeviceDriverMetal::pipeline_cache_create(const Vector<uint8_t> &p_data) {
	if (pipeline_cache_mode == PipelineCacheMode::DISABLED) {
		return false;
	}
	_wait_archive_release_task();
	_wait_compute_archive_flush_task();
	archive_tearing_down.store(false, std::memory_order_release);
	compute_archive_released_after_warmup.store(false, std::memory_order_release);

	NS::SharedPtr<NS::AutoreleasePool> pool = NS::TransferPtr(NS::AutoreleasePool::alloc()->init());
	String cache_path = _pipeline_get_cache_path();
	DirAccess::make_dir_recursive_absolute(cache_path.get_base_dir());

	NS::SharedPtr<MTL::BinaryArchiveDescriptor> desc = NS::TransferPtr(MTL::BinaryArchiveDescriptor::alloc()->init());
	NS::SharedPtr<MTL::BinaryArchive> loaded_archive;
	NS::Error *error = nullptr;
	if (FileAccess::exists(cache_path)) {
		const int64_t cache_size = FileAccess::get_size(cache_path);
		if (_is_archive_cap_enforced() && cache_size >= 0 && static_cast<uint64_t>(cache_size) > archive_max_bytes) {
			WARN_PRINT(vformat("Metal binary archive '%s' is %.1f MiB (exceeds %s MiB cap); purging oversized cache and starting fresh.",
					cache_path, cache_size / (1024.0 * 1024.0), String::num_uint64(archive_max_bytes / (1024ull * 1024ull))));
			DirAccess::remove_absolute(cache_path);
			DirAccess::remove_absolute(cache_path + ".tmp");
		} else {
			CharString path_utf8 = cache_path.utf8();
			NS::String *ns_path = NS::String::string(path_utf8.get_data(), NS::UTF8StringEncoding);
			desc->setUrl(NS::URL::fileURLWithPath(ns_path));
			loaded_archive = NS::TransferPtr(device->newBinaryArchive(desc.get(), &error));
			if (!loaded_archive || error != nullptr) {
				print_verbose(vformat("Metal binary archive load missed (%s); starting fresh archive.", cache_path));
				loaded_archive = nullptr;
				error = nullptr;
				desc->setUrl(nullptr);
			}
		}
	}
	if (!loaded_archive) {
		loaded_archive = NS::TransferPtr(device->newBinaryArchive(desc.get(), &error));
	}
	if (!loaded_archive) {
		return false;
	}
	MutexLock lock(archive_mutex);
	archive = std::move(loaded_archive);
	archive_count = 1;
	archive_added_pipelines = 0;
	archive_added_compute_pipelines = 0;
	archive_dirty = false;
	archive_activity_this_frame = false;
	archive_idle_frames = 0;
	archive_open_frames = 0;
	archive_active.store(true, std::memory_order_release);
	_ensure_compute_archive(true);
	return true;
}

void RenderingDeviceDriverMetal::pipeline_cache_free() {
	archive_tearing_down.store(true, std::memory_order_release);
	_wait_archive_release_task();
	_wait_compute_archive_flush_task();
	NS::SharedPtr<MTL::BinaryArchive> to_release;
	NS::SharedPtr<MTL::BinaryArchive> compute_to_release;
	{
		MutexLock lock(archive_mutex);
		to_release = std::move(archive);
		archive_count = 0;
		archive_added_pipelines = 0;
		archive_added_compute_pipelines = 0;
		archive_dirty = false;
		archive_activity_this_frame = false;
		archive_idle_frames = 0;
		archive_open_frames = 0;
		archive_active.store(false, std::memory_order_release);
	}
	{
		MutexLock lock(compute_archive_mutex);
		compute_to_release = std::move(compute_archive);
		compute_archive_known_keys.clear();
		compute_archive_added_pipelines = 0;
		compute_archive_idle_frames = 0;
		compute_archive_open_frames = 0;
		compute_archive_activity_this_frame = false;
		compute_archive_dirty = false;
		compute_archive_loaded_from_disk = false;
		compute_archive_disk_backed = false;
		compute_archive_released_after_warmup.store(false, std::memory_order_release);
		compute_archive_active.store(false, std::memory_order_release);
	}
	if (to_release || compute_to_release) {
		NS::SharedPtr<NS::AutoreleasePool> pool = NS::TransferPtr(NS::AutoreleasePool::alloc()->init());
		to_release.reset();
		compute_to_release.reset();
	}
}

size_t RenderingDeviceDriverMetal::pipeline_cache_query_size() {
	MutexLock lock(archive_mutex);
	return (archive_count > 0 ? archive_count : 1u) * 1024u;
}

Vector<uint8_t> RenderingDeviceDriverMetal::pipeline_cache_serialize() {
	// BUG-155B: In RELEASE_AFTER_WARMUP mode, binary archive persistence is owned
	// by background post-warmup release (_maybe_release_pipeline_archive). Running
	// MTLBinaryArchive::serializeToURL synchronously during RenderingDevice::finalize()
	// materializes the full archive on top of the live heap (+1.2 GB exit RSS spike).
	if (archive_tearing_down.load(std::memory_order_acquire) ||
			pipeline_cache_mode != PipelineCacheMode::PERSIST) {
		return Vector<uint8_t>();
	}
	NS::SharedPtr<MTL::BinaryArchive> local_archive;
	uint32_t local_archive_count = 0;
	bool was_dirty = false;
	{
		MutexLock lock(archive_mutex);
		was_dirty = archive_dirty;
		if (was_dirty) {
			local_archive = archive;
			local_archive_count = archive_count;
			archive_dirty = false;
		}
	}
	if (!local_archive || !was_dirty) {
		return Vector<uint8_t>();
	}

	_serialize_archive_to_path(local_archive.get(), _pipeline_get_cache_path(), local_archive_count);
	return Vector<uint8_t>();
}

#pragma mark - Rendering

// ----- SUBPASS -----

RDD::RenderPassID RenderingDeviceDriverMetal::render_pass_create(VectorView<Attachment> p_attachments, VectorView<Subpass> p_subpasses, VectorView<SubpassDependency> p_subpass_dependencies, uint32_t p_view_count, AttachmentReference p_fragment_density_map_attachment) {
	PixelFormats &pf = *pixel_formats;

	size_t subpass_count = p_subpasses.size();

#ifdef DEV_ENABLED
	// This loop validates the assumption that Godot configures subpass N is
	// dependent on all prior subpasses (transitively).
	for (uint32_t i = 0; i < p_subpass_dependencies.size(); i++) {
		const SubpassDependency &dep = p_subpass_dependencies[i];
		// If this asserts, then MDCommandBuffer::render_next_subpass will need
		// to be updated, as it assumes subpass N must complete before N+1 starts.
		DEV_ASSERT(dep.src_subpass < dep.dst_subpass && "unimplemented: subpass dependency not covered by the sequential subpass fence chain");
	}
#endif

	LocalVector<MDSubpass> subpasses;
	subpasses.resize(subpass_count);
	for (uint32_t i = 0; i < subpass_count; i++) {
		MDSubpass &subpass = subpasses[i];
		subpass.subpass_index = i;
		subpass.view_count = p_view_count;
		subpass.input_references = p_subpasses[i].input_references;
		subpass.color_references = p_subpasses[i].color_references;
		subpass.depth_stencil_reference = p_subpasses[i].depth_stencil_reference;
		subpass.resolve_references = p_subpasses[i].resolve_references;
		subpass.depth_resolve_reference = p_subpasses[i].depth_resolve_reference;
	}

	static const MTL::LoadAction LOAD_ACTIONS[] = {
		MTL::LoadActionLoad, // ATTACHMENT_LOAD_OP_LOAD
		MTL::LoadActionClear, // ATTACHMENT_LOAD_OP_CLEAR
		MTL::LoadActionDontCare, // ATTACHMENT_LOAD_OP_DONT_CARE
	};

	static const MTL::StoreAction STORE_ACTIONS[] = {
		MTL::StoreActionStore, // ATTACHMENT_STORE_OP_STORE
		MTL::StoreActionDontCare, // ATTACHMENT_STORE_OP_DONT_CARE
	};

	LocalVector<MDAttachment> attachments;
	attachments.resize(p_attachments.size());

	for (uint32_t i = 0; i < p_attachments.size(); i++) {
		const Attachment &a = p_attachments[i];
		MDAttachment &mda = attachments[i];
		MTL::PixelFormat format = pf.getMTLPixelFormat(a.format);
		mda.format = format;
		if (a.samples > TEXTURE_SAMPLES_1) {
			mda.samples = (*device_properties).find_nearest_supported_sample_count(a.samples);
		}
		mda.loadAction = LOAD_ACTIONS[a.load_op];
		mda.storeAction = STORE_ACTIONS[a.store_op];
		bool is_depth = pf.isDepthFormat(format);
		if (is_depth) {
			mda.type |= MDAttachmentType::Depth;
		}
		bool is_stencil = pf.isStencilFormat(format);
		if (is_stencil) {
			mda.type |= MDAttachmentType::Stencil;
			mda.stencilLoadAction = LOAD_ACTIONS[a.stencil_load_op];
			mda.stencilStoreAction = STORE_ACTIONS[a.stencil_store_op];
		}
		if (!is_depth && !is_stencil) {
			mda.type |= MDAttachmentType::Color;
		}
	}
	MDRenderPass *obj = memnew(MDRenderPass(std::move(attachments), std::move(subpasses)));
	return RenderPassID(obj);
}

void RenderingDeviceDriverMetal::render_pass_free(RenderPassID p_render_pass) {
	MDRenderPass *obj = (MDRenderPass *)(p_render_pass.id);
	if (obj) {
		memdelete(obj);
	}
}

// ----- COMMANDS -----

void RenderingDeviceDriverMetal::command_begin_render_pass(CommandBufferID p_cmd_buffer, RenderPassID p_render_pass, FramebufferID p_framebuffer, CommandBufferType p_cmd_buffer_type, const Rect2i &p_rect, VectorView<RenderPassClearValue> p_clear_values) {
	MDCommandBufferBase *cb = (MDCommandBufferBase *)(p_cmd_buffer.id);
	cb->render_begin_pass(p_render_pass, p_framebuffer, p_cmd_buffer_type, p_rect, p_clear_values);
}

void RenderingDeviceDriverMetal::command_end_render_pass(CommandBufferID p_cmd_buffer) {
	MDCommandBufferBase *cb = (MDCommandBufferBase *)(p_cmd_buffer.id);
	cb->render_end_pass();
}

void RenderingDeviceDriverMetal::command_next_render_subpass(CommandBufferID p_cmd_buffer, CommandBufferType p_cmd_buffer_type) {
	MDCommandBufferBase *cb = (MDCommandBufferBase *)(p_cmd_buffer.id);
	cb->render_next_subpass();
}

void RenderingDeviceDriverMetal::command_render_set_viewport(CommandBufferID p_cmd_buffer, VectorView<Rect2i> p_viewports) {
	MDCommandBufferBase *cb = (MDCommandBufferBase *)(p_cmd_buffer.id);
	cb->render_set_viewport(p_viewports);
}

void RenderingDeviceDriverMetal::command_render_set_scissor(CommandBufferID p_cmd_buffer, VectorView<Rect2i> p_scissors) {
	MDCommandBufferBase *cb = (MDCommandBufferBase *)(p_cmd_buffer.id);
	cb->render_set_scissor(p_scissors);
}

void RenderingDeviceDriverMetal::command_render_clear_attachments(CommandBufferID p_cmd_buffer, VectorView<AttachmentClear> p_attachment_clears, VectorView<Rect2i> p_rects) {
	MDCommandBufferBase *cb = (MDCommandBufferBase *)(p_cmd_buffer.id);
	cb->render_clear_attachments(p_attachment_clears, p_rects);
}

void RenderingDeviceDriverMetal::command_bind_render_pipeline(CommandBufferID p_cmd_buffer, PipelineID p_pipeline) {
	MDCommandBufferBase *cb = (MDCommandBufferBase *)(p_cmd_buffer.id);
	cb->bind_pipeline(p_pipeline);
}

void RenderingDeviceDriverMetal::command_bind_render_uniform_sets(CommandBufferID p_cmd_buffer, VectorView<UniformSetID> p_uniform_sets, ShaderID p_shader, uint32_t p_first_set_index, uint32_t p_set_count, uint32_t p_dynamic_offsets) {
	MDCommandBufferBase *cb = (MDCommandBufferBase *)(p_cmd_buffer.id);
	cb->render_bind_uniform_sets(p_uniform_sets, p_shader, p_first_set_index, p_set_count, p_dynamic_offsets);
}

void RenderingDeviceDriverMetal::command_render_draw(CommandBufferID p_cmd_buffer, uint32_t p_vertex_count, uint32_t p_instance_count, uint32_t p_base_vertex, uint32_t p_first_instance) {
	MDCommandBufferBase *cb = (MDCommandBufferBase *)(p_cmd_buffer.id);
	cb->render_draw(p_vertex_count, p_instance_count, p_base_vertex, p_first_instance);
}

void RenderingDeviceDriverMetal::command_render_draw_indexed(CommandBufferID p_cmd_buffer, uint32_t p_index_count, uint32_t p_instance_count, uint32_t p_first_index, int32_t p_vertex_offset, uint32_t p_first_instance) {
	MDCommandBufferBase *cb = (MDCommandBufferBase *)(p_cmd_buffer.id);
	cb->render_draw_indexed(p_index_count, p_instance_count, p_first_index, p_vertex_offset, p_first_instance);
}

void RenderingDeviceDriverMetal::command_render_draw_indexed_indirect(CommandBufferID p_cmd_buffer, BufferID p_indirect_buffer, uint64_t p_offset, uint32_t p_draw_count, uint32_t p_stride) {
	MDCommandBufferBase *cb = (MDCommandBufferBase *)(p_cmd_buffer.id);
	cb->render_draw_indexed_indirect(p_indirect_buffer, p_offset, p_draw_count, p_stride);
}

void RenderingDeviceDriverMetal::command_render_draw_indexed_indirect_count(CommandBufferID p_cmd_buffer, BufferID p_indirect_buffer, uint64_t p_offset, BufferID p_count_buffer, uint64_t p_count_buffer_offset, uint32_t p_max_draw_count, uint32_t p_stride) {
	MDCommandBufferBase *cb = (MDCommandBufferBase *)(p_cmd_buffer.id);
	cb->render_draw_indexed_indirect_count(p_indirect_buffer, p_offset, p_count_buffer, p_count_buffer_offset, p_max_draw_count, p_stride);
}

void RenderingDeviceDriverMetal::command_render_draw_indirect(CommandBufferID p_cmd_buffer, BufferID p_indirect_buffer, uint64_t p_offset, uint32_t p_draw_count, uint32_t p_stride) {
	MDCommandBufferBase *cb = (MDCommandBufferBase *)(p_cmd_buffer.id);
	cb->render_draw_indirect(p_indirect_buffer, p_offset, p_draw_count, p_stride);
}

void RenderingDeviceDriverMetal::command_render_draw_indirect_count(CommandBufferID p_cmd_buffer, BufferID p_indirect_buffer, uint64_t p_offset, BufferID p_count_buffer, uint64_t p_count_buffer_offset, uint32_t p_max_draw_count, uint32_t p_stride) {
	MDCommandBufferBase *cb = (MDCommandBufferBase *)(p_cmd_buffer.id);
	cb->render_draw_indirect_count(p_indirect_buffer, p_offset, p_count_buffer, p_count_buffer_offset, p_max_draw_count, p_stride);
}

void RenderingDeviceDriverMetal::command_render_bind_vertex_buffers(CommandBufferID p_cmd_buffer, uint32_t p_binding_count, const BufferID *p_buffers, const uint64_t *p_offsets, uint64_t p_dynamic_offsets) {
	MDCommandBufferBase *cb = (MDCommandBufferBase *)(p_cmd_buffer.id);
	cb->render_bind_vertex_buffers(p_binding_count, p_buffers, p_offsets, p_dynamic_offsets);
}

void RenderingDeviceDriverMetal::command_render_bind_index_buffer(CommandBufferID p_cmd_buffer, BufferID p_buffer, IndexBufferFormat p_format, uint64_t p_offset) {
	MDCommandBufferBase *cb = (MDCommandBufferBase *)(p_cmd_buffer.id);
	cb->render_bind_index_buffer(p_buffer, p_format, p_offset);
}

void RenderingDeviceDriverMetal::command_render_set_blend_constants(CommandBufferID p_cmd_buffer, const Color &p_constants) {
	MDCommandBufferBase *cb = (MDCommandBufferBase *)(p_cmd_buffer.id);
	cb->render_set_blend_constants(p_constants);
}

void RenderingDeviceDriverMetal::command_render_set_line_width(CommandBufferID p_cmd_buffer, float p_width) {
	if (!Math::is_equal_approx(p_width, 1.0f)) {
		ERR_FAIL_MSG("Setting line widths other than 1.0 is not supported by the Metal rendering driver.");
	}
}

// ----- PIPELINE -----

RenderingDeviceDriverMetal::Result<NS::SharedPtr<MTL::Function>> RenderingDeviceDriverMetal::_create_function(MDLibrary *p_library, NS::String *p_name, VectorView<PipelineSpecializationConstant> &p_specialization_constants) {
	NS::SharedPtr<MTL::Library> library = p_library->acquire_library();
	if (!library) {
		ERR_FAIL_V_MSG(ERR_CANT_CREATE, "Failed to compile Metal library");
	}

	MTL::Function *function = library->newFunction(p_name);
	ERR_FAIL_NULL_V_MSG(function, ERR_CANT_CREATE, "No function named main0");

	NS::Dictionary *constants_dict = function->functionConstantsDictionary();
	if (constants_dict->count() == 0) {
		return NS::TransferPtr(function);
	}

	LocalVector<MTL::FunctionConstant *> constants;
	NS::Enumerator<NS::String> *keys = constants_dict->keyEnumerator<NS::String>();
	while (NS::String *key = keys->nextObject()) {
		constants.push_back(constants_dict->object<MTL::FunctionConstant>(key));
	}

	// Check if already sorted by index.
	bool is_sorted = true;
	for (NS::UInteger i = 1; i < constants.size(); i++) {
		MTL::FunctionConstant *prev = constants[i - 1];
		MTL::FunctionConstant *curr = constants[i];
		if (prev->index() > curr->index()) {
			is_sorted = false;
			break;
		}
	}

	if (!is_sorted) {
		struct Comparator {
			bool operator()(const MTL::FunctionConstant *p, const MTL::FunctionConstant *q) const {
				return p->index() < q->index();
			}
		};

		constants.sort_custom<Comparator>();
	}

	// Build a sorted list of specialization constants by constant_id.
	uint32_t *indexes = (uint32_t *)alloca(p_specialization_constants.size() * sizeof(uint32_t));
	for (uint32_t i = 0; i < p_specialization_constants.size(); i++) {
		indexes[i] = i;
	}
	std::sort(indexes, &indexes[p_specialization_constants.size()], [&](int a, int b) {
		return p_specialization_constants[a].constant_id < p_specialization_constants[b].constant_id;
	});

	NS::SharedPtr<MTL::FunctionConstantValues> constantValues = NS::TransferPtr(MTL::FunctionConstantValues::alloc()->init());

	// Merge the sorted constants from the function with the sorted user constants.
	NS::UInteger i = 0;
	uint32_t j = 0;
	while (i < constants.size() && j < p_specialization_constants.size()) {
		MTL::FunctionConstant *curr = (MTL::FunctionConstant *)constants[i];
		const PipelineSpecializationConstant &sc = p_specialization_constants[indexes[j]];
		if (curr->index() == sc.constant_id) {
			switch (curr->type()) {
				case MTL::DataTypeBool:
				case MTL::DataTypeFloat:
				case MTL::DataTypeInt:
				case MTL::DataTypeUInt: {
					constantValues->setConstantValue(&sc.int_value, curr->type(), sc.constant_id);
				} break;
				default:
					ERR_FAIL_V_MSG(NS::TransferPtr(function), "Invalid specialization constant type");
			}
			i++;
			j++;
		} else if (curr->index() < sc.constant_id) {
			i++;
		} else {
			j++;
		}
	}

	// Handle R32UI_ALIGNMENT_CONSTANT_ID if present.
	if (i < constants.size()) {
		MTL::FunctionConstant *curr = constants[i];
		if (curr->index() == R32UI_ALIGNMENT_CONSTANT_ID) {
			uint32_t alignment = 16; // TODO(sgc): is this always correct?
			constantValues->setConstantValue(&alignment, curr->type(), curr->index());
			i++;
		}
	}

	NS::Error *err = nullptr;
	function->release();
	function = library->newFunction(p_name, constantValues.get(), &err);
	ERR_FAIL_NULL_V_MSG(function, ERR_CANT_CREATE, String("specialized function failed: ") + (err ? err->localizedDescription()->utf8String() : "unknown error"));

	return NS::TransferPtr(function);
}

// RDD::PolygonCullMode == MTL::CullMode.
static_assert(ENUM_MEMBERS_EQUAL(RDD::POLYGON_CULL_DISABLED, MTL::CullModeNone));
static_assert(ENUM_MEMBERS_EQUAL(RDD::POLYGON_CULL_FRONT, MTL::CullModeFront));
static_assert(ENUM_MEMBERS_EQUAL(RDD::POLYGON_CULL_BACK, MTL::CullModeBack));

// RDD::StencilOperation == MTL::StencilOperation.
static_assert(ENUM_MEMBERS_EQUAL(RDD::STENCIL_OP_KEEP, MTL::StencilOperationKeep));
static_assert(ENUM_MEMBERS_EQUAL(RDD::STENCIL_OP_ZERO, MTL::StencilOperationZero));
static_assert(ENUM_MEMBERS_EQUAL(RDD::STENCIL_OP_REPLACE, MTL::StencilOperationReplace));
static_assert(ENUM_MEMBERS_EQUAL(RDD::STENCIL_OP_INCREMENT_AND_CLAMP, MTL::StencilOperationIncrementClamp));
static_assert(ENUM_MEMBERS_EQUAL(RDD::STENCIL_OP_DECREMENT_AND_CLAMP, MTL::StencilOperationDecrementClamp));
static_assert(ENUM_MEMBERS_EQUAL(RDD::STENCIL_OP_INVERT, MTL::StencilOperationInvert));
static_assert(ENUM_MEMBERS_EQUAL(RDD::STENCIL_OP_INCREMENT_AND_WRAP, MTL::StencilOperationIncrementWrap));
static_assert(ENUM_MEMBERS_EQUAL(RDD::STENCIL_OP_DECREMENT_AND_WRAP, MTL::StencilOperationDecrementWrap));

// RDD::BlendOperation == MTL::BlendOperation.
static_assert(ENUM_MEMBERS_EQUAL(RDD::BLEND_OP_ADD, MTL::BlendOperationAdd));
static_assert(ENUM_MEMBERS_EQUAL(RDD::BLEND_OP_SUBTRACT, MTL::BlendOperationSubtract));
static_assert(ENUM_MEMBERS_EQUAL(RDD::BLEND_OP_REVERSE_SUBTRACT, MTL::BlendOperationReverseSubtract));
static_assert(ENUM_MEMBERS_EQUAL(RDD::BLEND_OP_MINIMUM, MTL::BlendOperationMin));
static_assert(ENUM_MEMBERS_EQUAL(RDD::BLEND_OP_MAXIMUM, MTL::BlendOperationMax));

std::atomic<uint64_t> RenderingDeviceDriverMetal::total_render_pso_cache_hits{ 0 };
std::atomic<uint64_t> RenderingDeviceDriverMetal::total_render_pso_cache_misses{ 0 };
std::atomic<uint64_t> RenderingDeviceDriverMetal::total_compute_pso_cache_hits{ 0 };
std::atomic<uint64_t> RenderingDeviceDriverMetal::total_compute_pso_cache_misses{ 0 };
std::atomic<uint64_t> RenderingDeviceDriverMetal::total_compute_pso_cache_evictions{ 0 };
std::atomic<uint64_t> RenderingDeviceDriverMetal::total_compute_libraries_released{ 0 };
std::atomic<uint64_t> RenderingDeviceDriverMetal::total_render_libraries_released{ 0 };
std::atomic<uint64_t> RenderingDeviceDriverMetal::total_lazy_libraries_unloaded{ 0 };
std::atomic<uint64_t> RenderingDeviceDriverMetal::total_compute_archive_hits{ 0 };
std::atomic<uint64_t> RenderingDeviceDriverMetal::total_compute_archive_additions{ 0 };

static _FORCE_INLINE_ uint32_t _metal_float_bits(float p_val) {
	uint32_t bits = 0;
	memcpy(&bits, &p_val, sizeof(bits));
	return bits;
}

static _FORCE_INLINE_ uint32_t _metal_spec_const_bits(const RDD::PipelineSpecializationConstant &p_sc) {
	switch (p_sc.type) {
		case RDD::PIPELINE_SPECIALIZATION_CONSTANT_TYPE_BOOL:
			return p_sc.bool_value ? 1u : 0u;
		case RDD::PIPELINE_SPECIALIZATION_CONSTANT_TYPE_FLOAT:
			return _metal_float_bits(p_sc.float_value);
		case RDD::PIPELINE_SPECIALIZATION_CONSTANT_TYPE_INT:
		default:
			return p_sc.int_value;
	}
}

bool RenderingDeviceDriverMetal::RenderPipelineCacheKey::operator==(const RenderPipelineCacheKey &p_rhs) const {
	if (cached_hash != p_rhs.cached_hash ||
			shader_id != p_rhs.shader_id ||
			vertex_format_id != p_rhs.vertex_format_id ||
			render_primitive != p_rhs.render_primitive ||
			dynamic_state != p_rhs.dynamic_state ||
			color_format_count != p_rhs.color_format_count ||
			depth_format != p_rhs.depth_format ||
			stencil_format != p_rhs.stencil_format ||
			raster_flags != p_rhs.raster_flags ||
			cull_mode != p_rhs.cull_mode ||
			front_face != p_rhs.front_face ||
			depth_bias_constant_bits != p_rhs.depth_bias_constant_bits ||
			depth_bias_slope_bits != p_rhs.depth_bias_slope_bits ||
			depth_bias_clamp_bits != p_rhs.depth_bias_clamp_bits ||
			patch_control_points != p_rhs.patch_control_points ||
			sample_count != p_rhs.sample_count ||
			msaa_flags != p_rhs.msaa_flags ||
			depth_stencil_flags != p_rhs.depth_stencil_flags ||
			depth_compare_op != p_rhs.depth_compare_op ||
			front_fail != p_rhs.front_fail ||
			front_pass != p_rhs.front_pass ||
			front_depth_fail != p_rhs.front_depth_fail ||
			front_compare != p_rhs.front_compare ||
			front_compare_mask != p_rhs.front_compare_mask ||
			front_write_mask != p_rhs.front_write_mask ||
			front_reference != p_rhs.front_reference ||
			back_fail != p_rhs.back_fail ||
			back_pass != p_rhs.back_pass ||
			back_depth_fail != p_rhs.back_depth_fail ||
			back_compare != p_rhs.back_compare ||
			back_compare_mask != p_rhs.back_compare_mask ||
			back_write_mask != p_rhs.back_write_mask ||
			back_reference != p_rhs.back_reference ||
			memcmp(blend_const_bits, p_rhs.blend_const_bits, sizeof(blend_const_bits)) != 0 ||
			blend_attachments.size() != p_rhs.blend_attachments.size() ||
			spec_constants.size() != p_rhs.spec_constants.size()) {
		return false;
	}
	for (uint32_t i = 0; i < color_format_count; ++i) {
		if (color_formats[i] != p_rhs.color_formats[i]) {
			return false;
		}
	}
	for (uint32_t i = 0; i < blend_attachments.size(); ++i) {
		if (!(blend_attachments[i] == p_rhs.blend_attachments[i])) {
			return false;
		}
	}
	for (uint32_t i = 0; i < spec_constants.size(); ++i) {
		if (!(spec_constants[i] == p_rhs.spec_constants[i])) {
			return false;
		}
	}
	return true;
}

bool RenderingDeviceDriverMetal::ComputePipelineCacheKey::operator==(const ComputePipelineCacheKey &p_rhs) const {
	if (cached_hash != p_rhs.cached_hash ||
			memcmp(library_key.data, p_rhs.library_key.data, sizeof(library_key.data)) != 0 ||
			uses_argument_buffers != p_rhs.uses_argument_buffers ||
			argument_buffer_count != p_rhs.argument_buffer_count ||
			dynamic_buffer_mask != p_rhs.dynamic_buffer_mask ||
			spec_constants.size() != p_rhs.spec_constants.size()) {
		return false;
	}
	for (uint32_t i = 0; i < spec_constants.size(); ++i) {
		if (!(spec_constants[i] == p_rhs.spec_constants[i])) {
			return false;
		}
	}
	return true;
}

RDD::PipelineID RenderingDeviceDriverMetal::render_pipeline_create(
		ShaderID p_shader,
		VertexFormatID p_vertex_format,
		RenderPrimitive p_render_primitive,
		PipelineRasterizationState p_rasterization_state,
		PipelineMultisampleState p_multisample_state,
		PipelineDepthStencilState p_depth_stencil_state,
		PipelineColorBlendState p_blend_state,
		VectorView<int32_t> p_color_attachments,
		BitField<PipelineDynamicStateFlags> p_dynamic_state,
		RenderPassID p_render_pass,
		uint32_t p_render_subpass,
		VectorView<PipelineSpecializationConstant> p_specialization_constants) {
	MDRenderShader *shader = (MDRenderShader *)(p_shader.id);
	MTL::VertexDescriptor *vert_desc = reinterpret_cast<MTL::VertexDescriptor *>(p_vertex_format.id);
	MDRenderPass *pass = (MDRenderPass *)(p_render_pass.id);

	// Build immutable cache key from all pipeline-defining inputs so repeated
	// render_pipeline_create calls reuse compiled Metal PSO + DepthStencilState.
	RenderPipelineCacheKey cache_key;
	cache_key.shader_id = p_shader.id;
	cache_key.vertex_format_id = p_vertex_format.id;
	cache_key.render_primitive = uint32_t(p_render_primitive);
	cache_key.dynamic_state = uint32_t(p_dynamic_state);
	{
		const MDSubpass &subpass = pass->subpasses[p_render_subpass];
		cache_key.color_format_count = MIN<uint32_t>(subpass.color_references.size(), MAX_COLOR_ATTACHMENT_COUNT);
		for (uint32_t i = 0; i < cache_key.color_format_count; ++i) {
			const uint32_t attachment = subpass.color_references[i].attachment;
			if (attachment != AttachmentReference::UNUSED) {
				cache_key.color_formats[i] = uint16_t(pass->attachments[attachment].format);
			}
		}
		if (subpass.depth_stencil_reference.attachment != AttachmentReference::UNUSED) {
			const MDAttachment &a = pass->attachments[subpass.depth_stencil_reference.attachment];
			if (a.type & MDAttachmentType::Depth) {
				cache_key.depth_format = uint16_t(a.format);
			}
			if (a.type & MDAttachmentType::Stencil) {
				cache_key.stencil_format = uint16_t(a.format);
			}
		}
	}
	cache_key.raster_flags =
			(p_rasterization_state.discard_primitives ? 1u : 0u) |
			(p_rasterization_state.enable_depth_clamp ? 2u : 0u) |
			(p_rasterization_state.wireframe ? 4u : 0u) |
			(p_rasterization_state.depth_bias_enabled ? 8u : 0u);
	cache_key.cull_mode = uint32_t(p_rasterization_state.cull_mode);
	cache_key.front_face = uint32_t(p_rasterization_state.front_face);
	cache_key.depth_bias_constant_bits = _metal_float_bits(p_rasterization_state.depth_bias_constant_factor);
	cache_key.depth_bias_slope_bits = _metal_float_bits(p_rasterization_state.depth_bias_slope_factor);
	cache_key.depth_bias_clamp_bits = _metal_float_bits(p_rasterization_state.depth_bias_clamp);
	cache_key.patch_control_points = p_rasterization_state.patch_control_points;
	{
		SampleCount resolved_samples = SampleCount1;
		if (p_multisample_state.sample_count > TEXTURE_SAMPLES_1) {
			resolved_samples = (*device_properties).find_nearest_supported_sample_count(p_multisample_state.sample_count);
		}
		cache_key.sample_count = uint32_t(resolved_samples);
	}
	cache_key.msaa_flags =
			(p_multisample_state.enable_alpha_to_coverage ? 1u : 0u) |
			(p_multisample_state.enable_alpha_to_one ? 2u : 0u);
	{
		const bool key_depth_enabled = p_depth_stencil_state.enable_depth_test && cache_key.depth_format != uint16_t(MTL::PixelFormatInvalid);
		const bool key_stencil_enabled = p_depth_stencil_state.enable_stencil && cache_key.stencil_format != uint16_t(MTL::PixelFormatInvalid);
		cache_key.depth_stencil_flags =
				(key_depth_enabled ? 1u : 0u) |
				(key_stencil_enabled ? 2u : 0u) |
				(p_depth_stencil_state.enable_depth_write ? 4u : 0u);
		if (key_depth_enabled || key_stencil_enabled) {
			cache_key.depth_compare_op = uint32_t(p_depth_stencil_state.depth_compare_operator);
		}
		if (key_stencil_enabled) {
			cache_key.front_fail = uint32_t(p_depth_stencil_state.front_op.fail);
			cache_key.front_pass = uint32_t(p_depth_stencil_state.front_op.pass);
			cache_key.front_depth_fail = uint32_t(p_depth_stencil_state.front_op.depth_fail);
			cache_key.front_compare = uint32_t(p_depth_stencil_state.front_op.compare);
			cache_key.front_compare_mask = p_depth_stencil_state.front_op.compare_mask;
			cache_key.front_write_mask = p_depth_stencil_state.front_op.write_mask;
			cache_key.front_reference = p_depth_stencil_state.front_op.reference;
			cache_key.back_fail = uint32_t(p_depth_stencil_state.back_op.fail);
			cache_key.back_pass = uint32_t(p_depth_stencil_state.back_op.pass);
			cache_key.back_depth_fail = uint32_t(p_depth_stencil_state.back_op.depth_fail);
			cache_key.back_compare = uint32_t(p_depth_stencil_state.back_op.compare);
			cache_key.back_compare_mask = p_depth_stencil_state.back_op.compare_mask;
			cache_key.back_write_mask = p_depth_stencil_state.back_op.write_mask;
			cache_key.back_reference = p_depth_stencil_state.back_op.reference;
		}
	}
	cache_key.blend_const_bits[0] = _metal_float_bits(p_blend_state.blend_constant.r);
	cache_key.blend_const_bits[1] = _metal_float_bits(p_blend_state.blend_constant.g);
	cache_key.blend_const_bits[2] = _metal_float_bits(p_blend_state.blend_constant.b);
	cache_key.blend_const_bits[3] = _metal_float_bits(p_blend_state.blend_constant.a);
	cache_key.blend_attachments.reserve(p_color_attachments.size());
	for (uint32_t i = 0; i < p_color_attachments.size(); ++i) {
		if (p_color_attachments[i] == ATTACHMENT_UNUSED) {
			continue;
		}
		const PipelineColorBlendState::Attachment &bs = p_blend_state.attachments[i];
		RenderPipelineBlendAttachmentKey bk;
		bk.slot = p_color_attachments[i];
		bk.enable_blend = bs.enable_blend ? 1u : 0u;
		bk.src_color = uint8_t(bs.src_color_blend_factor);
		bk.dst_color = uint8_t(bs.dst_color_blend_factor);
		bk.color_op = uint8_t(bs.color_blend_op);
		bk.src_alpha = uint8_t(bs.src_alpha_blend_factor);
		bk.dst_alpha = uint8_t(bs.dst_alpha_blend_factor);
		bk.alpha_op = uint8_t(bs.alpha_blend_op);
		bk.write_mask = (bs.write_r ? 1u : 0u) | (bs.write_g ? 2u : 0u) | (bs.write_b ? 4u : 0u) | (bs.write_a ? 8u : 0u);
		cache_key.blend_attachments.push_back(bk);
	}
	cache_key.spec_constants.reserve(p_specialization_constants.size());
	for (uint32_t i = 0; i < p_specialization_constants.size(); ++i) {
		PipelineSpecConstKey sk;
		sk.constant_id = p_specialization_constants[i].constant_id;
		sk.type = uint32_t(p_specialization_constants[i].type);
		sk.bits = _metal_spec_const_bits(p_specialization_constants[i]);
		cache_key.spec_constants.push_back(sk);
	}
	{
		uint32_t h = hash_murmur3_one_64(cache_key.shader_id);
		h = hash_murmur3_one_64(cache_key.vertex_format_id, h);
		h = hash_murmur3_one_32(cache_key.render_primitive, h);
		h = hash_murmur3_one_32(cache_key.dynamic_state, h);
		h = hash_murmur3_one_32(cache_key.color_format_count, h);
		if (cache_key.color_format_count > 0) {
			h = hash_murmur3_buffer(cache_key.color_formats, cache_key.color_format_count * sizeof(cache_key.color_formats[0]), h);
		}
		h = hash_murmur3_one_32((uint32_t(cache_key.depth_format) << 16) | cache_key.stencil_format, h);
		h = hash_murmur3_one_32(cache_key.raster_flags, h);
		h = hash_murmur3_one_32((cache_key.cull_mode << 8) | cache_key.front_face, h);
		h = hash_murmur3_one_32(cache_key.depth_bias_constant_bits, h);
		h = hash_murmur3_one_32(cache_key.depth_bias_slope_bits, h);
		h = hash_murmur3_one_32(cache_key.depth_bias_clamp_bits, h);
		h = hash_murmur3_one_32(cache_key.patch_control_points, h);
		h = hash_murmur3_one_32(cache_key.sample_count, h);
		h = hash_murmur3_one_32(cache_key.msaa_flags, h);
		h = hash_murmur3_one_32(cache_key.depth_stencil_flags, h);
		h = hash_murmur3_one_32(cache_key.depth_compare_op, h);
		if (cache_key.depth_stencil_flags & 2u) {
			h = hash_murmur3_one_32(cache_key.front_fail, h);
			h = hash_murmur3_one_32(cache_key.front_pass, h);
			h = hash_murmur3_one_32(cache_key.front_depth_fail, h);
			h = hash_murmur3_one_32(cache_key.front_compare, h);
			h = hash_murmur3_one_32(cache_key.front_compare_mask, h);
			h = hash_murmur3_one_32(cache_key.front_write_mask, h);
			h = hash_murmur3_one_32(cache_key.front_reference, h);
			h = hash_murmur3_one_32(cache_key.back_fail, h);
			h = hash_murmur3_one_32(cache_key.back_pass, h);
			h = hash_murmur3_one_32(cache_key.back_depth_fail, h);
			h = hash_murmur3_one_32(cache_key.back_compare, h);
			h = hash_murmur3_one_32(cache_key.back_compare_mask, h);
			h = hash_murmur3_one_32(cache_key.back_write_mask, h);
			h = hash_murmur3_one_32(cache_key.back_reference, h);
		}
		h = hash_murmur3_buffer(cache_key.blend_const_bits, sizeof(cache_key.blend_const_bits), h);
		for (const RenderPipelineBlendAttachmentKey &bk : cache_key.blend_attachments) {
			h = hash_murmur3_one_32(uint32_t(bk.slot), h);
			h = hash_murmur3_one_32(uint32_t(bk.enable_blend) | (uint32_t(bk.src_color) << 8) | (uint32_t(bk.dst_color) << 16) | (uint32_t(bk.color_op) << 24), h);
			h = hash_murmur3_one_32(uint32_t(bk.src_alpha) | (uint32_t(bk.dst_alpha) << 8) | (uint32_t(bk.alpha_op) << 16) | (uint32_t(bk.write_mask) << 24), h);
		}
		for (const PipelineSpecConstKey &sk : cache_key.spec_constants) {
			h = hash_murmur3_one_32(sk.constant_id, h);
			h = hash_murmur3_one_32(sk.type, h);
			h = hash_murmur3_one_32(sk.bits, h);
		}
		cache_key.cached_hash = hash_fmix32(h);
	}
	{
		MutexLock lock(pipeline_state_cache_mutex);
		if (const RenderPipelineCacheEntry *cached = render_pso_cache.getptr(cache_key)) {
			MDRenderPipeline *pipeline = new MDRenderPipeline();
			pipeline->state = cached->state;
			pipeline->depth_stencil = cached->depth_stencil;
			pipeline->sample_count = cached->sample_count;
			pipeline->raster_state = cached->raster_state;
			pipeline->shader = shader;
			render_pso_cache_hits.fetch_add(1, std::memory_order_relaxed);
			total_render_pso_cache_hits.fetch_add(1, std::memory_order_relaxed);
			return PipelineID(pipeline);
		}
	}

	os_signpost_id_t reflect_id = os_signpost_id_make_with_pointer(LOG_INTERVALS, shader);
	os_signpost_interval_begin(LOG_INTERVALS, reflect_id, "render_pipeline_create", "shader_name=%{public}s", shader->name.get_data());
	DEFER([=]() {
		os_signpost_interval_end(LOG_INTERVALS, reflect_id, "render_pipeline_create");
	});

	os_signpost_event_emit(LOG_DRIVER, OS_SIGNPOST_ID_EXCLUSIVE, "create_pipeline");

	NS::SharedPtr<NS::AutoreleasePool> pool = NS::TransferPtr(NS::AutoreleasePool::alloc()->init());
	NS::SharedPtr<MTL::RenderPipelineDescriptor> desc = NS::TransferPtr(MTL::RenderPipelineDescriptor::alloc()->init());

	{
		const MDSubpass &subpass = pass->subpasses[p_render_subpass];
		for (uint32_t i = 0; i < subpass.color_references.size(); i++) {
			uint32_t attachment = subpass.color_references[i].attachment;
			if (attachment != AttachmentReference::UNUSED) {
				const MDAttachment &a = pass->attachments[attachment];
				desc->colorAttachments()->object(i)->setPixelFormat(a.format);
			}
		}

		if (subpass.depth_stencil_reference.attachment != AttachmentReference::UNUSED) {
			uint32_t attachment = subpass.depth_stencil_reference.attachment;
			const MDAttachment &a = pass->attachments[attachment];

			if (a.type & MDAttachmentType::Depth) {
				desc->setDepthAttachmentPixelFormat(a.format);
			}

			if (a.type & MDAttachmentType::Stencil) {
				desc->setStencilAttachmentPixelFormat(a.format);
			}
		}
	}

	desc->setVertexDescriptor(vert_desc);
	desc->setLabel(conv::to_nsstring(shader->name));

	if (shader->uses_argument_buffers) {
		// Set mutability of argument buffers.
		for (uint32_t i = 0; i < shader->sets.size(); i++) {
			const UniformSet &set = shader->sets[i];
			const MTL::Mutability mutability = set.dynamic_uniforms.is_empty() ? MTL::MutabilityImmutable : MTL::MutabilityMutable;
			desc->vertexBuffers()->object(i)->setMutability(mutability);
			desc->fragmentBuffers()->object(i)->setMutability(mutability);
		}
	}

	// Input assembly & tessellation.

	std::unique_ptr<MDRenderPipeline> pipeline = std::make_unique<MDRenderPipeline>();

	switch (p_render_primitive) {
		case RENDER_PRIMITIVE_POINTS:
			desc->setInputPrimitiveTopology(MTL::PrimitiveTopologyClassPoint);
			break;
		case RENDER_PRIMITIVE_LINES:
		case RENDER_PRIMITIVE_LINES_WITH_ADJACENCY:
		case RENDER_PRIMITIVE_LINESTRIPS_WITH_ADJACENCY:
		case RENDER_PRIMITIVE_LINESTRIPS:
			desc->setInputPrimitiveTopology(MTL::PrimitiveTopologyClassLine);
			break;
		case RENDER_PRIMITIVE_TRIANGLES:
		case RENDER_PRIMITIVE_TRIANGLE_STRIPS:
		case RENDER_PRIMITIVE_TRIANGLES_WITH_ADJACENCY:
		case RENDER_PRIMITIVE_TRIANGLE_STRIPS_WITH_AJACENCY:
		case RENDER_PRIMITIVE_TRIANGLE_STRIPS_WITH_RESTART_INDEX:
			desc->setInputPrimitiveTopology(MTL::PrimitiveTopologyClassTriangle);
			break;
		case RENDER_PRIMITIVE_TESSELATION_PATCH:
			desc->setMaxTessellationFactor(p_rasterization_state.patch_control_points);
			desc->setTessellationPartitionMode(MTL::TessellationPartitionModeInteger);
			ERR_FAIL_V_MSG(PipelineID(), "tessellation not implemented");
			break;
		case RENDER_PRIMITIVE_MAX:
		default:
			desc->setInputPrimitiveTopology(MTL::PrimitiveTopologyClassUnspecified);
			break;
	}

	switch (p_render_primitive) {
		case RENDER_PRIMITIVE_POINTS:
			pipeline->raster_state.render_primitive = MTL::PrimitiveTypePoint;
			break;
		case RENDER_PRIMITIVE_LINES:
		case RENDER_PRIMITIVE_LINES_WITH_ADJACENCY:
			pipeline->raster_state.render_primitive = MTL::PrimitiveTypeLine;
			break;
		case RENDER_PRIMITIVE_LINESTRIPS:
		case RENDER_PRIMITIVE_LINESTRIPS_WITH_ADJACENCY:
			pipeline->raster_state.render_primitive = MTL::PrimitiveTypeLineStrip;
			break;
		case RENDER_PRIMITIVE_TRIANGLES:
		case RENDER_PRIMITIVE_TRIANGLES_WITH_ADJACENCY:
			pipeline->raster_state.render_primitive = MTL::PrimitiveTypeTriangle;
			break;
		case RENDER_PRIMITIVE_TRIANGLE_STRIPS:
		case RENDER_PRIMITIVE_TRIANGLE_STRIPS_WITH_AJACENCY:
		case RENDER_PRIMITIVE_TRIANGLE_STRIPS_WITH_RESTART_INDEX:
			pipeline->raster_state.render_primitive = MTL::PrimitiveTypeTriangleStrip;
			break;
		default:
			break;
	}

	// Rasterization.
	desc->setRasterizationEnabled(!p_rasterization_state.discard_primitives);
	pipeline->raster_state.clip_mode = p_rasterization_state.enable_depth_clamp ? MTL::DepthClipModeClamp : MTL::DepthClipModeClip;
	pipeline->raster_state.fill_mode = p_rasterization_state.wireframe ? MTL::TriangleFillModeLines : MTL::TriangleFillModeFill;

	static const MTL::CullMode CULL_MODE[3] = {
		MTL::CullModeNone,
		MTL::CullModeFront,
		MTL::CullModeBack,
	};
	pipeline->raster_state.cull_mode = CULL_MODE[p_rasterization_state.cull_mode];
	pipeline->raster_state.winding = (p_rasterization_state.front_face == POLYGON_FRONT_FACE_CLOCKWISE) ? MTL::WindingClockwise : MTL::WindingCounterClockwise;
	pipeline->raster_state.depth_bias.enabled = p_rasterization_state.depth_bias_enabled;
	pipeline->raster_state.depth_bias.depth_bias = p_rasterization_state.depth_bias_constant_factor;
	pipeline->raster_state.depth_bias.slope_scale = p_rasterization_state.depth_bias_slope_factor;
	pipeline->raster_state.depth_bias.clamp = p_rasterization_state.depth_bias_clamp;
	// In Metal there is no line width.
	if (!Math::is_equal_approx(p_rasterization_state.line_width, 1.0f)) {
		WARN_PRINT("unsupported: line width");
	}

	// Multisample.
	if (p_multisample_state.enable_sample_shading) {
		WARN_PRINT("unsupported: multi-sample shading");
	}

	if (p_multisample_state.sample_count > TEXTURE_SAMPLES_1) {
		pipeline->sample_count = (*device_properties).find_nearest_supported_sample_count(p_multisample_state.sample_count);
	}
	desc->setRasterSampleCount(static_cast<NS::UInteger>(pipeline->sample_count));
	desc->setAlphaToCoverageEnabled(p_multisample_state.enable_alpha_to_coverage);
	desc->setAlphaToOneEnabled(p_multisample_state.enable_alpha_to_one);

	// Depth buffer.
	bool depth_enabled = p_depth_stencil_state.enable_depth_test && desc->depthAttachmentPixelFormat() != MTL::PixelFormatInvalid;
	bool stencil_enabled = p_depth_stencil_state.enable_stencil && desc->stencilAttachmentPixelFormat() != MTL::PixelFormatInvalid;

	if (depth_enabled || stencil_enabled) {
		NS::SharedPtr<MTL::DepthStencilDescriptor> ds_desc = NS::TransferPtr(MTL::DepthStencilDescriptor::alloc()->init());

		pipeline->raster_state.depth_test.enabled = depth_enabled;
		ds_desc->setDepthWriteEnabled(p_depth_stencil_state.enable_depth_write);
		ds_desc->setDepthCompareFunction(COMPARE_OPERATORS[p_depth_stencil_state.depth_compare_operator]);
		if (p_depth_stencil_state.enable_depth_range) {
			WARN_PRINT("unsupported: depth range");
		}

		if (stencil_enabled) {
			pipeline->raster_state.stencil.enabled = true;
			pipeline->raster_state.stencil.front_reference = p_depth_stencil_state.front_op.reference;
			pipeline->raster_state.stencil.back_reference = p_depth_stencil_state.back_op.reference;

			{
				// Front.
				NS::SharedPtr<MTL::StencilDescriptor> sd = NS::TransferPtr(MTL::StencilDescriptor::alloc()->init());
				sd->setStencilFailureOperation(STENCIL_OPERATIONS[p_depth_stencil_state.front_op.fail]);
				sd->setDepthStencilPassOperation(STENCIL_OPERATIONS[p_depth_stencil_state.front_op.pass]);
				sd->setDepthFailureOperation(STENCIL_OPERATIONS[p_depth_stencil_state.front_op.depth_fail]);
				sd->setStencilCompareFunction(COMPARE_OPERATORS[p_depth_stencil_state.front_op.compare]);
				sd->setReadMask(p_depth_stencil_state.front_op.compare_mask);
				sd->setWriteMask(p_depth_stencil_state.front_op.write_mask);
				ds_desc->setFrontFaceStencil(sd.get());
			}
			{
				// Back.
				NS::SharedPtr<MTL::StencilDescriptor> sd = NS::TransferPtr(MTL::StencilDescriptor::alloc()->init());
				sd->setStencilFailureOperation(STENCIL_OPERATIONS[p_depth_stencil_state.back_op.fail]);
				sd->setDepthStencilPassOperation(STENCIL_OPERATIONS[p_depth_stencil_state.back_op.pass]);
				sd->setDepthFailureOperation(STENCIL_OPERATIONS[p_depth_stencil_state.back_op.depth_fail]);
				sd->setStencilCompareFunction(COMPARE_OPERATORS[p_depth_stencil_state.back_op.compare]);
				sd->setReadMask(p_depth_stencil_state.back_op.compare_mask);
				sd->setWriteMask(p_depth_stencil_state.back_op.write_mask);
				ds_desc->setBackFaceStencil(sd.get());
			}
		}

		pipeline->depth_stencil = NS::TransferPtr(device->newDepthStencilState(ds_desc.get()));
		ERR_FAIL_COND_V_MSG(!pipeline->depth_stencil, PipelineID(), "Failed to create depth stencil state");
	} else {
		// TODO(sgc): FB13671991 raised as Apple docs state calling setDepthStencilState:nil is valid, but currently generates an exception
		pipeline->depth_stencil = NS::RetainPtr(get_resource_cache().get_depth_stencil_state(false, false));
	}

	// Blend state.
	{
		for (uint32_t i = 0; i < p_color_attachments.size(); i++) {
			if (p_color_attachments[i] == ATTACHMENT_UNUSED) {
				continue;
			}

			const PipelineColorBlendState::Attachment &bs = p_blend_state.attachments[i];

			MTL::RenderPipelineColorAttachmentDescriptor *ca_desc = desc->colorAttachments()->object(p_color_attachments[i]);
			ca_desc->setBlendingEnabled(bs.enable_blend);

			ca_desc->setSourceRGBBlendFactor(BLEND_FACTORS[bs.src_color_blend_factor]);
			ca_desc->setDestinationRGBBlendFactor(BLEND_FACTORS[bs.dst_color_blend_factor]);
			ca_desc->setRgbBlendOperation(BLEND_OPERATIONS[bs.color_blend_op]);

			ca_desc->setSourceAlphaBlendFactor(BLEND_FACTORS[bs.src_alpha_blend_factor]);
			ca_desc->setDestinationAlphaBlendFactor(BLEND_FACTORS[bs.dst_alpha_blend_factor]);
			ca_desc->setAlphaBlendOperation(BLEND_OPERATIONS[bs.alpha_blend_op]);

			MTL::ColorWriteMask writeMask = MTL::ColorWriteMaskNone;
			if (bs.write_r) {
				writeMask |= MTL::ColorWriteMaskRed;
			}
			if (bs.write_g) {
				writeMask |= MTL::ColorWriteMaskGreen;
			}
			if (bs.write_b) {
				writeMask |= MTL::ColorWriteMaskBlue;
			}
			if (bs.write_a) {
				writeMask |= MTL::ColorWriteMaskAlpha;
			}
			ca_desc->setWriteMask(writeMask);
		}

		pipeline->raster_state.blend.r = p_blend_state.blend_constant.r;
		pipeline->raster_state.blend.g = p_blend_state.blend_constant.g;
		pipeline->raster_state.blend.b = p_blend_state.blend_constant.b;
		pipeline->raster_state.blend.a = p_blend_state.blend_constant.a;
	}

	// Dynamic state.

	if (p_dynamic_state.has_flag(DYNAMIC_STATE_DEPTH_BIAS)) {
		pipeline->raster_state.depth_bias.enabled = true;
	}

	if (p_dynamic_state.has_flag(DYNAMIC_STATE_BLEND_CONSTANTS)) {
		pipeline->raster_state.blend.enabled = true;
	}

	if (p_dynamic_state.has_flag(DYNAMIC_STATE_DEPTH_BOUNDS)) {
		// TODO(sgc): ??
	}

	if (p_dynamic_state.has_flag(DYNAMIC_STATE_STENCIL_COMPARE_MASK)) {
		// TODO(sgc): ??
	}

	if (p_dynamic_state.has_flag(DYNAMIC_STATE_STENCIL_WRITE_MASK)) {
		// TODO(sgc): ??
	}

	if (p_dynamic_state.has_flag(DYNAMIC_STATE_STENCIL_REFERENCE)) {
		pipeline->raster_state.stencil.enabled = true;
	}

	std::shared_ptr<MDLibrary> local_vert;
	std::shared_ptr<MDLibrary> local_frag;
	{
		MutexLock lock(pipeline_state_cache_mutex);
		if (shader->modules_destroyed) {
			ERR_FAIL_V_MSG(PipelineID(), "Render shader modules have already been destroyed.");
		}
		local_vert = shader->vert;
		local_frag = shader->frag;
	}

	if (local_vert) {
		Result<NS::SharedPtr<MTL::Function>> function_or_err = _create_function(local_vert.get(), MTLSTR("main0"), p_specialization_constants);
		ERR_FAIL_COND_V(std::holds_alternative<Error>(function_or_err), PipelineID());
		desc->setVertexFunction(std::get<NS::SharedPtr<MTL::Function>>(function_or_err).get());
	}

	if (local_frag) {
		Result<NS::SharedPtr<MTL::Function>> function_or_err = _create_function(local_frag.get(), MTLSTR("main0"), p_specialization_constants);
		ERR_FAIL_COND_V(std::holds_alternative<Error>(function_or_err), PipelineID());
		desc->setFragmentFunction(std::get<NS::SharedPtr<MTL::Function>>(function_or_err).get());
	}

	NS::SharedPtr<MTL::BinaryArchive> local_archive = _acquire_pipeline_archive();

	MTL::PipelineOption options = MTL::PipelineOptionNone;
	MTL::BinaryArchive *arc = local_archive.get();
	NS::Error *error = nullptr;
	if (arc) {
		NS::SharedPtr<NS::Array> archives = NS::TransferPtr(NS::Array::array(reinterpret_cast<NS::Object *const *>(&arc), 1)->retain());
		desc->setBinaryArchives(archives.get());
		if (archive_fail_on_miss) {
			options |= MTL::PipelineOptionFailOnBinaryArchiveMiss;
		}
		// Shared lock allows concurrent newRenderPipelineState reads across threads;
		// exclusive lock guards addRenderPipelineFunctions & serializeToURL (BUG-189B).
		// Strict lock order: archive_serialize_mutex -> archive_mutex.
		{
			std::shared_lock<std::shared_mutex> read_lock(archive_serialize_mutex);
			pipeline->state = NS::TransferPtr(device->newRenderPipelineState(desc.get(), options, nullptr, &error));
		}
		if (error == nullptr && pipeline->state && _can_add_to_pipeline_archive(arc, false)) {
			std::unique_lock<std::shared_mutex> write_lock(archive_serialize_mutex);
			if (_can_add_to_pipeline_archive(arc, false)) {
				NS::Error *arc_err = nullptr;
				if (arc->addRenderPipelineFunctions(desc.get(), &arc_err)) {
					_record_pipeline_archive_addition(arc, false);
				} else if (arc_err != nullptr) {
					print_verbose(String("Metal binary archive addRenderPipelineFunctions skipped: ") + arc_err->localizedDescription()->utf8String());
				}
			}
		}
	} else {
		pipeline->state = NS::TransferPtr(device->newRenderPipelineState(desc.get(), options, nullptr, &error));
	}
	pipeline->shader = shader;

	ERR_FAIL_COND_V_MSG(error != nullptr, PipelineID(), String("error creating pipeline: ") + error->localizedDescription()->utf8String());
	ERR_FAIL_COND_V_MSG(!pipeline->state, PipelineID(), "Failed to create render pipeline state");

	render_pso_cache_misses.fetch_add(1, std::memory_order_relaxed);
	total_render_pso_cache_misses.fetch_add(1, std::memory_order_relaxed);
	{
		MutexLock lock(pipeline_state_cache_mutex);
		if (render_pso_cache.size() >= MAX_RENDER_PSO_CACHE_ENTRIES) {
			render_pso_cache.clear();
		}
		RenderPipelineCacheEntry entry;
		entry.state = pipeline->state;
		entry.depth_stencil = pipeline->depth_stencil;
		entry.sample_count = pipeline->sample_count;
		entry.raster_state = pipeline->raster_state;
		render_pso_cache.insert(std::move(cache_key), std::move(entry));
	}

	// Release MTL::Function refs on descriptor, then unload MTL::Library LLVM AIR
	// bitcode while preserving compact zstd MSL for transparent re-hydration (BUG-188B).
	desc.reset();
	if (local_vert && local_vert->release_loaded_library()) {
		total_render_libraries_released.fetch_add(1, std::memory_order_relaxed);
		total_lazy_libraries_unloaded.fetch_add(1, std::memory_order_relaxed);
	}
	if (local_frag && local_frag->release_loaded_library()) {
		total_render_libraries_released.fetch_add(1, std::memory_order_relaxed);
		total_lazy_libraries_unloaded.fetch_add(1, std::memory_order_relaxed);
	}

	return PipelineID(pipeline.release());
}

#pragma mark - Compute

// ----- COMMANDS -----

void RenderingDeviceDriverMetal::command_begin_compute_pass(CommandBufferID p_cmd_buffer) {
	MDCommandBufferBase *cb = (MDCommandBufferBase *)(p_cmd_buffer.id);
	cb->compute_begin_pass();
}

void RenderingDeviceDriverMetal::command_end_compute_pass(CommandBufferID p_cmd_buffer) {
	MDCommandBufferBase *cb = (MDCommandBufferBase *)(p_cmd_buffer.id);
	cb->compute_end_pass();
}

void RenderingDeviceDriverMetal::command_compute_memory_barrier(CommandBufferID p_cmd_buffer) {
	MDCommandBufferBase *cb = (MDCommandBufferBase *)(p_cmd_buffer.id);
	cb->compute_memory_barrier();
}

void RenderingDeviceDriverMetal::command_bind_compute_pipeline(CommandBufferID p_cmd_buffer, PipelineID p_pipeline) {
	MDCommandBufferBase *cb = (MDCommandBufferBase *)(p_cmd_buffer.id);
	cb->bind_pipeline(p_pipeline);
}

void RenderingDeviceDriverMetal::command_bind_compute_uniform_sets(CommandBufferID p_cmd_buffer, VectorView<UniformSetID> p_uniform_sets, ShaderID p_shader, uint32_t p_first_set_index, uint32_t p_set_count, uint32_t p_dynamic_offsets) {
	MDCommandBufferBase *cb = (MDCommandBufferBase *)(p_cmd_buffer.id);
	cb->compute_bind_uniform_sets(p_uniform_sets, p_shader, p_first_set_index, p_set_count, p_dynamic_offsets);
}

void RenderingDeviceDriverMetal::command_compute_dispatch(CommandBufferID p_cmd_buffer, uint32_t p_x_groups, uint32_t p_y_groups, uint32_t p_z_groups) {
	MDCommandBufferBase *cb = (MDCommandBufferBase *)(p_cmd_buffer.id);
	cb->compute_dispatch(p_x_groups, p_y_groups, p_z_groups);
}

void RenderingDeviceDriverMetal::command_compute_dispatch_indirect(CommandBufferID p_cmd_buffer, BufferID p_indirect_buffer, uint64_t p_offset) {
	MDCommandBufferBase *cb = (MDCommandBufferBase *)(p_cmd_buffer.id);
	cb->compute_dispatch_indirect(p_indirect_buffer, p_offset);
}

// ----- PIPELINE -----

RDD::PipelineID RenderingDeviceDriverMetal::compute_pipeline_create(ShaderID p_shader, VectorView<PipelineSpecializationConstant> p_specialization_constants) {
	MDComputeShader *shader = (MDComputeShader *)(p_shader.id);

	// Fast path: unspecialized compute shaders retain their compiled PSO after
	// releasing MDLibrary / MTL::Library LLVM bitcode (BUG-185A).
	std::shared_ptr<MDLibrary> local_kernel;
	{
		MutexLock lock(pipeline_state_cache_mutex);
		if (p_specialization_constants.size() == 0 && shader->cached_unspecialized_pso) {
			MDComputePipeline *pipeline = new MDComputePipeline(shader->cached_unspecialized_pso);
			pipeline->compute_state.local = shader->local;
			pipeline->shader = shader;
			compute_pso_cache_hits.fetch_add(1, std::memory_order_relaxed);
			total_compute_pso_cache_hits.fetch_add(1, std::memory_order_relaxed);
			return PipelineID(pipeline);
		}
		local_kernel = shader->kernel;
	}

	const SHA256Digest *lib_key = shader->has_library_key ? &shader->library_key : (local_kernel ? local_kernel->get_cache_key() : nullptr);
	const bool can_cache = (lib_key != nullptr) && (!shader->uses_argument_buffers || shader->sets.size() <= 64);
	ComputePipelineCacheKey cache_key;
	if (can_cache) {
		cache_key.library_key = *lib_key;
		cache_key.uses_argument_buffers = shader->uses_argument_buffers;
		if (shader->uses_argument_buffers) {
			cache_key.argument_buffer_count = shader->sets.size();
			for (uint32_t i = 0; i < shader->sets.size(); ++i) {
				if (!shader->sets[i].dynamic_uniforms.is_empty()) {
					cache_key.dynamic_buffer_mask |= (uint64_t(1) << i);
				}
			}
		}
		cache_key.spec_constants.reserve(p_specialization_constants.size());
		uint32_t h = hash_murmur3_buffer(cache_key.library_key.data, sizeof(cache_key.library_key.data));
		h = hash_murmur3_one_32(cache_key.uses_argument_buffers ? 1u : 0u, h);
		h = hash_murmur3_one_32(cache_key.argument_buffer_count, h);
		h = hash_murmur3_one_64(cache_key.dynamic_buffer_mask, h);
		for (uint32_t i = 0; i < p_specialization_constants.size(); ++i) {
			PipelineSpecConstKey sk;
			sk.constant_id = p_specialization_constants[i].constant_id;
			sk.type = uint32_t(p_specialization_constants[i].type);
			sk.bits = _metal_spec_const_bits(p_specialization_constants[i]);
			h = hash_murmur3_one_32(sk.constant_id, h);
			h = hash_murmur3_one_32(sk.type, h);
			h = hash_murmur3_one_32(sk.bits, h);
			cache_key.spec_constants.push_back(sk);
		}
		cache_key.cached_hash = hash_fmix32(h);
		NS::SharedPtr<MTL::ComputePipelineState> hit_state;
		std::shared_ptr<MDLibrary> dropped_kernel;
		{
			MutexLock lock(pipeline_state_cache_mutex);
			if (const ComputePipelineCacheEntry *cached = compute_pso_cache.getptr(cache_key)) {
				hit_state = cached->state;
				if (!shader->has_specialization_constants && p_specialization_constants.size() == 0) {
					shader->cached_unspecialized_pso = hit_state;
					dropped_kernel = std::move(shader->kernel);
				}
			}
		}
		if (hit_state) {
			if (dropped_kernel || local_kernel) {
				NS::SharedPtr<NS::AutoreleasePool> drop_pool = NS::TransferPtr(NS::AutoreleasePool::alloc()->init());
				if (dropped_kernel) {
					dropped_kernel.reset();
					total_compute_libraries_released.fetch_add(1, std::memory_order_relaxed);
				}
				local_kernel.reset();
			}
			MDComputePipeline *pipeline = new MDComputePipeline(hit_state);
			pipeline->compute_state.local = shader->local;
			pipeline->shader = shader;
			compute_pso_cache_hits.fetch_add(1, std::memory_order_relaxed);
			total_compute_pso_cache_hits.fetch_add(1, std::memory_order_relaxed);
			return PipelineID(pipeline);
		}
	}

	ERR_FAIL_NULL_V_MSG(local_kernel.get(), PipelineID(), "Compute shader module has already been destroyed.");

	os_signpost_id_t reflect_id = os_signpost_id_make_with_pointer(LOG_INTERVALS, shader);
	os_signpost_interval_begin(LOG_INTERVALS, reflect_id, "compute_pipeline_create", "shader_name=%{public}s", shader->name.get_data());
	DEFER([=]() {
		os_signpost_interval_end(LOG_INTERVALS, reflect_id, "compute_pipeline_create");
	});

	os_signpost_event_emit(LOG_DRIVER, OS_SIGNPOST_ID_EXCLUSIVE, "create_pipeline");

	NS::SharedPtr<NS::AutoreleasePool> pool = NS::TransferPtr(NS::AutoreleasePool::alloc()->init());
	Result<NS::SharedPtr<MTL::Function>> function_or_err = _create_function(local_kernel.get(), MTLSTR("main0"), p_specialization_constants);
	ERR_FAIL_COND_V(std::holds_alternative<Error>(function_or_err), PipelineID());
	NS::SharedPtr<MTL::Function> function = std::get<NS::SharedPtr<MTL::Function>>(function_or_err);

	NS::SharedPtr<MTL::ComputePipelineDescriptor> desc = NS::TransferPtr(MTL::ComputePipelineDescriptor::alloc()->init());
	desc->setComputeFunction(function.get());
	desc->setLabel(conv::to_nsstring(shader->name));

	if (shader->uses_argument_buffers) {
		// Set mutability of argument buffers.
		for (uint32_t i = 0; i < shader->sets.size(); i++) {
			const UniformSet &set = shader->sets[i];
			const MTL::Mutability mutability = set.dynamic_uniforms.is_empty() ? MTL::MutabilityImmutable : MTL::MutabilityMutable;
			desc->buffers()->object(i)->setMutability(mutability);
		}
	}

	NS::SharedPtr<MTL::BinaryArchive> local_archive = _acquire_pipeline_archive();
	NS::SharedPtr<MTL::BinaryArchive> local_compute_archive = can_cache ? _ensure_compute_archive(false) : nullptr;

	MTL::PipelineOption options = MTL::PipelineOptionNone;
	MTL::BinaryArchive *arc = local_archive.get();
	MTL::BinaryArchive *compute_arc = local_compute_archive.get();
	NS::Error *error = nullptr;
	NS::SharedPtr<MTL::ComputePipelineState> state;

	bool already_in_compute_archive = false;
	bool compute_arc_loaded_from_disk = false;
	bool compute_arc_probed_hit = false;
	if (compute_arc) {
		MutexLock lock(compute_archive_mutex);
		if (compute_archive.get() == compute_arc) {
			already_in_compute_archive = compute_archive_known_keys.has(cache_key);
			compute_arc_loaded_from_disk = compute_archive_loaded_from_disk;
		}
	}

	if (arc || compute_arc) {
		NS::Object *arc_objs[2];
		NS::UInteger arc_count = 0;
		if (compute_arc) {
			arc_objs[arc_count++] = compute_arc;
		}
		if (arc) {
			arc_objs[arc_count++] = arc;
		}
		NS::SharedPtr<NS::Array> archives = NS::TransferPtr(NS::Array::array(arc_objs, arc_count)->retain());
		desc->setBinaryArchives(archives.get());
		if (archive_fail_on_miss) {
			options |= MTL::PipelineOptionFailOnBinaryArchiveMiss;
		}
		// Shared locks allow concurrent multi-threaded Metal PSO compilation;
		// strict lock order: archive_serialize_mutex -> compute_archive_serialize_mutex -> *_mutex.
		{
			std::shared_lock<std::shared_mutex> main_read_lock(archive_serialize_mutex, std::defer_lock);
			std::shared_lock<std::shared_mutex> compute_read_lock(compute_archive_serialize_mutex, std::defer_lock);
			if (arc) {
				main_read_lock.lock();
			}
			if (compute_arc) {
				compute_read_lock.lock();
			}
			if (compute_arc && !already_in_compute_archive && compute_arc_loaded_from_disk && !archive_fail_on_miss) {
				NS::Error *probe_err = nullptr;
				state = NS::TransferPtr(device->newComputePipelineState(desc.get(), MTL::PipelineOptionFailOnBinaryArchiveMiss, nullptr, &probe_err));
				if (probe_err == nullptr && state) {
					compute_arc_probed_hit = true;
				} else {
					state.reset();
				}
			}
			if (!state) {
				state = NS::TransferPtr(device->newComputePipelineState(desc.get(), options, nullptr, &error));
			}
		}

		if (error == nullptr && state) {
			if (already_in_compute_archive || compute_arc_probed_hit) {
				total_compute_archive_hits.fetch_add(1, std::memory_order_relaxed);
				if (compute_arc_probed_hit && compute_arc) {
					MutexLock lock(compute_archive_mutex);
					if (compute_archive.get() == compute_arc) {
						compute_archive_known_keys.insert(cache_key);
					}
				}
			} else if (compute_arc && can_cache) {
				std::unique_lock<std::shared_mutex> write_lock(compute_archive_serialize_mutex);
				bool can_add_compute = false;
				{
					MutexLock lock(compute_archive_mutex);
					can_add_compute = (compute_archive.get() == compute_arc) &&
							!compute_archive_known_keys.has(cache_key) &&
							compute_archive_added_pipelines < COMPUTE_ARCHIVE_MAX_PIPELINES;
				}
				if (can_add_compute) {
					NS::Error *arc_err = nullptr;
					if (compute_arc->addComputePipelineFunctions(desc.get(), &arc_err)) {
						MutexLock lock(compute_archive_mutex);
						if (compute_archive.get() == compute_arc) {
							compute_archive_known_keys.insert(cache_key);
							++compute_archive_added_pipelines;
							compute_archive_dirty = true;
							compute_archive_idle_frames = 0;
							compute_archive_activity_this_frame = true;
							total_compute_archive_additions.fetch_add(1, std::memory_order_relaxed);
						}
					} else if (arc_err != nullptr) {
						print_verbose(String("Metal compute binary archive addComputePipelineFunctions skipped: ") + arc_err->localizedDescription()->utf8String());
					}
				}
			}
			const bool managed_by_compute_archive = (compute_arc != nullptr) ||
					(can_cache && compute_archive_released_after_warmup.load(std::memory_order_acquire));
			if (arc && !managed_by_compute_archive && _can_add_to_pipeline_archive(arc, true)) {
				std::unique_lock<std::shared_mutex> write_lock(archive_serialize_mutex);
				if (_can_add_to_pipeline_archive(arc, true)) {
					NS::Error *arc_err = nullptr;
					if (arc->addComputePipelineFunctions(desc.get(), &arc_err)) {
						_record_pipeline_archive_addition(arc, true);
					} else if (arc_err != nullptr) {
						print_verbose(String("Metal binary archive addComputePipelineFunctions skipped: ") + arc_err->localizedDescription()->utf8String());
					}
				}
			}
		}
	} else {
		state = NS::TransferPtr(device->newComputePipelineState(desc.get(), options, nullptr, &error));
	}
	ERR_FAIL_COND_V_MSG(error != nullptr, PipelineID(), String("error creating pipeline: ") + error->localizedDescription()->utf8String());
	ERR_FAIL_COND_V_MSG(!state, PipelineID(), "Failed to create compute pipeline state");

	compute_pso_cache_misses.fetch_add(1, std::memory_order_relaxed);
	total_compute_pso_cache_misses.fetch_add(1, std::memory_order_relaxed);
	std::shared_ptr<MDLibrary> dropped_kernel;
	{
		MutexLock lock(pipeline_state_cache_mutex);
		if (can_cache) {
			if (compute_pso_cache.size() >= MAX_COMPUTE_PSO_CACHE_ENTRIES) {
				compute_pso_cache.clear();
				compute_pso_keys_by_library.clear();
			}
			ComputePipelineCacheEntry entry;
			entry.state = state;
			const bool inserted_new = !compute_pso_cache.has(cache_key);
			ComputePipelineCacheKey key_copy;
			if (inserted_new) {
				key_copy = cache_key;
			}
			compute_pso_cache.insert(std::move(cache_key), std::move(entry));
			if (inserted_new) {
				compute_pso_keys_by_library[key_copy.library_key].push_back(std::move(key_copy));
			}
		}
		if (!shader->has_specialization_constants && p_specialization_constants.size() == 0) {
			shader->cached_unspecialized_pso = state;
			dropped_kernel = std::move(shader->kernel);
		}
	}
	// Drop descriptor & MTL::Function refs before releasing MDLibrary / MTL::Library LLVM bitcode.
	desc.reset();
	function.reset();
	if (dropped_kernel) {
		dropped_kernel.reset();
		total_compute_libraries_released.fetch_add(1, std::memory_order_relaxed);
	} else if (local_kernel && local_kernel->release_loaded_library()) {
		total_lazy_libraries_unloaded.fetch_add(1, std::memory_order_relaxed);
	}
	local_kernel.reset();

	MDComputePipeline *pipeline = new MDComputePipeline(state);
	pipeline->compute_state.local = shader->local;
	pipeline->shader = shader;

	return PipelineID(pipeline);
}

#pragma mark - Raytracing

// ----- ACCELERATION STRUCTURE -----

RDD::AccelerationStructureID RenderingDeviceDriverMetal::blas_create(VectorView<AccelerationStructureGeometry> p_geometries, BitField<AccelerationStructureFlagBits> p_flags) {
	ERR_FAIL_V_MSG(AccelerationStructureID(), "Ray tracing is not currently supported by the Metal driver.");
}

RDD::AccelerationStructureID RenderingDeviceDriverMetal::tlas_create(uint32_t p_max_instance_count, BitField<AccelerationStructureFlagBits> p_flags) {
	ERR_FAIL_V_MSG(AccelerationStructureID(), "Ray tracing is not currently supported by the Metal driver.");
}

void RenderingDeviceDriverMetal::acceleration_structure_instance_write(uint8_t *r_driver_instance, const AccelerationStructureInstance &p_instance) {
	ERR_FAIL_MSG("Ray tracing is not currently supported by the Metal driver.");
}

void RenderingDeviceDriverMetal::acceleration_structure_free(RDD::AccelerationStructureID p_acceleration_structure) {
	ERR_FAIL_MSG("Ray tracing is not currently supported by the Metal driver.");
}

uint32_t RenderingDeviceDriverMetal::acceleration_structure_get_scratch_size_bytes(AccelerationStructureID p_acceleration_structure) {
	ERR_FAIL_V_MSG(0, "Ray tracing is not currently supported by the Metal driver.");
}

// ----- PIPELINE -----

RDD::RaytracingPipelineID RenderingDeviceDriverMetal::raytracing_pipeline_create(VectorView<PipelineShader> p_shaders, VectorView<uint32_t> p_raygen_shader_indices, VectorView<uint32_t> p_miss_shader_indices, VectorView<HitGroup> p_hit_groups, uint32_t p_max_trace_recursion_depth, ShaderID p_layout_defining_shader) {
	ERR_FAIL_V_MSG(RaytracingPipelineID(), "Ray tracing is not currently supported by the Metal driver.");
}

void RenderingDeviceDriverMetal::raytracing_pipeline_free(RDD::RaytracingPipelineID p_pipeline) {
	ERR_FAIL_MSG("Ray tracing is not currently supported by the Metal driver.");
}

bool RenderingDeviceDriverMetal::raytracing_pipeline_get_shader_group_handles(RaytracingPipelineID p_pipeline, uint32_t p_group_index_offset, VectorView<uint32_t> p_group_indices, uint8_t *r_data, uint32_t p_data_stride_bytes) {
	ERR_FAIL_V_MSG(false, "Ray tracing is not currently supported by the Metal driver.");
}

// ----- COMMANDS -----

void RenderingDeviceDriverMetal::command_build_blas(CommandBufferID p_cmd_buffer, AccelerationStructureID p_acceleration_structure, BufferID p_scratch_buffer) {
	ERR_FAIL_MSG("Ray tracing is not currently supported by the Metal driver.");
}

void RenderingDeviceDriverMetal::command_build_tlas(CommandBufferID p_cmd_buffer, AccelerationStructureID p_acceleration_structure, BufferID p_scratch_buffer, BufferID p_instance_buffer, uint32_t p_instance_offset, uint32_t p_instance_count) {
	ERR_FAIL_MSG("Ray tracing is not currently supported by the Metal driver.");
}

void RenderingDeviceDriverMetal::command_bind_raytracing_pipeline(CommandBufferID p_cmd_buffer, RaytracingPipelineID p_pipeline) {
	ERR_FAIL_MSG("Ray tracing is not currently supported by the Metal driver.");
}

void RenderingDeviceDriverMetal::command_bind_raytracing_uniform_set(CommandBufferID p_cmd_buffer, UniformSetID p_uniform_set, ShaderID p_shader, uint32_t p_set_index) {
	ERR_FAIL_MSG("Ray tracing is not currently supported by the Metal driver.");
}

void RenderingDeviceDriverMetal::command_trace_rays(CommandBufferID p_cmd_buffer, const ShaderBindingTable &p_raygen_sbt, const ShaderBindingTable &p_miss_sbt, const ShaderBindingTable &p_hit_sbt, uint32_t p_width, uint32_t p_height, uint32_t p_depth) {
	ERR_FAIL_MSG("Ray tracing is not currently supported by the Metal driver.");
}

#pragma mark - Queries

// ----- TIMESTAMP -----

RDD::QueryPoolID RenderingDeviceDriverMetal::timestamp_query_pool_create(uint32_t p_query_count) {
	return QueryPoolID(1);
}

void RenderingDeviceDriverMetal::timestamp_query_pool_free(QueryPoolID p_pool_id) {
}

void RenderingDeviceDriverMetal::timestamp_query_pool_get_results(QueryPoolID p_pool_id, uint32_t p_query_count, uint64_t *r_results) {
	// Metal doesn't support timestamp queries, so we just clear the buffer.
	bzero(r_results, p_query_count * sizeof(uint64_t));
}

uint64_t RenderingDeviceDriverMetal::timestamp_query_result_to_time(uint64_t p_result) {
	return p_result;
}

void RenderingDeviceDriverMetal::command_timestamp_query_pool_reset(CommandBufferID p_cmd_buffer, QueryPoolID p_pool_id, uint32_t p_query_count) {
}

void RenderingDeviceDriverMetal::command_timestamp_write(CommandBufferID p_cmd_buffer, QueryPoolID p_pool_id, uint32_t p_index) {
}

#pragma mark - Labels

void RenderingDeviceDriverMetal::command_begin_label(CommandBufferID p_cmd_buffer, const char *p_label_name, const Color &p_color) {
	MDCommandBufferBase *cb = (MDCommandBufferBase *)(p_cmd_buffer.id);
	cb->begin_label(p_label_name, p_color);
}

void RenderingDeviceDriverMetal::command_end_label(CommandBufferID p_cmd_buffer) {
	MDCommandBufferBase *cb = (MDCommandBufferBase *)(p_cmd_buffer.id);
	cb->end_label();
}

void RenderingDeviceDriverMetal::command_group_end(CommandBufferID p_cmd_buffer) {
	MDCommandBufferBase *cb = (MDCommandBufferBase *)(p_cmd_buffer.id);
	// Closing the encoder here is what guarantees no encoder straddles a level
	// boundary, which is what makes per-encoder fences sufficient.
	cb->end();
	cb->advance_sync_level();
}

#pragma mark - Debug

void RenderingDeviceDriverMetal::command_insert_breadcrumb(CommandBufferID p_cmd_buffer, uint32_t p_data) {
	// TODO: Implement.
}

#pragma mark - Submission

void RenderingDeviceDriverMetal::begin_segment(uint32_t p_frame_index, uint32_t p_frames_drawn) {
	_frame_index = p_frame_index;
	_frames_drawn = p_frames_drawn;
	_maybe_release_pipeline_archive(p_frames_drawn);
	_maybe_flush_compute_archive();
}

void RenderingDeviceDriverMetal::end_segment() {
	const Error error = end_segment_checked();
	ERR_FAIL_COND_MSG(error != OK, "Metal copy queue segment completion was refused.");
}

Error RenderingDeviceDriverMetal::end_segment_checked(bool p_nonblocking) {
	MutexLock lock(copy_queue_mutex);
	return _copy_queue_flush(p_nonblocking);
}

#pragma mark - Misc

void RenderingDeviceDriverMetal::set_object_name(ObjectType p_type, ID p_driver_id, const String &p_name) {
	NS::String *label = conv::to_nsstring(p_name);

	switch (p_type) {
		case OBJECT_TYPE_TEXTURE: {
			TextureInfo *tex_info = (TextureInfo *)p_driver_id.id;
			if (!tex_info->rasterization_rate_map) {
				tex_info->texture.get()->setLabel(label);
			}
		} break;
		case OBJECT_TYPE_SAMPLER: {
			// Can't set label after creation.
		} break;
		case OBJECT_TYPE_BUFFER: {
			const BufferInfo *buf_info = (const BufferInfo *)p_driver_id.id;
			buf_info->buffer.get()->setLabel(label);
		} break;
		case OBJECT_TYPE_SHADER: {
			MDShader *shader = (MDShader *)(p_driver_id.id);
			if (MDRenderShader *rs = dynamic_cast<MDRenderShader *>(shader); rs != nullptr) {
				std::shared_ptr<MDLibrary> vert_lib;
				std::shared_ptr<MDLibrary> frag_lib;
				{
					MutexLock lock(pipeline_state_cache_mutex);
					vert_lib = rs->vert;
					frag_lib = rs->frag;
				}
				if (vert_lib) {
					vert_lib->set_label(label);
				}
				if (frag_lib) {
					frag_lib->set_label(label);
				}
			} else if (MDComputeShader *cs = dynamic_cast<MDComputeShader *>(shader); cs != nullptr) {
				std::shared_ptr<MDLibrary> kernel_lib;
				{
					MutexLock lock(pipeline_state_cache_mutex);
					kernel_lib = cs->kernel;
				}
				if (kernel_lib) {
					kernel_lib->set_label(label);
				}
			} else {
				DEV_ASSERT(false);
			}
		} break;
		case OBJECT_TYPE_UNIFORM_SET: {
			MDUniformSet *set = (MDUniformSet *)(p_driver_id.id);
			set->arg_buffer.buffer->setLabel(label);
		} break;
		case OBJECT_TYPE_PIPELINE: {
			// Can't set label after creation.
		} break;
		default: {
			DEV_ASSERT(false);
		}
	}
}

uint64_t RenderingDeviceDriverMetal::get_resource_native_handle(DriverResource p_type, ID p_driver_id) {
	switch (p_type) {
		case DRIVER_RESOURCE_LOGICAL_DEVICE: {
			return (uint64_t)(uintptr_t)device;
		}
		case DRIVER_RESOURCE_PHYSICAL_DEVICE: {
			return 0;
		}
		case DRIVER_RESOURCE_TOPMOST_OBJECT: {
			return 0;
		}
		case DRIVER_RESOURCE_COMMAND_QUEUE: {
			return (uint64_t)(uintptr_t)get_command_queue();
		}
		case DRIVER_RESOURCE_QUEUE_FAMILY: {
			return 0;
		}
		case DRIVER_RESOURCE_TEXTURE: {
			if (p_driver_id.id == 0) {
				return 0;
			}
			TextureInfo *tex_info = (TextureInfo *)p_driver_id.id;
			return (uint64_t)(uintptr_t)tex_info->texture.get();
		}
		case DRIVER_RESOURCE_TEXTURE_VIEW: {
			if (p_driver_id.id == 0) {
				return 0;
			}
			TextureInfo *tex_info = (TextureInfo *)p_driver_id.id;
			return (uint64_t)(uintptr_t)tex_info->texture.get();
		}
		case DRIVER_RESOURCE_TEXTURE_DATA_FORMAT: {
			return 0;
		}
		case DRIVER_RESOURCE_SAMPLER: {
			return p_driver_id.id;
		}
		case DRIVER_RESOURCE_UNIFORM_SET: {
			return 0;
		}
		case DRIVER_RESOURCE_BUFFER: {
			if (p_driver_id.id == 0) {
				return 0;
			}
			const BufferInfo *buf_info = (const BufferInfo *)p_driver_id.id;
			return (uint64_t)(uintptr_t)buf_info->buffer.get();
		}
		case DRIVER_RESOURCE_COMPUTE_PIPELINE: {
			MDComputePipeline *pipeline = (MDComputePipeline *)(p_driver_id.id);
			return (uint64_t)(uintptr_t)pipeline->state.get();
		}
		case DRIVER_RESOURCE_RENDER_PIPELINE: {
			MDRenderPipeline *pipeline = (MDRenderPipeline *)(p_driver_id.id);
			return (uint64_t)(uintptr_t)pipeline->state.get();
		}
		default: {
			return 0;
		}
	}
}

Error RenderingDeviceDriverMetal::_copy_queue_copy_to_buffer(Span<uint8_t> p_src_data, MTL::Buffer *p_dst_buffer, uint64_t p_dst_offset) {
	MutexLock lock(copy_queue_mutex);
	if (copy_queue_completion_record) {
		const Error error = _copy_queue_flush();
		if (error != OK) {
			return error;
		}
	}
	if (_copy_queue_buffer_available() < p_src_data.size()) {
		const Error error = _copy_queue_flush();
		if (error != OK) {
			return error;
		}
	}

	MTL::BlitCommandEncoder *blit_encoder = _copy_queue_blit_encoder();

	memcpy(_copy_queue_buffer_ptr(), p_src_data.ptr(), p_src_data.size());

	blit_encoder->copyFromBuffer(copy_queue_buffer.buffer.get(), copy_queue_buffer_offset, p_dst_buffer, p_dst_offset, p_src_data.size());

	_copy_queue_buffer_consume(p_src_data.size());
	return OK;
}

Error RenderingDeviceDriverMetal::qualification_prepare_segment(QueueCallReport &report, bool p_nonblocking) {
	report = {};
	if (p_nonblocking) {
		if (!copy_queue_mutex.try_lock()) {
			return ERR_BUSY;
		}
	} else {
		copy_queue_mutex.lock();
	}
	struct Unlock {
		Mutex &mutex;
		~Unlock() { mutex.unlock(); }
	} copy_unlock{ copy_queue_mutex };
	if (!copy_queue_blit_encoder && !copy_queue_completion_record) {
		return OK;
	}
	if (p_nonblocking) {
		if (!completion_records_mutex.try_lock()) {
			return ERR_BUSY;
		}
	} else {
		completion_records_mutex.lock();
	}
	CompletionRecord *record = copy_queue_completion_record;
	if (!record) {
		// Reserve the exact callback identity and transfer one observer to the Frame
		// before this original queue commits. Refusal is still zero native effects.
		record = register_completion_record(copy_queue_command_buffer.get(), 0, true, true);
		if (!record) {
			completion_records_mutex.unlock();
			return active_native_source_admission && active_native_source_admission->error != OK ? active_native_source_admission->error : ERR_PARAMETER_RANGE_ERROR;
		}
		record->observers = 1;
		copy_queue_completion_record = record;
		NativeCallFact fact;
		fact.frame_observer_owned = true;
		fact.queue_index = 2;
		fact.serial = record->serial;
		fact.issue = IssueFact::IssuedUnknown;
		fact.completion = CompletionCapability::CommandBuffer;
		fact.completion_serial = record->serial;
		report.calls.push_back(fact);
		report.issued = true;
		report.native_issue_count = 1;
		copy_queue_blit_encoder.get()->endEncoding();
		copy_queue_blit_encoder.reset();
		copy_queue_command_buffer.get()->commit();
		if (p_nonblocking) {
			completion_records_mutex.unlock();
			return ERR_BUSY;
		}
	}
	// Never hold completion_records_mutex while waiting on Metal. The completion
	// callback and native release/poll paths share this mutex; holding it across
	// waitUntilCompleted starves native_call_release indefinitely.
	completion_records_mutex.unlock();
	if (!p_nonblocking && !record->completed.load(std::memory_order_acquire)) {
		copy_queue_command_buffer.get()->waitUntilCompleted();
		publish_command_buffer_status(record, copy_queue_command_buffer.get());
	}
	if (!record->completed.load(std::memory_order_acquire)) {
		return ERR_BUSY;
	}
	if (record->failed.load(std::memory_order_relaxed)) {
		return FAILED;
	}
	completion_records_mutex.lock();
	if (!record->completed.load(std::memory_order_acquire)) {
		completion_records_mutex.unlock();
		return ERR_BUSY;
	}
	if (record->failed.load(std::memory_order_relaxed)) {
		completion_records_mutex.unlock();
		return FAILED;
	}
	// Original staging bytes cannot be reused before this actual callback.
	// Frame/ticket observers keep the same source alive after owner retirement.
	record->retired.store(true);
	copy_queue_completion_record = nullptr;
	copy_queue_command_buffer.reset();
	copy_queue_buffer_offset = 0;
	if (!record->observers) {
		for (uint32_t i = 0; i < completion_records.size(); ++i) {
			if (completion_records[i] == record) {
				completion_records.remove_at(i);
				break;
			}
		}
		release_completion_record(record);
	}
	completion_records_mutex.unlock();
	return OK;
}

Error RenderingDeviceDriverMetal::_copy_queue_flush(bool p_nonblocking) {
	// Reuse this original copy queue's existing CompletionRecord issuer and
	// prepare path. Admission precedes registration, endEncoding and commit;
	// refusal leaves the encoder, staging bytes and destination custody intact.
	QueueCallReport report;
	const Error error = qualification_prepare_segment(report, p_nonblocking || !native_source_can_block());
	if (!report.calls.is_empty()) {
		report.error = error == ERR_BUSY ? OK : error;
		// prepare transfers one real observer. The existing Frame keeps it
		// even on a failed native prefix; outside an RD scope return it here.
#ifdef TESTS_ENABLED
		if (OS::get_singleton()->has_environment("GODOT_QUALIFICATION_CAPACITY_TRACE")) {
			print_line(vformat("QCAP source=copy_queue_flush original_completion_record=1 serial=%d native_issues=%d", report.calls[0].serial, report.native_issue_count));
		}
#endif
		if (!capture_hidden_native_source(report)) {
			for (const auto &call : report.calls) {
				const Error released = native_call_release(call.serial);
				CRASH_COND(released != OK);
			}
		}
	}
	return error;
}

Error RenderingDeviceDriverMetal::_copy_queue_initialize() {
	DEV_ASSERT(!copy_queue);

	copy_queue = NS::TransferPtr(device->newCommandQueue());
	copy_queue.get()->setLabel(MTLSTR("Copy Command Queue"));
	ERR_FAIL_COND_V(!copy_queue, ERR_CANT_CREATE);

	// Reserve 1 MiB for copy commands. If the buffer fills, it will be flushed automatically.
	copy_queue_buffer = allocator->new_buffer(1024 * 1024, MTL::ResourceStorageModeShared | MTL::ResourceHazardTrackingModeUntracked);
	copy_queue_buffer.buffer.get()->setLabel(MTLSTR("Copy Command Scratch Buffer"));

	return OK;
}

uint64_t RenderingDeviceDriverMetal::get_total_memory_used() {
	return device->currentAllocatedSize();
}

uint64_t RenderingDeviceDriverMetal::get_lazily_memory_used() {
	return 0; // TODO: Track this (grep for memoryless in Godot's Metal backend).
}

uint64_t RenderingDeviceDriverMetal::limit_get(Limit p_limit) {
	const MetalDeviceProperties &props = (*device_properties);
	const MetalLimits &limits = props.limits;
	uint64_t safe_unbounded = ((uint64_t)1 << 30);
#if defined(DEV_ENABLED)
#define UNKNOWN(NAME) \
	case NAME: \
		WARN_PRINT_ONCE("Returning maximum value for unknown limit " #NAME "."); \
		return safe_unbounded;
#else
#define UNKNOWN(NAME) \
	case NAME: \
		return safe_unbounded
#endif

	// clang-format off
	switch (p_limit) {
		case LIMIT_MAX_BOUND_UNIFORM_SETS:
			return limits.maxBoundDescriptorSets;
		case LIMIT_MAX_FRAMEBUFFER_COLOR_ATTACHMENTS:
			return limits.maxColorAttachments;
		case LIMIT_MAX_TEXTURES_PER_UNIFORM_SET:
			return limits.maxTexturesPerArgumentBuffer;
		case LIMIT_MAX_SAMPLERS_PER_UNIFORM_SET:
			return limits.maxSamplersPerArgumentBuffer;
		case LIMIT_MAX_STORAGE_BUFFERS_PER_UNIFORM_SET:
			return limits.maxBuffersPerArgumentBuffer;
		case LIMIT_MAX_STORAGE_IMAGES_PER_UNIFORM_SET:
			return limits.maxTexturesPerArgumentBuffer;
		case LIMIT_MAX_UNIFORM_BUFFERS_PER_UNIFORM_SET:
			return limits.maxBuffersPerArgumentBuffer;
		case LIMIT_MAX_DRAW_INDEXED_INDEX:
			return limits.maxDrawIndexedIndexValue;
		case LIMIT_MAX_FRAMEBUFFER_HEIGHT:
			return limits.maxFramebufferHeight;
		case LIMIT_MAX_FRAMEBUFFER_WIDTH:
			return limits.maxFramebufferWidth;
		case LIMIT_MAX_TEXTURE_ARRAY_LAYERS:
			return limits.maxImageArrayLayers;
		case LIMIT_MAX_TEXTURE_SIZE_1D:
			return limits.maxImageDimension1D;
		case LIMIT_MAX_TEXTURE_SIZE_2D:
			return limits.maxImageDimension2D;
		case LIMIT_MAX_TEXTURE_SIZE_3D:
			return limits.maxImageDimension3D;
		case LIMIT_MAX_TEXTURE_SIZE_CUBE:
			return limits.maxImageDimensionCube;
		case LIMIT_MAX_TEXTURES_PER_SHADER_STAGE:
			return limits.maxTexturesPerArgumentBuffer;
		case LIMIT_MAX_SAMPLERS_PER_SHADER_STAGE:
			return limits.maxSamplersPerArgumentBuffer;
		case LIMIT_MAX_STORAGE_BUFFERS_PER_SHADER_STAGE:
			return limits.maxBuffersPerArgumentBuffer;
		case LIMIT_MAX_STORAGE_IMAGES_PER_SHADER_STAGE:
			return limits.maxTexturesPerArgumentBuffer;
		case LIMIT_MAX_UNIFORM_BUFFERS_PER_SHADER_STAGE:
			return limits.maxBuffersPerArgumentBuffer;
		case LIMIT_MAX_PUSH_CONSTANT_SIZE:
			return limits.maxBufferLength;
		case LIMIT_MAX_UNIFORM_BUFFER_SIZE:
			return limits.maxBufferLength;
		case LIMIT_MAX_VERTEX_INPUT_ATTRIBUTE_OFFSET:
			return limits.maxVertexDescriptorLayoutStride;
		case LIMIT_MAX_VERTEX_INPUT_ATTRIBUTES:
			return limits.maxVertexInputAttributes;
		case LIMIT_MAX_VERTEX_INPUT_BINDINGS:
			return limits.maxVertexInputBindings;
		case LIMIT_MAX_VERTEX_INPUT_BINDING_STRIDE:
			return limits.maxVertexInputBindingStride;
		case LIMIT_MIN_UNIFORM_BUFFER_OFFSET_ALIGNMENT:
			return limits.minUniformBufferOffsetAlignment;
		case LIMIT_MAX_COMPUTE_WORKGROUP_COUNT_X:
			return limits.maxComputeWorkGroupCount.width;
		case LIMIT_MAX_COMPUTE_WORKGROUP_COUNT_Y:
			return limits.maxComputeWorkGroupCount.height;
		case LIMIT_MAX_COMPUTE_WORKGROUP_COUNT_Z:
			return limits.maxComputeWorkGroupCount.depth;
		case LIMIT_MAX_COMPUTE_WORKGROUP_INVOCATIONS:
			return std::max({ limits.maxThreadsPerThreadGroup.width, limits.maxThreadsPerThreadGroup.height, limits.maxThreadsPerThreadGroup.depth });
		case LIMIT_MAX_COMPUTE_WORKGROUP_SIZE_X:
			return limits.maxThreadsPerThreadGroup.width;
		case LIMIT_MAX_COMPUTE_WORKGROUP_SIZE_Y:
			return limits.maxThreadsPerThreadGroup.height;
		case LIMIT_MAX_COMPUTE_WORKGROUP_SIZE_Z:
			return limits.maxThreadsPerThreadGroup.depth;
		case LIMIT_MAX_COMPUTE_SHARED_MEMORY_SIZE:
			return limits.maxThreadGroupMemoryAllocation;
		case LIMIT_MAX_VIEWPORT_DIMENSIONS_X:
			return limits.maxViewportDimensionX;
		case LIMIT_MAX_VIEWPORT_DIMENSIONS_Y:
			return limits.maxViewportDimensionY;
		case LIMIT_SUBGROUP_SIZE:
			// MoltenVK sets the subgroupSize to the same as the maxSubgroupSize.
			return limits.maxSubgroupSize;
		case LIMIT_SUBGROUP_MIN_SIZE:
			return limits.minSubgroupSize;
		case LIMIT_SUBGROUP_MAX_SIZE:
			return limits.maxSubgroupSize;
		case LIMIT_SUBGROUP_IN_SHADERS:
			return (uint64_t)limits.subgroupSupportedShaderStages;
		case LIMIT_SUBGROUP_OPERATIONS:
			return (uint64_t)limits.subgroupSupportedOperations;
		case LIMIT_METALFX_TEMPORAL_SCALER_MIN_SCALE:
			return (uint64_t)((1.0 / limits.temporalScalerInputContentMaxScale) * 1000'000);
		case LIMIT_METALFX_TEMPORAL_SCALER_MAX_SCALE:
			return (uint64_t)((1.0 / limits.temporalScalerInputContentMinScale) * 1000'000);
		case LIMIT_MAX_SHADER_VARYINGS:
			return limits.maxShaderVaryings;
		default: {
#ifdef DEV_ENABLED
			WARN_PRINT("Returning maximum value for unknown limit " + itos(p_limit) + ".");
#endif
			return safe_unbounded;
		}
	}
	// clang-format on
	return 0;
}

uint64_t RenderingDeviceDriverMetal::api_trait_get(ApiTrait p_trait) {
	switch (p_trait) {
		case API_TRAIT_HONORS_PIPELINE_BARRIERS:
			return sync_mode == Barriers;
		case API_TRAIT_BUFFER_CLEARS_WITH_COPY_ENGINE:
			return true;
		case API_TRAIT_TEXTURE_CLEARS_WITH_COPY_ENGINE:
			return false;
		case API_TRAIT_TEXTURES_REQUIRE_LAYOUT_TRANSITIONS:
			return false;
		case API_TRAIT_COALESCE_COMPUTE_PASSES:
			return sync_mode == Barriers;
		default:
			return RenderingDeviceDriver::api_trait_get(p_trait);
	}
}

bool RenderingDeviceDriverMetal::has_feature(Features p_feature) {
	switch (p_feature) {
		case SUPPORTS_HALF_FLOAT:
			return true;
		case SUPPORTS_FRAGMENT_SHADER_WITH_ONLY_SIDE_EFFECTS:
			return true;
		case SUPPORTS_BUFFER_DEVICE_ADDRESS:
			return device_properties->features.supports_gpu_address;
		case SUPPORTS_METALFX_SPATIAL:
			return device_properties->features.metal_fx_spatial;
		case SUPPORTS_METALFX_TEMPORAL:
			return device_properties->features.metal_fx_temporal;
		case SUPPORTS_HDR_OUTPUT:
			return true;
		case SUPPORTS_IMAGE_ATOMIC_32_BIT:
			return device_properties->features.supports_native_image_atomics;
		case SUPPORTS_VULKAN_MEMORY_MODEL:
			return true;
		case SUPPORTS_POINT_SIZE:
			return true;
		case SUPPORTS_FRAMEBUFFER_DEPTH_RESOLVE:
			return device_properties->features.supports_msaa_depth_resolve;
		case SUPPORTS_RASTERIZATION_RATE_MAP: {
			bool is_supported = device->supportsRasterizationRateMap(1);
#if defined(VISIONOS_ENABLED)
			// We need to support 2 layers on visionOS. Using more than 2 layers shouldn't be needed.
			is_supported &= device->supportsRasterizationRateMap(2);
#endif
			return is_supported;
		}
		case SUPPORTS_GPU_MAPPABLE_BUFFER:
			return true;
		default:
			return false;
	}
}

const RDD::MultiviewCapabilities &RenderingDeviceDriverMetal::get_multiview_capabilities() {
	return multiview_capabilities;
}

const RDD::FragmentShadingRateCapabilities &RenderingDeviceDriverMetal::get_fragment_shading_rate_capabilities() {
	return fsr_capabilities;
}

const RDD::FragmentDensityMapCapabilities &RenderingDeviceDriverMetal::get_fragment_density_map_capabilities() {
	return fdm_capabilities;
}

String RenderingDeviceDriverMetal::get_api_version() const {
	return vformat("%d.%d", capabilities.version_major, capabilities.version_minor);
}

String RenderingDeviceDriverMetal::get_pipeline_cache_uuid() const {
	return pipeline_cache_id;
}

const RDD::Capabilities &RenderingDeviceDriverMetal::get_capabilities() const {
	return capabilities;
}

bool RenderingDeviceDriverMetal::is_composite_alpha_supported(CommandQueueID p_queue) const {
	// The CAMetalLayer.opaque property is configured according to this global setting.
	return OS::get_singleton()->is_layered_allowed();
}

size_t RenderingDeviceDriverMetal::get_texel_buffer_alignment_for_format(RDD::DataFormat p_format) const {
	return device->minimumLinearTextureAlignmentForPixelFormat(pixel_formats->getMTLPixelFormat(p_format));
}

size_t RenderingDeviceDriverMetal::get_texel_buffer_alignment_for_format(MTL::PixelFormat p_format) const {
	return device->minimumLinearTextureAlignmentForPixelFormat(p_format);
}

/******************/

RenderingDeviceDriverMetal::RenderingDeviceDriverMetal(RenderingContextDriverMetal *p_context_driver) :
		context_driver(p_context_driver) {
	DEV_ASSERT(p_context_driver != nullptr);
	if (String res = OS::get_singleton()->get_environment("GODOT_MTL_ARCHIVE_FAIL_ON_MISS"); res == "1") {
		archive_fail_on_miss = true;
	}
	if (archive_fail_on_miss) {
		pipeline_cache_mode = PipelineCacheMode::PERSIST;
	} else if (String cache_env = OS::get_singleton()->get_environment("ANGE_MTL_PIPELINE_CACHE"); cache_env == "0") {
		pipeline_cache_mode = PipelineCacheMode::DISABLED;
	} else if (cache_env == "persist") {
		pipeline_cache_mode = PipelineCacheMode::PERSIST;
	} else {
		pipeline_cache_mode = PipelineCacheMode::RELEASE_AFTER_WARMUP;
	}

	if (String max_mb_env = OS::get_singleton()->get_environment("ANGE_MTL_PIPELINE_CACHE_MAX_MB"); !max_mb_env.is_empty()) {
		if (max_mb_env.is_valid_int()) {
			int64_t max_mb = max_mb_env.to_int();
			if (max_mb > 0 && static_cast<uint64_t>(max_mb) <= (UINT64_MAX / (1024ull * 1024ull))) {
				archive_max_bytes = static_cast<uint64_t>(max_mb) * 1024ull * 1024ull;
			} else {
				WARN_PRINT(vformat("ANGE_MTL_PIPELINE_CACHE_MAX_MB='%s' out of range; using default %s MiB.",
						max_mb_env, String::num_uint64(ARCHIVE_MAX_BYTES_DEFAULT / (1024ull * 1024ull))));
			}
		} else {
			WARN_PRINT(vformat("ANGE_MTL_PIPELINE_CACHE_MAX_MB='%s' is not a valid integer; using default %s MiB.",
					max_mb_env, String::num_uint64(ARCHIVE_MAX_BYTES_DEFAULT / (1024ull * 1024ull))));
		}
	}

	if (String warmup_env = OS::get_singleton()->get_environment("ANGE_MTL_ARCHIVE_WARMUP_MIN_FRAMES"); !warmup_env.is_empty() && warmup_env.is_valid_int()) {
		int64_t frames = warmup_env.to_int();
		if (frames >= 0 && frames <= UINT32_MAX) {
			archive_warmup_min_frames = static_cast<uint32_t>(frames);
		}
	}
	if (String idle_env = OS::get_singleton()->get_environment("ANGE_MTL_ARCHIVE_WARMUP_IDLE_FRAMES"); !idle_env.is_empty() && idle_env.is_valid_int()) {
		int64_t frames = idle_env.to_int();
		if (frames >= 0 && frames <= UINT32_MAX) {
			archive_warmup_idle_frames = static_cast<uint32_t>(frames);
		}
	}

	// Barriers is the default; "none" selects native hazard tracking.
	// Unrecognized values intentionally keep the default.
	if (String res = OS::get_singleton()->get_environment("GODOT_MTL_SYNC_MODE"); res == U"none") {
		sync_mode = HazardTracking;
	}

#if TARGET_OS_OSX
	if (String res = OS::get_singleton()->get_environment("GODOT_MTL_SHADER_LOAD_STRATEGY"); res == U"immediate") {
		_shader_load_strategy = ShaderLoadStrategy::IMMEDIATE;
	} else {
		_shader_load_strategy = ShaderLoadStrategy::LAZY;
	}
#else
	// Always use the lazy strategy on other OSs like iOS, tvOS, or visionOS.
	_shader_load_strategy = ShaderLoadStrategy::LAZY;
#endif
}

RenderingDeviceDriverMetal::~RenderingDeviceDriverMetal() {
	auto relief = []() {
#ifdef MACOS_ENABLED
		malloc_zone_pressure_relief(nullptr, 0);
#endif
	};
	constexpr uint32_t TEARDOWN_BATCH = 16u;
	{
		MutexLock lock(pipeline_state_cache_mutex);
		LocalVector<RenderPipelineCacheKey> render_keys;
		render_keys.reserve(render_pso_cache.size());
		for (const KeyValue<RenderPipelineCacheKey, RenderPipelineCacheEntry> &kv : render_pso_cache) {
			render_keys.push_back(kv.key);
		}
		for (uint32_t i = 0; i < render_keys.size(); i += TEARDOWN_BATCH) {
			{
				NS::SharedPtr<NS::AutoreleasePool> pool = NS::TransferPtr(NS::AutoreleasePool::alloc()->init());
				const uint32_t end = MIN(i + TEARDOWN_BATCH, render_keys.size());
				for (uint32_t j = i; j < end; ++j) {
					render_pso_cache.erase(render_keys[j]);
				}
			}
			relief();
		}
		render_pso_cache.clear();

		LocalVector<ComputePipelineCacheKey> compute_keys;
		compute_keys.reserve(compute_pso_cache.size());
		for (const KeyValue<ComputePipelineCacheKey, ComputePipelineCacheEntry> &kv : compute_pso_cache) {
			compute_keys.push_back(kv.key);
		}
		for (uint32_t i = 0; i < compute_keys.size(); i += TEARDOWN_BATCH) {
			{
				NS::SharedPtr<NS::AutoreleasePool> pool = NS::TransferPtr(NS::AutoreleasePool::alloc()->init());
				const uint32_t end = MIN(i + TEARDOWN_BATCH, compute_keys.size());
				for (uint32_t j = i; j < end; ++j) {
					compute_pso_cache.erase(compute_keys[j]);
				}
			}
			relief();
		}
		compute_pso_cache.clear();
		compute_pso_keys_by_library.clear();
		compute_shader_owners_by_library.clear();
	}
	{
		NS::SharedPtr<NS::AutoreleasePool> pool = NS::TransferPtr(NS::AutoreleasePool::alloc()->init());
		pipeline_cache_free();
		drain_completion_records();
	}
	relief();
	{
		LocalVector<ShaderCacheEntry *> shader_entries;
		{
			MutexLock lock(_shader_cache_lock);
			shader_entries.reserve(_shader_cache.size());
			for (KeyValue<SHA256Digest, ShaderCacheEntry *> &kv : _shader_cache) {
				shader_entries.push_back(kv.value);
			}
			_shader_cache.clear();
		}
		for (uint32_t i = 0; i < shader_entries.size(); i += TEARDOWN_BATCH) {
			{
				NS::SharedPtr<NS::AutoreleasePool> pool = NS::TransferPtr(NS::AutoreleasePool::alloc()->init());
				const uint32_t end = MIN(i + TEARDOWN_BATCH, shader_entries.size());
				for (uint32_t j = i; j < end; ++j) {
					memdelete(shader_entries[j]);
				}
			}
			relief();
		}
	}

	memdelete(shader_container_format);
	memdelete(pixel_formats);
	memdelete(device_properties);

	if (allocator != nullptr) {
		{
			NS::SharedPtr<NS::AutoreleasePool> pool = NS::TransferPtr(NS::AutoreleasePool::alloc()->init());
			allocator->free_buffer(copy_queue_buffer);

#ifdef DEBUG_ENABLED
			if (OS::get_singleton()->is_stdout_verbose()) {
				MetalAllocatorStats stats;
				allocator->get_stats(stats);
				const char *pool_names[MetalAllocatorStats::POOL_COUNT] = { "private", "shared", "shared_wc", "private_small", "shared_small", "shared_wc_small" };
				for (uint32_t i = 0; i < MetalAllocatorStats::POOL_COUNT; i++) {
					const MetalAllocatorStats::Pool &p = stats.pools[i];
					print_verbose(vformat("Metal allocator pool %s: %d blocks, %s reserved, %s used, %d live allocations, %d dedicated (%s).",
							pool_names[i], p.block_count, String::humanize_size(p.reserved_bytes), String::humanize_size(p.used_bytes),
							p.allocation_count, p.dedicated_count, String::humanize_size(p.dedicated_bytes)));
				}
			}
#endif

			memdelete(allocator);
			allocator = nullptr;
		}
		relief();
	}
}

#pragma mark - Initialization

Error RenderingDeviceDriverMetal::_create_device() {
	device = context_driver->get_metal_device();

	device_scope = NS::TransferPtr(MTL::CaptureManager::sharedCaptureManager()->newCaptureScope(device));
	device_scope->setLabel(MTLSTR("Godot Frame"));
	device_scope->beginScope(); // Allow Xcode to capture the first frame, if desired.

	return OK;
}

void RenderingDeviceDriverMetal::encode_imported_resources(MTL::RenderCommandEncoder *p_enc) {
	MutexLock lock(imported_textures_mutex);
	if (!imported_textures.is_empty()) {
		p_enc->useResources((const MTL::Resource *const *)imported_textures.ptr(), imported_textures.size(), MTL::ResourceUsageRead, MTL::RenderStageVertex | MTL::RenderStageFragment);
	}
}

void RenderingDeviceDriverMetal::encode_imported_resources(MTL::ComputeCommandEncoder *p_enc) {
	MutexLock lock(imported_textures_mutex);
	if (!imported_textures.is_empty()) {
		p_enc->useResources((const MTL::Resource *const *)imported_textures.ptr(), imported_textures.size(), MTL::ResourceUsageRead);
	}
}

void RenderingDeviceDriverMetal::_check_capabilities() {
	capabilities.device_family = DEVICE_METAL;
	parse_msl_version(device_properties->features.msl_target_version, capabilities.version_major, capabilities.version_minor);
}

API_AVAILABLE(macos(11.0), ios(14.0), tvos(14.0))
static MetalDeviceProfile device_profile_from_properties(MetalDeviceProperties *p_device_properties) {
	using DP = MetalDeviceProfile;
	NS::OperatingSystemVersion os_version = NS::ProcessInfo::processInfo()->operatingSystemVersion();
	MetalDeviceProfile res;
	res.min_os_version = MinOsVersion(os_version.majorVersion, os_version.minorVersion, os_version.patchVersion);
#if TARGET_OS_OSX
	res.platform = DP::Platform::macOS;
#elif TARGET_OS_IPHONE
	res.platform = DP::Platform::iOS;
#elif TARGET_OS_VISION
	res.platform = DP::Platform::visionOS;
#else
#error "Unsupported Apple platform"
#endif
	res.features = {
		.msl_version = p_device_properties->features.msl_target_version,
		.use_argument_buffers = p_device_properties->features.argument_buffers_enabled(),
		.simdPermute = p_device_properties->features.simdPermute,
	};

	// highestFamily will only be set to an Apple GPU family
	switch (p_device_properties->features.highestFamily) {
		case MTL::GPUFamilyApple1:
			res.gpu = DP::GPU::Apple1;
			break;
		case MTL::GPUFamilyApple2:
			res.gpu = DP::GPU::Apple2;
			break;
		case MTL::GPUFamilyApple3:
			res.gpu = DP::GPU::Apple3;
			break;
		case MTL::GPUFamilyApple4:
			res.gpu = DP::GPU::Apple4;
			break;
		case MTL::GPUFamilyApple5:
			res.gpu = DP::GPU::Apple5;
			break;
		case MTL::GPUFamilyApple6:
			res.gpu = DP::GPU::Apple6;
			break;
		case MTL::GPUFamilyApple7:
			res.gpu = DP::GPU::Apple7;
			break;
		case MTL::GPUFamilyApple8:
			res.gpu = DP::GPU::Apple8;
			break;
		case MTL::GPUFamilyApple9:
			res.gpu = DP::GPU::Apple9;
			break;
		default: {
			// Programming error if the default case is hit.
			CRASH_NOW_MSG("Unsupported GPU family");
		} break;
	}

	return res;
}

Error RenderingDeviceDriverMetal::_initialize(uint32_t p_device_index, uint32_t p_frame_count) {
	context_device = context_driver->device_get(p_device_index);
	Error err = _create_device();
	ERR_FAIL_COND_V(err, ERR_CANT_CREATE);

	// Must be resolved before the allocator is created, so use_heaps sees the
	// final sync_mode.
	_resolve_sync_mode();

	// Must be created before _copy_queue_initialize(), which sources its scratch
	// buffer from the allocator. Barriers mode implies heap suballocation:
	// on Metal 3 it is the residency mechanism (useHeaps), and Metal 4 always
	// runs barriers.
	bool use_heaps = sync_mode == Barriers;
	allocator = MetalAllocator::create(device, use_heaps, is_local_rendering_device());
	if (use_heaps) {
		print_verbose("Metal: heap suballocation enabled.");
	}

	device_properties = memnew(MetalDeviceProperties(device));
	pixel_formats = memnew(PixelFormats(device, device_properties->features));
	device_profile = device_profile_from_properties(device_properties);
	resource_cache = std::make_unique<MDResourceCache>(device, *pixel_formats, device_properties->limits.maxPerStageBufferCount);
	shader_container_format = memnew(RenderingShaderContainerFormatMetal(&device_profile));

	_check_capabilities();

	err = _copy_queue_initialize();
	ERR_FAIL_COND_V(err, ERR_CANT_CREATE);

	_frame_count = p_frame_count;

	// Set the pipeline cache ID based on the Metal version.
	pipeline_cache_id = "metal-driver-" + get_api_version();

	if (device_properties->features.layeredRendering) {
		multiview_capabilities.is_supported = true;
		multiview_capabilities.max_view_count = device_properties->limits.maxViewports;
		// NOTE: I'm not sure what the limit is as I don't see it referenced anywhere
		multiview_capabilities.max_instance_count = UINT32_MAX;

		print_verbose("- Metal multiview supported:");
		print_verbose("  max view count: " + itos(multiview_capabilities.max_view_count));
		print_verbose("  max instances: " + itos(multiview_capabilities.max_instance_count));
	} else {
		print_verbose("- Metal multiview not supported");
	}

	// The Metal renderer requires Apple4 family. This is 2017 era A11 chips and newer.
	if (device_properties->features.highestFamily < MTL::GPUFamilyApple4) {
		String error_string = vformat("Your Apple GPU does not support the following features, which are required to use Metal-based renderers in Godot:\n\n");
		if (!device_properties->features.imageCubeArray) {
			error_string += "- No support for image cube arrays.\n";
		}

#if defined(APPLE_EMBEDDED_ENABLED)
		// Apple Embedded platforms exports currently don't exit themselves when this method returns `ERR_CANT_CREATE`.
		OS::get_singleton()->alert(error_string + "\nClick OK to exit (black screen will be visible).");
#else
		OS::get_singleton()->alert(error_string + "\nClick OK to exit.");
#endif

		return ERR_CANT_CREATE;
	}

	return OK;
}

const RenderingShaderContainerFormat &RenderingDeviceDriverMetal::get_shader_container_format() const {
	return *shader_container_format;
}
