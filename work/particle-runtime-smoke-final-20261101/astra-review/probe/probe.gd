extends SceneTree
var failures = []
func _initialize():
    call_deferred("run")
func draw_connections(p):
    var n = 0
    for c in RenderingServer.get_signal_connection_list("frame_pre_draw"):
        if c.callable.get_object() == p:
            n += 1
    return n
func run():
    var rows = []
    for dimension in [2, 3]:
        var p = CPUParticles2D.new() if dimension == 2 else CPUParticles3D.new()
        p.emitting = false
        p.fixed_fps = 30
        p.lifetime = 2.0
        root.add_child(p)
        for i in 5:
            await process_frame
        var before = {"processing": p.is_processing_internal(), "emitting": p.emitting, "draw_connections": draw_connections(p)}
        p.request_particles_process(0.5, 0.25)
        var after = {"processing": p.is_processing_internal(), "emitting": p.emitting, "draw_connections": draw_connections(p)}
        if not after.processing:
            failures.append(str(dimension) + "D: manual emit+residual does not restore internal processing")
        for i in 5:
            await process_frame
        rows.append({"dimension": dimension, "before": before, "after": after, "later_processing": p.is_processing_internal()})
        p.free()
        var bad = CPUParticles2D.new() if dimension == 2 else CPUParticles3D.new()
        bad.emitting = false
        root.add_child(bad)
        for i in 5:
            await process_frame
        var old_draw = draw_connections(bad)
        bad.request_particles_process(16777216.0, 0.0)
        var new_draw = draw_connections(bad)
        if old_draw != new_draw:
            failures.append(str(dimension) + "D: rejected request changed redraw connection")
        rows.append({"dimension": dimension, "invalid_before_connections": old_draw, "invalid_after_connections": new_draw})
        bad.free()
    print("ASTRA_CPU_MANUAL_RESULT=" + JSON.stringify({"rows": rows, "failures": failures}))
    quit(42 if failures else 0)
