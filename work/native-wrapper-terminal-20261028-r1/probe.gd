extends SceneTree
func _initialize():
 print("WRAPPER_LOOP_ENTERED")
 var result = GDExtensionManager.unload_extension("res://probe.gdextension")
 print("WRAPPER_UNLOAD result=", result)
 quit(0 if result == 0 else 1)
