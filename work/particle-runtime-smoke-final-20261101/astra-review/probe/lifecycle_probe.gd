extends SceneTree

var failures = []
var rows = []

func _initialize():
	call_deferred("run")

func draw_connections(p):
	var count = 0
	for connection in RenderingServer.get_signal_connection_list("frame_pre_draw"):
		if connection.callable.get_object() == p:
			count += 1
	return count

func make_particles(dimension):
	var p = CPUParticles2D.new() if dimension == 2 else CPUParticles3D.new()
	p.emitting = false
	p.fixed_fps = 30
	p.lifetime = 1.0
	p.amount = 16
	p.use_fixed_seed = true
	p.seed = 7
	root.add_child(p)
	return p

func check(condition, message):
	if not condition:
		failures.append(message)

func run():
	for dimension in [2, 3]:
		for one_shot in [false, true]:
			var p = make_particles(dimension)
			p.one_shot = one_shot
			for frame in 3:
				await process_frame
			var finished = {"count": 0}
			p.finished.connect(func(): finished.count += 1)
			p.request_particles_process(0.25, 0.0 if one_shot else 0.125)
			check(p.is_processing_internal(), str(dimension) + "D: manual start must enable processing")
			var frames = 0
			while p.is_processing_internal() and frames < 240:
				await process_frame
				frames += 1
			for frame in 3:
				await process_frame
			check(not p.is_processing_internal() and not p.emitting and finished.count == 1,
				str(dimension) + "D: lifecycle must finish exactly once and stop, one_shot=" + str(one_shot))
			rows.append({"dimension": dimension, "one_shot": one_shot, "frames": frames,
				"finished": finished.count, "processing": p.is_processing_internal(), "emitting": p.emitting})
			p.free()

		var tail = make_particles(dimension)
		for frame in 3:
			await process_frame
		tail.request_particles_process(0.0, 0.25)
		for frame in 3:
			await process_frame
		check(not tail.is_processing_internal() and not tail.emitting, str(dimension) + "D: residual-only must not revive an idle system")
		tail.free()

		var active = make_particles(dimension)
		active.local_coords = false
		for frame in 3:
			await process_frame
		active.request_particles_process(0.25, 0.0)
		var before = {"processing": active.is_processing_internal(), "emitting": active.emitting,
			"draw": draw_connections(active), "physics_processing": active.is_physics_processing_internal()}
		# An interpolation policy change must not be consumed by a rejected request.
		if dimension == 2:
			physics_interpolation = true
		active.request_particles_process(16777216.0, 0.0)
		var after = {"processing": active.is_processing_internal(), "emitting": active.emitting,
			"draw": draw_connections(active), "physics_processing": active.is_physics_processing_internal()}
		check(before == after, str(dimension) + "D: rejected request must preserve active observable state")
		rows.append({"dimension": dimension, "rejected_before": before, "rejected_after": after})
		# Keep the existing pending-request contract, then clear that pending input.
		active.request_particles_process(0.0, 0.0)
		if dimension == 2:
			check(active.is_physics_processing_internal(), "2D: admitted request must refresh interpolation")
		active.free()
		physics_interpolation = false
	print("CPU_MANUAL_LIFECYCLE=" + JSON.stringify({"rows": rows, "failures": failures}))
	quit(42 if failures else 0)
