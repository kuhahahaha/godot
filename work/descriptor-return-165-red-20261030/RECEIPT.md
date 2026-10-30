# Current-source descriptor return red control — 2026-10-30

An isolated worktree at `7a64a659b6aad1841a14a3e998a543198b3ef2f4` contains only the test in `RED_TEST.patch.b64` (decoded SHA-256 `b57d839eefff1117610dcf0c6bbed3203d69e1ffc429fb287763234256afa0d9`). An incremental module-enabled editor build reused copy-on-write objects, rc 0; the red-test binary SHA-256 was `e216f63c974c2d815c6ae663d9469ec3e2912a4595b486ed134764bd7f07ff27`. The test injects an Unknown result through a controlled driver, creates the original dependent set, and exercises the public free followed by two disposal passes.

The focused test returned 1 with 2/8 assertions passing: the checked method was called zero times, the legacy void free was called once, and the dependent count was reduced to zero. This establishes that the current RD path bypasses the checked return in this controlled case; it is not an observed Vulkan device loss. No production implementation is changed in this worktree. Full repair must retain every actual parent, preserve synchronous callback lifetime and parent-first cascade, avoid replay of unknown effects, and prevent destructive finalization; see `../descriptor-return-native-20261029/RECEIPT.md`.

After the red run, only its regenerable test object and test archive were removed to relieve disk pressure; source patch, executable, logs, earlier fixed artifacts, and other worktrees were preserved.

An additional test-only work-in-progress (`PARENT_WIP.patch.b64`, decoded SHA-256 `2347a59f522daba8f20b862fef2f7a9697516402de38a89404604e99726f9c5e`) adds a separate ordinary-buffer physical-retention assertion. It has not been compiled or run and may require fixture cleanup work once the full transaction exists; it is not evidence of a test result or a production fix.
