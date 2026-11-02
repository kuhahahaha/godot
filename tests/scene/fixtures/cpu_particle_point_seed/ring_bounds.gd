extends SceneTree

func _initialize() -> void:
	var failures: Array[String] = []
	var emitter_2d := CPUParticles2D.new()
	var emitter_3d := CPUParticles3D.new()
	var material := ParticleProcessMaterial.new()
	for emitter in [emitter_2d, emitter_3d, material]:
		emitter.emission_ring_radius = 5.0
		emitter.emission_ring_inner_radius = 1.0
		emitter.emission_ring_inner_radius = 6.0
		if emitter.emission_ring_inner_radius != 1.0:
			failures.append(emitter.get_class() + " accepted inner radius larger than outer radius")
		emitter.emission_ring_radius = 0.5
		if emitter.emission_ring_radius != 5.0:
			failures.append(emitter.get_class() + " accepted outer radius smaller than inner radius")
		emitter.emission_ring_radius = 7.0
		emitter.emission_ring_inner_radius = 6.0
		if emitter.emission_ring_radius != 7.0 or emitter.emission_ring_inner_radius != 6.0:
			failures.append(emitter.get_class() + " rejected valid radius transition")
	for dimension in [2, 3]:
		var cpu: Node = CPUParticles2D.new() if dimension == 2 else CPUParticles3D.new()
		var gpu: Node = GPUParticles2D.new() if dimension == 2 else GPUParticles3D.new()
		var roundtrip: Node = CPUParticles2D.new() if dimension == 2 else CPUParticles3D.new()
		cpu.emission_ring_inner_radius = 0.0
		cpu.emission_ring_radius = 0.5
		cpu.emission_ring_inner_radius = 0.4
		cpu.emission_shape = cpu.EMISSION_SHAPE_RING
		gpu.convert_from_particles(cpu)
		var converted_material: ParticleProcessMaterial = gpu.process_material
		if not is_equal_approx(converted_material.emission_ring_radius, 0.5) or not is_equal_approx(converted_material.emission_ring_inner_radius, 0.4):
			failures.append(str(dimension) + "D CPU to GPU conversion lost ring radii")
		roundtrip.emission_ring_radius = 10.0
		roundtrip.emission_ring_inner_radius = 9.0
		roundtrip.convert_from_particles(gpu)
		if not is_equal_approx(roundtrip.emission_ring_radius, 0.5) or not is_equal_approx(roundtrip.emission_ring_inner_radius, 0.4):
			failures.append(str(dimension) + "D GPU to CPU conversion lost ring radii")
		cpu.free()
		gpu.free()
		roundtrip.free()
	var saved := CPUParticles2D.new()
	saved.emitting = false
	saved.emission_ring_radius = 5.0
	saved.emission_ring_inner_radius = 4.0
	saved.emission_shape = CPUParticles2D.EMISSION_SHAPE_RING
	root.add_child(saved)
	var packed := PackedScene.new()
	if packed.pack(saved) != OK:
		failures.append("2D ring scene packing failed")
	else:
		var loaded := packed.instantiate() as CPUParticles2D
		if loaded == null or not is_equal_approx(loaded.emission_ring_radius, 5.0) or not is_equal_approx(loaded.emission_ring_inner_radius, 4.0):
			failures.append("2D ring scene roundtrip lost valid radii")
		if loaded != null:
			loaded.free()
	saved.free()
	emitter_2d.free()
	emitter_3d.free()
	print("CPU_RING_BOUNDS=" + JSON.stringify({"failures": failures}))
	quit(1 if failures.size() else 0)
