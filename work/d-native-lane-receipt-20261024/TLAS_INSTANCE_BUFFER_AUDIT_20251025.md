# TLAS Instance Buffer Audit

Date: 2026-10-25

Audited TLAS instance-buffer allocation and cleanup. The implementation checks
native buffer creation, frees the buffer on map failure before publishing it to
the TLAS, updates memory accounting only after a successful map, and defers
unmap/free until acceleration-structure disposal. Scratch-buffer replacement
also queues the old buffer before installing a new one and fails closed on
creation failure.

No safe independent production patch was identified. Runtime/GPU receipts are
still unavailable and the full build remains blocked by disk space; all
qualification flags remain false.
