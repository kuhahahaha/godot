extends SceneTree

func texture(format: Image.Format, pixel: Color) -> ImageTexture:
	var image := Image.create(1, 1, false, format)
	image.set_pixel(0, 0, pixel)
	return ImageTexture.create_from_image(image)

func _initialize() -> void:
	var valid := true
	var gpu_2d := GPUParticles2D.new()
	var cpu_2d := CPUParticles2D.new()
	var material_2d := ParticleProcessMaterial.new()
	material_2d.emission_shape = ParticleProcessMaterial.EMISSION_SHAPE_DIRECTED_POINTS
	material_2d.emission_point_texture = texture(Image.FORMAT_RGF, Color(2, 3, 0))
	material_2d.emission_normal_texture = texture(Image.FORMAT_RGF, Color(INF, 0, 0))
	material_2d.emission_point_count = 1
	gpu_2d.process_material = material_2d
	cpu_2d.convert_from_particles(gpu_2d)
	if cpu_2d.emission_points != PackedVector2Array([Vector2(2, 3)]) or not cpu_2d.emission_normals.is_empty():
		push_error("2D invalid normal changed valid points or persisted normal")
		valid = false
	var gpu_3d := GPUParticles3D.new()
	var cpu_3d := CPUParticles3D.new()
	var material_3d := ParticleProcessMaterial.new()
	material_3d.emission_shape = ParticleProcessMaterial.EMISSION_SHAPE_DIRECTED_POINTS
	material_3d.emission_point_texture = texture(Image.FORMAT_RGBF, Color(2, 3, 4))
	material_3d.emission_normal_texture = texture(Image.FORMAT_RGBF, Color(0, NAN, 0))
	material_3d.emission_point_count = 1
	gpu_3d.process_material = material_3d
	cpu_3d.convert_from_particles(gpu_3d)
	if cpu_3d.emission_points != PackedVector3Array([Vector3(2, 3, 4)]) or not cpu_3d.emission_normals.is_empty():
		push_error("3D invalid normal changed valid points or persisted normal")
		valid = false
	for node in [gpu_2d, cpu_2d, gpu_3d, cpu_3d]:
		node.free()
	quit(0 if valid else 1)
