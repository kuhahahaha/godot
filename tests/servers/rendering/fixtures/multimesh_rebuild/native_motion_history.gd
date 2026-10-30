extends SceneTree

signal velocity_read

# Original M09/ORACLES: orthographic width8, +1 world => prev-current=-1/8.
# The compositor only retains the public velocity RID. Actual texture readback
# runs later on the render thread, after the viewport has finished its frame.
class VelocityObserver extends CompositorEffect:
	var velocity: RID
	var size := Vector2i.ZERO
	var callbacks := 0

	func _init() -> void:
		effect_callback_type = EFFECT_CALLBACK_TYPE_POST_TRANSPARENT
		needs_motion_vectors = true

	func _render_callback(_callback_type: int, render_data: RenderData) -> void:
		var buffers := render_data.get_render_scene_buffers() as RenderSceneBuffersRD
		velocity = buffers.get_velocity_texture() if needs_motion_vectors else RID()
		size = buffers.get_internal_size()
		callbacks += 1

var observer: VelocityObserver
var bytes := PackedByteArray()
var checks := 0
var failures := 0
var output := ""
var omit_move := false
var cached_transform := false
var cached_hidden_first := false
var cached_start_hidden := false
var cached_late_motion := false

func _initialize() -> void:
	call_deferred("_run")

func _buffer(count: int, x: float) -> PackedFloat32Array:
	var data := PackedFloat32Array()
	data.resize(count * 12)
	for i in range(count):
		data[i * 12] = 1.0
		data[i * 12 + 5] = 1.0
		data[i * 12 + 10] = 1.0
		data[i * 12 + 3] = x if i == 0 else 20.0 + i
	return data

func _upload(mm: MultiMesh, count: int, x: float) -> void:
	if cached_transform:
		for i in range(count):
			var offset := x if i == 0 else 20.0 + i
			mm.set_instance_transform(i, Transform3D(Basis.IDENTITY, Vector3(offset, 0, 0)))
	else:
		mm.buffer = _buffer(count, x)

func _read_velocity() -> void:
	var rd := RenderingServer.get_rendering_device()
	var data := PackedByteArray()
	if rd == null or not observer.velocity.is_valid():
		call_deferred("_receive_velocity", data)
		return
	var format := rd.texture_get_format(observer.velocity)
	if format.format != RenderingDevice.DATA_FORMAT_R16G16_SFLOAT or format.width != 64 or format.height != 64:
		print("K02_MOTION_FORMAT_INVALID")
		call_deferred("_receive_velocity", data)
		return
	print("K02_MOTION_SOURCE ", JSON.stringify({"format": "R16G16_SFLOAT", "width": format.width, "height": format.height, "main_rd": true}))
	# Native velocity is sampled normally; it is not a COPY_FROM texture.
	# Copy values to an owned SSBO with a complete bounded compute pass.
	var source := RDShaderSource.new()
	source.source_compute = """#version 450
layout(local_size_x=8,local_size_y=8,local_size_z=1) in;
layout(set=0,binding=0) uniform sampler2D velocity;
layout(set=0,binding=1,std430) writeonly buffer Values { vec2 values[]; } output_data;
void main() {
	ivec2 p=ivec2(gl_GlobalInvocationID.xy);
	output_data.values[p.y*64+p.x]=texelFetch(velocity,p,0).xy;
}
"""
	var spirv := rd.shader_compile_spirv_from_source(source)
	if not spirv.compile_error_compute.is_empty():
		print("K02_MOTION_SHADER_ERROR ", spirv.compile_error_compute)
		call_deferred("_receive_velocity", data)
		return
	var resources: Array[RID] = []
	var shader := rd.shader_create_from_spirv(spirv)
	if shader.is_valid():
		resources.append(shader)
		var pipeline := rd.compute_pipeline_create(shader)
		var sampler := rd.sampler_create(RDSamplerState.new())
		var buffer := rd.storage_buffer_create(64 * 64 * 8)
		for rid in [pipeline, sampler, buffer]:
			if rid.is_valid():
				resources.append(rid)
		if pipeline.is_valid() and sampler.is_valid() and buffer.is_valid():
			var texture_uniform := RDUniform.new()
			texture_uniform.uniform_type = RenderingDevice.UNIFORM_TYPE_SAMPLER_WITH_TEXTURE
			texture_uniform.binding = 0
			texture_uniform.add_id(sampler)
			texture_uniform.add_id(observer.velocity)
			var output_uniform := RDUniform.new()
			output_uniform.uniform_type = RenderingDevice.UNIFORM_TYPE_STORAGE_BUFFER
			output_uniform.binding = 1
			output_uniform.add_id(buffer)
			var uniforms := rd.uniform_set_create([texture_uniform, output_uniform], shader, 0)
			if uniforms.is_valid():
				resources.append(uniforms)
				var list := rd.compute_list_begin()
				rd.compute_list_bind_compute_pipeline(list, pipeline)
				rd.compute_list_bind_uniform_set(list, uniforms, 0)
				rd.compute_list_dispatch(list, 8, 8, 1)
				rd.compute_list_end()
				data = rd.buffer_get_data(buffer)
	resources.reverse()
	for rid in resources:
		rd.free_rid(rid)
	call_deferred("_receive_velocity", data)

