# Skinned mesh capture native interface

This directory holds the engine raw capture interface and payload schema. The
optional implementation lives in `modules/skinned_mesh_capture_tests/` and is
compiled only when its tests/3D/module eligibility allows it. Documentation here
does not enable or link the module.

The E34 supervisor is the sole process, receiver and verdict owner. An earlier
`receiver_contract.cpp/.h` was an unused byte/seal/EOF placeholder with no consumer
or doctest. It was incorrectly compiled into every `tests=yes` build by recursive
`tests/SCsub` discovery. That placeholder is removed; no replacement receiver or
artificial test consumer is introduced. Both module-on and module-off discovery
must exclude it. Actual transport tests remain with the optional native module.

These documents previously lived in `tests/native/p01_probe/`. The immutable
payload schema and public `--p01-*` wire identifiers are unchanged. Old-to-new
mapping and original files are preserved under
`work/p01-native-package/native-test-structure/`; historical receipts retain their
original paths and hashes.
