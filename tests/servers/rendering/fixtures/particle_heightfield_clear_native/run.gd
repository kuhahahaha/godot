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
	if not fixture.has_method(method):
		print("HEIGHTFIELD_GEOMETRY_MISSING ", method)
		quit(1)
		return
	var callback := Callable(fixture, method)
	if method == "run_legacy_neighbor":
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
			"rotated":
				legacy_transform_case = 1
			"rotated_uniform":
				legacy_transform_case = 2
			"rotated_nonuniform":
				legacy_transform_case = 3
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
