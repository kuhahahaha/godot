# Independent review: native-disposal test shader cleanup

No finding in this routine test-only diff. The old direct shader_owner.free bypassed _free_dependencies, leaving the empty shader dependency-map key seen in the exact old-host cascade RED. The new release_shader uses normal free_rid, drains the existing pending-free path, and asserts both dependency maps empty. FakeDriver.shader_free checks the original fixture shader ID31 and counts its actual driver-free call; the case asserts zero shader frees after child disposal and exactly one after explicit shader cleanup. This preserves the parent shader until its own cleanup and does not attach a production witness or claim native GPU shader retirement.

No production file or diagnostic policy was modified by this review. No engine/GPU process was launched. Root retains syntax/build ownership. This is source review only: the new assertions have not run in a fixed host yet.

The future runner inventory updates only the NativeDisposal source-file hash. All17 exact test names and the other three source hashes are unchanged. Previous run EXPECTED_TESTS snapshots remain byte-for-byte unchanged; old source2578/host159fcc results, including the cascade shutdown ERROR, remain failed. The caller must supply the new canonical source/host identity explicitly for the next validation.
