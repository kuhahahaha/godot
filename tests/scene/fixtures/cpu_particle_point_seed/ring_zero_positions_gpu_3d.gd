extends SceneTree

func _initialize() -> void:
	call_deferred("run")

func run() -> void:
	var material := ParticleProcessMaterial.new()
	material.emission_shape = ParticleProcessMaterial.EMISSION_SHAPE_RING
	material.emission_ring_radius = 0.0
	material.emission_ring_inner_radius = 0.0
	material.emission_ring_height = 0.0
	material.emission_ring_cone_angle = 90.0
	material.initial_velocity_min = 0.0
	material.initial_velocity_max = 0.0
	material.gravity = Vector3.ZERO
	var particles := GPUParticles3D.new()
	particles.amount = 16
	particles.lifetime = 10.0
	particles.use_fixed_seed = true
	particles.seed = 12345
	particles.local_coords = true
	particles.process_material = material
	particles.draw_pass_1 = SphereMesh.new()
	root.add_child(particles)
	var camera := Camera3D.new()
	camera.position = Vector3(0.0, 0.0, 4.0)
	camera.current = true
	root.add_child(camera)
	particles.request_particles_process(2.0)
	for frame in 120:
		await process_frame
	await RenderingServer.frame_post_draw
	var data := RenderingServer.particles_get_current_data(particles.get_base())
	var positions: Array[Vector3] = []
	var failures: Array[String] = []
	const STRIDE := 128
	if data.size() != particles.amount * STRIDE:
		failures.append("GPU particle readback size mismatch: " + str(data.size()))
	else:
		for i in particles.amount:
			var base := i * STRIDE
			if data.decode_u32(base + 76) & 1 == 0:
				continue
			var position := Vector3(data.decode_float(base + 48), data.decode_float(base + 52), data.decode_float(base + 56))
			positions.append(position)
			if not position.is_finite() or Vector2(position.x, position.z).length() > 0.000001 or absf(position.y) > 0.00002:
				failures.append("GPU zero-radius ring emitted outside gravity tolerance")
	if positions.is_empty():
		failures.append("GPU ring emitted no observable particles")
	print("GPU_ZERO_RING_POSITIONS_3D=" + JSON.stringify({"positions": positions, "failures": failures, "bytes": data.size()}))
	particles.free()
	quit(1 if failures.size() else 0)
