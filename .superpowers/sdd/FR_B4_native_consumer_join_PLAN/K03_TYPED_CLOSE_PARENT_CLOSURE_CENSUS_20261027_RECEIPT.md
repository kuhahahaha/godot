# K03 typed-close parent closure census receipt — 2026-10-27

- Source path: Registry close → NativeServiceNode request/service → exact credits/children retirement → detach/reclaim.
- Live parent obligations return `ERR_BUSY` and keep the same service node attached.
- Service validates attachment serial, main RD, device epoch, and render thread; bounded work requeues.
- Detach and holder/device/library release happen only after all output rows and credits are empty.
- Source tests cover retained-child Busy and exact-credit release; this census did not rerun a mixed-host binary.
- No cancel, operation, completion, readback, Released, Ready/Verified, packet, physical-disposal, or GPU claim.
