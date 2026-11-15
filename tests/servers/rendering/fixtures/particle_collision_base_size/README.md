# Particle collision base-size guard fixture

`run.py` checks both GPU particle node setters and the RD/GLES3 storage setters. It then extracts the production storage setter bodies into a small C++ harness and verifies that valid non-negative sizes update the particle state while negative and non-finite values leave the previous size unchanged.

```sh
python3 tests/servers/rendering/fixtures/particle_collision_base_size/run.py
python3 tests/servers/rendering/fixtures/particle_collision_base_size/run.py --source-ref HEAD
```

The current source reports 7/7 harness and 4/4 source checks. The harness also verifies that an external output owner prevents a valid mutable collision-size update. Its owner pointer is only a test substitute for this guard; it does not exercise an actual external output lifecycle or GPU execution.

`--source-ref` checks only source guards at that revision; it does not run the C++ harness. Use an explicit pre-fix revision for a historical negative control; `HEAD` is the current revision.
