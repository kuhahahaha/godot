# D32 checked frame bridge evidence — 2026-11-06

The current Godot fork already exposes a fixed, consumable qualification path for a D32 depth producer through the generated `device_session_qualification_record_prepared_v4` entry. Its `GDExtensionQualificationRecordV4` carries the original V3 record plus `GDExtensionQualificationDepthPassV1`; bounded `GDExtensionQualificationDepthDrawV1` descriptors and push bytes are validated before native effects. The implementation in `gdextension_qualification_ticket.cpp` authenticates the same original ticket, operation identity, source/session/generation/digest, raster child, target object serial, content revision, shader/pipeline/framebuffer witnesses, and reverse uses. It records real depth-only begin/clear, pipeline bind, scissor, draw and end operations, then keeps the ticket's existing completion/readback/physical-retirement path.

This is the fixed API currently consumable by the qualification owner:

- `device_session_qualification_record_prepared_v4`
- `GDExtensionQualificationRecordV4`
- `GDExtensionQualificationDepthPassV1`
- `GDExtensionQualificationDepthDrawV1`
- `GDExtensionQualificationReportV3`

Validation run on the current editor:

- `--test-case='*[D32]*,*[Qualification]*depth*'`: 9 cases, 539 assertions, 0 failures.
- `--test-case='*[Qualification][Typed]*'`: 6 cases, 7,589 assertions, 0 failures.
- Full regression after the prepared/withdraw commits: 2,147 cases, 548,716 assertions, 0 failures, 3 skipped (see `/tmp/b4-prepared-withdraw-full.log`).

The focused checked-depth fixture verifies actual RD graph records for depth clear, pipeline bind, scissor, direct draw and end, and refusal of an invalid suffix without a new effect. The qualification ticket path separately verifies same-ticket source binding, bounded descriptors, native completion/readback and physical retirement invariants.

Remaining owner boundary: this API is qualification/native substrate, not the Scene HeightCaptureCheckedSink or production Sprite/LE192 receiver. The current contracts still require a Scene-owned producer caller that supplies the original operation/source/Host identity and a fixed owner choice for the checked frame/result. No new ABI, registry, scheduler, synthetic frame, or RID-as-authority was added here; those upstream joins remain the next integration seam.
