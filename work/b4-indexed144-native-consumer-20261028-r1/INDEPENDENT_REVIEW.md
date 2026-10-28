# Independent whole-module review and evidence audit

Reviewer: /root/indexed144_native_review, fresh context, read-only source and saved evidence. No build/GPU/SDK/rerun/edits.

Initial P1: first Indexed144 enable_group could consume dirty shader versions while updating only the newly enabled group, leaving ordinary materials stale. Initial P2: ordinary vertex/index region updates mutated immutable borrowed allocations. Parent reproduced both on actual Metal. Followup reviewed original lazy dirty-version preservation and allfour external-write rejection guards; both resolved, no remaining concrete P1/P2 in inspected changes/neighbors.

Final audit independently inspected commands/return codes/full renderer+native logs/helper/script and recomputed all21 sizes/SHA256 against disk. Both Metal Mobile/Forward+ show edited native ordinary outputs2/3, original vertex/index unchanged, correct bounds+format, deformation rejection,1976 covered RGB pixels with exact0diff/max_error0, load/unload/process0. Empty background cannot pass the alpha-coverage gate. Final native26/555 rc0; foreign-camera errors are negative history arms, renderer errors intended write/deformation refusals. No final dependency-map teardown errors. Pins before/after identical, canonical882d retained.

Source scope: direct current/previous decoder/handedness; ordinary variant indices; actual draw/precompile/reflection profile; VA input-mask/generations/point identity+replacement bindings; external current shadow identity; deformation rejection; AABB/dependency; borrowed clear. Tests additionally cover format and undersized-backing VA failure and metadata teardown. Current-only/previous-only cache cases change respective generation; buffer replacement changes both together. No cache realloc fault injection.

This supports tested private native seam, not production B4 caller/E09, complete runtime motion/history/shadow/mirrored-host/CUSTOM1–3/concurrency, E05 physical retirement, U08 or canonical integration. Generic cleanup/unload remains request evidence. E06 stamp outside review.
