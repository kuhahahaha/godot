extends SceneTree

var finished := false

func _initialize():
	call_deferred("start_probe")

func start_probe():
	RenderingServer.call_on_render_thread(record_probe)
	await create_timer(15).timeout
	if not finished:
		print("MAIN_RD_ASYNC_TIMEOUT=true")
		quit(13)

func record_probe():
	var device := RenderingServer.get_rendering_device()
	if device == null:
		report.call_deferred(false, "no_main_device", 0)
		return
	var length := 1024 * 1024 + 8192
	var offset := 128
	var source := PackedByteArray()
	source.resize(offset + length + 128)
	for i in range(source.size()):
		source[i] = i % 251 + 1
	var buffer := device.storage_buffer_create(source.size(), source)
	if not buffer.is_valid():
		report.call_deferred(false, "buffer_create", 0)
		return
	var status := device.buffer_get_data_async(buffer, receive_bytes.bind(buffer, source.slice(offset, offset + length)), offset, length)
	print("MAIN_RD_ASYNC_ENQUEUE=", status)
	if status != OK:
		device.free_rid(buffer)
		report.call_deferred(false, "enqueue", 0)

func receive_bytes(bytes: PackedByteArray, buffer: RID, expected: PackedByteArray):
	var device := RenderingServer.get_rendering_device()
	device.free_rid(buffer)
	report.call_deferred(bytes == expected, "normal_frame_raw_async", bytes.size())

func report(exact: bool, phase: String, size: int):
	if finished:
		print("MAIN_RD_ASYNC_DUPLICATE=true")
		quit(14)
		return
	finished = true
	print("MAIN_RD_ASYNC_EXACT=", exact, " phase=", phase, " size=", size)
	quit(0 if exact else 12)
