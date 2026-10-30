extends SceneTree

signal buffers_read

var mm: RID
var native := PackedByteArray()
var cached := PackedByteArray()
var command := PackedByteArray()
var checks := 0
var failures := 0
var structural_failures := 0
var output := ""
var omit_move := false
var capture_stage := ""
var witness: RID
var witness_checks := 0
const WITNESS_COLORS := [Color.MAGENTA, Color.CYAN, Color.YELLOW, Color.RED, Color.GREEN, Color.BLUE]

func _initialize() -> void:
	call_deferred("_run")

func _input_data(moved: bool) -> PackedFloat32Array:
	return PackedFloat32Array([1, 0, 0, 48 if moved else 24, 0, 1, 0, 32, 1, 0, 0, 72, 0, 1, 0, 32])

func _capture(stage: String) -> void:
	var rd := RenderingServer.get_rendering_device()
	var data := PackedByteArray()
	var words := PackedByteArray()
	if rd != null:
		var buffer := RenderingServer.multimesh_get_buffer_rd_rid(mm)
		var commands := RenderingServer.multimesh_get_command_buffer_rd_rid(mm)
		if buffer.is_valid():
			data = rd.buffer_get_data(buffer)
		if commands.is_valid():
			words = rd.buffer_get_data(commands)
	var cpu := RenderingServer.multimesh_get_buffer(mm).to_byte_array()
	call_deferred("_receive", data, cpu, words, stage)

func _receive(data: PackedByteArray, cpu: PackedByteArray, words: PackedByteArray, stage: String) -> void:
	capture_stage = stage
	native = data
	cached = cpu
	command = words
	buffers_read.emit()

func _sample(stage: String, label: String, image: Image, center: Vector2i, red: bool) -> void:
	var expected := Color.RED if red else Color.BLACK
	for y in range(-1, 2):
		for x in range(-1, 2):
			var point := center + Vector2i(x, y)
			var actual := image.get_pixelv(point)
			var passed := absf(actual.r - expected.r) <= 1.0 / 255.0 and absf(actual.g - expected.g) <= 1.0 / 255.0 and absf(actual.b - expected.b) <= 1.0 / 255.0
			checks += 1
			if not passed:
				failures += 1
			print("K02_LAYOUT_SAMPLE ", JSON.stringify({"stage": stage, "label": label, "xy": [point.x, point.y], "red": red, "actual": [actual.r, actual.g, actual.b], "pass": passed}))

func _stage(stage: String, viewport: SubViewport, count: int, moved: bool, ordinal: int) -> void:
	var expected_witness: Color = WITNESS_COLORS[ordinal]
	RenderingServer.canvas_item_set_modulate(witness, expected_witness)
	viewport.render_target_update_mode = SubViewport.UPDATE_ONCE
	var image: Image
	var polls := 0
	var fresh := false
	for attempt in range(12):
		await RenderingServer.frame_post_draw
		polls += 1
		image = viewport.get_texture().get_image()
		if image != null and image.get_size() == Vector2i(96, 64):
			var marker := image.get_pixel(4, 60)
			fresh = absf(marker.r - expected_witness.r) <= 1.0 / 255.0 and absf(marker.g - expected_witness.g) <= 1.0 / 255.0 and absf(marker.b - expected_witness.b) <= 1.0 / 255.0
			if fresh:
				break
	RenderingServer.call_on_render_thread(_capture.bind(stage))
	await buffers_read
	var expected := _input_data(moved and not omit_move).to_byte_array()
	var words: Array[int] = []
	for offset in range(0, command.size(), 4):
		words.append(command.decode_u32(offset))
	var valid := capture_stage == stage and native == expected and cached == expected and words == [6, count, 0, 0, 0]
	var image_valid := image != null and image.get_size() == Vector2i(96, 64)
	if not valid or not image_valid or not fresh:
		structural_failures += 1
	print("K02_LAYOUT_STAGE ", JSON.stringify({"stage": stage, "count": count, "moved": moved, "native_bytes": native.size(), "cached_bytes": cached.size(), "command": words, "data_pass": valid, "image_valid": image_valid, "witness_ordinal": ordinal, "witness_fresh": fresh, "frame_polls": polls}))
	if not image_valid:
		return
	if not output.is_empty():
		if image.save_png(output.path_join(stage + ".png")) != OK:
			structural_failures += 1
		for suffix in ["gpu", "cpu", "command"]:
			var file := FileAccess.open(output.path_join(stage + "." + suffix), FileAccess.WRITE)
			if file == null:
				structural_failures += 1
				continue
			file.store_buffer(command if suffix == "command" else (native if suffix == "gpu" else cached))
	for y in range(-1, 2):
		for x in range(-1, 2):
			var point := Vector2i(4 + x, 60 + y)
			var actual := image.get_pixelv(point)
			var passed := absf(actual.r - expected_witness.r) <= 1.0 / 255.0 and absf(actual.g - expected_witness.g) <= 1.0 / 255.0 and absf(actual.b - expected_witness.b) <= 1.0 / 255.0
			witness_checks += 1
			if not passed:
				structural_failures += 1
			print("K02_LAYOUT_WITNESS ", JSON.stringify({"stage": stage, "ordinal": ordinal, "xy": [point.x, point.y], "actual": [actual.r, actual.g, actual.b], "pass": passed}))
	_sample(stage, "old", image, Vector2i(24, 32), count > 0 and not moved)
	_sample(stage, "new", image, Vector2i(48, 32), count > 0 and moved)
	_sample(stage, "second", image, Vector2i(72, 32), count == 2)
	_sample(stage, "background_a", image, Vector2i(8, 8), false)
	_sample(stage, "background_b", image, Vector2i(88, 56), false)

