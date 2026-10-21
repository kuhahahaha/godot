# Native capture interface — raw observation integration candidate

This is D's current native raw operation/transport boundary, not a complete P01
witness schema or runtime qualification. A owns launch, receiver, original bytes,
process/descendant outcomes and all verdicts. The existing E34 receiver can decode
this envelope. Do not infer a fixture action or GPU completion from publication
order. No synthetic mode or script evidence injection is exposed.

On POSIX, provide exactly one two-token occurrence of each option before `--`:

- `--p01-capture-fd <inherited anonymous pipe write fd >= 3>`
- `--p01-run-id <parent fixed run identity>`
- `--p01-child-id <parent fixed child identity>`
- `--p01-manifest-sha256 <64 lowercase hex characters>`

Identity strings use ASCII letters, digits, `_`, `-`, `.`, `:` and at most 256
bytes. The native startup reads ordinary engine args once at SCENE registration.
It accepts a write-only pipe, sets close-on-exec and nonblocking mode, and owns
that descriptor after successful validation. It never opens a filename or socket.
The start `source_build_identity` is the actual executable file SHA-256, computed
by the engine. A must pin the same binary bytes before launch. Source identity
also requires the matching frozen source manifest; binary hash alone is not a
source coverage proof.

The start declares `stream_ids=["publication"]`. Envelope keys and canonical
unsigned decimal strings match the existing A JSONL receiver. Events contain
`origin=engine`; their payload is described by `native_hook_payload.schema.json`.
All integer payloads are rendered directly from native integers, without a
Variant/JSON floating-point conversion. Stream sequence starts at zero; original
hook sequence must be contiguous from one. Hook enum/name mapping is fixed in
`p01_capture_codec.cpp`: buffer_write, readback_queue, readback_complete,
buffer_allocate, buffer_free, buffer_clear, buffer_copy, writable_use,
buffer_dispose, skeleton_setter, skeleton_upload, skeleton_version,
skeleton_signal_before, skeleton_signal_after, skeleton_palette_request,
draw_record and driver_result. Unknown kinds poison encoding.

Current live publication uses `action_ref=unassociated`: this is an explicit gap,
not an invented action edge. The full fixture must associate original actions,
main-to-RT handoffs, exact requests, viewport/selected roles and callback receipts
before these records can project into the inherited specialized witness schema.
The live shutdown emits no seal. EOF without seal is rejected by A. The private
codec's seal tests validate encoding only; they do not activate runtime authority.

The codec supports bounded raw64 snapshot/consumer blobs (at most 36), with all
bytes and independently computed SHA-256 retained. Image artifact transport and
full typed witness projections remain unimplemented. The writer queue has 128
frames, each <=64 KiB; metadata codec limit is 16 MiB. Queue overflow, failed or
short write, malformed frame, unknown hook or sequence gap invalidates capture.
It does not dispose GPU resources. The writer uses no GPU waits; pipe failure
cannot trigger SIGPIPE process termination on the tested macOS path.

Native phase requests are private C++ values in `p01_phase_capture.h`; they retain
the pool and re-resolve original scene/mesh/skeleton RID owners inside the actual
after-scene or after-views hook. A draw-derived role must match the original
frame, viewport, mesh instance, actual vertex array, physical allocation serial
and complete hazard interval. Unbound previous input is not a previous draw
witness. No native source pointer escapes capture. Runtime admission still needs
true t0 proof and full source/action/retirement closure.

The engine-owned `p01_native_session` now reserves bounded immutable request
batches on main and sends a native callable through the original RS command
queue. Only its actual receive admits a batch at the next after-scene phase.
The session registry and callable retain ownership independently of the fixture;
close cancels unrecorded requests and cannot declare terminal while any delivery
callable holder or accepted readback remains. These private status fields are not
yet serialized action witnesses and the fixture has not yet activated this path.

Pool retirement observes the original buffer disposal plus the pool's registered
shader, compute pipeline and uniform-set frees. Reused native handles receive
new tokens; released observations remain until acknowledgment. This is explicitly
pool scope, not a device-wide retirement certificate. Cleanup is pumped outside
phase evidence admission so poison/overflow does not suppress retirement.

K01/K03 completion is not supplied by this interface. Original RD issue/wait
Errors now poison P01 evidence, but the legacy engine control flow remains
unchanged. Zero-wait poll/recycle, quiet-window admission, exact slot/cutoff
retirement, native callback-holder acknowledgement and actual library OS close
remain required under their original contracts.
