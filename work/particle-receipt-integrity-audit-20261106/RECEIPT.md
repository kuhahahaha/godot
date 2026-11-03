# Particle receipt integrity audit

Date: 2026-11-06

A low-cost integrity audit covered all **14** `work/particle-*20261106/RECEIPT.md` files currently present.

## Checks

- Every receipt has a top-level title.
- Every receipt states its production-source/evidence boundary.
- Every receipt that declares a raw-log SHA-256 has a matching `run.log`, and the digest matches the file bytes.
- Result: **14/14** receipts checked, **0** integrity issues.

No production source changed. No build or GPU run was started; available disk was approximately 119 MiB during the audit.
