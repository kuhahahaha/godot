# Image duplicate-reject fix review — 2026-10-24

Change `6ff3e53069` extends the image collector rejection guard so a second `reject(slot)` on an already rejected entry poisons the collector, matching the existing invalid-state policy for out-of-range, unreserved, completed, duplicate receipt, and late callback operations.

Focused source assertion verifies the second rejection poisons the collector. No binary selector was available in the existing executable, so this is source/static evidence only. Runtime/native/GPU/PCK/production qualification remains false.

Independent review requested; no full build rerun due the existing disk-space blocker.
