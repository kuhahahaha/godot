# Native qualification callable handoff

Date: 2026-11-01 (Asia/Shanghai)
Godot branch: `codex/particle-mainline`
Godot commit: `4792c3f9385d3e5187aa15c666c5f5395bf9b862`

## Fixed Godot artifacts

- Editor: `/Users/macbookair/Documents/Work/godot_ANGE/bin/godot.macos.editor.arm64`
- Editor SHA-256: `759c0f24ec2271f74ac0e7618f802a9a8980ad1b846ebc6eb09ebdfdfa05f5c0`
- Core DSO: `/Users/macbookair/Documents/Work/godot_ANGE/bin/libgodot.macos.editor.dev.arm64.dylib`
- Core DSO SHA-256: `bb8f801ad461026855f1065e2e411714109b6f30b810aa3db83fbcbb6735e0bc`

The binaries were built from the current Godot branch with the editor test target. The ratio-input patch is included in this identity; no ANGE DSO identity is implied.

## Callable surface already present

Registered by `core/extension/gdextension_qualification.cpp`:

- `device_session_qualification_prepare_uploads_v1`
- `device_session_qualification_upload_v1`
- `device_session_qualification_record_prepared_v1`
- `device_session_qualification_record_v1`
- `device_session_qualification_observe_v1`
- `device_session_qualification_facts_v1`
- `device_session_qualification_copy_v1`
- `device_session_qualification_release_v1`
- `device_session_qualification_cancel_v1`
- `device_session_qualification_open_v1` / `close_v1`

Implementation anchors:

- `core/extension/gdextension_qualification_ticket.cpp:239` — observe
- `:258` — copy
- `:269` — release
- `:316` — prepare uploads
- `:368` — upload
- `:405` — record prepared
- `:437` — cancel
- `:79` — internal `qualification_record_append`
- `:212` — same-ticket `qualification_ticket_step`
- `core/extension/gdextension_qualification_rd.cpp:184` — frame polling and exact completion serial matching

`record_prepared` preserves the original upload plan and appends the readback graph under the same ticket. `observe` schedules continuation and `copy` requires the exact report revision, completed state, physical retirement, and exact owned size. `release` remains a physical retirement operation; cancellation is separate.

## Normal host drive

A real RD path requires ordinary macOS NSApp startup with Metal and safe render thread:

```sh
bin/godot.macos.editor.arm64 --rendering-driver metal --render-thread safe --path <fixture>
```

Do not use `--headless` or `Main::test_setup` as production Metal evidence. The normal startup path initializes DisplayServerMetal, RenderingDevice, RendererCompositorRD, and the render-thread callback loop. All qualification allocation, fault injection, append, observation, and cleanup must stay on the render thread.

## Exact remaining owner input

The current Godot callable surface still cannot mint the missing original request identity. The original A/B owner must provide, and the production caller must retain through append and readback:

1. the original reservation/operation identity;
2. the checked append outcome distinguishing zero-effect refusal from issued/unknown side effects;
3. the source allocation/unit identity bound to that request;
4. the exact readback issuer and consumer for the same request/range.

Godot can correlate graph record, queue/native issue/completion and frame/fence facts after that identity is supplied. It cannot truthfully promote local qualification ticket serials to an R01/R02 original operation, and it cannot claim same-request completion/readback or Ready/G1 without the original source owner.

## Verification performed

```sh
scons platform=macos target=editor tests=yes arch=arm64 -j3
bin/godot.macos.editor.arm64 --headless --test --test-case='[ParticlesReadback] particle ratios reject non-finite values' --quit
```

Build passed. Ratio boundary test: 1 case / 7 assertions passed. Existing native qualification tests and receipts remain the source of their own claims; this handoff does not extend them.
