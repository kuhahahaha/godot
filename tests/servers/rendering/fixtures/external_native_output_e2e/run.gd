extends SceneTree

var executed := false

func _initialize() -> void:
	RenderingServer.set_render_loop_enabled(false)

func _process(_delta: float) -> bool:
	if executed:
		return true
	executed = true
	if not ClassDB.class_exists("ParticlesScenePairsTestFixture"):
		printerr("WAVE_G_NATIVE_OUTPUT_MISSING_FIXTURE")
		quit(77)
		return true
	var fixture: RefCounted = ClassDB.instantiate("ParticlesScenePairsTestFixture")
	var result: Dictionary = fixture.call("run_wave_g_native_output")
	print("WAVE_G_NATIVE_OUTPUT_RESULT=", JSON.stringify(result))
	var frames_drawn: int = int(result.get("frames_drawn", Engine.get_frames_drawn()))
	var check_count: int = int(result.get("check_count", (result.get("checks", {}) as Dictionary).size()))
	if bool(result.get("pass", false)) and frames_drawn == 0:
		print("WAVE_G_NATIVE_OUTPUT_PASS method=", result.get("rendering_method", ""), " checks=", check_count, " frames_drawn=", frames_drawn)
		quit(0)
	else:
		printerr("WAVE_G_NATIVE_OUTPUT_FAIL failures=", JSON.stringify(result.get("failures", [])))
		quit(1)
	return true
