extends Node3D

var failures: Array[String] = []

func _check(ok: bool, label: String) -> void:
	if not ok:
		failures.append(label)
		push_error("PARTICLE_DRAW_PASS_FAILURE " + label)

func _count_colors() -> Vector2i:
	var image := get_viewport().get_texture().get_image()
	var counts := Vector2i.ZERO
	for y in image.get_height():
		for x in image.get_width():
			var color := image.get_pixel(x, y)
			if color.r > 0.5 and color.g < 0.2 and color.b < 0.2:
				counts.x += 1
			if color.g > 0.5 and color.r < 0.2 and color.b < 0.2:
				counts.y += 1
	return counts

func _mesh(color: Color, offset: float) -> QuadMesh:
	var material := StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.albedo_color = color
	var mesh := QuadMesh.new()
	mesh.size = Vector2(0.08, 0.08)
	mesh.center_offset = Vector3(offset, 0.0, 0.0)
	mesh.material = material
	return mesh

func _sample(particles: GPUParticles3D, stage: String) -> Vector2i:
	for _i in 60:
		await get_tree().process_frame
	await RenderingServer.frame_post_draw
	var pixels := _count_colors()
	var aabb := particles.capture_aabb()
	print("PARTICLE_DRAW_PASS stage=%s passes=%d red=%d green=%d aabb_x=%f" % [stage, particles.draw_passes, pixels.x, pixels.y, aabb.size.x])
	_check(aabb.size.x > 0.2, stage + " AABB did not spread")
	return pixels

func _ready() -> void:
	var camera := Camera3D.new()
	camera.projection = Camera3D.PROJECTION_ORTHOGONAL
	camera.size = 2.0
	camera.position = Vector3(0.3, 0.0, 4.0)
	camera.current = true
	add_child(camera)
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
	particles.draw_pass_1 = _mesh(Color.RED, -0.08)
	particles.process_material = process_material
	particles.visibility_aabb = AABB(Vector3(-1.0, -1.0, -1.0), Vector3(3.0, 2.0, 2.0))
	add_child(particles)
	var first := await _sample(particles, "one")
	_check(particles.draw_passes == 1 and first.x > 4 and first.y == 0, "one pass pixels")
	particles.draw_passes = 2
	particles.draw_pass_2 = _mesh(Color.GREEN, 0.08)
	var second := await _sample(particles, "two")
	_check(particles.draw_passes == 2 and second.x > 4 and second.y > 4, "two pass pixels")
	particles.draw_passes = 1
	var third := await _sample(particles, "shrunk")
	_check(particles.draw_passes == 1 and third.x > 4 and third.y == 0, "shrunk pass pixels")
	particles.queue_free()
	for _i in 5:
		await get_tree().process_frame
	_check(not is_instance_valid(particles), "particle node remains after queue_free")
	print("PARTICLE_DRAW_PASS_COMPLETE failures=%d freed=%s" % [failures.size(), not is_instance_valid(particles)])
	get_tree().quit(0 if failures.is_empty() else 1)
