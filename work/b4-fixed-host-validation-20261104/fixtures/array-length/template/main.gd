extends Node
var rd: RenderingDevice
var shaders: Dictionary = {}
var pipelines: Dictionary = {}
var current_resources: Array[RID] = []
var cases: Array
var round_index := 0
var replies: Dictionary = {}
var rows: Array = []
var pending := 0
var failed := false

func _ready() -> void:
	cases = JSON.parse_string(FileAccess.get_file_as_string("res://cases.json"))
	if "--control-only" in OS.get_cmdline_user_args():
		cases = [cases[0]]
		cases[0].name = "no_length_active_binding_control"
		cases[0].input_name = "baseline_reuse_A_to_B"
		cases[0].pipeline = "A"
		cases[0].set_shader = "A"
	get_tree().create_timer(60).timeout.connect(func(): _fatal("completed readback timeout"))
	RenderingServer.call_on_render_thread(_prepare)

func _fatal(message: String) -> void:
	if failed:
		return
	failed = true
	print("LENGTH_FATAL ", message)
	RenderingServer.call_on_render_thread(_cleanup_all)

func _prepare() -> void:
	rd = RenderingServer.get_rendering_device()
	if rd == null:
		_fatal.call_deferred("main RD unavailable")
		return
	print("LENGTH_DEVICE ", rd.get_device_name(), " vendor=", rd.get_device_vendor_name(), " main_rd=true")
	var modes: Array = ["A"] if "--control-only" in OS.get_cmdline_user_args() else ["A","B"]
	for mode in modes:
		var spirv := RDShaderSPIRV.new()
		spirv.set_stage_bytecode(RenderingDevice.SHADER_STAGE_COMPUTE, FileAccess.get_file_as_bytes("res://control-" + mode + ".spv"))
		var shader := rd.shader_create_from_spirv(spirv)
		if not shader.is_valid():
			_fatal.call_deferred("shader creation " + mode)
			return
		shaders[mode] = shader
		var pipeline := rd.compute_pipeline_create(shader)
		if not pipeline.is_valid():
			_fatal.call_deferred("pipeline creation " + mode)
			return
		pipelines[mode] = pipeline
		print("LENGTH_PIPELINE_READY ",mode)

	_record_round()

func _uniform(binding: int, rid: RID) -> RDUniform:
	var uniform := RDUniform.new()
	uniform.uniform_type = RenderingDevice.UNIFORM_TYPE_STORAGE_BUFFER
	uniform.binding = binding
	uniform.add_id(rid)
	return uniform

func _record_round() -> void:
	var item: Dictionary = cases[round_index]
	var buffers: Array[RID] = []
	for index in 4:
		var bytes := FileAccess.get_file_as_bytes("res://" + item.input_name + "-" + str(index) + ".bin")
		var buffer := rd.storage_buffer_create(bytes.size(), bytes)
		if not buffer.is_valid():
			_fatal.call_deferred("buffer creation index=" + str(index))
			return
		buffers.append(buffer)
		current_resources.append(buffer)
	var uniforms0: Array[RDUniform] = [_uniform(0,buffers[0]),_uniform(3,buffers[1]),_uniform(5,buffers[3])]
	var uniforms1: Array[RDUniform] = [_uniform(2,buffers[2])]
	var set0 := rd.uniform_set_create(uniforms0,shaders[item.set_shader],0)
	if set0.is_valid():
		current_resources.append(set0)
	var set1 := rd.uniform_set_create(uniforms1,shaders[item.set_shader],1)
	if set1.is_valid():
		current_resources.append(set1)
	if not set0.is_valid() or not set1.is_valid():
		_fatal.call_deferred("uniform set creation")
		return
	var list := rd.compute_list_begin()
	rd.compute_list_bind_compute_pipeline(list,pipelines[item.pipeline])
	rd.compute_list_bind_uniform_set(list,set0,0)
	rd.compute_list_bind_uniform_set(list,set1,1)
	rd.compute_list_dispatch(list,1,1,1)
	rd.compute_list_end()
	for index in 4:
		var buffer_index: int = index
		var callback := func(data: PackedByteArray): _readback.call_deferred(buffer_index,data)
		var error := rd.buffer_get_data_async(buffers[index],callback)
		if error != OK:
			_fatal.call_deferred("readback request index="+str(index)+" error="+str(error))
			return

func _readback(index: int,data: PackedByteArray) -> void:
	if failed:
		return
	var words: Array = []
	for i in data.size()/4:
		words.append(data.decode_u32(i*4))
	replies[str(index)] = words
	pending += 1
	if pending != 4:
		return
	var item: Dictionary = cases[round_index]
	var passed := true
	for i in 4:
		var actual: Array = replies[str(i)]
		var expected: Array = item.expected_words[i]
		if actual.size() != expected.size():
			passed = false
		else:
			for j in actual.size():
				if actual[j] != int(expected[j]):
					passed = false
	rows.append({"name":item.name,"set_shader":item.set_shader,"pipeline":item.pipeline,"pass":passed,"observed":replies.duplicate(true),"expected_words":item.expected_words,"completed_readback_callbacks":4,"requested_byte_sizes":item.byte_sizes})
	round_index += 1
	replies.clear()
	pending = 0
	RenderingServer.call_on_render_thread(_after_round)

func _after_round() -> void:
	for i in range(current_resources.size()-1,-1,-1):
		rd.free_rid(current_resources[i])
	current_resources.clear()
	if round_index < cases.size():
		_record_round()
	else:
		_cleanup_all()

func _cleanup_all() -> void:
	if rd != null:
		for i in range(current_resources.size()-1,-1,-1):
			rd.free_rid(current_resources[i])
		current_resources.clear()
		for pipeline in pipelines.values():
			rd.free_rid(pipeline)
		for shader in shaders.values():
			rd.free_rid(shader)
	_finish.call_deferred()

func _finish() -> void:
	var passed: bool = not failed and rows.size()==cases.size()
	for row in rows:
		passed = passed and row["pass"]
	print("LENGTH_RESULT ",JSON.stringify({"pass":passed,"rows":rows,"main_rd":true,"free_requests_only":true,"physical_disposal_observed":false,"production_authority":false,"dynamic_binding_tested":false}))
	get_tree().quit(2 if failed else (0 if passed else 1))
