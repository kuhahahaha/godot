# GLES3 heightfield sizing and cache invalidation receipt

Date: 2026-11-02

The source fixture is RED at pre-change `HEAD` (missing degenerate extent fallback) and GREEN on current source (8/8). The staged patch clamps the thin framebuffer axis to at least one texel, uses a square positive resolution for degenerate or non-finite extents, and retires the cached texture and framebuffer when a heightfield's X/Z extent changes. The cache release covers both GLES3 utility accounting and GL objects.

This is source-level contract evidence. A live Compatibility renderer fixture has not yet exercised allocation, resize and reallocation. No GPU runtime qualification is claimed.
