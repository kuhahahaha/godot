# Particle collision uniform retry receipt

Date: 2026-11-02

`particle_collision_uniform_retry/run.py --source-ref HEAD` is the intentional RED baseline (exit 1: the collision rebuild rollback marker is absent). The current source fixture is GREEN at 7/7 checks. The RD fix marks descriptor-set rebuilds, and `fail_process` frees and clears only the current rebuilt collision uniform before restoring the cached collision tuple, forcing the next retry to rebuild instead of reusing a descriptor for a rejected tuple.
