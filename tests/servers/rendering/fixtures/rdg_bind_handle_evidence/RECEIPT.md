# Original RDG VA/IA bind record handles

Both original bind methods now return the exact existing kind3 graph/list/instruction-offset identity and register that original offset. contains_record recognizes both real bind instruction kinds. Original payload, byte offsets, copied arrays, stage flags and driver call ordering are unchanged; ordinary callers may discard the return.

macOS arm64 tests-enabled editor build exit0. Graph9cases282assertions passed; Graph/RecordSource/D32/NativeDisposal neighbors38cases4135assertions passed. Effective negative removing only the two contains-record instruction whitelist entries failed the new bind fixture30assertions while original payload still executed; restored, rebuilt and neighbors passed. Independent review read exactly the final source hashes and found no actionable issue.

Test covers sparse3-buffer VA with64-bit offsets, original dynamic mask, IA16/32 formats, mutation of caller arrays after append, captured vs discarded return payload equivalence, open/closed-list Pending then actual graph.end command-buffer mapping, invalid graph/frame/list/offset and stale-frame refusal.

This is original recording identity, not a source permit or native submission/completion proof. Checked VA/IA admission, transfer/tracker prefix, deferred descriptors, geometry draw profile, HeightFrame custody and Scene phase/source integration remain separate required work. This does not complete HEIGHT_RASTER.
