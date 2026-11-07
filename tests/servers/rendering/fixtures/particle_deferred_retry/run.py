#!/usr/bin/env python3
"""Exercise the production update queue around a failed buffer preparation."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[5]


def main():
    source = (ROOT / "servers/rendering/renderer_rd/storage_rd/particles_storage.cpp").read_text()
    start = source.index("\tSelfList<Particles>::List retry_list;", source.index("void ParticlesStorage::update_particles()"))
    end = source.index("\n\t\tif (particles->restart_request)", start)
    prefix = source[start:end]
    start = source.index("\t// Retry failed systems on the next update", end)
    end = source.index("\n}\n", start)
    tail = source[start:end]
    harness = r'''
#include <cstdio>
#include <cstdlib>
template<class T> struct SelfList {
    struct List {
        SelfList *head = nullptr;
        SelfList *first() { return head; }
        void add(SelfList *node) {
            if (node->owner) std::abort();
            node->owner = this; node->next = head; head = node;
        }
    };
    T *value;
    List *owner = nullptr;
    SelfList *next = nullptr;
    explicit SelfList(T *p) : value(p) {}
    T *self() { return value; }
    bool in_list() { return owner != nullptr; }
    void remove_from_list() {
        if (!owner) return;
        SelfList **p = &owner->head;
        while (*p != this) p = &(*p)->next;
        *p = next; owner = nullptr; next = nullptr;
    }
};
struct RID { bool valid = true; bool is_null() { return !valid; } };
struct Particles {
    int amount = 1;
    bool external = false, inactive = false;
    double inactive_time = 0;
    bool dirty = true, fail = false, retry_pending = false, frame_delta_committed = false, restart_request = false;
    unsigned frame_delta_committed_frame = UINT32_MAX;
    int attempts = 0, completed = 0;
    RID particle_buffer, particle_instance_buffer, particles_copy_uniform_set;
    SelfList<Particles> update_list{this};
};
struct RD {
    static RD *get_singleton() { static RD rd; return &rd; }
    bool uniform_set_is_valid(RID rid) { return rid.valid; }
};
struct Storage {
    SelfList<Particles>::List particle_update_list;
    void _particles_update_buffers(Particles *particles) {
        // Bound an immediate requeue regression without waiting for a hang.
        if (++particles->attempts > 4) std::abort();
        particles->particle_buffer.valid = !particles->fail;
    }
    void update_particles() {
        unsigned frame = 0;
''' + prefix + r'''
        particles->completed++;
    }
''' + tail + r'''
    }
};
int main() {
    Storage storage;
    Particles first, healthy, last, external;
    external.external = true;
    external.retry_pending = true;
    external.frame_delta_committed = true;
    external.frame_delta_committed_frame = 0;
    external.inactive = true;
    external.inactive_time = 7;
    storage.particle_update_list.add(&external.update_list);
    first.fail = last.fail = true;
    storage.particle_update_list.add(&last.update_list);
    storage.particle_update_list.add(&healthy.update_list);
    storage.particle_update_list.add(&first.update_list);
    storage.update_particles();
    bool first_pass = first.attempts == 1 && healthy.attempts == 1 && last.attempts == 1 &&
            healthy.completed == 1 && !healthy.dirty && !healthy.update_list.in_list() &&
            first.dirty && last.dirty && first.retry_pending && last.retry_pending &&
            first.update_list.in_list() && last.update_list.in_list();
    first.fail = last.fail = false;
    storage.update_particles();
    bool second_pass = first.attempts == 2 && healthy.attempts == 1 && last.attempts == 2 &&
            first.completed == 1 && healthy.completed == 1 && last.completed == 1 &&
            !first.dirty && !last.dirty && !first.retry_pending && !last.retry_pending &&
            storage.particle_update_list.first() == nullptr;
    const bool external_pass = external.attempts == 0 && external.completed == 0 && external.dirty &&
            external.retry_pending && external.frame_delta_committed && external.frame_delta_committed_frame == 0 &&
            external.inactive && external.inactive_time == 7 && !external.update_list.in_list();
    std::printf("particle deferred retry: %d/3 checks passed\n", int(first_pass) + int(second_pass) + int(external_pass));
    return first_pass && second_pass && external_pass ? 0 : 1;
}
'''
    with tempfile.TemporaryDirectory(prefix="particle-deferred-retry-") as temp:
        cpp, binary = Path(temp) / "test.cpp", Path(temp) / "test"
        cpp.write_text(harness)
        subprocess.run(["c++", "-std=c++17", "-Wall", "-Wextra", str(cpp), "-o", str(binary)], check=True)
        return subprocess.run([str(binary)], timeout=10).returncode


if __name__ == "__main__":
    raise SystemExit(main())
