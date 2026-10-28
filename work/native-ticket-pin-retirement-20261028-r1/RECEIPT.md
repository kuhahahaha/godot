# Qualification ticket writer pin retirement fix — 2026-10-28

Status: D-side native bug fixed and focused verified. This is an independent V1 lifecycle repair; E20 typed texture/sampler production remains gated on root adoption.

## Defect

`Registry::qualification_ticket_publish` treated a prepared ticket with no graph/native prefix as no-effect and immediately called `_qualification_unpin_resources`, even when an upload had already attempted an effect. A checked upload can return `ERR_BUSY` before its first graph segment, so this released the original writer pin and marked the ticket physically retired while the operation remained admitted for retry.

## Fix

The reducer now tracks `upload.attempted`, consumed bytes and captured segments as a live upload claim. The no-effect release path is allowed only when no upload claim exists. Settled cancellation or failed-prefix paths still release through their existing gates; successful state8 behavior is unchanged. The true unstarted V1 zero-effect ticket continues to release and reacquire its original pin.

## Evidence

- Before production archive refresh, the new regression against the stale core archive was RED: 19 qualification cases, 1 failed case, 3 failed assertions (`attempted upload keeps original writer pin...`). The stale private link also lacked E06 stamp object and failed unresolved `get_engine_build_identity`; this is preserved as harness provenance.
- Rebuilt the current core, servers and tests archives; relinked private host with current `engine_build_identity.gen` object. Focused `[Qualification]*` run: 19 cases / 339 assertions, rc0.
- `git diff --check` is clean for both changed source files. No SDK/ANGE builder or production actor/authority was added.
- Post-fix combined private host regression `[Qualification]*,[ShaderCompilePolicy]*,[B4]*`: 42 cases / 852 assertions, rc0. The three foreign-camera diagnostics are expected negative history cases.

## Scope

This fixes current V1 writer-pin custody only. It does not implement E20 typed image/sampler ABI, actual buffer→texture transfer, content version, readback or U08/Ready/G1. Those remain in the adopted-priority queue and require root adoption of the candidate first.
