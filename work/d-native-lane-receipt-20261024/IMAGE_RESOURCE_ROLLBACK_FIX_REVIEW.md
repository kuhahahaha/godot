# Image resource rollback fix — 2026-10-24

Commit `137833abc2` closes the image RID on both post-creation failure paths in `SkinnedMeshCaptureTestRDPool::record_image()`:

- resource tracking failure;
- missing/invalid image write-census evidence after `copy_to_rect`.

The slot RID is reset after `free_rid`, preventing release from later treating a failed image as live and preventing a leaked texture on error. This is a local resource-ownership fix and does not require the full renderer migration or a full build.

Static source checks and `git diff --check` pass. Runtime/native/GPU/PCK/platform/production qualification remains false; full build remains blocked by `No space left on device`.
