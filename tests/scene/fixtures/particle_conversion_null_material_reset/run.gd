extends SceneTree

const FIELDS_2D := ["direction", "spread", "color", "color_ramp", "color_initial_ramp", "emission_shape", "emission_sphere_radius", "emission_rect_extents", "emission_ring_radius", "emission_ring_inner_radius", "emission_points", "emission_normals", "emission_colors", "gravity", "lifetime_randomness", "split_scale", "scale_curve_x", "scale_curve_y"]
const FIELDS_3D := ["direction", "spread", "flatness", "color", "color_ramp", "color_initial_ramp", "emission_shape", "emission_sphere_radius", "emission_box_extents", "emission_ring_radius", "emission_ring_inner_radius", "emission_ring_axis", "emission_ring_height", "emission_ring_cone_angle", "emission_points", "emission_normals", "emission_colors", "gravity", "lifetime_randomness", "split_scale", "scale_curve_x", "scale_curve_y", "scale_curve_z"]

func check_dimension(gpu: Node, fresh: Node, reused: Node, fields: Array) -> bool:
	reused.set("direction", Vector3(0, 1, 0) if gpu is GPUParticles3D else Vector2(0, 1))
	reused.set("spread", 90.0)
	reused.set("color", Color.RED)
	reused.set("emission_shape", 1)
	reused.set("gravity", Vector3(1, 2, 3) if gpu is GPUParticles3D else Vector2(1, 2))
	reused.set("lifetime_randomness", 0.5)
	reused.set("split_scale", true)
	reused.set("color_ramp", Gradient.new())
	reused.set("scale_curve_x", Curve.new())
	reused.set("emission_points", PackedVector3Array([Vector3.ONE]) if gpu is GPUParticles3D else PackedVector2Array([Vector2.ONE]))
	reused.set_param_max(0, 7.0)
	reused.set_param_curve(0, Curve.new())
	reused.set_particle_flag(0, true)
	fresh.convert_from_particles(gpu)
	reused.convert_from_particles(gpu)
	var valid := true
	for field in fields:
		if fresh.get(field) != reused.get(field):
			push_error("stale %s: %s" % [gpu.get_class(), field])
			valid = false
	for param in range(12):
		if fresh.get_param_min(param) != reused.get_param_min(param) or fresh.get_param_max(param) != reused.get_param_max(param) or fresh.get_param_curve(param) != reused.get_param_curve(param):
			push_error("stale %s parameter %d" % [gpu.get_class(), param])
			valid = false
	for flag in range(3):
		if fresh.get_particle_flag(flag) != reused.get_particle_flag(flag):
			push_error("stale %s flag %d" % [gpu.get_class(), flag])
			valid = false
	return valid

func _initialize() -> void:
	var gpu_2d := GPUParticles2D.new()
	var fresh_2d := CPUParticles2D.new()
	var reused_2d := CPUParticles2D.new()
	var valid := check_dimension(gpu_2d, fresh_2d, reused_2d, FIELDS_2D)
	var gpu_3d := GPUParticles3D.new()
	var fresh_3d := CPUParticles3D.new()
	var reused_3d := CPUParticles3D.new()
	valid = check_dimension(gpu_3d, fresh_3d, reused_3d, FIELDS_3D) and valid
	gpu_2d.process_material = ParticleProcessMaterial.new()
	reused_2d.emission_points = PackedVector2Array([Vector2.ONE])
	reused_2d.convert_from_particles(gpu_2d)
	if not reused_2d.emission_points.is_empty():
		push_error("stale 2D emission points with present process material")
		valid = false
	gpu_3d.process_material = ParticleProcessMaterial.new()
	reused_3d.emission_points = PackedVector3Array([Vector3.ONE])
	reused_3d.convert_from_particles(gpu_3d)
	if not reused_3d.emission_points.is_empty():
		push_error("stale 3D emission points with present process material")
		valid = false
	for node in [gpu_2d, fresh_2d, reused_2d, gpu_3d, fresh_3d, reused_3d]:
		node.free()
	quit(0 if valid else 1)
