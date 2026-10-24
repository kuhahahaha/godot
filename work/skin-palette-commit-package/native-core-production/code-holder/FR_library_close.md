# Original K03 typed library close boundary

Status: implemented; final nonauthor review in progress (2026-10-22). Internal first stage of the original complete K03 code/holder package; no residency-only public mode and no new A production permission. [Executable plan](FR_library_close_PLAN.md).

The original dynamic loader discarded `OS::close_dynamic_library`'s Error and cleared the handle unconditionally. The wrapper then detached its metadata record; init-abort, reload, and last-ref paths could not distinguish a failed close. The subsequent code-holder protocol cannot be sound on this substrate.

Requirements: preserve the exact first close Error and numerical handle on failure; never infer continued callability; forbid retry/reopen/entry after a real failed close; retain original initialization Error independently; require all existing owning callers to check the result and stop before further teardown on failure. FunctionLoader success remains logical detach only. Legacy custom loaders must migrate to a checked Error return; their return is not an OS-close proof. No GDExtension C ABI is added in this stage.

Acceptance is the plan's actual loader/wrapper tests and isolated owning-caller failure paths, followed by implementation review. Full K03 remains gated on original G1, both G2 boundaries, G3, finite holder issuance/relay, all normal entry coverage, finalizer inventory and original source evidence. The original protocol and root ruling forbid using this stage alone to unlock A's production installation.

## Cross-package evidence note — 2026-10-24

P01/E25/E15-E16 closeout is limited to reviewed source commits and static receipts. `8cf518b407` is the P01 placeholder cleanup; `77e0a73bfc` is the E25 source revision rollover closure. The retirement seam receipt is `work/e15-e16-retirement-audit-20261024/STATIC_RETIREMENT_AUDIT.json`. The complete build remains disk-blocked by `No space left on device`, so no runtime/native/GPU/production qualification is asserted.
