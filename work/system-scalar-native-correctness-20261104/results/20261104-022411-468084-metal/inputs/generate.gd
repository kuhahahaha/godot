extends SceneTree

const SelectionIdentity = preload("res://selection_identity.gd")

func _initialize():
    var identity = Engine.get_version_info().get("controlled_build_identity", {})
    print("SYSTEM_SCALAR_HOST_IDENTITY=", JSON.stringify(identity))
    var expected = JSON.parse_string(OS.get_environment("SYSTEM_SCALAR_SELECTION_JSON"))
    if not SelectionIdentity.matches(identity, expected):
        push_error("SYSTEM_SCALAR actual host selection mismatch")
        quit(6)
        return
    var rd = RenderingServer.get_rendering_device()
    if rd == null:
        push_error("SYSTEM_SCALAR requires an actual main RenderingDevice")
        quit(2)
        return
    var source = RDShaderSource.new()
    source.source_compute = FileAccess.get_file_as_string("res://scalar.glsl")
    var spirv = rd.shader_compile_spirv_from_source(source)
    if not spirv.compile_error_compute.is_empty():
        push_error(spirv.compile_error_compute)
        quit(3)
        return
    var binary = rd.shader_compile_binary_from_spirv(spirv)
    if binary.is_empty():
        push_error("SYSTEM_SCALAR empty native container")
        quit(4)
        return
    var file = FileAccess.open("res://scalar.bin", FileAccess.WRITE)
    if file == null:
        quit(5)
        return
    file.store_buffer(binary)
    file.close()
    print("SYSTEM_SCALAR_CONTAINER bytes=", binary.size())
    quit(0)
