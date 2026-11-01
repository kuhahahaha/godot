// Primitive behavioral witness using the repository's real HashSet/ObjectID.
// Memory/error stubs isolate the container; this is not an engine reload test.
#include "core/object/object_id.h"
#include "core/templates/hash_set.h"
#include <cstdlib>
#include <cstdio>
#include <vector>
namespace Memory {
template <bool zero> void *alloc_static(size_t bytes, bool padded) {
	if (padded) { std::abort(); }
	return zero ? std::calloc(1, bytes) : std::malloc(bytes);
}
void *realloc_static(void *memory, size_t bytes, bool padded) {
	if (padded) { std::abort(); }
	return std::realloc(memory, bytes);
}
void free_static(void *memory, bool padded) {
	if (padded) { std::abort(); }
	std::free(memory);
}
}
void _err_print_error(const char *, const char *, int, const char *, const char *, bool, ErrorHandlerType) { std::abort(); }
int main() {
	for (bool snapshot : { false, true }) {
		HashSet<ObjectID> ids;
		for (uint64_t id = 1; id <= 4; id++) { ids.insert(ObjectID(id)); }
		int visits[5] = {};
		auto callback = [&](ObjectID id) {
			if (id == ObjectID(uint64_t(3))) { return; } // ObjectDB: victim is gone.
			visits[uint64_t(id)]++;
			if (id == ObjectID(uint64_t(1))) { ids.erase(ObjectID(uint64_t(3))); }
		};
		if (snapshot) {
			std::vector<ObjectID> copy;
			for (ObjectID id : ids) { copy.push_back(id); }
			for (ObjectID id : copy) { callback(id); }
		} else {
			for (const ObjectID &id : ids) { callback(id); }
		}
		std::printf("%s survivor_4_visits=%d expected=%d\n", snapshot ? "snapshot" : "direct", visits[4], snapshot ? 1 : 2);
		if (visits[4] != (snapshot ? 1 : 2)) { return 1; }
	}
}
