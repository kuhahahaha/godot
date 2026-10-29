extends SceneTree
func _initialize():
 var rd=RenderingServer.get_rendering_device()
 if rd==null:
  quit(71)
  return
 var metadata=JSON.parse_string(FileAccess.get_file_as_string("res://unit.json"))
 for kind in ["capture","body"]:
  var spirv=RDShaderSPIRV.new()
  spirv.bytecode_compute=FileAccess.get_file_as_bytes("res://"+kind+".spv")
  var binary=rd.shader_compile_binary_from_spirv_with_options(spirv,"original_numeric_"+kind,int(metadata.required_compile_flags))
  if binary.is_empty():
   quit(73)
   return
  var f=FileAccess.open("res://"+kind+".bin",FileAccess.WRITE)
  f.store_buffer(binary)
  f.close()
  print("ORIGINAL_NUMERIC_CONTAINER kind=",kind," flags=",metadata.required_compile_flags," spv_bytes=",spirv.bytecode_compute.size()," native_bytes=",binary.size())
 quit(0)
