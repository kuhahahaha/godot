#pragma once
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>

// Private byte transport only. It grants no event origin, completion or verdict.
// Takes ownership only of a validated inherited POSIX write pipe. No path open,
// listener, reconnect or script binding. Render-side publication never does I/O.
class P01CaptureWriter {
	mutable std::mutex mutex;
	std::condition_variable ready;
	std::deque<std::string> queue;
	std::thread worker;
	int descriptor = -1;
	bool started = false, closing = false, poisoned = false;
	uint64_t bytes = 0;
	void run();
public:
	P01CaptureWriter() = default;
	P01CaptureWriter(const P01CaptureWriter &) = delete;
	P01CaptureWriter &operator=(const P01CaptureWriter &) = delete;
	~P01CaptureWriter();
	bool start(int p_inherited_write_descriptor);
	bool publish(const std::string &p_line);
	void close();
	bool failed() const;
};
