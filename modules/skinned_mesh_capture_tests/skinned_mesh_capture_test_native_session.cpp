/**************************************************************************/
/*  skinned_mesh_capture_test_native_session.cpp                          */
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

#include "skinned_mesh_capture_test_native_session.h"

#include "skinned_mesh_capture_test_capture_startup.h"
#include "skinned_mesh_capture_test_observer.h"
#include "skinned_mesh_capture_test_phase_observer.h"

#include "core/os/thread.h"
#include "servers/rendering/rendering_server.h"
#include "servers/rendering/rendering_server_globals.h"

static std::mutex registry_mutex;
static std::vector<std::shared_ptr<SkinnedMeshCaptureTestNativeSession>> sessions;

SkinnedMeshCaptureTestNativeSession::SkinnedMeshCaptureTestNativeSession(uint64_t p_run, uint64_t p_epoch, bool p_dual, bool p_transport_required) : run(p_run), epoch(p_epoch), capacity(p_dual ? 18 : 8), transport_required(p_transport_required), collector(p_run, p_epoch, p_dual) {}
std::shared_ptr<SkinnedMeshCaptureTestNativeSession> SkinnedMeshCaptureTestNativeSession::create(uint64_t p_run, uint64_t p_epoch, bool p_dual, bool p_transport_required) {
	if (!p_run || !p_epoch) {
		return {};
	}
	std::lock_guard<std::mutex> lock(registry_mutex);
	// One acquired child at a time. A second fixture cannot replace its epoch.
	if (!sessions.empty()) {
		return {};
	}
	auto session = std::shared_ptr<SkinnedMeshCaptureTestNativeSession>(new SkinnedMeshCaptureTestNativeSession(p_run, p_epoch, p_dual, p_transport_required));
	sessions.push_back(session);
	return session;
}
// This native callable is retained by the original RS command queue. Destruction
// is a separate holder acknowledgment, never synthesized from receive/timeout.
class SkinnedMeshCaptureTestNativeSession::Delivery : public CallableCustom {
	std::shared_ptr<SkinnedMeshCaptureTestNativeSession> session;
	uint64_t publication;
	mutable bool received = false;
	static bool equal(const CallableCustom *p_a, const CallableCustom *p_b) { return p_a == p_b; }
	static bool less(const CallableCustom *p_a, const CallableCustom *p_b) { return std::less<const CallableCustom *>()(p_a, p_b); }

public:
	Delivery(std::shared_ptr<SkinnedMeshCaptureTestNativeSession> p_session, uint64_t p_publication) : session(p_session), publication(p_publication) {}
	~Delivery() { session->release_delivery(publication, received); }
	uint32_t hash() const override { return uint32_t(publication ^ (publication >> 32)); }
	String get_as_text() const override { return "P01 native FIFO delivery"; }
	CompareEqualFunc get_compare_equal_func() const override { return equal; }
	CompareLessFunc get_compare_less_func() const override { return less; }
	ObjectID get_object() const override { return ObjectID(); }
	bool is_valid() const override { return true; }
	void call(const Variant **, int p_argcount, Variant &, Callable::CallError &r_error) const override {
		r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
		auto *server = RenderingServer::get_singleton();
		if (p_argcount || received || !server || !server->is_on_render_thread()) {
			return;
		}
		received = true;
		session->receive(publication);
		r_error.error = Callable::CallError::CALL_OK;
	}
};
Error SkinnedMeshCaptureTestNativeSession::prepare_delivery(const std::vector<SkinnedMeshCaptureTestSessionRequest> &p_requests, Callable &r_delivery) {
	if (!Thread::is_main_thread()) {
		return ERR_UNAVAILABLE;
	}
	if (transport_required && !skinned_mesh_capture_test_capture_transport_available()) {
		close();
		return ERR_UNAVAILABLE;
	}
	// Never replace a caller-held callable under our mutex: its destructor may
	// acknowledge this same session.
	if (!r_delivery.is_null()) {
		return ERR_INVALID_PARAMETER;
	}
	std::lock_guard<std::mutex> lock(mutex);
	if (close_requested || published.closing || published.error != OK || delivery_error != OK) {
		return ERR_UNAVAILABLE;
	}
	if (p_requests.empty()) {
		return ERR_INVALID_PARAMETER;
	}
	uint64_t previous = last_request;
	for (const auto &request : p_requests) {
		if (!request.request || request.request <= previous || request.request > UINT64_MAX / 2 || request.scene.is_null() || request.mesh.is_null() || request.skeleton.is_null() || ((request.previous || request.image) && (!request.after_views || request.viewport.is_null()))) {
			return ERR_INVALID_PARAMETER;
		}
		previous = request.request;
	}
	if (p_requests.size() > capacity - admitted || next_publication == UINT64_MAX) {
		return ERR_BUSY;
	}
	const uint64_t publication = ++next_publication;
	for (const auto &request : p_requests) {
		pending.push_back({ request, publication, false });
	}
	last_request = previous;
	admitted += p_requests.size();
	++delivery_holders;
	r_delivery = Callable(memnew(Delivery(shared_from_this(), publication)));
	return OK;
}
Error SkinnedMeshCaptureTestNativeSession::enqueue(const SkinnedMeshCaptureTestSessionRequest &p_request) {
	return enqueue_batch({ p_request });
}
Error SkinnedMeshCaptureTestNativeSession::enqueue_batch(const std::vector<SkinnedMeshCaptureTestSessionRequest> &p_requests) {
	auto *server = RenderingServer::get_singleton();
	if (!server) {
		return ERR_UNAVAILABLE;
	}
	Callable delivery;
	const Error error = prepare_delivery(p_requests, delivery);
	if (error != OK) {
		return error;
	}
	// Setter commands published by this main thread precede this marker in the
	// original queue. Inline RT execution flushes that same queue first.
	server->call_on_render_thread(delivery);
	return OK;
}
void SkinnedMeshCaptureTestNativeSession::receive(uint64_t p_publication) {
	std::lock_guard<std::mutex> lock(mutex);
	for (auto &entry : pending) {
		if (entry.publication == p_publication) {
			entry.received = true;
		}
	}
	for (auto &job : jobs) {
		if (job.result.publication == p_publication) {
			job.result.received = true;
		}
	}
}
void SkinnedMeshCaptureTestNativeSession::release_delivery(uint64_t, bool p_received) {
	std::lock_guard<std::mutex> lock(mutex);
	--delivery_holders;
	if (!p_received) {
		delivery_error = ERR_INVALID_DATA;
		close_requested = true;
	}
}
void SkinnedMeshCaptureTestNativeSession::close() {
	std::lock_guard<std::mutex> lock(mutex);
	close_requested = true;
}
SkinnedMeshCaptureTestSessionStatus SkinnedMeshCaptureTestNativeSession::status() const {
	std::lock_guard<std::mutex> lock(mutex);
	SkinnedMeshCaptureTestSessionStatus copy = published;
	for (auto &result : copy.results) {
		result.snapshot = result.snapshot.duplicate();
		result.consumer = result.consumer.duplicate();
		result.image = result.image.duplicate();
	}
	return copy;
}
void SkinnedMeshCaptureTestNativeSession::publish() {
	std::lock_guard<std::mutex> lock(mutex);
	published.initialized = initialized;
	published.closing = closing;
	published.error = failure;
	published.cleanup_error = cleanup_failure;
	published.no_native_work = terminal && !pool.retirement_required();
	published.pool_retired = terminal && pool.retirement_required();
	published.results.clear();
	for (const auto &job : jobs) {
		published.results.push_back(job.result);
	}
}
void SkinnedMeshCaptureTestNativeSession::record(Job &p_job) {
	p_job.started = true;
	const uint32_t slot = uint32_t(&p_job - jobs.data());
	Error error;
	if (p_job.request.viewport.is_valid()) {
		uint64_t sequence = 0;
		error = SkinnedMeshCaptureTestNativeSource::select_draw(p_job.request.scene, p_job.request.mesh, p_job.request.skeleton, epoch, p_job.result.frame, p_job.request.viewport, 0, sequence);
		if (error == OK) {
			error = SkinnedMeshCaptureTestNativeSource::capture_selected(p_job.request.scene, p_job.request.mesh, p_job.request.skeleton, sequence, p_job.request.previous, pool, slot, p_job.result.source);
		}
	} else {
		error = SkinnedMeshCaptureTestNativeSource::capture_current(p_job.request.scene, p_job.request.mesh, p_job.request.skeleton, pool, slot, p_job.result.source);
	}
	if (error == OK) {
		p_job.result.recorded = true;
		error = pool.record_transport(slot);
	}
	if (error == OK) {
		error = pool.enqueue_pair(slot, run, epoch, p_job.request.request, collector, p_job.identities, p_job.enqueued);
	}
	if (error == OK && p_job.request.image) {
		auto *rd = RenderingDevice::get_singleton();
		const RID texture = RenderingServer::get_singleton()->viewport_get_texture(p_job.request.viewport);
		const RID source_texture = RSG::texture_storage->texture_get_rd_texture(texture);
		RID native_texture;
		error = pool.record_image(slot, source_texture, native_texture);
		if (error != OK || !rd || native_texture.is_null()) {
			if (error == OK) {
				error = ERR_UNAVAILABLE;
			}
		} else {
			const auto format = rd->texture_get_format(native_texture);
			if (format.width != 128 || format.height != 64 || format.depth != 1 || format.array_layers != 1 || format.mipmaps != 1 || (format.format != RenderingDevice::DATA_FORMAT_R8G8B8A8_UNORM && format.format != RenderingDevice::DATA_FORMAT_R8G8B8A8_SRGB)) {
				error = ERR_INVALID_DATA;
			} else {
				p_job.result.image_texture = native_texture.get_id();
				p_job.result.image_format = format.format;
				Callable callback;
				error = images.reserve(slot, callback);
				if (error == OK) {
					error = rd->texture_get_data_async(native_texture, 0, callback);
					if (error != OK) {
						const Error rejected = images.reject(slot);
						if (rejected != OK) {
							error = rejected;
						}
					}
				}
			}
		}
	}
	p_job.result.error = error == OK ? ERR_BUSY : error;
	if (error != OK) {
		fail(error);
		closing = true;
	}
}
void SkinnedMeshCaptureTestNativeSession::phase(uint64_t p_frame, bool p_after_views) {
	if (terminal || closing) {
		return;
	}
	if (transport_required && !skinned_mesh_capture_test_capture_transport_available()) {
		fail(ERR_UNAVAILABLE);
		closing = true;
		publish();
		return;
	}
	{
		std::lock_guard<std::mutex> lock(mutex);
		if (close_requested) {
			closing = true;
		}
		if (!p_after_views && !closing) {
			// Only an original RS FIFO receive admits a batch to the next phase.
			// An in-flight prior draw cannot consume newly published main work.
			for (auto it = pending.begin(); it != pending.end();) {
				if (!it->received) {
					++it;
					continue;
				}
				Job job;
				job.request = it->request;
				job.result.request = it->request.request;
				job.result.publication = it->publication;
				job.result.received = true;
				job.result.frame = p_frame;
				jobs.push_back(job);
				it = pending.erase(it);
			}
		}
	}
	if (closing) {
		return;
	}
	if (!initialize_attempted && !p_after_views && !jobs.empty()) {
		initialize_attempted = true;
		const Error error = pool.initialize(capacity == 18);
		if (error != OK) {
			fail(error);
			closing = true;
			publish();
			return;
		}
		initialized = true;
	}
	if (!initialized) {
		return;
	}
	for (auto &job : jobs) {
		if (closing) {
			break;
		}
		if (job.started || job.result.frame != p_frame || job.request.after_views != p_after_views) {
			continue;
		}
		record(job);
	}
	publish();
}
void SkinnedMeshCaptureTestNativeSession::maintain() {
	if (terminal) {
		return;
	}
	{
		std::lock_guard<std::mutex> lock(mutex);
		if (delivery_error != OK) {
			fail(delivery_error);
		}
		closing |= close_requested;
	}
	if (collector.poisoned() || images.poisoned()) {
		fail(ERR_INVALID_DATA);
		closing = true;
	}
	if (transport_required && !skinned_mesh_capture_test_capture_transport_available()) {
		fail(ERR_UNAVAILABLE);
		closing = true;
	}
	if (!closing && (skinned_mesh_capture_test_hook_is_poisoned() || skinned_mesh_capture_test_phase_poisoned())) {
		fail(ERR_INVALID_DATA);
		closing = true;
	}
	if (initialized && !released) {
		for (auto &job : jobs) {
			if (job.enqueued != 2 || job.result.completed) {
				continue;
			}
			const Error result = pool.validate_pair(job.identities[0], job.identities[1], collector);
			if (result == ERR_BUSY) {
				continue;
			}
			if (result != OK) {
				fail(result);
				job.result.error = result;
				closing = true;
				continue;
			}
			const Error snapshot = collector.copy_completed(job.identities[0], job.result.snapshot);
			const Error consumer = collector.copy_completed(job.identities[1], job.result.consumer);
			if (snapshot != OK || consumer != OK) {
				fail(ERR_INVALID_DATA);
				closing = true;
				continue;
			}
			if (job.request.image) {
				const Error image = images.copy(uint32_t(&job - jobs.data()), job.result.image);
				if (image == ERR_BUSY) {
					continue;
				}
				if (image != OK) {
					fail(image);
					closing = true;
					continue;
				}
			}
			if (transport_required) {
				if (job.publication_attempted) {
					continue;
				}
				job.publication_attempted = true;
				const auto id = std::to_string(job.request.request);
				const bool published_raw = skinned_mesh_capture_test_capture_blob("snapshot-" + id, job.result.snapshot.ptr(), job.result.snapshot.size()) && skinned_mesh_capture_test_capture_blob("consumer-" + id, job.result.consumer.ptr(), job.result.consumer.size());
				const bool published_image = !job.request.image || (published_raw && skinned_mesh_capture_test_capture_blob("image-" + id, job.result.image.ptr(), job.result.image.size()));
				if (!published_raw || !published_image) {
					fail(ERR_UNAVAILABLE);
					closing = true;
					continue;
				}
			}
			job.result.completed = true;
			job.result.error = OK;
		}
	}
	if (closing) {
		bool deliveries_drained = false;
		{
			std::lock_guard<std::mutex> lock(mutex);
			// Close admission before sampling holder count and pending requests.
			// Main cannot publish between this sample and terminal retirement.
			close_requested = true;
			deliveries_drained = delivery_holders == 0;
			for (const auto &entry : pending) {
				Job job;
				job.request = entry.request;
				job.result.request = entry.request.request;
				job.result.publication = entry.publication;
				job.result.received = entry.received;
				jobs.push_back(job);
			}
			pending.clear();
		}
		cancel_unrecorded();
		collector.close();
		images.close();
		if (deliveries_drained && collector.drained() && images.drained()) {
			// Last image holder release can follow a duplicate callback after the
			// first poison sample. Image drain prevents further image callbacks;
			// raw drain alone does not establish callable-holder release.
			if (collector.poisoned() || images.poisoned()) {
				fail(ERR_INVALID_DATA);
			}
			if (!pool.retirement_required()) {
				terminal = true;
			} else {
				if (!released) {
					const Error error = pool.release();
					if (error == OK) {
						released = true;
					} else {
						fail(error);
					}
				}
				if (released) {
					const Error retired = pool.physical_retirement_status();
					if (retired == OK) {
						terminal = true;
					} else if (retired != ERR_BUSY) {
						fail(retired);
					}
				}
			}
		}
	}
	publish();
}
void SkinnedMeshCaptureTestNativeSession::service_phase(uint64_t p_epoch, uint64_t p_frame, bool p_after_views) {
	uint64_t frame = 0;
	bool after_views = false;
	if (!skinned_mesh_capture_test_phase_capture_window(frame, after_views) || frame != p_frame || after_views != p_after_views) {
		return;
	}
	std::vector<std::shared_ptr<SkinnedMeshCaptureTestNativeSession>> snapshot;
	{
		std::lock_guard<std::mutex> lock(registry_mutex);
		snapshot = sessions;
	}
	for (const auto &session : snapshot) {
		if (session->epoch == p_epoch) {
			session->phase(p_frame, p_after_views);
		}
	}
}
void SkinnedMeshCaptureTestNativeSession::service_retirement() {
	auto *server = RenderingServer::get_singleton();
	if (!server || !server->is_on_render_thread()) {
		return;
	}
	std::vector<std::shared_ptr<SkinnedMeshCaptureTestNativeSession>> snapshot;
	{
		std::lock_guard<std::mutex> lock(registry_mutex);
		snapshot = sessions;
	}
	for (const auto &session : snapshot) {
		session->maintain();
	}
	std::lock_guard<std::mutex> lock(registry_mutex);
	for (auto it = sessions.begin(); it != sessions.end();) {
		if ((*it)->terminal) {
			it = sessions.erase(it);
		} else {
			++it;
		}
	}
}

void SkinnedMeshCaptureTestNativeSession::cancel_unrecorded() {
	for (auto &job : jobs) {
		if (!job.started) {
			job.started = true;
			job.result.canceled = true;
			job.result.error = ERR_SKIP;
		}
	}
}
