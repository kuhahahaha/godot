# Independent review — current pair performance baseline

Read-only review checked every timing command, rc file, hash pin, test summary, and runtime log.

1. Source and PCK runs are intentionally negative E05 controls; both reach Metal startup and fail before world/resource admission.
2. Import and pack return rc0. Pack emits only the documented x86_64 GDExtension warning; arm64 resolves the explicit DSO.
3. Retirement/readback selectors are measured independently and green; no timing is promoted to GPU completion or visible-output evidence.
4. RSS/peak-footprint values come directly from `/usr/bin/time -l`; no synthetic object/resource counts are reported because the CLI provides none.
5. Baseline is tied to editor, DSO, PCK hashes and should be regenerated after any source/build/pair change.
