extends SceneTree

func _initialize() -> void:
	var valid := true
	var gpu_2d := GPUParticles2D.new()
	var cpu_2d := CPUParticles2D.new()
	gpu_2d.emitting = true
	gpu_2d.amount_ratio = 0.0
	cpu_2d.convert_from_particles(gpu_2d)
	if cpu_2d.emitting:
		push_error("2D zero-ratio GPU source resumed emitting on CPU conversion")
		valid = false
	gpu_2d.amount_ratio = 1.0
	cpu_2d.convert_from_particles(gpu_2d)
	if not cpu_2d.emitting:
		push_error("2D nonzero-ratio GPU source did not restore CPU emission")
		valid = false

	var gpu_3d := GPUParticles3D.new()
	var cpu_3d := CPUParticles3D.new()
	gpu_3d.emitting = true
	gpu_3d.amount_ratio = 0.0
	cpu_3d.convert_from_particles(gpu_3d)
	if cpu_3d.emitting:
		push_error("3D zero-ratio GPU source resumed emitting on CPU conversion")
		valid = false
	gpu_3d.amount_ratio = 1.0
	cpu_3d.convert_from_particles(gpu_3d)
	if not cpu_3d.emitting:
		push_error("3D nonzero-ratio GPU source did not restore CPU emission")
		valid = false

	for node in [gpu_2d, cpu_2d, gpu_3d, cpu_3d]:
		node.free()
	quit(0 if valid else 1)
