extends SceneTree

func _initialize() -> void:
	var cpu := CPUParticles3D.new()
	var gpu := GPUParticles3D.new()
	gpu.transform_align = GPUParticles3D.TRANSFORM_ALIGN_Z_BILLBOARD
	gpu.convert_from_particles(cpu)
	var valid := gpu.transform_align == GPUParticles3D.TRANSFORM_ALIGN_DISABLED
	gpu.free()
	cpu.free()
	if not valid:
		push_error("CPU to GPU 3D conversion retained old transform alignment")
	quit(0 if valid else 1)
