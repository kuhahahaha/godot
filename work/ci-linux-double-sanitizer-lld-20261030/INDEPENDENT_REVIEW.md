# CI linker repair independent review — 2026-10-30

The first public lld attempt was reviewed from the raw job log. It reached the final `g++ -fuse-ld=lld` link and failed because the runner could not find `ld`; this is consistent with the workflow lacking an lld package installation. The follow-up is limited to matrix metadata plus conditional `apt-get install lld`, preserving GCC, double precision, ASAN/UBSAN, and dynamic libstdc++ settings. It does not alter engine source or lifecycle behavior. A new CI run is required before claiming green.
