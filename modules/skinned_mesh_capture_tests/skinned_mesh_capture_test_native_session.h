/**************************************************************************/
/*  skinned_mesh_capture_test_native_session.h                            */
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
#include "skinned_mesh_capture_test_image_collector.h"
#include "skinned_mesh_capture_test_native_source.h"

#include <array>
#include <memory>
#include <mutex>
#include <vector>

// Main publishes immutable requests; only the original RT phase records work.
// The registry retains this owner independently of Node/extension lifetimes.
struct SkinnedMeshCaptureTestSessionRequest {
	uint64_t request = 0;
	RID scene, mesh, skeleton, viewport;
	bool after_views = false;
	bool previous = false;
	bool image = false;
};
struct SkinnedMeshCaptureTestSessionResult {
	uint64_t request = 0, frame = 0, publication = 0;
	bool received = false;
	Error error = ERR_BUSY;
	bool recorded = false;
	bool completed = false;
	bool canceled = false;
	SkinnedMeshCaptureTestNativeSourceObservation source;
	PackedByteArray snapshot, consumer, image;
	uint64_t image_texture = 0;
	uint32_t image_format = 0;
};
struct SkinnedMeshCaptureTestSessionStatus {
	bool initialized = false;
	bool closing = false;
	bool no_native_work = false;
	bool pool_retired = false;
	Error error = OK;
	Error cleanup_error = OK;
	std::vector<SkinnedMeshCaptureTestSessionResult> results;
};
class SkinnedMeshCaptureTestNativeSession : public std::enable_shared_from_this<SkinnedMeshCaptureTestNativeSession> {
	friend class SkinnedMeshCaptureTestNativeSessionTestAccess;
	const uint64_t run, epoch;
	const uint32_t capacity;
	const bool transport_required;
	mutable std::mutex mutex;
	struct Pending {
		SkinnedMeshCaptureTestSessionRequest request;
		uint64_t publication;
		bool received = false;
	};
	std::vector<Pending> pending;
	uint64_t next_publication = 0;
	uint32_t delivery_holders = 0;
	Error delivery_error = OK;
	class Delivery;
	Error prepare_delivery(const std::vector<SkinnedMeshCaptureTestSessionRequest> &p_requests, Callable &r_delivery);
	void receive(uint64_t p_publication);
	void release_delivery(uint64_t p_publication, bool p_received);
	SkinnedMeshCaptureTestSessionStatus published;
	uint64_t last_request = 0;
	uint32_t admitted = 0;
	bool close_requested = false;
	// Everything below is accessed only on RT, including after fixture deletion.
	SkinnedMeshCaptureTestRDPool pool;
	SkinnedMeshCaptureTestReadbackCollector collector;
	SkinnedMeshCaptureTestImageCollector images;
	struct Job {
		SkinnedMeshCaptureTestSessionRequest request;
		SkinnedMeshCaptureTestSessionResult result;
		SkinnedMeshCaptureTestReadbackIdentity identities[2];
		uint32_t enqueued = 0;
		bool started = false;
		bool publication_attempted = false;
	};
	std::vector<Job> jobs;
	bool initialize_attempted = false;
	bool initialized = false;
	bool closing = false;
	bool released = false;
	bool terminal = false;
	Error failure = OK;
	Error cleanup_failure = OK;
	void fail(Error p_error) {
		if (failure == OK) {
			failure = p_error;
		} else if (cleanup_failure == OK && failure != p_error) {
			cleanup_failure = p_error;
		}
	}
	void cancel_unrecorded();
	void publish();
	void phase(uint64_t p_frame, bool p_after_views);
	void maintain();
	void record(Job &p_job);
	SkinnedMeshCaptureTestNativeSession(uint64_t p_run, uint64_t p_epoch, bool p_dual, bool p_transport_required);

public:
	static std::shared_ptr<SkinnedMeshCaptureTestNativeSession> create(uint64_t p_run, uint64_t p_epoch, bool p_dual, bool p_transport_required = false);
	Error enqueue(const SkinnedMeshCaptureTestSessionRequest &p_request);
	Error enqueue_batch(const std::vector<SkinnedMeshCaptureTestSessionRequest> &p_requests);
	void close();
	SkinnedMeshCaptureTestSessionStatus status() const;
	static void service_phase(uint64_t p_epoch, uint64_t p_frame, bool p_after_views);
	static void service_retirement();
};
