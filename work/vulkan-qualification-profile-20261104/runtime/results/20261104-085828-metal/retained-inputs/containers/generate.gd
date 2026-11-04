extends SceneTree
func _initialize():
 var rd = RenderingServer.get_rendering_device()
 if rd == null:
  push_error("MULTI_V2 requires the actual main RenderingDevice")
  quit(2)
  return
 for kind in ["capture", "body"]:
  var source = RDShaderSource.new()
  source.source_compute = FileAccess.get_file_as_string("res://" + kind + ".glsl")
  var spirv = rd.shader_compile_spirv_from_source(source)
  if not spirv.compile_error_compute.is_empty():
   push_error(spirv.compile_error_compute)
   quit(3)
   return
  var bytes = rd.shader_compile_binary_from_spirv(spirv)
  if bytes.is_empty():
   push_error("MULTI_V2 empty native container: " + kind)
   quit(4)
   return
  var file = FileAccess.open("res://" + kind + ".bin", FileAccess.WRITE)
  if file == null:
   quit(5)
   return
  file.store_buffer(bytes)
  file.close()
  print("MULTI_V2_CONTAINER kind=", kind, " bytes=", bytes.size())
 quit(0)
