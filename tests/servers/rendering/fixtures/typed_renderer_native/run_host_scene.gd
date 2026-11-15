extends SceneTree

var fixture: RefCounted
var viewport: SubViewport
var camera: Camera3D
var host: Node3D
var started_usec: int
var render_loop_stopped := false
var retirement_requested := false
var script_host_affine: Array = []
var script_host_id: int

func _initialize() -> void:
    started_usec = Time.get_ticks_usec()
    call_deferred("_start")

func _start() -> void:
    viewport = SubViewport.new()
    viewport.size = Vector2i(128, 128)
    viewport.own_world_3d = true
    viewport.render_target_update_mode = SubViewport.UPDATE_DISABLED
    root.add_child(viewport)
    host = Node3D.new()
    viewport.add_child(host)
    host.transform = Transform3D(Basis.from_scale(Vector3(2, 1, 0.5)), Vector3(0, 0, 0))
    host.force_update_transform()
    script_host_id = host.get_instance_id()
    var actual: Transform3D = host.global_transform
    for row in range(3):
        for column in range(3):
            script_host_affine.append(actual.basis[column][row])
        script_host_affine.append(actual.origin[row])
    camera = Camera3D.new()
    viewport.add_child(camera)
    camera.projection = Camera3D.PROJECTION_ORTHOGONAL
    camera.size = 4
    camera.position = Vector3(0, 0, 5)
    camera.make_current()
    camera.force_update_transform()
    RenderingServer.set_render_loop_enabled(true)
    viewport.render_target_update_mode = SubViewport.UPDATE_ONCE
    RenderingServer.force_draw(false, 1.0 / 60.0) # Actual viewport buffers, before source custody.
    fixture = ClassDB.instantiate("ParticlesScenePairsTestFixture")
    RenderingServer.call_on_render_thread(Callable(fixture, "typed_renderer_host_scene_begin").bind(
        viewport.get_viewport_rid(), camera.get_camera_rid(), viewport.find_world_3d().scenario, actual, script_host_id))

func _process(_delta: float) -> bool:
    if fixture == null:
        return false
    var observed: Dictionary = fixture.result()
    if not observed.is_empty():
        if int(observed.get("error", 0)) != OK or int(observed.get("prefix_retire_error", 0)) != OK or int(observed.get("host_scene_error", 0)) != OK:
            observed["host_script_affine"] = script_host_affine
            observed["host_script_node_id"] = script_host_id
            print("TYPED_RENDERER_HOST_SCENE_REFUSED ", JSON.stringify(observed))
            quit(1)
            return true
        if bool(observed.get("geometry_ready", false)) and not render_loop_stopped:
            RenderingServer.set_render_loop_enabled(false)
            render_loop_stopped = true
        if bool(observed.get("host_scene_ready", false)):
            retirement_requested = true
        if bool(observed.get("prefix_retire_complete", false)):
            observed["host_script_affine"] = script_host_affine
            observed["host_script_node_id"] = script_host_id
            viewport.free() # Borrowed World3D scenario returns through its actual owner.
            observed["host_script_node_freed"] = not is_instance_valid(host)
            observed["host_script_camera_freed"] = not is_instance_valid(camera)
            observed["host_script_viewport_freed"] = not is_instance_valid(viewport)
            print("TYPED_RENDERER_HOST_SCENE_PREFIX_RETIRED_CONTINUATION_REQUIRED ", JSON.stringify(observed))
            quit(77)
            return true
    if Time.get_ticks_usec() - started_usec >= 65000000:
        print("TYPED_RENDERER_HOST_SCENE_TIMEOUT_CUSTODY_RETAINED ", JSON.stringify(observed))
        quit(1)
        return true
    if not render_loop_stopped:
        RenderingServer.force_draw(false, _delta)
    RenderingServer.call_on_render_thread(Callable(fixture, "typed_renderer_producer_advance").bind(retirement_requested))
    return false
