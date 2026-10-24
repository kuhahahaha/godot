# Resource observe-free propagation blocker — 2026-10-25

`RenderCaptureTestResourceRegistry::observe_free()` fails closed on duplicate/stale frees, but the four production call sites in `RenderingDevice::_free_pending_resources()` ignore its boolean result for compute pipelines, uniform sets, shaders, and textures.

A duplicate or stale free therefore remains invisible to `capture_test_coverage_failed`, even though the registry correctly rejects it. Propagating the result into the coverage failure flag is the likely minimal fix.

This source file is part of the mixed, owner-unattributed renderer migration currently dirty in the shared tree. Applying a local patch would cross that lineage and risk staging unrelated changes. Precise blocker recorded; no source mutation made.

Full build remains skipped; runtime/native/GPU/PCK/platform/production qualification remains false.
