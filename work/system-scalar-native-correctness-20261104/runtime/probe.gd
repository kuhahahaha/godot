extends SceneTree

func _initialize():
    var identity = Engine.get_version_info().get("controlled_build_identity", {})
    print("SYSTEM_SCALAR_HOST_IDENTITY=", JSON.stringify(identity))
    var expected = JSON.parse_string(OS.get_environment("SYSTEM_SCALAR_SELECTION_JSON"))
    if identity != expected:
        push_error("SYSTEM_SCALAR actual host selection mismatch")
        quit(6)
        return
    print("SYSTEM_SCALAR_LOOP_INITIALIZED")
    quit(0)
