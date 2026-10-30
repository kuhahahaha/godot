extends SceneTree

func _initialize():
	call_deferred("run_probe")

func run_probe():
	var loaded := GDExtensionManager.load_extension("res://ange.gdextension")
	print("U01_EXTENSION=", loaded)
	if loaded != GDExtensionManager.LOAD_STATUS_OK:
		quit(2)
		return
	var world: Node = ClassDB.instantiate("EcsWorld")
	root.add_child(world)
	await process_frame
	var entered: bool = world.enter_world("res://pack/arena.world")
	print("U01_ENTER_WORLD=", entered)
	root.remove_child(world)
	world.free()
	await process_frame
	var unloaded := GDExtensionManager.unload_extension("res://ange.gdextension")
	print("U01_UNLOAD=", unloaded)
	quit(0 if entered and unloaded == GDExtensionManager.LOAD_STATUS_OK else 1)
