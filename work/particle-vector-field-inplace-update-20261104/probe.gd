extends SceneTree

const STRIDE := 128
var field: GPUParticlesAttractorVectorField3D
var particles: GPUParticles3D
var field_texture: ImageTexture3D

func _initialize() -> void:
	call_deferred("run")

func make_image(red: float) -> Image:
	var image := Image.create(2, 2, false, Image.FORMAT_RGBA8)
	image.fill(Color(red, 0.5, 0.5, 1.0))
	return image

func set_field(red: float) -> void:
	var images: Array[Image] = [make_image(red), make_image(red)]
	field_texture = ImageTexture3D.new()
	field_texture.create(Image.FORMAT_RGBA8, 2, 2, 2, false, images)
	field.texture = field_texture

func read_motion() -> Dictionary:
	var data := RenderingServer.particles_get_current_data(particles.get_base())
	var active := 0
	var finite := true
	var sum_x := 0.0
	var min_x := INF
	var max_x := -INF
	if data.size() == particles.amount * STRIDE:
		for i in particles.amount:
			var b := i * STRIDE
			if data.decode_u32(b + 76) & 1 == 0:
				continue
			var x := data.decode_float(b + 48)
			active += 1
			finite = finite and is_finite(x)
			sum_x += x
			min_x = minf(min_x, x)
			max_x = maxf(max_x, x)
	return {"bytes":data.size(), "active":active, "finite":finite, "mean_x":sum_x / max(1, active), "min_x":min_x, "max_x":max_x}

func sample(label: String) -> Dictionary:
	particles.restart()
	for frame in 35:
		await process_frame
	await RenderingServer.frame_post_draw
	var row := {"stage":label, "motion":read_motion()}
	print("VECTOR_FIELD_STAGE " + JSON.stringify(row))
	return row

func run() -> void:
	var camera := Camera3D.new()
	root.add_child(camera)
	camera.position = Vector3(0, 0, 8)
	camera.current = true
	field = GPUParticlesAttractorVectorField3D.new()
	field.size = Vector3(20, 20, 20)
	field.strength = -12.0
	field.attenuation = 0.0
	field.directionality = 0.0
	root.add_child(field)
	var pm := ParticleProcessMaterial.new()
	pm.gravity = Vector3.ZERO
	pm.initial_velocity_min = 0.0
	pm.initial_velocity_max = 0.0
	particles = GPUParticles3D.new()
	particles.amount = 8
	particles.lifetime = 4.0
	particles.explosiveness = 1.0
	particles.one_shot = true
	particles.use_fixed_seed = true
	particles.seed = 12345
	particles.process_material = pm
	particles.emitting = false
	particles.visibility_aabb = AABB(Vector3(-20,-20,-20),Vector3(40,40,40))
	var mesh := QuadMesh.new()
	mesh.size = Vector2(0.1,0.1)
	particles.draw_pass_1 = mesh
	root.add_child(particles)
	for frame in 20:
		await process_frame
	await RenderingServer.frame_post_draw
	set_field(0.0)
	var plus := await sample("plus_x")
	var original_texture := field_texture
	var original_rid := field_texture.get_rid()
	field_texture.update([make_image(1.0), make_image(1.0)])
	var minus := await sample("updated_minus_x")
	print("VECTOR_FIELD_RESULT " + JSON.stringify({"plus":plus,"minus":minus,"same_resource": field_texture == original_texture,"same_rid": field_texture.get_rid() == original_rid}))
	quit(0)
