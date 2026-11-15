extends SceneTree

func point_texture(format: Image.Format, pixel: Color) -> ImageTexture:
	var image := Image.create(1, 1, false, format)
	image.set_pixel(0, 0, pixel)
	return ImageTexture.create_from_image(image)

func _initialize() -> void:
	var valid := true
	var gpu_2d := GPUParticles2D.new()
	var cpu_2d := CPUParticles2D.new()
	var material_2d := ParticleProcessMaterial.new()
	material_2d.emission_shape = ParticleProcessMaterial.EMISSION_SHAPE_POINTS
	material_2d.emission_point_texture = point_texture(Image.FORMAT_RGF, Color(2, 3, 0))
	material_2d.emission_point_count = 1
	material_2d.emission_shape_scale = Vector3(2, 3, 1)
	material_2d.emission_shape_offset = Vector3(5, 7, 0)
	gpu_2d.process_material = material_2d
	cpu_2d.convert_from_particles(gpu_2d)
	if cpu_2d.emission_points != PackedVector2Array([Vector2(9, 16)]):
		push_error("2D converted point ignored GPU emission shape transform")
		valid = false
	var gpu_3d := GPUParticles3D.new()
	var cpu_3d := CPUParticles3D.new()
	var material_3d := ParticleProcessMaterial.new()
	material_3d.emission_shape = ParticleProcessMaterial.EMISSION_SHAPE_POINTS
	material_3d.emission_point_texture = point_texture(Image.FORMAT_RGBF, Color(2, 3, 4))
	material_3d.emission_point_count = 1
	material_3d.emission_shape_scale = Vector3(2, 3, 4)
	material_3d.emission_shape_offset = Vector3(5, 7, 11)
	gpu_3d.process_material = material_3d
	cpu_3d.convert_from_particles(gpu_3d)
	if cpu_3d.emission_points != PackedVector3Array([Vector3(9, 16, 27)]):
		push_error("3D converted point ignored GPU emission shape transform")
		valid = false
	for node in [gpu_2d, cpu_2d, gpu_3d, cpu_3d]:
		node.free()
	quit(0 if valid else 1)
