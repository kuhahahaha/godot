extends SceneTree

# Orthographic near1/far9, camera z5: native reversed depths at z0/+1/-1
# are exactly .5/.625/.375. Color readback is sRGB, tolerance one byte.
const DEPTH_CENTERS: Array[Vector2i] = [Vector2i(16, 48), Vector2i(48, 48), Vector2i(80, 48)]
const SHADOW_CENTERS: Array[Vector2i] = [Vector2i(25, 48), Vector2i(57, 48), Vector2i(89, 48)]
const LABELS: Array[String] = ["mesh", "multimesh", "gpu_particles"]
const TOLERANCE := 1.0 / 255.0
const WITNESS_COLORS: Array[Color] = [Color.MAGENTA, Color.CYAN, Color.YELLOW, Color.RED, Color.GREEN, Color.BLUE]
var checks := 0
var failures := 0
var witness_checks := 0
var witness_failures := 0
var witness_rect: ColorRect

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
	print("K02_DEPTH_SHADOW_WITNESS_FRAME ", JSON.stringify({"stage": stage, "ordinal": ordinal, "frame_polls": polls, "fresh": fresh}))
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
			print("K02_DEPTH_SHADOW_WITNESS ", JSON.stringify({"stage": stage, "ordinal": ordinal, "xy": [point.x, point.y], "actual": [actual.r, actual.g, actual.b], "pass": passed}))
	return image

func _quad(size: float) -> QuadMesh:
	var material := StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.cull_mode = BaseMaterial3D.CULL_DISABLED
	var mesh := QuadMesh.new()
	mesh.size = Vector2(size, size)
	mesh.material = material
	return mesh

func _set_transforms(mm: MultiMesh, count: int) -> void:
	mm.set_instance_transform(0, Transform3D.IDENTITY)
	for i in range(1, count):
		mm.set_instance_transform(i, Transform3D(Basis.IDENTITY, Vector3(20.0 + i * 4.0, 0, 0)))

func _image(stage: String, viewport: SubViewport, ordinal: int) -> Image:
	var image: Image = await _witness_image(stage, viewport, ordinal)
	if image == null or image.get_size() != Vector2i(96, 96):
		failures += 1
		print("K02_DEPTH_SHADOW_IMAGE_INVALID ", stage)
		return null
	var directory := OS.get_environment("GODOT_TEST_K02_DEPTH_SHADOW_OUT")
	if not directory.is_empty():
		var error := image.save_png(directory.path_join(stage + ".png"))
		if error != OK:
			failures += 1
			print("K02_DEPTH_SHADOW_IMAGE_WRITE_FAILED ", stage, " ", error)
			return null
	return image

func _samples(stage: String, label: String, image: Image, center: Vector2i, expected: Color) -> void:
	for y in range(-1, 2):
		for x in range(-1, 2):
			var point := center + Vector2i(x, y)
			var actual := image.get_pixelv(point)
			var ok := absf(actual.r - expected.r) <= TOLERANCE and absf(actual.g - expected.g) <= TOLERANCE and absf(actual.b - expected.b) <= TOLERANCE
			checks += 1
			if not ok:
				failures += 1
			print("K02_DEPTH_SHADOW_SAMPLE ", JSON.stringify({"stage": stage, "label": label, "xy": [point.x, point.y], "expected": [expected.r, expected.g, expected.b], "actual": [actual.r, actual.g, actual.b], "pass": ok}))

func _depth_stage(stage: String, viewport: SubViewport, mm_depth: float, ordinal: int) -> void:
	var image: Image = await _image(stage, viewport, ordinal)
	if image == null:
		return
	var depths: Array[float] = [0.5, mm_depth, 0.375]
	for i in range(3):
		_samples(stage, LABELS[i], image, DEPTH_CENTERS[i], Color(depths[i], depths[i], depths[i]).linear_to_srgb())
	_samples(stage, "background", image, Vector2i(5, 5), Color.BLACK)
	print("K02_DEPTH_SHADOW_STAGE ", stage)

func _shadow_stage(stage: String, viewport: SubViewport, mm_shadow: bool, ordinal: int) -> void:
	var image: Image = await _image(stage, viewport, ordinal)
	if image == null:
		return
	for i in range(3):
		_samples(stage, LABELS[i], image, SHADOW_CENTERS[i], Color.WHITE if i == 1 and not mm_shadow else Color.BLACK)
	_samples(stage, "lit_receiver", image, Vector2i(5, 5), Color.WHITE)
	print("K02_DEPTH_SHADOW_STAGE ", stage)

