#include "p01_phase_capture.h"
#include "p01_phase_observer.h"
#include <mutex>

static std::mutex request_mutex;
static std::vector<P01PhaseCaptureRequest> requests;
static std::vector<P01PhaseCaptureResult> results;
static uint64_t request_epoch = 0;
static uint64_t last_request = 0;
static bool cancelled = false;

Error p01_phase_capture_enqueue(const P01PhaseCaptureRequest &p_request) {
	std::lock_guard<std::mutex> lock(request_mutex);
	if (!p_request.epoch || !p_request.request || !p_request.frame || !p_request.pool || p_request.scene.is_null() || p_request.mesh.is_null() || p_request.skeleton.is_null() || p_request.slot >= 18) { return ERR_INVALID_PARAMETER; }
	if (p_request.previous && (!p_request.after_views || (!p_request.draw_sequence && p_request.selected_viewport.is_null()))) { return ERR_INVALID_PARAMETER; }
	if (p_request.epoch < request_epoch) { return ERR_INVALID_PARAMETER; }
	if (p_request.epoch > request_epoch) {
		if (!requests.empty()) { return ERR_BUSY; }
		request_epoch = p_request.epoch;
		last_request = 0;
		cancelled = false;
		results.clear();
	}
	if (cancelled || p_request.request <= last_request || requests.size() + results.size() >= 18) { return ERR_INVALID_PARAMETER; }
	requests.push_back(p_request);
	last_request = p_request.request;
	return OK;
}
std::vector<P01PhaseCaptureResult> p01_phase_capture_results() {
	std::lock_guard<std::mutex> lock(request_mutex);
	return results;
}
void p01_phase_capture_cancel(uint64_t p_epoch) {
	std::lock_guard<std::mutex> lock(request_mutex);
	if (p_epoch != request_epoch) { return; }
	cancelled = true;
	requests.clear();
}
void p01_phase_capture_service(uint64_t p_epoch, uint64_t p_frame, bool p_after_views) {
	uint64_t frame = 0;
	bool after_views = false;
	if (!p01_phase_capture_window(frame, after_views) || frame != p_frame || after_views != p_after_views) { return; }
	std::lock_guard<std::mutex> lock(request_mutex);
	if (cancelled || p_epoch != request_epoch) { return; }
	for (auto it = requests.begin(); it != requests.end();) {
		if (it->frame > p_frame || (it->frame == p_frame && it->after_views && !p_after_views)) { ++it; continue; }
		P01PhaseCaptureResult result;
		result.epoch = it->epoch; result.request = it->request;
		result.frame = p_frame; result.after_views = p_after_views;
		if (it->frame != p_frame || it->after_views != p_after_views) {
			result.error = ERR_INVALID_DATA;
		} else {
			uint64_t draw_sequence = it->draw_sequence;
			bool selection_valid = true;
			if (it->selected_viewport.is_valid()) {
				selection_valid = p_after_views && P01NativeSource::select_draw(it->scene, it->mesh, it->skeleton, it->epoch, p_frame, it->selected_viewport, it->draw_sequence, draw_sequence) == OK;
			}
			if (!selection_valid) {
				result.error = ERR_INVALID_DATA;
			} else {
				result.error = draw_sequence ? P01NativeSource::capture_selected(it->scene, it->mesh, it->skeleton, draw_sequence, it->previous, *it->pool, it->slot, result.source) : P01NativeSource::capture_current(it->scene, it->mesh, it->skeleton, *it->pool, it->slot, result.source);
			}
		}
		results.push_back(result);
		it = requests.erase(it);
	}
}
