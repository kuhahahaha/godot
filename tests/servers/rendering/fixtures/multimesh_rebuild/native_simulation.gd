extends SceneTree

# 96x96 orthographic width6: 16px/world unit. Native fixed step .25 at
# velocity2 moves .5 world (8px). ParticleProcessMaterial integrates its
# initial clear step too; these locations are fixed before rendering.
const CENTERS: Array[int] = [24, 32, 40, 48, 56]
const TOLERANCE := 1.0 / 255.0
const WITNESS_COLORS: Array[Color] = [Color.MAGENTA, Color.CYAN, Color.YELLOW, Color.RED, Color.GREEN, Color.BLUE]
var checks := 0
var failures := 0
var witness_checks := 0
var witness_failures := 0
var witness_rect: ColorRect
var profile := "normal"

func _initialize() -> void:
	call_deferred("_run")

func _wait_frames(count: int) -> void:
	for i in range(count):
		await RenderingServer.frame_post_draw

func _make_witness(viewport: SubViewport) -> void:
	var layer := CanvasLayer.new()
	layer.layer = 128
	viewport.add_child(layer)
	witness_rect = ColorRect.new()
	witness_rect.position = Vector2(2, 88)
	witness_rect.size = Vector2(4, 4)
	witness_rect.color = Color.BLACK
	witness_rect.mouse_filter = Control.MOUSE_FILTER_IGNORE
	layer.add_child(witness_rect)

func _witness_image(stage: String, viewport: SubViewport, ordinal: int) -> Image:
	var expected: Color = WITNESS_COLORS[ordinal]
	witness_rect.color = expected
	var image: Image
	var polls := 0
	var fresh := false
	for attempt in range(12):
		await RenderingServer.frame_post_draw
		polls += 1
		image = viewport.get_texture().get_image()
		if image != null and image.get_size() == Vector2i(96, 96):
			var actual := image.get_pixel(4, 90)
			fresh = absf(actual.r - expected.r) <= TOLERANCE and absf(actual.g - expected.g) <= TOLERANCE and absf(actual.b - expected.b) <= TOLERANCE
			if fresh:
				break
	print("K02_SIMULATION_WITNESS_FRAME ", JSON.stringify({"stage": stage, "ordinal": ordinal, "frame_polls": polls, "fresh": fresh}))
	if image == null or image.get_size() != Vector2i(96, 96):
		witness_failures += 1
		return null
	for y in range(-1, 2):
		for x in range(-1, 2):
			var point := Vector2i(4 + x, 90 + y)
			var actual := image.get_pixelv(point)
			var passed := absf(actual.r - expected.r) <= TOLERANCE and absf(actual.g - expected.g) <= TOLERANCE and absf(actual.b - expected.b) <= TOLERANCE
			witness_checks += 1
			if not passed:
				witness_failures += 1
			print("K02_SIMULATION_WITNESS ", JSON.stringify({"stage": stage, "ordinal": ordinal, "xy": [point.x, point.y], "actual": [actual.r, actual.g, actual.b], "pass": passed}))
	return image

func _quad(color: Color) -> QuadMesh:
	var material := StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.cull_mode = BaseMaterial3D.CULL_DISABLED
	material.albedo_color = color
	var mesh := QuadMesh.new()
	mesh.size = Vector2(0.5, 0.5)
	mesh.material = material
	return mesh

func _set_transforms(mm: MultiMesh, count: int) -> void:
	mm.set_instance_transform(0, Transform3D.IDENTITY)
	for i in range(1, count):
		mm.set_instance_transform(i, Transform3D(Basis.IDENTITY, Vector3(20.0 + i * 4.0, 0, 0)))

