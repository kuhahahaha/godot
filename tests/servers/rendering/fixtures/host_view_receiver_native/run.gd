extends SceneTree
var fixture: RefCounted
var viewport: SubViewport
var camera: Camera3D
var images: Dictionary = {}
var capture_dir: String
var started: int
var replacement_mode: bool

func _initialize() -> void:
 started = Time.get_ticks_msec()
 replacement_mode = OS.get_environment("ANGE_HOST_VIEW_MODE") == "replacement"
 capture_dir = OS.get_environment("ANGE_HOST_VIEW_CAPTURE_DIR")
 if not ClassDB.class_exists("ParticlesScenePairsTestFixture") or not capture_dir.is_absolute_path() or DirAccess.make_dir_recursive_absolute(capture_dir) != OK:
  quit(77)
  return
 fixture = ClassDB.instantiate("ParticlesScenePairsTestFixture")
 call_deferred("_start")

func _pump(target: int) -> bool:
 while int(fixture.call("result").get("stage", -1)) < target:
  if Time.get_ticks_msec() - started > 30000:
   print("HOST_VIEW_RECEIVER_TIMEOUT ", JSON.stringify(fixture.call("result")))
   quit(1)
   return false
  RenderingServer.call_on_render_thread(Callable(fixture, "host_view_gpu_advance"))
  await process_frame
 return true

func _draw() -> void:
 viewport.render_target_update_mode = SubViewport.UPDATE_ONCE
 RenderingServer.force_draw(false, 1.0 / 60.0)

func _image(label: String) -> void:
 var image := viewport.get_texture().get_image()
 image.convert(Image.FORMAT_RGBA8)
 var bytes := image.get_data()
 var different := 0
 for offset in range(0, bytes.size(), 4):
  if bytes[offset] != bytes[0] or bytes[offset + 1] != bytes[1] or bytes[offset + 2] != bytes[2] or bytes[offset + 3] != bytes[3]:
   different += 1
 var hash := HashingContext.new()
 hash.start(HashingContext.HASH_SHA256)
 hash.update(bytes)
 var path := capture_dir.path_join(label + ".png")
 images[label] = {"sha256": hash.finish().hex_encode(), "nonuniform_pixels": different,
  "width": image.get_width(), "height": image.get_height(), "path": path,
  "saved": image.save_png(path) == OK,
  "draw_calls": viewport.get_render_info(Viewport.RENDER_INFO_TYPE_VISIBLE, Viewport.RENDER_INFO_DRAW_CALLS_IN_FRAME)}

func _start() -> void:
 viewport = SubViewport.new()
 viewport.size = Vector2i(128, 128)
 viewport.own_world_3d = true
 viewport.render_target_update_mode = SubViewport.UPDATE_DISABLED
 root.add_child(viewport)
 var world := Node3D.new()
 viewport.add_child(world)
 var environment := WorldEnvironment.new()
 environment.environment = Environment.new()
 environment.environment.background_mode = Environment.BG_COLOR
 environment.environment.background_color = Color(0.04, 0.08, 0.16, 1)
 world.add_child(environment)
 var mesh := MeshInstance3D.new()
 var box := BoxMesh.new()
 box.size = Vector3(1.2, 0.8, 0.6)
 var material := StandardMaterial3D.new()
 material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
 material.albedo_color = Color(0.9, 0.3, 0.08, 1)
 box.material = material
 mesh.mesh = box
 mesh.position = Vector3(-0.35, 0.15, 0)
 world.add_child(mesh)
 camera = Camera3D.new()
 camera.projection = Camera3D.PROJECTION_ORTHOGONAL
 camera.size = 4
 camera.near = 0.1
 camera.far = 20
 world.add_child(camera)
 camera.position = Vector3(0, 0, 5)
 camera.make_current()
 camera.force_update_transform()
 RenderingServer.set_render_loop_enabled(true)
 _draw() # Initialize the actual main RD render buffers before subscription.
 var viewport_rid := viewport.get_viewport_rid()
 RenderingServer.call_on_render_thread(Callable(fixture, "host_view_gpu_begin").bind(viewport_rid, camera.get_camera_rid(), replacement_mode))
 if not await _pump(1):
  return
 _draw() # Sole source is the original native render path.
 if not await _pump(2):
  return
 _image("first")
 if replacement_mode:
  var other := Camera3D.new()
  other.projection = camera.projection
  other.size = camera.size
  other.near = camera.near
  other.far = camera.far
  world.add_child(other)
  other.position = Vector3(0.7, 0.25, 5)
  other.make_current() # Actual viewport camera replacement closes subscribers.
  other.force_update_transform()
  _draw()
  _image("closed")
  RenderingServer.call_on_render_thread(Callable(fixture, "host_view_gpu_close"))
 else:
  RenderingServer.call_on_render_thread(Callable(fixture, "host_view_gpu_close"))
  if not await _pump(3):
   return
  camera.position = Vector3(0.7, 0.25, 5)
  camera.force_update_transform()
  _draw() # Rendering continues after cancellation; the paid leaf stays closed.
  _image("closed")
 if not await _pump(4):
  return
 var report: Dictionary = fixture.call("result")
 var passed: bool = report.get("pass", false) and report.get("calls", 0) == 1
 for value: Dictionary in images.values():
  passed = passed and value["saved"] and value["width"] == 128 and value["height"] == 128 and value["nonuniform_pixels"] > 0 and value["nonuniform_pixels"] < 128 * 128 and value["draw_calls"] > 0
 passed = passed and images["first"]["sha256"] != images["closed"]["sha256"]
 report["images"] = images
 report["mode"] = "replacement" if replacement_mode else "cancel"
 report["pass"] = passed
 print("HOST_VIEW_RECEIVER_NATIVE ", JSON.stringify(report))
 viewport.free()
 fixture = null
 quit(0 if passed else 1)
