# Frame poll native-code propagation review

Reviewer `/root/frame_poll_error_review` found and root fixed one correctness issue: qualification native-call poll writes its raw code to the ticket, then `_stall_for_frame` returns `FAILED`; an unconditional frame-wait copy previously overwrote it with zero. The final implementation carries a `frame_wait_native_code_valid` marker. Native/fence poll errors populate the internal code; qualification poll errors preserve their already-published report code; generic non-BUSY frame errors use a valid zero code. The C ABI, physical return, G1 predicates and blocking wait path remain unchanged.

The reviewer found no lifetime, cleanup, concurrency, physical or G1 issue. The regression now exercises both direct fence poll raw `-13` and qualification native-call poll raw `-7`. Read-only review; no reviewer edits/builds/runs.
