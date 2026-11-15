extends SceneTree

func make_texture(format: Image.Format, first: Color) -> ImageTexture:
	var image := Image.create(1, 1, false, format)
	image.set_pixel(0, 0, first)
	return ImageTexture.create_from_image(image)

func check_2d() -> bool:
	var gpu := GPUParticles2D.new()
	var cpu := CPUParticles2D.new()
	var material := ParticleProcessMaterial.new()
	material.emission_shape = ParticleProcessMaterial.EMISSION_SHAPE_DIRECTED_POINTS
	material.emission_point_texture = make_texture(Image.FORMAT_RGF, Color(INF, 2, 0))
	material.emission_normal_texture = make_texture(Image.FORMAT_RGF, Color(1, 0, 0))
	material.emission_color_texture = make_texture(Image.FORMAT_RGBA8, Color.RED)
	material.emission_point_count = 1
	gpu.process_material = material
	cpu.convert_from_particles(gpu)
	var valid := cpu.emission_points.is_empty() and cpu.emission_normals.is_empty() and cpu.emission_colors.is_empty()
	if not valid:
		push_error("2D invalid point texture left partial emission arrays")
	gpu.free()
	cpu.free()
	return valid

func check_3d() -> bool:
	var gpu := GPUParticles3D.new()
	var cpu := CPUParticles3D.new()
	var material := ParticleProcessMaterial.new()
	material.emission_shape = ParticleProcessMaterial.EMISSION_SHAPE_DIRECTED_POINTS
	material.emission_point_texture = make_texture(Image.FORMAT_RGBF, Color(1, NAN, 3))
	material.emission_normal_texture = make_texture(Image.FORMAT_RGBF, Color(1, 0, 0))
	material.emission_color_texture = make_texture(Image.FORMAT_RGBA8, Color.GREEN)
	material.emission_point_count = 1
	gpu.process_material = material
	cpu.convert_from_particles(gpu)
	var valid := cpu.emission_points.is_empty() and cpu.emission_normals.is_empty() and cpu.emission_colors.is_empty()
	if not valid:
		push_error("3D invalid point texture left partial emission arrays")
	gpu.free()
	cpu.free()
	return valid

func _initialize() -> void:
	var valid := check_2d()
	valid = check_3d() and valid
	quit(0 if valid else 1)
