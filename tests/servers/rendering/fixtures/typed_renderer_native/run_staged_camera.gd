extends SceneTree

const TIMEOUT_USEC := 95_000_000
const VIEW_SIZE := Vector2i(128, 128)
const RECORD_COLOR := Color(0.75, 0.25, 0.125, 1.0)

var fixture: RefCounted
var viewport: SubViewport
var camera: Camera3D
var host: Node3D
var started_usec: int
var render_loop_stopped := false
var retirement_requested := false
var selected_draw_done := false
var draw_events := 0
var draw_events_before_selected := 0
var draw_events_after_selected := 0
var draw_events_before_retirement := 0
var script_host_affine: Array[float] = []
var script_host_id: int
var script_viewport_rid: int
var image_evidence: Dictionary = {}
var pixel_error := OK
var partial_reason := ""


func _initialize() -> void:
    started_usec = Time.get_ticks_usec()
    RenderingServer.frame_pre_draw.connect(_on_frame_pre_draw)
    call_deferred("_start")


func _on_frame_pre_draw() -> void:
    draw_events += 1


func _start() -> void:
    viewport = SubViewport.new()
    viewport.size = VIEW_SIZE
    viewport.own_world_3d = true
    viewport.render_target_update_mode = SubViewport.UPDATE_DISABLED
    root.add_child(viewport)
    script_viewport_rid = viewport.get_viewport_rid().get_id()
    host = Node3D.new()
    viewport.add_child(host)
    host.transform = Transform3D(Basis.from_scale(Vector3(2, 1, 0.5)), Vector3.ZERO)
    host.force_update_transform()
    script_host_id = host.get_instance_id()
    var actual := host.global_transform
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
    RenderingServer.force_draw(false, 1.0 / 60.0)
    fixture = ClassDB.instantiate("ParticlesScenePairsTestFixture")
    RenderingServer.call_on_render_thread(Callable(fixture, "typed_renderer_host_scene_begin").bind(
        viewport.get_viewport_rid(), camera.get_camera_rid(), viewport.find_world_3d().scenario,
        actual, script_host_id, true))


func _color_near(sample: Color, expected: Color, tolerance: float) -> bool:
    return absf(sample.r - expected.r) <= tolerance and absf(sample.g - expected.g) <= tolerance \
        and absf(sample.b - expected.b) <= tolerance


func _rgba(color: Color) -> Array[float]:
    return [color.r, color.g, color.b, color.a]


func _read_pixels(capture: Dictionary) -> bool:
    if not selected_draw_done or draw_events != draw_events_after_selected \
        or int(capture.get("viewport", 0)) != script_viewport_rid \
        or int(capture.get("present_seq", 0)) <= 0 or int(capture.get("capture_ordinal", 0)) <= 0:
        return false
    # The original OBSERVE and protected copies have completed. The selected
    # viewport has drawn once since the executor was armed and is still live.
    var image := viewport.get_texture().get_image()
    if image == null or image.is_empty() or image.is_compressed() or image.get_size() != VIEW_SIZE:
        return false
    var bytes := image.get_data()
    var digest := HashingContext.new()
    if digest.start(HashingContext.HASH_SHA256) != OK or digest.update(bytes) != OK:
        return false
    var center := image.get_pixel(64, 64)
    var background := image.get_pixel(2, 2)
    var foreground_count := 0
    var background_count := 0
    var srgb := RECORD_COLOR.linear_to_srgb()
    for y in range(VIEW_SIZE.y):
        for x in range(VIEW_SIZE.x):
            var sample := image.get_pixel(x, y)
            if _color_near(sample, RECORD_COLOR, 0.07) or _color_near(sample, srgb, 0.07):
                foreground_count += 1
            if _color_near(sample, background, 0.01):
                background_count += 1
    image_evidence = {
        "width": image.get_width(), "height": image.get_height(), "format": image.get_format(),
        "byte_count": bytes.size(), "sha256": digest.finish().hex_encode(),
        "present_seq": int(capture.present_seq), "capture_ordinal": int(capture.capture_ordinal),
        "viewport_rid": script_viewport_rid, "foreground_pixels": foreground_count,
        "background_pixels": background_count, "center_rgba": _rgba(center),
        "background_rgba": _rgba(background),
    }
    return draw_events == draw_events_after_selected and foreground_count >= 128 and background_count >= 128 \
        and (_color_near(center, RECORD_COLOR, 0.07) or _color_near(center, srgb, 0.07)) \
        and not (_color_near(background, RECORD_COLOR, 0.07) or _color_near(background, srgb, 0.07))


