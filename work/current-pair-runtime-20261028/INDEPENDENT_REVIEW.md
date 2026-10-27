# Independent review — current pair runtime probe (2026-10-28)

## Scope

This review covers only the fixed runtime probe in `RECEIPT.json`, using the Godot dev editor binary and the current A Debug DSO named there. It reviews process return codes, extension initialization, world admission, invalid input rejection, unload, and crash/close diagnostics.

## Findings

- `runtime-vfx`: `rc=0`; `U01_EXTENSION=0`, `U01_ENTER_WORLD=true`, `U01_UNLOAD=0`.
- `runtime-no-vfx`: `rc=0`; `U01_EXTENSION=0`, `U01_ENTER_WORLD=true`, `U01_UNLOAD=0`.
- `runtime-invalid-vfx`: `rc=1`; `U01_EXTENSION=0`, `VFX-SYN-001 unknown kind: invalid_ingestion_control`, `U01_ENTER_WORLD=false`, `U01_UNLOAD=0`.
- No `SIGSEGV`, `CrashHandler`, or leaked-at-exit marker occurred in the three logs.
- The current VFX fixture reaches world admission but does not exercise an authenticated particle execution request, GPU/native qualification, completion issuer, same-request readback, visible sprite output, PCK export, or Ready.

## Decision

**Accepted as a current-pair runtime-boundary receipt.** It supersedes the stale binary identity for this narrow probe only. It does not supersede the known negative U02 owner/qualification evidence and does not close U01, E06, R01/R02, K01, GPU, readback, PCK, or Ready.

## Provenance limitation

Both source worktrees are mixed/dirty. The DSO's linked source commit is not inferred from either current HEAD; the host and DSO SHA-256 values in `RECEIPT.json` and `SOURCE_MANIFEST.json` are the authoritative pair identity.
