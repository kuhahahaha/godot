# Native CurveTexture image oracle

Build a standalone godot-cpp extension and compare direct E22 RGBAF capture
bytes against the original CurveTexture `_update` image uploaded through RS.
This is an ordinary resource/image fixture, independent of ANGE transactions.

```sh
python3 tests/scene/fixtures/texture_snapshot/run_image_probe.py \
  --godot /absolute/fixed/editor \
  --sdk /absolute/godot-cpp \
  --precision single \
  --engine-receipt /absolute/stable-build-receipt.json \
  --output /absolute/new-results-directory
```

The supplied build receipt records `build_rc=0`, `source_stable=true`, exact
`source_before`/`source_after` SHA-256 maps and `binary_sha256` from an actual
build. The runner checks the current source/binary against it. Preserve the
full build command/log, dirty patch or commit, generated controlled-build
receipt and tests alongside that receipt; a supplied JSON file alone is not
build provenance. The godot-cpp archive is externally built; its exact bytes
and the headers actually used are pinned, not claimed to have been rebuilt.

Each Metal Mobile and Forward+ run must have a main RD, report the requested
actual renderer and run outside editor-hint mode. There is no headless
substitution. Missing RD returns 77 and fails the required gate. Each run
executes 54 combinations (width 32/33/4096, RGB/RED, 0/1/3 points, bake
resolution 1/2/1000) and the original silent bake-resolution 8-to-2 regression.
The expected total is 75,665 checks per renderer. Image RF/RGBF components
are expanded to RGBAF with alpha one and compared bitwise, without invoking
any new bake helper to generate expected values.

CurveTexture does not override `get_image` in this fork. The fixture instead
reads its actual RID using `RenderingServer.texture_2d_get`. RD storage
caches that image only after a read in editor-hint mode; this fixture rejects
that mode and reads each new or normally refreshed texture once. Therefore
this path reaches RD texture readback on the supported host. Readback and
normal texture creation/refresh are outside the snapshot call and outside
allocator measurements. No main-RD submit/sync is used.

A silent resolution mutation deliberately adds no changed notification.
The regression captures fresh bytes before explicitly baking and refreshing
the original wrapper using the public mode setters for the image oracle.
Previously captured output remains immutable. Complete source tuples,
result status and full pixel count are checked for successful captures.

The runner preserves exact commands, compiler output, compile dependency
list, SDK header hashes before/after compilation, engine/extension/input
hashes, raw runtime logs, expected counts and host exit/timeout results.
Nonzero failures, incorrect check counts, renderer mismatch, errors or leak
diagnostics fail.
This does not qualify E20/U08, original source-map ownership, production
ANGE consumers, GPU scheduling performance, Ready or G1. The CPU tests
separately measure allocator entries and verify cache isolation, callback
traces, stale recovery and numeric/ABI behavior. Use `--precision double` with a double-real Editor and a matching SDK built
from that Editor's dumped double API. The runner then selects the double
archive and compilation define; each plan must report the matching real width.
Double mode also requires `--sdk-receipt` and `--api-receipt` from the actual
SDK build and engine API dump. Their successful statuses and exact archive,
API, precision and engine hashes must agree. Single mode pins the supplied
external SDK without claiming a rebuild of that archive.