func _run() -> void:
	if RenderingServer.get_rendering_device() == null or Engine.is_editor_hint():
		print("K02_DEPTH_SHADOW_SKIP=77 main RD or non-editor context unavailable")
		quit(77)
		return
	var omit_depth := OS.get_cmdline_user_args().has("--omit-multimesh-depth")
	var omit_shadow := OS.get_cmdline_user_args().has("--omit-multimesh-shadow")
	print("K02_DEPTH_SHADOW_HOST ", JSON.stringify({"editor_feature": OS.has_feature("editor"), "release_feature": OS.has_feature("template_release"), "double_precision": OS.has_feature("double"), "editor_hint": Engine.is_editor_hint(), "cache_enabled": ProjectSettings.get_setting("rendering/shader_compiler/shader_cache/enabled")}))
	print("K02_DEPTH_SHADOW_RUNTIME ", JSON.stringify({"main_rd": true, "editor_hint": Engine.is_editor_hint(), "cache_enabled": ProjectSettings.get_setting("rendering/shader_compiler/shader_cache/enabled"), "omit_depth": omit_depth, "omit_shadow": omit_shadow}))
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
	camera.near = 1.0
	camera.far = 9.0
	camera.position.z = 5.0
	viewport.add_child(camera)
	camera.make_current()
	var mesh := MeshInstance3D.new()
	mesh.mesh = _quad(0.5)
	mesh.position = Vector3(-2, 0, 0)
	viewport.add_child(mesh)
	var mm := MultiMesh.new()
	mm.transform_format = MultiMesh.TRANSFORM_3D
	mm.mesh = _quad(0.5)
	if omit_depth:
		mm.mesh.material.depth_draw_mode = BaseMaterial3D.DEPTH_DRAW_DISABLED
	mm.instance_count = 3
	_set_transforms(mm, 3)
	var multimesh := MultiMeshInstance3D.new()
	multimesh.multimesh = mm
	multimesh.position.z = 1.0
	viewport.add_child(multimesh)
	var process := ParticleProcessMaterial.new()
	process.emission_shape = ParticleProcessMaterial.EMISSION_SHAPE_POINT
	process.gravity = Vector3.ZERO
	process.initial_velocity_min = 0.0
	process.initial_velocity_max = 0.0
	var particles := GPUParticles3D.new()
	particles.amount = 1
	particles.lifetime = 60.0
	particles.one_shot = true
	particles.explosiveness = 1.0
	particles.preprocess = 0.3
	particles.use_fixed_seed = true
	particles.seed = 7
	particles.local_coords = true
	particles.process_material = process
	particles.draw_pass_1 = _quad(0.5)
	particles.visibility_aabb = AABB(Vector3(-1, -1, -1), Vector3(2, 2, 2))
	particles.position = Vector3(2, 0, -1)
	viewport.add_child(particles)
	var depth_shader := Shader.new()
	depth_shader.code = "shader_type spatial; render_mode unshaded, cull_disabled, depth_draw_never, depth_test_disabled, fog_disabled; uniform sampler2D depth_texture : hint_depth_texture, filter_nearest; void vertex() { POSITION = vec4(VERTEX.xy, 1.0, 1.0); } void fragment() { ALBEDO = vec3(texture(depth_texture, SCREEN_UV).r); ALPHA = 1.0; }"
	var depth_material := ShaderMaterial.new()
	depth_material.shader = depth_shader
	var probe := MeshInstance3D.new()
	probe.mesh = _quad(2.0)
	probe.material_override = depth_material
	probe.extra_cull_margin = 100.0
	probe.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	viewport.add_child(probe)
	await _wait_frames(8)
	await _depth_stage("depth_all", viewport, 0.625, 0)
	mm.visible_instance_count = 0
	await _wait_frames(3)
	await _depth_stage("depth_visible_0", viewport, 0.0, 1)
	mm.visible_instance_count = -1
	mm.instance_count = 5
	_set_transforms(mm, 5)
	await _wait_frames(3)
	await _depth_stage("depth_restored_resize", viewport, 0.625, 2)
	probe.visible = false
	mm.mesh.material.depth_draw_mode = BaseMaterial3D.DEPTH_DRAW_OPAQUE_ONLY
	for item in [mesh, multimesh, particles]:
		item.position.z = 0.6
		item.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_SHADOWS_ONLY
	if omit_shadow:
		multimesh.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	var receiver_shader := Shader.new()
	receiver_shader.code = "shader_type spatial; render_mode ambient_light_disabled, specular_disabled; void fragment() { ALBEDO = vec3(1.0); } void light() { DIFFUSE_LIGHT += vec3(1.0) * ATTENUATION; }"
	var receiver_material := ShaderMaterial.new()
	receiver_material.shader = receiver_shader
	var receiver := MeshInstance3D.new()
	receiver.mesh = _quad(6.0)
	receiver.material_override = receiver_material
	receiver.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	viewport.add_child(receiver)
	var light := DirectionalLight3D.new()
	light.rotation.y = -PI / 4.0
	light.shadow_enabled = true
	light.shadow_bias = 0.0
	light.shadow_normal_bias = 0.0
	light.directional_shadow_mode = DirectionalLight3D.SHADOW_ORTHOGONAL
	light.directional_shadow_max_distance = 12.0
	viewport.add_child(light)
	await _wait_frames(5)
	await _shadow_stage("shadow_all", viewport, true, 3)
	mm.visible_instance_count = 0
	await _wait_frames(3)
	await _shadow_stage("shadow_visible_0", viewport, false, 4)
	mm.visible_instance_count = -1
	await _wait_frames(3)
	await _shadow_stage("shadow_restored", viewport, true, 5)
	viewport.queue_free()
	await process_frame
	await RenderingServer.frame_post_draw
	var passed := failures == 0 and witness_failures == 0 and witness_checks == 54
	print("K02_DEPTH_SHADOW_WITNESS_RESULT ", JSON.stringify({"stages": 6, "checks": witness_checks, "failures": witness_failures, "pass": witness_failures == 0 and witness_checks == 54}))
	print("K02_DEPTH_SHADOW_RESULT ", JSON.stringify({"checks": checks, "failures": failures, "stages": 6, "pass": passed, "scope": "ordinary_depth_and_shadow_pixels"}))
	quit(0 if passed else 1)
