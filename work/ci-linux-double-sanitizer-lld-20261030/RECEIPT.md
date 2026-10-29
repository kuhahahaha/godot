# Linux double-sanitizer linker fix — 2026-10-30

The public baseline `aea82acff39fea5727d6c764b2b706cc4252f867` retained the prior `use_static_cpp=no` change and selected `linker=lld` for the Linux double-precision GCC ASAN/UBSAN editor job. The resulting run was:

- [CI run 40921907239](https://github.com/kuhahahaha/godot/actions/runs/40921907239)
- All completed platform jobs passed.
- The remaining Linux double GCC job failed at final link with `collect2: fatal error: cannot find 'ld'` while invoking `g++ -fuse-ld=lld`.
- The failure is a workflow tool provisioning defect: the job did not install the lld package. No source compilation or runtime failure was observed.

The follow-up workflow edit adds an explicit `linker: lld` matrix field and installs the `lld` package when that field is selected. This remains a source-only CI repair pending a new public run. Local `git diff --check` passes; no Godot runtime, owner, GPU, Ready, or G1 claim is made from the failed run.

The earlier mold failure remains recorded in the source manifest: AccessKit Rust `.eh_frame` `R_X86_64_PC32` relocation out of range. The current patch addresses the subsequent missing-tool failure without reverting to mold.
