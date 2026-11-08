# Scene worker source integration — candidate only

Applied the nine-file `scene44ea-all.patch` from D source commit `6a94a851d38735187002c29b2a7c0fff893b3cd2` onto Scene `44ea079b80ab58c5e41d90fe53cd00dbb7e9a4c6`. All nine package after-images match byte-for-byte. No particles storage or candidate assertion overlay was used. Source and host hashes, build arguments, exact commands and results are in `SCENE6A94_WORKER_VALIDATION.json`.

## Local Scene correction

A real MultiMesh refusal run exposed `bind_uniform_set` leaving its zero-transfer interval at offset zero after VA/IA had already appended two transfers. Original enrollment compared offset 0 against capture cursor 2 and correctly marked the range uncaptured. The only Scene correction records the actual current raw transfer cursor before the original uniform bind. Original authenticated receipt matching, capacity-five postbind reservation refusal, source retention and retirement authority remain unchanged.

The original refusal case is the regression: before the correction it missed the intended RD instance-profile refusal and poisoned frame issue; afterward both renderers reach the original scoped `p_instances != 1` refusal, unwind and attempt the ordinary recovery producer without a sticky frame issue. The recovery still fails the independent texture premise below, so this is not a green full-case claim.

## Actual acceptance

Same real Metal Apple M5 profile: NativeHeightRange 22 cases / 865 assertions; nativepass 9 / 691; source selection 8 GPU cases; legacy neighbors 12 GPU cases; new real worker 1 / 48 checks; standalone native clear 2 / 34 checks each all pass. Original normal candidate and geometry-refusal recovery are red on Mobile and Forward+ (four cases). All original 24 GPU cases were attempted. The original two-renderer runner exits after its first red; a private copy changes only its renderer loop to Forward+ to execute the remaining original assertion body.

The first unexpected failure is the checked draw texture guard at RD 7844. Finite diagnostic Mobile replay established set 0, binding 15 is freshly created LTC LUT1, RID 3766686318630, transfer worker 0 / operation 77; binding 16 LUT2 is operation 78. The original dual worker locks report processed 71, submitted 71, counter 78, recording true. Texture exists, has a driver, no qualification return and no owner; this is an actual pending upload rather than stale worker metadata. Scene adds no prewarm, ignores no guard and invents no completion. D owns the required authenticated texture transfer join.

Temporary diagnostic changes have been removed. Independent source review by `native_renderer_merge_review` found no integration blocker in the exact nine files or the one-line Scene cursor correction. The remaining four red original cases prohibit stable-mainline integration. This commit is a source-pinned candidate for the next D texture-join lane.

## Evidence disposition

`/tmp/scene-worker-join-6a94-20261109` retains source/package verification, diagnostic first causes, build and serial-cohort receipts for the parent/native join lane. Large repeated failure streams were hashed and replaced with their first-cause excerpts; `failure-stream-cleanup.json` records byte counts and hashes. Native `bin/obj` is retained for the next incremental build; no builder or GPU runner remains.
