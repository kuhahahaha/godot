extends SceneTree

var failures: Array[String] = []
var observations: Array[Dictionary] = []

func _initialize() -> void:
	call_deferred("_run")

func _run_case(label: String, emitter_basis: Basis, emitter_origin: Vector3, camera_basis: Basis, points: PackedVector3Array) -> void:
	var root := Node3D.new()
	get_root().add_child(root)
	var camera := Camera3D.new()
	root.add_child(camera)
	camera.global_transform = Transform3D(camera_basis, Vector3.ZERO)
	camera.make_current()
	var particle := CPUParticles3D.new()
	particle.emitting = false
	particle.amount = 64
	particle.lifetime = 100.0
	particle.local_coords = true
	particle.draw_order = CPUParticles3D.DRAW_ORDER_VIEW_DEPTH
	particle.use_fixed_seed = true
	particle.seed = 1
	particle.explosiveness = 1.0
	particle.gravity = Vector3.ZERO
	particle.initial_velocity_min = 0.0
	particle.initial_velocity_max = 0.0
	particle.emission_shape = CPUParticles3D.EMISSION_SHAPE_POINTS
	particle.emission_points = points
	particle.mesh = QuadMesh.new()
	root.add_child(particle)
	particle.global_transform = Transform3D(emitter_basis, emitter_origin)
	particle.force_update_transform()
	particle.request_particles_process(0.125)
	# The public request synchronously processes CPU particles. Its ordinary
	# frame_pre_draw callback performs the multimesh upload before this signal.
	await RenderingServer.frame_post_draw
	var buffer := RenderingServer.multimesh_get_buffer(particle.get_base())
	var valid := buffer.size() == 64 * 20
	var sorted := valid
	var both_points: Array[bool] = [false, false]
	var depths: Array[float] = []
	var previous := -INF
	if valid:
		for i in range(64):
			var offset := i * 20
			var position := Vector3(buffer[offset + 3], buffer[offset + 7], buffer[offset + 11])
			both_points[0] = both_points[0] or position.is_equal_approx(points[0])
			both_points[1] = both_points[1] or position.is_equal_approx(points[1])
			# Independent reference: project actual world positions on camera Z.
			# A common camera origin term cannot affect the ordering.
			var world_position := particle.global_transform * position
			var depth := camera.global_basis.z.dot(world_position)
			sorted = sorted and depth + 0.00001 >= previous
			previous = depth
			depths.append(depth)
	var ok := valid and both_points[0] and both_points[1] and sorted
	observations.append({"case": label, "ok": ok, "exact_multimesh_bytes": valid, "both_fixed_points_emitted": both_points[0] and both_points[1], "world_depth_nondecreasing": sorted, "world_depths": depths})
	if not ok:
		failures.append(label)
	root.queue_free()
	await process_frame

func _run() -> void:
	await process_frame
	var a := PackedVector3Array([Vector3(0, 0, -1), Vector3(1, 0, 0)])
	await _run_case("translation", Basis.IDENTITY, Vector3(100, 0, 0), Basis.IDENTITY, a)
	await _run_case("rotation", Basis(Vector3.UP, 0.6), Vector3.ZERO, Basis.IDENTITY, a)
	await _run_case("nonuniform_scale", Basis.from_scale(Vector3(4, 1, 0.25)), Vector3.ZERO, Basis(Vector3.UP, 0.7), PackedVector3Array([Vector3(1, 0, 0), Vector3(0, 0, 1)]))
	await _run_case("shear", Basis(Vector3(1, 0, 0), Vector3(0, 1, 0), Vector3(2, 0, 1)), Vector3.ZERO, Basis.IDENTITY, a)
	print("CPU_PARTICLES_DEPTH_RESULT ", JSON.stringify({"pass": failures.is_empty() and observations.size() == 4, "failures": failures, "cases": observations, "scope": "actual public CPU3D producer and multimesh depth order; no pixel assertion"}))
	quit(0 if failures.is_empty() and observations.size() == 4 else 43)
