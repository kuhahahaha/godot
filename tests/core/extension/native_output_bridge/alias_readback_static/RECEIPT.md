# Output alias readback static plan

Original freeze now admits a valid prepared output-alias readback plan after checking exact producer role/generation/extent, whole producer WRITE use, original source preparation, copy action/readback slot and READ window. An explicit READBACK runtime ERR_UNAVAILABLE gate remains before capture/staging/native effects for output aliases. Ordinary non-alias readback and KnownPrefix cancellation are unchanged. This batch does not provide operational alias readback.

The new original accepted fixture uses actual storage-create and prepare ports. It verifies exact real source identity, backing/generation/access mismatches, missing live alias witness/root, ten invalid static mutations and one valid freeze without graph/native/readback issue or physical custody. All temporary mutation fields are restored. Source storage returns through its original cancel/retire/observe/release path.

Build: `scons -Q platform=macos target=editor tests=yes debug_symbols=no module_particles_runtime_tests_enabled=yes -j2`.

RED before production changes:1case,225 assertions,9 target assertion failures,exit1. GREEN:1case,225 assertions,exit0,no diagnostics. Combined:216cases,91988assertions,exit0,with exactly the existing three intentional pipeline allocation failures,three foreign-camera refusals and one deliberate invalid UTF-8 diagnostic. Final binary156994696bytes,SHA256 `35dc9524bafcfc8d5bf4f7ee1da32826eceb98b729c9aee6a1863c6d630aa0ae`. Full commands/logs and source hashes are adjacent.

First test candidate compile failed by accessing private NativeDisposalWitness.cell. Replaced with original RD witness observation refusing ERR_UNAUTHORIZED and unchanged EMPTY report; production access was not widened. A premature run of the old binary matched zero cases and is excluded from all acceptance claims; the saved RED-r2 has the actual newly built case. Initial compile log is retained separately from intended RED.

Limits: CPU/dummy static-plan evidence only. No BOUND/witness/readiness state is injected. Actual internal acquire/factory alias, runtime preflight, staging copy/delivery/native completion and real GPU readback remain pending. Public Instance192 query/Create remain gated. Runtime gate source ordering is independently reviewed but not exercised via a lawful bound alias in this batch. The entire long goal remains active.
