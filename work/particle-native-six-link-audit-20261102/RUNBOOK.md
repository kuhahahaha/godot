# Fixed-pair production capture runbook

Status: WAITING_FOR_OWNER_PACKAGE. No production run is represented here.

## Intake and command

A supplies three complete JSON capture specs named `success.json`, `failure.json`, and `cancel.json` in one absolute package directory. Each spec uses the existing `capture_run.py` schema:

- `scenario`: the matching success/failure/cancel value.
- `cwd`: absolute production project directory; do not rely on the controller's working directory.
- `timeout_seconds`: positive finite capture deadline, chosen to allow settlement and drain.
- `declared_versions`: nonempty `godot_revision`, `ange_revision`, `editor_version`, `sdk_version`, `dso_version`, `renderer`, and `device`.
- `inputs`: `editor`, `script`, `sdk`, `dso`, `request`, `oracle`, `issuer`, and `project`; each has an absolute regular-file `path` and owner-supplied `sha256`. `project` means the project.godot file. Pin additional loaded assets and configuration under additional input names. Dirty source manifests belong here as additional pinned inputs.
- `command`: literal argv array beginning with the pinned editor or `{editor}`. `{script}` is the only other supported substitution. A supplies actual project, renderer and product caller arguments, plus the request/oracle/issuer paths accepted by that caller. The controller does not create a caller or add ABI calls.

Use an output directory that does not already exist. After substituting the actual owner package and destination paths, these are the capture commands:

```sh
python3 /Users/macbookair/Documents/Work/godot_ANGE/work/particle-native-six-link-audit-20261102/capture_run.py --spec /ABSOLUTE/OWNER_PACKAGE/success.json --output /ABSOLUTE/EVIDENCE/success
python3 /Users/macbookair/Documents/Work/godot_ANGE/work/particle-native-six-link-audit-20261102/capture_run.py --spec /ABSOLUTE/OWNER_PACKAGE/failure.json --output /ABSOLUTE/EVIDENCE/failure
python3 /Users/macbookair/Documents/Work/godot_ANGE/work/particle-native-six-link-audit-20261102/capture_run.py --spec /ABSOLUTE/OWNER_PACKAGE/cancel.json --output /ABSOLUTE/EVIDENCE/cancel
```

`/ABSOLUTE/...` are unresolved placeholders, not runnable production inputs. Run success first and inspect its provenance before the other scenarios. Keep each controller shell rc and capture.json. CLI rc 0 means process capture succeeded; rc 1 means a captured process/pin/timeout/drift failure; rc 2 means specification or controller I/O failure. The actual child rc is `capture.json.returncode`, which may be null when no child launched. None of these return codes establishes production acceptance.

## Evidence review table

All rows start PENDING. Populate observed values and raw artifact offsets after a real run; retain original logs even if a gate fails. The detailed existing ABI fields are in R02_CORRELATION_ACCEPTANCE_TABLE.md.

| Gate | Evidence required from this request | Pass condition |
| --- | --- | --- |
| Fixed pair and issuer | Input before/after hashes, owner source manifest, actual caller and retained request source | No drift; editor/SDK/DSO agree with owner package; authentic caller actually executes |
| Prepare/upload/submit | Original owner/request identity, bridge and ticket refs, prepare tuple, upload payload digests, each API rc, record ranges | One retained request reaches accepted submit with matching tuple and original resources |
| Completion | Same-ticket report revision/state, issue/completion facts and serials, source backing/offset/size | Native completion corresponds to accepted submit and original ranges |
| Exact readback | Same-ticket copy rc/count, raw result file, byte length/SHA-256, every range and readback_request serial, pinned independent oracle | State 8, current revision, physical retirement, capacity and copied count equal owned_size; bytes and range layout match oracle |
| Release | Same-ticket release rc, physical retirement and outstanding reference observations | Successful release after accepted work settles and native references retire |
| Drain/exit | Every typed child retirement/ack rc, bridge close/unload rc, process rc | All owned resources acknowledged and close/drain succeeds before normal exit |
| Failure/cancel | Original failure trigger or cancel rc, rejected later admission, admitted-work settlement, release and drain evidence | Expected failure/cancel outcome and physical return both observed for that scenario |

The owner caller must export raw exact readback bytes before releasing the ticket. Pin its output destination in the command/package, preserve the file, record length/SHA-256 and associate it with the original ticket/report revision. stdout containing a digest alone is insufficient. If the current caller cannot export this evidence, that gate remains PENDING; do not substitute bytes from a native probe.

## Return package

Return the three specs unchanged, all capture.json/stdout.bin/stderr.bin files, raw readback files and range metadata, shell and child return codes, byte lengths/hashes, owner logs for return/drain, and the populated gate table. Distinguish not-run, rejected, failed and passed rows. A forced stop leaves cleanup NOT_ESTABLISHED_AFTER_FORCED_STOP even if earlier output looked successful. The controller intentionally always emits production_acceptance NOT_PASSED; a separate source-aware review of all evidence is required.

Native fixture/control tests remain scoped to their own inputs. This runbook does not upgrade them to production six-link, Ready, G1 or GPU lifecycle acceptance.
