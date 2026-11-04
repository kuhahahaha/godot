extends Node
var rd: RenderingDevice
var retained: Array[RID] = []
var cases: Array = []
var replies: Dictionary = {}
var result_rows: Array = []
var failed := false
var pending := 0
var mutant := false

func _ready() -> void:
	var all_cases: Array = JSON.parse_string(FileAccess.get_file_as_string("res://cases.json"))
	mutant = "--mutant" in OS.get_cmdline_user_args()
	for item in all_cases:
		if not mutant or item.name == "source_invalid":
			cases.append(item)
	pending = cases.size() * 2
	get_tree().create_timer(60).timeout.connect(func(): _fatal("readback timeout"))
	RenderingServer.call_on_render_thread(_record)

func _fatal(message: String) -> void:
	if failed:
		return
	failed = true
	print("GUARD_FATAL ", message)
	if rd != null and not retained.is_empty():
		RenderingServer.call_on_render_thread(_cleanup)
	else:
		get_tree().quit(2)

func _retain(rid: RID) -> RID:
	if rid.is_valid():
		retained.append(rid)
	return rid

func _push(capacity: int, surfaces: int, base: int, count: int) -> PackedByteArray:
	var data := PackedByteArray()
	data.resize(32)
	var words := [capacity, 55, 0, surfaces, base, count, 0, 0]
	for i in words.size():
		data.encode_u32(i * 4, words[i])
	return data

func _dispatch(list: int, pipeline: RID, uniforms: RID, push: PackedByteArray, groups: int) -> void:
	rd.compute_list_bind_compute_pipeline(list, pipeline)
	rd.compute_list_bind_uniform_set(list, uniforms, 0)
	rd.compute_list_set_push_constant(list, push, 32)
	rd.compute_list_dispatch(list, groups, 1, 1)

func _record() -> void:
	rd = RenderingServer.get_rendering_device()
	if rd == null:
		_fatal.call_deferred("main RD unavailable")
		return
	print("GUARD_DEVICE ", rd.get_device_name(), " vendor=", rd.get_device_vendor_name(), " main_rd=true mutant=", mutant)
	var shaders: Dictionary = {}
	var pipelines: Dictionary = {}
	for mode in ["INIT", "VALIDATE", "FINALIZE", "INIT_EMPTY", "FINALIZE_EMPTY"]:
		var spirv := RDShaderSPIRV.new()
		var filename: String = "INIT_MUTANT" if mutant and mode == "INIT" else mode
		spirv.set_stage_bytecode(RenderingDevice.SHADER_STAGE_COMPUTE, FileAccess.get_file_as_bytes("res://" + filename + ".spv"))
		var shader := _retain(rd.shader_create_from_spirv(spirv))
		if not shader.is_valid():
			_fatal.call_deferred("shader create " + mode)
			return
		print("NUMERIC_SHADER_FLAGS ", mode, " requested=", rd.shader_get_compile_flags(shader))
		var pipeline := _retain(rd.compute_pipeline_create(shader))
		if not pipeline.is_valid():
			_fatal.call_deferred("pipeline create " + mode)
			return
		shaders[mode] = shader
		pipelines[mode] = pipeline
	for item in cases:
		var buffers: Array[RID] = []
		for binding in 8:
			if item.capacity == 0 and binding < 2:
				buffers.append(RID())
				continue
			var data := FileAccess.get_file_as_bytes("res://inputs/" + item.name + "-" + str(binding) + ".bin")
			var buffer := _retain(rd.storage_buffer_create(data.size(), data))
			if not buffer.is_valid():
				_fatal.call_deferred("buffer create " + item.name + ":" + str(binding))
				return
			buffers.append(buffer)
		var uniforms: Array[RDUniform] = []
		for binding in 8:
			if not buffers[binding].is_valid():
				continue
			var uniform := RDUniform.new()
			uniform.uniform_type = RenderingDevice.UNIFORM_TYPE_STORAGE_BUFFER
			uniform.binding = binding
			uniform.add_id(buffers[binding])
			uniforms.append(uniform)
		var modes: Array = ["INIT_EMPTY", "FINALIZE_EMPTY"] if item.capacity == 0 else ["INIT", "VALIDATE", "FINALIZE"]
		var sets: Dictionary = {}
		for mode in modes:
			var uniform_set := _retain(rd.uniform_set_create(uniforms, shaders[mode], 0))
			if not uniform_set.is_valid():
				_fatal.call_deferred("set create " + item.name + ":" + mode)
				return
			sets[mode] = uniform_set
		var list := rd.compute_list_begin()
		var init_mode: String = modes[0]
		var final_mode: String = modes[-1]
		_dispatch(list, pipelines[init_mode], sets[init_mode], _push(item.capacity, item.surfaces, 0, 0), 1)
		rd.compute_list_add_barrier(list)
		var base := 0
		while base < item.capacity:
			var count: int = mini(64, item.capacity - base)
			_dispatch(list, pipelines["VALIDATE"], sets["VALIDATE"], _push(item.capacity, item.surfaces, base, count), 1)
			rd.compute_list_add_barrier(list)
			base += count
		_dispatch(list, pipelines[final_mode], sets[final_mode], _push(item.capacity, item.surfaces, 0, 0), 1)
		rd.compute_list_end()
		for part in ["status", "commands"]:
			var index: int = 5 if part == "status" else 6
			var case_name: String = item.name
			var part_name: String = part
			var callback := func(data: PackedByteArray): _readback.call_deferred(case_name, part_name, data)
			var error := rd.buffer_get_data_async(buffers[index], callback)
			if error != OK:
				_fatal.call_deferred("readback request " + item.name + ":" + part + " error=" + str(error))
				return

func _readback(case_name: String, part: String, data: PackedByteArray) -> void:
	if failed:
		return
	if not replies.has(case_name):
		replies[case_name] = {}
	var words: Array = []
	for i in data.size() / 4:
		words.append(data.decode_u32(i * 4))
	replies[case_name][part] = words
	pending -= 1
	if pending == 0:
		for item in cases:
			var observed: Dictionary = replies[item.name]
			var passed: bool = _same_words(observed.status, item.expected_status) and _same_words(observed.commands, item.expected_commands)
			result_rows.append({"name": item.name, "pass": passed, "observed": observed, "expected_status": item.expected_status, "expected_commands": item.expected_commands, "completed_readback_callbacks": 2})
		RenderingServer.call_on_render_thread(_cleanup)

func _cleanup() -> void:
	for i in range(retained.size() - 1, -1, -1):
		rd.free_rid(retained[i])
	_finish.call_deferred()

func _finish() -> void:
	var passed := not failed
	for row in result_rows:
		passed = passed and row["pass"]
	var report := {"pass": passed, "mutant": mutant, "rows": result_rows, "main_rd": true, "cleanup_free_requested_only": true, "physical_disposal_observed": false, "production_authority": false, "wrapper_execution": false}
	print("GUARD_RESULT ", JSON.stringify(report))
	get_tree().quit(2 if failed else (0 if passed else 1))

func _same_words(observed: Array, expected: Array) -> bool:
	if observed.size() != expected.size():
		return false
	for i in observed.size():
		if observed[i] != int(expected[i]):
			return false
	return true