func _receive_velocity(data: PackedByteArray) -> void:
	bytes = data
	velocity_read.emit()

func _sample(stage: String, label: String, xy: Vector2i, expected: Vector2, image: Image) -> void:
	for y in range(-1, 2):
		for x in range(-1, 2):
			var point := xy + Vector2i(x, y)
			var value := image.get_pixelv(point)
			var ok := absf(value.r - expected.x) <= pow(2.0, -12.0) and absf(value.g - expected.y) <= pow(2.0, -12.0)
			checks += 1
			if not ok:
				failures += 1
			print("K02_MOTION_SAMPLE ", JSON.stringify({"stage": stage, "label": label, "xy": [point.x, point.y], "expected": [expected.x, expected.y], "actual": [value.r, value.g], "pass": ok}))

func _stage(stage: String, viewport: SubViewport, center_x: int, expected: Vector2) -> void:
	var prior_callbacks := observer.callbacks
	viewport.render_target_update_mode = SubViewport.UPDATE_ONCE
	# Require an actual new callback, not only a count of elapsed main frames.
	for i in range(12):
		await RenderingServer.frame_post_draw
		if observer.callbacks > prior_callbacks:
			break
	if observer.callbacks <= prior_callbacks or observer.size != Vector2i(64, 64):
		failures += 1
		print("K02_MOTION_FRAME_INVALID ", stage)
		return
	RenderingServer.call_on_render_thread(_read_velocity)
	await velocity_read
	if bytes.size() != 64 * 64 * 8:
		failures += 1
		print("K02_MOTION_BYTES_INVALID ", stage, " ", bytes.size())
		return
	if not output.is_empty():
		var file := FileAccess.open(output.path_join(stage + ".rgf"), FileAccess.WRITE)
		if file == null:
			failures += 1
			return
		file.store_buffer(bytes)
	var image := Image.create_from_data(64, 64, false, Image.FORMAT_RGF, bytes)
	_sample(stage, "multimesh", Vector2i(center_x, 32), expected, image)
	_sample(stage, "background", Vector2i(5, 5), Vector2.ZERO, image)
	var color := viewport.get_texture().get_image()
	if not output.is_empty() and color.save_png(output.path_join(stage + ".png")) != OK:
		failures += 1
		return
	var pixel := color.get_pixel(center_x, 32)
	var color_ok := absf(pixel.r) <= 1.0 / 255.0 and absf(pixel.g - 1.0) <= 1.0 / 255.0 and absf(pixel.b) <= 1.0 / 255.0
	checks += 1
	if not color_ok:
		failures += 1
	print("K02_MOTION_COLOR ", JSON.stringify({"stage": stage, "xy": [center_x, 32], "actual": [pixel.r, pixel.g, pixel.b], "pass": color_ok}))
	print("K02_MOTION_STAGE ", JSON.stringify({"stage": stage, "center_x": center_x, "expected_velocity": [expected.x, expected.y], "callback_delta": observer.callbacks - prior_callbacks, "bytes": bytes.size()}))

