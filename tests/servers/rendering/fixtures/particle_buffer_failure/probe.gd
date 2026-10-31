extends SceneTree

func _initialize() -> void:
	call_deferred("_run")

func _run() -> void:
	if RenderingServer.get_rendering_device() == null:
		quit(77)
		return
	var dimensions := "2d"
	var args := OS.get_cmdline_user_args()
	if not args.is_empty():
		dimensions = args[0]
	if dimensions == "3d":
		await _run_3d()
		return
	if dimensions == "motion_retry":
		await _run_3d(true)
		return
	if dimensions == "motion_control":
		await _run_3d(false, true)
		return
	if dimensions == "uniform_retry":
		await _run_3d(false, false, true)
		return
	if dimensions != "2d":
		quit(78)
		return
	var viewport := SubViewport.new()
	viewport.size = Vector2i(32, 32)
	viewport.world_2d = World2D.new()
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var camera := Camera2D.new()
	camera.position = Vector2(16, 16)
	viewport.add_child(camera)
	var image := Image.create(4, 4, false, Image.FORMAT_RGBA8)
	image.fill(Color.RED)
	var material := ParticleProcessMaterial.new()
	material.gravity = Vector3.ZERO
	material.initial_velocity_min = 0.0
	material.initial_velocity_max = 0.0
	var particles := GPUParticles2D.new()
	particles.position = Vector2(16, 16)
	particles.texture = ImageTexture.create_from_image(image)
	particles.process_material = material
	particles.amount = 1073741824
	particles.fixed_fps = 30
	particles.visibility_rect = Rect2(-16, -16, 32, 32)
	viewport.add_child(particles)
	print("PARTICLE_BUFFER_FAILURE_BEFORE")
	await RenderingServer.frame_post_draw
	print("PARTICLE_BUFFER_FAILURE_AFTER")
	particles.amount = 1
	particles.restart()
	for i in range(30):
		await RenderingServer.frame_post_draw
	var output := viewport.get_texture().get_image()
	var red := 0
	for y in range(32):
		for x in range(32):
			var color := output.get_pixel(x, y)
			if color.r > 0.8 and color.g < 0.2 and color.b < 0.2:
				red += 1
	print("PARTICLE_BUFFER_FAILURE_RECOVERED ", red)
	quit(0 if red > 0 else 1)

func _run_3d(motion_retry := false, motion_control := false, uniform_retry := false) -> void:
	var viewport := SubViewport.new()
	viewport.size = Vector2i(32, 32)
	viewport.own_world_3d = true
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var environment := Environment.new()
	environment.background_mode = Environment.BG_COLOR
	environment.background_color = Color.BLACK
	environment.tonemap_mode = Environment.TONE_MAPPER_LINEAR
	var world_environment := WorldEnvironment.new()
	world_environment.environment = environment
	viewport.add_child(world_environment)
	var camera := Camera3D.new()
	camera.projection = Camera3D.PROJECTION_ORTHOGONAL
	camera.size = 4.0
	camera.position = Vector3(0, 0, 5)
	viewport.add_child(camera)
	camera.make_current()
	var material := ParticleProcessMaterial.new()
	material.gravity = Vector3.ZERO
	material.initial_velocity_min = 0.0
	material.initial_velocity_max = 0.0
	var draw_material := StandardMaterial3D.new()
	draw_material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	draw_material.cull_mode = BaseMaterial3D.CULL_DISABLED
	draw_material.albedo_color = Color.RED
	var mesh := QuadMesh.new()
	mesh.size = Vector2(1.2, 1.2)
	mesh.material = draw_material
	var particles := GPUParticles3D.new()
	particles.process_material = material
	particles.draw_pass_1 = mesh
	particles.amount = 1073741824
	particles.fixed_fps = 30
	particles.visibility_aabb = AABB(Vector3(-2, -2, -2), Vector3(4, 4, 4))
	viewport.add_child(particles)
	if uniform_retry:
		particles.amount = 1
		particles.lifetime = 60.0
		particles.one_shot = true
		particles.explosiveness = 1.0
		particles.preprocess = 0.3
		particles.use_fixed_seed = true
		particles.seed = 7
		OS.set_environment("GODOT_TEST_PARTICLES_REJECT_MATERIAL_UNIFORM_ONCE", "1")
		print("PARTICLE_UNIFORM_FAILURE_BEFORE")
		await RenderingServer.frame_post_draw
		print("PARTICLE_UNIFORM_FAILURE_AFTER")
		for i in range(8):
			await RenderingServer.frame_post_draw
		var output := viewport.get_texture().get_image()
		var red := 0
		for y in range(32):
			for x in range(32):
				var color := output.get_pixel(x, y)
				if color.r > 0.8 and color.g < 0.2 and color.b < 0.2:
					red += 1
		print("PARTICLE_UNIFORM_RECOVERED ", red)
		quit(0 if red > 0 else 1)
		return
	if motion_retry or motion_control:
		particles.amount = 1
		particles.lifetime = 60.0
		particles.one_shot = true
		particles.explosiveness = 1.0
		particles.preprocess = 0.3
		particles.use_fixed_seed = true
		particles.seed = 7
		for i in range(8):
			await RenderingServer.frame_post_draw
		var initial := viewport.get_texture().get_image()
		var initial_red := 0
		for y in range(32):
			for x in range(32):
				var color := initial.get_pixel(x, y)
				if color.r > 0.8 and color.g < 0.2 and color.b < 0.2:
					initial_red += 1
		print("PARTICLE_BUFFER_MOTION_READY ", initial_red)
		viewport.use_taa = true
		if motion_retry:
			OS.set_environment("GODOT_TEST_PARTICLES_REJECT_INSTANCE_ONCE", "1")
		print("PARTICLE_BUFFER_FAILURE_BEFORE")
		await RenderingServer.frame_post_draw
		print("PARTICLE_BUFFER_FAILURE_AFTER")
		for i in range(8):
			await RenderingServer.frame_post_draw
		var output := viewport.get_texture().get_image()
		var red := 0
		for y in range(32):
			for x in range(32):
				var color := output.get_pixel(x, y)
				if color.r > 0.8 and color.g < 0.2 and color.b < 0.2:
					red += 1
		print("PARTICLE_BUFFER_FAILURE_RECOVERED ", red)
		quit(0 if red > 0 else 1)
		return
	print("PARTICLE_BUFFER_FAILURE_BEFORE")
	await RenderingServer.frame_post_draw
	print("PARTICLE_BUFFER_FAILURE_AFTER")
	particles.amount = 1
	particles.restart()
	for i in range(30):
		await RenderingServer.frame_post_draw
	var output := viewport.get_texture().get_image()
	var red := 0
	for y in range(32):
		for x in range(32):
			var color := output.get_pixel(x, y)
			if color.r > 0.8 and color.g < 0.2 and color.b < 0.2:
				red += 1
	print("PARTICLE_BUFFER_FAILURE_RECOVERED ", red)
	quit(0 if red > 0 else 1)
