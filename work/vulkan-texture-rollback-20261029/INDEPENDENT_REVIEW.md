# Independent review — Vulkan texture allocation rollback

The patch follows the existing manual buffer rollback pattern. The guard owns
only the original image/allocation created by this call, is disarmed by normal
publication, and is used for allocation, bind and image-view failures. The
fault-injection test observes real Vulkan/VMA allocation counts and requires
actual rollback masks; it does not fabricate a driver success or texture ID.

Debug, Release and actual `use_ubsan=yes` evidence is retained with exact
source and binary hashes. The change is independent of the E20 owner,
qualification, completion, readback and Ready/G1 seams.

Disposition: no P1/P2 finding in this bounded cleanup slice.

The follow-up image-view injection is reproducible after real format setup and
confirms the post-allocation rollback path; no pre-existing buffer rollback
case is broadened or altered.


The follow-up rejects `DATA_FORMAT_MAX` before indexing `RD_TO_VK_FORMAT` in
texture creation and both shared-view entry points. The RED test demonstrates
that the prior behavior could publish a texture and leak an allocation; the
GREEN Debug/UBSan and Release evidence shows the guard is fail-closed and does
not broaden the allocation rollback or lifecycle authority surface.


The query follow-up stays bounded to Vulkan format inputs. Invalid formats are
rejected before RD_TO_VK_FORMAT access, and the shared-format output is
initialized before returning false. RED/Debug/UBSan/Release evidence supports
fail-closed behavior without adding fallback or authority.


The extension guard is the same fail-closed format-input policy as the texture
and query entry points. The RED SIGSEGV is eliminated without creating a
synthetic image/view or changing ownership and lifecycle behavior.


The vertex-format guard executes before resource allocation, so invalid input
cannot publish a native vertex format or leave allocator pages alive. RED,
Debug, actual UBSan and Release evidence stays within the Vulkan driver input
boundary and does not broaden lifecycle authority.


The two guards execute before native buffer dereference or image allocation.
RED/Debug/UBSan/Release evidence is bounded to input rejection and allocation
safety; no owner or lifecycle authority is introduced.


The base-format guard runs before mutable-format list processing and native
image creation, eliminating the MoltenVK invalid-pixel-format path. Evidence
is limited to Vulkan input/resource safety and preserves all lifecycle gates.


The render-pass guard executes before any attachment format table lookup or
native render-pass creation. The baseline and final focused probes both return a
null handle; the final source path guarantees the invalid sentinel is rejected
without relying on Vulkan/MoltenVK behavior. Evidence remains limited to direct
input validation and does not introduce lifecycle or qualification authority.


The layout guards run before any `RD_TO_VK_LAYOUT` indexing in the render-pass
attachment/reference paths exercised by this entry. Valid layout behavior is
unchanged; invalid sentinels fail closed with an empty RenderPassID. Evidence is
limited to input validation and does not add lifecycle or qualification authority.


The sample-count and primitive guards run before RD sample/primitive table
access and before any pipeline or image allocation. Valid values retain the
existing closest-supported-sample behavior; sentinel inputs fail closed. The
evidence adds no owner, completion, readback, Ready or G1 authority.


The texture-type guard runs before `RD_TEX_TYPE_TO_VK_IMG_TYPE` indexing and
native image setup. Valid texture types retain existing behavior; the sentinel
fails closed without allocator growth. This adds no lifecycle or qualification
authority.


Each command guard executes before its first layout-table access and before
null resource handles are dereferenced. Valid layouts and command behavior are
unchanged; invalid sentinels return without issuing Vulkan work. This adds no
lifecycle, owner, completion, readback, Ready or G1 authority.


The view-count guard runs before the multiview bit shift, so zero and oversized
counts cannot enter undefined arithmetic or Vulkan creation. Valid counts retain
existing behavior and this evidence adds no lifecycle authority.
