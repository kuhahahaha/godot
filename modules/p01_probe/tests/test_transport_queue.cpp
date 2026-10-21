#include "modules/p01_probe/p01_transport.h"
#include <cassert>
int main() {
	P01TransportQueue q;
	P01CaptureEvent e{0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0};
	for (int i = 0; i < 128; i++) assert(q.push(e));
	assert(!q.push(e));
	assert(q.is_poisoned());
	P01CaptureEvent out{};
	for (int i = 0; i < 128; i++) assert(q.pop(out));
	assert(!q.pop(out));
	return 0;
}
