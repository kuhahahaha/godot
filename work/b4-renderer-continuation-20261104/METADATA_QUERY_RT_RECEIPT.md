# Original RT MetadataQuery slice

Original bounded QualificationRt mailbox kind24 carries a normalized pointer-free request plus synchronous copied RID payload, and returns requirements/error with inline detail96. No new queue, ticket or grant. Accepted-before-close work drains using the existing admission; new reserve after close refuses. Fixed P1 configuration remains unchanged.

Behavioral RED: build exit0; 3 cases/29 assertions/8 failures, process1. Initial test attempted dispatch after a failed REQUIRE in the no-exception build and crashed; excluded from acceptance. Explicit failure exits fixed that fixture defect.

GREEN: build exit0; MetadataQuery3cases/73assertions pass; full QualificationRt11cases/631assertions pass. Binary SHA832adc0a75ac0265d1075944ee03188c3544d8995aab12d53b4ea1b746ae34f2. Logs, source hashes and exact build identity: metadata-query-validation-20261108/RECEIPT.json.

Independent Sol implementation and test review completed. Packed source retention is covered; consumption-side two-array reconstruction still lacks direct behavioral observation. Private fixture does not prove real RenderingServer FIFO, GPU or visible Sprite. Worker case is conditionally registered and this thread-enabled build ran its assertions. Public Instance192 and the full caller/Finalize/Seal/render/retirement chain remain open.

Frozen source/SDK and A adapter candidate remain at ../native-output-prequote-rt-20261108/candidate. Original A Catch target runtime2cases/91assertions pass was separate limited relink; A owns central integration.
