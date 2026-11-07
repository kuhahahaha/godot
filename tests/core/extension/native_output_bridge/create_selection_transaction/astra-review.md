# Independent Create transaction candidate review

Reviewed Sol's 01-create-integration.patch and candidate particles_storage_external.cpp/.h against the current original owner/layout/retirement paths. This is an independent source review of that integration patch, not execution evidence. No active source edits, builds, tests or Git operations. No confirmed new blocker found for the proposed private opt-in transaction. Public Instance192 remains unavailable.

## Reachability and authorization

The new receiver argument defaults to false in the private declaration. Both existing production call sites omit it. `_external_create_typed` retains its SceneOnly gate; the public metadata query retains its SceneOnly gate; V3 recomputes through that query before reaching its unchanged default-false receiver call. There is no new exported private-opt-in selector and no callable path in this patch that enables public Instance192.

Even a future internal true caller must provide an original metadata lease and protected context: receiver validates the exact lease/request/device before incrementing generation, constructing placement owner or allocating an internal particles RID. Native guard readiness and layout checks preceding authentication only read original state; they issue no lease, factory, source permission or account admission. Private requirements query intentionally has no producer context and requires actual main RenderingServer/RD render-thread identity. It reports sizing only.

The helper assumes its ExternalOutputConfig is decoded, as the signature/comment and current call site establish. It is not a replacement raw-config validator; future internal test/production callers must preserve that precondition. Normal public SceneOnly configs are validated by `_external_decode_create` first.

## SceneOnly query equivalence

Public input ABI/limit ABI, reserved fields, main-thread check, configuration decode, profile refusal and nonzero-limit refusal remain in the same outer path. The extracted helper repeats harmless limit/thread checks. Its SceneOnly branch does not consult guard readiness, calls the same zero-directory native planner, returns layout_version1, identical native bytes/alignment, config-derived slot count, zero material_selection_count/binding_count and identical limits. No SceneOnly behavior regression found. Existing exact-size and wire tests remain appropriate regression evidence once root runs this version.

## Original rollback stages

Before placement/root installation the caller still owns p_metadata. Directory installation has no error returns after placement begins; thus the existing directory-error destructor path is not hiding partially installed active rows. RID allocation refusal destroys empty reserved rows, marks RELEASED and destroys the placement owner; metadata remains in the caller. Root-install failure unlinks any unused pin through the original install_generation_root rollback, clears particles->external, destroys empty rows/owner and frees the unpublished RID. No selection lease exists in these pre-root paths.

After root installation the receiver moves metadata custody into the placement owner before acquiring selection. On any acquisition error—including metadata/context/singleton refusal before generation_selection_attempted—the receiver calls withdraw, marks unpublished and FAULTED_RETIRING, writes only failure status and returns. It does not destroy an active lease, return owner metadata or lose the original reverse owner locator. This closes the pre-attempt caller-obligation gap identified in the component audit. Successful identity/capability fields are assigned only after complete selection.

The receiver retains the existing internal convention that callers initialize r_result before entry. Existing public callers do so. Any new direct test wrapper must zero the result as well; no failure-path field clearing is added by this patch. That is a caller precondition, not a newly exposed untrusted endpoint.

## Planner/guard consistency

Internal requirements and receiver both use actual saved engine preparation Error plus actual wrapper readiness and the same plan_generation_selection_layout. That planner separately derives original mesh/material rows, P storage and P*M bindings while taking explicit consumer limits. Receiver constructs with inline selection opt-in and installs exactly that layout. The minimal component constructor has null/unconfigured guard and cannot pass by supplying fabricated DTO readiness.

No retry/preparation is triggered by query. A failed initial engine guard remains unavailable, which is consistent with its renderer-owned lifecycle. No guessed shader/pipeline or pass-specific resource is created here. SceneOnly bypasses this guard branch.

## Lifetime and evidence boundary

Failed installed owners remain reachable by their original generation root; original bounded OBSERVE returns selection prefixes, directories and root, then same-callback particles_free returns metadata for unpublished owners. The patch does not bypass source/frontier/device-loss gates or cancel the request. Successful owners still require caller withdrawal/free. Its capabilities remain zero with an explicit warning that canonical public capability semantics are unresolved.

This patch adds no directed execution coverage itself. The current CPU component lifecycle fixture does not exercise these private receiver branches or real engine guard readiness; it composes the underlying original owner operations through a test bridge. Required next evidence is an actual true-opt-in receiver fixture under real engine guard state, including pre-root refusals, successful selection and post-root pre-lease/prefix failure. Neither source review nor CPU component success justifies removing the public profile gate, declaring raster/seal/read completeness, or claiming GPU qualification.

## Applied version and latest verification boundary

Final production re-read after root application: active particles_storage_external.cpp SHA256 `ffe511878a5d1783037848f6801d58c770659da6437f00f5a48dc948c1520efb` and particles_storage.h `cb87d8f4f12df888c11d8cf203e3b5a14effccd1a74ab036d08caed10b69b48b` are byte-identical to the reviewed candidate. All five surrounding production files listed in SUBSTRATE.json remain hash-identical to that review baseline. Rechecked both private receiver calls omit the optional flag, its default remains false, and both public SceneOnly profile gates remain present. No additional production regression found. The final five-file delivery consists of these two production files plus three refusal-fixture files; this follow-up independently assesses production, not claiming independent authorship review of my own fixture.

Read root's saved build/test artifacts: `work/instance192-create-integration-build-r1.log`, `work/native-output-continued-create-integration-focused-r1.{json,log}`, and corresponding `combined-r1.{json,log}`. Parent reports build exit0; saved runner JSON independently records both test exits0. Log totals are focused7 cases/796 assertions and combined215 cases/91763 assertions, all passed. Both runner receipts identify156978168-byte binary SHA256 `e8ff190cde7c0b44dcf1347adc4c1ecf87a8878039d9359346c4e6e715e2e0f2`. Parent assessed combined diagnostics as the existing expected set; this reviewer checked saved receipt/totals rather than rerunning or independently reclassifying every diagnostic.

New execution evidence covers actual unprepared component guard refusal through the private native requirements helper and true-opt-in receiver: exact ERR_UNCONFIGURED, unchanged caller metadata, no generation/RID/root/selection effect. Public query/V3 still refuse Instance192; focused tests also cover existing SceneOnly metadata/wire cases. This replaces the earlier “patch adds no directed execution coverage” statement only for these refusal/regression paths. The successful private receiver path under a real normal engine guard remains NOT RUN. No GPU qualification, public Instance192 enablement, seal/read/consumer completeness or positive native receiver acceptance follows from these CPU/dummy results.
