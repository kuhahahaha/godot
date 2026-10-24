# Codec/frame terminal cleanup blocker — 2026-10-25

Audited capture codec terminal append/seal/transport behavior and frame-event validation as an independent local native cleanup surface.

The codec poisons on post-seal/duplicate/overflow/sequence-gap operations, preserves already admitted completion drain, and makes seal require a started, unpoisoned stream. Frame validation rejects zero identity fields, non-increasing sequence, and offset overflow before any downstream use. No safe standalone source patch is justified.

Disposition: precise blocker/no-change receipt. Existing optional-module binary selectors remain unavailable; full build remains skipped. Runtime/native/GPU/PCK/platform/production qualification remains false.
