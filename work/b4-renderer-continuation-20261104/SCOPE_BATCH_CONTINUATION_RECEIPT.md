# Exact row scope and bounded batch custody slice

Original native scope now opens its exact stable row, routes the renderer row factory through that scope, and independently authenticates mandatory cleanup after cancel/withdraw/effects. Scope exit seals and unlinks while preserving original pin/root/group/prefixes. Old PhysicalContext does not gain validity.

Shared private capture primitives inventory/adopt up to two actual holds into the exact pre-reserved group. Per-RID pin multiplicity and actual transferred-hold count survive finite WORK slices; physical free remains once per incarnation. Legacy physical single-hold capture keeps its original admission and uses the common core. No public or standalone scope grant is added.

Narrow RED restores only expired-context row preflight: build0; 3cases/2997assertions/12targetfailures, process1 without engine crash. Restored GREEN build0: scope+batch6cases/3448assertions pass. NativeDisposal/PipelinePrefix/ExternalOutputOwner/QualificationRt45cases/8752assertions pass. Exact commands/hashes/logs: scope-batch-validation-20261108/RECEIPT.json.

Independent Sol scope/batch/merged-interaction review found no remaining blocking issue in these slices. New batch fixture cleanup was corrected for no-exception REQUIRE continuation before runtime. Root fixed one added comment indentation.

Task2 remains incomplete: atomic P tuple-bank reservation, scope-authenticated batch finish, prospective complete closure before effects, geometry/material metadata transfer and real renderer callers remain open. Batch dependency fixed-point and excess reserved cells are source-reviewed but not runtime exercised by buffer-only tests. Public Instance192/Finalize/Seal, GPU/visible Sprite and complete no-draw retirement are not claimed.
