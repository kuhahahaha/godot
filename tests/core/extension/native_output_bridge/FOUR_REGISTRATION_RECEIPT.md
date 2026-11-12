# Four registered typed output procedures

Replace gdextension_interface.gen.h and gdextension_external_output_typed.h together. DTO definitions and constants now come solely from the original JSON generator; old OutputCreate/Seal/Cancel/Withdraw typedef names remain aliases. The old header-sdk-current snapshot is unchanged.

Guarded lookup names: device_session_output_create_v1, device_session_output_seal_v1, device_session_output_cancel_v1, device_session_output_withdraw_v1. Legacy lookup refuses all four.

Create retains original_output_admission_required; seal retains original_record_bridge_required after its original validation. Cancel needs the actual retained slot root belonging to this exact original request and matching the original Core owner tuple. A distinct later request still lacks the original successor join and refuses; sharing attachment is insufficient. Withdraw with a valid RID/generation returns original_output_generation_owner_required until a genuine generation-level root exists. Public Variant facade behavior is unchanged. Acquire/Poll procedures/results remain absent. Presence is not source/Host admission.

Actual protected RECORD tests: normal RED 19 target failures, then unchanged tests GREEN 1 case/1154 assertions. Full engine suite 2095 cases/525775 assertions, zero failures. Separate existing Metal native390 normal-runtime regression passes; the new typed component uses CPU native fixtures and does not prove source admission, shader work or whole-output release. The initial crashed v2 test was rejected and is preserved separately.

Exact source/host/SDK/raw result pins and provenance limits are in FOUR_REGISTRATION_RECEIPT.json.
