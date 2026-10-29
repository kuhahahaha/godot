# Vulkan resource query guards R60

Production commit `7dc693f89b` checks null buffer entries before dynamic-offset access and checks a null texture before copyable-layout access. A valid output is zeroed before failure. Test-only follow-up `42005b041a` seeds nonzero layout fields to verify this behavior.

Before the guards, both focused tests crashed with SIGSEGV (rc 134). After the fix, focused layout is 1 case / 4 assertions and dynamic offsets 1 / 3; Debug Vulkan regression is 62 / 213; K03/P2 is 22 / 350; UBSan Vulkan is 62 / 213 with no reported sanitizer diagnostics. Debug, UBSan and release builds and release version returned rc 0. Post-follow-up sentinel focused and full Debug regression passed. The UBSan and release results predate the test-only sentinel follow-up and are not represented as built from that later test revision.

These checks reject null handles; they cannot authenticate non-null stale handles. This is bounded input validation, not owner, operation, completion, readback, Ready or G1 evidence. Logs, command lines and input hashes are captured beside this receipt.
