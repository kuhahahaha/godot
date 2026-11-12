extends SceneTree

func _initialize() -> void:
	call_deferred("run")

func run() -> void:
	var particles := CPUParticles3D.new()
	particles.emitting = false
	particles.use_fixed_seed = true
	particles.seed = 12345
	particles.amount = 16
	particles.lifetime = 10.0
	particles.local_coords = true
	particles.emission_shape = CPUParticles3D.EMISSION_SHAPE_RING
	particles.emission_ring_radius = 10.0
	particles.emission_ring_inner_radius = 5.0
	particles.emission_ring_height = 0.0
	particles.emission_ring_cone_angle = 90.0
	particles.initial_velocity_min = 0.0
	particles.initial_velocity_max = 0.0
	particles.gravity = Vector3.ZERO
	root.add_child(particles)
	for frame in 2:
		await process_frame
	particles.request_particles_process(2.0)
	for frame in 3:
		await process_frame
	RenderingServer.force_draw()
	RenderingServer.force_sync()
	var data := RenderingServer.multimesh_get_buffer(particles.get_base())
	var radii: Array[float] = []
	var failures: Array[String] = []
	if data.size() < particles.amount * 20:
		failures.append("3D ring MultiMesh buffer is incomplete")
	else:
		for i in particles.amount:
			var base := i * 20
			if data[base] == 0.0:
				continue
			var pos := Vector3(data[base + 3], data[base + 7], data[base + 11])
			var radius := Vector2(pos.x, pos.y).length()
			radii.append(radius)
			if not pos.is_finite() or radius < 5.0 - 0.0001 or radius > 10.0 + 0.0001 or absf(pos.z) > 0.0001:
				failures.append("3D ring emitted outside its annulus")
	if radii.is_empty():
		failures.append("3D ring emitted no observable particles")
	print("CPU_RING_POSITIONS_3D=" + JSON.stringify({"radii": radii, "failures": failures}))
	particles.free()
	quit(1 if failures.size() else 0)
