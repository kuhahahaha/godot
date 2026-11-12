# Actual finalize/LASTREF CPU lifecycle acceptance — 2026-11-07

This archive is separate from the standalone-D32 real-Metal archive. It demonstrates actual public `RenderingDevice::finalize()` refusals and actual heap-RD destructor LASTREF fail-stop using an original range reducer and a finite synthetic native source. It does not claim real Metal teardown or production Scene/public-ABI ownership completion.

`evidence/validation/` is the historical R1 run, NOT ACCEPTED pending the original P1: nonfatal child doctest checks could be hidden by `_Exit` and a permissive runner. The original review, original hashes, R2 fix review and final pins are preserved under `evidence/review/`. R2 uses fatal child callback invariants and a common GREEN/RED diagnostic rejection predicate.

`evidence/validation-r2/` preserves every original build/command/result receipt, child log, controlled mutation note, restored CPU XML and audit. GREEN busy exits 0 while retaining the unresolved owner; LASTREF exits 78 with the actual close-failure route. Removing only the finalizer height-return refusal guard reaches the dependent-free sentinel, exit 87. The controlled RED runner accepts that exact sentinel, while the normal runner rejects both RED modes. Source restoration is pinned to `69c3d7bf81ebf5ebd92692d1423805248b9971e9f829d1b19f2b22d633dd10fc`.

The restored host is `91eadbadd47d6c6c3227788da6cecc9cec4e7bc7a3a0f9cec90c80655b03c160`. Focused CPU: 61 cases / 6902 assertions / zero failures; full CPU: 2281 cases / 585821 assertions / zero failures / three skipped. The final formatted runner was rerun successfully; its exact busy/LASTREF logs are in `restored/formatted-runner/`.

`final_source/` contains the final CPU test, runner, README and production `rendering_device.cpp` only. RD.h is intentionally outside this snapshot. No engine binary is included. `FINAL_SOURCE_PINS.json` pins the four exact source byte sequences. Runtime results and build identities are preserved separately; source pins alone are not build provenance.

`ORIGINAL_FILE_MAP.json` lists original and stored byte lengths/SHA-256 for every copied artifact. C++, XML, logs, diff and draft source artifacts use deterministic gzip with unchanged decompressed bytes. `MANIFEST.json` pins all archive payload files. `ACCEPTANCE_RECEIPT.json` records the independently checked raw hashes, exits, XML totals and restoration evidence. No implementation or engine execution was performed by the archivist.

Limit retained from the reviewed design: because the Frame is idle after original source recycle, stable counters alone cannot distinguish a second empty flush from the intended phase skip. The test also checks the real phase flag and safe repeated refusal. Default full-suite execution performs no destructive child work unless the exact environment mode is set.
