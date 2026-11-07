# Variable particle delta GPU float boundary

Implementation: `c11002b55e3bee21774634a855bad61a79a96c0c`. A 2-second variable-FPS frame at speed_scale=FLT_MAX produces a finite double that overflows the float GPU frame delta. The update now rejects before _particles_process and queues retry. Two clear states preserve process state/remainder; ordinary recovery and exactly FLT_MAX delta remain accepted.

Actual extracted-production harness RED: 54/56 in both default and REAL_T_IS_DOUBLE modes, exit 1, only the two intended rejection checks fail. GREEN: 56/56 in both modes, exit 0. The same final fixture against `--source-ref 4a1ff33a5c` reproduces RED (54/56 in both, exit 1). Primary rerun also 56/56 twice. Existing process stub and extraction were unchanged. Independent spec/quality review PASS. Logs: `work/particle-boundary-next-20261107/`.

The implementation entered the module-on editor built during close-contention validation; combined engine tests: 116 cases, 62109 assertions, exit 0. Editor SHA256 `524742cc065423a4090d78f2196cd83bc9057788db3bb5ee54629d690657597b`. The overflow regression itself is an extracted production update/control-flow harness; no live GPU frame upload or rendered output is certified. This does not promise rollback of independently successful preprocessing before the variable-FPS branch.
