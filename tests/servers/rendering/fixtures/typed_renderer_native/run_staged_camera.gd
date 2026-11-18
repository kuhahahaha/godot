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
var selected_draw_done := false
var second_selected_draw_done := false
var draw_events := 0
var draw_events_before_first_selected := 0
var draw_events_after_first_selected := 0
var draw_events_before_selected := 0
var draw_events_after_selected := 0
var draw_events_before_retirement := 0
var script_host_affine: Array[float] = []
var script_host_id: int
var script_viewport_rid: int
var retirement_turns := 0
var successor_prep_turns := 0
var image_evidence: Dictionary = {}
var pixel_error := OK
var pixel_detail := ""
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


func _has_fixture_error(observed: Dictionary) -> bool:
    for error_key in ["error", "prefix_retire_error", "host_scene_error", "staged_error"]:
        if int(observed.get(error_key, OK)) != OK:
            return true
    return false


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
    # The original owner has finished physical maintenance. Its selected
    # viewport remains live, and the draw counter still identifies its frame.
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
    observed["staged_pixel_detail"] = pixel_detail
    observed["staged_partial_reason"] = partial_reason
    observed["staged_retirement_owner_turns"] = retirement_turns
    observed["staged_successor_prep_turns"] = successor_prep_turns
    observed["staged_no_following_draw"] = {
        "counter_source": "RenderingServer.frame_pre_draw",
        "before_first_selected": draw_events_before_first_selected,
        "after_first_selected": draw_events_after_first_selected,
        "before_selected": draw_events_before_selected, "after_selected": draw_events_after_selected,
        "before_retirement": draw_events_before_retirement, "after_retirement": draw_events,
        "delta_after_selected": draw_events - draw_events_after_selected,
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


func _timeout(observed: Dictionary) -> bool:
    _report(observed)
    print("TYPED_RENDERER_STAGED_CAMERA_TIMEOUT_CUSTODY_RETAINED ", JSON.stringify(observed))
    quit(1)
    return true


func _retire_in_selected_turn(observed: Dictionary) -> bool:
    if not RenderingServer.is_on_render_thread():
        return _refuse(observed, "retirement_requires_inline_original_render_owner")
    if not selected_draw_done or RenderingServer.is_render_loop_enabled() \
        or viewport.render_target_update_mode != SubViewport.UPDATE_DISABLED:
        return _refuse(observed, "retirement_requires_selected_draw_and_disabled_updates")
    var bank_refused := bool(observed.get("staged_bank_refused", false))
    var genuine_prior := bool(observed.get("staged_genuine_prior_enabled", false))
    var capture: Dictionary = observed.get("staged_genuine_prior", {}).get("successor_capture", {}) \
        if genuine_prior else observed.get("staged_capture", {})
    if not bank_refused and int(capture.get("viewport", 0)) != script_viewport_rid:
        return _refuse(observed, "retirement_requires_original_selected_viewport")

    draw_events_before_retirement = draw_events
    var advance := Callable(fixture, "typed_renderer_producer_advance").bind(true)
    while not bool(observed.get("prefix_object_destroyed", false)):
        if Time.get_ticks_usec() - started_usec >= TIMEOUT_USEC:
            return _timeout(observed)
        # The original owner services its bounded OBSERVE and retirement work.
        # Flush its FIFO without returning to Main's pending-resource draw path.
        RenderingServer.call_on_render_thread(advance)
        RenderingServer.force_sync()
        retirement_turns += 1
        observed = fixture.result()
        if _has_fixture_error(observed):
            return _refuse(observed)

    if draw_events != draw_events_before_retirement or draw_events != draw_events_after_selected:
        return _refuse(observed, "render_server_draw_before_selected_pixels")
    var raster_err := int(observed.get("staged_successor_raster_error", OK)) if genuine_prior else int(observed.get("staged_raster_error", OK))
    if bank_refused:
        pass
    elif raster_err != OK:
        partial_reason = "original_raster_receipt"
        pixel_error = ERR_INVALID_DATA
        pixel_detail = "skipped_without_original_raster_receipt"
    elif not _read_pixels(capture):
        partial_reason = "selected_capture_pixels"
        pixel_error = ERR_INVALID_DATA
        pixel_detail = "selected_capture_pixels_after_original_retirement"
    _report(observed)
    viewport.free()
    observed["staged_no_following_draw"]["after_retirement"] = draw_events
    observed["staged_no_following_draw"]["delta_after_selected"] = draw_events - draw_events_after_selected
    observed["host_script_node_freed"] = not is_instance_valid(host)
    observed["host_script_camera_freed"] = not is_instance_valid(camera)
    observed["host_script_viewport_freed"] = not is_instance_valid(viewport)
    if draw_events_before_retirement != draw_events_after_selected \
        or draw_events != draw_events_before_retirement:
        return _refuse(observed, "render_server_draw_after_selected")
    if bank_refused:
        print("TYPED_RENDERER_STAGED_BANK_REFUSAL_RETIRED ", JSON.stringify(observed))
        quit(0)
    elif genuine_prior:
        if not partial_reason.is_empty():
            return _refuse(observed, partial_reason)
        print("TYPED_RENDERER_STAGED_GENUINE_PRIOR_RETIRED ", JSON.stringify(observed))
        quit(0)
    elif partial_reason.is_empty():
        print("TYPED_RENDERER_STAGED_CAMERA_PREFIX_RETIRED_CONTINUATION_REQUIRED ", JSON.stringify(observed))
        quit(77)
    else:
        print("TYPED_RENDERER_STAGED_CAMERA_PARTIAL_RETIRED_CONTINUATION_REQUIRED ", JSON.stringify(observed))
        quit(79)
    return true


func _drive_genuine_prior_and_retire() -> bool:
    draw_events_before_first_selected = draw_events_before_selected
    draw_events_after_first_selected = draw_events_after_selected
    var advance := Callable(fixture, "typed_renderer_producer_advance").bind(false)
    var observed: Dictionary = fixture.result()
    while not bool(observed.get("staged_successor_native_ready", false)):
        if Time.get_ticks_usec() - started_usec >= TIMEOUT_USEC:
            return _timeout(observed)
        RenderingServer.call_on_render_thread(advance)
        RenderingServer.force_sync()
        successor_prep_turns += 1
        observed = fixture.result()
        if _has_fixture_error(observed):
            return _refuse(observed)
    if draw_events != draw_events_after_first_selected or RenderingServer.is_render_loop_enabled():
        return _refuse(observed, "unexpected_draw_between_presentations")
    draw_events_before_selected = draw_events
    viewport.render_target_update_mode = SubViewport.UPDATE_ONCE
    RenderingServer.force_draw(false, 0.0)
    viewport.render_target_update_mode = SubViewport.UPDATE_DISABLED
    draw_events_after_selected = draw_events
    second_selected_draw_done = true
    if draw_events_after_selected != draw_events_before_selected + 1:
        return _refuse(observed, "second_selected_draw_count")
    var post_draw_turns := 0
    while not bool(observed.get("staged_successor_readbacks_verified", false)):
        if Time.get_ticks_usec() - started_usec >= TIMEOUT_USEC:
            return _timeout(observed)
        RenderingServer.call_on_render_thread(advance)
        RenderingServer.force_sync()
        successor_prep_turns += 1
        post_draw_turns += 1
        observed = fixture.result()
        if _has_fixture_error(observed):
            return _refuse(observed)
        if post_draw_turns >= 4 and not bool(observed.get("staged_genuine_prior", {}).get("successor_record_consumed", false)):
            return _refuse(observed, "successor_record_not_consumed_after_second_draw")
    var prior: Dictionary = observed.get("staged_genuine_prior", {})
    if not bool(prior.get("predecessor_live_before_ack", false)) or not bool(prior.get("predecessor_returned_live", false)) \
        or int(prior.get("successor_physical_returns_pending", -1)) != 1:
        return _refuse(observed, "successor_original_predecessor_lifetime")
    return _retire_in_selected_turn(observed)


func _process(delta: float) -> bool:
    if fixture == null:
        return false
    var observed: Dictionary = fixture.result()
    if _has_fixture_error(observed):
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
        if bool(observed.get("staged_genuine_prior_enabled", false)):
            return _drive_genuine_prior_and_retire()
    if bool(observed.get("staged_bank_refused", false)):
        return _retire_in_selected_turn(observed)
    if bool(observed.get("raw_guard_status_verified", false)) \
        and bool(observed.get("records_gpu_readback_verified", false)) \
        and not bool(observed.get("staged_genuine_prior_enabled", false)):
        return _retire_in_selected_turn(observed)
    if Time.get_ticks_usec() - started_usec >= TIMEOUT_USEC:
        return _timeout(observed)
    if not render_loop_stopped:
        RenderingServer.force_draw(false, delta)
    RenderingServer.call_on_render_thread(Callable(fixture, "typed_renderer_producer_advance").bind(false))
    return false
