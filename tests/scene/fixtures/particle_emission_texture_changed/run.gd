extends SceneTree

signal observed(result: Dictionary)

const SLOTS := ["emission_point_texture", "emission_normal_texture", "emission_color_texture"]
const UNIFORMS := ["emission_texture_points", "emission_texture_normal", "emission_texture_color"]
var started := Time.get_ticks_msec()
var checks := 0
var failures: Array[String] = []
var observations: Array[Dictionary] = []

func _initialize() -> void:
	call_deferred("_run")

func _process(_delta: float) -> bool:
	if Time.get_ticks_msec() - started > 30000:
		printerr("PARTICLE_EMISSION_TEXTURE_TIMEOUT")
		quit(2)
	return false

func _check(ok: bool, label: String) -> void:
	checks += 1
	if not ok:
		failures.append(label)

func _publish(result: Dictionary) -> void:
	observed.emit(result)

func _capture(material: RID) -> void:
	var rd := RenderingServer.get_rendering_device()
	var result := {"main_rd_render_thread": rd != null, "material": str(material), "textures": []}
	if rd != null:
		result["device"] = rd.get_device_name()
		for uniform_name in UNIFORMS:
			var value: Variant = RenderingServer.material_get_param(material, uniform_name)
			result.textures.append(value if value is RID else RID())
		result["points_valid"] = RenderingServer.material_get_param(material, "emission_texture_points_valid")
	call_deferred("_publish", result)

func _snapshot(material: ParticleProcessMaterial, label: String) -> void:
	var expected: Array[RID] = []
	for slot in SLOTS:
		var texture: Texture2D = material.get(slot)
		expected.append(texture.get_rid() if texture != null else RID())
	var points_valid := material.emission_point_texture != null
	var material_rid := material.get_rid()
	await process_frame
	RenderingServer.call_on_render_thread(_capture.bind(material_rid))
	var result: Dictionary = await observed
	_check(result.get("main_rd_render_thread", false), label + ": actual main RD callback")
	_check(result.textures.size() == 3, label + ": three actual material parameters")
	if result.textures.size() == 3:
		for index in range(3):
			_check(result.textures[index] == expected[index], label + ": latest " + SLOTS[index] + " RID")
	_check(result.has("points_valid") and result.points_valid == points_valid, label + ": point validity retains resource-reference semantics")
	result["expected"] = expected
	result["label"] = label
	observations.append(result)

func _connections(texture: Resource, target_id: int) -> int:
	var count := 0
	for connection in texture.get_signal_connection_list("changed"):
		if connection.callable.get_object_id() == target_id:
			count += 1
	return count

func _set_all(material: ParticleProcessMaterial, texture: Texture2D) -> void:
	for slot in SLOTS:
		material.set(slot, texture)

func _emit_on_render_thread(texture: Resource) -> void:
	# Emit only: the atlas member was changed on the main thread with signals blocked.
	var rd := RenderingServer.get_rendering_device()
	texture.emit_changed()
	call_deferred("_publish", {"main_rd_render_thread": rd != null})

