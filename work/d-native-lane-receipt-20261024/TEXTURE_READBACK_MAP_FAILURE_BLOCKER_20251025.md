# Texture Readback Map Failure Blocker

Date: 2026-10-25

`RenderingDevice::texture_get_data()` allocates a temporary CPU buffer for
non-CPU-readable textures. If `driver->buffer_map(tmp_buffer)` returns null,
the function returns immediately without unmapping/freeing the temporary
buffer. This is a native resource leak on the texture readback failure path.

The minimal fix is to free `tmp_buffer` before returning the empty vector. It
was not committed because `servers/rendering/rendering_device.cpp` remains a
mixed dirty migration file with unrelated owner-unattributed changes.

No safe owner-attributed source mutation was made. Qualification remains
source/static only and all runtime/native/GPU/PCK/platform/performance/
production flags remain false.
