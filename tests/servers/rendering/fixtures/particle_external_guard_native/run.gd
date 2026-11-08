extends SceneTree

var fixture: RefCounted
var started_msec := 0
const TIMEOUT_MSEC := 120000
const MODES := ["INIT", "VALIDATE", "FINALIZE", "INIT_EMPTY", "FINALIZE_EMPTY"]

func _initialize() -> void:
	if not ClassDB.class_exists("ParticlesExternalGuardTestFixture"):
		printerr("PARTICLE_EXTERNAL_GUARD_NATIVE_MISSING tests-only fixture class not registered")
		quit(77)
		return
	fixture = ClassDB.instantiate("ParticlesExternalGuardTestFixture")
	if fixture == null:
		printerr("PARTICLE_EXTERNAL_GUARD_NATIVE_CREATE_FAILED")
		quit(1)
		return
	started_msec = Time.get_ticks_msec()
	call_deferred("_start")

func _start() -> void:
	RenderingServer.call_on_render_thread(Callable(fixture, "run"))

func _expected_checks() -> Dictionary:
	var expected: Dictionary = {}
	expected["material_features.original_owner"] = true
	for material_case in ["plain", "vertex_inverse", "fragment_inverse", "alpha_hash", "alpha_scissor", "normal_only", "both"]:
		for suffix in ["unissued_unchanged", "selection", "compiled_fact", "generation", "original_version", "inverse_requirement", "release", "released_refused", "empty_selection", "empty_invalidates"]:
			expected["material_features." + material_case + "." + suffix] = true
	for label in ["original main RD render-thread owner", "outside all active command lists", "first.prepare", "repeat.prepare", "reprepare.prepare"]:
		expected[label] = true
	for phase in ["first", "reprepare"]:
		for suffix in ["ready", "five_distinct_shaders", "five_distinct_pipelines"]:
			expected[phase + "." + suffix] = true
		for mode in MODES:
			for suffix in ["actual_shader", "actual_pipeline", "original_shader_association", "reflected_compute_layout", "reflected_source_roles"]:
				expected[phase + "." + mode + "." + suffix] = true
	for mode in MODES:
		expected["repeat." + mode + ".same_original_rids"] = true
	for phase in ["first_release", "final_release"]:
		expected[phase + ".owner_release"] = true
		expected[phase + ".not_ready"] = true
		for mode in MODES:
			for suffix in ["getters_empty", "shader_owner_removed", "pipeline_owner_removed"]:
				expected[phase + "." + mode + "." + suffix] = true
	expected["engine.actual_prepared_owner"] = true
	for mode in MODES:
		for suffix in ["actual_pipeline", "original_shader_association", "reflected_compute_layout", "reflected_source_roles", "distinct_engine_variant", "survives_local_release"]:
			expected["engine." + mode + "." + suffix] = true
	return expected

func _checks_complete(checks: Variant) -> bool:
	if typeof(checks) != TYPE_ARRAY:
		return false
	var expected: Dictionary = _expected_checks()
	if checks.size() != expected.size() or expected.size() != 202:
		return false
	var seen: Dictionary = {}
	for row in checks:
		if typeof(row) != TYPE_DICTIONARY or typeof(row.get("check")) != TYPE_STRING or typeof(row.get("ok")) != TYPE_BOOL:
			return false
		var label: String = row["check"]
		if not expected.has(label) or seen.has(label) or not row["ok"]:
			return false
		seen[label] = true
	return seen.size() == expected.size()

func _process(_delta: float) -> bool:
	if fixture == null:
		return false
	var report: Dictionary = fixture.call("result")
	if report.get("complete", false):
		var passed: bool = report.get("started", false) and report.get("error", -1) == OK and report.get("mode_count", 0) == 5
		passed = passed and report.get("failures", ["missing"]).is_empty() and _checks_complete(report.get("checks"))
		passed = passed and report.get("dispatch_recorded", true) == false and report.get("source_authority_supplied", true) == false and report.get("physical_return_claimed", true) == false
		print("PARTICLE_EXTERNAL_GUARD_NATIVE_RESULT=", JSON.stringify(report))
		if passed:
			print("PARTICLE_EXTERNAL_GUARD_NATIVE_PASS five_modes=1 checks=202 idempotent=1 owner_released=1 reprepare=1 physical_return_claimed=0")
		quit(0 if passed else 1)
	elif Time.get_ticks_msec() - started_msec > TIMEOUT_MSEC:
		printerr("PARTICLE_EXTERNAL_GUARD_NATIVE_TIMEOUT: callback completion not observed; no success/physical return inferred")
		quit(124)
	return false
