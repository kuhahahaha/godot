extends Node

var frames := 0
var failures: Array[String] = []

func _check(ok: bool, label: String) -> void:
	if not ok:
		failures.append(label)
		push_error("PARTICLE_READBACK_FAILURE " + label)

func _ready() -> void:
	var root_2d := Node2D.new()
	add_child(root_2d)
	var process_2d := ParticleProcessMaterial.new()
	process_2d.direction = Vector3(1, 0, 0)
	process_2d.spread = 0.0
	process_2d.gravity = Vector3.ZERO
	process_2d.initial_velocity_min = 30.0
	process_2d.initial_velocity_max = 30.0
	var particles_2d := GPUParticles2D.new()
	particles_2d.amount = 64
	particles_2d.lifetime = 1.0
	particles_2d.process_material = process_2d
	particles_2d.visibility_rect = Rect2(-200, -200, 400, 400)
	root_2d.add_child(particles_2d)
	var direct_rid := RenderingServer.particles_create()
	RenderingServer.particles_set_amount(direct_rid, 4)
	var initial_data := RenderingServer.particles_get_current_data(direct_rid)
	print("PARTICLE_READBACK stage=initial_data bytes=%d" % initial_data.size())
	_check(initial_data.is_empty(), "unallocated direct particle data must be empty")
	var initial_2d := particles_2d.capture_rect()
	print("PARTICLE_READBACK stage=initial_2d rect=%s" % initial_2d)
	_check(initial_2d.position.is_finite() and initial_2d.size.is_finite(), "initial 2D rect non-finite")
	var root_3d := Node3D.new()
	add_child(root_3d)
	var camera_3d := Camera3D.new()
	camera_3d.projection = Camera3D.PROJECTION_ORTHOGONAL
	camera_3d.size = 4.0
	camera_3d.position = Vector3(0.5, 0.0, 4.0)
	camera_3d.current = true
	root_3d.add_child(camera_3d)
	var process_3d := ParticleProcessMaterial.new()
	process_3d.direction = Vector3(1, 0, 0)
	process_3d.spread = 0.0
	process_3d.gravity = Vector3.ZERO
	process_3d.initial_velocity_min = 1.0
	process_3d.initial_velocity_max = 1.0
	var particles_3d := GPUParticles3D.new()
	particles_3d.amount = 64
	particles_3d.lifetime = 1.0
	particles_3d.process_material = process_3d
	var mesh_3d := QuadMesh.new()
	mesh_3d.size = Vector2(0.1, 0.1)
	particles_3d.draw_pass_1 = mesh_3d
	root_3d.add_child(particles_3d)
	var initial_3d := particles_3d.capture_aabb()
	print("PARTICLE_READBACK stage=initial_3d aabb=%s" % initial_3d)
	_check(initial_3d.position.is_finite() and initial_3d.size.is_finite(), "initial 3D AABB non-finite")
	for _i in 60:
		await get_tree().process_frame
		frames += 1
	await RenderingServer.frame_post_draw
	var active_2d := particles_2d.capture_rect()
	var active_3d := particles_3d.capture_aabb()
	print("PARTICLE_READBACK stage=active frames=%d rect2d=%s aabb3d=%s" % [frames, active_2d, active_3d])
	_check(active_2d.position.is_finite() and active_2d.size.is_finite(), "active 2D rect non-finite")
	_check(active_3d.position.is_finite() and active_3d.size.is_finite(), "active 3D AABB non-finite")
	_check(active_2d.size.x > 0.0, "active 2D rect has no spread")
	_check(active_3d.size.x > 0.0, "active 3D AABB has no spread")
	particles_2d.queue_free()
	particles_3d.queue_free()
	RenderingServer.free_rid(direct_rid)
	for _i in 5:
		await get_tree().process_frame
	print("PARTICLE_READBACK_COMPLETE frames=%d failures=%d freed2d=%s freed3d=%s" % [frames, failures.size(), not is_instance_valid(particles_2d), not is_instance_valid(particles_3d)])
	get_tree().quit(0 if failures.is_empty() else 1)
