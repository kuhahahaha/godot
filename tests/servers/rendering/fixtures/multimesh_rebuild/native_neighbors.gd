extends SceneTree

# Frozen 96x96 masks: orthographic width/height 6, with centers at
# world x = -2, 0, +2. All samples are well inside 1.2-unit quads.
const RED_MASK: Array[Vector2i] = [Vector2i(15, 47), Vector2i(16, 47), Vector2i(17, 47), Vector2i(15, 48), Vector2i(16, 48), Vector2i(17, 48), Vector2i(15, 49), Vector2i(16, 49), Vector2i(17, 49)]
const GREEN_MASK: Array[Vector2i] = [Vector2i(47, 47), Vector2i(48, 47), Vector2i(49, 47), Vector2i(47, 48), Vector2i(48, 48), Vector2i(49, 48), Vector2i(47, 49), Vector2i(48, 49), Vector2i(49, 49)]
const BLUE_MASK: Array[Vector2i] = [Vector2i(79, 47), Vector2i(80, 47), Vector2i(81, 47), Vector2i(79, 48), Vector2i(80, 48), Vector2i(81, 48), Vector2i(79, 49), Vector2i(80, 49), Vector2i(81, 49)]
const BLACK_MASK: Array[Vector2i] = [Vector2i(3, 3), Vector2i(92, 92)]
const CHANNEL_TOLERANCE := 1.0 / 255.0

func _initialize() -> void:
	call_deferred("_run")

func _quad(color: Color) -> QuadMesh:
	var material := StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.cull_mode = BaseMaterial3D.CULL_DISABLED
	material.albedo_color = color
	var mesh := QuadMesh.new()
	mesh.size = Vector2(1.2, 1.2)
	mesh.material = material
	return mesh

func _wait_frames(count: int) -> void:
	for i in range(count):
		await RenderingServer.frame_post_draw

func _set_transforms(multimesh: MultiMesh, count: int) -> void:
	multimesh.set_instance_transform(0, Transform3D.IDENTITY)
	for i in range(1, count):
		multimesh.set_instance_transform(i, Transform3D(Basis.IDENTITY, Vector3(20.0 + 4.0 * i, 0.0, 0.0)))

func _sample(stage: String, label: String, image: Image, mask: Array[Vector2i], expected: Color) -> bool:
	var ok := true
	for point in mask:
		var actual := image.get_pixelv(point)
		var matches := absf(actual.r - expected.r) <= CHANNEL_TOLERANCE and absf(actual.g - expected.g) <= CHANNEL_TOLERANCE and absf(actual.b - expected.b) <= CHANNEL_TOLERANCE
		print("K02_NEIGHBOR_SAMPLE ", JSON.stringify({"stage": stage, "sample": label, "xy": [point.x, point.y], "expected_rgb": [expected.r, expected.g, expected.b], "actual_rgb": [actual.r, actual.g, actual.b], "match": matches}))
		ok = ok and matches
	return ok

func _check_stage(stage: String, viewport: SubViewport, green: Color) -> bool:
	var image := viewport.get_texture().get_image()
	if image == null or image.is_empty() or image.get_size() != Vector2i(96, 96):
		print("K02_NEIGHBOR_STAGE ", JSON.stringify({"stage": stage, "image_valid": false, "pass": false}))
		return false
	var artifact_dir := OS.get_environment("GODOT_TEST_K02_NEIGHBOR_OUT")
	if not artifact_dir.is_empty():
		var save_error := image.save_png(artifact_dir.path_join(stage + ".png"))
		if save_error != OK:
			print("K02_NEIGHBOR_STAGE ", JSON.stringify({"stage": stage, "artifact_error": save_error, "pass": false}))
			return false
	var red_ok := _sample(stage, "mesh_red", image, RED_MASK, Color(1, 0, 0))
	var green_ok := _sample(stage, "multimesh", image, GREEN_MASK, green)
	var blue_ok := _sample(stage, "gpu_particles_blue", image, BLUE_MASK, Color(0, 0, 1))
	var black_ok := _sample(stage, "background", image, BLACK_MASK, Color(0, 0, 0))
	var ok := red_ok and green_ok and blue_ok and black_ok
	print("K02_NEIGHBOR_STAGE ", JSON.stringify({"stage": stage, "image_valid": true, "mesh_red": red_ok, "multimesh": green_ok, "gpu_particles_blue": blue_ok, "background": black_ok, "pass": ok}))
	return ok

