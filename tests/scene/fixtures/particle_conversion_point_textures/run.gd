extends SceneTree

func point_texture_2d() -> ImageTexture:
	var image := Image.create(2, 1, false, Image.FORMAT_RGF)
	image.set_pixel(0, 0, Color(3, 4, 0))
	image.set_pixel(1, 0, Color(5, 6, 0))
	return ImageTexture.create_from_image(image)

func point_texture_3d() -> ImageTexture:
	var image := Image.create(2, 1, false, Image.FORMAT_RGBF)
	image.set_pixel(0, 0, Color(3, 4, 5))
	image.set_pixel(1, 0, Color(6, 7, 8))
	return ImageTexture.create_from_image(image)

func color_texture() -> ImageTexture:
	var image := Image.create(2, 1, false, Image.FORMAT_RGBA8)
	image.set_pixel(0, 0, Color.RED)
	image.set_pixel(1, 0, Color.GREEN)
	return ImageTexture.create_from_image(image)

func _initialize() -> void:
	var valid := true
	var gpu_2d := GPUParticles2D.new()
	var cpu_2d := CPUParticles2D.new()
	var material_2d := ParticleProcessMaterial.new()
	material_2d.emission_shape = ParticleProcessMaterial.EMISSION_SHAPE_DIRECTED_POINTS
	material_2d.emission_point_texture = point_texture_2d()
	material_2d.emission_normal_texture = point_texture_2d()
	material_2d.emission_color_texture = color_texture()
	material_2d.emission_point_count = 2
	gpu_2d.process_material = material_2d
	cpu_2d.convert_from_particles(gpu_2d)
	if cpu_2d.emission_points != PackedVector2Array([Vector2(3, 4), Vector2(5, 6)]):
		push_error("2D point texture did not convert to CPU points")
		valid = false
	if cpu_2d.emission_normals != PackedVector2Array([Vector2(3, 4), Vector2(5, 6)]) or cpu_2d.emission_colors != PackedColorArray([Color.RED, Color.GREEN]):
		push_error("2D normal or color texture did not convert")
		valid = false
	var gpu_3d := GPUParticles3D.new()
	var cpu_3d := CPUParticles3D.new()
	var material_3d := ParticleProcessMaterial.new()
	material_3d.emission_shape = ParticleProcessMaterial.EMISSION_SHAPE_DIRECTED_POINTS
	material_3d.emission_point_texture = point_texture_3d()
	material_3d.emission_normal_texture = point_texture_3d()
	material_3d.emission_color_texture = color_texture()
	material_3d.emission_point_count = 2
	gpu_3d.process_material = material_3d
	cpu_3d.convert_from_particles(gpu_3d)
	if cpu_3d.emission_points != PackedVector3Array([Vector3(3, 4, 5), Vector3(6, 7, 8)]):
		push_error("3D point texture did not convert to CPU points")
		valid = false
	if cpu_3d.emission_normals != PackedVector3Array([Vector3(3, 4, 5), Vector3(6, 7, 8)]) or cpu_3d.emission_colors != PackedColorArray([Color.RED, Color.GREEN]):
		push_error("3D normal or color texture did not convert")
		valid = false
	material_2d.emission_point_count = 3
	cpu_2d.convert_from_particles(gpu_2d)
	if not cpu_2d.emission_points.is_empty():
		push_error("2D point count beyond image retained old points")
		valid = false
	material_3d.emission_point_count = 3
	cpu_3d.convert_from_particles(gpu_3d)
	if not cpu_3d.emission_points.is_empty():
		push_error("3D point count beyond image retained old points")
		valid = false
	for node in [gpu_2d, cpu_2d, gpu_3d, cpu_3d]:
		node.free()
	quit(0 if valid else 1)
