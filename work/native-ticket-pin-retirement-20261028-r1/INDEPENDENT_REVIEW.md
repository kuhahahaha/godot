# Independent review — V1 writer pin retirement repair

Reviewer: /root/indexed144_native_review, fresh read-only source review; no build/edit/rerun.

`qualification_ticket_publish` now computes a conservative live upload claim from `attempted`, consumed bytes or captured segments. The checked upload path marks `attempted` only after destination validation and successful resource pinning, immediately before `_buffer_update`; therefore ERR_BUSY before a graph prefix cannot release the writer pin or mark the ticket physically retired. Settled append failure and prepared cancellation still satisfy their existing `sources_returned` gates and release pins. The true unstarted prepared zero-effect path remains releasable and reacquirable.

Focused evidence supplied by the implementer: stale-core RED was 19 qualification cases with one failing case/three assertions; current core+servers+tests archives plus E06 stamp object relinked the private host, and `[Qualification]*` GREEN is 19 cases / 339 assertions, rc0. No P1/P2 was found in the change. Scope remains V1 writer custody; E20 typed image/sampler production and full GPU/Ready/G1 chain are outside this repair.
