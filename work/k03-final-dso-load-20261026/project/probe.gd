extends SceneTree
func _initialize():
    var result := GDExtensionManager.load_extension("res://ange.gdextension")
    print("K03_DSO_LOAD=", result)
    quit(0 if result == GDExtensionManager.LOAD_STATUS_OK else 1)