func _run() -> void:
	if RenderingServer.get_rendering_device() == null:
		print("K02_NEIGHBORS_SKIP=77 main RenderingDevice unavailable")
		quit(77)
		return

	var viewport := SubViewport.new()
	viewport.size = Vector2i(96, 96)
	viewport.own_world_3d = true
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	viewport.transparent_bg = false
	viewport.msaa_3d = Viewport.MSAA_DISABLED
	viewport.screen_space_aa = Viewport.SCREEN_SPACE_AA_DISABLED
	viewport.use_taa = false
	root.add_child(viewport)

	var environment := Environment.new()
	environment.background_mode = Environment.BG_COLOR
	environment.background_color = Color(0, 0, 0)
	environment.tonemap_mode = Environment.TONE_MAPPER_LINEAR
	environment.glow_enabled = false
	var world_environment := WorldEnvironment.new()
	world_environment.environment = environment
	viewport.add_child(world_environment)

	var camera := Camera3D.new()
	camera.projection = Camera3D.PROJECTION_ORTHOGONAL
	camera.size = 6.0
	camera.position = Vector3(0, 0, 5)
	viewport.add_child(camera)
	camera.make_current()

	var red := MeshInstance3D.new()
	red.mesh = _quad(Color(1, 0, 0))
	red.position = Vector3(-2, 0, 0)
	viewport.add_child(red)

	var mm := MultiMesh.new()
	mm.transform_format = MultiMesh.TRANSFORM_3D
	mm.mesh = _quad(Color(0, 1, 0))
	mm.instance_count = 3
	_set_transforms(mm, 3)
	var green := MultiMeshInstance3D.new()
	green.multimesh = mm
	viewport.add_child(green)

	var process := ParticleProcessMaterial.new()
	process.emission_shape = ParticleProcessMaterial.EMISSION_SHAPE_POINT
	process.gravity = Vector3.ZERO
	process.initial_velocity_min = 0.0
	process.initial_velocity_max = 0.0
	process.scale_min = 1.0
	process.scale_max = 1.0
	var blue := GPUParticles3D.new()
	blue.amount = 1
	blue.lifetime = 60.0
	blue.one_shot = true
	blue.explosiveness = 1.0
	blue.preprocess = 0.3
	blue.use_fixed_seed = true
	blue.seed = 7
	blue.local_coords = true
	blue.process_material = process
	blue.draw_pass_1 = _quad(Color(0, 0, 1))
	blue.visibility_aabb = AABB(Vector3(-1, -1, -1), Vector3(2, 2, 2))
	blue.position = Vector3(2, 0, 0)
	viewport.add_child(blue)

	await _wait_frames(8)
	var all_visible := _check_stage("all_visible", viewport, Color(0, 1, 0))
	mm.visible_instance_count = 0
	await _wait_frames(3)
	var hidden := _check_stage("visible_0", viewport, Color(0, 0, 0))
	mm.visible_instance_count = -1
	await _wait_frames(3)
	var restored := _check_stage("visible_minus_1", viewport, Color(0, 1, 0))
	mm.instance_count = 5
	_set_transforms(mm, 5)
	await _wait_frames(3)
	var resized := _check_stage("same_mesh_resize_3_to_5", viewport, Color(0, 1, 0))
	var passed := all_visible and hidden and restored and resized
	viewport.queue_free()
	await process_frame
	await RenderingServer.frame_post_draw
	print("K02_NEIGHBORS_RESULT ", JSON.stringify({"pass": passed, "all_visible": all_visible, "visible_0": hidden, "visible_minus_1": restored, "same_mesh_resize_3_to_5": resized, "scope": "ordinary_pixels_only"}))
	quit(0 if passed else 1)