func _run() -> void:
	print("K02_LAYOUT_HOST ", JSON.stringify({"editor_feature": OS.has_feature("editor"), "release_feature": OS.has_feature("template_release"), "double_precision": OS.has_feature("double"), "cache_enabled": bool(ProjectSettings.get_setting("rendering/shader_compiler/shader_cache/enabled")), "editor_hint": Engine.is_editor_hint()}))
	output = OS.get_environment("GODOT_TEST_LAYOUT_OUT")
	omit_move = OS.get_cmdline_user_args().has("--omit-move")
	if RenderingServer.get_rendering_device() == null:
		quit(77)
		return
	var viewport := SubViewport.new()
	viewport.size = Vector2i(96, 64)
	viewport.world_2d = World2D.new()
	viewport.render_target_update_mode = SubViewport.UPDATE_DISABLED
	root.add_child(viewport)
	var mesh := RenderingServer.mesh_create()
	var arrays: Array = []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = PackedVector3Array([Vector3(-6, -6, 0), Vector3(6, -6, 0), Vector3(6, 6, 0), Vector3(-6, 6, 0)])
	arrays[Mesh.ARRAY_INDEX] = PackedInt32Array([0, 1, 2, 0, 2, 3])
	RenderingServer.mesh_add_surface_from_arrays(mesh, RenderingServer.PRIMITIVE_TRIANGLES, arrays)
	mm = RenderingServer.multimesh_create()
	RenderingServer.multimesh_allocate_data(mm, 3, RenderingServer.MULTIMESH_TRANSFORM_3D, true, true, true)
	RenderingServer.multimesh_set_mesh(mm, mesh)
	var initial := PackedFloat32Array()
	initial.resize(60)
	for i in range(3):
		initial[i * 20] = 1
		initial[i * 20 + 5] = 1
		initial[i * 20 + 10] = 1
		initial[i * 20 + 14] = 1
		initial[i * 20 + 15] = 1
		initial[i * 20 + 16] = 0.25
	RenderingServer.multimesh_set_buffer(mm, initial)
	RenderingServer.call_on_render_thread(_capture.bind("initial_3d"))
	await buffers_read
	var old_command: Array[int] = []
	for offset in range(0, command.size(), 4):
		old_command.append(command.decode_u32(offset))
	var old_valid := capture_stage == "initial_3d" and native == initial.to_byte_array() and cached == initial.to_byte_array() and old_command == [6, 3, 0, 0, 0] and RenderingServer.multimesh_get_mesh(mm) == mesh
	if not old_valid:
		structural_failures += 1
	print("K02_LAYOUT_INITIAL ", JSON.stringify({"pass": old_valid, "cpu_bytes": cached.size(), "gpu_bytes": native.size(), "command": old_command, "same_mesh": RenderingServer.multimesh_get_mesh(mm) == mesh}))
	if not output.is_empty():
		for suffix in ["gpu", "cpu", "command"]:
			var file := FileAccess.open(output.path_join("initial_3d." + suffix), FileAccess.WRITE)
			if file == null:
				structural_failures += 1
				continue
			file.store_buffer(command if suffix == "command" else (native if suffix == "gpu" else cached))
	RenderingServer.multimesh_allocate_data(mm, 2, RenderingServer.MULTIMESH_TRANSFORM_2D, false, false, true)
	if RenderingServer.multimesh_get_mesh(mm) != mesh:
		structural_failures += 1
	RenderingServer.multimesh_set_buffer(mm, _input_data(false))
	RenderingServer.multimesh_set_custom_aabb(mm, AABB(Vector3(0, 0, -1), Vector3(96, 64, 2)))
	var item := RenderingServer.canvas_item_create()
	RenderingServer.canvas_item_set_parent(item, viewport.find_world_2d().canvas)
	RenderingServer.canvas_item_set_modulate(item, Color.RED)
	RenderingServer.canvas_item_add_multimesh(item, mm)
	witness = RenderingServer.canvas_item_create()
	RenderingServer.canvas_item_set_parent(witness, viewport.find_world_2d().canvas)
	RenderingServer.canvas_item_add_rect(witness, Rect2(2, 58, 4, 4), Color.WHITE)
	await _stage("both", viewport, 2, false, 0)
	RenderingServer.multimesh_set_visible_instances(mm, 1)
	await _stage("one", viewport, 1, false, 1)
	RenderingServer.multimesh_set_visible_instances(mm, 0)
	await _stage("zero", viewport, 0, false, 2)
	RenderingServer.multimesh_set_visible_instances(mm, -1)
	await _stage("restored", viewport, 2, false, 3)
	if not omit_move:
		RenderingServer.multimesh_instance_set_transform_2d(mm, 0, Transform2D(Vector2(1, 0), Vector2(0, 1), Vector2(48, 32)))
	await _stage("move", viewport, 2, true, 4)
	await _stage("hold", viewport, 2, true, 5)
	RenderingServer.free_rid(witness)
	RenderingServer.free_rid(item)
	RenderingServer.free_rid(mm)
	RenderingServer.free_rid(mesh)
	viewport.queue_free()
	await process_frame
	print("K02_LAYOUT_RESULT ", JSON.stringify({"checks": checks, "witness_checks": witness_checks, "pixel_failures": failures, "structural_failures": structural_failures, "omit_move": omit_move}))
	quit(0 if failures == 0 and structural_failures == 0 else 1)
