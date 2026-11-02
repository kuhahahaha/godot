extends SceneTree

func _initialize() -> void:
	var valid := true
	var cpu_2d := CPUParticles2D.new()
	var gpu_2d := GPUParticles2D.new()
	cpu_2d.emission_shape = CPUParticles2D.EMISSION_SHAPE_DIRECTED_POINTS
	cpu_2d.emission_points = PackedVector2Array([Vector2(3, 4), Vector2(5, 6)])
	cpu_2d.emission_normals = PackedVector2Array([Vector2.RIGHT, Vector2.UP])
	cpu_2d.emission_colors = PackedColorArray([Color.RED, Color.GREEN])
	gpu_2d.convert_from_particles(cpu_2d)
	var mat_2d := gpu_2d.process_material as ParticleProcessMaterial
	if mat_2d.emission_point_count != 2 or mat_2d.emission_point_texture == null or mat_2d.emission_normal_texture == null or mat_2d.emission_color_texture == null:
		push_error("2D CPU point arrays did not encode GPU emission textures")
		valid = false
	var roundtrip_2d := CPUParticles2D.new()
	roundtrip_2d.convert_from_particles(gpu_2d)
	if roundtrip_2d.emission_points != cpu_2d.emission_points or roundtrip_2d.emission_normals != cpu_2d.emission_normals or roundtrip_2d.emission_colors != cpu_2d.emission_colors:
		push_error("2D point, normal or color array changed on CPU-GPU-CPU conversion")
		valid = false
	var cpu_3d := CPUParticles3D.new()
	var gpu_3d := GPUParticles3D.new()
	cpu_3d.emission_shape = CPUParticles3D.EMISSION_SHAPE_DIRECTED_POINTS
	cpu_3d.emission_points = PackedVector3Array([Vector3(3, 4, 5), Vector3(6, 7, 8)])
	cpu_3d.emission_normals = PackedVector3Array([Vector3.RIGHT, Vector3.UP])
	cpu_3d.emission_colors = PackedColorArray([Color.RED, Color.GREEN])
	gpu_3d.convert_from_particles(cpu_3d)
	var mat_3d := gpu_3d.process_material as ParticleProcessMaterial
	if mat_3d.emission_point_count != 2 or mat_3d.emission_point_texture == null or mat_3d.emission_normal_texture == null or mat_3d.emission_color_texture == null:
		push_error("3D CPU point arrays did not encode GPU emission textures")
		valid = false
	var roundtrip_3d := CPUParticles3D.new()
	roundtrip_3d.convert_from_particles(gpu_3d)
	if roundtrip_3d.emission_points != cpu_3d.emission_points or roundtrip_3d.emission_normals != cpu_3d.emission_normals or roundtrip_3d.emission_colors != cpu_3d.emission_colors:
		push_error("3D point, normal or color array changed on CPU-GPU-CPU conversion")
		valid = false
	for node in [cpu_2d, gpu_2d, roundtrip_2d, cpu_3d, gpu_3d, roundtrip_3d]:
		node.free()
	quit(0 if valid else 1)
