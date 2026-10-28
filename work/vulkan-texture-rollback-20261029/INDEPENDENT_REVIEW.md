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
