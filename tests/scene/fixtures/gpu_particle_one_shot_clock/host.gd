extends Node
# Public scene lifecycle only: headless does not prove GPU simulation or pixels.
var cases: Array = []
var failures: Array = []
var checks := 0
var frame := 0

func _ready() -> void:
	process_priority = 1000 # Observe default-priority particle internal notifications first.
	for dimension in [2, 3]:
		for scenario in ["paused", "fractional", "fast", "changing", "restart", "canceled", "stopped"]:
			var p = GPUParticles2D.new() if dimension == 2 else GPUParticles3D.new()
			p.emitting = false
			p.one_shot = true
			p.lifetime = 0.25
			p.explosiveness = 0.0
			p.speed_scale = 0.0 if scenario == "paused" else (0.5 if scenario in ["fractional", "changing"] else 2.0)
			add_child(p)
			var state = {"node": p, "name": "%dD/%s" % [dimension, scenario], "scenario": scenario, "elapsed": 0.0, "signals": 0, "prior_signals": 0, "canceled": false, "stopped": false}
			p.finished.connect(func(): state.signals += 1)
			p.restart()
			cases.append(state)

func expect(ok: bool, label: String) -> void:
	checks += 1
	if not ok and failures.size() < 100:
		failures.append("frame %d %s" % [frame, label])

func _process(delta: float) -> void:
	frame += 1
	if frame == 1:
		# Nodes added in _ready may miss the first internal-process traversal.
		# Start a fresh shot once every node is already in the tree.
		for state in cases:
			state.node.restart()
		return
	for state in cases:
		var p = state.node
		state.elapsed += delta * p.speed_scale
		var should_emit: bool = not state.stopped and state.elapsed <= 0.25
		var completed: bool = state.elapsed > 0.5
		var expected_signals: int = state.prior_signals + (1 if completed and not state.canceled else 0)
		expect(p.emitting == should_emit, state.name + " emitting follows accumulated simulation delta")
		expect(state.signals == expected_signals, state.name + " finished count follows accumulated simulation delta")
		if state.scenario == "paused" and frame == 40:
			p.speed_scale = 1.0
		if state.scenario == "changing":
			if frame == 4:
				p.speed_scale = 0.0
			if frame == 24:
				p.speed_scale = 2.0
		if state.scenario == "restart" and frame == 40:
			state.prior_signals = state.signals
			state.elapsed = 0.0
			p.speed_scale = 0.5
			p.restart()
			expect(p.emitting, state.name + " restart reactivates emission")
		if state.scenario == "canceled" and frame == 2:
			p.emitting = true
			state.canceled = true
		if state.scenario == "stopped" and frame == 2:
			p.emitting = false
			state.stopped = true
	if frame == 100:
		for state in cases:
			state.node.free()
		print("GPU_ONESHOT_ENGINE=" + JSON.stringify({"failures": failures, "checks": checks, "frames": frame, "cases": cases.size()}))
		get_tree().quit(1 if not failures.is_empty() else 0)
