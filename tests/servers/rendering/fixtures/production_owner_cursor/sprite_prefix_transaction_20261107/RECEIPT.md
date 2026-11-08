# Sprite pipeline-prefix transaction

The internal prefix entry now accepts the actual closed row and a nonzero resource bound, validates the original slot/row/generation/lease, then calls the unchanged owned factory. Only successful reservation opens and attaches the row. Rejection leaves it closed without an allowance, cached context or new group. The redundant pre-action owner bind is removed; original physical context/pin and exact current RESERVING action authentication remain in the factory.

The actual acquired-slot regression checks zero allowance, absent context, BOUND current context, repeated refusal, and the same callback context after expiry. Request revision/frontier/group serial/native and graph counts, native allocation/free counts, draw command count and row identity remain unchanged. Subsequent original OBSERVE still returns the rows.

Actual final macOS build exited 0 in 53.08 seconds. Focused CPU: 11 cases/6,415 assertions; expanded combined CPU: 247 cases/98,810 assertions; both zero failures. Commands, host identity and original output hashes are archived here. The combined source also contains the independent native-disposal/material-hold portability repair.

This is prerequisite state handling, not a real renderer pass opener or successful native pipeline/material/geometry preparation. No guessed framebuffer/subpass/version, public gate change, arbitrary BOUND admission, saved production context, guard/seal/view/draw or GPU qualification is claimed. The tests-first patch requires the new signature and was not executed as a behavioral RED.
