extends SceneTree

var rows: Array[Dictionary] = []

func _initialize() -> void:
	call_deferred("_start")

func _start() -> void:
	RenderingServer.call_on_render_thread(_on_render_thread)

func _arrays(indexed: bool = false, center: float = 0.0) -> Array:
	var arrays: Array = []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = PackedVector3Array([Vector3(center - 0.25, -0.25, 0), Vector3(center + 0.25, -0.25, 0), Vector3(center, 0.25, 0)])
	if indexed:
		arrays[Mesh.ARRAY_INDEX] = PackedInt32Array([0, 1, 2, 0, 2, 1])
	return arrays

func _record(label: String, passed: bool, observed: Variant) -> void:
	rows.append({"case": label, "pass": passed, "observed": observed})

func _command(rd: RenderingDevice, mm: RID, expected: Array, label: String) -> void:
	var command := RenderingServer.multimesh_get_command_buffer_rd_rid(mm)
	var words: Array[int] = []
	if command.is_valid():
		var bytes := rd.buffer_get_data(command)
		for offset in range(0, bytes.size(), 4):
			words.append(bytes.decode_u32(offset))
	_record(label, command.is_valid() and words == expected, words)

func _bounds(mm: RID, position: Vector3, size: Vector3, label: String) -> void:
	var actual := RenderingServer.multimesh_get_aabb(mm)
	_record(label, actual.position == position and actual.size == size, {"position": actual.position, "size": actual.size})

