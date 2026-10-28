extends SceneTree
func _initialize():
 var loaded=GDExtensionManager.load_extension("res://probe.gdextension")
 print("ORIGINAL_NUMERIC_LOAD=",loaded)
 if loaded!=0:
  quit(81)
  return
 var closed=GDExtensionManager.unload_extension("res://probe.gdextension")
 print("ORIGINAL_NUMERIC_UNLOAD=",closed)
 quit(0 if closed==0 else 82)
