# Synchronous immutable callback lock boundaries

RD releases its actual recursive free guards before synchronous invalidation callbacks and resolves RID state again after callbacks return. Dependency traversal releases both original guards around child frees. The original outer free frame remains live through synchronous retries; existing retained child/capture references protect cleanup. Callback timing and physical return semantics remain synchronous. Existing test cleanup calls now pass real mutex guards.

Actual RED: the original lock implementation failed all three callback mutex probes in the held/native-owner test. Fixed runtime: 3/3 MaterialNativeHold cases, 871 assertions, exit 0 and empty stdout/stderr. Native observe 20/20, disposal 7/7, record 9/9, ResourceUse 2/2, and exact qualification neighbors 128/128 also passed. Qualification negative tests emitted 776 bytes of expected diagnostics. The combined build passed with frozen inputs unchanged; two mechanical check passes passed.

DELIVERY.json records staged source hashes and the one separately uncommitted metadata admission gate excluded from this delivery. The tested host also contains other Core metadata and pipeline changes. Independent source review found no additional issue in these lock boundaries.

The callback probes establish the actual RD mutex boundary on CPU. At the initial local run, platform CI and TSAN were pending; the follow-up below supplies actual TSAN evidence. The initial request fixture failure was outside this lock delivery. Live GPU qualification remains unproven.

Follow-up CI on 8dbf961e9b: the actual ThreadSanitizer job and Unit tests step passed, with 1972 cases, 501915 assertions and no ThreadSanitizer warnings in the downloaded raw job log. CI_TSAN.json records its hash and URL. This validates the pushed lock delivery; the full matrix has a separate Mono editor failure under owner triage, and this evidence does not cover later uncommitted Core changes.

Current Core follow-up on a638a36536: the actual Linux ThreadSanitizer job and Unit tests step passed with 1980 cases, 516718 assertions, three skipped cases and zero reported ThreadSanitizer warnings. The runner reported 3331 matches of its existing suppressions. This covers the delivered cursor, metadata and installed physical port changes; it does not reuse the older narrow result as their evidence. CI_TSAN.json retains both runs and raw-log hashes. The other platform jobs are still pending, and this is no managed Mono, P2 candidate or live GPU acceptance.
