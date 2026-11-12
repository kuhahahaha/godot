extends SceneTree

func _initialize() -> void:
	call_deferred("run")

func run() -> void:
	var p := CPUParticles3D.new()
	p.emitting = false
	p.use_fixed_seed = true
	p.seed = 12345
	p.amount = 16
	p.lifetime = 10.0
	p.local_coords = true
	p.emission_shape = CPUParticles3D.EMISSION_SHAPE_POINTS
	var points := PackedVector3Array()
	for i in 32:
		points.append(Vector3(i + 1, 0, 0))
	p.emission_points = points
	root.add_child(p)
	for frame in 2:
		await process_frame
	p.request_particles_process(2.0)
	for frame in 3:
		await process_frame
	RenderingServer.force_draw()
	RenderingServer.force_sync()
	var data := RenderingServer.multimesh_get_buffer(p.get_base())
	var positions: Array[float] = []
	for i in p.amount:
		if data.size() >= (i + 1) * 20 and data[i * 20 + 0] != 0.0:
			positions.append(data[i * 20 + 3])
	var expected: Array[float] = [20.0, 20.0, 16.0, 24.0]
	var matches := positions == expected
	print("CPU_POINT_POSITIONS_3D=" + JSON.stringify({"count": positions.size(), "positions": positions, "matches": matches}))
	p.free()
	quit(0 if matches else 1)
