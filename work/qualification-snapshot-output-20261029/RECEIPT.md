# Qualification snapshot output integrity

Commit `ed4ccf580e` rejects overlapping facts/copy output spans before writing out_written, preserving the destination bytes on malformed overlap. Independent output counts still clear on ordinary refusal. V1 signatures and ticket lifecycle are unchanged. Fact span size is checked before size_t conversion.

Original RED: 1 case / 8 assertions, 2 intended destination-integrity failures. Initial fix GREEN: 1/8. Review found zero-on-refusal regression; added tests RED 1/12 with 2 intended failures, final GREEN 1/12. Final Debug Qualification 21/358, RT 4/378, physical 7/46, K03/P2 22/350. Final UBSan Qualification 21/358 with no sanitizer diagnostics. Release build/version rc0.

Real Metal qrt_ticket final passes rc0: successful facts paging and exact 16 bytes (7,8,9,10), original source identities, physical return, ticket/bridge acknowledgement and clean DSO unload. A prior run stopped on duplicate record ERR_BUSY before copy; retained in native-ticket/. Retry and final both passed. The intermittent duplicate-record refusal remains an investigation item; this receipt does not establish deterministic duplicate-record behavior.

No U08 owner, canonical ANGE E09 chain, Ready or full G1 claim is made. This change fixes the existing native export boundary. Binaries were built from final source before commit and report the base f0da042b8 label; hashes and source commit are recorded separately.
