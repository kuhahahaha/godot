extends SceneTree

func _initialize() -> void:
	_run.call_deferred()

func _run() -> void:
	if Engine.is_editor_hint():
		printerr("E22_GRADIENT_EDITOR_MODE_UNSUPPORTED")
		quit(3)
		return
	print("E22_GRADIENT_RUNTIME editor_hint=false")
	if RenderingServer.get_rendering_device() == null:
		printerr("E22_GRADIENT_DEVICE_UNAVAILABLE")
		quit(77)
		return
	if GDExtensionManager.load_extension("res://probe.gdextension") != GDExtensionManager.LOAD_STATUS_OK:
		quit(3)
		return
	var cases: Array = []
	for width in [2, 33, 4096]:
		for profile in range(5):
			for mode in range(3):
				for space in range(3):
					for hdr in [false, true]:
						var offsets := PackedFloat32Array()
						var colors := PackedColorArray()
						if profile == 1:
							offsets = PackedFloat32Array([0.5])
							colors = PackedColorArray([Color(1.5, 0.5, 4.0, 2.0)])
						elif profile == 2:
							offsets = PackedFloat32Array([0.0, 0.4, 1.0])
							colors = PackedColorArray([Color(0.1, 0.2, 0.3, 1.0), Color(0.8, 0.4, 0.2, 0.6), Color(0.3, 0.7, 0.9, 0.8)])
						elif profile == 3:
							offsets = PackedFloat32Array([0.8, 0.2, 0.5, 0.5, 0.0, 1.0])
							colors = PackedColorArray([Color(0.8, 0.2, 0.3, 1.0), Color(0.1, 0.7, 0.5, 0.7), Color(0.4, 0.3, 0.9, 0.5), Color(0.7, 0.6, 0.2, 0.9), Color(0.2, 0.4, 0.1, 1.0), Color(0.6, 0.5, 0.4, 1.0)])
						elif profile == 4:
							for i in range(4096):
								offsets.append(float(4095 - i) / 4095.0)
								colors.append(Color(float(i % 17) / 16.0, float(i % 11) / 10.0, float(i % 7) / 6.0, 1.0))
						var gradient := Gradient.new()
						gradient.offsets = offsets
						gradient.colors = colors
						gradient.interpolation_mode = mode
						gradient.interpolation_color_space = space
						var texture := GradientTexture1D.new()
						texture.width = width
						texture.use_hdr = hdr
						texture.gradient = gradient
						cases.append([texture, width, profile, mode, space, hdr, offsets.size()])
	# GradientTexture1D's original deferred update_now uploads each texture here.
	await process_frame
	await process_frame
	var probe = ClassDB.instantiate("TextureSnapshotImageProbe")
	if probe == null:
		quit(3)
		return
	for c in cases:
		probe.run_case(c[0], c[1], c[2], c[3], c[4], c[5], c[6])
	var result: int = probe.finish()
	probe = null
	cases.clear()
	await process_frame
	await process_frame
	quit(result)
