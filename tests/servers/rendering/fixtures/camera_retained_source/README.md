# Retained native camera source validation

The actual render-camera capture now supports an exact-current immutable retained payload. Ordinary capture and the existing copying getter allocate no retained object; the first successful retained borrow allocates one, repeated borrows reuse it. Caller references survive capture advance, epoch change, invalidation and viewport destruction. The payload preserves the original presentation key and full camera values.

Parent actual execution on the reviewed three-file delta: build exit 0 (37.44s); focused 4 cases / 191 assertions and camera-neighbor 17 cases / 519 assertions, all failures 0. Native Metal Mobile and Forward+ viewport regression exit 0 (7.04s), 22 lifecycle checks each and four exact RGBA comparisons, draw counts 2 -> 2. GPU regression exercises the existing real camera/render path; the new retained getter's lifecycle coverage is CPU, not a new GPU Sprite draw.

Source identity, final executable hash, original commands, logs, XML, independent source review and PNG evidence are archived here. Base HEAD was 6a3ee010ca with only the reviewed three-file camera source delta in the compiled product. CI portability edits began after this build. Original absolute paths in logs identify the execution; archived PNGs retain identical bytes.

This is a native source-lifetime primitive. It grants no external selected-view permission and settles no output budget. Accepted output, pre-list preparation, checked Sprite draw, native completion and no-later-draw closure remain separate work. Independent review records multiview and cross-viewport token-model limits.
