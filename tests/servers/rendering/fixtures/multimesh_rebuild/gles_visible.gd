extends SceneTree

func _initialize() -> void:
	RenderingServer.call_on_render_thread(_test)

func _test() -> void:
	var mm := RenderingServer.multimesh_create()
	RenderingServer.multimesh_allocate_data(mm, 3, RenderingServer.MULTIMESH_TRANSFORM_3D, false, false, false)
	RenderingServer.multimesh_set_visible_instances(mm, 1)
	var one := RenderingServer.multimesh_get_visible_instances(mm)
	RenderingServer.multimesh_set_visible_instances(mm, -1)
	var all := RenderingServer.multimesh_get_visible_instances(mm)
	RenderingServer.multimesh_set_visible_instances(mm, 0)
	var zero := RenderingServer.multimesh_get_visible_instances(mm)
	RenderingServer.call("multimesh_allocate_data", mm, 0, 99, false, false, false)
	var retained_count := RenderingServer.multimesh_get_instance_count(mm)
	var retained_visible := RenderingServer.multimesh_get_visible_instances(mm)
	var passed := one == 1 and all == -1 and zero == 0 and retained_count == 3 and retained_visible == 0
	RenderingServer.free_rid(mm)
	print("B4_MULTIMESH_GLES_VISIBLE=" + JSON.stringify({"one": one, "all": all, "zero": zero, "retained_count": retained_count, "retained_visible": retained_visible, "pass": passed}))
	call_deferred("quit", 0 if passed else 1)
