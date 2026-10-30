extends SceneTree
func _initialize():
    call_deferred("run_probe")
func run_probe():
    var loaded = GDExtensionManager.load_extension("res://probe.gdextension")
    if loaded != 0:
        print("B4_SAMPLE_LOAD_ERROR=", loaded)
        quit(81)
        return
    var ordinary = root.get_node("K05NativeFixture/Ordinary")
    var external = root.get_node("K05NativeFixture/External")
    var instance = ordinary.get_child(1)
    var material = instance.mesh.surface_get_material(0)
    var shader = material.shader
    shader.code = "shader_type spatial; render_mode unshaded, cull_disabled, fog_disabled; uniform sampler2D b4_sample : source_color; void fragment() { ALBEDO = texture(b4_sample, UV).rgb; }"
    var black_image = Image.create(1, 1, false, Image.FORMAT_RGBA8)
    black_image.fill(Color(0, 0, 0, 1))
    var white_image = Image.create(1, 1, false, Image.FORMAT_RGBA8)
    white_image.fill(Color(1, 1, 1, 1))
    var black = ImageTexture.create_from_image(black_image)
    var white = ImageTexture.create_from_image(white_image)
    RenderingServer.shader_set_default_texture_parameter(shader.get_rid(), "b4_sample", black.get_rid(), 0)
    for i in range(12):
        await process_frame
    var black_pixel = ordinary.get_texture().get_image().get_pixel(48, 48)
    var black_external = external.get_texture().get_image().get_pixel(48, 48)
    RenderingServer.shader_set_default_texture_parameter(shader.get_rid(), "b4_sample", white.get_rid(), 0)
    for i in range(12):
        await process_frame
    var white_pixel = ordinary.get_texture().get_image().get_pixel(48, 48)
    var white_external = external.get_texture().get_image().get_pixel(48, 48)
    var changed = black_pixel.r < 0.1 and white_pixel.r > 0.7 and black_external.r < 0.1 and white_external.r > 0.7
    print("B4_DEFAULT_TEXTURE_PIXELS black=", black_pixel, " white=", white_pixel, " external_black=", black_external, " external_white=", white_external, " changed=", changed)
    var closed = GDExtensionManager.unload_extension("res://probe.gdextension")
    print("B4_SAMPLE_UNLOAD=", closed)
    quit(0 if changed and closed == 0 else 1)
