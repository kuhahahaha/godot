extends SceneTree
func _init():
    var shader := Shader.new()
    shader.code = "shader_type spatial; void vertex() { VERTEX = (INV_MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz; } void fragment() { ALBEDO = abs((INV_MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz); }"
    var material := ShaderMaterial.new()
    material.shader = shader
    material.get_rid()
    print("SPATIAL_INVERSE_PROBE_FINISHED")
    quit()
