extends SceneTree

func _initialize():
 call_deferred("run_probe")

func run_probe():
 var status = GDExtensionManager.load_extension("res://probe.gdextension")
 print("B4_LIST_LOAD=", status)
 if status != 0:
  quit(81)
  return
 var closed = GDExtensionManager.unload_extension("res://probe.gdextension")
 print("B4_LIST_UNLOAD=", closed)
 quit(0 if closed == 0 else 82)
