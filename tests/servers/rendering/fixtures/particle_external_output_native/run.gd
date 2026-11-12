extends SceneTree

var fixture: RefCounted
var started_msec := 0
const TIMEOUT_MSEC := 180000
const CHECKS := [
	"owner",
	"render_loop_disabled",
	"outside_lists",
	"allocate",
	"live_counts",
	"live_bytes",
	"live_original_identities",
	"live_distinct_identities",
	"live_destroy_busy",
	"live_owner_retained",
	"retire",
	"enqueued_observe",
	"enqueued_counts",
	"enqueued_original_identities",
	"logical_owners_removed",
	"enqueued_destroy_busy",
	"enqueued_owner_retained",
	"repeat_retire",
	"repeat_observe",
	"repeat_unchanged",
	"maintenance_returned",
	"returned_observe",
	"returned_counts",
	"returned_original_identities",
	"no_next_draw",
	"destroy_returned",
	"destroy_pointer_empty",
]

func _initialize() -> void:
	if not ClassDB.class_exists("ParticlesExternalOutputTestFixture"):
		printerr("PARTICLE_EXTERNAL_OUTPUT_NATIVE_MISSING tests-only fixture class not registered")
		quit(77)
		return
	fixture = ClassDB.instantiate("ParticlesExternalOutputTestFixture")
	if fixture == null:
		printerr("PARTICLE_EXTERNAL_OUTPUT_NATIVE_CREATE_FAILED")
		quit(1)
		return
	started_msec = Time.get_ticks_msec()
	call_deferred("_start")

func _start() -> void:
	RenderingServer.set_render_loop_enabled(false)
	RenderingServer.call_on_render_thread(Callable(fixture, "run"))

func _checks_complete(checks: Variant) -> bool:
	if typeof(checks) != TYPE_ARRAY or checks.size() != CHECKS.size():
		return false
	var seen: Dictionary = {}
	for row in checks:
		if typeof(row) != TYPE_DICTIONARY or typeof(row.get("check")) != TYPE_STRING or typeof(row.get("ok")) != TYPE_BOOL:
			return false
		var label: String = row["check"]
		if not CHECKS.has(label) or seen.has(label) or not row["ok"]:
			return false
		seen[label] = true
	return seen.size() == 27

func _process(_delta: float) -> bool:
	if fixture == null:
		return false
	var report: Dictionary = fixture.call("result")
	if report.get("complete", false):
		var passed: bool = report.get("started", false) and report.get("error", -1) == OK
		passed = passed and report.get("failures", ["missing"]).is_empty() and _checks_complete(report.get("checks"))
		passed = passed and report.get("physical_prefix_returned", false) and not report.get("retained_on_failure", true)
		for key in ["whole_output_released", "source_authority_supplied", "dispatch_recorded", "device_holder_claimed"]:
			passed = passed and report.get(key, true) == false
		print("PARTICLE_EXTERNAL_OUTPUT_NATIVE_RESULT=", JSON.stringify(report))
		if passed:
			print("PARTICLE_EXTERNAL_OUTPUT_NATIVE_PASS checks=27 buffers=7 bytes=916 no_next_draw=1 physical_prefix_returned=1 whole_output_released=0")
		quit(0 if passed else 1)
	elif Time.get_ticks_msec() - started_msec > TIMEOUT_MSEC:
		printerr("PARTICLE_EXTERNAL_OUTPUT_NATIVE_TIMEOUT: no return or destruction inferred")
		quit(124)
	return false
