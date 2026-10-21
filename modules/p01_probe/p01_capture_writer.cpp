#include "p01_capture_writer.h"
#ifdef UNIX_ENABLED
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

P01CaptureWriter::~P01CaptureWriter() { close(); }
bool P01CaptureWriter::start(int p_inherited_write_descriptor) {
	std::lock_guard<std::mutex> lock(mutex);
	if (started || closing) { return false; }
#ifdef UNIX_ENABLED
	struct stat status;
	const int flags = fcntl(p_inherited_write_descriptor, F_GETFL);
	if (p_inherited_write_descriptor < 3 || flags < 0 || (flags & O_ACCMODE) != O_WRONLY || fstat(p_inherited_write_descriptor, &status) != 0 || !S_ISFIFO(status.st_mode)) { return false; }
	if (fcntl(p_inherited_write_descriptor, F_SETFL, flags | O_NONBLOCK) != 0 || fcntl(p_inherited_write_descriptor, F_SETFD, FD_CLOEXEC) != 0) { return false; }
#ifdef F_SETNOSIGPIPE
	if (fcntl(p_inherited_write_descriptor, F_SETNOSIGPIPE, 1) != 0) { return false; }
#endif
	descriptor = p_inherited_write_descriptor;
	started = true;
	worker = std::thread(&P01CaptureWriter::run, this);
	return true;
#else
	(void)p_inherited_write_descriptor;
	return false;
#endif
}
bool P01CaptureWriter::publish(const std::string &p_line) {
	std::lock_guard<std::mutex> lock(mutex);
	if (!started || closing || poisoned) { return false; }
	if (p_line.empty() || p_line.size() > 65536 || p_line.back() != '\n' || p_line.find('\n') != p_line.size() - 1 || queue.size() >= 128 || bytes > (16ULL * 1024 * 1024 + 4ULL * 512 * 1024 * 1024) - p_line.size()) {
		poisoned = true;
		ready.notify_one();
		return false;
	}
	bytes += p_line.size();
	queue.push_back(p_line);
	ready.notify_one();
	return true;
}
void P01CaptureWriter::run() {
#ifdef UNIX_ENABLED
	// SIGPIPE is blocked only on this native writer thread. EPIPE becomes
	// evidence failure; it cannot terminate the render process or retire work.
	sigset_t blocked;
	sigemptyset(&blocked); sigaddset(&blocked, SIGPIPE);
	if (pthread_sigmask(SIG_BLOCK, &blocked, nullptr) != 0) {
		std::lock_guard<std::mutex> lock(mutex); poisoned = true;
	}
	for (;;) {
		std::string line;
		{
			std::unique_lock<std::mutex> lock(mutex);
			ready.wait(lock, [&] { return closing || poisoned || !queue.empty(); });
			if (poisoned || (closing && queue.empty())) { break; }
			line = std::move(queue.front()); queue.pop_front();
		}
		const ssize_t written = ::write(descriptor, line.data(), line.size());
		if (written < 0 || size_t(written) != line.size()) {
			std::lock_guard<std::mutex> lock(mutex); poisoned = true;
			break;
		}
	}
	::close(descriptor);
	std::lock_guard<std::mutex> lock(mutex);
	descriptor = -1;
	queue.clear();
#endif
}
void P01CaptureWriter::close() {
	{
		std::lock_guard<std::mutex> lock(mutex);
		closing = true;
		ready.notify_one();
	}
	if (worker.joinable()) { worker.join(); }
}
bool P01CaptureWriter::failed() const { std::lock_guard<std::mutex> lock(mutex); return poisoned; }
