extends "res://native_neighbors.gd"

# Reuses native_neighbors' immutable masks, literal RGB and 1/255 tolerance.
# Green is a RenderingServer MultiMesh RID attached to this viewport's actual
# World3D scenario; no external particle object or private native API is used.
signal command_patch_completed(report: Dictionary)

func _set_rd_transforms(multimesh: RID, count: int) -> void:
	RenderingServer.multimesh_instance_set_transform(multimesh, 0, Transform3D.IDENTITY)
	for i in range(1, count):
		RenderingServer.multimesh_instance_set_transform(multimesh, i, Transform3D(Basis.IDENTITY, Vector3(20.0 + 4.0 * i, 0.0, 0.0)))

func _allocate(multimesh: RID, count: int, indirect: bool) -> void:
	RenderingServer.multimesh_allocate_data(multimesh, count, RenderingServer.MULTIMESH_TRANSFORM_3D, false, false, indirect)
	_set_rd_transforms(multimesh, count)

# Called through the render thread callback, between render lists. Only the
# documented instanceCount word (offset 4) is changed to 0 or capacity.
func _patch_on_render_thread(multimesh: RID, count: int, expected_capacity: int) -> void:
	var rd := RenderingServer.get_rendering_device()
	var capacity := RenderingServer.multimesh_get_instance_count(multimesh)
	var visible := RenderingServer.multimesh_get_visible_instances(multimesh)
	var command := RenderingServer.multimesh_get_command_buffer_rd_rid(multimesh)
	var error := ERR_UNAVAILABLE
	if rd != null and command.is_valid() and capacity == expected_capacity and visible == -1 and (count == 0 or count == capacity):
		var word := PackedByteArray()
		word.resize(4)
		word.encode_u32(0, count)
		error = rd.buffer_update(command, 4, 4, word)
	var report := {"error": error, "word_instance_count": count, "capacity": capacity, "visible": visible, "command_valid": command.is_valid()}
	call_deferred("_notify_patch_completed", report)

func _notify_patch_completed(report: Dictionary) -> void:
	command_patch_completed.emit(report)

func _patch(multimesh: RID, value: int, capacity: int) -> bool:
	RenderingServer.call_on_render_thread(_patch_on_render_thread.bind(multimesh, value, capacity))
	var report: Dictionary = await command_patch_completed
	var ok: bool = report.error == OK and report.capacity == capacity and report.visible == -1 and report.command_valid
	report["pass"] = ok
	print("K02_INDIRECT_PATCH ", JSON.stringify(report))
	return ok

func _observe(stage: String, viewport: SubViewport, multimesh: RID, expected_green: Color, expected_count: int, expected_visible: int, expect_indirect: bool) -> bool:
	var capacity := RenderingServer.multimesh_get_instance_count(multimesh)
	var visible := RenderingServer.multimesh_get_visible_instances(multimesh)
	var command := RenderingServer.multimesh_get_command_buffer_rd_rid(multimesh)
	var state_ok := capacity == expected_count and visible == expected_visible and command.is_valid() == expect_indirect
	var pixels_ok := _check_stage(stage, viewport, expected_green)
	print("K02_INDIRECT_STAGE ", JSON.stringify({"stage": stage, "capacity": capacity, "visible": visible, "command_valid": command.is_valid(), "state_pass": state_ok, "pixels_pass": pixels_ok, "pass": state_ok and pixels_ok}))
	return state_ok and pixels_ok

func _run() -> void:
	if RenderingServer.get_rendering_device() == null:
		print("K02_INDIRECT_SKIP=77 main RenderingDevice unavailable")
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

	var green_mesh := _quad(Color(0, 1, 0))
	var multimesh := RenderingServer.multimesh_create()
	_allocate(multimesh, 3, false)
	RenderingServer.multimesh_set_mesh(multimesh, green_mesh.get_rid())
	var green_instance := RenderingServer.instance_create()
	RenderingServer.instance_set_base(green_instance, multimesh)
	RenderingServer.instance_set_scenario(green_instance, viewport.find_world_3d().scenario)

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

	var results := {}
	await _wait_frames(8)
	results["direct_3"] = _observe("direct_3", viewport, multimesh, Color(0, 1, 0), 3, -1, false)
	_allocate(multimesh, 3, true)
	await _wait_frames(3)
	results["indirect_3"] = _observe("indirect_3", viewport, multimesh, Color(0, 1, 0), 3, -1, true)
	results["patch_zero"] = await _patch(multimesh, 0, 3)
	await _wait_frames(3)
	results["indirect_word_zero"] = _observe("indirect_word_zero", viewport, multimesh, Color(0, 0, 0), 3, -1, true)
	results["patch_three"] = await _patch(multimesh, 3, 3)
	await _wait_frames(3)
	results["indirect_word_restored"] = _observe("indirect_word_restored", viewport, multimesh, Color(0, 1, 0), 3, -1, true)
	RenderingServer.multimesh_set_visible_instances(multimesh, 0)
	await _wait_frames(3)
	results["indirect_visible_zero"] = _observe("indirect_visible_zero", viewport, multimesh, Color(0, 0, 0), 3, 0, true)
	RenderingServer.multimesh_set_visible_instances(multimesh, -1)
	await _wait_frames(3)
	results["indirect_visible_all"] = _observe("indirect_visible_all", viewport, multimesh, Color(0, 1, 0), 3, -1, true)
	_allocate(multimesh, 3, false)
	await _wait_frames(3)
	results["direct_again_3"] = _observe("direct_again_3", viewport, multimesh, Color(0, 1, 0), 3, -1, false)
	_allocate(multimesh, 3, true)
	await _wait_frames(3)
	results["indirect_again_3"] = _observe("indirect_again_3", viewport, multimesh, Color(0, 1, 0), 3, -1, true)
	_allocate(multimesh, 5, true)
	await _wait_frames(3)
	results["indirect_resize_3_to_5"] = _observe("indirect_resize_3_to_5", viewport, multimesh, Color(0, 1, 0), 5, -1, true)
	var passed := true
	for value in results.values():
		passed = passed and bool(value)
	RenderingServer.free_rid(green_instance)
	RenderingServer.free_rid(multimesh)
	viewport.queue_free()
	await process_frame
	await RenderingServer.frame_post_draw
	print("K02_INDIRECT_RESULT ", JSON.stringify({"pass": passed, "cases": results, "scope": "ordinary_indirect_color_pixels"}))
	quit(0 if passed else 1)
