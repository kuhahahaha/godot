# Synchronous Readback Map Failure Blocker

Date: 2026-10-25

`RenderingDevice::buffer_get_data()` creates a temporary readback buffer and
returns immediately when `driver->buffer_map()` fails. The temporary driver
buffer is not freed on that error path, so repeated map failures can leak native
readback resources.

A minimal fix is to free `tmp_buffer` before returning the empty vector. It was
not committed because `servers/rendering/rendering_device.cpp` is a mixed dirty
migration file with extensive unrelated owner-unattributed changes; isolating
and committing this hunk would claim the broader renderer migration slice.

No safe owner-attributed patch was made. Qualification remains source/static
only; runtime/native/GPU/PCK/platform/performance/production flags remain false.
