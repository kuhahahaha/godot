# ANGE E05 typed-close focused native regression — 2026-10-27

The current ANGE native test binary ran eleven focused reservation/typed-close cases with **210 assertions**, all passing (rc0). The cases cover immutable operation correlation, cancellation retention, close/drop refusal, late-failure discharge, publication ordering, generation preservation, retry identity, and invalid-session refusal.

This is CPU/in-process native coverage. It confirms local guards and lifecycle behavior only. It does not supply the original U08 producer, an E09 same-request association, an external operation/completion/readback authority, packet publication, physical disposal, Ready, Verified, or GPU qualification.

Evidence: `K03_ANGE_E05_TYPED_CLOSE_FOCUSED_20261027_REPORT.json` and `/tmp/k03-ange-e05-typed-close-focused-20261027.log`.