func _on_render_thread() -> void:
	var rd := RenderingServer.get_rendering_device()
	if rd == null:
		_record("main_rd", false, "unavailable")
		call_deferred("_finish", 77)
		return
	var mesh := RenderingServer.mesh_create()
	RenderingServer.mesh_add_surface_from_arrays(mesh, RenderingServer.PRIMITIVE_TRIANGLES, _arrays())
	var mm := RenderingServer.multimesh_create()
	RenderingServer.multimesh_allocate_data(mm, 3, RenderingServer.MULTIMESH_TRANSFORM_3D, false, false, false)
	RenderingServer.multimesh_set_mesh(mm, mesh)
	RenderingServer.multimesh_allocate_data(mm, 3, RenderingServer.MULTIMESH_TRANSFORM_3D, false, false, true)
	_command(rd, mm, [3, 3, 0, 0, 0], "M01_M03_initial_nonindexed_command")
	RenderingServer.multimesh_set_visible_instances(mm, 1)
	_command(rd, mm, [3, 1, 0, 0, 0], "M07_visible_one_control")
	RenderingServer.multimesh_set_visible_instances(mm, -1)
	_command(rd, mm, [3, 3, 0, 0, 0], "M07_visible_all_sentinel")
	RenderingServer.multimesh_set_visible_instances(mm, 0)
	_command(rd, mm, [3, 0, 0, 0, 0], "M08_valid_zero_command")
	RenderingServer.multimesh_set_visible_instances(mm, 3)
	_command(rd, mm, [3, 3, 0, 0, 0], "M08_visible_capacity_control")
	RenderingServer.multimesh_set_visible_instances(mm, -1)
	RenderingServer.multimesh_allocate_data(mm, 5, RenderingServer.MULTIMESH_TRANSFORM_3D, false, false, true)
	_command(rd, mm, [3, 5, 0, 0, 0], "M04_same_mesh_resize")
	RenderingServer.multimesh_set_mesh(mm, RID())
	_record("M05_null_mesh_no_command", not RenderingServer.multimesh_get_command_buffer_rd_rid(mm).is_valid(), null)
	RenderingServer.multimesh_set_mesh(mm, mesh)
	RenderingServer.multimesh_allocate_data(mm, 5, RenderingServer.MULTIMESH_TRANSFORM_3D, false, false, false)
	_record("M02_same_shape_true_to_false", not RenderingServer.multimesh_get_command_buffer_rd_rid(mm).is_valid(), null)
	RenderingServer.multimesh_allocate_data(mm, 5, RenderingServer.MULTIMESH_TRANSFORM_3D, true, true, true)
	_command(rd, mm, [3, 5, 0, 0, 0], "M10_3D_color_custom_rebuild")
	RenderingServer.multimesh_allocate_data(mm, 2, RenderingServer.MULTIMESH_TRANSFORM_2D, false, false, true)
	var data := PackedFloat32Array()
	data.resize(16)
	data[0] = 1.0
	data[5] = 1.0
	data[8] = 1.0
	data[13] = 1.0
	data[11] = 2.0
	RenderingServer.multimesh_set_buffer(mm, data)
	var instance_buffer := RenderingServer.multimesh_get_buffer_rd_rid(mm)
	var buffer_bytes := rd.buffer_get_data(instance_buffer)
	_record("M10_2D_layout_size", buffer_bytes.size() == 64, buffer_bytes.size())
	_command(rd, mm, [3, 2, 0, 0, 0], "M10_layout_rebuild_command")
	_bounds(mm, Vector3(-0.25, -0.25, 0), Vector3(2.5, 0.5, 0.00001), "M06_auto_bounds_before_mutation")
	RenderingServer.mesh_clear(mesh)
	_record("M06_in_place_clear_no_command", not RenderingServer.multimesh_get_command_buffer_rd_rid(mm).is_valid(), null)
	_bounds(mm, Vector3.ZERO, Vector3(2, 0, 0), "M06_auto_bounds_after_clear")
	RenderingServer.mesh_add_surface_from_arrays(mesh, RenderingServer.PRIMITIVE_TRIANGLES, _arrays())
	_command(rd, mm, [3, 2, 0, 0, 0], "M06_in_place_add_one")
	RenderingServer.mesh_add_surface_from_arrays(mesh, RenderingServer.PRIMITIVE_TRIANGLES, _arrays(true, 10.0))
	_command(rd, mm, [3, 2, 0, 0, 0, 6, 2, 0, 0, 0], "M06_in_place_add_indexed_second")
	_bounds(mm, Vector3(-0.25, -0.25, 0), Vector3(12.5, 0.5, 0.00001), "M06_auto_bounds_before_remove")
	RenderingServer.mesh_surface_remove(mesh, 0)
	_command(rd, mm, [6, 2, 0, 0, 0], "M06_in_place_remove_first")
	_bounds(mm, Vector3(9.75, -0.25, 0), Vector3(2.5, 0.5, 0.00001), "M06_auto_bounds_after_remove")
	RenderingServer.mesh_clear(mesh)
	RenderingServer.mesh_add_surface_from_arrays(mesh, RenderingServer.PRIMITIVE_TRIANGLES, _arrays())
	RenderingServer.mesh_add_surface_from_arrays(mesh, RenderingServer.PRIMITIVE_TRIANGLES, _arrays(true, 10.0))
	RenderingServer.multimesh_set_visible_instances(mm, 1)
	if "--stale-second-surface" in OS.get_cmdline_user_args():
		# No drawable scene uses this RID. Model a missed second-surface update.
		var stale_count := PackedByteArray()
		stale_count.resize(4)
		stale_count.encode_u32(0, 2)
		var command := RenderingServer.multimesh_get_command_buffer_rd_rid(mm)
		_record("control_second_surface_patch", rd.buffer_update(command, 24, 4, stale_count) == OK, 2)
	_command(rd, mm, [3, 1, 0, 0, 0, 6, 1, 0, 0, 0], "M07_multi_surface_visible_one")
	RenderingServer.multimesh_set_visible_instances(mm, 0)
	_command(rd, mm, [3, 0, 0, 0, 0, 6, 0, 0, 0, 0], "M08_multi_surface_visible_zero")
	RenderingServer.multimesh_set_visible_instances(mm, -1)
	_command(rd, mm, [3, 2, 0, 0, 0, 6, 2, 0, 0, 0], "M07_multi_surface_visible_all")
	RenderingServer.multimesh_allocate_data(mm, 7, RenderingServer.MULTIMESH_TRANSFORM_3D, true, true, true)
	_command(rd, mm, [3, 7, 0, 0, 0, 6, 7, 0, 0, 0], "M04_multi_surface_capacity_3D_fields")
	var fields_bytes := rd.buffer_get_data(RenderingServer.multimesh_get_buffer_rd_rid(mm)).size()
	_record("M10_multi_surface_3D_fields_size", fields_bytes == 560, fields_bytes)
	RenderingServer.multimesh_allocate_data(mm, 2, RenderingServer.MULTIMESH_TRANSFORM_2D, false, false, true)
	_command(rd, mm, [3, 2, 0, 0, 0, 6, 2, 0, 0, 0], "M10_multi_surface_2D_rebuild")
	var plain_bytes := rd.buffer_get_data(RenderingServer.multimesh_get_buffer_rd_rid(mm)).size()
	_record("M10_multi_surface_2D_size", plain_bytes == 64, plain_bytes)
	RenderingServer.multimesh_set_mesh(mm, RID())
	_record("M05_multi_surface_null_mesh_no_command", not RenderingServer.multimesh_get_command_buffer_rd_rid(mm).is_valid(), null)
	RenderingServer.multimesh_set_mesh(mm, mesh)
	_command(rd, mm, [3, 2, 0, 0, 0, 6, 2, 0, 0, 0], "M05_multi_surface_mesh_restore")
	RenderingServer.mesh_clear(mesh)
	_record("M05_multi_surface_empty_mesh_no_command", not RenderingServer.multimesh_get_command_buffer_rd_rid(mm).is_valid(), null)
	RenderingServer.mesh_add_surface_from_arrays(mesh, RenderingServer.PRIMITIVE_TRIANGLES, _arrays())
	RenderingServer.mesh_add_surface_from_arrays(mesh, RenderingServer.PRIMITIVE_TRIANGLES, _arrays(true, 10.0))
	_command(rd, mm, [3, 2, 0, 0, 0, 6, 2, 0, 0, 0], "M05_multi_surface_empty_mesh_restore")
	RenderingServer.free_rid(mm)
	RenderingServer.free_rid(mesh)
	var passed := true
	for row in rows:
		passed = passed and bool(row.pass)
	call_deferred("_finish", 0 if passed else 1)

func _finish(code: int) -> void:
	print("B4_MULTIMESH_REBUILD_MATRIX=" + JSON.stringify(rows))
	quit(code)
