# Finalizer post-drain controls

The registered CPU case `post-drain finalization BUSY resumes without reissue` seeds one original RD range through the reducer, marks the original frame/worker drain phase complete, and exercises `_native_height_ranges_prepare_finalization()` with `native_call_release` returning `ERR_BUSY`. It asserts the range and driver remain owned, then retries after release becomes available and checks that graph/issue/poll counts do not increase. This is the safe in-process proof of the post-drain cursor.

A LAST_REF fail-stop must remain a separate process control because `require_close_success` calls `_Exit(78)`. The runner should invoke the focused test binary with an environment-selected child mode that constructs an RD with one unresolved retained range, calls `finalize()` until the post-drain range return refuses, and lets the final `RenderingDevice` destructor take the existing LAST_REF route. Parent assertions: exit status 78 and `GDEXTENSION_CLOSE_FAILED error=... route=0` on stderr; no driver-free event before termination. Do not run this child mode inside the doctest process.

The current source batch does not add that child process mode. Root may add it after the focused CPU build if the existing test harness supports environment-selected subprocess execution; production finalization remains unchanged.
