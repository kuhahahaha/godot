# GPU particle sub-emitter post-enter-tree receipt

Date: 2026-11-02

`GPUParticles2D/3D` retry sub-emitter binding during `NOTIFICATION_POST_ENTER_TREE`, after descendants have entered the scene tree. `_attach_sub_emitter()` now sends an empty RID when the target is missing, self-referential, or of the wrong particle type, clearing a stale backend link. The source fixture is RED at pre-change `HEAD` (0/4) and GREEN on current source (4/4); it verifies source structure only. No runtime sub-emitter lifecycle test or rebuilt binary is claimed for this isolated commit.

The staged source patch contains only the two attach functions and two post-enter cases. Existing `NOTIFICATION_EXIT_TREE` clearing remains in place.
