extends SceneTree

var fixture: RefCounted
var case_index := 0
var started_msec := 0
const CHECKS := [
	"main_rd_rt",
	"disabled_outside_lists",
	"fixed_case",
	"quote_zero_extent_rejected",
	"quote_over_limit_rejected",
	"quote_issued",
	"quote_observed",
	"quote_descriptor_policy",
	"quote_native_bytes",
	"quote_scope_unknown",
	"quoted_create_verified",
	"actual_depth_within_quote",
	"framebuffer_created",
	"pair_captured",
	"live_original_children",
	"shared_attachment_identity",
	"unknown_overhead_preserved",
	"depth_clear_recorded",
	"original_driver_framebuffer_materialized",
	"retire_framebuffer",
	"framebuffer_enqueued_not_returned",
	"only_framebuffer_driver_returned",
	"half_returned_original_identities",
	"retire_depth",
	"depth_enqueued_not_returned",
	"both_driver_returned",
	"returned_original_identities_bytes",
	"repeat_retire",
	"no_next_draw"
]

func _initialize() -> void:
	for argument in OS.get_cmdline_user_args():
		if argument == "--case=1":
			case_index = 1
	if not ClassDB.class_exists("NativeDepthAllocationTestFixture"):
		printerr("NATIVE_DEPTH_ALLOCATION_MISSING")
		quit(77)
		return
	fixture = ClassDB.instantiate("NativeDepthAllocationTestFixture")
	started_msec = Time.get_ticks_msec()
	call_deferred("_start")

func _start() -> void:
	RenderingServer.set_render_loop_enabled(false)
	RenderingServer.call_on_render_thread(Callable(fixture, "run").bind(case_index))

func _process(_delta: float) -> bool:
	if fixture == null:
		return false
	var report: Dictionary = fixture.call("result")
	if report.get("complete", false):
		var passed: bool = report.get("started", false) and report.get("error", -1) == OK
		passed = passed and report.get("failures", ["missing"]).is_empty()
		passed = passed and report.get("physical_prefix_returned", false) and not report.get("retained_on_failure", true)
		var seen: Dictionary = {}
		var rows: Array = report.get("checks", [])
		for row in rows:
			var label: String = row.get("check", "")
			passed = passed and CHECKS.has(label) and not seen.has(label) and row.get("ok", false)
			seen[label] = true
		passed = passed and rows.size() == CHECKS.size() and seen.size() == CHECKS.size()
		print("NATIVE_DEPTH_ALLOCATION_RESULT=", JSON.stringify(report))
		if passed:
			print("NATIVE_DEPTH_ALLOCATION_PASS case=", case_index)
		quit(0 if passed else 77 if report.get("error", -1) == ERR_UNAVAILABLE else 1)
	elif Time.get_ticks_msec() - started_msec > 90000:
		printerr("NATIVE_DEPTH_ALLOCATION_TIMEOUT")
		quit(124)
	return false
