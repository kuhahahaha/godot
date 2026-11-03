extends SceneTree

const STRIDE := 128
var particles: GPUParticles3D
var material: ParticleProcessMaterial
var old_curve: GradientTexture1D

func _initialize() -> void:
	call_deferred("run")

func curve_with(value: float) -> GradientTexture1D:
	var gradient := Gradient.new()
	gradient.set_color(0, Color(value, value, value))
	gradient.set_color(1, Color(value, value, value))
	var texture := GradientTexture1D.new()
	texture.gradient = gradient
	return texture

func read_motion() -> Dictionary:
	var data := RenderingServer.particles_get_current_data(particles.get_base())
	var active := 0
	var finite := true
	var sum_x := 0.0
	var sum_vx := 0.0
	if data.size() == particles.amount * STRIDE:
		for i in particles.amount:
			var b := i * STRIDE
			if data.decode_u32(b + 76) & 1 == 0:
				continue
			var x := data.decode_float(b + 48)
			var vx := data.decode_float(b + 64)
			active += 1
			finite = finite and is_finite(x) and is_finite(vx)
			sum_x += x
			sum_vx += vx
	return {"bytes": data.size(), "active": active, "finite": finite, "mean_x": sum_x / max(1, active), "mean_vx": sum_vx / max(1, active)}

func sample(label: String) -> Dictionary:
	particles.restart()
	for frame in 30:
		await process_frame
	await RenderingServer.frame_post_draw
	var row := {"stage": label, "motion": read_motion()}
	print("VELOCITY_LIMIT_STAGE " + JSON.stringify(row))
	return row

func run() -> void:
	var camera := Camera3D.new()
	camera.position = Vector3(0, 0, 8)
	camera.current = true
	root.add_child(camera)
	material = ParticleProcessMaterial.new()
	material.gravity = Vector3.ZERO
	material.direction = Vector3.RIGHT
	material.spread = 0.0
	material.initial_velocity_min = 5.0
	material.initial_velocity_max = 5.0
	old_curve = curve_with(0.1)
	material.velocity_limit_curve = old_curve
	particles = GPUParticles3D.new()
	particles.amount = 8
	particles.lifetime = 4.0
	particles.one_shot = true
	particles.explosiveness = 1.0
	particles.use_fixed_seed = true
	particles.seed = 12345
	particles.process_material = material
	particles.emitting = false
	particles.visibility_aabb = AABB(Vector3(-20, -20, -20), Vector3(40, 40, 40))
	particles.draw_pass_1 = SphereMesh.new()
	root.add_child(particles)
	for frame in 20:
		await process_frame
	await RenderingServer.frame_post_draw
	var low := await sample("initial_low")
	old_curve.gradient.set_color(0, Color.WHITE)
	old_curve.gradient.set_color(1, Color.WHITE)
	var high := await sample("updated_high")
	var replacement := curve_with(0.1)
	material.velocity_limit_curve = replacement
	var replaced := await sample("replaced_low")
	old_curve.gradient.set_color(0, Color(0.5, 0.5, 0.5))
	old_curve.gradient.set_color(1, Color(0.5, 0.5, 0.5))
	var detached := await sample("detached_old_modified")
	var rows := [low, high, replaced, detached]
	var passed := true
	for row in rows:
		passed = passed and row.motion.bytes == particles.amount * STRIDE and row.motion.active == particles.amount and row.motion.finite
	passed = passed and high.motion.mean_x > low.motion.mean_x * 3.0
	passed = passed and abs(replaced.motion.mean_x - low.motion.mean_x) < 0.1
	passed = passed and abs(detached.motion.mean_x - replaced.motion.mean_x) < 0.1
	print("VELOCITY_LIMIT_RESULT renderer=%s driver=%s passed=%s rows=%s" % [RenderingServer.get_current_rendering_method(), RenderingServer.get_current_rendering_driver_name(), str(passed), JSON.stringify(rows)])
	quit(0 if passed else 1)
