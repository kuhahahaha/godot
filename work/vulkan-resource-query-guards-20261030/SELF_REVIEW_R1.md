# Self-review

The copyable-layout output is initialized only after confirming that the output pointer is non-null, and the resource check precedes all dereferences. Dynamic offsets return zero rather than a partial mask if a null entry is encountered; the existing non-null processing is unchanged. Null checks do not establish validity of arbitrary non-null pointers. Neither patch touches lifecycle ownership or backend completion. Tests include both crash reproductions and the nonzero-output reset. This is a self-review, not an independent review.
