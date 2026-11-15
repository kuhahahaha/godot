extends SceneTree

var executed := false

func _initialize() -> void:
	RenderingServer.set_render_loop_enabled(false)

func _process(_delta: float) -> bool:
	if executed:
		return true
	executed = true
	if not ClassDB.class_exists("ParticlesScenePairsTestFixture"):
		printerr("MATERIAL_QUALIFICATION_ACTION7_MISSING_FIXTURE")
		quit(77)
		return true
	var fixture: RefCounted = ClassDB.instantiate("ParticlesScenePairsTestFixture")
	var result: Dictionary = fixture.call("run_material_qualification_action7")
	print("MATERIAL_QUALIFICATION_ACTION7_RESULT=", JSON.stringify(result))
	if bool(result.get("pass", false)) and int(result.get("frames_drawn", -1)) == 0:
		print("MATERIAL_QUALIFICATION_ACTION7_PASS method=", result.get("rendering_method", ""), " checks=", result.get("check_count", 0), " frames_drawn=0")
		quit(0)
	else:
		printerr("MATERIAL_QUALIFICATION_ACTION7_FAIL failures=", JSON.stringify(result.get("failures", [])))
		quit(1)
	return true
