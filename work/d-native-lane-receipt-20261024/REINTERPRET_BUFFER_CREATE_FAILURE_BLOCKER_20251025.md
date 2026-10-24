# Reinterpret Buffer Create Failure Blocker

Date: 2026-10-25

`RenderingDevice::_texture_create_reinterpret_buffer()` stores the result of
`driver->buffer_create()` without checking for failure, immediately increases
`buffer_memory`, and creates a resource tracker with the returned buffer ID.
If native buffer creation fails, the shared fallback retains an invalid buffer,
its memory accounting is overstated, and later paths may use the invalid
tracker.

A minimal fix would fail closed on a null buffer ID and only update accounting
and tracker state after successful creation. It was not committed because
`servers/rendering/rendering_device.cpp` is a mixed dirty migration file with
unrelated owner-unattributed changes.

No safe owner-attributed source mutation was made. Qualification remains
source/static only and all runtime/native/GPU/PCK/platform/performance/
production flags remain false.
