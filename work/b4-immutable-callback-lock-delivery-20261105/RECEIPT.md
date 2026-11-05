# Synchronous immutable callback lock boundaries

RD releases its actual recursive free guards before synchronous invalidation callbacks and resolves RID state again after callbacks return. Dependency traversal releases both original guards around child frees. The original outer free frame remains live through synchronous retries; existing retained child/capture references protect cleanup. Callback timing and physical return semantics remain synchronous. Existing test cleanup calls now pass real mutex guards.

Actual RED: the original lock implementation failed all three callback mutex probes in the held/native-owner test. Fixed runtime: 3/3 MaterialNativeHold cases, 871 assertions, exit 0 and empty stdout/stderr. Native observe 20/20, disposal 7/7, record 9/9, ResourceUse 2/2, and exact qualification neighbors 128/128 also passed. Qualification negative tests emitted 776 bytes of expected diagnostics. The combined build passed with frozen inputs unchanged; two mechanical check passes passed.

DELIVERY.json records staged source hashes and the one separately uncommitted metadata admission gate excluded from this delivery. The tested host also contains other Core metadata and pipeline changes. Independent source review found no additional issue in these lock boundaries.

The callback probes establish the actual RD mutex boundary on CPU. Actual platform CI and TSAN must still run; this receipt does not close the previous CI TSAN failure or establish live GPU qualification. A separate request fixture release-sequence failure remains outside this delivery.
