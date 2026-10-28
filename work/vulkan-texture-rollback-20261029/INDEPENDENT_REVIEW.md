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
