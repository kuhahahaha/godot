# Image collector close-drain review — 2026-10-24

The image collector `drained()` predicate now requires `close()` to have been called before reporting a drained state. This keeps image collector teardown symmetric with the readback collector and prevents an unopened, empty collector from being reported as drained.

Independent review verdict: **APPROVE_LIMITED**. The change is limited to the state predicate and adds a focused assertion that an unopened collector is not drained. No holder, receipt, rejection, or late-callback behavior was changed.

Verification boundary:

- Source/static checks: passed.
- `git diff --check`: passed.
- Existing binary selector: returned success with `0 tests matched`; the optional module is absent from that binary, so this is not runtime evidence.
- Full editor/tests build: not rerun; previously blocked by `ar: ... No space left on device`.
- Runtime, native, GPU, PCK, and production qualification: false.
