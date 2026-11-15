extends SceneTree

func _initialize() -> void:
	var valid := true
	var cpu_2d := CPUParticles2D.new()
	var gpu_2d := GPUParticles2D.new()
	cpu_2d.emission_shape = CPUParticles2D.EMISSION_SHAPE_POINTS
	gpu_2d.convert_from_particles(cpu_2d)
	if gpu_2d.amount_ratio != 0.0 or not gpu_2d.emitting:
		push_error("2D empty CPU point source still emits after GPU conversion")
		valid = false
	cpu_2d.emission_points = PackedVector2Array([Vector2(2, 3)])
	gpu_2d.convert_from_particles(cpu_2d)
	if gpu_2d.amount_ratio != 1.0:
		push_error("2D populated CPU point source did not restore GPU emission ratio")
		valid = false
	var cpu_3d := CPUParticles3D.new()
	var gpu_3d := GPUParticles3D.new()
	cpu_3d.emission_shape = CPUParticles3D.EMISSION_SHAPE_DIRECTED_POINTS
	gpu_3d.convert_from_particles(cpu_3d)
	if gpu_3d.amount_ratio != 0.0 or not gpu_3d.emitting:
		push_error("3D empty CPU point source still emits after GPU conversion")
		valid = false
	cpu_3d.emission_points = PackedVector3Array([Vector3(2, 3, 4)])
	gpu_3d.convert_from_particles(cpu_3d)
	if gpu_3d.amount_ratio != 1.0:
		push_error("3D populated CPU point source did not restore GPU emission ratio")
		valid = false
	for node in [cpu_2d, gpu_2d, cpu_3d, gpu_3d]:
		node.free()
	quit(0 if valid else 1)
