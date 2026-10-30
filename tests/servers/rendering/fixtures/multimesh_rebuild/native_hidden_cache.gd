extends SceneTree

signal buffers_read

const INSTANCE_COUNT := 1025
const HIDDEN_A := 700
const HIDDEN_B := 1024

class MotionDemand extends CompositorEffect:
	var callbacks := 0

	func _init() -> void:
		effect_callback_type = EFFECT_CALLBACK_TYPE_POST_TRANSPARENT
		needs_motion_vectors = true

	func _render_callback(_callback_type: int, _render_data: RenderData) -> void:
		callbacks += 1

var demand: MotionDemand
var tested_mm: RID
var gpu := PackedByteArray()
var cpu := PackedByteArray()
var checks := 0
var failures := 0
var output := ""
var no_motion := false
var with_fields := false
var omit_second := false

func _initialize() -> void:
	call_deferred("_run")

func _position(x: float, y: float) -> Transform3D:
	return Transform3D(Basis.IDENTITY, Vector3(x, y, 0))

func _initial_data() -> PackedFloat32Array:
	var stride := 20 if with_fields else 12
	var data := PackedFloat32Array()
	data.resize(INSTANCE_COUNT * stride)
	for i in range(INSTANCE_COUNT):
		data[i * stride] = 1
		data[i * stride + 5] = 1
		data[i * stride + 10] = 1
		data[i * stride + 3] = -2 if i == 0 else 20 + i
		data[i * stride + 7] = 1 if i == 0 else 0
		if with_fields:
			for field in range(4):
				data[i * stride + 12 + field] = 1
				data[i * stride + 16 + field] = 0.25
	return data

func _capture() -> void:
	var rd := RenderingServer.get_rendering_device()
	var buffer := RenderingServer.multimesh_get_buffer_rd_rid(tested_mm)
	var bytes := PackedByteArray()
	if rd != null and buffer.is_valid():
		bytes = rd.buffer_get_data(buffer)
	var current := RenderingServer.multimesh_get_buffer(tested_mm).to_byte_array()
	call_deferred("_receive", bytes, current)

func _receive(native: PackedByteArray, current: PackedByteArray) -> void:
	gpu = native
	cpu = current
	buffers_read.emit()

func _sample(stage: String, label: String, image: Image, center: Vector2i, expected: Color) -> void:
	for y in range(-1, 2):
		for x in range(-1, 2):
			var point := center + Vector2i(x, y)
			var actual := image.get_pixelv(point)
			var ok := absf(actual.r - expected.r) <= 1.0 / 255.0 and absf(actual.g - expected.g) <= 1.0 / 255.0 and absf(actual.b - expected.b) <= 1.0 / 255.0
			checks += 1
			if not ok:
				failures += 1
			print("K02_HIDDEN_SAMPLE ", JSON.stringify({"stage": stage, "label": label, "xy": [point.x, point.y], "expected": [expected.r, expected.g, expected.b], "actual": [actual.r, actual.g, actual.b], "pass": ok}))

func _stage(stage: String, viewport: SubViewport, first_x: int, reveal: bool) -> void:
	var before := demand.callbacks
	viewport.render_target_update_mode = SubViewport.UPDATE_ONCE
	for i in range(12):
		await RenderingServer.frame_post_draw
		if demand.callbacks > before:
			break
	if demand.callbacks != before + 1:
		failures += 1
		print("K02_HIDDEN_FRAME_INVALID ", stage)
		return
	RenderingServer.call_on_render_thread(_capture)
	await buffers_read
	var size := INSTANCE_COUNT * (20 if with_fields else 12) * 4
	if cpu.size() != size or gpu.size() != size * (1 if no_motion else 2):
		failures += 1
		print("K02_HIDDEN_BUFFER_INVALID ", stage, " ", cpu.size(), " ", gpu.size())
		return
	var image := viewport.get_texture().get_image()
	if image == null or image.get_size() != Vector2i(64, 64):
		failures += 1
		print("K02_HIDDEN_IMAGE_INVALID ", stage)
		return
	if not output.is_empty():
		if image.save_png(output.path_join(stage + ".png")) != OK:
			failures += 1
			return
		for suffix in ["gpu", "cpu"]:
			var file := FileAccess.open(output.path_join(stage + "." + suffix), FileAccess.WRITE)
			if file == null:
				failures += 1
				return
			file.store_buffer(gpu if suffix == "gpu" else cpu)
	_sample(stage, "first_old", image, Vector2i(16, 24), Color.GREEN if first_x == 16 else Color.BLACK)
	_sample(stage, "first_new", image, Vector2i(24, 24), Color.GREEN if first_x == 24 else Color.BLACK)
	_sample(stage, "hidden_a", image, Vector2i(40, 32), (Color.RED if with_fields else Color.GREEN) if reveal else Color.BLACK)
	_sample(stage, "hidden_b", image, Vector2i(48, 40), (Color.BLUE if with_fields else Color.GREEN) if reveal else Color.BLACK)
	_sample(stage, "background", image, Vector2i(5, 5), Color.BLACK)
	var stride := 20 if with_fields else 12
	print("K02_HIDDEN_STAGE ", JSON.stringify({"stage": stage, "first_x": first_x, "reveal": reveal, "callback_delta": demand.callbacks - before, "cpu_bytes": cpu.size(), "gpu_bytes": gpu.size(), "cached_a_x": cpu.decode_float(HIDDEN_A * stride * 4 + 12), "cached_b_x": cpu.decode_float(HIDDEN_B * stride * 4 + 12)}))

