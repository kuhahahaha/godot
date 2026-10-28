# Vulkan texture allocation rollback — 2026-10-29

This D/native slice repairs a real partial-allocation leak in
`RenderingDeviceDriverVulkan::texture_create` when extra GPU memory tracking
uses the manual `vkCreateImage` → `vmaAllocateMemoryForImage` →
`vmaBindImageMemory2` path. Allocation and bind failures now destroy the
original image and free any acquired VMA allocation before returning. Image
view failure uses the same rollback path.

## TDD and validation

- RED runtime test rc1 reproduced the missing rollback: both injected stages
  returned a non-null texture, failed the rollback-mask assertions, increased
  VMA allocation count, and terminated with VMA's live-allocation assertion.
- GREEN Debug focused test: 1 case / 8 assertions, rc0.
- Existing Vulkan allocation regression: 2 cases / 16 assertions, rc0.
- K03/P2 regression: 22 cases / 350 assertions, rc0.
- macOS arm64 `template_release` build and `--version`: rc0.
- Actual `use_ubsan=yes` build and focused test: rc0, 1 case / 8 assertions.

The rollback mask proves stage 1 releases the image and stage 2 releases both
image and memory; VMA allocation count returns to its pre-call value. No
fallback texture, synthetic owner, qualification authority, or GPU/Ready claim
is introduced. This is native allocation cleanup evidence only. The initial recorded binaries carry the pre-commit build identity `72aa6a2073`; the image-view follow-up Debug/UBSan artifacts were rebuilt from the committed `e322d87abe` base plus the pinned follow-up source hashes. Exact source and artifact hashes are pinned separately in `SOURCE_MANIFEST.json`.

## Image-view failure coverage

A follow-up fault stage injects `vkCreateImageView` failure after the image and
allocation have both been acquired. The same rollback mask and allocation
count checks pass: Debug focused is 1 case / 11 assertions, Vulkan allocation
regression is 2 cases / 19 assertions, and actual `use_ubsan=yes` focused is 1
case / 11 assertions. The test fixture also supplies the real view format, so
UBSan does not depend on an out-of-range default format.

## Direct-driver view-format validation

A RED direct-driver test passed the default `DATA_FORMAT_MAX` view into
`texture_create`; the old path published a texture, increased the VMA allocation
count, and aborted at teardown (rc134). The driver now rejects invalid formats
before any Vulkan/VMA effect, and applies the same guard to shared-view entry
points. GREEN Debug is 1 case / 4 assertions, the Vulkan allocation regression is
3 cases / 23 assertions, and actual `use_ubsan=yes` reports the same 1/4 and 3/23
counts, all rc0. The template_release rebuild and version probe are also rc0.
The R6 binaries carry the pre-commit `7ee45a66b` build identity; source hashes
are pinned to the committed follow-up `65a70669e5` in `SOURCE_MANIFEST.json`.
This is a direct-driver input validation repair only; no qualification, owner,
completion, readback, Ready or G1 claim is made.

## Direct-driver format query validation

A RED query test passed DATA_FORMAT_MAX to the Vulkan texture-usage and
sampler-filter capability methods. The usage method returned a nonzero bitfield
without rejecting the input; the filter method had no explicit guard. The
follow-up now rejects invalid formats in both queries and in
texture_can_make_shared_with_format, initializing the out parameter before
the fail-closed return. Debug focused is 1 case / 6 assertions, the Vulkan
allocation regression is 4 cases / 29 assertions, K03/P2 remains 22 cases / 350
assertions, and actual use_ubsan=yes repeats the 1/6 and 4/29 results, all
rc0. The template_release rebuild/version probe is rc0. R7 artifacts carry
the 11bdfc52b build identity; source hashes pin the follow-up source commit
separately in SOURCE_MANIFEST.json. This is direct input validation only; no
qualification, completion, readback, Ready or G1 claim is made.

## Extension texture format validation

The same direct-driver fixture also exercises texture creation from an external
native image. Before the guard, passing DATA_FORMAT_MAX reached the Vulkan
format table and crashed with SIGSEGV (rc134). The extension entry now rejects
the format before any image-view operation. The focused Debug result is 1 case
/ 7 assertions, Vulkan allocation regression is 4 cases / 30 assertions,
K03/P2 remains 22 cases / 350 assertions, and actual use_ubsan=yes repeats
1/7 and 4/30, all rc0. The template_release rebuild/version probe is rc0.
R8 artifacts carry the 7a1cb83fa build identity; source hashes pin the
follow-up source commit in SOURCE_MANIFEST.json. This remains bounded direct
input validation; no owner, qualification, completion, readback, Ready or G1
claim is made.

## Direct-driver vertex format validation

A RED direct-driver test passed the default DATA_FORMAT_MAX vertex attribute
into vertex_format_create. The old path returned a format and reported live
PagedAllocator pages at exit. The driver now preflights every vertex attribute
format before allocating VertexFormatInfo or indexing RD_TO_VK_FORMAT. GREEN
Debug is 1 case / 3 assertions, the Vulkan allocation regression is 5 cases /
33 assertions, K03/P2 remains 22 cases / 350 assertions, and actual
use_ubsan=yes repeats 1/3 and 5/33, all rc0. The template_release rebuild and
version probe are rc0. R9 artifacts carry the 9a61e804e build identity; source
hashes pin the follow-up source commit in SOURCE_MANIFEST.json. This remains
bounded direct input validation only; no qualification, completion, readback,
Ready or G1 claim is made.

