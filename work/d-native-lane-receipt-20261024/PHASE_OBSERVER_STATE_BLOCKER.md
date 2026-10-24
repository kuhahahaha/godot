# Phase observer state blocker — 2026-10-25

Audited the native phase observer's frame/after-views state transitions, capture-window lifetime, foreign-thread rejection, overflow, unfinished-frame poison, and scheduled capture result propagation.

The observer enforces paired phases, monotonic frame order, one owning thread, a bounded event window, and clears the thread-local capture window after service returns. Existing focused tests cover normal pairs, overflow, unfinished end, foreign thread, missed windows, and dummy-renderer `ERR_UNAVAILABLE` without fabricating success.

No safe standalone source patch is justified. Any change to phase pairing or poison timing would alter the producer timing contract and requires owner/runtime evidence.

Disposition: precise blocker/no-change receipt. Full build remains skipped; runtime/native/GPU/PCK/platform/production qualification remains false.