func _sample(stage: String, label: String, image: Image, center: Vector2i, expected: Color) -> void:
	for y in range(-1, 2):
		for x in range(-1, 2):
			var point := center + Vector2i(x, y)
			var actual := image.get_pixelv(point)
			var ok := absf(actual.r - expected.r) <= TOLERANCE and absf(actual.g - expected.g) <= TOLERANCE and absf(actual.b - expected.b) <= TOLERANCE
			checks += 1
			if not ok:
				failures += 1
			print("K02_SIMULATION_SAMPLE ", JSON.stringify({"stage": stage, "label": label, "xy": [point.x, point.y], "expected": [expected.r, expected.g, expected.b], "actual": [actual.r, actual.g, actual.b], "pass": ok}))

func _stage(stage: String, viewport: SubViewport, center_x: int, ordinal: int) -> void:
	var image: Image = await _witness_image(stage, viewport, ordinal)
	if image == null or image.get_size() != Vector2i(96, 96):
		failures += 1
		print("K02_SIMULATION_IMAGE_INVALID ", stage)
		return
	var directory := OS.get_environment("GODOT_TEST_K02_SIMULATION_OUT")
	if not directory.is_empty():
		var error := image.save_png(directory.path_join(stage + ".png"))
		if error != OK:
			failures += 1
			print("K02_SIMULATION_IMAGE_WRITE_FAILED ", stage, " ", error)
			return
	for x in CENTERS:
		_sample(stage, "particle_" + str(x), image, Vector2i(x, 24), Color.BLUE if x == center_x else Color.BLACK)
	_sample(stage, "mesh", image, Vector2i(16, 48), Color.RED)
	_sample(stage, "multimesh", image, Vector2i(48, 48), Color.GREEN)
	_sample(stage, "background", image, Vector2i(5, 5), Color.BLACK)
	print("K02_SIMULATION_STAGE ", JSON.stringify({"stage": stage, "expected_center_x": center_x}))

