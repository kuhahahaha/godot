extends SceneTree

var viewport_rid: RID
var rows: Array[Dictionary] = []

func _initialize() -> void:
	call_deferred("_start")

func _start() -> void:
	viewport_rid = root.get_viewport_rid()
	RenderingServer.call_on_render_thread(_test)

func _data(count: int, moved: bool) -> PackedFloat32Array:
	var data := PackedFloat32Array()
	data.resize(count * 12)
	for i in range(count):
		data[i * 12] = 1.0
		data[i * 12 + 5] = 1.0
		data[i * 12 + 10] = 1.0
		data[i * 12 + 3] = float(i + 1) * 2.0 if moved else 0.0
	return data

func _command(rd: RenderingDevice, mm: RID, count: int, label: String) -> void:
	var command := RenderingServer.multimesh_get_command_buffer_rd_rid(mm)
	var words: Array[int] = []
	if command.is_valid():
		var bytes := rd.buffer_get_data(command)
		for offset in range(0, bytes.size(), 4):
			words.append(bytes.decode_u32(offset))
	rows.append({"case": label, "pass": words == [3, count, 0, 0, 0], "words": words})

func _test() -> void:
	var rd := RenderingServer.get_rendering_device()
	if rd == null:
		call_deferred("_finish", 77)
		return
	var mesh := RenderingServer.mesh_create()
	var arrays: Array = []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = PackedVector3Array([Vector3(-0.25, -0.25, 0), Vector3(0.25, -0.25, 0), Vector3(0, 0.25, 0)])
	RenderingServer.mesh_add_surface_from_arrays(mesh, RenderingServer.PRIMITIVE_TRIANGLES, arrays)
	var mm := RenderingServer.multimesh_create()
	RenderingServer.multimesh_allocate_data(mm, 3, RenderingServer.MULTIMESH_TRANSFORM_3D, false, false, true)
	RenderingServer.multimesh_set_mesh(mm, mesh)
	RenderingServer.multimesh_set_buffer(mm, _data(3, false))
	var before := RenderingServer.multimesh_get_buffer_rd_rid(mm)
	RenderingServer.viewport_set_use_taa(viewport_rid, true)
	var moved := _data(3, true)
	RenderingServer.multimesh_set_buffer(mm, moved)
	var after := RenderingServer.multimesh_get_buffer_rd_rid(mm)
	var bytes := rd.buffer_get_data(after)
	var expected := moved.to_byte_array()
	expected.append_array(moved.to_byte_array())
	rows.append({"case": "M09_motion_buffer_replaced", "pass": before != after, "replaced": before != after})
	rows.append({"case": "M09_complete_current_previous", "pass": bytes == expected, "bytes": bytes.size()})
	_command(rd, mm, 3, "M09_original_command")
	RenderingServer.multimesh_allocate_data(mm, 5, RenderingServer.MULTIMESH_TRANSFORM_3D, false, false, true)
	var resized := _data(5, true)
	RenderingServer.multimesh_set_buffer(mm, resized)
	var resized_bytes := rd.buffer_get_data(RenderingServer.multimesh_get_buffer_rd_rid(mm))
	var resized_expected := resized.to_byte_array()
	resized_expected.append_array(resized.to_byte_array())
	rows.append({"case": "M09_motion_after_same_mesh_resize", "pass": resized_bytes == resized_expected, "bytes": resized_bytes.size()})
	_command(rd, mm, 5, "M09_resized_command")
	RenderingServer.viewport_set_use_taa(viewport_rid, false)
	RenderingServer.free_rid(mm)
	RenderingServer.free_rid(mesh)
	var passed := true
	for row in rows:
		passed = passed and bool(row.pass)
	call_deferred("_finish", 0 if passed else 1)

func _finish(code: int) -> void:
	print("B4_MULTIMESH_MOTION=" + JSON.stringify(rows))
	quit(code)
