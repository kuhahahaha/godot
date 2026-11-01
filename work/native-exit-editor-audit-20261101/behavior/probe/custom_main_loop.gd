extends MainLoop

var observation: Dictionary
func _initialize():
	var windows = 0
	var editor_nodes = 0
	for id in Node.get_orphan_node_ids():
		var node = instance_from_id(id)
		if node is Window:
			windows += 1
		if is_instance_valid(node) and node.get_class() == "EditorNode":
			editor_nodes += 1
	observation = {"selected_self":Engine.get_main_loop() == self, "is_scene_tree":self.is_class("SceneTree"), "debug_build":OS.is_debug_build(), "orphan_windows":windows, "orphan_editor_nodes":editor_nodes}
	print("CUSTOM_MAIN_LOOP_OBSERVATION ", JSON.stringify(observation))
	var ok = observation["selected_self"] and not observation["is_scene_tree"] and observation["debug_build"] and windows == 0 and editor_nodes == 0
	print("CUSTOM_MAIN_LOOP_INITIALIZED explicit_script=1 passed=", ok)
func _process(_delta):
	return true
