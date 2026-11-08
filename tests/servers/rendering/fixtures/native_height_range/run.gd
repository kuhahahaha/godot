extends SceneTree

var fixture: RefCounted
var case_index := 0
var started_msec := 0
var awaited_callback := 1
var ordinary_loop_started := false
var return_loop_disabled := false
const CHECKS := [
	"main_rd_rt",
	"loop_disabled_for_setup",
	"fixed_case",
	"quote_issued",
	"quote_observed",
	"quoted_depth_created",
	"framebuffer_created",
	"pair_captured",
	"initial_clear_recorded",
	"initial_clear_completed",
	"standalone_ready",
	"ranges_prepared",
	"ranges_recorded",
	"ranges_closed",
	"closed_not_complete",
	"pair_live_before_issue",
	"native_ranges_complete",
	"completion_is_not_physical_return",
	"correct_issue_path",
	"loop_disabled_for_return",
	"first_range_returned",
	"second_range_still_complete",
	"second_range_returned",
	"range_return_is_not_physical_return",
	"retire_framebuffer",
	"framebuffer_enqueued_not_returned",
	"framebuffer_returned_depth_live",
	"retire_depth",
	"depth_enqueued_not_returned",
	"both_physical_returned",
	"identities_preserved",
	"native_complete_after_return",
	"no_producer_claim",
	"cleanup_continuation_settled"
]

const WORKER_CHECKS := [
	"worker_vertex_buffer_created",
	"worker_transfer_pending",
	"worker_vertex_array_created",
	"worker_sink_recorded_0",
	"worker_first_bind_transfer_0",
	"worker_sink_closed_0",
	"worker_sink_recorded_1",
	"worker_first_bind_transfer_1",
	"worker_sink_closed_1",
	"worker_resources_live_after_native_complete",
	"worker_resources_live_before_retire",
	"worker_resources_enqueued",
	"worker_resources_returned",
	"worker_no_next_draw",
]

func _initialize() -> void:
	for argument in OS.get_cmdline_user_args():
		if argument == "--case=1":
			case_index = 1
		elif argument == "--case=2":
			case_index = 2
	if not ClassDB.class_exists("NativeHeightRangeTestFixture"):
		printerr("NATIVE_HEIGHT_RANGE_MISSING")
		quit(77)
		return
	fixture = ClassDB.instantiate("NativeHeightRangeTestFixture")
	started_msec = Time.get_ticks_msec()
	call_deferred("_start")

func _start() -> void:
	RenderingServer.set_render_loop_enabled(false)
	RenderingServer.call_on_render_thread(Callable(fixture, "begin").bind(case_index))

func _process(_delta: float) -> bool:
	if fixture == null:
		return false
	var report: Dictionary = fixture.call("result")
	if report.get("complete", false):
		RenderingServer.set_render_loop_enabled(false)
		var passed: bool = report.get("started", false) and report.get("error", -1) == OK
		passed = passed and report.get("failures", ["missing"]).is_empty()
		passed = passed and report.get("physical_prefix_returned", false) and not report.get("retained_on_failure", true)
		var expected_checks: Array = CHECKS.duplicate()
		if case_index == 2:
			expected_checks.append_array(WORKER_CHECKS)
		var seen: Dictionary = {}
		var rows: Array = report.get("checks", [])
		for row in rows:
			var label: String = row.get("check", "")
			passed = passed and expected_checks.has(label) and not seen.has(label) and row.get("ok", false)
			seen[label] = true
		passed = passed and rows.size() == expected_checks.size() and seen.size() == expected_checks.size()
		print("NATIVE_HEIGHT_RANGE_RESULT=", JSON.stringify(report))
		if passed:
			print("NATIVE_HEIGHT_RANGE_PASS case=", case_index)
		quit(0 if passed else 77 if report.get("error", -1) == ERR_UNAVAILABLE else 1)
		return false
	if Time.get_ticks_msec() - started_msec > 60000:
		RenderingServer.set_render_loop_enabled(false)
		printerr("NATIVE_HEIGHT_RANGE_TIMEOUT")
		quit(124)
		return false
	# Bounded case completes every step inside begin's single RT callback.
	# Never enqueue another callback while that callback still owns the range.
	if case_index != 1:
		return false
	# Only one callback can be in flight. All range state stays on the RT;
	# result() returns a mutex-protected deep snapshot of the same fixture Ref.
	if report.get("callbacks", 0) < awaited_callback:
		return false
	if case_index == 1 and report.get("phase", 0) == 1 and not ordinary_loop_started:
		ordinary_loop_started = true
		RenderingServer.set_render_loop_enabled(true)
	if report.get("phase", 0) == 2 and not return_loop_disabled:
		return_loop_disabled = true
		RenderingServer.set_render_loop_enabled(false)
	awaited_callback = int(report.get("callbacks", 0)) + 1
	RenderingServer.call_on_render_thread(Callable(fixture, "advance"))
	return false
