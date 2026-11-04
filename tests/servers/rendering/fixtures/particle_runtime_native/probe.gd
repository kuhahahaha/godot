extends SceneTree

func _initialize() -> void:
	call_deferred("_run")

func _finish(data: Dictionary, code: int) -> void:
	print("PARTICLES_GPU_RESULT ", JSON.stringify(data))
	quit(code)

func _run() -> void:
	if not ClassDB.class_exists("ParticlesRuntimeTestFixture"):
		_finish({"unavailable": true, "reason": "tests_only_module_missing", "gpu_pass": false}, 41)
		return
	var fixture: RefCounted = ClassDB.instantiate("ParticlesRuntimeTestFixture")
	if fixture == null:
		_finish({"unavailable": true, "reason": "fixture_instantiation_failed", "gpu_pass": false}, 42)
		return
	# Three frames only warm up the ordinary main RD; they never prove completion.
	for i in range(3):
		await process_frame
	var begin_error: int = fixture.call("begin")
	if begin_error != OK:
		_finish({"unavailable": true, "reason": "main_metal_owner_unavailable", "error": begin_error, "gpu_pass": false}, 42)
		return
	# This frame budget can only fail. Native callbacks and retirement witnesses pass.
	for frames in range(180):
		await process_frame
		var advance_error: int = fixture.call("advance")
		if advance_error == ERR_BUSY:
			continue
		var data: Dictionary = fixture.call("result")
		data["advance_error"] = advance_error
		data["rendered_frames"] = frames + 1
		var checks: Array = data.get("checks", [])
		var native_checks_ok := checks.size() == 292
		for row in checks:
			native_checks_ok = native_checks_ok and typeof(row) == TYPE_DICTIONARY and row.get("ok") == true
		data["gpu_pass"] = advance_error == OK and data.get("done", false) and data.get("released", false) and data.get("driver", "") == "metal" and data.get("renderer", "") == "mobile" and data.get("failures", ["missing"]).is_empty() and data.get("readbacks_queued", 0) == 6 and data.get("readbacks_completed", 0) == 6 and native_checks_ok
		_finish(data, 0 if data["gpu_pass"] else 43)
		return
	var data: Dictionary = fixture.call("result")
	data["gpu_pass"] = false
	data["reason"] = "bounded_frame_budget_exhausted"
	_finish(data, 44)
