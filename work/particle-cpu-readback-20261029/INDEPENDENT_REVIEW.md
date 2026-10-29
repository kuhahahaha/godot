# Independent review — particle CPU readback — 2026-10-29

## Review scope

Reviewed the storage interface, RD and GLES3 implementations, dummy fallback,
RenderingServer forwarding/binding, and the focused regression test.

## Findings

- **Accepted:** every concrete `RendererParticlesStorage` implementation is
  updated, so the new pure virtual does not leave an abstract renderer.
- **Accepted:** RD uses the actual `RID_Owner` lookup and validates the actual
  GPU buffer size before returning bytes; no fabricated owner or serial is
  introduced.
- **Accepted:** the synchronous readback is explicit in the API contract and
  follows the existing `particles_get_current_aabb` synchronization model.
- **Accepted:** GLES3 returns its real current instance buffer, while dummy
  storage is empty by design.
- **Follow-up:** the binary layout is backend-specific (RD process storage vs
  GLES3 packed instance storage). A future ANGE consumer must select the
  backend contract before decoding bytes.
- **External blocker:** no ANGE production particle loader/qualification caller
  exists in the current source tree, so this change cannot establish E09,
  completion, readback authority, or release semantics by itself.

## Review conclusion

**PASS — limited production CPU-visible particle readback.** The focused test,
Debug affected-object build, Release affected-object build, and UBSan object
check are sufficient for this scoped change. The full suite has unrelated
failures recorded in `RECEIPT.md`.

Current checkout recheck confirms the focused readback cases still pass 2/5.
