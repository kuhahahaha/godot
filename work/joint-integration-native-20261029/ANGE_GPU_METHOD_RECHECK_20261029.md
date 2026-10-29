# ANGE GPU method source-closure recheck

This is a shared-tree verification record for the current ANGE particle method work. The ANGE checkout was already mixed and staged before this recheck; no ANGE commit, reset, cleanup, or source export was performed.

The first focused link attempt (`/tmp/ange-gpu-method-focused.log`) compiled the modified GPU objects but failed at `ange_gpu_linking_test` because the focused static closure omitted the lifecycle-kernel, template-layout, and C06 stage-plan owner/view objects. The unresolved symbols included `scalar_shared_scan_recipe`, `resolve_compiled_schedule`, `derive_checked_template_layouts`, and `PhaseCapabilityProfileView` methods.

The source-closure change is now recorded by ANGE commit `21d55546d` (`integrate C03 C04 static particle source closure`). The warning-only indentation/control-flow cleanup is recorded separately by ANGE commit `6121d4e5d` (`Clarify particle method control flow`). The shared tree still contains unrelated dirty paths, which were left untouched.

Command:

```text
/usr/bin/python3 /Users/macbookair/.local/bin/scons -Q -j2 test-gpu-linking test-method-qualification test-gpu-serializer-projection
```

Build rc: `0` (`/tmp/ange-gpu-method-focused-r3-build.rc`). The target was up to date after the repaired link and emitted no `warning:` or `error:` lines in the final log.

Runtime results:

- `ange_gpu_linking_test`: rc `0`; retained-source controls and 73 allocation-failure points passed.
- `ange_method_qualification_test`: rc `0`; 23 method vectors and exact raw/fault/guard/epilogue negative controls passed.
- `ange_gpu_serializer_projection_test`: rc `0`; 338 checks, 58 bytes, 3 spans passed.

This proves only the focused CPU/static source closure in the current mixed ANGE tree. It does not prove a clean commit, product DSO, native E09 scope, same-request readback, Ready, or G1.
