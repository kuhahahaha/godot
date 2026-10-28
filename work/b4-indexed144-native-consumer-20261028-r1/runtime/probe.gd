extends SceneTree
func _initialize():
 var rd=RenderingServer.get_rendering_device()
 if rd==null:
  quit(71)
  return
 for kind in ["mobile","clustered"]:
  var source=RDShaderSource.new()
  source.source_compute=FileAccess.get_file_as_string("res://"+kind+".comp")
  var spirv=rd.shader_compile_spirv_from_source(source)
  var error=spirv.get_stage_compile_error(RenderingDevice.SHADER_STAGE_COMPUTE)
  if not error.is_empty():
   printerr("DECODER_COMPILE_ERROR kind=",kind," error=",error)
   quit(72)
   return
  var shader=rd.shader_create_from_spirv(spirv)
  var pipeline=rd.compute_pipeline_create(shader)
  var output=rd.storage_buffer_create(128)
  var input=rd.storage_buffer_create(128,FileAccess.get_file_as_bytes("res://input.bin"))
  var out_uniform=RDUniform.new()
  out_uniform.uniform_type=RenderingDevice.UNIFORM_TYPE_STORAGE_BUFFER
  out_uniform.binding=0
  out_uniform.add_id(output)
  var in_uniform=RDUniform.new()
  in_uniform.uniform_type=RenderingDevice.UNIFORM_TYPE_STORAGE_BUFFER
  in_uniform.binding=1
  in_uniform.add_id(input)
  var uniforms=rd.uniform_set_create([out_uniform,in_uniform],shader,0)
  var list=rd.compute_list_begin()
  rd.compute_list_bind_compute_pipeline(list,pipeline)
  rd.compute_list_bind_uniform_set(list,uniforms,0)
  rd.compute_list_dispatch(list,1,1,1)
  rd.compute_list_end()
  var bytes=rd.buffer_get_data(output)
  if bytes.size()!=128:
   quit(73)
   return
  var file=FileAccess.open("res://"+kind+".readback.bin",FileAccess.WRITE)
  file.store_buffer(bytes)
  file.close()
  print("B4_DECODER_READBACK kind=",kind," bytes=",bytes.size())
  for rid in [uniforms,pipeline,shader,output,input]:
   rd.free_rid(rid)
 quit(0)
