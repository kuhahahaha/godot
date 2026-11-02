# GLES3 particle readback layout and empty-buffer receipt

Date: 2026-11-02

`particles_get_current_aabb()` now selects the 2D or 3D instance stride and parses the matching transform layout. Both AABB and current-data readback return an empty value before a GL read when the selected buffer is unallocated. This guards initial allocation and failed-allocation states and avoids reading 2D instances as 3D data.

The focused source fixture was RED at pre-change `HEAD` (missing AABB empty-buffer guard) and GREEN on current source (7/7, including 2D and 3D layout checks). `git diff --cached --check` passed. A live Compatibility renderer readback fixture has not run; source-level evidence alone does not qualify GPU behavior.
