extends SceneTree
func _initialize():
 call_deferred("run_probe")
func run_probe():
 var loaded=GDExtensionManager.load_extension("res://probe.gdextension")
 print("K05_NATIVE_LOAD=",loaded)
 if loaded!=0:
  quit(81)
  return
 for frame in range(8):
  await process_frame
 var ordinary=root.get_node("K05NativeFixture/Ordinary").get_texture().get_image()
 var external=root.get_node("K05NativeFixture/External").get_texture().get_image()
 ordinary.convert(Image.FORMAT_RGBA8)
 external.convert(Image.FORMAT_RGBA8)
 ordinary.save_png("res://ordinary.png")
 external.save_png("res://external.png")
 var different=0
 var covered=0
 var max_error=0.0
 for y in range(96):
  for x in range(96):
   var a=ordinary.get_pixel(x,y)
   var b=external.get_pixel(x,y)
   var error=max(abs(a.r-b.r),max(abs(a.g-b.g),abs(a.b-b.b)))
   max_error=max(max_error,error)
   if error>3.0/255.0:
    different+=1
   if a.a>0.9:
    covered+=1
 print("K05_NATIVE_COMPARE covered=",covered," different=",different," max_error=",max_error)
 var closed=GDExtensionManager.unload_extension("res://probe.gdextension")
 print("K05_NATIVE_UNLOAD=",closed)
 quit(0 if covered>100 and covered<8000 and different<20 and closed==0 else 82)
