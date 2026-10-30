extends SceneTree
func _initialize():
    call_deferred("run_probe")
func run_probe():
    var loaded = GDExtensionManager.load_extension("res://probe.gdextension")
    if loaded != 0:
        print("B4_MATERIAL_LOAD_ERROR=", loaded)
        quit(81)
        return
    for i in range(3):
        await process_frame
    var node = root.get_node("K05NativeFixture/External")
    var mesh = node.get_child(1).get_base()
    var before = RenderingServer.mesh_surface_get_material(mesh, 0)
    RenderingServer.mesh_surface_set_material(mesh, 0, before)
    var same_material_ok = RenderingServer.mesh_surface_get_material(mesh, 0) == before
    print("B4_MATERIAL_SAME_NOOP=", same_material_ok)
    var ordinary_mesh = root.get_node("K05NativeFixture/Ordinary").get_child(1).get_base()
    var ordinary_before = RenderingServer.mesh_surface_get_material(ordinary_mesh, 0)
    var replacement = StandardMaterial3D.new()
    RenderingServer.mesh_surface_set_material(ordinary_mesh, 0, replacement.get_rid())
    var ordinary_changed = RenderingServer.mesh_surface_get_material(ordinary_mesh, 0) == replacement.get_rid()
    RenderingServer.mesh_surface_set_material(ordinary_mesh, 0, ordinary_before)
    print("B4_MATERIAL_ORDINARY_CHANGED=", ordinary_changed)
    RenderingServer.mesh_surface_set_material(mesh, 0, replacement.get_rid())
    var after = RenderingServer.mesh_surface_get_material(mesh, 0)
    var unchanged = before.is_valid() and after == before
    print("B4_MATERIAL_EXTERNAL_UNCHANGED=", unchanged)
    if not unchanged:
        RenderingServer.mesh_surface_set_material(mesh, 0, before)
    var closed = GDExtensionManager.unload_extension("res://probe.gdextension")
    print("B4_MATERIAL_UNLOAD=", closed)
    quit(0 if unchanged and same_material_ok and ordinary_changed and closed == 0 else 1)
