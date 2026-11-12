extends SceneTree

func _initialize() -> void:
	var rd := RenderingServer.create_local_rendering_device()
	if rd == null:
		push_error("Local RenderingDevice unavailable")
		quit(77)
		return
	var source := RDShaderSource.new()
	source.source_compute = FileAccess.get_file_as_string(OS.get_environment("ANGE_HEIGHTFIELD_TEST_SHADER"))
	var spirv := rd.shader_compile_spirv_from_source(source, false)
	if spirv.get_stage_bytecode(RenderingDevice.SHADER_STAGE_COMPUTE).is_empty():
		push_error("Compute shader compile failed: " + spirv.get_stage_compile_error(RenderingDevice.SHADER_STAGE_COMPUTE))
		quit(2)
		return
	var shader := rd.shader_create_from_spirv(spirv)
	var pipeline := rd.compute_pipeline_create(shader)
	var output := rd.storage_buffer_create(4 * 16)
	var bytes := PackedByteArray()
	bytes.resize(256 * 2 * 4)
	for z in range(2):
		for x in range(256):
			bytes.encode_float((z * 256 + x) * 4, 0.5 if z == 0 else 0.25 + 0.5 * (float(x) + 0.5) / 256.0)
	var format := RDTextureFormat.new()
	format.width = 256
	format.height = 2
	format.format = RenderingDevice.DATA_FORMAT_R32_SFLOAT
	format.usage_bits = RenderingDevice.TEXTURE_USAGE_SAMPLING_BIT
	var height := rd.texture_create(format, RDTextureView.new(), [bytes])
	var sampler_state := RDSamplerState.new()
	sampler_state.min_filter = RenderingDevice.SAMPLER_FILTER_LINEAR
	sampler_state.mag_filter = RenderingDevice.SAMPLER_FILTER_LINEAR
	var sampler := rd.sampler_create(sampler_state)
	var uniforms: Array[RDUniform] = []
	for binding in range(3):
		var uniform := RDUniform.new()
		uniform.binding = binding
		uniform.uniform_type = [RenderingDevice.UNIFORM_TYPE_STORAGE_BUFFER, RenderingDevice.UNIFORM_TYPE_SAMPLER, RenderingDevice.UNIFORM_TYPE_TEXTURE][binding]
		uniform.add_id([output, sampler, height][binding])
		uniforms.append(uniform)
	var set := rd.uniform_set_create(uniforms, shader, 0)
	if not pipeline.is_valid() or not set.is_valid():
		push_error("Compute resources unavailable")
		quit(2)
		return
	var list := rd.compute_list_begin()
	rd.compute_list_bind_compute_pipeline(list, pipeline)
	rd.compute_list_bind_uniform_set(list, set, 0)
	rd.compute_list_dispatch(list, 1, 1, 1)
	rd.compute_list_end()
	rd.submit()
	rd.sync()
	var actual := rd.buffer_get_data(output)
	var sloped_normal := Vector3(-0.5, 1, 0).normalized()
	var expected := [Vector4(0, 1, 0, 0.2), Vector4(-1, 0, 0, 0.2), Vector4(0, 1, 0, 0.2), Vector4(sloped_normal.x, sloped_normal.y, 0, 0.15 * sloped_normal.y)]
	var passed := actual.size() == 64
	for i in range(4):
		var observed := Vector4(actual.decode_float(i * 16), actual.decode_float(i * 16 + 4), actual.decode_float(i * 16 + 8), actual.decode_float(i * 16 + 12))
		print("HEIGHTFIELD_NORMAL_GPU ", i, " observed=", observed, " expected=", expected[i])
		passed = passed and observed.distance_to(expected[i]) < 0.005
	rd.free_rid(output)
	rd.free_rid(height)
	rd.free_rid(sampler)
	rd.free_rid(shader)
	rd.free()
	quit(0 if passed else 1)
