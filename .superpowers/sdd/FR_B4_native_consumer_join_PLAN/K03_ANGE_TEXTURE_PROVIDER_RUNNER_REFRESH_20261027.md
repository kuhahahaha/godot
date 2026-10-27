# ANGE texture provider runner refresh — 2026-10-27

Fixed the private E22 texture provider runner's stale assertion census. The runner now expects the current **5,242 assertions** and includes the `capture_shape_plan_and_ranges` section (12 assertions).

The real runner then passed baseline and restored-green runs, rejected all 13 semantic mutants, and preserved the 90 policy rows / 670 oracle vectors. The change is test-harness maintenance only; it does not add an owner, producer, native qualification, operation/completion/readback, Ready, Verified, or GPU authority.

Evidence: `K03_ANGE_TEXTURE_PROVIDER_RUNNER_REFRESH_20261027_REPORT.json` and `/tmp/k03-texture-provider-fixed-20261027/RUN.json`.
