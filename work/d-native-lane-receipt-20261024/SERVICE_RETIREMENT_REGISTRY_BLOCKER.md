# Service-retirement registry blocker — 2026-10-24

Audited native-session `service_retirement()` registry cleanup.

The method requires the render thread, snapshots shared pointers under `registry_mutex`, services each session outside the registry lock, then erases only sessions whose `terminal` state was established by `maintain()`. The local snapshot keeps a session alive through maintenance and cleanup even if registry removal occurs immediately afterward.

No safe standalone patch is justified. Moving erase before maintenance or dropping the snapshot would risk use-after-release or skipping retirement; adding a second registry lock/order would require a broader ownership contract.

Disposition: precise blocker/no-change receipt. Full build remains skipped; runtime/native/GPU/PCK/platform/production qualification remains false.
