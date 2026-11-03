# Particle receipt integrity audit

Date: 2026-11-06

A low-cost integrity audit covers all **16** `work/particle-*20261106/RECEIPT.md` files currently present.

## Checks

- Every receipt has a top-level title: **16/16**.
- Every receipt states a live-GPU/evidence boundary: **16/16**.
- Every receipt has a result, evidence, or findings section: **14/16**; the two aggregate audit/index-style receipts intentionally use their own structured headings.
- Every receipt that declares a raw-log SHA-256 has a matching `run.log`, and the digest matches the file bytes.
- Result: **16/16** receipts checked, **0** integrity issues.

No production source changed. No build or live GPU run was started during this audit; available disk was approximately 252 MiB.
