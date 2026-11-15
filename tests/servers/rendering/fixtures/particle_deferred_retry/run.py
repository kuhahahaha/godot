#!/usr/bin/env python3
"""CPU stub controls for literal production queue, activation and retry slices."""
from pathlib import Path
import argparse
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[5]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=ROOT / "servers/rendering/renderer_rd/storage_rd/particles_storage.cpp",
                        help="production source input, including a private mutation control")
    source = parser.parse_args().source.read_text()
    start = source.index("\tSelfList<Particles>::List retry_list;", source.index("void ParticlesStorage::update_particles()"))
    end = source.index("\n\t\tif (particles->restart_request)", start)
    prefix = source[start:end]
    start = source.index("\t// Retry failed systems on the next update", end)
    end = source.index("\n}\n", start)
    tail = source[start:end]
    activation_start = source.index("\tSelfList<Particles>::List retry_list;", source.index("void ParticlesStorage::update_particles()"))
    activation_end = source.index("\n\t\tint fixed_fps = 0;", activation_start)
    loops_start = source.index("\t\t// Validate process times before comparisons", activation_end)
    loops_end = source.index("\t\t// Ensure that memory is initialized", loops_start)
    success_start = source.index("\t\tparticles->request_process_time = 0.0;", loops_end)
    success_end = source.index("\t// Retry failed systems on the next update", success_start)
    copy_start = source.index("\t\t// Copy particles to instance buffer.", loops_end)
    copy_condition_start = source.index("\t\tif (", copy_start)
    copy_condition_end = source.index("{", copy_condition_start) + 1
    copy_calls_start = source.index("\t\t\tRD::ComputeListID compute_list = ", copy_condition_end)
    copy_calls_end = source.index("\n\t\t}", copy_calls_start)
    fragments = {
        "PREFIX": source[activation_start:activation_end],
        "LOOPS": source[loops_start:loops_end],
        "COPY_CONDITION": source[copy_condition_start:copy_condition_end],
        "COPY_CALLS": source[copy_calls_start:copy_calls_end],
        "SUCCESS": source[success_start:success_end],
        "TAIL": tail,
    }
    harness = r'''
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cmath>
#include <initializer_list>
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define ERR_PRINT(message) ((void)0)
#ifdef REAL_T_IS_DOUBLE
using real_t = double;
#else
using real_t = float;
#endif
namespace Math { template<class T> bool is_finite(T v) { return std::isfinite(v); } }
// SelfList/RID/RD and process dispatch are CPU stubs, not engine/GPU execution.
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
struct RID { bool valid = false; int id = 0; bool is_null() const { return !valid; } };
namespace RSE {
enum { PARTICLES_DRAW_ORDER_INDEX, PARTICLES_DRAW_ORDER_VIEW_DEPTH };
enum { PARTICLES_TRANSFORM_ALIGN_DISABLED, PARTICLES_TRANSFORM_ALIGN_Z_BILLBOARD,
       PARTICLES_TRANSFORM_ALIGN_Z_BILLBOARD_Y_TO_VELOCITY, PARTICLES_TRANSFORM_ALIGN_LOCAL_BILLBOARD };
enum { PARTICLES_MODE_2D };
}
// Pipeline lookup and copy recording below are CPU resource/counter shells.
// The production copy eligibility and actual bind/call statements are literal.
struct Pipeline {
    RID rid{true, 11};
    int lookups = 0;
    RID get_rid() { lookups++; return rid; }
};
struct ParticlesShader {
    enum { COPY_MODE_FILL_INSTANCES };
    struct CopyPushConstant { int copy_mode_2d = 0; uint32_t invocation_offset = 0; };
    Pipeline copy_pipelines[2][1];
} particles_shader;
struct RendererCompositorRD {
    double delta = 0;
    static RendererCompositorRD *get_singleton() { static RendererCompositorRD v; return &v; }
    double get_frame_delta_time() { return delta; }
};
struct Dependency {
    enum { DEPENDENCY_CHANGED_AABB };
    int notifications = 0;
    void changed_notify(int) { notifications++; }
};
struct Particles {
    int amount = 1;
    bool external = false, inactive = false;
    double inactive_time = 0;
    bool dirty = true, fail = false, retry_pending = false, frame_delta_committed = false, restart_request = false;
    unsigned frame_delta_committed_frame = UINT32_MAX;
    int attempts = 0, completed = 0;
    int fixed_fps = 64, succeeded = 0, clears = 0, fail_on = 0, prepares = 0;
    bool emitting = true, clear = false, pre_process_pending = false;
    double speed_scale = 1, lifetime = 1, phase = 0, prev_phase = 0;
    double frame_remainder = 0, pre_process_time = 0;
    real_t pre_process_time_remaining = 0, request_process_time = 0, request_process_time_residual = 0;
    uint64_t prev_ticks = 0;
    unsigned userdata_count = 0;
    int prepared_userdata_count = -1;
    int draw_order = RSE::PARTICLES_DRAW_ORDER_INDEX;
    int transform_align = RSE::PARTICLES_TRANSFORM_ALIGN_DISABLED;
    int mode = RSE::PARTICLES_MODE_2D;
    Dependency dependency;
    RID particle_buffer{true, 1}, particle_instance_buffer{true, 2};
    RID particles_copy_uniform_set{true, 3}, trail_bind_pose_uniform_set{true, 4};
    SelfList<Particles> update_list{this};
};
struct RD {
    using ComputeListID = long long;
    enum { LIMIT_MAX_COMPUTE_WORKGROUP_COUNT_X };
    uint64_t max_groups = 65535;
    uint64_t limit_get(int) const { return max_groups; }
    static constexpr ComputeListID INVALID_ID = -1;
    int pipeline_checks = 0, validated_pipeline = 0, bound_pipeline = 0;
    int copy_begins = 0, copy_dispatches = 0, copy_ends = 0;
    bool copy_active = false, pipeline_bound = false;
    static RD *get_singleton() { static RD rd; return &rd; }
    bool uniform_set_is_valid(RID rid) { return rid.valid; }
    bool compute_pipeline_is_valid(RID rid) {
        pipeline_checks++; validated_pipeline = rid.id;
        return rid.valid && (rid.id == 11 || rid.id == 12);
    }
    ComputeListID compute_list_begin() { copy_begins++; copy_active = true; return 8; }
    void compute_list_bind_compute_pipeline(ComputeListID, RID rid) {
        bound_pipeline = rid.id;
        pipeline_bound = rid.valid && (rid.id == 11 || rid.id == 12);
    }
    void compute_list_bind_uniform_set(ComputeListID, RID, unsigned) {}
    void compute_list_set_push_constant(ComputeListID, const void *, unsigned) {}
    void compute_list_dispatch_threads(ComputeListID, unsigned, unsigned, unsigned) {
        if (copy_active && pipeline_bound) copy_dispatches++;
    }
    void compute_list_end() { copy_ends++; copy_active = false; pipeline_bound = false; }
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
        (void)retrying; // This queue-only control stops before processing.
        particles->completed++;
    }
''' + tail + r'''
    }
};
struct ActivationStorage {
    unsigned frame=7;
    SelfList<Particles>::List particle_update_list;
    // Resource preparation and process dispatch are recording stubs. Successful
    // process calls model only phase advance/clear commit, not shader behavior.
    void _particles_update_buffers(Particles *p) {
        p->prepares++;
        if (p->prepared_userdata_count >= 0) p->userdata_count = unsigned(p->prepared_userdata_count);
    }
    bool _particles_process(Particles *p, double delta) {
        if (++p->attempts > 30) std::abort();
        if (p->attempts == p->fail_on) return false;
        if (p->clear) p->clears++;
        p->clear=false; p->phase += delta; p->succeeded++; return true;
    }
    void update_particles() {
@PREFIX@
        const int fixed_fps=particles->fixed_fps;
@LOOPS@
        // Geometry/push-constant construction is outside this CPU control.
        ParticlesShader::CopyPushConstant copy_push_constant;
        const unsigned total_amount = unsigned(particles->amount);
@COPY_CONDITION@
@COPY_CALLS@
        }
@SUCCESS@
@TAIL@
    }
};
static bool activation_checks() {
    int checks=0, failures=0;
    auto check=[&](bool value, const char *name) {
        checks++; if (!value) { failures++; std::printf("FAIL: %s\n", name); }
    };
    for (bool inactive : {false, true}) {
        ActivationStorage s; Particles p;
        p.inactive=inactive; p.inactive_time=inactive ? 4.0 : 0.0; p.clear=true; p.fail_on=2;
        RendererCompositorRD::get_singleton()->delta=0.0625;
        s.particle_update_list.add(&p.update_list);
        s.update_particles();
        check(p.succeeded == 1 && p.frame_remainder == 0.046875 && p.frame_delta_committed && p.retry_pending && p.update_list.in_list() &&
                !p.inactive && p.inactive_time == 0, "partial fixed step remains queued");
        p.fail_on=0;
        s.update_particles();
        std::printf("fixed initial_inactive=%d succeeded=%d clears=%d phase=%.6f remainder=%.6f prepares=%d\n", inactive, p.succeeded, p.clears, p.phase, p.frame_remainder, p.prepares);
        check(p.succeeded == 4 && p.clears == 1 && p.phase == 0.0625 && p.frame_remainder == 0, "same-frame retry resumes exactly three pending steps without reinitializing");
        check(!p.retry_pending && !p.dirty && !p.update_list.in_list() && !p.frame_delta_committed && p.dependency.notifications == 1, "success removes retry and publishes once");
    }
    // Both one-shot queues can fail before ordinary frame-delta debit.
    for (bool preprocess : {false, true}) {
        ActivationStorage s; Particles p;
        p.inactive=true; p.inactive_time=4.0; p.clear=true; p.fail_on=2;
        if (preprocess) p.pre_process_time=0.0625;
        else p.request_process_time=0.0625;
        RendererCompositorRD::get_singleton()->delta=0;
        s.particle_update_list.add(&p.update_list); s.update_particles();
        const real_t remaining=preprocess ? p.pre_process_time_remaining : p.request_process_time;
        check(p.succeeded == 1 && remaining == 0.046875 && !p.inactive &&
                p.inactive_time == 0 && !p.frame_delta_committed && p.retry_pending,
                "one-shot failure preserves activated epoch before frame debit");
        p.fail_on=0; s.update_particles();
        std::printf("one-shot preprocess=%d succeeded=%d clears=%d phase=%.6f\n", preprocess, p.succeeded, p.clears, p.phase);
        check(p.succeeded == 4 && p.clears == 1 && p.phase == 0.0625 &&
                !p.pre_process_pending && p.request_process_time == 0 &&
                !p.retry_pending && !p.update_list.in_list() && p.dependency.notifications == 1,
                "one-shot retry retains advanced phase and initializes once");
    }
    // Explicit restart between retries deliberately starts a new epoch.
    {
        ActivationStorage s; Particles p; p.inactive=true; p.clear=true; p.fail_on=2;
        RendererCompositorRD::get_singleton()->delta=0.0625;
        s.particle_update_list.add(&p.update_list); s.update_particles();
        p.fail_on=0; p.restart_request=true; s.update_particles();
        check(p.succeeded == 5 && p.clears == 2 && p.phase == 0.0625 && !p.restart_request, "explicit restart invalidates pending epoch");
    }
    // Uniform refusal before restart must preserve the pending restart and list.
    {
        ActivationStorage s; Particles p; p.inactive=true; p.restart_request=true;
        p.inactive_time=4.0; p.particles_copy_uniform_set.valid=false;
        RendererCompositorRD::get_singleton()->delta=0.0625;
        s.particle_update_list.add(&p.update_list); s.update_particles();
        check(p.attempts == 0 && p.inactive && p.inactive_time == 4.0 && p.restart_request && p.retry_pending && p.update_list.in_list(), "uniform readiness refusal preserves pending restart");
        p.particles_copy_uniform_set.valid=true; s.update_particles();
        check(p.succeeded == 4 && p.clears == 1 && !p.restart_request && !p.update_list.in_list(), "readiness recovery performs exactly one restart");
    }
    // Non-emitting lifetime debit is rolled back on failure and charged once.
    {
        ActivationStorage s; Particles p; p.emitting=false; p.inactive_time=0.25; p.fail_on=1;
        RendererCompositorRD::get_singleton()->delta=0.0625;
        s.particle_update_list.add(&p.update_list); s.update_particles();
        check(!p.inactive && p.inactive_time == 0.25 && p.retry_pending, "failed non-emitting frame restores lifetime debit");
        p.fail_on=0; s.update_particles();
        check(!p.inactive && p.inactive_time == 0.3125 && p.succeeded == 4, "non-emitting retry debits lifetime once");
    }
    std::printf("outer retry literal: %d/%d checks passed\n", checks-failures, checks);
    return failures == 0;
}

static bool copy_preflight_checks() {
    int checks=0, failures=0;
    auto check=[&](bool value, const char *name) {
        checks++; if (!value) { failures++; std::printf("FAIL: %s\n", name); }
    };
    RD *rd=RD::get_singleton();
    // Null and nonowned copy pipelines must both fail readiness. The active
    // particle already has pending temporal work; expected values are literal.
    for (RID unavailable : {RID{}, RID{true, 99}}) {
        *rd=RD(); particles_shader=ParticlesShader();
        particles_shader.copy_pipelines[0][0].rid=unavailable;
        ActivationStorage s; Particles p;
        p.phase=0.125; p.prev_phase=0.25; p.prev_ticks=41; p.clear=true;
        p.inactive_time=0.25; p.frame_remainder=0.03125;
        p.pre_process_time=0.0625; p.pre_process_pending=true;
        p.pre_process_time_remaining=0.015625;
        p.request_process_time=0.03125; p.request_process_time_residual=0.015625;
        RendererCompositorRD::get_singleton()->delta=0.0625;
        s.particle_update_list.add(&p.update_list); s.update_particles();
        check(p.prepares==1 && p.attempts==0 && p.succeeded==0 && p.retry_pending && p.dirty &&
                s.particle_update_list.first()==&p.update_list && p.update_list.next==nullptr &&
                p.dependency.notifications==0 && rd->copy_begins==0,
                "unavailable required copy queues once before simulation or publication");
        check(p.phase==0.125 && p.prev_phase==0.25 && p.prev_ticks==41 && p.clear &&
                p.frame_remainder==0.03125 && !p.frame_delta_committed && p.frame_delta_committed_frame==UINT32_MAX,
                "unavailable required copy preserves active simulation and delta state");
        check(p.pre_process_pending && p.pre_process_time_remaining==0.015625 &&
                p.pre_process_time==0.0625 && p.request_process_time==0.03125 && p.request_process_time_residual==0.015625,
                "unavailable required copy preserves preprocess and one-shot requests");
        check(!p.inactive && p.inactive_time==0.25 && !p.restart_request,
                "unavailable active copy restores inactivity debit without creating restart");
        particles_shader.copy_pipelines[0][0].rid={true, 11};
        s.update_particles();
        check(p.prepares==2 && p.succeeded==10 && p.clears==1 && p.phase==0.28125 &&
                p.frame_remainder==0 && !p.pre_process_pending && p.request_process_time==0 &&
                p.request_process_time_residual==0 && !p.retry_pending && !p.update_list.in_list() &&
                p.dependency.notifications==1 && rd->copy_dispatches==1,
                "copy readiness recovery consumes pending work and publishes exactly once");
        s.update_particles();
        check(p.prepares==2 && p.succeeded==10 && p.dependency.notifications==1 && rd->copy_dispatches==1,
                "successful copy recovery leaves no duplicate queued update");
    }
    // All four view-dependent exclusions are independent rows. No predicate is
    // reimplemented here: the copy branch and prefix come from production.
    const int view_cases[][2]={
        {RSE::PARTICLES_DRAW_ORDER_VIEW_DEPTH, RSE::PARTICLES_TRANSFORM_ALIGN_DISABLED},
        {RSE::PARTICLES_DRAW_ORDER_INDEX, RSE::PARTICLES_TRANSFORM_ALIGN_Z_BILLBOARD},
        {RSE::PARTICLES_DRAW_ORDER_INDEX, RSE::PARTICLES_TRANSFORM_ALIGN_Z_BILLBOARD_Y_TO_VELOCITY},
        {RSE::PARTICLES_DRAW_ORDER_INDEX, RSE::PARTICLES_TRANSFORM_ALIGN_LOCAL_BILLBOARD},
    };
    for (const auto &view : view_cases) {
        *rd=RD(); particles_shader=ParticlesShader();
        particles_shader.copy_pipelines[0][0].rid={};
        ActivationStorage s; Particles p;
        p.draw_order=view[0]; p.transform_align=view[1]; p.clear=true;
        RendererCompositorRD::get_singleton()->delta=0.0625;
        s.particle_update_list.add(&p.update_list); s.update_particles();
        check(p.succeeded==4 && p.clears==1 && p.dependency.notifications==1 && !p.retry_pending &&
                !p.update_list.in_list() && rd->pipeline_checks==0 && rd->copy_begins==0 &&
                particles_shader.copy_pipelines[0][0].lookups==0,
                "view-dependent update bypasses unavailable outer-copy pipeline");
    }
    {
        *rd=RD(); particles_shader=ParticlesShader();
        particles_shader.copy_pipelines[0][0].rid={};
        particles_shader.copy_pipelines[1][0].rid={true, 12};
        ActivationStorage s; Particles p;
        p.userdata_count=0; p.prepared_userdata_count=1;
        RendererCompositorRD::get_singleton()->delta=0.0625;
        s.particle_update_list.add(&p.update_list); s.update_particles();
        check(p.userdata_count==1 && p.succeeded==4 && !p.retry_pending && p.dependency.notifications==1 &&
                particles_shader.copy_pipelines[0][0].lookups==0 &&
                particles_shader.copy_pipelines[1][0].lookups==1 && rd->pipeline_checks==1,
                "copy readiness selects prepared userdata pipeline exactly once");
        check(rd->validated_pipeline==12 && rd->bound_pipeline==12 && rd->copy_dispatches==1,
                "actual copy bind reuses the validated cached pipeline RID");
    }
    // Preflight follows logical inactivity transitions. Neither a stable idle
    // system nor one that expires this update ever reaches the copy operation.
    for (bool expired_now : {false, true}) {
        *rd=RD(); particles_shader=ParticlesShader();
        particles_shader.copy_pipelines[0][0].rid={};
        ActivationStorage s; Particles p;
        p.emitting=false; p.inactive=!expired_now; p.inactive_time=expired_now ? 1.1875 : 4.0;
        RendererCompositorRD::get_singleton()->delta=0.0625;
        s.particle_update_list.add(&p.update_list); s.update_particles();
        check(p.inactive && p.attempts==0 && !p.retry_pending && !p.dirty && !p.update_list.in_list() &&
                p.dependency.notifications==0 && rd->pipeline_checks==0 && rd->copy_begins==0 &&
                particles_shader.copy_pipelines[0][0].lookups==0 &&
                p.inactive_time==(expired_now ? 1.25 : 4.0),
                "idle or newly expired particle skips unavailable copy without retry");
    }
    {
        *rd=RD(); particles_shader=ParticlesShader();
        particles_shader.copy_pipelines[0][0].rid={};
        ActivationStorage s; Particles p;
        p.emitting=false; p.inactive_time=0.25; p.phase=0.125;
        RendererCompositorRD::get_singleton()->delta=0.0625;
        s.particle_update_list.add(&p.update_list); s.update_particles();
        check(!p.inactive && p.inactive_time==0.25 && p.phase==0.125 && p.succeeded==0 &&
                p.retry_pending && p.update_list.in_list() && p.dependency.notifications==0,
                "active non-emitting copy refusal restores lifetime debit before processing");
    }
    // Activation and explicit restart install a clear epoch before readiness.
    // Their logical transition remains; no temporal work may be consumed yet.
    for (bool restart : {false, true}) {
        *rd=RD(); particles_shader=ParticlesShader();
        particles_shader.copy_pipelines[0][0].rid={};
        ActivationStorage s; Particles p;
        p.inactive=!restart; p.inactive_time=4.0; p.restart_request=restart;
        p.phase=0.25; p.clear=false; p.request_process_time=0.03125;
        RendererCompositorRD::get_singleton()->delta=0.0625;
        s.particle_update_list.add(&p.update_list); s.update_particles();
        check(p.succeeded==0 && p.attempts==0 && p.phase==0 && p.clear && !p.inactive &&
                p.inactive_time==(restart ? 4.0 : 0.0) && !p.restart_request &&
                p.request_process_time==0.03125 && p.retry_pending && p.update_list.in_list() &&
                p.dependency.notifications==0 && rd->copy_begins==0,
                "logical activation or restart survives copy refusal without consuming work");
        particles_shader.copy_pipelines[0][0].rid={true, 11};
        s.update_particles();
        check(p.succeeded==6 && p.clears==1 && p.phase==0.09375 && p.request_process_time==0 &&
                !p.retry_pending && !p.update_list.in_list() && p.dependency.notifications==1 && rd->copy_dispatches==1,
                "activated copy readiness recovery initializes exactly once");
    }
    std::printf("particle copy preflight literal: %d/%d checks passed\n",checks-failures,checks);
    return failures==0;
}

static bool dispatch_limit_checks() {
    RD *rd = RD::get_singleton(); *rd=RD(); particles_shader=ParticlesShader();
    ActivationStorage s; Particles p;
    p.phase=0.125; p.clear=true; p.request_process_time=0.03125;
    RendererCompositorRD::get_singleton()->delta=0.0625;
    rd->max_groups=0; s.particle_update_list.add(&p.update_list); s.update_particles();
    const bool preserved = p.succeeded==0 && p.attempts==0 && p.phase==0.125 && p.clear &&
            p.request_process_time==0.03125 && p.frame_remainder==0 && !p.frame_delta_committed;
    const bool queued = p.retry_pending && p.dirty && p.update_list.in_list() &&
            p.update_list.next==nullptr && rd->copy_begins==0 && p.dependency.notifications==0;
    rd->max_groups=1; s.update_particles();
    const bool recovered = p.succeeded==6 && p.clears==1 && p.phase==0.21875 &&
            p.request_process_time==0 && !p.retry_pending && !p.update_list.in_list() &&
            rd->copy_dispatches==1 && p.dependency.notifications==1;
    s.update_particles();
    const bool once = p.succeeded==6 && p.dependency.notifications==1 && rd->copy_dispatches==1;
    const int passed=int(preserved)+int(queued)+int(recovered)+int(once);
    std::printf("particle dispatch limit preflight: %d/4 checks passed\n",passed);
    return passed==4;
}

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
    const bool activation_pass=activation_checks();
    const bool copy_pass=copy_preflight_checks();
    const bool dispatch_pass=dispatch_limit_checks();
    return first_pass && second_pass && external_pass && activation_pass && copy_pass && dispatch_pass ? 0 : 1;
}
'''
    for name, body in fragments.items():
        marker = "@" + name + "@"
        assert harness.count(marker) == 1, marker
        harness = harness.replace(marker, body)
    with tempfile.TemporaryDirectory(prefix="particle-deferred-retry-") as temp:
        cpp = Path(temp) / "test.cpp"
        cpp.write_text(harness)
        result = 0
        for precision in ("float", "double"):
            binary = Path(temp) / precision
            command = ["c++", "-std=c++17", "-Wall", "-Wextra"]
            if precision == "double":
                command.append("-DREAL_T_IS_DOUBLE")
            subprocess.run(command + [str(cpp), "-o", str(binary)], check=True)
            print(f"CPU stub controls ({precision}):", flush=True)
            result |= subprocess.run([str(binary)], timeout=10).returncode
        return result


if __name__ == "__main__":
    raise SystemExit(main())
