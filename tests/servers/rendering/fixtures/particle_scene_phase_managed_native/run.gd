extends SceneTree
const REQUIRED_CHECKS = [
	"output_scene_public_exact_reentry",
	"output_scene_uncommitted_busy",
	"output_scene_original_committed_facts",
	"output_scene_committed_host_matches",
	"output_scene_dirty_busy",
	"output_scene_committed_wrong_host",
	"output_scene_restored_host_matches",
	"output_scene_wrong_scenario",
	"output_scene_wrong_base",
	"output_scene_restored_identity_matches",
	"output_scene_wrong_generation",
	"output_scene_stale_instance",
	"output_scene_original_request_released",

	"connection_public_bad_fields_atomic",
	"connection_public_bad_integers_atomic",
	"connection_public_bad_kind_resolution_atomic",
	"connection_public_bad_lengths_atomic",
	"connection_public_bad_schema_atomic",
	"connection_public_bad_types_atomic",
	"connection_public_capacity_atomic",
	"connection_public_collision_instance_atomic",
	"connection_public_empty_rid_atomic",
	"connection_public_height_limits_atomic",
	"connection_public_hidden_revoked_atomic",
	"connection_public_no_scenario_atomic",
	"connection_public_particles_rid_atomic",
	"connection_public_phase_closed_revoked_atomic",
	"connection_public_source_limit_atomic",
	"connection_public_stale_generation_atomic",
	"connection_public_wrong_base_atomic",
	"connection_public_wrong_device_tag_atomic",
	"connection_public_sphere_boundaries_connect",
	"connection_public_height_boundaries_connect",
]
var fixture: RefCounted
var started: int
func _initialize() -> void:
	if not ClassDB.class_exists("ParticlesScenePairsTestFixture"):
		print("SCENE_PHASE_MANAGED_NATIVE_MISSING")
		quit(1)
		return
	fixture = ClassDB.instantiate("ParticlesScenePairsTestFixture")
	started = Time.get_ticks_msec()
	call_deferred("_start")
func _start() -> void:
	RenderingServer.set_render_loop_enabled(true)
	RenderingServer.call_on_render_thread(Callable(fixture, "run_phase"))
func _process(_delta: float) -> bool:
	if fixture == null:
		return false
	# Headless main loops may not enter RenderingServer::draw automatically.
	# Force the production draw path so _draw() performs scene_phase_dispatch.
	if RenderingServer.is_render_loop_enabled():
		RenderingServer.force_draw(false, _delta)
	RenderingServer.call_on_render_thread(Callable(fixture, "advance"))
	var result: Dictionary = fixture.call("result")
	if result.has("pass"):
		var passed: bool = typeof(result["pass"]) == TYPE_BOOL and result["pass"] == true
		if typeof(result.get("checks")) != TYPE_DICTIONARY:
			print("SCENE_PHASE_MANAGED_NATIVE_INVALID_CHECKS")
			passed = false
		else:
			var checks: Dictionary = result["checks"]
			for key in REQUIRED_CHECKS:
				if not checks.has(key) or typeof(checks[key]) != TYPE_BOOL or checks[key] != true:
					print("SCENE_PHASE_MANAGED_NATIVE_INVALID_CHECK ", key)
					passed = false
		print("SCENE_PHASE_MANAGED_NATIVE ", JSON.stringify(result))
		quit(0 if passed else 1)
	elif Time.get_ticks_msec() - started > 30000:
		print("SCENE_PHASE_MANAGED_NATIVE_TIMEOUT")
		quit(1)
	return false