func _run() -> void:
	if RenderingServer.get_rendering_device() == null or Engine.is_editor_hint():
		print("K02_SIMULATION_SKIP=77 main RD or non-editor context unavailable")
		quit(77)
		return
	for candidate in ["fractional", "preprocess", "zero-velocity", "omit-second-request"]:
		if OS.get_cmdline_user_args().has("--" + candidate):
			profile = candidate
	print("K02_SIMULATION_HOST ", JSON.stringify({"editor_feature": OS.has_feature("editor"), "release_feature": OS.has_feature("template_release"), "double_precision": OS.has_feature("double"), "editor_hint": Engine.is_editor_hint(), "cache_enabled": ProjectSettings.get_setting("rendering/shader_compiler/shader_cache/enabled")}))
	print("K02_SIMULATION_RUNTIME ", JSON.stringify({"profile": profile, "main_rd": true, "editor_hint": Engine.is_editor_hint(), "cache_enabled": ProjectSettings.get_setting("rendering/shader_compiler/shader_cache/enabled")}))
	var viewport := SubViewport.new()
	viewport.size = Vector2i(96, 96)
	viewport.own_world_3d = true
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	viewport.msaa_3d = Viewport.MSAA_DISABLED
	viewport.msaa_2d = Viewport.MSAA_DISABLED
	viewport.screen_space_aa = Viewport.SCREEN_SPACE_AA_DISABLED
	viewport.use_taa = false
	root.add_child(viewport)
	_make_witness(viewport)
	var environment := Environment.new()
	environment.background_mode = Environment.BG_COLOR
	environment.background_color = Color.BLACK
	environment.tonemap_mode = Environment.TONE_MAPPER_LINEAR
	environment.glow_enabled = false
	var world_environment := WorldEnvironment.new()
	world_environment.environment = environment
	viewport.add_child(world_environment)
	var camera := Camera3D.new()
	camera.projection = Camera3D.PROJECTION_ORTHOGONAL
	camera.size = 6.0
	camera.position.z = 5.0
	viewport.add_child(camera)
	camera.make_current()
	var red := MeshInstance3D.new()
	red.mesh = _quad(Color.RED)
	red.position.x = -2.0
	viewport.add_child(red)
	var mm := MultiMesh.new()
	mm.transform_format = MultiMesh.TRANSFORM_3D
	mm.mesh = _quad(Color.GREEN)
	mm.instance_count = 3
	_set_transforms(mm, 3)
	var green := MultiMeshInstance3D.new()
	green.multimesh = mm
	viewport.add_child(green)
	var process := ParticleProcessMaterial.new()
	process.emission_shape = ParticleProcessMaterial.EMISSION_SHAPE_POINT
	process.direction = Vector3.RIGHT
	process.spread = 0.0
	process.gravity = Vector3.ZERO
	process.initial_velocity_min = 0.0 if profile == "zero-velocity" else 2.0
	process.initial_velocity_max = process.initial_velocity_min
	process.scale_min = 1.0
	process.scale_max = 1.0
	process.lifetime_randomness = 0.0
	process.inherit_velocity_ratio = 0.0
	process.turbulence_enabled = false
	process.collision_mode = ParticleProcessMaterial.COLLISION_DISABLED
	for parameter in [ParticleProcessMaterial.PARAM_ANGULAR_VELOCITY, ParticleProcessMaterial.PARAM_ORBIT_VELOCITY, ParticleProcessMaterial.PARAM_RADIAL_VELOCITY, ParticleProcessMaterial.PARAM_DIRECTIONAL_VELOCITY, ParticleProcessMaterial.PARAM_LINEAR_ACCEL, ParticleProcessMaterial.PARAM_RADIAL_ACCEL, ParticleProcessMaterial.PARAM_TANGENTIAL_ACCEL, ParticleProcessMaterial.PARAM_DAMPING]:
		process.set_param_min(parameter, 0.0)
		process.set_param_max(parameter, 0.0)
	process.set_particle_flag(ParticleProcessMaterial.PARTICLE_FLAG_DISABLE_Z, true)
	var particles := GPUParticles3D.new()
	particles.amount = 1
	particles.amount_ratio = 1.0
	particles.lifetime = 8.0
	particles.one_shot = false
	particles.explosiveness = 1.0
	particles.randomness = 0.0
	particles.preprocess = 0.5 if profile == "preprocess" else 0.0
	particles.fixed_fps = 4
	particles.speed_scale = 0.0
	particles.interpolate = false
	particles.fract_delta = profile == "fractional"
	particles.use_fixed_seed = true
	particles.seed = 7
	particles.local_coords = true
	particles.process_material = process
	particles.draw_pass_1 = _quad(Color.BLUE)
	particles.visibility_aabb = AABB(Vector3(-2, -1, -1), Vector3(5, 2, 2))
	particles.position = Vector3(-1.5, 1.5, 0)
	viewport.add_child(particles)
	var initial := 24 if profile == "zero-velocity" else 40 if profile == "preprocess" else 32
	var displacement := 0 if profile == "zero-velocity" else 8
	await _wait_frames(8)
	await _stage("initial", viewport, initial, 0)
	await _wait_frames(3)
	await _stage("paused_hold", viewport, initial, 1)
	particles.request_particles_process(0.25, 0.0)
	await _wait_frames(3)
	await _stage("requested_1", viewport, initial + displacement, 2)
	mm.instance_count = 5
	_set_transforms(mm, 5)
	if profile != "omit-second-request":
		particles.request_particles_process(0.25, 0.0)
	await _wait_frames(3)
	await _stage("requested_2_resize", viewport, initial + 2 * displacement, 3)
	viewport.queue_free()
	await process_frame
	await RenderingServer.frame_post_draw
	var passed := failures == 0 and witness_failures == 0 and witness_checks == 36
	print("K02_SIMULATION_WITNESS_RESULT ", JSON.stringify({"stages": 4, "checks": witness_checks, "failures": witness_failures, "pass": witness_failures == 0 and witness_checks == 36}))
	print("K02_SIMULATION_RESULT ", JSON.stringify({"profile": profile, "stages": 4, "checks": checks, "failures": failures, "pass": passed, "scope": "ordinary_planar_native_simulation_pixels"}))
	quit(0 if passed else 1)
