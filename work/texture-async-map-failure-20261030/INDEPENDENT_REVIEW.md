# Independent review — async texture readback map failure

Read-only review covered `texture_get_data_async()`, frame texture readback flush, async buffer readback error handling, staging cleanup, and the current pair.

1. A null texture staging map now terminates that request before any pointer arithmetic or memcpy.
2. Previously mapped regions are already unmapped; the failing null map requires no unmap. Frame staging vectors are cleared normally after the request batch.
3. Success callbacks remain success-only; failure cannot expose partial or fabricated bytes. This mirrors async buffer readback, which records an error and withholds callback delivery.
4. RenderingDevice/Qualification/K03/R01 regressions pass. Current Metal pair stops at E05 before readback admission, so no positive native result is claimed.

Residual coverage: a dedicated fake driver test that forces `buffer_map()` null during texture flush would directly assert no callback; the source guard is now explicit and the existing frame/readback failure suites remain green.
