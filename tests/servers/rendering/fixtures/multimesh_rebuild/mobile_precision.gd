extends SceneTree

func _initialize() -> void:
	_run.call_deferred()

func _wait_frames(count: int) -> void:
	for i in range(count):
		await RenderingServer.frame_post_draw

func _stage(motion: bool) -> bool:
	var viewport := SubViewport.new()
	viewport.size = Vector2i(96, 96)
	viewport.own_world_3d = true
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	viewport.debug_draw = Viewport.DEBUG_DRAW_MOTION_VECTORS if motion else Viewport.DEBUG_DRAW_DISABLED
	root.add_child(viewport)
	var environment := Environment.new()
	environment.background_mode = Environment.BG_COLOR
	environment.background_color = Color.BLACK
	environment.tonemap_mode = Environment.TONE_MAPPER_LINEAR
	var world_environment := WorldEnvironment.new()
	world_environment.environment = environment
	viewport.add_child(world_environment)
	var camera := Camera3D.new()
	camera.position = Vector3(0, 0, 5)
	camera.projection = Camera3D.PROJECTION_ORTHOGONAL
	camera.size = 6.0
	viewport.add_child(camera)
	camera.make_current()
	var material := StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.albedo_color = Color.RED
	var mesh := QuadMesh.new()
	mesh.size = Vector2(1.2, 1.2)
	mesh.material = material
	var instance := MeshInstance3D.new()
	instance.mesh = mesh
	viewport.add_child(instance)
	await _wait_frames(8)
	instance.position.x = 0.2
	await _wait_frames(1)
	var image := viewport.get_texture().get_image()
	var calls := viewport.get_render_info(Viewport.RENDER_INFO_TYPE_VISIBLE, Viewport.RENDER_INFO_DRAW_CALLS_IN_FRAME)
	var image_ok := image != null and not image.is_empty() and image.get_size() == Vector2i(96, 96)
	var color_ok := true
	if image_ok and not motion:
		var color := image.get_pixel(48, 48)
		color_ok = color.r >= 254.0 / 255.0 and color.g <= 1.0 / 255.0 and color.b <= 1.0 / 255.0
	var output := OS.get_environment("GODOT_TEST_PRECISION_OUT")
	var save_ok := true
	if image_ok and not output.is_empty():
		save_ok = image.save_png(output.path_join("motion.png" if motion else "color.png")) == OK
	var passed := image_ok and calls > 0 and color_ok and save_ok
	print("MOBILE_PRECISION_STAGE ", JSON.stringify({"motion_vectors_requested": motion, "debug_draw": viewport.debug_draw, "draw_calls": calls, "image_valid": image_ok, "ordinary_red": color_ok if not motion else null, "pass": passed}))
	viewport.queue_free()
	await process_frame
	await _wait_frames(2)
	return passed

func _run() -> void:
	var cache_enabled := bool(ProjectSettings.get_setting("rendering/shader_compiler/shader_cache/enabled"))
	if Engine.is_editor_hint() or cache_enabled:
		printerr("MOBILE_PRECISION_CACHE_OR_EDITOR_MODE")
		quit(3)
		return
	print("MOBILE_PRECISION_RUNTIME cache_enabled=false editor_hint=false")
	if RenderingServer.get_rendering_device() == null:
		printerr("MOBILE_PRECISION_UNAVAILABLE")
		quit(77)
		return
	var color := await _stage(false)
	var motion := await _stage(true)
	await _wait_frames(3)
	print("MOBILE_PRECISION_RESULT ", JSON.stringify({"pass": color and motion, "stages": 2, "scope": "ordinary_native_draw_and_motion_debug_request; previous_motion_variant_not_proven"}))
	quit(0 if color and motion else 1)
