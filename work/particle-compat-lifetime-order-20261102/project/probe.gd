extends Node3D

var failures: Array[String] = []

func _check(ok: bool, label: String) -> void:
	if not ok:
		failures.append(label)
		push_error("PARTICLE_LIFETIME_FAILURE " + label)

func _count_red() -> int:
	var image := get_viewport().get_texture().get_image()
	var count := 0
	for y in image.get_height():
		for x in image.get_width():
			var color := image.get_pixel(x, y)
			if color.r > 0.5 and color.g < 0.2 and color.b < 0.2:
				count += 1
	return count

func _ready() -> void:
	var camera := Camera3D.new()
	camera.projection = Camera3D.PROJECTION_ORTHOGONAL
	camera.size = 2.0
	camera.position = Vector3(0.3, 0.0, 4.0)
	camera.current = true
	add_child(camera)
	var draw_material := StandardMaterial3D.new()
	draw_material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	draw_material.albedo_color = Color(1.0, 0.0, 0.0, 1.0)
	var mesh := QuadMesh.new()
	mesh.size = Vector2(0.08, 0.08)
	mesh.material = draw_material
	var process_material := ParticleProcessMaterial.new()
	process_material.direction = Vector3(1.0, 0.0, 0.0)
	process_material.spread = 0.0
	process_material.gravity = Vector3.ZERO
	process_material.initial_velocity_min = 1.0
	process_material.initial_velocity_max = 1.0
	var particles := GPUParticles3D.new()
	particles.amount = 64
	particles.lifetime = 0.5
	particles.fixed_fps = 60
	particles.use_fixed_seed = true
	particles.seed = 1234
	particles.draw_order = GPUParticles3D.DRAW_ORDER_LIFETIME
	particles.draw_pass_1 = mesh
	particles.process_material = process_material
	particles.visibility_aabb = AABB(Vector3(-1.0, -1.0, -1.0), Vector3(3.0, 2.0, 2.0))
	add_child(particles)
	for _i in 60:
		await get_tree().process_frame
	await RenderingServer.frame_post_draw
	var lifetime_pixels := _count_red()
	var lifetime_aabb := particles.capture_aabb()
	print("PARTICLE_LIFETIME stage=lifetime pixels=%d aabb=%s" % [lifetime_pixels, lifetime_aabb])
	_check(lifetime_pixels > 4 and lifetime_aabb.size.x > 0.2, "lifetime sort did not render spread")
	particles.draw_order = GPUParticles3D.DRAW_ORDER_REVERSE_LIFETIME
	for _i in 60:
		await get_tree().process_frame
	await RenderingServer.frame_post_draw
	var reverse_pixels := _count_red()
	var reverse_aabb := particles.capture_aabb()
	print("PARTICLE_LIFETIME stage=reverse pixels=%d aabb=%s" % [reverse_pixels, reverse_aabb])
	_check(reverse_pixels > 4 and reverse_aabb.size.x > 0.2, "reverse lifetime sort did not render spread")
	particles.queue_free()
	for _i in 5:
		await get_tree().process_frame
	_check(not is_instance_valid(particles), "particle node remains after queue_free")
	print("PARTICLE_LIFETIME_COMPLETE failures=%d freed=%s" % [failures.size(), not is_instance_valid(particles)])
	get_tree().quit(0 if failures.is_empty() else 1)
