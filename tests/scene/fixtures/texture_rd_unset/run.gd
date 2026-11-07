extends SceneTree

var elapsed := 0.0

func _initialize() -> void:
	call_deferred("_start")

func _start() -> void:
	RenderingServer.call_on_render_thread(_probe)

func _process(delta: float) -> bool:
	elapsed += delta
	if elapsed > 30.0:
		printerr("TEXTURE_RD_UNSET_TIMEOUT")
		quit(2)
	return false

func _check(result: Dictionary, condition: bool, label: String) -> void:
	result.checks += 1
	if not condition:
		result.failures.append(label)

func _dimensions(wrapper: Resource, kind: int) -> Array:
	var dimensions := [wrapper.get_width(), wrapper.get_height()]
	if kind == RenderingDevice.TEXTURE_TYPE_3D:
		dimensions.append(wrapper.get_depth())
	elif kind != RenderingDevice.TEXTURE_TYPE_2D:
		dimensions.append(wrapper.get_layers())
	return dimensions

func _probe() -> void:
	var report := {"main_rd": false, "checks": 0, "failures": [], "cases": []}
	var rd := RenderingServer.get_rendering_device()
	if rd == null:
		report.failures.append("Actual main RenderingDevice unavailable")
		call_deferred("_finish", report)
		return
	report.main_rd = true
	var cases := [
		["Texture2DRD", RenderingDevice.TEXTURE_TYPE_2D, 1, 1],
		["Texture2DArrayRD", RenderingDevice.TEXTURE_TYPE_2D_ARRAY, 2, 1],
		["TextureCubemapRD", RenderingDevice.TEXTURE_TYPE_CUBE, 6, 1],
		["TextureCubemapArrayRD", RenderingDevice.TEXTURE_TYPE_CUBE_ARRAY, 12, 1],
		["Texture3DRD", RenderingDevice.TEXTURE_TYPE_3D, 1, 2],
	]
	for config in cases:
		var result := {"name": config[0], "checks": 0, "failures": [], "clear_getters": []}
		var format := RDTextureFormat.new()
		format.texture_type = config[1]
		format.format = RenderingDevice.DATA_FORMAT_R8G8B8A8_UNORM
		format.width = 4
		format.height = 4
		format.depth = config[3]
		format.array_layers = config[2]
		format.usage_bits = RenderingDevice.TEXTURE_USAGE_SAMPLING_BIT
		var source := rd.texture_create(format, RDTextureView.new(), [])
		_check(result, source.is_valid(), "Native texture creation")
		if source.is_valid():
			var wrapper: Resource = ClassDB.instantiate(config[0])
			var notifications := [0, 0]
			wrapper.changed.connect(func(): notifications[0] += 1)
			wrapper.property_list_changed.connect(func(): notifications[1] += 1)
			var populated := [4, 4]
			if config[1] == RenderingDevice.TEXTURE_TYPE_3D:
				populated.append(config[3])
			elif config[1] != RenderingDevice.TEXTURE_TYPE_2D:
				populated.append(config[2])
			var empty := [0, 0] if populated.size() == 2 else [0, 0, 0]
			for cycle in range(2):
				var prefix := "Cycle %d: " % (cycle + 1)
				var before_changed: int = notifications[0]
				wrapper.set_texture_rd_rid(source)
				_check(result, wrapper.get_texture_rd_rid() == source, prefix + "Bind getter")
				_check(result, _dimensions(wrapper, config[1]) == populated, prefix + "Bind dimensions")
				_check(result, notifications[0] == before_changed + 1, prefix + "Bind changed signal")
				var before_property: int = notifications[1]
				before_changed = notifications[0]
				wrapper.set_texture_rd_rid(RID())
				var cleared: RID = wrapper.get_texture_rd_rid()
				result.clear_getters.append(str(cleared))
				_check(result, not cleared.is_valid(), prefix + "Clear native RD getter must be empty")
				_check(result, _dimensions(wrapper, config[1]) == empty, prefix + "Clear dimensions")
				_check(result, notifications[0] == before_changed + 1, prefix + "Clear changed signal")
				_check(result, notifications[1] == before_property + 1, prefix + "Clear property-list signal")
				_check(result, rd.texture_is_valid(source), prefix + "Caller-owned native texture survives clear")
			wrapper = null
			_check(result, rd.texture_is_valid(source), "Caller-owned native texture survives wrapper destruction")
			rd.free_rid(source)
			_check(result, not rd.texture_is_valid(source), "Caller releases native texture")
		report.checks += result.checks
		for failure in result.failures:
			report.failures.append(result.name + ": " + failure)
		report.cases.append(result)
	call_deferred("_finish", report)

func _finish(report: Dictionary) -> void:
	print("TEXTURE_RD_UNSET_RESULT " + JSON.stringify(report))
	quit(0 if report.failures.is_empty() else 1)
