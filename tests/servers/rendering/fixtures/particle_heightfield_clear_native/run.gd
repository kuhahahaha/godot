extends SceneTree

var fixture: RefCounted
var started: int

func _initialize() -> void:
	if not ClassDB.class_exists("ParticlesHeightfieldClearTestFixture"):
		quit(77)
		return
	fixture = ClassDB.instantiate("ParticlesHeightfieldClearTestFixture")
	started = Time.get_ticks_msec()
	call_deferred("_start")

func _start() -> void:
	var selection_case := OS.get_environment("ANGE_HEIGHTFIELD_SOURCE_SELECTION")
	# H12 needs normal main-RD frame progress for its real SceneCull/readback.
	# Keep the established disabled-loop behavior for every existing case.
	RenderingServer.set_render_loop_enabled(selection_case != "")
	var transform_case := OS.get_environment("ANGE_HEIGHTFIELD_TRANSFORM_CASE")
	var method := "run_geometry" if OS.get_environment("ANGE_HEIGHTFIELD_GEOMETRY") == "1" else "run"
	if method == "run_geometry" and OS.get_environment("ANGE_HEIGHTFIELD_EXTENTS_Y") != "":
		method = "run_geometry_params"
	if transform_case != "":
		method = "run_transformed"
	if OS.get_environment("ANGE_HEIGHTFIELD_LEGACY_NORMAL") == "1":
		method = "run_legacy_normal"
	var neighbor_case := OS.get_environment("ANGE_HEIGHTFIELD_LEGACY_NEIGHBOR_CASE")
	if neighbor_case != "":
		method = "run_legacy_neighbor"
	if selection_case != "":
		method = "run_source_selection"
	if not fixture.has_method(method):
		print("HEIGHTFIELD_GEOMETRY_MISSING ", method)
		quit(1)
		return
	var callback := Callable(fixture, method)
	if method == "run_source_selection":
		var selection_names := ["cast_shadow_off", "alpha_discard", "hidden", "particles_excluded"]
		var selection_index := selection_names.find(selection_case)
		if selection_index < 0:
			quit(1)
			return
		callback = callback.bind(selection_index)
	elif method == "run_legacy_neighbor":
		var neighbor_names := ["sphere", "box", "sdf", "sphere_attractor", "box_attractor", "vector_attractor"]
		var neighbor_index := neighbor_names.find(neighbor_case)
		if neighbor_index < 0:
			quit(1)
			return
		callback = callback.bind(neighbor_index)
	elif method == "run_legacy_normal":
		var legacy_transform_case := 0
		match transform_case:
			"", "identity":
				legacy_transform_case = 0
			"rotation":
				legacy_transform_case = 1
			"uniform":
				legacy_transform_case = 2
			"nonuniform":
				legacy_transform_case = 3
			"tilt_nonuniform_rotation":
				legacy_transform_case = 4
			_:
				quit(1)
				return
		callback = callback.bind(legacy_transform_case)
	elif method == "run_transformed":
		var scale := Vector3.ONE
		match transform_case:
			"rotated":
				scale = Vector3.ONE
			"rotated_uniform":
				scale = Vector3(2, 2, 2)
			"rotated_nonuniform":
				scale = Vector3(2, 3, 4)
			_:
				quit(1)
				return
		callback = callback.bind(scale)
	elif method == "run_geometry":
		callback = callback.bind(float(OS.get_environment("ANGE_HEIGHTFIELD_GEOMETRY_HEIGHT")))
	elif method == "run_geometry_params":
		callback = callback.bind(float(OS.get_environment("ANGE_HEIGHTFIELD_GEOMETRY_HEIGHT")), float(OS.get_environment("ANGE_HEIGHTFIELD_EXTENTS_Y")), float(OS.get_environment("ANGE_HEIGHTFIELD_TRANSFORM_Y")))
	RenderingServer.call_on_render_thread(callback)

func _process(_delta: float) -> bool:
	var result: Dictionary = fixture.call("result")
	if result.has("pass"):
		print("HEIGHTFIELD_CLEAR_NATIVE_PASS " if result["pass"] else "HEIGHTFIELD_CLEAR_NATIVE_FAIL ", JSON.stringify(result))
		quit(0 if result["pass"] else 1)
	elif Time.get_ticks_msec() - started > 30000:
		print("HEIGHTFIELD_CLEAR_NATIVE_FAIL ", JSON.stringify(result))
		quit(1)
	return false