func _run() -> void:
	if RenderingServer.get_rendering_device() == null or RenderingServer.get_current_rendering_method() != "forward_plus":
		print("K02_HIDDEN_UNAVAILABLE Forward+ main RD required")
		quit(77)
		return
	no_motion = OS.get_cmdline_user_args().has("--no-motion")
	with_fields = OS.get_cmdline_user_args().has("--with-fields")
	omit_second = OS.get_cmdline_user_args().has("--omit-second-edit")
	output = OS.get_environment("GODOT_TEST_K02_HIDDEN_OUT")
	print("K02_HIDDEN_RUNTIME ", JSON.stringify({"no_motion": no_motion, "with_fields": with_fields, "omit_second": omit_second, "cache_enabled": ProjectSettings.get_setting("rendering/shader_compiler/shader_cache/enabled"), "editor_hint": Engine.is_editor_hint()}))
	var viewport := SubViewport.new()
	viewport.size = Vector2i(64, 64)
	viewport.own_world_3d = true
	viewport.render_target_update_mode = SubViewport.UPDATE_DISABLED
	viewport.use_taa = false
	viewport.msaa_3d = Viewport.MSAA_DISABLED
	root.add_child(viewport)
	var environment := Environment.new()
	environment.background_mode = Environment.BG_COLOR
	environment.background_color = Color.BLACK
	environment.tonemap_mode = Environment.TONE_MAPPER_LINEAR
	var world := WorldEnvironment.new()
	world.environment = environment
	demand = MotionDemand.new()
	demand.needs_motion_vectors = not no_motion
	var compositor := Compositor.new()
	compositor.compositor_effects = [demand]
	world.compositor = compositor
	viewport.add_child(world)
	var camera := Camera3D.new()
	camera.projection = Camera3D.PROJECTION_ORTHOGONAL
	camera.size = 8
	camera.position.z = 5
	viewport.add_child(camera)
	camera.make_current()
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	for i in range(6):
		await RenderingServer.frame_post_draw
	viewport.render_target_update_mode = SubViewport.UPDATE_DISABLED
	var material := StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.albedo_color = Color.WHITE if with_fields else Color.GREEN
	material.vertex_color_use_as_albedo = with_fields
	var mesh := QuadMesh.new()
	mesh.size = Vector2(0.75, 0.75)
	mesh.material = material
	var mm := MultiMesh.new()
	mm.transform_format = MultiMesh.TRANSFORM_3D
	mm.use_colors = with_fields
	mm.use_custom_data = with_fields
	mm.mesh = mesh
	mm.instance_count = INSTANCE_COUNT
	mm.custom_aabb = AABB(Vector3(-3, -2, -1), Vector3(1050, 4, 2))
	mm.buffer = _initial_data()
	if with_fields:
		mm.set_instance_color(0, Color.GREEN)
	mm.visible_instance_count = 1
	tested_mm = mm.get_rid()
	var instance := MultiMeshInstance3D.new()
	instance.multimesh = mm
	viewport.add_child(instance)
	await _stage("initial", viewport, 16, false)
	mm.set_instance_transform(HIDDEN_A, _position(1, 0))
	if with_fields:
		mm.set_instance_color(HIDDEN_A, Color.RED)
		mm.set_instance_custom_data(HIDDEN_A, Color(0.5, 0.25, 0.75, 1))
	await _stage("hidden_a", viewport, 16, false)
	if not omit_second:
		mm.set_instance_transform(HIDDEN_B, _position(2, -1))
		if with_fields:
			mm.set_instance_color(HIDDEN_B, Color.BLUE)
			mm.set_instance_custom_data(HIDDEN_B, Color(0.75, 0.5, 0.25, 1))
	await _stage("hidden_b", viewport, 16, false)
	mm.set_instance_transform(0, _position(-1, 1))
	await _stage("visible_move", viewport, 24, false)
	mm.visible_instance_count = -1
	await _stage("reveal", viewport, 24, true)
	await _stage("hold", viewport, 24, true)
	viewport.queue_free()
	for i in range(2):
		await process_frame
	print("K02_HIDDEN_RESULT ", JSON.stringify({"stages": 6, "checks": checks, "failures": failures, "pass": failures == 0, "scope": "ordinary_hidden_cached_instance_carry"}))
	quit(0 if failures == 0 else 1)
