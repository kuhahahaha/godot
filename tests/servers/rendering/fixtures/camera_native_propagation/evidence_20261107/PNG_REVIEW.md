# Independent offline camera PNG and RED/GREEN review

Status: **PASS**. No Godot process, GPU work, build, production-source edit, or
original-evidence edit was performed by this review. The checked evidence root
is `work/sprite-camera-native-propagation-20261107/`.

Reproduce from the repository root using only the Python standard library:

```sh
python3 work/sprite-camera-native-propagation-png-review-20261107/verify_offline.py \
  --evidence work/sprite-camera-native-propagation-20261107 \
  --output work/sprite-camera-native-propagation-png-review-20261107/RESULT.json
```

The standalone script does not import the GDScript fixture or its Python
validator. It verifies PNG chunk CRCs, zlib completion, 8-bit noninterlaced
RGB/RGBA structure, and reconstructs PNG filters 0 through 4. RGB is expanded to
opaque RGBA. Synthetic known-pixel RGB and RGBA self-checks exercise every filter
and verify that deliberate CRC corruption is rejected. All actual evidence PNGs
are color type 6 and use filters 1, 2 and 4.

Twenty frozen images were independently decoded: six camera GREEN, six camera
RED and eight ordinary viewport images (four before/after comparisons). Every
PNG is 128x128, every raw image is 65,536 RGBA bytes, every computed raw SHA256
matches its original GDScript report, every nonuniform count matches the report,
and every PNG CRC validates. The result records each source PNG hash as well as
its independently reconstructed raw RGBA hash. Complete hashes of reviewed logs
and process result JSONs are also recorded.

## Camera GREEN

Both reported processes exited zero, the runner exited zero in 2.11 seconds,
and the real startup lines identify Metal 4.0 / Apple M3 Pro (Apple9), once for
Forward Mobile and once for Forward+. Both reports contain exactly the expected
24 checks, all strictly true, stage 4 and the explicit source-only authority
boundary. No ERROR/WARNING diagnostics or timeout marker occurs.

Each backend's unregistered image equals its first registered image byte for
byte. Camera movement changes its second image bytes and hash. All six images
report one actual draw call. Independent nonuniform counts are 988 for each
first/unregistered image and 1014 for each second image.

| Backend | First raw RGBA SHA256 | Second raw RGBA SHA256 |
| --- | --- | --- |
| Mobile | `50d9dafec320766ca41e7d461948892b96c7bf3b4e6fd95991523ae30c7be7b8` | `79a633382babdbe5914e2dddec0dd13ba202fec7ca2c4f4bad7a1a2d30df88c8` |
| Forward+ | `d8db115b2dac0a38871bbdd239e22ecd4471fac8e906f3cdf597b8ea0ca8f6f3` | `4259de89740dc63a29fa8b384ffefeb06a25c2c434cfc4e5724e3319a5a82e4a` |

The printed observations also independently agree on payload/source viewport
RID, view_id 0x43414d, epoch 1, presentation sequence 1 then 2, increasing capture
ordinal, different capture object IDs, expected camera transforms, and changed
camera transform between samples. Native old-Ref lifetime is represented by the
RT checks; a PNG cannot itself establish that lifetime.

## Camera RED sensitivity

Both backend processes exited one, the runner exited one in 4.52 seconds, and
both still emitted the expected Metal device line, complete stage-4 report,
one-draw images, nonuniform output and the same moved-camera image change.
There are no ERROR/WARNING diagnostics or timeout markers.

The exact false checks in each RED report are:

- `production_cache_nonnull_1`
- `production_cache_nonnull_2`
- `old_ref_survives_invalidate`
- `old_ref_survives_free`

All nine other reported checks remain strictly true: actual viewport identity,
registration and cleanup still complete. Checks requiring access to a captured
sample are absent, as expected when production never materializes that sample.
The two cache failures are the direct target; old-Ref survival failures are the
downstream effect. Every one of the six RED raw images equals its corresponding
GREEN image byte for byte. This review therefore verifies target-specific
observation failure with otherwise normal rendering, rather than accepting an
arbitrary nonzero exit as mutation detection. The parent owns the build/source
mutation provenance recorded elsewhere in the evidence root.

## Ordinary viewport image control

`FINAL_VIEWPORT_GPU.log` and its process result pass the same diagnostics,
real-Metal-backend and successful-exit checks. Both reports have their recorded
checks true, and retain the explicit `external_scene_connection=not-present`
and `pair_membership=not-observed` boundaries.

For Mobile and Forward+, top and side before/after image pairs are independently
byte-equal (four comparisons across eight PNGs). Each image has 1300 nonuniform
pixels and two reported draw calls. Each backend's four images happen to share
one hash; this is recorded as an image-invariance control, not proof that top and
side are distinguishable scenes:

- Mobile: `3235d4d3f8e63c27cd916647c8a0b2cb29eef7ab92813e73daaa068b936b2a21`
- Forward+: `ff6c135a03a63519d78a8d98187c3c6c91478ba31f8cc3413a6fea642ed734fc`

This is frozen-evidence verification. It adds independent PNG/JSON consistency
and target-failure checks, but does not rerun all Clustered pass variants,
authenticate a binary's source contents, or expand external consumer authority.
