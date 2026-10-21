#pragma once
#include "p01_native_source.h"
#include <array>
#include <memory>
#include <mutex>
#include <vector>

// Main publishes immutable requests; only the original RT phase records work.
// The registry retains this owner independently of Node/extension lifetimes.
struct P01SessionRequest {
	uint64_t request = 0;
	RID scene, mesh, skeleton, viewport;
	bool after_views = false, previous = false;
};
struct P01SessionResult {
	uint64_t request = 0, frame = 0, publication = 0;
	bool received = false;
	Error error = ERR_BUSY;
	bool recorded = false, completed = false, canceled = false;
	P01NativeSourceObservation source;
	PackedByteArray snapshot, consumer;
};
struct P01SessionStatus {
	bool initialized = false, closing = false, no_native_work = false, pool_retired = false;
	Error error = OK, cleanup_error = OK;
	std::vector<P01SessionResult> results;
};
class P01NativeSession : public std::enable_shared_from_this<P01NativeSession> {
	friend class P01NativeSessionTestAccess;
	const uint64_t run, epoch;
	const uint32_t capacity;
	const bool transport_required;
	mutable std::mutex mutex;
	struct Pending { P01SessionRequest request; uint64_t publication; bool received = false; };
	std::vector<Pending> pending;
	uint64_t next_publication = 0;
	uint32_t delivery_holders = 0;
	Error delivery_error = OK;
	class Delivery;
	Error prepare_delivery(const std::vector<P01SessionRequest> &p_requests, Callable &r_delivery);
	void receive(uint64_t p_publication);
	void release_delivery(uint64_t p_publication, bool p_received);
	P01SessionStatus published;
	uint64_t last_request = 0;
	uint32_t admitted = 0;
	bool close_requested = false;
	// Everything below is accessed only on RT, including after fixture deletion.
	P01RDPool pool;
	P01ReadbackCollector collector;
	struct Job { P01SessionRequest request; P01SessionResult result; P01ReadbackIdentity identities[2]; uint32_t enqueued = 0; bool started = false; };
	std::vector<Job> jobs;
	bool initialize_attempted = false, initialized = false, closing = false, released = false, terminal = false;
	Error failure = OK, cleanup_failure = OK;
	void fail(Error p_error) { if (failure == OK) { failure = p_error; } else if (cleanup_failure == OK && failure != p_error) { cleanup_failure = p_error; } }
	void cancel_unrecorded();
	void publish();
	void phase(uint64_t p_frame, bool p_after_views);
	void maintain();
	void record(Job &p_job);
	P01NativeSession(uint64_t p_run, uint64_t p_epoch, bool p_dual, bool p_transport_required);
public:
	static std::shared_ptr<P01NativeSession> create(uint64_t p_run, uint64_t p_epoch, bool p_dual, bool p_transport_required = false);
	Error enqueue(const P01SessionRequest &p_request);
	Error enqueue_batch(const std::vector<P01SessionRequest> &p_requests);
	void close();
	P01SessionStatus status() const;
	static void service_phase(uint64_t p_epoch, uint64_t p_frame, bool p_after_views);
	static void service_retirement();
};
