# Original generation surface sources

This component extends the existing ExternalOutputOwner and its quoted metadata slab. Each already flattened GenerationMaterialSelection row now retains its original mesh RID/native surface ordinal and a source-only NativeSurfaceHold alongside the independently retained material selection/generation. The original metadata layout uses the actual expanded row size, including the original NativeImmutableHold's self-owned three-RID inline storage; no extra surface directory or public API is introduced.

`MeshStorage::mesh_surface_source_hold_acquire` retains only the existing vertex/attribute/index backings, using a finite limit equal to its actual roots (at most three). Its roots are a stack array, and the original native hold uses inline storage for limits up to three. The complete dependency closure is validated before any hold count changes; a fourth dependency is refused without falling back to unquoted allocation. Larger original holds retain their prior Vector path. It does not create a shader input-mask Version, VA, pipeline or draw. Source-only holds remain owned by the generation; exact later native preparation derives its own hold from the retained original Surface. Internal Create invokes this component after CPU selection acquisition. A failed later source retains its successful prefix, withdraws admission and returns through the original bounded generation OBSERVE service. That service drops source holds before CPU material/mesh selections and metadata/root return. Dropping a borrowed source hold is not physical GPU completion or foreign-resource free.

The existing slot preparation rows associate the exact generation source/material generation. Forward Mobile and Forward Clustered use that association for Instance192 and compare primitive, surface format, AABB and UV scale before deriving native geometry. Indexed144 keeps its original slot-owned source and derive path. Surface return checks the original owner's finite cell and corresponding source; slot retirement precedes generation source return, so the preparation pointer cannot outlive the original slab.

## Inline-storage accounting correction

A follow-up audit found that the earlier expanded row quote included only the NativeImmutableHold Vector wrapper, while its RID data and the temporary LocalVector roots allocated outside the slab. The correction embeds three RID values and a count in the same original holder; move, release and original physical capture use one private resource span and clear both storage modes. No caller-provided backing pointer can outlive its row. This changes only internal C++ layout, with the original actual `sizeof` quote and validation recalculated together; the public GDExtension ABI is unchanged.

The new NativeImmutableInline regression covers three-resource dependency closure, inline move construction/assignment/self-move, fourth-dependency refusal with zero counters, original retiring release, and the preserved larger Vector move/return path. The existing actual FIFO production capture now captures its pipeline/shader through a limit-three inline hold while keeping the qualified limit-eight Vector capture. GenerationSelectionLayout checks the exact single-row quote and native inline storage size. These follow-up changes await the root coordinator's unified rebuild and execution; the earlier results below precede this accounting correction.

## Earlier verification

The root coordinator ran the combined final source build with:

```sh
scons -Q tests=yes platform=macos target=editor debug_symbols=no module_particles_runtime_tests_enabled=yes -j2
```

Final build: exit 0, 30.29 seconds. Root then ran the rebuilt host with the focused selectors:

```sh
bin/godot.macos.editor.arm64 --headless --test \
  --test-case='*[GenerationSurfaceSource]*,*[GenerationSelection]*,*[NativeSurfaceHold]*,*[CameraCapture]*'
```

Result: 13/13 cases, 2,061/2,061 assertions, exit 0. This is combined-source evidence including the separately owned camera component, not thirteen source-component cases. The two new GenerationSurfaceSource cases exercise unequal original mesh surface flattening (1 + 2), material-generation/bounds identity, no Version creation during source acquisition, actual later mask-1 derivation and release, invalid callback context without effects, duplicate acquisition refusal, and later-source failure with exact-prefix bounded return. Both production consumers use the metadata predicate whose primitive/format/AABB/UV refusal controls run for each tested entry.

Initial fixture execution failed because its default storage-only driver branch was used for actual mesh vertex buffers and because mesh cleanup followed fixture device teardown. The corrected fixture uses the existing finite mesh-directory buffer tracker and disposes ordinary MeshStorage-owned buffers/Versions before device teardown. Original storage assertions remain intact. No failed run is described as passing.

`git diff --check` passes. Independent source review is in [GENERATION_SURFACE_SOURCE_REVIEW.md](GENERATION_SURFACE_SOURCE_REVIEW.md).

## Scope

This delivers original source retention and its exact pipeline-preparation derive association. The original public surface/view/resolve gates remain closed. Actual source-to-ExternalSurfaceSelection production acquisition, an admitted positive external pipeline preparation, visible Sprite draw, every-pass use/adoption and complete native retirement still require the continuing original production caller package. The focused tests use the existing finite CPU driver; they do not establish new Metal/GPU execution or full P4 acceptance. No new ABI, RenderingServer call, registry, selected-view identity or grant was added.

Integrated inline-holder validation: 6 selected NativeImmutableInline/GenerationSelectionLayout/GenerationSurfaceSource/NativeSurfaceHold cases, 1,320 assertions, exit 0. OutputObserveV3 separately passed 1 case/198 assertions. CPU evidence only; actual metadata sizeof includes inline RID storage. See INTEGRATED_VALIDATION.json.
