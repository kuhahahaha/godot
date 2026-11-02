extends SceneTree

func _initialize() -> void:
	var failures: Array[String] = []
	var material := ParticleProcessMaterial.new()
	var rid := material.get_rid()
	material.emission_ring_radius = 5.0
	material.emission_ring_inner_radius = 6.0
	var grow := [RenderingServer.material_get_param(rid, "emission_ring_radius"), RenderingServer.material_get_param(rid, "emission_ring_inner_radius")]
	if grow != [6.0, 6.0]:
		failures.append("growing inner did not publish both radii")
	material.emission_ring_radius = 0.5
	var shrink := [RenderingServer.material_get_param(rid, "emission_ring_radius"), RenderingServer.material_get_param(rid, "emission_ring_inner_radius")]
	if shrink != [0.5, 0.5]:
		failures.append("shrinking outer did not publish both radii")
	material.emission_ring_inner_radius = 0.4
	var restore := [RenderingServer.material_get_param(rid, "emission_ring_radius"), RenderingServer.material_get_param(rid, "emission_ring_inner_radius")]
	if not is_equal_approx(float(restore[0]), 0.5) or not is_equal_approx(float(restore[1]), 0.4):
		failures.append("restoring valid inner did not publish its radius")
	print("RING_BACKEND_PROBE=" + JSON.stringify({"grow": grow, "shrink": shrink, "restore": restore, "failures": failures}))
	quit(1 if failures.size() else 0)
