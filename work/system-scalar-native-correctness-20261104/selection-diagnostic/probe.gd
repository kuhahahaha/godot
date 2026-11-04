extends SceneTree
func _initialize():
    var a = Engine.get_version_info()["controlled_build_identity"]
    var b = JSON.parse_string(OS.get_environment("SYSTEM_SCALAR_SELECTION_JSON"))
    print("DIAG dict_equal=", a == b)
    for key in a:
        print("DIAG key=", key, " a_type=", typeof(a[key]), " b_type=", typeof(b[key]), " scalar_equal=", a[key] == b[key])
    quit(0)
