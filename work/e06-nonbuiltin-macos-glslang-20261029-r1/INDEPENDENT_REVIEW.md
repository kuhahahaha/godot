# Independent review — R1

Scope: `platform/macos/detect.py` external glslang selection and the recorded
nonbuiltin editor build.

Findings:

- No fallback to the vendored compiler is reachable from the new branch.
- The branch requires the public glslang header and a library directory before
  mutating the include/library search path; an absent installation exits with
  a build error.
- The three libraries linked by the successful build are the selected external
  installation, as shown by `otool -L`.
- The runtime identity remains schema 0 / unavailable for nonbuiltin builds;
  the receipt does not reinterpret that record as a schema-1 identity.
- No change weakens G1, creates an operation/completion/readback authority, or
  promotes Ready.

Disposition: no P1/P2 finding in this bounded source/evidence review. The
external compiler identity contract remains an owner boundary and is retained
as an open queue item.
