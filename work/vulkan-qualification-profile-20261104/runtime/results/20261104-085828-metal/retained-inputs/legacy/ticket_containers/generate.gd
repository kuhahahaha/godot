extends SceneTree
func _initialize():
 var rd = RenderingServer.get_rendering_device()
 for kind in ["capture", "body"]:
  var source = RDShaderSource.new()
  var size = "1" if kind == "capture" else "64"
  source.source_compute = "#version 450\nlayout(local_size_x="+size+",local_size_y=1,local_size_z=1) in;\nlayout(set=0,binding=0,std430) buffer Data {uint words[];} data;\nlayout(push_constant,std430) uniform Push {uint base;uint envelope;uint context;uint iteration;} p;\nvoid main() {"
  if kind == "capture":
   source.source_compute += "if(p.iteration==1)data.words[0]+=p.base+p.envelope+p.context+p.iteration;else data.words[0]=p.base+p.envelope+p.context+p.iteration;"
  else:
   source.source_compute += "uint i=gl_GlobalInvocationID.x;if(i<p.envelope)data.words[i]=data.words[0]+i+p.base+p.context+p.iteration;"
  source.source_compute += "}\n"
  var spirv = rd.shader_compile_spirv_from_source(source)
  if not spirv.compile_error_compute.is_empty():
   push_error(spirv.compile_error_compute)
   quit(2)
   return
  var bytes = rd.shader_compile_binary_from_spirv(spirv)
  if bytes.is_empty():
   quit(3)
   return
  var file = FileAccess.open("res://"+kind+".bin", FileAccess.WRITE)
  file.store_buffer(bytes)
  file.close()
  print("QB1_TICKET_CONTAINER kind=",kind," bytes=",bytes.size())
 quit(0)
