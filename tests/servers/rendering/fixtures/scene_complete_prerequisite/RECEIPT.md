# Complete native Scene prerequisite adaptation

The fixed 33-file Scene prerequisite patch was applied after the existing typed phase, map-failure, owner cleanup, residual maintenance and Scene45 admission changes. It preserves E09 operation/cursor/digest bind refusal (`ERR_UNAVAILABLE`) and does not fabricate Scene authority.

Root build succeeded with tests and particles runtime tests enabled (59.96s). Focused CPU/dummy checks: NativeScenePhase/NativeScenePhaseCloseWaiting/ScenePhase 4 cases, 420 assertions; Scene receiver/camera/generation selection/guard filters 18 cases, 5885 assertions. These are CPU/dummy checks and do not certify GPU output or physical retirement. Existing deliberate pipeline diagnostics remain expected.

The full original Metal run is separately recorded under `tests/servers/rendering/fixtures/scene45_incremental_admission/`: native generation produced all 117 pairs, but the unchanged runner rejects ordinal0 role6 before completion because its fixed RT max segment count128 is below the required131 for 4192 bytes. No quota or load was changed.
