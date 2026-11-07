extends SceneTree

signal completed(result: Dictionary)

var fixture: RefCounted
var checks := 0
var failures: Array[String] = []
var observations: Array[Dictionary] = []
var started := Time.get_ticks_msec()
var sources: Array[RID] = []

func _initialize() -> void:
	if not ClassDB.class_exists("ParticlesExternalGuardTestFixture"):
		printerr("PARTICLE_TEXTURE_UNAVAILABLE: tests=yes module_particles_runtime_tests_enabled=yes required")
		quit(77)
		return
	fixture = ClassDB.instantiate("ParticlesExternalGuardTestFixture")
	if not fixture.has_method("observe_particle_collision_texture"):
		printerr("PARTICLE_TEXTURE_UNAVAILABLE: collision observer missing")
		quit(77)
		return
	call_deferred("_run")

func _process(_delta: float) -> bool:
	if Time.get_ticks_msec() - started > 30000:
		printerr("PARTICLE_TEXTURE_TIMEOUT")
		quit(2)
	return false

func _check(ok: bool, label: String) -> void:
	checks += 1
	if not ok:
		failures.append(label)

func _publish(result: Dictionary) -> void:
	completed.emit(result)

func _create_sources() -> void:
	var rd := RenderingServer.get_rendering_device()
	var result := {"available": rd != null, "sources": []}
	if rd != null:
		for _index in range(2):
			var format := RDTextureFormat.new()
			format.texture_type = RenderingDevice.TEXTURE_TYPE_3D
			format.format = RenderingDevice.DATA_FORMAT_R8G8B8A8_UNORM
			format.width = 4
			format.height = 4
			format.depth = 4
			format.usage_bits = RenderingDevice.TEXTURE_USAGE_SAMPLING_BIT
			result.sources.append(rd.texture_create(format, RDTextureView.new(), []))
	call_deferred("_publish", result)

func _bind_on_render_thread(texture: Texture3DRD, native: RID) -> void:
	# Both valid binding and its changed emission occur on the actual main RD thread.
	texture.texture_rd_rid = native
	call_deferred("_publish", {"native": texture.texture_rd_rid, "rid": texture.get_rid()})

func _bind(texture: Texture3DRD, native: RID) -> void:
	RenderingServer.call_on_render_thread(_bind_on_render_thread.bind(texture, native))
	var result: Dictionary = await completed
	_check(result.native == native and result.rid.is_valid(), "Main RD bind publishes requested source")

func _capture(collision: RID) -> void:
	call_deferred("_publish", fixture.observe_particle_collision_texture(collision))

func _snapshot(node: VisualInstance3D, texture: Texture3DRD, label: String) -> void:
	# Clearing Texture3DRD leaves a lazy 2D placeholder; it cannot resolve as a 3D field.
	var expected := texture.get_rid() if texture != null else RID()
	var populated := texture != null and texture.texture_rd_rid.is_valid()
	await process_frame
	RenderingServer.call_on_render_thread(_capture.bind(node.get_base()))
	var result: Dictionary = await completed
	_check(result.get("main_rd_render_thread", false), label + ": actual main RD thread")
	_check(result.get("collision_valid", false), label + ": actual collision owner")
	_check(result.has("field_texture") and result.field_texture == expected, label + ": latest resource RS RID")
	_check(result.has("resolved_3d_texture") and result.resolved_3d_texture.is_valid() == populated, label + ": actual 3D binding availability")
	result["expected"] = str(expected)
	result["label"] = label
	observations.append(result)

func _connections(texture: Resource, target: Object) -> int:
	var count := 0
	for connection in texture.get_signal_connection_list("changed"):
		if connection.callable.get_object() == target:
			count += 1
	return count

func _release_sources() -> void:
	var rd := RenderingServer.get_rendering_device()
	var alive := true
	for source in sources:
		alive = alive and rd.texture_is_valid(source)
		if rd.texture_is_valid(source):
			rd.free_rid(source)
	call_deferred("_publish", {"caller_sources_survived": alive})

func _run() -> void:
	RenderingServer.call_on_render_thread(_create_sources)
	var created: Dictionary = await completed
	_check(created.available and created.sources.size() == 2, "Original main RD available")
	if not created.available or created.sources.size() != 2:
		_finish()
		return
	for source in created.sources:
		sources.append(source)
	_check(sources[0].is_valid() and sources[1].is_valid() and sources[0] != sources[1], "Two distinct real native 3D textures")
	if not sources[0].is_valid() or not sources[1].is_valid():
		_finish()
		return
	var shared := Texture3DRD.new()
	await _bind(shared, sources[0])
	var nodes: Array[VisualInstance3D] = [GPUParticlesCollisionSDF3D.new(), GPUParticlesAttractorVectorField3D.new()]
	for node in nodes:
		root.add_child(node)
		node.set("texture", shared)
		await _snapshot(node, shared, node.get_class() + " initial A")
	# Two cycles ensure the original association cannot accidentally recover once.
	for cycle in range(2):
		shared.texture_rd_rid = RID()
		for node in nodes:
			await _snapshot(node, shared, node.get_class() + " shared clear %d" % cycle)
		await _bind(shared, sources[(cycle + 1) % 2])
		for node in nodes:
			await _snapshot(node, shared, node.get_class() + " render-thread rebind %d" % cycle)
	for node in nodes:
		var prefix := node.get_class()
		node.set("texture", shared)
		node.set("texture", shared)
		_check(_connections(shared, node) == 1, prefix + " repeated same-ref has one subscription")
		await _snapshot(node, shared, prefix + " repeated same-ref")
		var replacement := Texture3DRD.new()
		await _bind(replacement, sources[0])
		node.set("texture", replacement)
		_check(_connections(shared, node) == 0, prefix + " old subscription detached")
		_check(_connections(replacement, node) == 1, prefix + " replacement subscribed")
		shared.emit_changed()
		await _snapshot(node, replacement, prefix + " old resource signal after replacement")
		# Queue the old notification and replace synchronously before it can run.
		replacement.emit_changed()
		node.set("texture", shared)
		await _snapshot(node, shared, prefix + " queued old signal reads current member")
		shared.emit_changed()
		node.set("texture", null)
		await _snapshot(node, null, prefix + " queued old signal reads current null")
		_check(_connections(shared, node) == 0, prefix + " null detaches subscription")
		shared.emit_changed()
		await _snapshot(node, null, prefix + " detached signal keeps null")
		node.set("texture", replacement)
		replacement.emit_changed()
		node.free()
		await process_frame
		_check(replacement.get_signal_connection_list("changed").is_empty(), prefix + " destroyed receiver disconnects with queued signal")
		replacement.texture_rd_rid = RID()
	# Drop all wrapper associations before checking caller ownership and freeing sources.
	shared.texture_rd_rid = RID()
	RenderingServer.call_on_render_thread(_release_sources)
	var released: Dictionary = await completed
	_check(released.caller_sources_survived, "Caller-owned native sources survive wrapper clear and node destruction")
	_finish()

func _finish() -> void:
	print("PARTICLE_TEXTURE_RESULT " + JSON.stringify({"checks": checks, "failures": failures, "observations": observations}))
	quit(0 if failures.is_empty() else 1)
