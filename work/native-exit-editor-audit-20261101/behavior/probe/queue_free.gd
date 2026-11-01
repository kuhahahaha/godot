extends SceneTree

class TrackedNode extends Node:
	var evidence: Dictionary
	func _notification(what):
		if what == NOTIFICATION_PREDELETE:
			evidence["predelete"] += 1
			print("QUEUE_FREE_PREDELETE count=", evidence["predelete"])

var evidence = {"tree_exited": 0, "predelete": 0}

func _initialize():
	call_deferred("run_behavior")

func child_exited():
	evidence["tree_exited"] += 1
	print("QUEUE_FREE_TREE_EXITED count=", evidence["tree_exited"])

func run_behavior():
	# Await this actual selected tree, not SceneTree's global singleton.
	await self.process_frame
	var args = OS.get_cmdline_user_args()
	var mode = args[0] if not args.is_empty() else "detached"
	if mode != "attached" and mode != "detached":
		print("QUEUE_FREE_UNEXPECTED bad_mode=", mode)
		quit(43)
		return
	var child = TrackedNode.new()
	child.evidence = evidence
	self.root.add_child(child)
	child.tree_exited.connect(child_exited)
	if child.get_tree() != self or not child.is_inside_tree():
		print("QUEUE_FREE_UNEXPECTED actual_tree_mismatch=1")
		child.free()
		quit(43)
		return
	print("QUEUE_FREE_ENROLLED actual_self_tree=1 editor=", Engine.is_editor_hint(), " mode=", mode)
	if mode == "detached":
		self.root.remove_child(child)
		if child.is_inside_tree() or child.get_parent() != null:
			print("QUEUE_FREE_UNEXPECTED detach_failed=1")
			child.free()
			quit(43)
			return
	child.queue_free()
	await self.process_frame
	await self.process_frame
	var invalid = not is_instance_valid(child)
	var ok = invalid and evidence["tree_exited"] == 1 and evidence["predelete"] == 1
	print("QUEUE_FREE_RESULT ", JSON.stringify({"editor":Engine.is_editor_hint(), "mode":mode, "actual_self_frames":2, "node_invalid":invalid, "tree_exited":evidence["tree_exited"], "predelete":evidence["predelete"], "passed":ok}))
	if not ok:
		# Diagnostic cleanup follows the recorded assertion; it cannot turn RED
		# into success or let this probe's child add another exit leak.
		print("QUEUE_FREE_RED cause=queue_did_not_drain_after_two_actual_tree_frames")
		if is_instance_valid(child):
			print("QUEUE_FREE_MANUAL_CLEANUP after_failed_assertion=1")
			child.free()
		quit(42)
		return
	print("QUEUE_FREE_GREEN actual_free=1 tree_exited=1 predelete=1")
	quit(0)
