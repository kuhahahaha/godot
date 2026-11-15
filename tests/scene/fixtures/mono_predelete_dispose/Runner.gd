extends Node

var failures := 0

func check(value: bool, detail: String) -> void:
	if not value:
		failures += 1
		push_error(detail)

func free_detached(value: Object) -> void:
	if is_instance_valid(value):
		if value.get_script() != null:
			# Failed-case cleanup after the detached-script assertion, not success.
			value.set_script(null)
		value.free()

func _ready() -> void:
	var node_script = load("res://DisposeProbe.cs")
	var ref_script = load("res://RefDisposeProbe.cs")
	var controller_script = load("res://Controller.cs")
	if node_script == null or ref_script == null or controller_script == null:
		check(false, "Actual C# scripts did not load")
		get_tree().quit(1)
		return
	if not node_script.can_instantiate() or not ref_script.can_instantiate() or not controller_script.can_instantiate():
		check(false, "Actual C# scripts cannot instantiate")
		get_tree().quit(1)
		return
	var controller = controller_script.new()
	add_child(controller)

	var before: int = controller.NodeDisposeCount()
	var native = node_script.new()
	add_child(native)
	check(native.get_script() == node_script, "Native-free probe must own a real CSharpInstance")
	check(not native.get_signal_connection_list("Ping").is_empty(), "C# event connection was not created")
	native.free()
	check(not is_instance_valid(native), "Native predelete did not delete its owner")
	check(controller.NodeDisposeCount() == before + 1, "Native predelete must Dispose exactly once")

	before = controller.NodeDisposeCount()
	var explicit = node_script.new()
	add_child(explicit)
	controller.DisposeTarget(explicit)
	check(is_instance_valid(explicit), "Explicit non-RefCounted Dispose must keep its native owner")
	check(explicit.get_script() == null, "Explicit Dispose must detach the native CSharpInstance")
	check(explicit.get_signal_connection_list("Ping").is_empty(), "Explicit Dispose left a C# event connection")
	check(controller.NodeDisposeCount() == before + 1, "Explicit Dispose must run once")
	free_detached(explicit)

	before = controller.NodeDisposeCount()
	var synthetic = node_script.new()
	add_child(synthetic)
	# Internal cleanup value is deliberately sent via the ordinary public API.
	# A sticky CSharpInstance flag alone must not impersonate native predelete.
	synthetic.notification(3, true)
	check(is_instance_valid(synthetic), "An ordinary notification must not delete its native owner")
	check(synthetic.get_script() == null, "A synthetic cleanup notification must retain the original detach path")
	check(synthetic.get_signal_connection_list("Ping").is_empty(), "Synthetic cleanup left a C# event connection")
	check(controller.NodeDisposeCount() == before + 1, "Synthetic cleanup must Dispose once")
	free_detached(synthetic)

	before = controller.NodeDisposeCount()
	var outer = node_script.new()
	var inner = node_script.new()
	add_child(outer)
	add_child(inner)
	controller.SetNested(outer, inner)
	outer.free()
	check(not is_instance_valid(outer) and not is_instance_valid(inner), "Nested native predelete did not return both owners")
	check(controller.NodeDisposeCount() == before + 2, "Nested predelete must restore the original cleanup scope")

	before = controller.NodeDisposeCount()
	outer = node_script.new()
	var external = node_script.new()
	add_child(outer)
	add_child(external)
	controller.SetExternal(outer, external)
	outer.free()
	check(not is_instance_valid(outer), "Outer native predelete did not finish")
	check(is_instance_valid(external) and external.get_script() == null, "Another owner's explicit Dispose must not borrow the cleanup scope")
	check(controller.NodeDisposeCount() == before + 2, "Cross-owner cleanup must dispose both managed objects once")
	free_detached(external)

	var ref_before: int = controller.RefDisposeCount()
	var ref_probe = ref_script.new()
	var weak = weakref(ref_probe)
	check(ref_probe.get_script() == ref_script, "RefCounted probe must own a real CSharpInstance")
	controller.DisposeTarget(ref_probe)
	check(is_instance_valid(ref_probe) and ref_probe.get_script() == null, "Explicit RefCounted Dispose must detach while the native Variant retains it")
	check(ref_probe.get_signal_connection_list("Ping").is_empty(), "RefCounted Dispose left a C# event connection")
	check(controller.RefDisposeCount() == ref_before + 1, "RefCounted Dispose must run once")
	if ref_probe.get_script() != null:
		ref_probe.set_script(null) # Failed-case cleanup before the native hold returns.
	ref_probe = null
	check(weak.get_ref() == null, "RefCounted native hold did not return after explicit Dispose")

	if failures == 0:
		print("MONO_PREDELETE_DISPOSE_OK")
	get_tree().quit(0 if failures == 0 else 1)
