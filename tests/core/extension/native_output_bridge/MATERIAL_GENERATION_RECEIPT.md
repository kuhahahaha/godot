# Selected material generation follows shader changes

Changing shader code or a default texture now advances every actual owning Material generation and synchronizes MaterialData in the existing owner traversal. Existing held-input refusal runs first. Generations saturate at UINT64_MAX. Native hold acquisition optionally checks a selected generation before acquiring any hold; stale or exhausted selections refuse, while reselected current generations retain the normal acquisition path.

The regression first ran with the final acquire guard and tests but original mutation behavior: 3 cases passed, the new case failed with exactly 6 stale-selection assertions (1118 successful). Applying only the two mutation updates passed all 4 cases / 1122 assertions. Full engine suite passed 2094 cases / 524621 assertions. Exact source and immutable host hashes and raw evidence paths are in MATERIAL_GENERATION_RECEIPT.json.

Tests use the existing CPU native-driver fixture and check actual native holds, stored code/default selection and immutable UBO state. They do not compile GLSL or prove a new GPU program, source admission, full pipeline preparation or created pipeline failure-prefix retirement.

Separate existing actual Metal native390 regression passed all runner checks; this is normal-runtime evidence only.
