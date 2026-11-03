extends SceneTree
const OUT := "/Users/macbookair/Documents/Work/godot_ANGE/work/particle-gpu3d-emission-curve-visible-20261105"
var observations: Array[Dictionary] = []
func _initialize() -> void: call_deferred("run")
func wait_frames(n: int) -> void:
	for i in n: await process_frame
	await RenderingServer.frame_post_draw
func curve_with(value: float) -> GradientTexture1D:
	var gradient := Gradient.new()
	gradient.set_color(0, Color(value, value, value))
	gradient.set_color(1, Color(value, value, value))
	var texture := GradientTexture1D.new()
	texture.gradient = gradient
	return texture
func capture(label: String) -> Dictionary:
	var image := root.get_texture().get_image()
	var pixel_count := 0
	var luminance := 0.0
	for y in range(112, 128):
		for x in range(152, 168):
			var pixel := image.get_pixel(x, y)
			if pixel.r > 0.05 and pixel.g > 0.05 and pixel.b > 0.05:
				pixel_count += 1
				luminance += (pixel.r + pixel.g + pixel.b) / 3.0
	var saved := image.save_png(OUT.path_join(label + ".png")) == OK
	var row := {"stage": label, "pixels": pixel_count, "luminance": luminance, "saved": saved}
	observations.append(row)
	print("GPU3D_EMISSION_STAGE %s pixels=%d luminance=%.3f saved=%s" % [label, pixel_count, luminance, str(saved)])
	return row
func run() -> void:
	var world := Node3D.new()
	root.add_child(world)
	var camera := Camera3D.new()
	camera.projection = Camera3D.PROJECTION_ORTHOGONAL
	camera.size = 3.0
	camera.position = Vector3(0, 0, 5)
	camera.current = true
	world.add_child(camera)
	var mesh_material := StandardMaterial3D.new()
	mesh_material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	mesh_material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	mesh_material.vertex_color_use_as_albedo = true
	var mesh := QuadMesh.new()
	mesh.size = Vector2(0.20, 0.20)
	mesh.material = mesh_material
	var particles := GPUParticles3D.new()
	particles.amount = 8
	particles.lifetime = 1.0
	particles.fixed_fps = 30
	particles.use_fixed_seed = true
	particles.seed = 12345
	particles.visibility_aabb = AABB(Vector3(-1, -1, -1), Vector3(2, 2, 2))
	particles.draw_pass_1 = mesh
	var process_material := ParticleProcessMaterial.new()
	process_material.gravity = Vector3.ZERO
	process_material.initial_velocity_min = 0.0
	process_material.initial_velocity_max = 0.0
	process_material.color = Color(0.2, 0.2, 0.2)
	var old := curve_with(0.0)
	process_material.emission_curve = old
	particles.process_material = process_material
	particles.emitting = true
	world.add_child(particles)
	await wait_frames(60)
	var initial := capture("initial_low")
	old.gradient.set_color(0, Color.WHITE)
	old.gradient.set_color(1, Color.WHITE)
	await wait_frames(20)
	var updated := capture("updated_high")
	var replacement := curve_with(0.0)
	process_material.emission_curve = replacement
	await wait_frames(20)
	var replaced := capture("replaced_low")
	old.gradient.set_color(0, Color(0.5, 0.5, 0.5))
	old.gradient.set_color(1, Color(0.5, 0.5, 0.5))
	await wait_frames(20)
	var detached := capture("detached_old_modified")
	var passed: bool = initial.pixels > 0 and updated.pixels == initial.pixels and updated.luminance > initial.luminance * 1.25 and replaced.pixels == initial.pixels and abs(replaced.luminance - initial.luminance) < 0.1 and detached.pixels == replaced.pixels and abs(detached.luminance - replaced.luminance) < 0.1
	for row in observations: passed = passed and row.saved
	print("GPU3D_EMISSION_RESULT renderer=%s driver=%s passed=%s observations=%s" % [RenderingServer.get_current_rendering_method(), RenderingServer.get_current_rendering_driver_name(), str(passed), JSON.stringify(observations)])
	quit(0 if passed else 1)
