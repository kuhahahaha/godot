extends SceneTree

var fixture: RefCounted
var viewport: SubViewport
var camera: Camera3D
var baseline: Dictionary = {}
var views: Dictionary = {}
var observed := false
var started: int
var capture_dir: String
var baseline_only := OS.get_environment("ANGE_SCENE_VIEWPORT_BASELINE_ONLY") == "1"

func _initialize() -> void:
	started = Time.get_ticks_msec()
	if not ClassDB.class_exists("ParticlesScenePairsTestFixture"):
		print("SCENE_VIEWPORT_INVARIANCE_NATIVE_MISSING")
		quit(77)
		return
	fixture = ClassDB.instantiate("ParticlesScenePairsTestFixture")
	capture_dir = OS.get_environment("ANGE_SCENE_VIEWPORT_CAPTURE_DIR")
	if not capture_dir.is_absolute_path() or DirAccess.make_dir_recursive_absolute(capture_dir) != OK:
		push_error("An absolute writable ANGE_SCENE_VIEWPORT_CAPTURE_DIR is required")
		quit(1)
		return
	call_deferred("_start")

func _start() -> void:
	viewport = SubViewport.new()
	viewport.size = Vector2i(128, 128)
	viewport.own_world_3d = true
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	viewport.use_debanding = false
	root.add_child(viewport)
	var world := Node3D.new()
	viewport.add_child(world)
	var environment := WorldEnvironment.new()
	environment.environment = Environment.new()
	environment.environment.background_mode = Environment.BG_COLOR
	environment.environment.background_color = Color(0.08, 0.08, 0.08, 1)
	environment.environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	environment.environment.ambient_light_color = Color.WHITE
	environment.environment.ambient_light_energy = 0.7
	world.add_child(environment)
	var mesh := MeshInstance3D.new()
	var box := BoxMesh.new()
	box.size = Vector3(0.8, 0.8, 0.8)
	var material := StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.albedo_color = Color.WHITE
	box.material = material
	mesh.mesh = box
	mesh.position = Vector3(-0.7, 0, 0)
	mesh.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_ON
	world.add_child(mesh)
	mesh.force_update_transform()
	var sprite := Sprite3D.new()
	var image := Image.create(16, 16, false, Image.FORMAT_RGBA8)
	image.fill(Color(0.2, 0.8, 0.3, 1))
	sprite.texture = ImageTexture.create_from_image(image)
	sprite.pixel_size = 0.05
	sprite.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	sprite.position = Vector3(0.7, 0, 0)
	world.add_child(sprite)
	sprite.force_update_transform()
	var light := DirectionalLight3D.new()
	light.rotation_degrees = Vector3(-60, -20, 0)
	light.shadow_enabled = true
	world.add_child(light)
	light.force_update_transform()
	camera = Camera3D.new()
	camera.projection = Camera3D.PROJECTION_ORTHOGONAL
	camera.size = 4
	camera.near = 0.1
	camera.far = 20
	world.add_child(camera)
	camera.make_current()
	RenderingServer.set_render_loop_enabled(true)
	baseline["top"] = _snapshot("top", "before")
	baseline["side"] = _snapshot("side", "before")
	if baseline_only:
		_observe()
		_finish({"pass": true, "mode": "baseline-only"})
		return
	# Enrollment and original RECORD are scheduled on RT. Only production draw
	# dispatches the typed phase callback which installs the generation root.
	RenderingServer.call_on_render_thread(Callable(fixture, "run_viewport_invariance").bind(viewport.find_world_3d().scenario))

func _snapshot(view: String, suffix: String) -> Dictionary:
	if view == "top":
		camera.position = Vector3(0, 5, 0)
		camera.look_at(Vector3.ZERO, Vector3.FORWARD)
	else:
		camera.position = Vector3(0, 0, 5)
		camera.look_at(Vector3.ZERO, Vector3.UP)
	camera.force_update_transform()
	RenderingServer.force_draw(false, 1.0 / 60.0)
	# This reads the actual viewport GPU result, not a frame-count proxy.
	var image := viewport.get_texture().get_image()
	image.convert(Image.FORMAT_RGBA8)
	var bytes := image.get_data()
	# Count actual RGBA8 pixels differing from the first pixel. A black or
	# otherwise uniform baseline cannot pass as rendered scene evidence.
	var nonuniform_pixels := 0
	for offset in range(0, bytes.size(), 4):
		if bytes[offset] != bytes[0] or bytes[offset + 1] != bytes[1] or bytes[offset + 2] != bytes[2] or bytes[offset + 3] != bytes[3]:
			nonuniform_pixels += 1
	var hash := HashingContext.new()
	hash.start(HashingContext.HASH_SHA256)
	hash.update(bytes)
	var path := capture_dir.path_join(view + "_" + suffix + ".png")
	var saved := image.save_png(path)
	return {"bytes": bytes, "sha256": hash.finish().hex_encode(), "path": path,
		"saved": saved == OK, "nonuniform_pixels": nonuniform_pixels, "width": image.get_width(), "height": image.get_height(),
		"draw_calls": viewport.get_render_info(Viewport.RENDER_INFO_TYPE_VISIBLE, Viewport.RENDER_INFO_DRAW_CALLS_IN_FRAME)}

func _observe() -> void:
	observed = true
	for view in ["top", "side"]:
		var after := _snapshot(view, "after")
		var before: Dictionary = baseline[view]
		views[view] = {"before_draw_calls": before["draw_calls"], "after_draw_calls": after["draw_calls"],
			"before_nonuniform_pixels": before["nonuniform_pixels"], "after_nonuniform_pixels": after["nonuniform_pixels"],
			"rgba_equal": before["bytes"] == after["bytes"], "before_sha256": before["sha256"], "after_sha256": after["sha256"],
			"width": after["width"], "height": after["height"], "before_path": before["path"], "after_path": after["path"],
			"saved": before["saved"] and after["saved"]}
	baseline.clear()
	if baseline_only:
		return
	# stage4 only advertised readiness. Actual image readbacks above now permit
	# the test to resume its original no-future-draw owner close sequence.
	RenderingServer.call_on_render_thread(Callable(fixture, "finish_viewport_observation"))

func _finish(result: Dictionary) -> void:
	var passed: bool = result["pass"] and observed
	for view in views.values():
		passed = passed and view["before_nonuniform_pixels"] > 0 and view["after_nonuniform_pixels"] > 0 and view["rgba_equal"] and view["saved"] and view["before_draw_calls"] == 2 and view["after_draw_calls"] == 2 and view["width"] == 128 and view["height"] == 128
	result["views"] = views
	result["pair_membership"] = "not-observed"
	result["external_scene_connection"] = "not-present"
	result["pass"] = passed
	viewport.free()
	viewport = null
	print("SCENE_VIEWPORT_INVARIANCE_NATIVE ", JSON.stringify(result))
	quit(0 if passed else 1)

func _process(_delta: float) -> bool:
	if fixture == null or viewport == null:
		return false
	if RenderingServer.is_render_loop_enabled():
		RenderingServer.force_draw(false, _delta)
	RenderingServer.call_on_render_thread(Callable(fixture, "advance"))
	var result: Dictionary = fixture.call("result")
	if result.get("viewport_ready", false) and not observed:
		_observe()
	if result.has("pass"):
		result["mode"] = "managed"
		_finish(result)
	elif Time.get_ticks_msec() - started > 30000:
		print("SCENE_VIEWPORT_INVARIANCE_NATIVE_TIMEOUT ", JSON.stringify(result))
		quit(1)
	return false
