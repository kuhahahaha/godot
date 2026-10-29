# Native poll error code propagation

This D-side fix preserves the raw native code for nonblocking native-call and fence poll failures that reach a qualification ticket. `RenderingDevice` resets an internal frame-wait code/validity pair at each poll, captures native codes from chain/hidden/fence observations, and `Registry::qualification_ticket_step` copies the pair only when this frame-wait source is valid. A qualification native poll that already wrote the ticket report is preserved instead of being overwritten with zero. No C ABI field, owner issuer, physical retirement, or G1 predicate changed.

The direct fence regression returns raw `-13`; the same-ticket qualification native poll regression returns raw `-7` in both fact and report; the chain native-call poll regression returns raw `-21`. Final Debug focused poll is 3 cases / 9 assertions; Qualification 29/478, FrameWait17/301, RT4/378, physical7/46, K03/P2 22/350 and device_exit133/2619 all rc0. UBSan 45/777 passes without diagnostics; Release build/version rc0.

The test uses scripted driver outcomes and proves propagation/retention only. It does not claim real device loss, hardware fault acceptance, production E09/Scope, Ready or G1. Existing map-error transport receipt remains separate.
