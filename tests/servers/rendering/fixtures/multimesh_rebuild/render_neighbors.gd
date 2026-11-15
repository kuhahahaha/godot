extends SceneTree

var viewport: SubViewport
var mm_indirect_rid: RID
var quad_blue: QuadMesh
var rows: Array[Dictionary] = []

func _initialize() -> void:
	call_deferred("_run")

func _make_unshaded_quad(color: Color) -> QuadMesh:
	var mat := StandardMaterial3D.new()
	mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	mat.albedo_color = color
	mat.cull_mode = BaseMaterial3D.CULL_DISABLED
	var q := QuadMesh.new()
	q.size = Vector2(2.4, 2.4)
	q.material = mat
	return q

func _wait_frames(count: int) -> void:
	for _i in range(count):
		await process_frame

func _sample() -> Dictionary:
	var img := viewport.get_texture().get_image()
	if img == null or img.is_empty():
		return {"valid": false}
	return {
		"valid": true,
		"mesh": img.get_pixel(32, 32),
		"mm_direct": img.get_pixel(96, 32),
		"mm_indirect": img.get_pixel(160, 32),
		"particles": img.get_pixel(224, 32),
	}

func _is_red(c: Color) -> bool:
	return c.r > 0.75 and c.g < 0.25 and c.b < 0.25

func _is_green(c: Color) -> bool:
	return c.g > 0.75 and c.r < 0.25 and c.b < 0.25

func _is_blue(c: Color) -> bool:
	return c.b > 0.75 and c.r < 0.25 and c.g < 0.25

func _is_yellow(c: Color) -> bool:
	return c.r > 0.75 and c.g > 0.75 and c.b < 0.25

func _is_dark(c: Color) -> bool:
	return c.r < 0.15 and c.g < 0.15 and c.b < 0.15

func _set_indirect_transform(x: float) -> void:
	var data := PackedFloat32Array([
		1.0, 0.0, 0.0, x,
		0.0, 1.0, 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0,
		1.0, 0.0, 0.0, 100.0,
		0.0, 1.0, 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0,
	])
	RenderingServer.multimesh_set_buffer(mm_indirect_rid, data)

