extends SceneTree

var fixture: RefCounted
var started_usec: int
var render_loop_stopped := false

func _initialize() -> void:
    RenderingServer.set_render_loop_enabled(true)
    fixture = ClassDB.instantiate("ParticlesScenePairsTestFixture")
    started_usec = Time.get_ticks_usec()
    RenderingServer.call_on_render_thread(Callable(fixture, "typed_renderer_producer_begin"))

func _process(_delta: float) -> bool:
    var observed: Dictionary = fixture.result()
    if not observed.is_empty():
        if int(observed.get("error", 0)) != OK or int(observed.get("prefix_retire_error", 0)) != OK:
            print("TYPED_RENDERER_PREFIX_RETIRE_REFUSED ", JSON.stringify(observed))
            quit(1)
            return true
        if bool(observed.get("geometry_ready", false)) and not render_loop_stopped:
            RenderingServer.set_render_loop_enabled(false)
            render_loop_stopped = true
        if bool(observed.get("prefix_retire_complete", false)) and bool(observed.get("prefix_object_destroyed", false)):
            print("TYPED_RENDERER_PREFIX_RETIRED_CONTINUATION_REQUIRED ", JSON.stringify(observed))
            quit(77)
            return true
    if Time.get_ticks_usec() - started_usec >= 45000000:
        print("TYPED_RENDERER_PREFIX_RETIRE_TIMEOUT_CUSTODY_RETAINED ", JSON.stringify(observed))
        quit(1)
        return true
    if not render_loop_stopped:
        RenderingServer.force_draw(false, _delta)
    var retire_prefix := int(observed.get("stage", 0)) == 2
    RenderingServer.call_on_render_thread(Callable(fixture, "typed_renderer_producer_advance").bind(retire_prefix))
    return false
