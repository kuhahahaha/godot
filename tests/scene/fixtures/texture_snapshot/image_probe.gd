extends SceneTree

func _initialize() -> void:
	_run.call_deferred()

func _run() -> void:
	if Engine.is_editor_hint():
		printerr("E22_IMAGE_EDITOR_MODE_UNSUPPORTED")
		quit(3)
		return
	print("E22_IMAGE_RUNTIME editor_hint=false")
	if RenderingServer.get_rendering_device() == null:
		printerr("E22_IMAGE_DEVICE_UNAVAILABLE")
		quit(77)
		return
	if GDExtensionManager.load_extension("res://probe.gdextension") != GDExtensionManager.LOAD_STATUS_OK:
		quit(3)
		return
	await process_frame
	await process_frame
	var probe = ClassDB.instantiate("TextureSnapshotImageProbe")
	if probe == null:
		quit(3)
		return
	var result: int = probe.run()
	probe = null
	await process_frame
	quit(result)