func _run() -> void:
	var driver := RenderingServer.get_current_rendering_driver_name()
	var method := RenderingServer.get_current_rendering_method()
	if driver == "dummy":
		print("B4_ORDINARY_NEIGHBORS=" + JSON.stringify({"driver": driver, "method": method, "skipped": true}))
		quit(77)
		return

	viewport = SubViewport.new()
	viewport.size = Vector2i(256, 64)
	viewport.own_world_3d = true
	viewport.transparent_bg = false
	viewport.render_target_clear_mode = SubViewport.CLEAR_MODE_ALWAYS
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)

	var env := Environment.new()
	env.background_mode = Environment.BG_COLOR
	env.background_color = Color(0.0, 0.0, 0.0, 1.0)
	var world_env := WorldEnvironment.new()
	world_env.environment = env
	viewport.add_child(world_env)

	var cam := Camera3D.new()
	cam.projection = Camera3D.PROJECTION_ORTHOGONAL
	cam.size = 4.0
	cam.position = Vector3(0.0, 0.0, 5.0)
	cam.current = true
	viewport.add_child(cam)

	# 1. Ordinary MeshInstance3D at x = -6 (pixel x = 32)
	var mesh_inst := MeshInstance3D.new()
	mesh_inst.mesh = _make_unshaded_quad(Color(1.0, 0.0, 0.0))
	mesh_inst.position = Vector3(-6.0, 0.0, 0.0)
	viewport.add_child(mesh_inst)

	# 2. Ordinary direct MultiMeshInstance3D at x = -2 (pixel x = 96)
	var mm_direct := MultiMesh.new()
	mm_direct.transform_format = MultiMesh.TRANSFORM_3D
	mm_direct.instance_count = 2
	mm_direct.mesh = _make_unshaded_quad(Color(0.0, 1.0, 0.0))
	mm_direct.set_instance_transform(0, Transform3D(Basis(), Vector3(-2.0, 0.0, 0.0)))
	mm_direct.set_instance_transform(1, Transform3D(Basis(), Vector3(100.0, 0.0, 0.0)))
	var mmi_direct := MultiMeshInstance3D.new()
	mmi_direct.multimesh = mm_direct
	viewport.add_child(mmi_direct)

	# 3. Indirect MultiMeshInstance3D at x = +2 (pixel x = 160)
	quad_blue = _make_unshaded_quad(Color(0.0, 0.0, 1.0))
	var mm_indirect := MultiMesh.new()
	var mmi_indirect := MultiMeshInstance3D.new()
	mmi_indirect.multimesh = mm_indirect
	viewport.add_child(mmi_indirect)
	mm_indirect_rid = mm_indirect.get_rid()
	RenderingServer.multimesh_allocate_data(mm_indirect_rid, 2, RenderingServer.MULTIMESH_TRANSFORM_3D, false, false, true)
	RenderingServer.multimesh_set_mesh(mm_indirect_rid, quad_blue.get_rid())
	_set_indirect_transform(2.0)

	# 4. Ordinary GPUParticles3D at x = +6 (pixel x = 224)
	var pmat := ParticleProcessMaterial.new()
	pmat.gravity = Vector3.ZERO
	pmat.initial_velocity_min = 0.0
	pmat.initial_velocity_max = 0.0
	var particles := GPUParticles3D.new()
	particles.position = Vector3(6.0, 0.0, 0.0)
	particles.amount = 8
	particles.lifetime = 10.0
	particles.explosiveness = 1.0
	particles.local_coords = true
	particles.process_material = pmat
	particles.draw_pass_1 = _make_unshaded_quad(Color(1.0, 1.0, 0.0))
	particles.emitting = true
	viewport.add_child(particles)

	await _wait_frames(5)
	var s1 := _sample()
	rows.append({
		"case": "R03_M01_all_four_neighbors_render",
		"pass": bool(s1.get("valid", false)) and _is_red(s1.mesh) and _is_green(s1.mm_direct) and _is_blue(s1.mm_indirect) and _is_yellow(s1.particles),
		"mesh": str(s1.get("mesh")),
		"mm_direct": str(s1.get("mm_direct")),
		"mm_indirect": str(s1.get("mm_indirect")),
		"particles": str(s1.get("particles")),
	})

	# M08: visible_instances = 0 suppresses indirect MultiMesh draw without disturbing neighbors.
	RenderingServer.multimesh_set_visible_instances(mm_indirect_rid, 0)
	await _wait_frames(3)
	var s2 := _sample()
	rows.append({
		"case": "M08_zero_visible_hides_only_indirect_multimesh",
		"pass": bool(s2.get("valid", false)) and _is_red(s2.mesh) and _is_green(s2.mm_direct) and _is_dark(s2.mm_indirect) and _is_yellow(s2.particles),
		"mm_indirect": str(s2.get("mm_indirect")),
	})

	# M07: visible_instances = -1 restores indirect MultiMesh draw.
	RenderingServer.multimesh_set_visible_instances(mm_indirect_rid, -1)
	await _wait_frames(3)
	var s3 := _sample()
	rows.append({
		"case": "M07_sentinel_minus_one_restores_indirect_draw",
		"pass": bool(s3.get("valid", false)) and _is_red(s3.mesh) and _is_green(s3.mm_direct) and _is_blue(s3.mm_indirect) and _is_yellow(s3.particles),
		"mm_indirect": str(s3.get("mm_indirect")),
	})

	# M02: indirect true -> false transition retires command buffer and draws via direct path.
	RenderingServer.multimesh_allocate_data(mm_indirect_rid, 2, RenderingServer.MULTIMESH_TRANSFORM_3D, false, false, false)
	_set_indirect_transform(2.0)
	await _wait_frames(3)
	var s4 := _sample()
	var cmd_retired := true
	if RenderingServer.get_rendering_device() != null:
		cmd_retired = not RenderingServer.multimesh_get_command_buffer_rd_rid(mm_indirect_rid).is_valid()
	rows.append({
		"case": "M02_indirect_to_direct_renders_and_retires_command",
		"pass": cmd_retired and bool(s4.get("valid", false)) and _is_red(s4.mesh) and _is_green(s4.mm_direct) and _is_blue(s4.mm_indirect) and _is_yellow(s4.particles),
		"cmd_retired": cmd_retired,
		"mm_indirect": str(s4.get("mm_indirect")),
	})

	viewport.queue_free()
	await process_frame

	var passed := true
	for row in rows:
		passed = passed and bool(row.pass)
	print("B4_ORDINARY_NEIGHBORS=" + JSON.stringify({"driver": driver, "method": method, "rows": rows, "pass": passed}))
	quit(0 if passed else 1)
