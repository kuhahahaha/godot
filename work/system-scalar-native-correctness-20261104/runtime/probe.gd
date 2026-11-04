extends SceneTree

const SelectionIdentity = preload("res://selection_identity.gd")

func _init():
    var identity = Engine.get_version_info().get("controlled_build_identity", {})
    print("SYSTEM_SCALAR_HOST_IDENTITY=", JSON.stringify(identity))
    var expected = JSON.parse_string(OS.get_environment("SYSTEM_SCALAR_SELECTION_JSON"))
    if not SelectionIdentity.matches(identity, expected):
        push_error("SYSTEM_SCALAR actual host selection mismatch")
        quit(6)
        return

func _initialize():
    print("SYSTEM_SCALAR_LOOP_INITIALIZED")
    quit(0)
