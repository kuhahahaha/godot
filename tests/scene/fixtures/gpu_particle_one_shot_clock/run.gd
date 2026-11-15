extends SceneTree

func _initialize() -> void:
	root.add_child(preload("res://host.gd").new())
