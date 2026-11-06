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
	RenderingServer.set_render_loop_enabled(false)
	var method := "run_geometry" if OS.get_environment("ANGE_HEIGHTFIELD_GEOMETRY") == "1" else "run"
	if not fixture.has_method(method):
		print("HEIGHTFIELD_GEOMETRY_MISSING ", method)
		quit(1)
		return
	var callback := Callable(fixture, method)
	if method == "run_geometry":
		callback = callback.bind(float(OS.get_environment("ANGE_HEIGHTFIELD_GEOMETRY_HEIGHT")))
	RenderingServer.call_on_render_thread(callback)

func _process(_delta: float) -> bool:
	var result: Dictionary = fixture.call("result")
	if result.has("pass"):
		print("HEIGHTFIELD_CLEAR_NATIVE_PASS ", JSON.stringify(result))
		quit(0 if result["pass"] else 1)
	elif Time.get_ticks_msec() - started > 30000:
		print("HEIGHTFIELD_CLEAR_NATIVE_FAIL ", JSON.stringify(result))
		quit(1)
	return false
