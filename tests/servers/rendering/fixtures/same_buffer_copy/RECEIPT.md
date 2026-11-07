# Same-buffer transfer graph

A legal nonoverlapping same-buffer copy now visits its real buffer tracker once as a writer, with COPY_READ and COPY_WRITE present before barrier generation. Exact command/driver/tracker identity restricts this normalization to buffer copies; existing graph self-dependency guards remain.

Original source failed three directed CPU/mock cases: 14 of 94 assertions failed and six self-dependency errors were emitted. Fixed source passed all three cases / 94 assertions with exit0 and no engine diagnostics. Tests inspect transfer access bits, preceding/following dependency paths, exact copy regions, and emitted incoming/outgoing barriers for both buffer transition traits. Exact binary identities/argv are in the run JSON files. Red source bytes have a .snapshot suffix to avoid test discovery.

This does not certify any real GPU backend. The executable also contains the concurrently validated native-output batch; this regression itself targets the independent graph behavior.
