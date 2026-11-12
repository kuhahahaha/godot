extends SceneTree

var fixture: RefCounted
var started_usec: int
var polls := 0
var render_loop_stopped := false

func _initialize() -> void:
    RenderingServer.set_render_loop_enabled(true)
    fixture = ClassDB.instantiate("ParticlesScenePairsTestFixture")
    started_usec = Time.get_ticks_usec()
    RenderingServer.call_on_render_thread(Callable(fixture, "typed_renderer_producer_begin"))

func _process(_delta: float) -> bool:
    var observed: Dictionary = fixture.result()
    if not observed.is_empty():
        if int(observed.get("error", 0)) != OK:
            print("TYPED_RENDERER_PRODUCER_REFUSED ", JSON.stringify(observed))
            quit(1)
            return true
        if bool(observed.get("geometry_ready", false)) and not render_loop_stopped:
            RenderingServer.set_render_loop_enabled(false)
            render_loop_stopped = true
            # The next queued owner advance performs enrollment outside lists.
        if int(observed.get("stage", 0)) == 2 and bool(observed.get("source_import_verified", false)):
            # Deliberately not an acceptance-success exit: GPU completion,
            # typed Runtime provenance, Records, Seal and draw remain pending.
            print("TYPED_RENDERER_SOURCE_RECORDED_CONTINUATION_REQUIRED ", JSON.stringify(observed))
            quit(77)
            return true
    if Time.get_ticks_usec() - started_usec >= 22000000:
        print("TYPED_RENDERER_PRODUCER_TIMEOUT_CUSTODY_RETAINED ", JSON.stringify(observed))
        quit(1)
        return true
    polls += 1
    if not render_loop_stopped:
        # Main-thread engine draw owns actual worker submission during warmup.
        RenderingServer.force_draw(false, _delta)
    RenderingServer.call_on_render_thread(Callable(fixture, "typed_renderer_producer_advance"))
    return false
