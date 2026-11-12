extends SceneTree

signal observed(value: Dictionary)

const SLOTS = [
	[ParticleProcessMaterial.PARAM_ANGULAR_VELOCITY, "angular_velocity_texture"],
	[ParticleProcessMaterial.PARAM_ORBIT_VELOCITY, "orbit_velocity_curve"],
	[ParticleProcessMaterial.PARAM_LINEAR_ACCEL, "linear_accel_texture"],
	[ParticleProcessMaterial.PARAM_RADIAL_ACCEL, "radial_accel_texture"],
	[ParticleProcessMaterial.PARAM_TANGENTIAL_ACCEL, "tangent_accel_texture"],
	[ParticleProcessMaterial.PARAM_DAMPING, "damping_texture"],
	[ParticleProcessMaterial.PARAM_ANGLE, "angle_texture"],
	[ParticleProcessMaterial.PARAM_SCALE, "scale_curve"],
	[ParticleProcessMaterial.PARAM_HUE_VARIATION, "hue_rot_curve"],
	[ParticleProcessMaterial.PARAM_ANIM_SPEED, "animation_speed_curve"],
	[ParticleProcessMaterial.PARAM_ANIM_OFFSET, "animation_offset_curve"],
	[ParticleProcessMaterial.PARAM_TURB_INFLUENCE_OVER_LIFE, "turbulence_influence_over_life"],
	[ParticleProcessMaterial.PARAM_RADIAL_VELOCITY, "radial_velocity_curve"],
	[ParticleProcessMaterial.PARAM_SCALE_OVER_VELOCITY, "scale_over_velocity_curve"],
	[ParticleProcessMaterial.PARAM_DIRECTIONAL_VELOCITY, "directional_velocity_curve"],
]

var uniform_name := ""
var started := Time.get_ticks_msec()
var checks := 0
var failures: Array[String] = []
var observations: Array[Dictionary] = []

func _initialize() -> void:
	call_deferred("_run")

func _process(_delta: float) -> bool:
	if Time.get_ticks_msec() - started > 30000:
		printerr("PARTICLE_PARAM_TEXTURE_TIMEOUT")
		quit(2)
	return false

func _check(ok: bool, label: String) -> void:
	checks += 1
	if not ok:
		failures.append(uniform_name + "/" + label)

func _publish(value: Dictionary) -> void:
	observed.emit(value)

func _capture(material: RID, immediate: Variant) -> void:
	var rd := RenderingServer.get_rendering_device()
	var value: Variant = RenderingServer.material_get_param(material, uniform_name)
	call_deferred("_publish", {"main_rd": rd != null, "type": typeof(value), "value": value, "immediate": immediate})

func _snapshot(material: ParticleProcessMaterial, expected: Variant, label: String, drain := true) -> void:
	if drain:
		await process_frame
	# Keep the raw Variant: NIL is distinct from an empty RID Variant.
	var immediate: Variant = RenderingServer.material_get_param(material.get_rid(), uniform_name)
	RenderingServer.call_on_render_thread(_capture.bind(material.get_rid(), immediate))
	var result: Dictionary = await observed
	_check(result.main_rd, label + ": main RD render thread")
	_check(result.type == typeof(expected) and result.value == expected, label + ": render-thread raw Variant")
	_check(typeof(result.immediate) == typeof(expected) and result.immediate == expected, label + ": immediate raw Variant")
	observations.append({"label": uniform_name + "/" + label, "type": result.type, "value": str(result.value), "immediate_type": typeof(result.immediate), "immediate": str(result.immediate), "expected_type": typeof(expected), "expected": str(expected)})

func _connections(resource: Resource, id: int) -> Array:
	var found := []
	for connection in resource.get_signal_connection_list("changed"):
		if connection.callable.get_object_id() == id:
			found.append(connection)
	return found

func _subscription(resource: Resource, material: ParticleProcessMaterial, label: String) -> void:
	var connections := _connections(resource, material.get_instance_id())
	_check(connections.size() == 1, label + ": exactly one target connection")
	if connections.size() == 1:
		_check(connections[0].flags == Object.CONNECT_DEFERRED, label + ": deferred flag")

