# GPU one-shot scaled clock fixture

This fixture checks the scene-side `GPUParticles2D` and `GPUParticles3D` one-shot lifecycle clock against scaled elapsed simulation time. The source runner extracts the literal production notification, restart, and emitting bodies into a small C++ shell. It is deterministic source evidence and does not claim GPU draw or renderer execution.

The public host runner uses `--headless --fixed-fps 32` and requires exactly 14 cases, 2774 checks, and 100 frames. It exercises public SceneTree lifecycle state only. A current post-fix controlled binary should produce `engine_exit=0`; the fixed pre-change revision `0510e0c82d` should fail the same assertions. The runner records binary and fixture SHA-256 values and rejects empty/truncated result markers.

Commands from the repository root:

```sh
python3 tests/scene/fixtures/gpu_particle_one_shot_clock/run.py --source-ref 0510e0c82d --output work/gpu-one-shot-clock-20261111/red-current.json
python3 tests/scene/fixtures/gpu_particle_one_shot_clock/run.py --output work/gpu-one-shot-clock-20261111/green.json
python3 tests/scene/fixtures/gpu_particle_one_shot_clock/run_runner_control.py
python3 tests/scene/fixtures/gpu_particle_one_shot_clock/run_host.py --godot /path/to/godot.macos.template_debug.arm64 --output work/gpu-one-shot-clock-20261111/host.json
```
