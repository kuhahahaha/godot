# d23 native ticket and prepared-upload controls — 2026-10-30

The editor built from source `d23e0b5486ff398dc49ef6b24e213730d350a798`, SHA-256 `58d849014e17937a6e3f50664cc3ff0b3f5d21a1623c5a41a7f25fe79f280b7f`, ran two existing controlled D native fixtures on non-headless Metal 4.0 Forward Mobile, safe render thread, Apple M3 Pro. The original editor identifies its build version as `bcd34ebcc` because of reused generated version metadata; the source/build relationship and binary identity are documented separately in `../rd-retirement-cell-scope-20261030/RECEIPT.md`.

- `qrt_ticket`: runner rc 0, all recorded checks true; same-ticket native completion, 16 exact readback bytes `7,8,9,10`, native physical free and acknowledgement observed.
- `qrt_upload`: runner rc 0, all recorded checks true; same-ticket prepare, segmented upload and prepared record operations (kinds 7/8/9), settled prefix, physical free and acknowledgement observed. The deliberate rejected upload returns native error 30 before the two accepted segments. This upload check is distinct from the ticket exact-readback check.

Each run's nine input hashes matched before and after. Commands, hash maps, structured checks and complete logs are retained alongside this receipt. These are controlled native fixture checks using a real backend; they do not prove that an ANGE product request has authenticated association, completion or exact readback, and do not establish Ready or G1.
