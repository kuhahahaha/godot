# Independent review: detached-directory candidate

Scope: read-only semantic review of `candidate-tuple-detached-directory/after` (as selected by its `vfs.json`), focusing on detached Surface closure acquisition, shared/last-directory return, retry/error behavior, and lock ordering. No live source edits, build, or runtime execution were performed.

## Verdict

No actionable semantic defect found in the reviewed detached-directory repair. This is a static source conclusion, not runtime proof.

## Evidence and transition review

* `_native_hold_prepare_tuple_directory` freezes the retired Surface outside Registry/RD locks, enumerates its full original directory, deduplicates valid roots within the admitted limit, and expands the selected physical hold before adoption (`after/servers/rendering/renderer_rd/storage_rd/mesh_storage.cpp:1014-1070`). Failures clear temporary root storage and thaw the directory; the selected hold remains retained.
* `_native_immutable_hold_expand_directory` acquires the complete transitive closure under a continuous RD lock, verifies every original selected resource is included, then replaces the original physical share. Acquisition refusal preserves the original hold; a containment failure releases only the temporary shares (`after/servers/rendering/rendering_device_immutable_hold.cpp:507-541`).
* Finish uses the prepared detached source instead of the live-only source, so a retained Surface detached by `_mesh_surface_clear` can reach the same prepaid geometry batch (`mesh_storage.cpp:1072-1075`; `after/servers/rendering/renderer_rd/storage_rd/particles_storage_retire.cpp:281-340`). The two inventories precede adoption, and adopted flags preserve retry progress.
* `_native_hold_capture_tuple_directory` stamps the exact prepaid group only after physical adoption. RD validates the original pin/group and every original root cell (`mesh_storage.cpp:1078-1090`; `rendering_device_immutable_hold.cpp:541-600`).
* Shared metadata is returned through the exact original OBSERVE/budget proof before decrementing `immutable_holds`; the last wrapper marks every directory ordinal before common-group release (`mesh_storage.cpp:1093-1127`, `1231-1258`; `particles_storage_retire.cpp:649-669`). A shared wrapper clears its Surface pointer before another wrapper may delete it.
* The return sequence deliberately takes multiple turns: common G settlement; `_native_hold_return_directory` clears the wrapper binding and remembers last-owner directory return; the next turn calls `_native_hold_begin_return`; only then does the row set `geometry_metadata_returned[i]` and later `_native_hold_finish_return` (`particles_storage_retire.cpp:661-689`; `mesh_storage.cpp:1260-1395`). This permits both shared metadata already returned and last-owner metadata deletion without treating an emptied wrapper as native disposal proof.
* The Surface `version_lock` handshake occurs outside Registry/RD locks. Finish authenticates the retained cleanup scope before and after that handshake (`after/core/extension/gdextension_production_output.cpp:261-295`). VA factories honor `output_directory_holds`, preventing directory growth while frozen (`mesh_storage.cpp:677-680`; `after/servers/rendering/renderer_rd/storage_rd/mesh_storage.h:778-783`).

## Limits

The review establishes source-level transition consistency only. Actual detached completion, shared/last-owner cleanup, exhaustion/refusal retries, and concurrent lock behavior still require focused execution evidence; existing syntax outputs do not establish those properties.
