# Native open-items revalidation R4 — 2026-10-29

This revalidation follows the E22 core texture snapshot export commit
`e7ed9f3669`. One previously open D/core seam is now closed: the real
`ange_texture_snapshot_plan_v1` and `ange_texture_snapshot_v1` exports are
registered and validated against retained CurveTexture/GradientTexture1D
objects. The exact source and binary identities are in `SOURCE_HASHES.txt`;
source census outputs are in `COMMAND_OUTPUT.json`.

The remaining native acceptance seams are still item-scoped:

1. The production ANGE tree still has no original owner caller of
   `require_main_rd_qualification()`; the default main-RD admission remains
   fail-closed.
2. `StageExecutor::start_native_qualification` and
   `ParticleProgramLoader::request_qualification` remain definitions/internal
   delegation without a normal production preparation/record actor issuing the
   complete sequence.
3. Graph handles, path lengths and command-buffer mapping are real and green,
   but the complete checked append → request-owned native issuer → completion
   settlement → same-request exact-range readback join remains unproved.
4. Post-SCENE SceneTree/Engine wrapper enrollment and exact final BINDING
   disposal remain required before G1.

The current bounded regressions are retained in the E20/E22 receipts and are
all green. No fallback, synthetic authority, operation/completion/readback
association, Ready promotion, or G1 relaxation was introduced. These four
seams constrain only their corresponding acceptance rows; independent queue
work continues.