func _emit_on_render_thread(resource: Resource) -> void:
	var rd := RenderingServer.get_rendering_device()
	resource.emit_changed()
	call_deferred("_publish", {"main_rd": rd != null})

func _run_slot(parameter: int, sampler: String) -> void:
	uniform_name = sampler
	var image := Image.create(2, 2, false, Image.FORMAT_RGBA8)
	image.fill(Color.WHITE)
	var a := ImageTexture.create_from_image(image)
	image.fill(Color.RED)
	var b := ImageTexture.create_from_image(image)
	_check(a.get_rid().is_valid() and b.get_rid().is_valid() and a.get_rid() != b.get_rid(), "valid distinct live A/B texture RIDs")
	var wrapper := AtlasTexture.new()
	wrapper.atlas = a
	var first := ParticleProcessMaterial.new()
	var second := ParticleProcessMaterial.new()
	first.set_param_texture(parameter, wrapper)
	second.set_param_texture(parameter, wrapper)
	await _snapshot(first, a.get_rid(), "initial A")
	await _snapshot(second, a.get_rid(), "shared initial A")
	wrapper.atlas = b
	var before_drain: Variant = RenderingServer.material_get_param(first.get_rid(), uniform_name)
	_check(typeof(before_drain) == TYPE_RID and before_drain == a.get_rid(), "changed remains queued before drain")
	await _snapshot(first, b.get_rid(), "A to B")
	await _snapshot(second, b.get_rid(), "shared A to B")
	# Silently change the RID. The same-reference setter must refresh synchronously.
	wrapper.set_block_signals(true)
	wrapper.atlas = a
	wrapper.set_block_signals(false)
	first.set_param_texture(parameter, wrapper)
	first.set_param_texture(parameter, wrapper)
	await _snapshot(first, a.get_rid(), "same-ref immediate refresh", false)
	_subscription(wrapper, first, "same-ref first")
	_subscription(wrapper, second, "shared second")
	wrapper.emit_changed()
	await _snapshot(second, a.get_rid(), "shared notification refresh")
	wrapper.atlas = null
	await _snapshot(first, wrapper.get_rid(), "empty wrapper retains RID Variant")
	_check(typeof(wrapper.get_rid()) == TYPE_RID, "empty wrapper expected type RID")
	wrapper.atlas = b
	await _snapshot(first, b.get_rid(), "empty wrapper recovers")
	wrapper.set_block_signals(true)
	wrapper.atlas = a
	wrapper.set_block_signals(false)
	RenderingServer.call_on_render_thread(_emit_on_render_thread.bind(wrapper))
	var emitted: Dictionary = await observed
	_check(emitted.main_rd, "changed emitted on actual main RD render thread")
	await _snapshot(first, a.get_rid(), "render-thread changed first")
	await _snapshot(second, a.get_rid(), "render-thread changed shared")
	var replacement := AtlasTexture.new()
	replacement.atlas = b
	wrapper.atlas = b
	first.set_param_texture(parameter, replacement)
	# Make a queued old notification read a different current member RID.
	replacement.set_block_signals(true)
	replacement.atlas = a
	replacement.set_block_signals(false)
	_check(_connections(wrapper, first.get_instance_id()).is_empty(), "old resource connection absent")
	_subscription(replacement, first, "replacement")
	await _snapshot(first, a.get_rid(), "queued old callback reads current member")
	wrapper.atlas = a
	wrapper.atlas = b
	await _snapshot(first, a.get_rid(), "old later mutation cannot restore old RID")
	await _snapshot(second, b.get_rid(), "old wrapper still updates shared second")
	replacement.atlas = b
	first.set_param_texture(parameter, null)
	await _snapshot(first, null, "clear immediate NIL before queued drain", false)
	_check(_connections(replacement, first.get_instance_id()).is_empty(), "clear removes resource connection")
	replacement.atlas = a
	await _snapshot(first, null, "detached later mutation stays NIL")
	var doomed: ParticleProcessMaterial = ParticleProcessMaterial.new()
	doomed.set_param_texture(parameter, wrapper)
	doomed.get_rid()
	var doomed_id := doomed.get_instance_id()
	var doomed_weak: WeakRef = weakref(doomed)
	wrapper.atlas = a
	doomed = null
	_check(doomed_weak.get_ref() == null, "receiver destroyed before deferred drain")
	await process_frame
	_check(_connections(wrapper, doomed_id).is_empty(), "destroyed receiver connection absent")
	wrapper.atlas = b
	await _snapshot(second, b.get_rid(), "wrapper alive and mutated after destruction")
	_check(a.get_rid().is_valid() and b.get_rid().is_valid(), "source textures remain live")

