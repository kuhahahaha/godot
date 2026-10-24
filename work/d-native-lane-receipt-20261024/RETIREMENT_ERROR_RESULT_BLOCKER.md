# Retirement error result propagation blocker — 2026-10-24

Audited native-session pool release and physical-retirement failure paths after the publication error fix.

No additional source patch is justified:

- `pool.release()` failures are retained through `fail()` and keep `released=false`, allowing a later maintenance pass to retry retirement.
- `physical_retirement_status()` failures other than `ERR_BUSY` are retained through the same path; `ERR_BUSY` remains a retry state.
- `cleanup_failure` is published separately from the first session failure through `published.cleanup_error`.
- Marking terminal on a failed release would be incorrect because resources may still be live.

Disposition: precise blocker/no-change receipt. Full build remains skipped; runtime/native/GPU/PCK/platform/production qualification remains false.
