extends SceneTree

func _initialize() -> void:
	RenderingServer.call_on_render_thread(_test)

func _test() -> void:
	var rd := RenderingServer.get_rendering_device()
	if rd == null:
		call_deferred("quit", 77)
		return
	var mesh := RenderingServer.mesh_create()
	var arrays: Array = []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = PackedVector3Array([Vector3(-0.25, -0.25, 0), Vector3(0.25, -0.25, 0), Vector3(0, 0.25, 0)])
	RenderingServer.mesh_add_surface_from_arrays(mesh, RenderingServer.PRIMITIVE_TRIANGLES, arrays)
	var mm := RenderingServer.multimesh_create()
	RenderingServer.multimesh_allocate_data(mm, 1024, RenderingServer.MULTIMESH_TRANSFORM_3D, false, false, true)
	RenderingServer.multimesh_set_mesh(mm, mesh)
	var data := PackedFloat32Array()
	data.resize(1024 * 12)
	for i in range(1024):
		data[i * 12] = 1.0
		data[i * 12 + 5] = 1.0
		data[i * 12 + 10] = 1.0
	RenderingServer.multimesh_set_buffer(mm, data)
	RenderingServer.multimesh_set_visible_instances(mm, 1)
	RenderingServer.multimesh_instance_set_transform(mm, 700, Transform3D(Basis.IDENTITY, Vector3(7, 0, 0)))
	RenderingServer.multimesh_get_aabb(mm)
	RenderingServer.multimesh_instance_set_transform(mm, 0, Transform3D(Basis.IDENTITY, Vector3(1, 0, 0)))
	RenderingServer.multimesh_get_aabb(mm)
	var buffer := RenderingServer.multimesh_get_buffer_rd_rid(mm)
	var hidden := rd.buffer_get_data(buffer).decode_float(700 * 48 + 12)
	RenderingServer.multimesh_set_visible_instances(mm, -1)
	RenderingServer.multimesh_get_aabb(mm)
	var all := rd.buffer_get_data(buffer).decode_float(700 * 48 + 12)
	var command := RenderingServer.multimesh_get_command_buffer_rd_rid(mm)
	var all_count := rd.buffer_get_data(command).decode_u32(4)
	RenderingServer.multimesh_set_visible_instances(mm, 1)
	RenderingServer.multimesh_set_visible_instances(mm, 1024)
	RenderingServer.multimesh_get_aabb(mm)
	var explicit := rd.buffer_get_data(buffer).decode_float(700 * 48 + 12)
	var passed := hidden == 0.0 and all == 7.0 and all_count == 1024 and explicit == 7.0
	RenderingServer.free_rid(mm)
	RenderingServer.free_rid(mesh)
	print("B4_MULTIMESH_CACHED_VISIBLE=" + JSON.stringify({"hidden": hidden, "all": all, "all_count": all_count, "explicit": explicit, "pass": passed}))
	call_deferred("quit", 0 if passed else 1)
