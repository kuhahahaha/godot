extends SceneTree

var finished_count := [0, 0, 0]

func _initialize() -> void:
	call_deferred("run")

func _finished(index: int) -> void:
	finished_count[index] += 1

func run() -> void:
	var root_node := Node.new()
	root.add_child(root_node)
	var restart_3d := GPUParticles3D.new()
	var emit_3d := GPUParticles3D.new()
	var restart_2d := GPUParticles2D.new()
	var particles := [restart_3d, emit_3d, restart_2d]
	for index in range(particles.size()):
		var particle = particles[index]
		particle.emitting = false
		particle.one_shot = true
		particle.lifetime = 1.0
		particle.explosiveness = 0.75
		particle.finished.connect(_finished.bind(index))
		root_node.add_child(particle)
	restart_3d.restart()
	emit_3d.emitting = true
	restart_2d.restart()
	var emission_window_valid := true
	for frame in range(1, 91):
		await process_frame
		if frame in [30, 45] and not (restart_3d.emitting and emit_3d.emitting and restart_2d.emitting):
			emission_window_valid = false
		if frame in [1, 15, 30, 45, 90]:
			print("STATE frame=%d restart3d=%s emit3d=%s restart2d=%s finished=%s" % [frame, restart_3d.emitting, emit_3d.emitting, restart_2d.emitting, finished_count])
	var valid := emission_window_valid and not restart_3d.emitting and not emit_3d.emitting and not restart_2d.emitting and finished_count == [1, 1, 1]
	print("RESULT %s" % ["PASS" if valid else "FAIL"])
	root_node.free()
	quit(0 if valid else 1)