func _run() -> void:
	var image := Image.create(4, 4, false, Image.FORMAT_RGBAF)
	image.fill(Color(0.25, 0.5, 0.75, 1.0))
	var a := ImageTexture.create_from_image(image)
	var other_image := Image.create(4, 4, false, Image.FORMAT_RGBAF)
	other_image.fill(Color(0.75, 0.25, 0.5, 1.0))
	var b := ImageTexture.create_from_image(other_image)
	_check(a.get_rid().is_valid() and b.get_rid().is_valid() and a.get_rid() != b.get_rid(), "Two live distinct texture RIDs")
	_check(a.get_image().get_pixel(0, 0) != b.get_image().get_pixel(0, 0), "Live source textures have distinct contents")
	var shared := AtlasTexture.new()
	shared.atlas = a
	var first := ParticleProcessMaterial.new()
	var second := ParticleProcessMaterial.new()
	first.emission_shape = ParticleProcessMaterial.EMISSION_SHAPE_DIRECTED_POINTS
	second.emission_shape = ParticleProcessMaterial.EMISSION_SHAPE_DIRECTED_POINTS
	first.emission_point_count = 1
	second.emission_point_count = 1
	_set_all(first, shared)
	_set_all(second, shared)
	await _snapshot(first, "first initial A all slots")
	await _snapshot(second, "second initial A all slots")
	shared.atlas = b
	await _snapshot(first, "first shared resource A to B all slots")
	await _snapshot(second, "second shared resource A to B all slots")
	# Repeating one slot cannot duplicate subscriptions or detach the other slots.
	for slot in SLOTS:
		first.set(slot, shared)
		first.set(slot, shared)
	_check(_connections(shared, first.get_instance_id()) == 3, "Same-ref setters preserve one subscription per slot")
	shared.atlas = a
	await _snapshot(first, "first changed after same-ref setters")
	await _snapshot(second, "second shares A after same-ref setters")
	shared.atlas = null
	await _snapshot(first, "first shared resource now null RID")
	await _snapshot(second, "second shared resource now null RID")
	shared.atlas = b
	await _snapshot(first, "first shared resource recovers B")
	# Force the only changed signal to originate on the actual render thread.
	shared.set_block_signals(true)
	shared.atlas = a
	shared.set_block_signals(false)
	RenderingServer.call_on_render_thread(_emit_on_render_thread.bind(shared))
	var emission: Dictionary = await observed
	_check(emission.get("main_rd_render_thread", false), "Changed emitted on actual main RD render thread")
	await _snapshot(first, "first render-thread signal uses current A")
	await _snapshot(second, "second render-thread signal uses current A")
	for slot_index in range(3):
		var slot: String = SLOTS[slot_index]
		_set_all(first, shared)
		shared.atlas = a
		var replacement := AtlasTexture.new()
		replacement.atlas = b
		first.set(slot, replacement)
		_check(_connections(shared, first.get_instance_id()) == 2, slot + ": shared resource retains other two slot subscriptions")
		_check(_connections(replacement, first.get_instance_id()) == 1, slot + ": replacement has one subscription")
		shared.atlas = b
		await _snapshot(first, slot + " replacement retains shared other slots")
		shared.atlas = a
		await _snapshot(first, slot + " shared remaining slots still update")
		# Queue old-resource notifications before changing the slot's current member.
		replacement.atlas = a
		first.set(slot, shared)
		shared.atlas = b
		await _snapshot(first, slot + " queued replacement notification reads current shared B")
		shared.atlas = a
		first.set(slot, null)
		await _snapshot(first, slot + " queued shared notification reads current null")
		_check(_connections(shared, first.get_instance_id()) == 2, slot + ": null detaches exactly one shared subscription")
		shared.atlas = b
		await _snapshot(first, slot + " detached slot remains null and other slots update")
	# Detach each slot; changing the old resource must leave all parameters empty.
	_set_all(first, null)
	_check(_connections(shared, first.get_instance_id()) == 0, "All slots detached remove all shared subscriptions")
	shared.atlas = a
	await _snapshot(first, "all detached old resource remains empty")
	# Queue all three callbacks, then release the last strong reference to a material.
	var doomed: ParticleProcessMaterial = ParticleProcessMaterial.new()
	_set_all(doomed, shared)
	doomed.get_rid()
	var doomed_id := doomed.get_instance_id()
	var doomed_weak: WeakRef = weakref(doomed)
	shared.atlas = b
	doomed = null
	await process_frame
	_check(doomed_weak.get_ref() == null, "Material destroyed with three changed callbacks queued")
	_check(_connections(shared, doomed_id) == 0, "Destroyed receiver subscriptions removed")
	await _snapshot(second, "surviving material updates after other material destruction")
	_check(a.get_rid().is_valid() and b.get_rid().is_valid(), "Both distinct source textures remain alive through all stages")
	print("PARTICLE_EMISSION_TEXTURE_RESULT " + JSON.stringify({"checks": checks, "failures": failures, "observations": observations}))
	quit(0 if failures.is_empty() else 1)
