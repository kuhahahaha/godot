extends SceneTree

var physics_frames := 0

func _initialize() -> void:
	print("R1_SMALL_POOL_INITIALIZED ", PhysicsServer3D.get_class(), " ", PhysicsServer2D.get_class())

func _physics_process(_delta: float) -> bool:
	physics_frames += 1
	print("R1_SMALL_POOL_PHYSICS_FRAME ", physics_frames)
	if physics_frames == 3:
		print("R1_SMALL_POOL_BOTH_SYNC_EPOCHS_COMPLETED")
		quit(0)
	return false
