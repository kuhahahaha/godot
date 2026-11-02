# Particle collision base-size guard fixture

`run.py` checks both GPU particle node setters and the RD/GLES3 storage setters. It then extracts the production storage setter bodies into a small C++ harness and verifies that valid non-negative sizes update the particle state while negative and non-finite values leave the previous size unchanged.

```sh
python3 tests/servers/rendering/fixtures/particle_collision_base_size/run.py
python3 tests/servers/rendering/fixtures/particle_collision_base_size/run.py --source-ref HEAD
```

The current source reports 6/6 harness and 4/4 source checks. `--source-ref HEAD` is expected to fail against the pre-fix baseline.
