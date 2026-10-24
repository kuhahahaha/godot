# Status payload duplication blocker — 2026-10-24

Audited native-session `status()` payload duplication and empty/error result propagation.

The method copies the published snapshot under mutex and duplicates all three byte payloads (`snapshot`, `consumer`, `image`) before returning. Empty payloads remain empty; error/canceled/completed fields are copied together from the same immutable publication snapshot. Existing tests cover status before/after close, retained callback holders, late duplicate image, and no-native-work retirement.

No safe standalone patch is justified. Adding live-job reads or synthesizing payload errors in `status()` would violate the publication boundary and could pair bytes with stale status fields.

Disposition: precise blocker/no-change receipt. Full build remains skipped; runtime/native/GPU/PCK/platform/production qualification remains false.
