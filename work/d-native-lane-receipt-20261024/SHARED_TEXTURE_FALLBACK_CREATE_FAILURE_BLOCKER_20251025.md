# Shared Texture Fallback Create Failure Blocker

Date: 2026-10-25

`RenderingDevice::texture_create_shared()` and
`texture_create_shared_from_slice()` allocate a shared-fallback driver texture
and increment `texture_memory` before later validation of the copied texture's
primary `driver_id`. If that later validation fails, the function returns an
empty RID without freeing the fallback texture/tracker or rolling back the
memory accounting.

A minimal rollback would free the fallback driver texture, release its tracker,
and undo the accounting before returning. It was not committed because
`servers/rendering/rendering_device.cpp` is a mixed dirty migration file with
unrelated owner-unattributed changes.

No safe owner-attributed source mutation was made. Qualification remains
source/static only and all runtime/native/GPU/PCK/platform/performance/
production flags remain false.
