# GLES3 particle lifetime split arithmetic receipt

Date: 2026-11-02

The lifetime-order copy and reverse-sort paths now use one checked split helper before GL buffer work. It rejects zero or oversized amounts, negative/non-finite phases and overflowed scaled phases, and computes the split with 64-bit indexing. This removes the old signed `int(amount * phase)` conversion and modulo-by-zero path.

The source fixture was RED at pre-change `HEAD` (missing helper) and GREEN on current source (5/5); staged diff check passed. The isolated change is the two callers, helper and its declaration. Current mixed-tree Compatibility smoke and test-enabled build preceded this isolated commit and are only neighboring evidence. A clean rebuild and runtime lifetime-order draw test are not yet claimed.
