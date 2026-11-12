extends SceneTree
var failures: Array[String] = []
var signal_counts := {2: 0, 3: 0}
var rows: Array[Dictionary] = []
func _initialize() -> void:
	call_deferred("run")
func finished(dimension: int) -> void:
	signal_counts[dimension] += 1
	# Publish the final CPU buffer before the next inactive update disconnects redraw.
	call_deferred("publish_final_buffer")
func publish_final_buffer() -> void:
	RenderingServer.force_draw()
	RenderingServer.force_sync()
func settle() -> void:
	for frame in 3:
		await process_frame
	RenderingServer.force_draw()
	RenderingServer.force_sync()
func count_active(p: CPUParticles3D) -> Dictionary:
	var buffer := RenderingServer.multimesh_get_buffer(p.get_base())
	var indices: Array[int] = []
	for i in 8:
		if buffer.size() >= (i + 1) * 20 and buffer[i * 20] != 0.0:
			indices.append(i)
	return {"buffer_size": buffer.size(), "active_indices": indices}
func run() -> void:
	for dimension in [2, 3]:
		var p = CPUParticles2D.new() if dimension == 2 else CPUParticles3D.new()
		p.emitting = false
		p.amount = 8
		p.lifetime = 0.875
		p.fixed_fps = 4
		p.one_shot = true
		p.use_fixed_seed = true
		p.seed = 12345
		p.fract_delta = true
		p.explosiveness = 0
		p.randomness = 0
		p.lifetime_randomness = 0
		p.speed_scale = 0
		p.gravity = Vector2.ZERO if dimension == 2 else Vector3.ZERO
		p.finished.connect(finished.bind(dimension))
		root.add_child(p)
		await settle()
		# Request processing temporarily uses speed_scale=1 and fixed_fps=4:
		# exactly four .25 simulation calls, natural updates stay paused.
		p.request_particles_process(1.0)
		await settle()
		var row := {"dimension": dimension, "stage": "four_steps", "emitting": p.emitting, "finished": signal_counts[dimension]}
		if dimension == 3:
			row.merge(count_active(p))
			if row.buffer_size != 160 or not row.active_indices.has(7):
				failures.append("3D missing old-cycle tail birth: " + JSON.stringify(row))
		rows.append(row)
		if p.emitting:
			failures.append(str(dimension) + "D emission did not stop")
		# Explicit residual processing avoids implicitly re-enabling emission.
		p.request_particles_process(0, 2.0)
		await settle()
		row = {"dimension": dimension, "stage": "drained", "emitting": p.emitting, "finished": signal_counts[dimension]}
		if dimension == 3:
			row.merge(count_active(p))
			if row.buffer_size != 160 or not row.active_indices.is_empty():
				failures.append("3D drain retained active slots or lost buffer: " + JSON.stringify(row))
		rows.append(row)
		if signal_counts[dimension] != 1:
			failures.append(str(dimension) + "D expected one finished after drain")
		p.restart(true)
		var restart_emitting_immediately: bool = p.emitting
		if not restart_emitting_immediately:
			failures.append(str(dimension) + "D restart did not immediately reactivate emission")
		p.request_particles_process(1.0)
		await settle()
		row = {"dimension": dimension, "stage": "restart_four_steps", "restart_emitting_immediately": restart_emitting_immediately, "emitting": p.emitting, "finished": signal_counts[dimension]}
		if dimension == 3:
			row.merge(count_active(p))
			if row.buffer_size != 160 or not row.active_indices.has(7):
				failures.append("3D restart missing old-cycle tail birth")
		rows.append(row)
		p.request_particles_process(0, 2.0)
		await settle()
		if signal_counts[dimension] != 2:
			failures.append(str(dimension) + "D expected second finished after restart drain")
		row = {"dimension": dimension, "stage": "restart_drained", "finished": signal_counts[dimension]}
		if dimension == 3:
			row.merge(count_active(p))
			if row.buffer_size != 160 or not row.active_indices.is_empty():
				failures.append("3D restart drain retained active slots or lost buffer: " + JSON.stringify(row))
		rows.append(row)
		p.free()
	print("CPU_ONESHOT_ENGINE=" + JSON.stringify({"rows": rows, "failures": failures, "2d_birth_count": "not publicly observable in this fixture"}))
	quit(1 if failures.size() else 0)
