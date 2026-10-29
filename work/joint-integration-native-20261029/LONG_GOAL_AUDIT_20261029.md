# Long-goal completion audit

Status: `active`. This audit is evidence-led and intentionally separates local native work, cross-repository integration, and external-owner dependencies. It does not mark the long goal complete.

| Requirement | Current evidence | Status |
| --- | --- | --- |
| A verified independent native work line exists | `native-numeric-unit-correlation-20261029`: real Metal safe positive/cancel controls, retained unit identity, exact readback, physical return and SDK close; `qualification-readback-map-error-20261029`: map/poll raw-code paths, focused/regression/UBSan/Release evidence | **Proven at bounded scope** |
| D-side actionable defects are fixed with regression evidence | Commits `114ca8dcc1`, `6a015c0031`, `c41778870c`; RED/GREEN logs, current Qualification29/478, poll3/9, UBSan45/777, Release0 | **Proven at bounded scope** |
| Public Godot integration line is reproducible | Branch `codex/particle-mainline`, commits `b364625644`, `9f6b9d0bed`, `ce5a594daa`, `732256bfbb`, `305fbb783f`, `1d4a6845bb`; 282 source target replay, generated ABI body equality, dynamic fixture 3/32 | **Proven as local input baseline** |
| Real cross-side request → native completion → same-request readback | A real pair was run on Metal, but stops at `install_main_storage / host.storage_owner_transport / E05 / UNAVAILABLE`; no production E09 Scope or same-request ANGE result | **Incomplete; precise owner boundary recorded** |
| U08 storage/account inputs and committed Host observer | `read_storage_owner_inputs()` remains unconditional refusal before main pair/storage/native port/context/observer commit; `NATIVE_HOST_FOR_B.md` and A runtime receipt pin the exact chain | **External owner blocked, dependent only** |
| U02 holder → E06 request/wait/Prepared production caller | Current joint status explicitly records no production caller; no synthetic demand/observer/Ready fallback used | **Incomplete; separate owner dependency** |
| Method-native descriptor path | C04 map, C05 `materialize_uniform_sets`/lease receipt, E06 shader/pipeline, E09 receipt consumption; texture producer/reverse pin missing; numeric set0 evidence excluded | **Incomplete by design, not falsely promoted** |
| C04/C06 focused source closure | `ANGE_GPU_METHOD_RECHECK_20261029.md`: ANGE commits `21d55546d` and `6121d4e5d`; build rc0; linking, method qualification and serializer projection controls rc0 | **Proven at committed focused scope; product/native integration pending** |
| Full FR/GPU/Verified/Ready/G1 acceptance | No full-corpus, hardware-fault, E09 production, U08, Ready or G1 receipt exists | **Not proven** |

The queue remains active. The E05/U08 owner gate limits the real end-to-end row only; it does not close or cancel the independent D native line. Existing unrelated dirty files in the Godot worktree remain untouched and are not part of the public mainline commits.
