# ANGE current provider oracle focused regression — 2026-10-27

The current pure provider line is green after mutation checks: curve provider baseline/restored **2,108 assertions** with 11 mutants rejected; mesh source baseline/restored **555 assertions** with 14 mutants rejected and the E23 neighbor at 1,953 assertions; texture provider baseline/restored **5,242 assertions** with 13 mutants rejected. Provider-oracle portability also passed its isolated unittest.

The texture runner has a maintenance mismatch: it still expects 5,230 assertions and omits the new `capture_shape_plan_and_ranges` section (12 assertions). A temporary copy with the expected total/census corrected ran the full texture suite green; the tracked/untracked runner was not edited.

This package is CPU/pure semantic evidence only. It does not establish an owner, public producer, capture transaction, native qualification, operation/completion/readback, Ready, Verified, or GPU authority.

Evidence: `K03_ANGE_CURRENT_PROVIDER_ORACLE_FOCUSED_20261027_REPORT.json` and the three raw reports under `/tmp/`.