func _run() -> void:
	if RenderingServer.get_rendering_device() == null or RenderingServer.get_current_rendering_method() != "forward_plus":
		print("K02_MOTION_UNAVAILABLE Forward+ main RD required")
		quit(77)
		return
	omit_move = OS.get_cmdline_user_args().has("--omit-second-move")
	cached_hidden_first = OS.get_cmdline_user_args().has("--cached-hidden-first")
	cached_start_hidden = OS.get_cmdline_user_args().has("--cached-start-hidden")
	cached_late_motion = OS.get_cmdline_user_args().has("--cached-late-motion")
	cached_transform = OS.get_cmdline_user_args().has("--cached-transform") or cached_hidden_first or cached_start_hidden or cached_late_motion
	output = OS.get_environment("GODOT_TEST_K02_MOTION_OUT")
	print("K02_MOTION_RUNTIME ", JSON.stringify({"omit_second_move": omit_move, "cached_transform": cached_transform, "cached_hidden_first": cached_hidden_first, "cached_start_hidden": cached_start_hidden, "cached_late_motion": cached_late_motion, "cache_enabled": ProjectSettings.get_setting("rendering/shader_compiler/shader_cache/enabled"), "editor_hint": Engine.is_editor_hint(), "renderer": RenderingServer.get_current_rendering_method()}))
	var viewport := SubViewport.new()
	viewport.size = Vector2i(64, 64)
	viewport.own_world_3d = true
	viewport.render_target_update_mode = SubViewport.UPDATE_DISABLED
	viewport.use_taa = false
	viewport.msaa_3d = Viewport.MSAA_DISABLED
	root.add_child(viewport)
	var environment := Environment.new()
	environment.background_mode = Environment.BG_COLOR
	environment.background_color = Color.BLACK
	environment.tonemap_mode = Environment.TONE_MAPPER_LINEAR
	var world := WorldEnvironment.new()
	world.environment = environment
	observer = VelocityObserver.new()
	observer.needs_motion_vectors = not cached_late_motion
	var compositor := Compositor.new()
	compositor.compositor_effects = [observer]
	world.compositor = compositor
	viewport.add_child(world)
	var camera := Camera3D.new()
	camera.projection = Camera3D.PROJECTION_ORTHOGONAL
	camera.size = 8.0
	camera.position.z = 5.0
	viewport.add_child(camera)
	camera.make_current()
	# Establish camera history before introducing the tested MultiMesh.
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	for i in range(6):
		await RenderingServer.frame_post_draw
	viewport.render_target_update_mode = SubViewport.UPDATE_DISABLED
	var material := StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.albedo_color = Color.GREEN
	var mesh := QuadMesh.new()
	mesh.size = Vector2(0.75, 0.75)
	mesh.material = material
	var mm := MultiMesh.new()
	mm.transform_format = MultiMesh.TRANSFORM_3D
	mm.mesh = mesh
	var initial_count := 1536 if cached_hidden_first else 3
	mm.instance_count = initial_count
	var instance := MultiMeshInstance3D.new()
	instance.multimesh = mm
	viewport.add_child(instance)
	if cached_hidden_first or cached_start_hidden or cached_late_motion:
		if cached_hidden_first:
			# Only region2 is dirty while regions0/1 are visible: no upload.
			mm.visible_instance_count = 1024
			mm.set_instance_transform(1024, Transform3D(Basis.IDENTITY, Vector3(20, 0, 0)))
		elif cached_start_hidden:
			mm.visible_instance_count = 0
			_upload(mm, initial_count, -1.0)
		else:
			# Establish an older cached position before motion is enabled.
			_upload(mm, initial_count, -2.0)
		var callbacks := observer.callbacks
		viewport.render_target_update_mode = SubViewport.UPDATE_ONCE
		for i in range(12):
			await RenderingServer.frame_post_draw
			if observer.callbacks > callbacks:
				break
		if observer.callbacks <= callbacks:
			failures += 1
			print("K02_MOTION_FRAME_INVALID cached_prelude")
		if cached_start_hidden:
			mm.visible_instance_count = -1
		observer.needs_motion_vectors = true
	_upload(mm, initial_count, -1.0)
	await _stage("initial", viewport, 24, Vector2(-0.125 if cached_late_motion else 0.0, 0.0))
	_upload(mm, initial_count, 0.0)
	await _stage("move_1", viewport, 32, Vector2(-0.125, 0.0))
	if not omit_move:
		_upload(mm, initial_count, 1.0)
	await _stage("move_2", viewport, 40, Vector2(-0.125, 0.0))
	mm.instance_count = 5
	mm.visible_instance_count = -1
	_upload(mm, 5, 1.0)
	await _stage("resize", viewport, 40, Vector2.ZERO)
	_upload(mm, 5, 2.0)
	await _stage("move_after_resize", viewport, 48, Vector2(-0.125, 0.0))
	for i in range(3):
		await RenderingServer.frame_post_draw
	await _stage("hold", viewport, 48, Vector2.ZERO)
	viewport.queue_free()
	for i in range(2):
		await process_frame
	print("K02_MOTION_RESULT ", JSON.stringify({"stages": 6, "checks": checks, "failures": failures, "pass": failures == 0, "scope": "ordinary_multimesh_velocity_texture"}))
	quit(0 if failures == 0 else 1)
