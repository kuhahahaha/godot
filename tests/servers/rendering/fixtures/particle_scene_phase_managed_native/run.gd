extends SceneTree
var fixture: RefCounted
var started: int
func _initialize() -> void:
	if not ClassDB.class_exists("ParticlesScenePairsTestFixture"):
		print("SCENE_PHASE_MANAGED_NATIVE_MISSING")
		quit(1)
		return
	fixture = ClassDB.instantiate("ParticlesScenePairsTestFixture")
	started = Time.get_ticks_msec()
	call_deferred("_start")
func _start() -> void:
	RenderingServer.set_render_loop_enabled(true)
	RenderingServer.call_on_render_thread(Callable(fixture, "run_phase"))
func _process(_delta: float) -> bool:
	if fixture == null:
		return false
	# Headless main loops may not enter RenderingServer::draw automatically.
	# Force the production draw path so _draw() performs scene_phase_dispatch.
	if RenderingServer.is_render_loop_enabled():
		RenderingServer.force_draw(false, _delta)
	RenderingServer.call_on_render_thread(Callable(fixture, "advance"))
	var result: Dictionary = fixture.call("result")
	if result.has("pass"):
		print("SCENE_PHASE_MANAGED_NATIVE ", JSON.stringify(result))
		quit(0 if result["pass"] else 1)
	elif Time.get_ticks_msec() - started > 30000:
		print("SCENE_PHASE_MANAGED_NATIVE_TIMEOUT")
		quit(1)
	return false
