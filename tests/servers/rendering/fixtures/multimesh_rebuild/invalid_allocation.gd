extends SceneTree

func _initialize() -> void:
	RenderingServer.call_on_render_thread(_test)

func _test() -> void:
	var mm := RenderingServer.multimesh_create()
	RenderingServer.multimesh_allocate_data(mm, 3, RenderingServer.MULTIMESH_TRANSFORM_3D, false, false, false)
	var before := RenderingServer.multimesh_get_instance_count(mm)
	RenderingServer.call("multimesh_allocate_data", mm, 0, 99, false, false, false)
	var after := RenderingServer.multimesh_get_instance_count(mm)
	RenderingServer.free_rid(mm)
	print("B4_MULTIMESH_INVALID_FORMAT=" + JSON.stringify({"before": before, "after": after, "pass": before == 3 and after == 3}))
	call_deferred("quit", 0 if before == 3 and after == 3 else 1)
