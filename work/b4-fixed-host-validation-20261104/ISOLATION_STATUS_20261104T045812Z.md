# Fixed old-attempt-02 host: exact isolation and independent groups

Host SHA159fcc04ab3da17cb71dea76d9733d2c2972976b53e6d3ad7a70cba530bd711d, canonical source2578a541f19ad123af988ceec9ecb9e68c731f37. All seven processes execute the same immutable fixed host, each separately with exact case filters. No GPU run was started.

Raw evidence: `runs/20261104T045812Z-native-isolation-and-independent/` (per-process XML/stdout/stderr, RESULTS.json and FILES.json).

| Selection | Cases/assertions | Process result |
| --- | --- | --- |
| NativeDisposal buffer queue | 1 /15 | rc0, exact XML pass, no ERROR |
| NativeDisposal set cascade | 1 /21 | rc0, assertions pass, **shutdown ERROR** dependency_map.size at RD finalize; not a clean pass |
| NativeDisposal preflight/serial mismatch | 1 /10 | rc0, exact XML pass, no ERROR |
| NativeDisposal failed native creation | 1 /9 | rc0, exact XML pass, no ERROR |
| ExternalOutputGuard | 5 /444 | rc0, exact XML pass, no ERROR |
| ExternalOutputConfig | 6 /1103 | rc0, exact XML pass, no ERROR |
| Spatial inverse model | 2 /14 | rc0, exact XML pass, no ERROR |

The shutdown error is now independently localized to the set-cascade case. Other three native-disposal cases pass cleanly in isolation. The independent13 cases pass cleanly with1561 assertions. These facts are separate from the failed NativeDisposal group and **do not constitute17 clean passes**. The error was not suppressed or whitelisted.

Root was notified immediately and retains ownership of the fixture cleanup change and next canonical build. No C++ or diagnostic oracle was changed by this agent. Actual Metal validation remains pending the new fixed host under root's serialization.
