# Independent review — K03 withdraw/retire audit (2026-10-27)

## Verdict

**Limited-scope accept; no P0–P2 findings.** The evidence supports the stated
local lifecycle ordering and fail-closed guards. It does not establish public
`Released`, physical/native disposal, Ready, Verified, packet, or GPU behavior.

## Verification

- Raw log hashes match JSON exactly: Native
  `e7b46639b7c386373be11236492b5dee9314048d6f04cf23580b31999fb79071` and
  External `44e858ba85997578ee362255c89fe4f86f32677f914ba6673f8c4c374c54b90f`.
  Doctest output matches the recorded rc/case/assertion counts: 2/166 and
  3/45, all passing.
- All five source hashes reproduce from source commit
  `1753faf66685154b1ffe2482bfe367c6039d4216` via `git show`.
- Source inspection confirms `particles_free()` marks free requested and
  withdraws the native owner; finalization retires completed uses, requires
  `can_destroy()`, then performs authenticated Registry release before clearing
  the external pointer, destroying storage, and freeing the RID.
- `release_native_owner()` returns `ERR_BUSY` until `can_destroy()` succeeds;
  `can_destroy()` requires withdrawn state, no native effects, empty slots and
  empty NativeUse cells. The destructor asserts prior authenticated release.
- NativeUse retirement remains predicate-driven; pending/unknown uses retain
  the owner. Public polling exposes only `Created`/`Withdrawing`, so metadata
  withdrawal or Registry release cannot be presented as public `Released` or
  physical disposal.
- Editor/DSO hashes are consistent across the package and receipt. No matching
  live binaries are present in this worktree, so those two values are
  receipt-level provenance rather than fresh binary rehashes here.

## Scope boundary

The all-false claim matrix is appropriate for U08, E09, operation, completion,
readback, packet, physical disposal, Released, Ready, Verified and GPU. The
focused tests demonstrate lifecycle refusal and internal guard coverage only;
they do not prove a production external producer or physical GPU disposal.

No source changes were made and no tests were rerun.
