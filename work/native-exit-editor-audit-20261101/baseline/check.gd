extends SceneTree
func _initialize():
	call_deferred("check_editor")
func check_editor():
	while EditorInterface.get_resource_filesystem().is_scanning():
		await process_frame
	await process_frame
	var plugin = EditorExportPlugin.new()
	var ok = Engine.is_editor_hint() and ClassDB.class_exists("EditorExportPlugin") and ClassDB.class_exists("GDScript") and plugin != null
	print("ANGE_EDITOR_CAPABILITY ", JSON.stringify({"editor_hint":Engine.is_editor_hint(), "EditorExportPlugin":ClassDB.class_exists("EditorExportPlugin"), "GDScript":ClassDB.class_exists("GDScript"), "plugin_instance":plugin != null}))
	plugin = null
	await process_frame
	quit(0 if ok else 1)
