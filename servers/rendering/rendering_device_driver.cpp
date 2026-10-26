/**************************************************************************/
/*  rendering_device_driver.cpp                                           */
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

#include "rendering_device_driver.h"

thread_local RenderingDeviceDriver::QueueCallReport *RenderingDeviceDriver::active_queue_call_report = nullptr;

/**************/
/**** MISC ****/
/**************/

uint64_t RenderingDeviceDriver::api_trait_get(ApiTrait p_trait) {
	// Sensible canonical defaults.
	switch (p_trait) {
		case API_TRAIT_HONORS_PIPELINE_BARRIERS:
			return 1;
		case API_TRAIT_TEXTURES_REQUIRE_LAYOUT_TRANSITIONS:
			return true;
		case API_TRAIT_SHADER_CHANGE_INVALIDATION:
			return SHADER_CHANGE_INVALIDATION_ALL_BOUND_UNIFORM_SETS;
		case API_TRAIT_TEXTURE_TRANSFER_ALIGNMENT:
			return 1;
		case API_TRAIT_TEXTURE_DATA_ROW_PITCH_STEP:
			return 1;
		case API_TRAIT_SECONDARY_VIEWPORT_SCISSOR:
			return 1;
		case API_TRAIT_BUFFER_CLEARS_WITH_COPY_ENGINE:
		case API_TRAIT_TEXTURE_CLEARS_WITH_COPY_ENGINE:
			return true;
		case API_TRAIT_USE_GENERAL_IN_COPY_QUEUES:
			return false;
		case API_TRAIT_BUFFERS_REQUIRE_TRANSITIONS:
			return false;
		case API_TRAIT_TEXTURE_OUTPUTS_REQUIRE_CLEARS:
			return false;
		default:
			ERR_FAIL_V(0);
	}
}

/******************/

void RenderingDeviceDriver::command_begin_compute_pass(CommandBufferID p_cmd_buffer) {
}

void RenderingDeviceDriver::command_end_compute_pass(CommandBufferID p_cmd_buffer) {
}

RenderingDeviceDriver::FencePollResult RenderingDeviceDriver::fence_poll(FenceID p_fence, uint64_t p_expected_submission_serial) {
	return FencePollResult();
}

RenderingDeviceDriver::FencePollResult RenderingDeviceDriver::fence_wait_observed(FenceID p_fence, uint64_t p_expected_submission_serial) {
	FencePollResult result;
	result.native_error = true;
	result.native_code = ERR_UNAVAILABLE;
	return result;
}

Error RenderingDeviceDriver::fence_recycle(FenceID p_fence, uint64_t p_completed_serial) {
	return ERR_UNAVAILABLE;
}

RenderingDeviceDriver::QueueCallReport RenderingDeviceDriver::get_last_queue_call_report() const {
	MutexLock lock(queue_call_report_mutex);
	return last_queue_call_report;
}

Error RenderingDeviceDriver::command_queue_execute_and_present_observed(CommandQueueID p_cmd_queue, VectorView<SemaphoreID> p_wait_semaphores, VectorView<CommandBufferID> p_cmd_buffers, VectorView<SemaphoreID> p_cmd_semaphores, FenceID p_cmd_fence, VectorView<SwapChainID> p_swap_chains, QueueCallReport &r_report) {
	r_report = QueueCallReport();
	r_report.calls.reserve(8);
	QueueCallReport *previous_report = active_queue_call_report;
	active_queue_call_report = &r_report;
	const Error err = command_queue_execute_and_present(p_cmd_queue, p_wait_semaphores, p_cmd_buffers, p_cmd_semaphores, p_cmd_fence, p_swap_chains);
	active_queue_call_report = previous_report;
	r_report.error = err;
	return err;
}

RenderingDeviceDriver::~RenderingDeviceDriver() {}

#include "core/extension/gdextension_library_exit_guard.h"
#include "servers/rendering/rendering_device.h"
void RenderingDeviceDriver::notify_session_device_lost() const {
 GDExtensionDeviceExit::Registry::get_singleton().device_lost(session_owner);
}

void RenderingDeviceDriver::session_loss_identity(uint64_t& registry,uint64_t& epoch) const { GDExtensionDeviceExit::Registry::get_singleton().device_loss_identity(session_owner,registry,epoch); }
void RenderingDeviceDriver::notify_session_device_lost(uint64_t registry,uint64_t epoch) { GDExtensionDeviceExit::Registry::get_singleton().device_lost(registry,epoch); }
