# Scene985a LTC authenticated texture worker join — green native cohort

Applied only the fixed four-file `LTC_TEXTURE_WORKER_JOIN_985a.patch` (SHA256 `aa1d909361a849da3c30992baaddc37a96c811ba0df434c5c4699a8aafc32c57`) to exact receiver `985a08eb56ded83ea5fc17ac84db5e7cd0df6f54`. ZIP bytes/hash/CRC, all four preimages, four candidates and all15 Scene producer closure receiver hashes were independently verified. The existing UniformSet raw transfer cursor correction remains in place; no old patch was stacked.

The original immutable sampled textures now expose finite raw upload tuples at checked UniformSet bind. The original worker checker captures the actual pending receipt under its existing locks; sink enrollment authenticates the exact raw tuple/count. A later checked draw may use a still-pending texture only when the current active, open same-frame/epoch range already contains that exact enrolled receipt and original worker dependency. Public noncapture calls and existing lifecycle/source/return authority retain their original paths. No prewarm, fake completion, dropped guard or test assertion change.

## Local acceptance on the unchanged real Metal profile

Host SHA256 `d82c3f64d957066c18d7869ac6658b3a0dec4431f9b4a131ca25a5007b8705ea`:

- Original normal candidate: Mobile + Forward+,2/2 pass.
- Original real MultiMesh refusal, same-session ordinary recovery and retained physical returns: Mobile + Forward+,2/2 pass.
- Original source selection8/8 and legacy neighbors12/12 pass. Thus all original24 GPU cases pass.
- CPU NativeHeightRange22 cases/865 assertions, nativepass9/691 and CheckedBind5/411 pass.
- Additional real native worker1/48 checks and clear2/34+34 checks pass.

These are local serial executions on the exact same host; the original Python/GDScript assertion bodies were unchanged. No Forward+-only continuation was needed because both original two-renderer runners passed. Before this patch, receiver985a had the four original candidate/refusal recovery cases red at the real pending LTC texture premise; its historical receipt remains pinned to that receiver.

First -j4 incremental build encountered `No space left on device`. That builder terminated; after available space recovered, same-profile -j2 retry succeeded. This is recorded separately from source correctness. External source-stage manifest flags and D CPU receipts are historical input, not local GPU qualification.

Independent reviewer `native_renderer_merge_review` found no blocking source defect in the exact four-file patch, preserved offset or15-file closure. Source/host identities were checked again after the serial cohort. Full command/source/host hashes and counts are in `SCENELTC985A_GREEN_VALIDATION.json`. Stable native branch selection and integration remain with the parent task; this receipt alone does not choose a branch.
