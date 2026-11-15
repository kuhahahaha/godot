extends SceneTree

signal observed(result: Dictionary)

var fixture: RefCounted
var checks := 0
var failures: Array[String] = []
var observations: Array[Dictionary] = []
var started := Time.get_ticks_msec()

func _initialize() -> void:
	if not ClassDB.class_exists("ParticlesExternalGuardTestFixture"):
		printerr("VRS_TARGET_UNAVAILABLE: build with tests=yes module_particles_runtime_tests_enabled=yes")
		quit(77)
		return
	fixture = ClassDB.instantiate("ParticlesExternalGuardTestFixture")
	if not fixture.has_method("observe_viewport_vrs_texture"):
		printerr("VRS_TARGET_UNAVAILABLE: test observer missing")
		quit(77)
		return
	call_deferred("_run")

func _process(_delta: float) -> bool:
	if Time.get_ticks_msec() - started > 30000:
		printerr("VRS_TARGET_TIMEOUT")
		quit(2)
	return false

func _check(ok: bool, label: String) -> void:
	checks += 1
	if not ok:
		failures.append(label)

func _capture(viewport: RID) -> void:
	var result: Dictionary = fixture.observe_viewport_vrs_texture(viewport)
	call_deferred("_publish", result)

func _emit_changed_on_render_thread(texture: Resource, viewport: RID) -> void:
	var result: Dictionary = fixture.observe_viewport_vrs_texture(viewport)
	texture.emit_changed()
	call_deferred("_publish", result)

func _publish(result: Dictionary) -> void:
	observed.emit(result)

func _snapshot(viewport: SubViewport, expected: RID, label: String) -> void:
	# Drain a main-loop turn so deferred changed callbacks are eligible to run.
	await process_frame
	RenderingServer.call_on_render_thread(_capture.bind(viewport.get_viewport_rid()))
	var result: Dictionary = await observed
	_check(result.get("main_rd_render_thread", false), label + ": actual main RD render thread")
	_check(result.get("target_valid", false), label + ": actual render target valid")
	_check(result.has("vrs_texture") and result.vrs_texture == expected, label + ": actual target VRS RID")
	result["expected"] = str(expected)
	result["label"] = label
	observations.append(result)

func _connections(texture: Resource, target: Object) -> int:
	var count := 0
	for connection in texture.get_signal_connection_list("changed"):
		if connection.callable.get_object() == target:
			count += 1
	return count

func _run() -> void:
	var image := Image.create(4, 4, false, Image.FORMAT_RGBA8)
	image.fill(Color.WHITE)
	var a := ImageTexture.create_from_image(image)
	image.fill(Color.BLACK)
	var b := ImageTexture.create_from_image(image)
	_check(a.get_rid().is_valid() and b.get_rid().is_valid() and a.get_rid() != b.get_rid(), "Two distinct actual texture RIDs")
	var atlas := AtlasTexture.new()
	atlas.atlas = a
	var v1 := SubViewport.new()
	var v2 := SubViewport.new()
	root.add_child(v1)
	root.add_child(v2)
	v1.vrs_texture = atlas
	v2.vrs_texture = atlas
	await _snapshot(v1, a.get_rid(), "v1 initial")
	await _snapshot(v2, a.get_rid(), "v2 initial")
	atlas.atlas = b
	await _snapshot(v1, b.get_rid(), "v1 same resource changed A to B")
	await _snapshot(v2, b.get_rid(), "v2 shared resource changed A to B")
	v1.vrs_texture = atlas
	v1.vrs_texture = atlas
	await _snapshot(v1, b.get_rid(), "v1 repeated same-ref setter repairs/keeps current B")
	_check(_connections(atlas, v1) == 1, "Repeated same-ref setter has one changed subscription")
	atlas.atlas = null
	await _snapshot(v1, RID(), "v1 same-ref changed to null RID")
	await _snapshot(v2, RID(), "v2 shared changed to null RID")
	var replacement := AtlasTexture.new()
	replacement.atlas = a
	v1.vrs_texture = replacement
	await _snapshot(v1, a.get_rid(), "v1 replacement initial A")
	_check(_connections(atlas, v1) == 0, "Old resource subscription detached")
	_check(_connections(replacement, v1) == 1, "Replacement resource subscribed")
	atlas.atlas = b
	await _snapshot(v1, a.get_rid(), "v1 old resource change leaves replacement A")
	await _snapshot(v2, b.get_rid(), "v2 old shared resource remains live")
	replacement.atlas = b
	await _snapshot(v1, b.get_rid(), "v1 replacement changed A to B")
	v2.vrs_texture = null
	await _snapshot(v2, RID(), "v2 explicit null setter")
	_check(_connections(atlas, v2) == 0, "Null setter detaches subscription")
	atlas.atlas = a
	await _snapshot(v2, RID(), "v2 detached resource change leaves null")
	v1.vrs_texture = replacement
	replacement.atlas = null
	await _snapshot(v1, RID(), "v1 replacement same-ref changed to null")
	# Establish an empty target even on the baseline, avoiding stale-RID coincidence.
	v1.vrs_texture = replacement
	# Change the RID without a main-thread notification, then emit only on RT.
	replacement.set_block_signals(true)
	replacement.atlas = b
	replacement.set_block_signals(false)
	RenderingServer.call_on_render_thread(_emit_changed_on_render_thread.bind(replacement, v1.get_viewport_rid()))
	var emission: Dictionary = await observed
	_check(emission.get("main_rd_render_thread", false), "Changed signal emitted on actual main RD render thread")
	_check(emission.get("target_valid", false), "Render-thread emission retains actual target")
	_check(emission.has("vrs_texture") and not emission.vrs_texture.is_valid(), "Render-thread emission starts from observed empty target")
	await _snapshot(v1, b.get_rid(), "Render-thread changed refreshes on main thread")
	# These old-resource notifications are queued before the setter replaces it.
	replacement.atlas = a
	var latest := AtlasTexture.new()
	latest.atlas = b
	v1.vrs_texture = latest
	await _snapshot(v1, b.get_rid(), "Queued old notification reads current replacement B")
	latest.atlas = a
	v1.vrs_texture = null
	await _snapshot(v1, RID(), "Queued old notification reads current null member")
	v1.vrs_texture = latest
	latest.atlas = b
	v1.free()
	v2.free()
	await process_frame
	_check(latest.get_signal_connection_list("changed").is_empty(), "Destroyed viewport subscription removed with notification queued")
	print("VRS_TARGET_RESULT " + JSON.stringify({"checks": checks, "failures": failures, "observations": observations}))
	quit(0 if failures.is_empty() else 1)
