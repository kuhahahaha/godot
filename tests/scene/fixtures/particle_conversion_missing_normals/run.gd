extends SceneTree

func _initialize() -> void:
	var valid := true
	var cpu_2d := CPUParticles2D.new()
	var gpu_2d := GPUParticles2D.new()
	cpu_2d.emission_shape = CPUParticles2D.EMISSION_SHAPE_DIRECTED_POINTS
	cpu_2d.emission_points = PackedVector2Array([Vector2(2, 3)])
	gpu_2d.convert_from_particles(cpu_2d)
	var material_2d := gpu_2d.process_material as ParticleProcessMaterial
	if material_2d.emission_shape != ParticleProcessMaterial.EMISSION_SHAPE_POINTS or material_2d.emission_point_texture == null or material_2d.emission_normal_texture != null:
		push_error("2D directed points without normals kept GPU normal sampling")
		valid = false
	cpu_2d.emission_normals = PackedVector2Array([Vector2.RIGHT, Vector2.UP])
	gpu_2d.convert_from_particles(cpu_2d)
	material_2d = gpu_2d.process_material as ParticleProcessMaterial
	if material_2d.emission_shape != ParticleProcessMaterial.EMISSION_SHAPE_POINTS or material_2d.emission_normal_texture != null:
		push_error("2D mismatched normal count kept GPU normal sampling")
		valid = false
	var cpu_3d := CPUParticles3D.new()
	var gpu_3d := GPUParticles3D.new()
	cpu_3d.emission_shape = CPUParticles3D.EMISSION_SHAPE_DIRECTED_POINTS
	cpu_3d.emission_points = PackedVector3Array([Vector3(2, 3, 4)])
	gpu_3d.convert_from_particles(cpu_3d)
	var material_3d := gpu_3d.process_material as ParticleProcessMaterial
	if material_3d.emission_shape != ParticleProcessMaterial.EMISSION_SHAPE_POINTS or material_3d.emission_point_texture == null or material_3d.emission_normal_texture != null:
		push_error("3D directed points without normals kept GPU normal sampling")
		valid = false
	cpu_3d.emission_normals = PackedVector3Array([Vector3.RIGHT, Vector3.UP])
	gpu_3d.convert_from_particles(cpu_3d)
	material_3d = gpu_3d.process_material as ParticleProcessMaterial
	if material_3d.emission_shape != ParticleProcessMaterial.EMISSION_SHAPE_POINTS or material_3d.emission_normal_texture != null:
		push_error("3D mismatched normal count kept GPU normal sampling")
		valid = false
	for node in [cpu_2d, gpu_2d, cpu_3d, gpu_3d]:
		node.free()
	quit(0 if valid else 1)