## Direct buffer and shareable-format validation

A RED direct-driver buffer test passed DATA_FORMAT_MAX with an empty BufferID;
the old path dereferenced the buffer and SIGSEGVed. A second RED test supplied
an invalid shareable format to texture_create; the old path published a
texture and aborted on VMA teardown. The driver now rejects the buffer format
before dereference and preflights every shareable format before image
allocation. Debug focused is 1/3 for each new case, Vulkan allocation
regression is 7 cases / 39 assertions, K03/P2 remains 22/350, and actual
use_ubsan=yes repeats 1/3, 1/3 and 7/39, all rc0. The template_release
rebuild/version probe is rc0. R10 artifacts carry the d08b51635c build
identity; source hashes pin the follow-up source commit in
SOURCE_MANIFEST.json. This is direct input validation only; no qualification,
completion, readback, Ready or G1 claim is made.

## Direct-driver base texture format validation

A RED direct-driver test passed DATA_FORMAT_MAX as the base TextureFormat.
The old path reached MoltenVK with invalid pixelFormat 0 and aborted. The
driver now rejects the base format before shareable-list handling or image
allocation. GREEN Debug is 1 case / 3 assertions, Vulkan allocation
regression is 8 cases / 42 assertions, K03/P2 remains 22/350, and actual
use_ubsan=yes repeats 1/3 and 8/42, all rc0. The template_release rebuild and
version probe are rc0. R11 artifacts carry the 9ae7de8310 build identity;
source hashes pin the follow-up source commit in SOURCE_MANIFEST.json. This
is direct input validation only; no qualification, completion, readback,
Ready or G1 claim is made.

## Direct render-pass attachment format validation

`render_pass_create()` now rejects `DATA_FORMAT_MAX` for every attachment before
indexing `RD_TO_VK_FORMAT` or constructing the native attachment descriptions.
The pre-guard baseline probe returned a null handle (1 case / 3 assertions), so
this defensive slice has no RED crash to claim; the final focused test verifies
the fail-closed result at the driver boundary. Debug focused is 1 case / 3
assertions, Vulkan allocation regression is 9 cases / 45 assertions, K03/P2 is
22 cases / 350 assertions, and actual `use_ubsan=yes` repeats 1/3 and 9/45, all
rc0. The template_release rebuild and version probe are rc0. R12 artifacts
carry the `5f082531b7` build identity; source hashes pin the exact source in
`SOURCE_MANIFEST.json`. This is direct input validation only; no qualification,
completion, readback, Ready or G1 claim is made.

## Direct render-pass attachment layout validation

`render_pass_create()` now rejects `TEXTURE_LAYOUT_MAX` before attachment
initial/final layout table lookup, subpass input/color/resolve reference
conversion, depth-stencil reference conversion, or fragment-density map layout
lookup. Debug focused is 1 case / 3 assertions, Vulkan allocation regression is
10 cases / 48 assertions, K03/P2 is 22 cases / 350 assertions, and actual
`use_ubsan=yes` repeats 1/3 and 10/48, all rc0. The template_release rebuild
and version probe are rc0. R13 artifacts carry the `c4da5dc43` build identity;
source hashes pin the exact source in `SOURCE_MANIFEST.json`. This remains
direct input validation only; no qualification, completion, readback, Ready or G1
claim is made.

## Direct sample-count and primitive validation

The Vulkan driver now rejects `TEXTURE_SAMPLES_MAX` before sample-count table
lookup in texture creation and render-pass attachments, and rejects
`RENDER_PRIMITIVE_MAX` plus invalid pipeline sample counts before pipeline state
translation. Focused Debug coverage is 1/4 for texture samples, 1/3 for render
pass samples, and 1/4 for pipeline primitive/sample inputs. The full Vulkan
regression is 13 cases / 59 assertions, K03/P2 is 22 cases / 350 assertions,
and actual `use_ubsan=yes` Vulkan regression is 13/59; all are rc0. The
template_release rebuild and version probe are rc0. R14 artifacts carry the
`b484c5cac` build identity; source hashes pin the exact source in
`SOURCE_MANIFEST.json`. This remains direct input validation only; no
qualification, completion, readback, Ready or G1 claim is made.

## Direct texture type validation

`texture_create()` now rejects `TEXTURE_TYPE_MAX` before the image-type table
lookup or any native image allocation. Debug focused is 1 case / 4 assertions,
Vulkan allocation regression is 14 cases / 63 assertions, K03/P2 is 22 cases /
350 assertions, and actual `use_ubsan=yes` Vulkan regression is 14/63; all rc0.
The template_release rebuild and version probe are rc0. R15 artifacts carry the
`f5e874900` build identity; source hashes pin the exact source in
`SOURCE_MANIFEST.json`. This remains direct input validation only; no
qualification, completion, readback, Ready or G1 claim is made.
