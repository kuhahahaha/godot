# Ordinary render pipeline RID capacity failure

The ordinary factory now reserves a real RID after validation and before native driver creation. Exhausted capacity refuses before creating a pipeline. Both quiet and nonquiet driver failures return the CPU reservation; success initializes that same RID. The qualification branch keeps its existing reserved path.

The regression fills the actual default thread-safe RID arena. Original code fails exactly one assertion by calling the driver after capacity is exhausted (93 assertions pass). With unchanged tests, the fix passes 85 assertions, including repeated sole-slot failures and later successful original pending disposal. Nine RED-only orphan cleanup checks are absent when creation is correctly refused. Full engine suite: 2099 cases / 527491 assertions, zero failures. Separate actual Metal native390 normal-runtime regression passes.

CPU test metadata does not prove GLSL execution, GPU arena exhaustion, source admission or full material preparation. Exact source, immutable host, raw results and cleanup limits are pinned in PIPELINE_RID_CAPACITY_RECEIPT.json.
