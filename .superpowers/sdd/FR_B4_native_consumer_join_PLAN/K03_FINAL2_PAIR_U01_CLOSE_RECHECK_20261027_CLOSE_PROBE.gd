extends SceneTree

func _initialize():
    call_deferred("run_probe")

func run_probe():
    var loaded := GDExtensionManager.load_extension("res://ange.gdextension")
    print("CLOSE_EXTENSION_STATUS=", loaded)
    print("CLOSE_EXTENSION_OK=", loaded == GDExtensionManager.LOAD_STATUS_OK)
    if loaded != GDExtensionManager.LOAD_STATUS_OK:
        quit(2)
        return
    var world: Node = ClassDB.instantiate("EcsWorld")
    root.add_child(world)
    await process_frame
    var entered: bool = world.enter_world("res://pack/arena.world")
    print("CLOSE_ENTER_WORLD=", entered)
    print("CLOSE_ACTIVE_BEFORE=", world.is_world_active())
    world.exit_world()
    await process_frame
    var active_after: bool = world.is_world_active()
    print("CLOSE_ACTIVE_AFTER_EXIT=", active_after)
    root.remove_child(world)
    world.free()
    await process_frame
    var unloaded := GDExtensionManager.unload_extension("res://ange.gdextension")
    print("CLOSE_UNLOAD_STATUS=", unloaded)
    print("CLOSE_UNLOAD_OK=", unloaded == GDExtensionManager.LOAD_STATUS_OK)
    quit(0 if entered and !active_after and unloaded == GDExtensionManager.LOAD_STATUS_OK else 1)
