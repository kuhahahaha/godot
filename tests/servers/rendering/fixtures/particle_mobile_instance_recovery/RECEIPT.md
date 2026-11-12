# Mobile particle instance recovery

Instance-only particle resource rebuilds notify `DEPENDENCY_CHANGED_PARTICLES_INSTANCES`. Forward Mobile ignored that notification and kept an invalidated transforms descriptor. Handle it in the existing dirty geometry/dependency group, matching Forward Clustered.

The actual main-Metal fixture creates geometry with the original culler initialization, caches its real descriptor, frees the old copy set and enters the existing instance-resource recovery branch. It refreshes geometry only through the dirty-list gate and authenticates the new descriptor against the rebuilt instance buffer. A subsequent unchanged update must leave geometry clean and retain the descriptor/simulation buffer.

Corrected RED: 386 observations, exactly three target failures, all eight async readbacks completed, all 31 allocations and 28 actual native resources closed. GREEN with the one-case fix: 390 true observations, eight readbacks, 31 allocations and 29 actual native resources closed. RED never creates the recovered transforms set, accounting for its four absent shared observations. The original 362 checks remain required in both runs. Exactly thirteen injected errors remain required.

Final same-host full engine run: 2082 cases / 521421 assertions / zero failures, with existing stdout571/stderr4017 negative-test diagnostics retained. Initial attempts omitted culler dependency initialization and are retained as invalid comparisons; final RED2/GREEN2 use identical corrected fixtures and differ only in the production callback case.

See DELIVERY.json for exact host, source, diagnostic and evidence identities. This accepts native descriptor recovery and physical retirement only; it does not accept rendered pixels, motion vectors, ANGE production or whole OutputReleased.
