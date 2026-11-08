# Applied aggregate planned hold increments

Private aggregate preflight computes each intended new hold's actual reverse closure, deduplicates within each hold, and sums memberships across at most three holds before checking the original per-resource and device counters. Actual acquisition retains increment1 through the same predicates. No counter, hold, factory or staging cell is changed by preflight.

Combined build with independently reviewed graph prerequisite exit0. Aggregate1/63; combined target10/111; graph neighbors39/899; restored union232/86738 passed. Independent-only resource and device mutations failed exactly2 and1 intended assertions, normal exit1; each exact source restoration/rebuild returned target green. See RUNTIME_RECEIPT.json. No GPU run for this slice.

No production backend calls the aggregate helper yet. Exact retained roots, material raw-root cap before dedup, future factory identity, original reservations/origins/authenticated scope and execution revalidation remain. This is not full G admission.
