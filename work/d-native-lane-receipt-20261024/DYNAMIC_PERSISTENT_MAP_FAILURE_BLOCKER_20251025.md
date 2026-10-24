# Dynamic Persistent Map Failure Blocker

Date: 2026-10-25

`RenderingDevice::_buffer_update()` directly calls
`driver->buffer_persistent_map_advance()` in the dynamic-persistent path and
immediately writes through the returned pointer. Unlike `_buffer_initialize()`,
this path has no null check or error return, so a native map failure can turn
into an invalid memory write.

A minimal fix would validate the mapped pointer and return an error before
`memcpy`/`buffer_flush`. It was not committed because
`servers/rendering/rendering_device.cpp` is a mixed dirty migration file with
unrelated owner-unattributed changes; isolating this hunk would claim that
broader renderer slice.

No safe owner-attributed source mutation was made. Qualification remains
source/static only and all runtime/native/GPU/PCK/platform/performance/
production flags remain false.
