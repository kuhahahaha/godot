extends SceneTree

func _initialize() -> void:
	var failures: Array[String] = []
	for dimension in [2, 3]:
		for gpu in [false, true]:
			var saved: Node
			if dimension == 2:
				saved = GPUParticles2D.new() if gpu else CPUParticles2D.new()
			else:
				saved = GPUParticles3D.new() if gpu else CPUParticles3D.new()
			saved.emitting = false
			var radii: Object = saved
			if gpu:
				var material := ParticleProcessMaterial.new()
				material.emission_shape = ParticleProcessMaterial.EMISSION_SHAPE_RING
				saved.process_material = material
				radii = material
			else:
				saved.emission_shape = saved.EMISSION_SHAPE_RING
			radii.emission_ring_radius = 0.5
			radii.emission_ring_inner_radius = 0.4
			root.add_child(saved)
			var packed := PackedScene.new()
			var label := ("GPU" if gpu else "CPU") + str(dimension) + "D"
			if packed.pack(saved) != OK:
				failures.append(label + " scene packing failed")
			else:
				var loaded := packed.instantiate()
				if loaded == null:
					failures.append(label + " scene instantiation failed")
				else:
					var loaded_radii: Object = loaded.process_material if gpu else loaded
					if loaded_radii == null or not is_equal_approx(loaded_radii.emission_ring_radius, 0.5) or not is_equal_approx(loaded_radii.emission_ring_inner_radius, 0.4):
						failures.append(label + " scene roundtrip lost valid radii")
					loaded.free()
			saved.free()
	print("RING_SCENE_ROUNDTRIP=" + JSON.stringify({"failures": failures}))
	quit(1 if failures.size() else 0)