func _cross_slot() -> void:
	var image := Image.create(2, 2, false, Image.FORMAT_RGBA8)
	image.fill(Color.WHITE)
	var a := ImageTexture.create_from_image(image)
	image.fill(Color.RED)
	var b := ImageTexture.create_from_image(image)
	_check(a.get_rid().is_valid() and b.get_rid().is_valid() and a.get_rid() != b.get_rid(), "cross-slot valid distinct A/B")
	var wrapper := AtlasTexture.new()
	wrapper.atlas = a
	var replacement := AtlasTexture.new()
	replacement.atlas = a
	var material := ParticleProcessMaterial.new()
	material.set_param_texture(ParticleProcessMaterial.PARAM_ANGULAR_VELOCITY, wrapper)
	material.set_param_texture(ParticleProcessMaterial.PARAM_DAMPING, wrapper)
	_check(_connections(wrapper, material.get_instance_id()).size() == 2, "cross-slot two bound subscriptions")
	uniform_name = "angular_velocity_texture"
	await _snapshot(material, a.get_rid(), "cross-slot initial")
	uniform_name = "damping_texture"
	await _snapshot(material, a.get_rid(), "cross-slot initial")
	wrapper.atlas = b
	uniform_name = "angular_velocity_texture"
	await _snapshot(material, b.get_rid(), "cross-slot shared change")
	uniform_name = "damping_texture"
	await _snapshot(material, b.get_rid(), "cross-slot shared change")
	material.set_param_texture(ParticleProcessMaterial.PARAM_ANGULAR_VELOCITY, replacement)
	_check(_connections(wrapper, material.get_instance_id()).size() == 1, "replacing one slot preserves other subscription")
	wrapper.atlas = a
	wrapper.atlas = b
	uniform_name = "angular_velocity_texture"
	await _snapshot(material, a.get_rid(), "cross-slot detached first")
	uniform_name = "damping_texture"
	await _snapshot(material, b.get_rid(), "cross-slot retained second")
	material.set_param_texture(ParticleProcessMaterial.PARAM_ANGULAR_VELOCITY, null)
	wrapper.atlas = a
	uniform_name = "damping_texture"
	await _snapshot(material, a.get_rid(), "cross-slot second survives clear")
	uniform_name = "angular_velocity_texture"
	await _snapshot(material, null, "cross-slot first stays NIL")
	var passive := ParticleProcessMaterial.new()
	for parameter in [ParticleProcessMaterial.PARAM_INITIAL_LINEAR_VELOCITY, ParticleProcessMaterial.PARAM_TURB_VEL_INFLUENCE, ParticleProcessMaterial.PARAM_TURB_INIT_DISPLACEMENT]:
		passive.set_param_texture(parameter, wrapper)
	_check(_connections(wrapper, passive.get_instance_id()).is_empty(), "non-sampler slots have no subscriptions")

func _run() -> void:
	for slot in SLOTS:
		await _run_slot(slot[0], slot[1])
	await _cross_slot()
	print("PARTICLE_PARAM_TEXTURE_RESULT " + JSON.stringify({"checks": checks, "failures": failures, "observations": observations}))
	quit(0 if failures.is_empty() else 1)