func _report(observed: Dictionary) -> void:
    observed["host_script_affine"] = script_host_affine
    observed["host_script_node_id"] = script_host_id
    observed["staged_pixels"] = image_evidence
    observed["staged_pixel_error"] = pixel_error
    observed["staged_partial_reason"] = partial_reason
    observed["staged_no_following_draw"] = {
        "counter_source": "RenderingServer.frame_pre_draw",
        "before_selected": draw_events_before_selected, "after_selected": draw_events_after_selected,
        "before_retirement": draw_events_before_retirement, "after_retirement": draw_events,
        "render_loop_disabled": not RenderingServer.is_render_loop_enabled(),
        "viewport_updates_disabled": is_instance_valid(viewport) and viewport.render_target_update_mode == SubViewport.UPDATE_DISABLED,
    }


func _refuse(observed: Dictionary, script_detail: String = "") -> bool:
    _report(observed)
    if not script_detail.is_empty():
        observed["staged_script_error"] = ERR_INVALID_DATA
        observed["staged_script_detail"] = script_detail
    print("TYPED_RENDERER_STAGED_CAMERA_REFUSED ", JSON.stringify(observed))
    quit(1)
    return true


func _process(delta: float) -> bool:
    if fixture == null:
        return false
    var observed: Dictionary = fixture.result()
    for error_key in ["error", "prefix_retire_error", "host_scene_error", "staged_error"]:
        if int(observed.get(error_key, OK)) != OK:
            return _refuse(observed)
    if bool(observed.get("geometry_ready", false)) and not render_loop_stopped:
        RenderingServer.set_render_loop_enabled(false)
        render_loop_stopped = true
    if bool(observed.get("staged_native_ready", false)) and not selected_draw_done:
        if not render_loop_stopped or RenderingServer.is_render_loop_enabled():
            return _refuse(observed, "selected_draw_requires_disabled_loop")
        draw_events_before_selected = draw_events
        viewport.render_target_update_mode = SubViewport.UPDATE_ONCE
        RenderingServer.force_draw(false, 0.0)
        viewport.render_target_update_mode = SubViewport.UPDATE_DISABLED
        draw_events_after_selected = draw_events
        selected_draw_done = true
        if draw_events_after_selected != draw_events_before_selected + 1:
            return _refuse(observed, "selected_draw_count")
    if not retirement_requested and bool(observed.get("staged_bank_refused", false)):
        draw_events_before_retirement = draw_events
        retirement_requested = true
    if not retirement_requested and bool(observed.get("raw_guard_status_verified", false)) \
        and bool(observed.get("records_gpu_readback_verified", false)):
        if int(observed.get("staged_raster_error", OK)) != OK:
            partial_reason = "original_raster_receipt"
        if not _read_pixels(observed.get("staged_capture", {})):
            pixel_error = ERR_INVALID_DATA
            if partial_reason.is_empty():
                partial_reason = "selected_capture_pixels"
        draw_events_before_retirement = draw_events
        retirement_requested = true
    if bool(observed.get("prefix_object_destroyed", false)):
        if not retirement_requested or (not bool(observed.get("staged_bank_refused", false)) and draw_events != draw_events_before_retirement):
            return _refuse(observed, "render_server_draw_during_retirement")
        _report(observed)
        var draws_before_scene_free := draw_events
        viewport.free()
        observed["staged_no_following_draw"]["after_retirement"] = draw_events
        observed["host_script_node_freed"] = not is_instance_valid(host)
        observed["host_script_camera_freed"] = not is_instance_valid(camera)
        observed["host_script_viewport_freed"] = not is_instance_valid(viewport)
        if draw_events != (draws_before_scene_free if bool(observed.get("staged_bank_refused", false)) else draw_events_before_retirement):
            return _refuse(observed, "render_server_draw_during_scene_free")
        if bool(observed.get("staged_bank_refused", false)):
            print("TYPED_RENDERER_STAGED_BANK_REFUSAL_RETIRED ", JSON.stringify(observed))
            quit(0)
        elif partial_reason.is_empty():
            print("TYPED_RENDERER_STAGED_CAMERA_PREFIX_RETIRED_CONTINUATION_REQUIRED ", JSON.stringify(observed))
            quit(77)
        else:
            print("TYPED_RENDERER_STAGED_CAMERA_PARTIAL_RETIRED_CONTINUATION_REQUIRED ", JSON.stringify(observed))
            quit(79)
        return true
    if Time.get_ticks_usec() - started_usec >= TIMEOUT_USEC:
        _report(observed)
        print("TYPED_RENDERER_STAGED_CAMERA_TIMEOUT_CUSTODY_RETAINED ", JSON.stringify(observed))
        quit(1)
        return true
    if not render_loop_stopped:
        RenderingServer.force_draw(false, delta)
    RenderingServer.call_on_render_thread(Callable(fixture, "typed_renderer_producer_advance").bind(retirement_requested))
    return false
