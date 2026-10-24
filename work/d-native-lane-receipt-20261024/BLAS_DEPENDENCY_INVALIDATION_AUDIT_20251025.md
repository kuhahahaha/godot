# BLAS Dependency Invalidation Audit

Date: 2026-10-25

Audited BLAS build dependency invalidation and cleanup. BLAS build validates
its owner and scratch allocation before graph admission, then clears prior TLAS
invalidations only after `draw_graph.add_blas_build()` succeeds. Dependency
removal updates reciprocal BLAS/TLAS sets and clears the source set; freeing a
BLAS/TLAS also removes reciprocal links before deferred native destruction.

No safe independent production patch was identified. Runtime/GPU receipts are
still unavailable and the full build remains blocked by disk space; all
qualification flags remain false.
