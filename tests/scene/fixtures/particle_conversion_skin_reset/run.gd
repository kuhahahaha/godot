extends SceneTree

func _initialize() -> void:
	var cpu := CPUParticles3D.new()
	var gpu := GPUParticles3D.new()
	var old_skin := Skin.new()
	gpu.draw_skin = old_skin
	gpu.convert_from_particles(cpu)
	var valid := gpu.draw_skin == null
	gpu.free()
	cpu.free()
	if not valid:
		push_error("CPU to GPU 3D conversion retained old skin")
	quit(0 if valid else 1)
