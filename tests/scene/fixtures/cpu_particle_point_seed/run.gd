extends SceneTree

var failures: Array[String] = []

func _initialize() -> void:
	call_deferred("run")

func run() -> void:
	for dimension in [2, 3]:
		var p = CPUParticles2D.new() if dimension == 2 else CPUParticles3D.new()
		p.emitting = false
		p.use_fixed_seed = true
		p.seed = 12345
		p.amount = 16
		p.lifetime = 10.0
		p.local_coords = true
		p.emission_shape = p.EMISSION_SHAPE_POINTS
		if dimension == 2:
			var points_2d := PackedVector2Array()
			for i in 32:
				points_2d.append(Vector2(i + 1, 0))
			p.emission_points = points_2d
		else:
			var points_3d := PackedVector3Array()
			for i in 32:
				points_3d.append(Vector3(i + 1, 0, 0))
			p.emission_points = points_3d
		root.add_child(p)
		for frame in 2:
			await process_frame
		seed(98765)
		var expected := randi()
		seed(98765)
		p.request_particles_process(0.2)
		for frame in 2:
			await process_frame
		var actual := randi()
		if actual != expected:
			failures.append(str(dimension) + "D point emission consumed global RNG")
		p.free()
	var ring := CPUParticles2D.new()
	ring.emitting = false
	ring.use_fixed_seed = true
	ring.seed = 12345
	ring.amount = 16
	ring.lifetime = 10.0
	ring.emission_shape = CPUParticles2D.EMISSION_SHAPE_RING
	ring.emission_ring_radius = 10.0
	ring.emission_ring_inner_radius = 5.0
	root.add_child(ring)
	for frame in 2:
		await process_frame
	seed(98765)
	var ring_expected := randi()
	seed(98765)
	ring.request_particles_process(0.2)
	for frame in 2:
		await process_frame
	if randi() != ring_expected:
		failures.append("2D ring emission consumed global RNG")
	ring.free()
	print("CPU_POINT_SEED=" + JSON.stringify({"failures": failures}))
	quit(1 if failures.size() else 0)
