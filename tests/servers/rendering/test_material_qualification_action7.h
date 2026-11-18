/**************************************************************************/
/*  test_material_qualification_action7.h                                 */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

// Included inside TestProductionRequest::Access, after SingletonEnrollment.

struct MaterialQualificationAction7Ports {
	GDExtensionInterfaceDeviceSessionProductionQuoteV1 quote = nullptr;
	GDExtensionInterfaceDeviceSessionProductionAcceptV1 accept = nullptr;
	GDExtensionInterfaceDeviceSessionProductionInstallRecordReceiverV1 install_receiver = nullptr;
	GDExtensionInterfaceDeviceSessionProductionOpenV1 open = nullptr;
	GDExtensionInterfaceDeviceSessionProductionCloseV1 close = nullptr;
	GDExtensionInterfaceDeviceSessionProductionRequestRequirementsV1 requirements = nullptr;
	GDExtensionInterfaceDeviceSessionProductionReserveV2 reserve = nullptr;
	GDExtensionInterfaceDeviceSessionProductionObserveV3 observe = nullptr;
	GDExtensionInterfaceDeviceSessionProductionRecordScheduleV1 record_schedule = nullptr;
	GDExtensionInterfaceDeviceSessionProductionRecordConsumeV2 record_consume = nullptr;
	GDExtensionInterfaceDeviceSessionProductionAckSegmentV1 ack_segment = nullptr;
	GDExtensionInterfaceDeviceSessionProductionFactsV2 facts_v2 = nullptr;
	GDExtensionInterfaceDeviceSessionProductionCancelV1 cancel = nullptr;
	GDExtensionInterfaceDeviceSessionProductionReleaseV2 release = nullptr;

	GDExtensionInterfaceDeviceSessionProductionPrepareUploadV2 prepare_upload_v2 = nullptr;
	GDExtensionInterfaceDeviceSessionProductionPrepareMaterialQualificationV1 prepare_mq = nullptr;
	GDExtensionInterfaceDeviceSessionProductionReadMaterialSelectionV1 read_selection = nullptr;
	GDExtensionInterfaceDeviceSessionProductionPrepareMaterialRecipeV1 prepare_recipe = nullptr;
	GDExtensionInterfaceDeviceSessionProductionRecordMaterialQualificationV1 record_mq = nullptr;
	GDExtensionInterfaceDeviceSessionProductionReadMaterialQualificationV1 read_mq = nullptr;

	template <typename T>
	static T get_fn(const char *p_name) {
		return reinterpret_cast<T>(GDExtension::get_interface_function(p_name));
	}

	bool load() {
		quote = get_fn<GDExtensionInterfaceDeviceSessionProductionQuoteV1>("device_session_production_quote_v1");
		accept = get_fn<GDExtensionInterfaceDeviceSessionProductionAcceptV1>("device_session_production_accept_v1");
		install_receiver = get_fn<GDExtensionInterfaceDeviceSessionProductionInstallRecordReceiverV1>("device_session_production_install_record_receiver_v1");
		open = get_fn<GDExtensionInterfaceDeviceSessionProductionOpenV1>("device_session_production_open_v1");
		close = get_fn<GDExtensionInterfaceDeviceSessionProductionCloseV1>("device_session_production_close_v1");
		requirements = get_fn<GDExtensionInterfaceDeviceSessionProductionRequestRequirementsV1>("device_session_production_request_requirements_v1");
		reserve = get_fn<GDExtensionInterfaceDeviceSessionProductionReserveV2>("device_session_production_reserve_v2");
		observe = get_fn<GDExtensionInterfaceDeviceSessionProductionObserveV3>("device_session_production_observe_v3");
		record_schedule = get_fn<GDExtensionInterfaceDeviceSessionProductionRecordScheduleV1>("device_session_production_record_schedule_v1");
		record_consume = get_fn<GDExtensionInterfaceDeviceSessionProductionRecordConsumeV2>("device_session_production_record_consume_v2");
		ack_segment = get_fn<GDExtensionInterfaceDeviceSessionProductionAckSegmentV1>("device_session_production_ack_segment_v1");
		facts_v2 = get_fn<GDExtensionInterfaceDeviceSessionProductionFactsV2>("device_session_production_facts_v2");
		cancel = get_fn<GDExtensionInterfaceDeviceSessionProductionCancelV1>("device_session_production_cancel_v1");
		release = get_fn<GDExtensionInterfaceDeviceSessionProductionReleaseV2>("device_session_production_release_v2");

		prepare_upload_v2 = get_fn<GDExtensionInterfaceDeviceSessionProductionPrepareUploadV2>("device_session_production_prepare_upload_v2");
		prepare_mq = get_fn<GDExtensionInterfaceDeviceSessionProductionPrepareMaterialQualificationV1>("device_session_production_prepare_material_qualification_v1");
		read_selection = get_fn<GDExtensionInterfaceDeviceSessionProductionReadMaterialSelectionV1>("device_session_production_read_material_selection_v1");
		prepare_recipe = get_fn<GDExtensionInterfaceDeviceSessionProductionPrepareMaterialRecipeV1>("device_session_production_prepare_material_recipe_v1");
		record_mq = get_fn<GDExtensionInterfaceDeviceSessionProductionRecordMaterialQualificationV1>("device_session_production_record_material_qualification_v1");
		read_mq = get_fn<GDExtensionInterfaceDeviceSessionProductionReadMaterialQualificationV1>("device_session_production_read_material_qualification_v1");

		return quote && accept && install_receiver && open && close && requirements && reserve &&
				observe && record_schedule && record_consume && ack_segment && facts_v2 && cancel && release &&
				prepare_upload_v2 && prepare_mq && read_selection && prepare_recipe && record_mq && read_mq;
	}
};

static GDExtensionMaterialQualificationPrepareV1 make_valid_mq_prepare(
		uint64_t p_action_ordinal,
		uint64_t p_material_rid,
		uint64_t p_mesh_rid,
		uint32_t p_generation_mode,
		uint64_t p_expected_generation,
		uint32_t p_payload_bytes) {
	GDExtensionMaterialQualificationPrepareV1 prep = {};
	prep.abi = { 1, sizeof(GDExtensionMaterialQualificationPrepareV1), 0 };
	prep.action_ordinal = p_action_ordinal;
	prep.origin.session = { 1, 2, 3, 4 };
	prep.origin.request_device = { 1, 2 };
	prep.origin.request_serial = 10;
	prep.origin.request_nonce = 77;
	prep.origin.record_generation = 1;
	prep.origin.range_begin = 10;
	prep.origin.range_end = 11;
	prep.origin.occurrence = 1;
	prep.origin.attempt = 1;
	prep.resource_content_version = 1;
	prep.surface_index = 0;
	prep.generation_mode = p_generation_mode;
	prep.material_rid = p_material_rid;
	prep.expected_material_generation = p_expected_generation;
	prep.material_content.bytes[0] = 0x5a;
	prep.candidate_mesh_rid = p_mesh_rid;
	prep.candidate_mesh_present = 1;
	prep.recipe = GDEXTENSION_MATERIAL_QUALIFICATION_RECIPE_CURRENT_RESET_F32_WORLD_OPAQUE_V1;
	prep.target.device = { 1, 2 };
	prep.target.numeric_target_variant = 1;
	if (p_payload_bytes >= 16) {
		prep.target.profile_canonical_offset = 0;
		prep.target.profile_canonical_bytes = 8;
		prep.target.driver_name = { 1, 8, 5, 0 };
		prep.mapping.renderer_contract = { 1, 13, 3, 0 };
	}
	prep.payload_bytes = p_payload_bytes;
	return prep;
}

static bool mq_digest_nonzero(const GDExtensionMaterialQualificationDigestV1 &p_digest) {
	for (uint32_t i = 0; i < 32; ++i) {
		if (p_digest.bytes[i] != 0) {
			return true;
		}
	}
	return false;
}

static bool mq_digest_equal(const GDExtensionMaterialQualificationDigestV1 &p_a, const GDExtensionMaterialQualificationDigestV1 &p_b) {
	return std::memcmp(p_a.bytes, p_b.bytes, 32) == 0;
}

static bool verify_mq_fixed_oracle_shape(const GDExtensionMaterialQualificationFixedOracleV1 &p_oracle) {
	if (p_oracle.abi.abi_version != 1 || p_oracle.abi.struct_size != 512 || p_oracle.abi.reserved != 0) {
		return false;
	}
	if (p_oracle.recipe_version != 1 || p_oracle.verifier_version != 1 ||
			p_oracle.vector_count != 1 || p_oracle.obligation_count != 1 ||
			p_oracle.raw_words != 1024 || p_oracle.raw_bytes != 4096 ||
			p_oracle.metadata_bytes != 1000 || p_oracle.raw_word_offset != 1000 ||
			p_oracle.clear_bits != 0x00000000u || p_oracle.middle_bits != 0x3f000000u ||
			p_oracle.width != 32 || p_oracle.height != 32) {
		return false;
	}
	for (uint32_t y = 0; y < 32; ++y) {
		const uint32_t expected_int = (y >= 9 && y <= 22) ? 0x007ffe00u : 0x00000000u;
		const uint32_t expected_ext = (y < 8 || y > 23) ? 0xffffffffu : 0xff0000ffu;
		if (p_oracle.interior_mask[y] != expected_int || p_oracle.exterior_mask[y] != expected_ext) {
			return false;
		}
		if ((p_oracle.interior_mask[y] & p_oracle.exterior_mask[y]) != 0u) {
			return false;
		}
	}
	return mq_digest_nonzero(p_oracle.recipe) && mq_digest_nonzero(p_oracle.verifier) &&
			mq_digest_nonzero(p_oracle.vector_table) && mq_digest_nonzero(p_oracle.fixture_geometry) &&
			mq_digest_nonzero(p_oracle.camera) && mq_digest_nonzero(p_oracle.instances);
}

static void run_material_qualification_action7_cpu() {
	using namespace GDExtensionDeviceExit;

	// 1. Static & runtime ABI byte-layout and constant verification.
	static_assert(sizeof(GDExtensionMaterialQualificationTextV1) == 16);
	static_assert(sizeof(GDExtensionMaterialQualificationDeviceV1) == 16);
	static_assert(sizeof(GDExtensionMaterialQualificationDigestV1) == 32);
	static_assert(sizeof(GDExtensionMaterialQualificationSessionV1) == 32);
	static_assert(sizeof(GDExtensionMaterialQualificationOriginV1) == 104);
	static_assert(sizeof(GDExtensionMaterialQualificationTargetV1) == 392);
	static_assert(sizeof(GDExtensionMaterialQualificationMappingV1) == 176);
	static_assert(sizeof(GDExtensionMaterialQualificationPrepareV1) == 784);
	static_assert(sizeof(GDExtensionMaterialQualificationSelectionFactsV1) == 232);
	static_assert(sizeof(GDExtensionMaterialQualificationRawDepthV1) == 144);
	static_assert(sizeof(GDExtensionMaterialQualificationResultMetadataV1) == 1000);
	static_assert(sizeof(GDExtensionMaterialQualificationFixedOracleV1) == 512);
	static_assert(sizeof(GDExtensionMaterialQualificationResultMetadataV1) + 1024 * sizeof(uint32_t) == 5096);

	CHECK(GDEXTENSION_PRODUCTION_MATERIAL_QUALIFICATION_V1 == 7);
	CHECK(GDEXTENSION_MATERIAL_QUALIFICATION_READ_CURRENT_V1 == 1);
	CHECK(GDEXTENSION_MATERIAL_QUALIFICATION_EXACT_V1 == 2);
	CHECK(GDEXTENSION_MATERIAL_QUALIFICATION_RECIPE_CURRENT_RESET_F32_WORLD_OPAQUE_V1 == 1);
	CHECK(GDEXTENSION_MATERIAL_QUALIFICATION_PENDING_V1 == 1);
	CHECK(GDEXTENSION_MATERIAL_QUALIFICATION_COMPLETE_V1 == 2);
	CHECK(GDEXTENSION_MATERIAL_QUALIFICATION_FAILED_V1 == 3);
	CHECK(GDEXTENSION_MATERIAL_QUALIFICATION_CANCELLED_V1 == 4);
	CHECK(GDEXTENSION_MATERIAL_QUALIFICATION_UNKNOWN_V1 == 5);
	CHECK(GDEXTENSION_MATERIAL_QUALIFICATION_NEEDS_INVERSE_V1 == 1);
	CHECK(GDEXTENSION_MATERIAL_QUALIFICATION_ZERO_DISPLACEMENT_V1 == 2);
	CHECK(GDEXTENSION_MATERIAL_QUALIFICATION_SOURCE_GATE_KNOWN_MASK_V1 == 0x00000fffu);
	CHECK(GDEXTENSION_MATERIAL_QUALIFICATION_MAX_FACTS_V1 == 8);

	MaterialQualificationAction7Ports ports;
	REQUIRE(ports.load());

	auto *server = Object::cast_to<RenderingServerDefault>(RenderingServer::get_singleton());
	REQUIRE(server != nullptr);
	const auto baseline_counts = singleton_counts();

	{
		struct RecordProbe {
			GDExtensionDeviceSessionResultV1 premature_record = {};
			uint32_t calls = 0;
		} probe;

		SingletonEnrollment fixture;
		fixture.public_port = true;
		REQUIRE(fixture.prepare_device(server) == OK);

		OrdinalPlan pool = {};
		pool.attachment_requests = 4;
		pool.capacity.abi = production_dto_abi<GDExtensionProductionCapacityV1>();
		pool.capacity.requests = 4;
		pool.capacity.suboperations = 8;
		pool.capacity.resources = 4;
		pool.capacity.resource_uses = 4;
		pool.capacity.graph_memberships = 24;
		pool.capacity.return_cells = 4;
		pool.capacity.retained_payload_bytes = 32768;
		{
			MutexLock registry_lock(fixture.registry.mutex);
			MutexLock rd_lock(fixture.rd._thread_safe_);
			REQUIRE(fixture.registry.production_membership_bound_locked(fixture.rd, pool.capacity.graph_memberships, fixture.rd.transfer_worker_pool_max_size, pool.capacity.native_memberships) == OK);
		}

		auto receiver = +[](void *p_data, ProductionPhysicalContextRef p_ctx, GDExtensionProductionRequestRefV1 p_req, const GDExtensionProductionOperationV1 *) {
			auto *self = static_cast<RecordProbe *>(p_data);
			++self->calls;
			auto fn = MaterialQualificationAction7Ports::get_fn<GDExtensionInterfaceDeviceSessionProductionRecordMaterialQualificationV1>("device_session_production_record_material_qualification_v1");
			if (fn) {
				self->premature_record = fn(p_ctx, p_req, 0);
			}
		};

		REQUIRE(fixture.initialize_owner(1, false, false, receiver, &probe, false, 1, false, +[](void *, GDExtensionProductionAttachmentRefV1) {}, false, false, false, false, &pool, nullptr, true) == OK);
		fixture.frame.leave();

		// 2. Action7 request requirements & malformed declaration rejections.
		const GDExtensionProductionActionV2 valid_action = {
			0, 10, 7, 0, GDEXTENSION_PRODUCTION_MATERIAL_QUALIFICATION_V1,
			UINT32_MAX, 0, 0, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX
		};
		GDExtensionProductionActionV2 bad_action = valid_action;
		bad_action.physical_unit = 0;
		GDExtensionProductionMetadataQuoteV1 req_quote = {};
		CHECK(ports.requirements(fixture.attachment, &bad_action, 1, nullptr, 0, nullptr, 0, nullptr, 0, nullptr, 0, 0, 32, &req_quote).native_error == ERR_INVALID_DATA);
		bad_action = valid_action;
		bad_action.readback_slot = 0;
		CHECK(ports.requirements(fixture.attachment, &bad_action, 1, nullptr, 0, nullptr, 0, nullptr, 0, nullptr, 0, 0, 32, &req_quote).native_error == ERR_INVALID_DATA);
		bad_action = valid_action;
		bad_action.use_count = 1;
		CHECK(ports.requirements(fixture.attachment, &bad_action, 1, nullptr, 0, nullptr, 0, nullptr, 0, nullptr, 0, 0, 32, &req_quote).native_error == ERR_INVALID_DATA);

		REQUIRE(ports.requirements(fixture.attachment, &valid_action, 1, nullptr, 0, nullptr, 0, nullptr, 0, nullptr, 0, 0, 32, &req_quote).native_error == OK);
		CHECK(req_quote.capacity.graph_memberships == 4);
		CHECK(req_quote.capacity.retained_payload_bytes == 5096 + 32);

		const GDExtensionProductionOperationV1 op = { production_dto_abi<GDExtensionProductionOperationV1>(), 101, 1, 10, 11 };
		GDExtensionProductionRequestRefV1 req = {};
		REQUIRE(ports.reserve(fixture.attachment, &op, &req_quote.capacity, &valid_action, 1, nullptr, 0, nullptr, 0, nullptr, 0, &req).native_error == OK);

		// Reserve a second non-action7 COPY request to test wrong-action-kind rejection.
		const GDExtensionProductionActionV2 copy_action = {
			0, 10, 1, 0, GDEXTENSION_PRODUCTION_COPY_V1,
			UINT32_MAX, 0, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX
		};
		const GDExtensionProductionResourceV1 upload_res = { GDEXTENSION_PRODUCTION_STORAGE_V1, 0, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, 16, 0 };
		const GDExtensionProductionUseV1 upload_use = { 0, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 16, 0 };
		GDExtensionProductionMetadataQuoteV1 upload_quote = {};
		REQUIRE(ports.requirements(fixture.attachment, &copy_action, 1, &upload_res, 1, &upload_use, 1, nullptr, 0, nullptr, 0, 0, 16, &upload_quote).native_error == OK);
		const GDExtensionProductionOperationV1 upload_op = { production_dto_abi<GDExtensionProductionOperationV1>(), 102, 1, 10, 11 };
		GDExtensionProductionRequestRefV1 upload_req = {};
		REQUIRE(ports.reserve(fixture.attachment, &upload_op, &upload_quote.capacity, &copy_action, 1, &upload_res, 1, &upload_use, 1, nullptr, 0, &upload_req).native_error == OK);

		// 3. Premature read/recipe/record/result calls BEFORE prepare_material_qualification_v1.
		GDExtensionMaterialQualificationSelectionFactsV1 sel = {};
		CHECK(ports.read_selection(req, 0, &sel).native_error == ERR_UNCONFIGURED);
		GDExtensionMaterialQualificationFixedOracleV1 oracle = {};
		CHECK(ports.prepare_recipe(req, 0, &oracle).native_error == ERR_UNCONFIGURED);
		GDExtensionMaterialQualificationResultMetadataV1 meta = {};
		uint32_t raw_words[1024] = {};
		CHECK(ports.read_mq(req, 0, &meta, 1000, raw_words, 1024).native_error == ERR_UNCONFIGURED);
		CHECK(ports.record_mq({ fixture.instance, 1 }, req, 0).native_error == ERR_UNAUTHORIZED);

		// 4. Exhaustive negative validations on prepare_material_qualification_v1.
		const uint8_t payload[16] = { 'p', 'r', 'o', 'f', 'i', 'l', 'e', '0', 'm', 'e', 't', 'a', 'l', 'm', 'o', 'b' };
		const auto valid_prep = make_valid_mq_prepare(0, 0x1001, 0x2002, GDEXTENSION_MATERIAL_QUALIFICATION_READ_CURRENT_V1, 0, 16);

		// Null / misaligned prepare.
		CHECK(ports.prepare_mq(req, nullptr, payload, 16).native_error == ERR_INVALID_PARAMETER);
		alignas(16) uint8_t misaligned_buf[sizeof(GDExtensionMaterialQualificationPrepareV1) + 16] = {};
		std::memcpy(misaligned_buf + 1, &valid_prep, sizeof(valid_prep));
		CHECK(ports.prepare_mq(req, reinterpret_cast<const GDExtensionMaterialQualificationPrepareV1 *>(misaligned_buf + 1), payload, 16).native_error == ERR_INVALID_PARAMETER);

		// Bad ABI / reserved fields.
		{
			auto bad = valid_prep;
			bad.abi.abi_version = 2;
			CHECK(ports.prepare_mq(req, &bad, payload, 16).native_error == ERR_INVALID_PARAMETER);
			bad = valid_prep;
			bad.abi.struct_size = 780;
			CHECK(ports.prepare_mq(req, &bad, payload, 16).native_error == ERR_INVALID_PARAMETER);
			bad = valid_prep;
			bad.abi.reserved = 1;
			CHECK(ports.prepare_mq(req, &bad, payload, 16).native_error == ERR_INVALID_PARAMETER);
			bad = valid_prep;
			bad.reserved = 1;
			CHECK(ports.prepare_mq(req, &bad, payload, 16).native_error == ERR_INVALID_PARAMETER);
			bad = valid_prep;
			bad.target.reserved = 1;
			CHECK(ports.prepare_mq(req, &bad, payload, 16).native_error == ERR_INVALID_PARAMETER);
			bad = valid_prep;
			bad.mapping.reserved = 1;
			CHECK(ports.prepare_mq(req, &bad, payload, 16).native_error == ERR_INVALID_PARAMETER);
		}

		// Null payload when payload_bytes > 0, mismatched payload_bytes, overlapping prepare & payload.
		CHECK(ports.prepare_mq(req, &valid_prep, nullptr, 16).native_error == ERR_INVALID_PARAMETER);
		CHECK(ports.prepare_mq(req, &valid_prep, payload, 8).native_error == ERR_INVALID_PARAMETER);
		CHECK(ports.prepare_mq(req, &valid_prep, reinterpret_cast<const uint8_t *>(&valid_prep), 16).native_error == ERR_INVALID_PARAMETER);

		// Out-of-bounds canonical slice & MQText bounds.
		{
			auto bad = valid_prep;
			bad.target.profile_canonical_offset = 12;
			bad.target.profile_canonical_bytes = 8;
			CHECK(ports.prepare_mq(req, &bad, payload, 16).native_error == ERR_INVALID_PARAMETER);

			bad = valid_prep;
			bad.target.driver_name = { 1, 14, 4, 0 };
			CHECK(ports.prepare_mq(req, &bad, payload, 16).native_error == ERR_INVALID_PARAMETER);

			bad = valid_prep;
			bad.target.driver_name = { 0, 4, 0, 0 };
			CHECK(ports.prepare_mq(req, &bad, payload, 16).native_error == ERR_INVALID_PARAMETER);

			bad = valid_prep;
			bad.target.driver_name = { 2, 0, 0, 0 };
			CHECK(ports.prepare_mq(req, &bad, payload, 16).native_error == ERR_INVALID_PARAMETER);

			bad = valid_prep;
			bad.target.driver_name = { 1, 8, 4, 9 };
			CHECK(ports.prepare_mq(req, &bad, payload, 16).native_error == ERR_INVALID_PARAMETER);
		}

		// Invalid candidate mesh / material RID / recipe / generation_mode combinations.
		{
			auto bad = valid_prep;
			bad.candidate_mesh_present = 0;
			CHECK(ports.prepare_mq(req, &bad, payload, 16).native_error == ERR_INVALID_DATA);

			bad = valid_prep;
			bad.candidate_mesh_rid = 0;
			CHECK(ports.prepare_mq(req, &bad, payload, 16).native_error == ERR_INVALID_DATA);

			bad = valid_prep;
			bad.material_rid = 0;
			CHECK(ports.prepare_mq(req, &bad, payload, 16).native_error == ERR_INVALID_DATA);

			bad = valid_prep;
			bad.recipe = 2;
			CHECK(ports.prepare_mq(req, &bad, payload, 16).native_error == ERR_INVALID_DATA);

			bad = valid_prep;
			bad.generation_mode = 0;
			CHECK(ports.prepare_mq(req, &bad, payload, 16).native_error == ERR_INVALID_DATA);

			bad = valid_prep;
			bad.generation_mode = GDEXTENSION_MATERIAL_QUALIFICATION_READ_CURRENT_V1;
			bad.expected_material_generation = 5;
			CHECK(ports.prepare_mq(req, &bad, payload, 16).native_error == ERR_INVALID_DATA);

			bad = valid_prep;
			bad.generation_mode = GDEXTENSION_MATERIAL_QUALIFICATION_EXACT_V1;
			bad.expected_material_generation = 0;
			CHECK(ports.prepare_mq(req, &bad, payload, 16).native_error == ERR_INVALID_DATA);

			bad = valid_prep;
			bad.generation_mode = GDEXTENSION_MATERIAL_QUALIFICATION_EXACT_V1;
			bad.expected_material_generation = UINT64_MAX;
			CHECK(ports.prepare_mq(req, &bad, payload, 16).native_error == ERR_INVALID_DATA);
		}

		// Wrong action_ordinal, wrong action kind (UPLOAD), and payload exceeding retained_payload_bytes (32).
		{
			auto bad = valid_prep;
			bad.action_ordinal = 1;
			CHECK(ports.prepare_mq(req, &bad, payload, 16).native_error == ERR_INVALID_DATA);

			CHECK(ports.prepare_mq(upload_req, &valid_prep, payload, 16).native_error == ERR_INVALID_DATA);

			uint8_t big_payload[48] = {};
			auto over_budget = valid_prep;
			over_budget.payload_bytes = 48;
			CHECK(ports.prepare_mq(req, &over_budget, big_payload, 48).native_error == ERR_OUT_OF_MEMORY);
		}

		// 5. Valid prepare, idempotent exact replay, and mutated replay rejection.
		REQUIRE(ports.prepare_mq(req, &valid_prep, payload, 16).native_error == OK);
		CHECK(ports.prepare_mq(req, &valid_prep, payload, 16).native_error == OK);

		{
			auto mutated_hdr = valid_prep;
			mutated_hdr.resource_content_version = 2;
			CHECK(ports.prepare_mq(req, &mutated_hdr, payload, 16).native_error == ERR_ALREADY_EXISTS);

			uint8_t mutated_payload[16];
			std::memcpy(mutated_payload, payload, 16);
			mutated_payload[0] ^= 0xff;
			CHECK(ports.prepare_mq(req, &valid_prep, mutated_payload, 16).native_error == ERR_ALREADY_EXISTS);
		}

		// 6. Selection read before RT OBSERVE returns PENDING; premature recipe/read_mq/record_mq reject cleanly.
		CHECK(ports.read_selection(req, 0, nullptr).native_error == ERR_INVALID_PARAMETER);
		CHECK(ports.read_selection(req, 1, &sel).native_error == ERR_INVALID_DATA);
		REQUIRE(ports.read_selection(req, 0, &sel).native_error == OK);
		CHECK(sel.state == GDEXTENSION_MATERIAL_QUALIFICATION_PENDING_V1);
		CHECK(sel.material_rid == 0x1001);
		CHECK(sel.candidate_mesh_rid == 0x2002);

		CHECK(ports.prepare_recipe(req, 0, nullptr).native_error == ERR_INVALID_PARAMETER);
		CHECK(ports.prepare_recipe(req, 0, &oracle).native_error == ERR_BUSY);

		// Schedule RECORD before action7 recipe is prepared -> freeze refuses with ERR_UNCONFIGURED!
		uint64_t record_ctrl = 0;
		CHECK(ports.record_schedule(req, &record_ctrl) == ERR_UNCONFIGURED);

		// Output buffer size / alignment / overlap rejections on read_material_qualification_v1.
		CHECK(ports.read_mq(req, 0, nullptr, 1000, raw_words, 1024).native_error == ERR_INVALID_PARAMETER);
		CHECK(ports.read_mq(req, 0, &meta, 999, raw_words, 1024).native_error == ERR_INVALID_PARAMETER);
		CHECK(ports.read_mq(req, 0, &meta, 1000, nullptr, 1024).native_error == ERR_INVALID_PARAMETER);
		CHECK(ports.read_mq(req, 0, &meta, 1000, raw_words, 1023).native_error == ERR_INVALID_PARAMETER);
		alignas(8) uint8_t overlap_arena[5120] = {};
		CHECK(ports.read_mq(
					  req,
					  0,
					  reinterpret_cast<GDExtensionMaterialQualificationResultMetadataV1 *>(overlap_arena),
					  1000,
					  reinterpret_cast<uint32_t *>(overlap_arena + 512),
					  1024)
						.native_error == ERR_INVALID_PARAMETER);
		CHECK(ports.read_mq(req, 0, &meta, 1000, raw_words, 1024).native_error == ERR_UNCONFIGURED);

		// 7. Cancel before OBSERVE transitions selection to CANCELLED and releases cleanly with zero draws.
		REQUIRE(ports.cancel(req).native_error == OK);
		REQUIRE(ports.read_selection(req, 0, &sel).native_error == OK);
		CHECK(sel.state == GDEXTENSION_MATERIAL_QUALIFICATION_CANCELLED_V1);

		const GDExtensionProductionServiceBudgetV1 budget = {
			production_dto_abi<GDExtensionProductionServiceBudgetV1>(),
			OS::get_singleton()->get_ticks_usec() + 10000000,
			4, 8, 16, 0
		};
		GDExtensionProductionServiceBudgetV1 rem = {};
		GDExtensionProductionReportV2 rel_report = {};
		const Error rel1 = Error(ports.release(req, &budget, &rem, &rel_report).native_error);
		CHECK((rel1 == OK || rel1 == ERR_BUSY));
		if (rel_report.service_pending) {
			server->sync();
			REQUIRE(ports.release(req, &budget, &rem, &rel_report).native_error == OK);
		}
		CHECK(rel_report.service_pending == 0);

		REQUIRE(ports.cancel(upload_req).native_error == OK);
		const Error rel2 = Error(ports.release(upload_req, &budget, &rem, &rel_report).native_error);
		CHECK((rel2 == OK || rel2 == ERR_BUSY));
		if (rel_report.service_pending) {
			server->sync();
			REQUIRE(ports.release(upload_req, &budget, &rem, &rel_report).native_error == OK);
		}
		CHECK(rel_report.service_pending == 0);

		// 8. Mixed [UPLOAD_V1, MATERIAL_QUALIFICATION_V1] enforces shared r->payload_used against retained_payload_bytes.
		{
			fixture.rd.upload_staging_buffers.block_size = fixture.rd.download_staging_buffers.block_size = 4096;
			const GDExtensionProductionActionV2 mixed_actions[2] = {
				{ 0, 10, 1, 0, GDEXTENSION_PRODUCTION_UPLOAD_V1, UINT32_MAX, 0, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX },
				{ 1, 11, 7, 0, GDEXTENSION_PRODUCTION_MATERIAL_QUALIFICATION_V1, UINT32_MAX, 1, 0, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX },
			};
			const GDExtensionProductionResourceV1 mixed_res = {
				GDEXTENSION_PRODUCTION_STORAGE_V1, 0, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, 64, 0
			};
			const GDExtensionProductionUseV1 mixed_use = {
				0, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 64, 0
			};
			uint8_t upload_bytes[64];
			std::memset(upload_bytes, 0xa5, sizeof(upload_bytes));
			GDExtensionProductionUploadV2 upload_dto = {};
			upload_dto.abi = production_dto_abi<GDExtensionProductionUploadV2>();
			upload_dto.action_ordinal = 0;
			upload_dto.canonical_suboperation = 10;
			upload_dto.source_recipe_substep = 1;
			upload_dto.destination_slot = 0;
			upload_dto.destination_offset = 0;
			upload_dto.bytes = 64;

			GDExtensionProductionMetadataQuoteV1 mixed_quote = {};
			REQUIRE(ports.requirements(fixture.attachment, mixed_actions, 2, &mixed_res, 1, &mixed_use, 1, &upload_dto, 1, nullptr, 0, 0, 64 + 16, &mixed_quote).native_error == OK);
			CHECK(mixed_quote.capacity.retained_payload_bytes == 5096 + 64 + 16);

			auto mixed_mq_prep = make_valid_mq_prepare(1, 0x1001, 0x2002, GDEXTENSION_MATERIAL_QUALIFICATION_READ_CURRENT_V1, 0, 16);
			mixed_mq_prep.origin.range_begin = 10;
			mixed_mq_prep.origin.range_end = 12;

			// 8a. Tight retained_payload_bytes (5112 = 5096 + 16): UPLOAD (64B) succeeds, then MQ (needs 5112B) rejects with ERR_OUT_OF_MEMORY.
			{
				GDExtensionProductionCapacityV1 tight_cap = mixed_quote.capacity;
				tight_cap.retained_payload_bytes = 5096 + 16;
				const GDExtensionProductionOperationV1 tight_op = { production_dto_abi<GDExtensionProductionOperationV1>(), 103, 1, 10, 12 };
				GDExtensionProductionRequestRefV1 tight_req = {};
				REQUIRE(ports.reserve(fixture.attachment, &tight_op, &tight_cap, mixed_actions, 2, &mixed_res, 1, &mixed_use, 1, nullptr, 0, &tight_req).native_error == OK);
				REQUIRE(ports.prepare_upload_v2(tight_req, &upload_dto, upload_bytes, 64).native_error == OK);
				CHECK(ports.prepare_mq(tight_req, &mixed_mq_prep, payload, 16).native_error == ERR_OUT_OF_MEMORY);

				REQUIRE(ports.cancel(tight_req).native_error == OK);
				const Error r_tight = Error(ports.release(tight_req, &budget, &rem, &rel_report).native_error);
				CHECK((r_tight == OK || r_tight == ERR_BUSY));
				if (rel_report.service_pending) {
					server->sync();
					REQUIRE(ports.release(tight_req, &budget, &rem, &rel_report).native_error == OK);
				}
				CHECK(rel_report.service_pending == 0);
			}

			// 8b. Reverse order with tight retained_payload_bytes (5144 = 5112 + 32): MQ (5112B) succeeds, then UPLOAD (64B > 32B left) rejects!
			{
				GDExtensionProductionCapacityV1 tight_cap2 = mixed_quote.capacity;
				tight_cap2.retained_payload_bytes = 5096 + 16 + 32;
				const GDExtensionProductionOperationV1 tight_op2 = { production_dto_abi<GDExtensionProductionOperationV1>(), 104, 1, 10, 12 };
				GDExtensionProductionRequestRefV1 tight_req2 = {};
				REQUIRE(ports.reserve(fixture.attachment, &tight_op2, &tight_cap2, mixed_actions, 2, &mixed_res, 1, &mixed_use, 1, nullptr, 0, &tight_req2).native_error == OK);
				REQUIRE(ports.prepare_mq(tight_req2, &mixed_mq_prep, payload, 16).native_error == OK);
				CHECK(ports.prepare_upload_v2(tight_req2, &upload_dto, upload_bytes, 64).native_error == ERR_PARAMETER_RANGE_ERROR);

				REQUIRE(ports.cancel(tight_req2).native_error == OK);
				const Error r_tight2 = Error(ports.release(tight_req2, &budget, &rem, &rel_report).native_error);
				CHECK((r_tight2 == OK || r_tight2 == ERR_BUSY));
				if (rel_report.service_pending) {
					server->sync();
					REQUIRE(ports.release(tight_req2, &budget, &rem, &rel_report).native_error == OK);
				}
				CHECK(rel_report.service_pending == 0);
			}

			// 8c. Exact-fit quote capacity (5176 = 5096 + 64 + 16): both UPLOAD and MQ succeed, and exact replays do not double-charge!
			{
				const GDExtensionProductionOperationV1 fit_op = { production_dto_abi<GDExtensionProductionOperationV1>(), 105, 1, 10, 12 };
				GDExtensionProductionRequestRefV1 fit_req = {};
				REQUIRE(ports.reserve(fixture.attachment, &fit_op, &mixed_quote.capacity, mixed_actions, 2, &mixed_res, 1, &mixed_use, 1, nullptr, 0, &fit_req).native_error == OK);
				REQUIRE(ports.prepare_upload_v2(fit_req, &upload_dto, upload_bytes, 64).native_error == OK);
				REQUIRE(ports.prepare_mq(fit_req, &mixed_mq_prep, payload, 16).native_error == OK);
				CHECK(ports.prepare_mq(fit_req, &mixed_mq_prep, payload, 16).native_error == OK);
				CHECK(ports.prepare_upload_v2(fit_req, &upload_dto, upload_bytes, 64).native_error == OK);

				REQUIRE(ports.cancel(fit_req).native_error == OK);
				const Error r_fit = Error(ports.release(fit_req, &budget, &rem, &rel_report).native_error);
				CHECK((r_fit == OK || r_fit == ERR_BUSY));
				if (rel_report.service_pending) {
					server->sync();
					REQUIRE(ports.release(fit_req, &budget, &rem, &rel_report).native_error == OK);
				}
				CHECK(rel_report.service_pending == 0);
			}

			// Non-RD / Dummy backend refusal check: when RendererSceneRenderRD::get_singleton() is absent or backend == 0,
			// selection and recipe preparation fail closed deterministically with ERR_UNAVAILABLE without crash.
			if (RendererSceneRenderRD::get_singleton() == nullptr || RendererSceneRenderRD::get_singleton()->get_native_material_recipe_backend() == 0) {
				GDExtensionProductionRequestRefV1 non_rd_req = {};
				const GDExtensionProductionOperationV1 non_rd_op = { production_dto_abi<GDExtensionProductionOperationV1>(), 188, 1, 10, 11 };
				REQUIRE(ports.reserve(fixture.attachment, &non_rd_op, &req_quote.capacity, &valid_action, 1, nullptr, 0, nullptr, 0, nullptr, 0, &non_rd_req).native_error == OK);
				REQUIRE(ports.prepare_mq(non_rd_req, &valid_prep, payload, 16).native_error == OK);
				GDExtensionProductionObserveResultV3 obs = {};
				ports.observe(non_rd_req, &budget, &obs);
				server->sync();
				ports.observe(non_rd_req, &budget, &obs);
				GDExtensionMaterialQualificationSelectionFactsV1 non_rd_sel = {};
				REQUIRE(ports.read_selection(non_rd_req, 0, &non_rd_sel).native_error == OK);
				CHECK(non_rd_sel.state == GDEXTENSION_MATERIAL_QUALIFICATION_FAILED_V1);
				CHECK(non_rd_sel.native_error == ERR_UNAVAILABLE);
				GDExtensionMaterialQualificationFixedOracleV1 non_rd_oracle = {};
				CHECK(ports.prepare_recipe(non_rd_req, 0, &non_rd_oracle).native_error == ERR_UNAVAILABLE);
				REQUIRE(ports.cancel(non_rd_req).native_error == OK);
				const Error r_non_rd = Error(ports.release(non_rd_req, &budget, &rem, &rel_report).native_error);
				CHECK((r_non_rd == OK || r_non_rd == ERR_BUSY));
				if (rel_report.service_pending) {
					server->sync();
					REQUIRE(ports.release(non_rd_req, &budget, &rem, &rel_report).native_error == OK);
				}
			}
		}

		fixture.cleanup_owner();
	}

	const auto after_counts = singleton_counts();
	CHECK(after_counts.records == baseline_counts.records);
	CHECK(after_counts.views == baseline_counts.views);
	CHECK(after_counts.devices == baseline_counts.devices);
	CHECK(after_counts.device_refs == baseline_counts.device_refs);
	CHECK(after_counts.attachments == baseline_counts.attachments);
	CHECK(after_counts.private_refs == baseline_counts.private_refs);
	CHECK(after_counts.external_refs == baseline_counts.external_refs);
}

struct LiveMetalMaterialQualificationHarness {
	SingletonEnrollment enrollment;
	MaterialQualificationAction7Ports ports;
	RenderingServer *server = nullptr;
	RenderingDevice *device = nullptr;
	uint64_t initial_rd_draws = 0;
	Dictionary checks;
	Vector<String> failures;

	GDExtensionProductionRequestRefV1 active_record_req = {};
	GDExtensionDeviceSessionResultV1 record_call_result = { ERR_UNCONFIGURED, 0, 0, 0 };
	GDExtensionDeviceSessionResultV1 duplicate_record_result = { ERR_UNCONFIGURED, 0, 0, 0 };
	GDExtensionProductionRequestRefV1 active_record_req_b = {};
	GDExtensionDeviceSessionResultV1 record_call_result_b = { ERR_UNCONFIGURED, 0, 0, 0 };
	GDExtensionDeviceSessionResultV1 duplicate_record_result_b = { ERR_UNCONFIGURED, 0, 0, 0 };
	uint32_t record_callbacks = 0;

	Dictionary diagnostics;

	void record_check(const String &p_name, bool p_ok) {
		checks[p_name] = p_ok;
		if (!p_ok) {
			failures.push_back(p_name);
		}
	}

	static void on_record(void *p_userdata, GDExtensionDeviceExit::ProductionPhysicalContextRef p_ctx, GDExtensionProductionRequestRefV1 p_req, const GDExtensionProductionOperationV1 *) {
		auto &self = *static_cast<LiveMetalMaterialQualificationHarness *>(p_userdata);
		++self.record_callbacks;
		if (p_req.handle_serial == self.active_record_req.handle_serial && p_req.registry_instance == self.active_record_req.registry_instance && self.active_record_req.handle_serial != 0) {
			self.record_call_result = self.ports.record_mq(p_ctx, p_req, 0);
			if (self.record_call_result.native_error == OK) {
				self.duplicate_record_result = self.ports.record_mq(p_ctx, p_req, 0);
			}
			return;
		}
		if (p_req.handle_serial == self.active_record_req_b.handle_serial && p_req.registry_instance == self.active_record_req_b.registry_instance && self.active_record_req_b.handle_serial != 0) {
			self.record_call_result_b = self.ports.record_mq(p_ctx, p_req, 0);
			if (self.record_call_result_b.native_error == OK) {
				self.duplicate_record_result_b = self.ports.record_mq(p_ctx, p_req, 0);
			}
			return;
		}
		self.record_call_result = { ERR_UNAUTHORIZED, 99, 0, 0 };
	}

	GDExtensionProductionServiceBudgetV1 make_budget() const {
		return {
			GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionServiceBudgetV1>(),
			OS::get_singleton()->get_ticks_usec() + 15000000,
			8, 32, 64, 0
		};
	}

	GDExtensionProductionServiceBudgetV1 make_tight_budget(uint32_t p_passes = 2, uint32_t p_receipts = 6) const {
		return {
			GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionServiceBudgetV1>(),
			OS::get_singleton()->get_ticks_usec() + 15000000,
			1, p_receipts, p_passes, 0
		};
	}

	bool verify_depth_oracle_words(const String &p_label, const GDExtensionMaterialQualificationFixedOracleV1 &p_oracle, const uint32_t *p_raw_words) {
		uint32_t interior_checked = 0;
		uint32_t exterior_checked = 0;
		uint32_t mismatches = 0;
		uint32_t first_int_bad = 0;
		uint32_t first_ext_bad = 0;
		for (uint32_t y = 0; y < 32; ++y) {
			const uint32_t int_mask = p_oracle.interior_mask[y];
			const uint32_t ext_mask = p_oracle.exterior_mask[y];
			for (uint32_t x = 0; x < 32; ++x) {
				const uint32_t word = p_raw_words[y * 32 + x];
				if ((int_mask >> x) & 1u) {
					++interior_checked;
					if (word != p_oracle.middle_bits) {
						if (!first_int_bad) {
							first_int_bad = word;
						}
						++mismatches;
					}
				} else if ((ext_mask >> x) & 1u) {
					++exterior_checked;
					if (word != p_oracle.clear_bits) {
						if (!first_ext_bad) {
							first_ext_bad = word;
						}
						++mismatches;
					}
				}
			}
		}
		if (mismatches != 0) {
			Dictionary d;
			d["mismatches"] = mismatches;
			d["center_word"] = p_raw_words[16 * 32 + 16];
			d["corner_word"] = p_raw_words[0];
			d["first_int_bad"] = first_int_bad;
			d["first_ext_bad"] = first_ext_bad;
			diagnostics[p_label + ".depth_mismatch"] = d;
		}
		const bool exact = (interior_checked == 196 && exterior_checked == 768 && mismatches == 0);
		record_check(p_label + ".depth_oracle_exact_bits", exact);
		return exact;
	}

	Error reserve_action7_request(uint64_t p_nonce, uint32_t p_payload_bytes, GDExtensionProductionRequestRefV1 &r_req) {
		const GDExtensionProductionActionV2 action = {
			0, 10, 7, 0, GDEXTENSION_PRODUCTION_MATERIAL_QUALIFICATION_V1,
			UINT32_MAX, 0, 0, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX
		};
		GDExtensionProductionMetadataQuoteV1 quote = {};
		for (uint32_t tries = 0; tries < 64; ++tries) {
			const Error q_err = Error(ports.requirements(enrollment.attachment, &action, 1, nullptr, 0, nullptr, 0, nullptr, 0, nullptr, 0, 0, p_payload_bytes, &quote).native_error);
			if (q_err == OK) {
				break;
			}
			if (q_err != ERR_BUSY) {
				return q_err;
			}
			server->sync();
			OS::get_singleton()->delay_usec(500);
		}
		const GDExtensionProductionOperationV1 op = {
			GDExtensionDeviceExit::production_dto_abi<GDExtensionProductionOperationV1>(),
			p_nonce, 1, 10, 11
		};
		for (uint32_t tries = 0; tries < 64; ++tries) {
			const Error r_err = Error(ports.reserve(enrollment.attachment, &op, &quote.capacity, &action, 1, nullptr, 0, nullptr, 0, nullptr, 0, &r_req).native_error);
			if (r_err == OK) {
				return OK;
			}
			if (r_err != ERR_BUSY) {
				return r_err;
			}
			server->sync();
			OS::get_singleton()->delay_usec(500);
		}
		return ERR_BUSY;
	}

	Error pump_single_observe(GDExtensionProductionRequestRefV1 p_req, GDExtensionProductionObserveResultV3 &r_observed) {
		const auto budget = make_budget();
		Error err = Error(ports.observe(p_req, &budget, &r_observed).native_error);
		if (err != OK && err != ERR_BUSY) {
			return err;
		}
		server->sync();
		const auto consume_budget = make_budget();
		err = Error(ports.observe(p_req, &consume_budget, &r_observed).native_error);
		if (err != OK && err != ERR_BUSY) {
			return err;
		}
		if (err == ERR_BUSY || r_observed.report.service_pending != 0) {
			OS::get_singleton()->delay_usec(500);
		}
		return OK;
	}

	bool cancel_and_release_no_draw(const String &p_label, GDExtensionProductionRequestRefV1 p_req, bool p_already_cancelled = false) {
		const uint64_t draws_before = Engine::get_singleton()->get_frames_drawn();
		const uint64_t rd_draws_before = device ? device->get_frames_drawn() : 0;
		if (!p_already_cancelled) {
			const Error c_err = Error(ports.cancel(p_req).native_error);
			record_check(p_label + ".cancel_ok", c_err == OK);
			if (c_err != OK) {
				return false;
			}
		}
		GDExtensionProductionReportV2 report = {};
		bool reclaimed = false;
		for (uint32_t turn = 0; turn < 256; ++turn) {
			const auto budget = make_budget();
			GDExtensionProductionServiceBudgetV1 rem = {};
			const Error r_err = Error(ports.release(p_req, &budget, &rem, &report).native_error);
			if (r_err != OK && r_err != ERR_BUSY) {
				record_check(p_label + ".release_error_clean", false);
				return false;
			}
			if (!report.service_pending && r_err == OK) {
				reclaimed = true;
				break;
			}
			server->sync();
			const auto consume_budget = make_budget();
			const Error c_err = Error(ports.release(p_req, &consume_budget, &rem, &report).native_error);
			if (c_err != OK && c_err != ERR_BUSY) {
				record_check(p_label + ".release_error_clean", false);
				return false;
			}
			if (!report.service_pending && c_err == OK) {
				reclaimed = true;
				break;
			}
			OS::get_singleton()->delay_usec(500);
		}
		const uint64_t draws_after = Engine::get_singleton()->get_frames_drawn();
		const uint64_t rd_draws_after = device ? device->get_frames_drawn() : 0;
		record_check(p_label + ".reclaimed", reclaimed && report.service_pending == 0 && report.physical_returns_pending == 0);
		record_check(p_label + ".zero_following_draws", draws_before == 0 && draws_after == 0);
		record_check(p_label + ".rd_frames_drawn_unchanged_on_release", rd_draws_before == initial_rd_draws && rd_draws_after == rd_draws_before);
		return reclaimed && draws_after == 0 && rd_draws_after == rd_draws_before;
	}

	bool run_positive_qualification(
			const String &p_label,
			uint64_t p_nonce,
			RID p_material,
			RID p_mesh,
			uint32_t p_generation_mode,
			uint64_t p_expected_generation) {
		const uint64_t rd_draws_before = device ? device->get_frames_drawn() : 0;
		GDExtensionProductionRequestRefV1 req = {};
		const uint8_t payload[16] = { 'c', 'a', 'n', 'o', 'n', '0', '0', '1', 'm', 'e', 't', 'a', 'l', 'm', 'q', '7' };
		const Error res_err = reserve_action7_request(p_nonce, sizeof(payload), req);
		record_check(p_label + ".reserve", res_err == OK);
		if (res_err != OK) {
			return false;
		}

		const auto prep = make_valid_mq_prepare(0, p_material.get_id(), p_mesh.get_id(), p_generation_mode, p_expected_generation, sizeof(payload));
		const Error prep_err = Error(ports.prepare_mq(req, &prep, payload, sizeof(payload)).native_error);
		record_check(p_label + ".prepare_mq", prep_err == OK);
		if (prep_err != OK) {
			cancel_and_release_no_draw(p_label, req);
			return false;
		}

		GDExtensionMaterialQualificationSelectionFactsV1 sel = {};
		record_check(p_label + ".initial_pending", ports.read_selection(req, 0, &sel).native_error == OK && sel.state == GDEXTENSION_MATERIAL_QUALIFICATION_PENDING_V1);

		// Drive Phase A (RT Selection + Shader Compilation/Warmup) with zero scene draws.
		bool selection_complete = false;
		for (uint32_t turn = 0; turn < 128; ++turn) {
			GDExtensionProductionObserveResultV3 obs = {};
			if (pump_single_observe(req, obs) != OK) {
				break;
			}
			if (ports.read_selection(req, 0, &sel).native_error == OK && sel.state != GDEXTENSION_MATERIAL_QUALIFICATION_PENDING_V1) {
				selection_complete = (sel.state == GDEXTENSION_MATERIAL_QUALIFICATION_COMPLETE_V1);
				break;
			}
		}
		record_check(p_label + ".selection_complete", selection_complete);
		record_check(p_label + ".selection_gates_clean",
				sel.material_generation != 0 &&
						sel.shader_version_rid != 0 &&
						sel.candidate_mesh_revision != 0 &&
						sel.compiler_known == 3 &&
						(sel.compiler_values & GDEXTENSION_MATERIAL_QUALIFICATION_ZERO_DISPLACEMENT_V1) != 0 &&
						sel.source_gate_known == GDEXTENSION_MATERIAL_QUALIFICATION_SOURCE_GATE_KNOWN_MASK_V1 &&
						sel.source_gate_rejections == 0 &&
						sel.candidate_geometry_profile == GDEXTENSION_MATERIAL_QUALIFICATION_GEOMETRY_STATIC_FINITE_SURFACE_V1 &&
						sel.candidate_primitive == uint32_t(RSE::PRIMITIVE_TRIANGLES));
		if (!selection_complete) {
			Dictionary d;
			d["state"] = sel.state;
			d["native_error"] = sel.native_error;
			d["source_gate_known"] = sel.source_gate_known;
			d["source_gate_rejections"] = sel.source_gate_rejections;
			diagnostics[p_label + ".selection"] = d;
			cancel_and_release_no_draw(p_label, req);
			return false;
		}

		// Drive Phase B (RT Recipe Preparation + Fixed Oracle V1).
		GDExtensionMaterialQualificationFixedOracleV1 oracle = {};
		const Error first_recipe = Error(ports.prepare_recipe(req, 0, &oracle).native_error);
		record_check(p_label + ".recipe_first_busy_and_oracle_valid", first_recipe == ERR_BUSY && verify_mq_fixed_oracle_shape(oracle));

		bool recipe_ready = false;
		Error last_rec_err = first_recipe;
		for (uint32_t turn = 0; turn < 128; ++turn) {
			GDExtensionProductionObserveResultV3 obs = {};
			const Error pump_err = pump_single_observe(req, obs);
			const Error rec_err = Error(ports.prepare_recipe(req, 0, &oracle).native_error);
			last_rec_err = rec_err;
			if (rec_err == OK) {
				recipe_ready = verify_mq_fixed_oracle_shape(oracle);
				break;
			}
			if (rec_err != ERR_BUSY || pump_err != OK) {
				break;
			}
		}
		record_check(p_label + ".recipe_ready", recipe_ready);
		if (!recipe_ready) {
			diagnostics[p_label + ".recipe_err"] = int64_t(last_rec_err);
			cancel_and_release_no_draw(p_label, req);
			return false;
		}

		// Schedule & execute RECORD on RT via command_queue FIFO (zero scene draws).
		active_record_req = req;
		record_call_result = { ERR_UNCONFIGURED, 0, 0, 0 };
		duplicate_record_result = { ERR_UNCONFIGURED, 0, 0, 0 };
		const uint32_t callbacks_before = record_callbacks;

		uint64_t record_ctrl = 0;
		const Error sched_err = Error(ports.record_schedule(req, &record_ctrl));
		record_check(p_label + ".record_schedule", sched_err == OK && record_ctrl != 0);
		if (sched_err != OK) {
			cancel_and_release_no_draw(p_label, req);
			return false;
		}
		server->sync();

		GDExtensionProductionRecordReportV2 rec_receipt = {};
		const Error cons_err = Error(ports.record_consume(req, record_ctrl, &rec_receipt).native_error);
		record_check(p_label + ".record_consume",
				cons_err == OK &&
						rec_receipt.result.native_error == OK &&
						record_callbacks == callbacks_before + 1 &&
						record_call_result.native_error == OK &&
						duplicate_record_result.native_error == ERR_ALREADY_EXISTS);
		if (cons_err != OK || record_call_result.native_error != OK) {
			cancel_and_release_no_draw(p_label, req);
			return false;
		}
		if (rec_receipt.flags & GDEXTENSION_PRODUCTION_RECORD_HAS_SEGMENT_V1) {
			GDExtensionDeviceExit::CallbackScope owner_entry(enrollment.reference);
			const Error ack_err = owner_entry.get_error() == OK
					? Error(ports.ack_segment(req, rec_receipt.segment.segment, rec_receipt.segment.segment_revision).native_error)
					: owner_entry.get_error();
			record_check(p_label + ".ack_segment", ack_err == OK);
			if (ack_err != OK) {
				cancel_and_release_no_draw(p_label, req);
				return false;
			}
		}

		// Before GPU submission/completion, read_material_qualification_v1 must return ERR_BUSY without partial writes.
		GDExtensionMaterialQualificationResultMetadataV1 meta = {};
		uint32_t raw_words[1024];
		for (uint32_t i = 0; i < 1024; ++i) {
			raw_words[i] = 0xdeadbeefu;
		}
		const Error early_read = Error(ports.read_mq(req, 0, &meta, 1000, raw_words, 1024).native_error);
		record_check(p_label + ".early_read_busy_atomic", early_read == ERR_BUSY && raw_words[0] == 0xdeadbeefu && meta.state == 0);

		// Drive K01 graph seal, Metal command buffer execution, and async D32_SFLOAT readback via paid OBSERVE (zero scene draws!).
		bool gpu_complete = false;
		uint32_t final_fact_count = 0;
		uint64_t final_revision = 0;
		GDExtensionProductionReportV2 last_report = {};
		int32_t last_read_mq_err = -1;
		for (uint32_t turn = 0; turn < 256; ++turn) {
			GDExtensionProductionObserveResultV3 obs = {};
			if (pump_single_observe(req, obs) != OK) {
				break;
			}
			last_report = obs.report;
			if (obs.report.cause.native_error == OK &&
					obs.report.ready_readbacks >= 1 &&
					obs.report.known_obligations > 0 &&
					obs.report.complete_obligations == obs.report.known_obligations &&
					obs.report.possible_obligations == 0) {
				final_fact_count = obs.report.fact_count;
				final_revision = obs.report.revision;
				const auto read_res = ports.read_mq(req, 0, &meta, 1000, raw_words, 1024);
				last_read_mq_err = read_res.native_error;
				if (read_res.native_error == OK) {
					gpu_complete = true;
					break;
				}
				if (read_res.native_error != ERR_BUSY) {
					break;
				}
			} else if (obs.report.cause.native_error != OK && obs.report.cause.native_error != ERR_BUSY) {
				break;
			}
			OS::get_singleton()->delay_usec(500);
		}
		record_check(p_label + ".gpu_complete", gpu_complete);
		record_check(p_label + ".fact_count_within_frozen_8", final_fact_count >= 1 && final_fact_count <= GDEXTENSION_MATERIAL_QUALIFICATION_MAX_FACTS_V1);
		if (!gpu_complete) {
			Dictionary d;
			d["cause_err"] = last_report.cause.native_error;
			d["cause_reason"] = last_report.cause.reason;
			d["fact_count"] = last_report.fact_count;
			d["ready_readbacks"] = last_report.ready_readbacks;
			d["known_obligations"] = last_report.known_obligations;
			d["complete_obligations"] = last_report.complete_obligations;
			d["possible_obligations"] = last_report.possible_obligations;
			d["read_mq_err"] = last_read_mq_err;
			diagnostics[p_label + ".observe_last"] = d;
		} else {
			GDExtensionProductionFactV2 facts[GDEXTENSION_MATERIAL_QUALIFICATION_MAX_FACTS_V1] = {};
			uint32_t written = 0;
			const Error facts_err = Error(ports.facts_v2(req, final_revision, 0, facts, GDEXTENSION_MATERIAL_QUALIFICATION_MAX_FACTS_V1, &written).native_error);
			record_check(p_label + ".facts_v2_valid", facts_err == OK && written == final_fact_count);
		}

		if (gpu_complete) {
			record_check(p_label + ".metadata_valid",
					meta.abi.abi_version == 1 &&
							meta.abi.struct_size == 1000 &&
							meta.state == GDEXTENSION_MATERIAL_QUALIFICATION_COMPLETE_V1 &&
							meta.native_shader_rid != 0 &&
							meta.pipeline_rid != 0 &&
							meta.render_target_rid != 0 &&
							meta.actual_samples == 1 &&
							meta.bind_append_serial != 0 &&
							meta.draw_append_serial > meta.bind_append_serial &&
							meta.list_end_append_serial > meta.draw_append_serial &&
							meta.depth.width == 32 &&
							meta.depth.height == 32 &&
							meta.depth.row_bytes == 128 &&
							meta.depth.raw_offset == 1000 &&
							meta.depth.raw_bytes == 4096 &&
							meta.depth.issued_bytes == 4096 &&
							meta.depth.completed_bytes == 4096 &&
							meta.depth.delivered == 1 &&
							meta.depth.source_returned == 1 &&
							meta.vector_count == oracle.vector_count &&
							meta.obligation_count == oracle.obligation_count &&
							mq_digest_equal(meta.recipe, oracle.recipe) &&
							mq_digest_equal(meta.verifier, oracle.verifier) &&
							mq_digest_equal(meta.vector_table, oracle.vector_table) &&
							mq_digest_equal(meta.fixture_geometry, oracle.fixture_geometry) &&
							mq_digest_equal(meta.fixed_camera, oracle.camera) &&
							mq_digest_equal(meta.fixed_instance_records, oracle.instances));

			verify_depth_oracle_words(p_label, oracle, raw_words);
		}

		record_check(p_label + ".zero_scene_draws_before_release", Engine::get_singleton()->get_frames_drawn() == 0);
		record_check(p_label + ".rd_frames_drawn_unchanged_before_release", device->get_frames_drawn() == rd_draws_before);
		return cancel_and_release_no_draw(p_label, req) && gpu_complete;
	}

	bool run_source_gate_rejection(
			const String &p_label,
			uint64_t p_nonce,
			RID p_material,
			RID p_mesh,
			uint32_t p_expected_gate_bit) {
		GDExtensionProductionRequestRefV1 req = {};
		if (reserve_action7_request(p_nonce, 16, req) != OK) {
			record_check(p_label + ".reserve", false);
			return false;
		}
		const uint8_t payload[16] = { 'g', 'a', 't', 'e', 't', 'e', 's', 't', 'm', 'e', 't', 'a', 'l', 'm', 'q', '7' };
		const auto prep = make_valid_mq_prepare(0, p_material.get_id(), p_mesh.get_id(), GDEXTENSION_MATERIAL_QUALIFICATION_READ_CURRENT_V1, 0, 16);
		if (ports.prepare_mq(req, &prep, payload, 16).native_error != OK) {
			record_check(p_label + ".prepare", false);
			cancel_and_release_no_draw(p_label, req);
			return false;
		}

		GDExtensionMaterialQualificationSelectionFactsV1 sel = {};
		for (uint32_t turn = 0; turn < 128; ++turn) {
			GDExtensionProductionObserveResultV3 obs = {};
			if (pump_single_observe(req, obs) != OK) {
				break;
			}
			if (ports.read_selection(req, 0, &sel).native_error == OK && sel.state != GDEXTENSION_MATERIAL_QUALIFICATION_PENDING_V1) {
				break;
			}
		}
		const bool gate_reported = sel.state == GDEXTENSION_MATERIAL_QUALIFICATION_COMPLETE_V1 &&
				sel.source_gate_known == GDEXTENSION_MATERIAL_QUALIFICATION_SOURCE_GATE_KNOWN_MASK_V1 &&
				(sel.source_gate_rejections & p_expected_gate_bit) == p_expected_gate_bit;
		record_check(p_label + ".selection_gate_rejected", gate_reported);

		GDExtensionMaterialQualificationFixedOracleV1 oracle = {};
		const Error rec_err = Error(ports.prepare_recipe(req, 0, &oracle).native_error);
		record_check(p_label + ".recipe_refused", rec_err == ERR_UNAVAILABLE);

		return cancel_and_release_no_draw(p_label, req) && gate_reported && rec_err == ERR_UNAVAILABLE;
	}

	Dictionary run_all() {
		using namespace GDExtensionDeviceExit;
		Dictionary out;

		server = RenderingServer::get_singleton();
		device = server ? server->get_rendering_device() : nullptr;
		const String driver_name = OS::get_singleton()->get_current_rendering_driver_name();
		const String render_method = OS::get_singleton()->get_current_rendering_method();
		out["driver"] = driver_name;
		out["rendering_method"] = render_method;

		server->set_render_loop_enabled(false);
		const uint64_t initial_draws = Engine::get_singleton()->get_frames_drawn();
		initial_rd_draws = device ? device->get_frames_drawn() : UINT64_MAX;
		const uint64_t min_initial_rd_draws = device ? (uint64_t(device->frames.size()) + 1u) : 0u;
		record_check("pre_first_scene_draw_at_entry", initial_draws == 0);
		record_check("pre_first_rd_frame_at_entry", device != nullptr && initial_rd_draws >= min_initial_rd_draws && initial_rd_draws != UINT64_MAX);
		record_check("live_metal_driver", driver_name == "metal" && device != nullptr && device == RenderingDevice::get_singleton() && device->is_main_instance);
		record_check("ports_loaded", ports.load());

		auto *scene_render = RendererSceneRenderRD::get_singleton();
		const uint32_t expected_backend = render_method == "mobile" ? 1u : (render_method == "forward_plus" ? 2u : 0u);
		record_check("renderer_backend_matches_method", scene_render != nullptr && expected_backend != 0 && scene_render->get_native_material_recipe_backend() == expected_backend);

		if (!failures.is_empty()) {
			out["pass"] = false;
			out["checks"] = checks;
			out["failures"] = failures;
			return out;
		}

		const auto baseline_counts = singleton_counts();

		OrdinalPlan pool = {};
		pool.attachment_requests = 4;
		pool.capacity.abi = production_dto_abi<GDExtensionProductionCapacityV1>();
		pool.capacity.requests = 4;
		pool.capacity.suboperations = 8;
		pool.capacity.resources = 4;
		pool.capacity.resource_uses = 4;
		pool.capacity.graph_memberships = 32;
		pool.capacity.return_cells = 4;
		pool.capacity.retained_payload_bytes = 32768;
		{
			MutexLock registry_lock(enrollment.registry.mutex);
			MutexLock rd_lock(device->_thread_safe_);
			const Error bound_err = enrollment.registry.production_membership_bound_locked(*device, pool.capacity.graph_memberships, device->transfer_worker_pool_max_size, pool.capacity.native_memberships);
			record_check("pool_membership_bound", bound_err == OK);
		}
		// Keep public_port = false so SingletonEnrollment never invokes doctest CHECK() macros
		// when running inside standalone GDScript live-Metal fixtures; MaterialQualificationAction7Ports
		// already exercises all public C ABI entrypoints via GDExtension::get_interface_function().
		enrollment.public_port = false;
		enrollment.server = Object::cast_to<RenderingServerDefault>(server);
		const Error init_err = enrollment.initialize_owner(
				1, false, true, &LiveMetalMaterialQualificationHarness::on_record, this, false, 1, false,
				+[](void *, GDExtensionProductionAttachmentRefV1) {}, false, false, false, false, &pool, device, true);
		enrollment.frame.leave();
		record_check("enrollment_initialized", init_err == OK);

		if (init_err == OK) {
			// Scenario 1: Cold-start ReadCurrent StandardMaterial3D (Opaque CULL_BACK) + BoxMesh before ANY scene draw or server->sync()!
			{
				Ref<BoxMesh> box;
				box.instantiate();
				Ref<StandardMaterial3D> std_opaque;
				std_opaque.instantiate();
				std_opaque->set_albedo(Color(0.8f, 0.2f, 0.1f, 1.0f));
				std_opaque->set_cull_mode(BaseMaterial3D::CULL_BACK);
				run_positive_qualification("positive_read_current_box_cull_back", 1001, std_opaque->get_rid(), box->get_rid(), GDEXTENSION_MATERIAL_QUALIFICATION_READ_CURRENT_V1, 0);
			}

			// Scenario 1b: Custom Spatial ShaderMaterial + compressed ArrayMesh (ARRAY_FLAG_COMPRESS_ATTRIBUTES)!
			{
				Array arr;
				arr.resize(RSE::ARRAY_MAX);
				PackedVector3Array vtx;
				vtx.push_back(Vector3(-1.0f, -1.0f, 0.0f));
				vtx.push_back(Vector3(1.0f, 1.0f, 0.0f));
				vtx.push_back(Vector3(-1.0f, 1.0f, 0.0f));
				PackedVector3Array nrm;
				nrm.push_back(Vector3(0.0f, 0.0f, 1.0f));
				nrm.push_back(Vector3(0.0f, 0.0f, 1.0f));
				nrm.push_back(Vector3(0.0f, 0.0f, 1.0f));
				PackedVector2Array uvs;
				uvs.push_back(Vector2(0.0f, 1.0f));
				uvs.push_back(Vector2(1.0f, 0.0f));
				uvs.push_back(Vector2(0.0f, 0.0f));
				PackedInt32Array idx;
				idx.push_back(0);
				idx.push_back(1);
				idx.push_back(2);
				arr[RSE::ARRAY_VERTEX] = vtx;
				arr[RSE::ARRAY_NORMAL] = nrm;
				arr[RSE::ARRAY_TEX_UV] = uvs;
				arr[RSE::ARRAY_INDEX] = idx;
				Ref<ArrayMesh> comp_mesh;
				comp_mesh.instantiate();
				comp_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arr, TypedArray<Array>(), Dictionary(), Mesh::ARRAY_FLAG_COMPRESS_ATTRIBUTES);
				Ref<Shader> sh_valid;
				sh_valid.instantiate();
				sh_valid->set_code("shader_type spatial;\nvoid fragment() { ALBEDO = vec3(UV, 0.5); ROUGHNESS = 0.4; }\n");
				Ref<ShaderMaterial> mat_valid;
				mat_valid.instantiate();
				mat_valid->set_shader(sh_valid);
				run_positive_qualification("positive_compressed_arraymesh_shader", 1009, mat_valid->get_rid(), comp_mesh->get_rid(), GDEXTENSION_MATERIAL_QUALIFICATION_READ_CURRENT_V1, 0);
			}

			// Scenario 2: Exact matching generation StandardMaterial3D (Opaque CULL_DISABLED + metallic/roughness) + QuadMesh!
			{
				Ref<QuadMesh> quad;
				quad.instantiate();
				Ref<StandardMaterial3D> std_double_sided;
				std_double_sided.instantiate();
				std_double_sided->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
				std_double_sided->set_metallic(0.4f);
				std_double_sided->set_roughness(0.35f);
				const RID mat_rid = std_double_sided->get_rid();
				const RID mesh_rid = quad->get_rid();
				// Prepare ONLY this candidate material on RT to read its exact generation for Exact mode.
				static std::atomic<uint64_t> s_rt_exact_gen{ 0 };
				s_rt_exact_gen.store(0, std::memory_order_relaxed);
				server->call_on_render_thread(callable_mp_static(+[](RID p_mat) {
					if (auto *ms = RendererRD::MaterialStorage::get_singleton()) {
						if (ms->material_prepare_native_qualification(p_mat) == OK) {
							s_rt_exact_gen.store(ms->material_get_generation(p_mat), std::memory_order_release);
						}
					}
				}).bind(mat_rid));
				server->sync();
				const uint64_t exact_gen = s_rt_exact_gen.load(std::memory_order_acquire);
				record_check("exact_generation_nonzero", exact_gen != 0);
				if (exact_gen != 0) {
					run_positive_qualification("positive_exact_quad_cull_disabled", 1002, mat_rid, mesh_rid, GDEXTENSION_MATERIAL_QUALIFICATION_EXACT_V1, exact_gen);

					// Scenario 4: Wrong Exact material generation fails selection cleanly and releases with zero scene draws.
					GDExtensionProductionRequestRefV1 wrong_gen_req = {};
					if (reserve_action7_request(1003, 16, wrong_gen_req) == OK) {
						const uint8_t payload[16] = { 'w', 'r', 'o', 'n', 'g', 'g', 'e', 'n', 'm', 'e', 't', 'a', 'l', 'm', 'q', '7' };
						const auto prep = make_valid_mq_prepare(0, mat_rid.get_id(), mesh_rid.get_id(), GDEXTENSION_MATERIAL_QUALIFICATION_EXACT_V1, exact_gen + 99, 16);
						record_check("wrong_exact_gen.prepare", ports.prepare_mq(wrong_gen_req, &prep, payload, 16).native_error == OK);
						GDExtensionMaterialQualificationSelectionFactsV1 sel = {};
						for (uint32_t turn = 0; turn < 16; ++turn) {
							GDExtensionProductionObserveResultV3 obs = {};
							pump_single_observe(wrong_gen_req, obs);
							if (ports.read_selection(wrong_gen_req, 0, &sel).native_error == OK && sel.state != GDEXTENSION_MATERIAL_QUALIFICATION_PENDING_V1) {
								break;
							}
						}
						record_check("wrong_exact_gen.selection_failed", sel.state == GDEXTENSION_MATERIAL_QUALIFICATION_FAILED_V1 && sel.native_error != OK);
						GDExtensionMaterialQualificationFixedOracleV1 oracle = {};
						record_check("wrong_exact_gen.recipe_refused", ports.prepare_recipe(wrong_gen_req, 0, &oracle).native_error != OK);
						cancel_and_release_no_draw("wrong_exact_gen", wrong_gen_req);
					}
				}
			}

			// Scenario 3: All 5 required source-gate rejections on live Metal.
			{
				Ref<BoxMesh> box;
				box.instantiate();
				const RID mesh_rid = box->get_rid();

				// 3a. StandardMaterial3D TRANSPARENCY_ALPHA -> ALPHA (1u << 2)
				Ref<StandardMaterial3D> mat_alpha;
				mat_alpha.instantiate();
				mat_alpha->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
				run_source_gate_rejection("gate_std_transparency_alpha", 2001, mat_alpha->get_rid(), mesh_rid, 1u << 2);

				// 3b. StandardMaterial3D DEPTH_DRAW_DISABLED -> DEPTH_MODE (1u << 7)
				Ref<StandardMaterial3D> mat_no_depth;
				mat_no_depth.instantiate();
				mat_no_depth->set_depth_draw_mode(BaseMaterial3D::DEPTH_DRAW_DISABLED);
				run_source_gate_rejection("gate_std_depth_draw_disabled", 2002, mat_no_depth->get_rid(), mesh_rid, 1u << 7);

				// 3c. Spatial shader reading TIME -> TIME (1u << 4)
				Ref<Shader> sh_time;
				sh_time.instantiate();
				sh_time->set_code("shader_type spatial;\nvoid fragment() { ALBEDO = vec3(sin(TIME)); }\n");
				Ref<ShaderMaterial> mat_time;
				mat_time.instantiate();
				mat_time->set_shader(sh_time);
				run_source_gate_rejection("gate_shader_time", 2003, mat_time->get_rid(), mesh_rid, 1u << 4);

				// 3d. Spatial shader writing VERTEX -> VERTEX_WRITE (1u << 0)
				Ref<Shader> sh_vtx;
				sh_vtx.instantiate();
				sh_vtx->set_code("shader_type spatial;\nvoid vertex() { VERTEX += vec3(0.0, 0.1, 0.0); }\n");
				Ref<ShaderMaterial> mat_vtx;
				mat_vtx.instantiate();
				mat_vtx->set_shader(sh_vtx);
				run_source_gate_rejection("gate_shader_vertex_write", 2004, mat_vtx->get_rid(), mesh_rid, 1u << 0);

				// 3e. Spatial shader writing DEPTH -> DEPTH_WRITE (1u << 5)
				Ref<Shader> sh_depth;
				sh_depth.instantiate();
				sh_depth->set_code("shader_type spatial;\nvoid fragment() { ALBEDO = vec3(1.0); DEPTH = 0.25; }\n");
				Ref<ShaderMaterial> mat_depth;
				mat_depth.instantiate();
				mat_depth->set_shader(sh_depth);
				run_source_gate_rejection("gate_shader_depth_write", 2005, mat_depth->get_rid(), mesh_rid, 1u << 5);
			}

			// Scenario 5: Cancel & paid RELEASE with no following draw at all 4 lifecycle stages.
			{
				Ref<BoxMesh> box;
				box.instantiate();
				Ref<StandardMaterial3D> mat;
				mat.instantiate();
				const RID mesh_rid = box->get_rid();
				const RID mat_rid = mat->get_rid();
				const uint8_t payload[16] = { 'c', 'a', 'n', 'c', 'e', 'l', 's', 't', 'a', 'g', 'e', 'm', 'q', '7', '0', '1' };

				// 5a. Cancel before OBSERVE.
				{
					GDExtensionProductionRequestRefV1 req = {};
					if (reserve_action7_request(3001, 16, req) == OK) {
						const auto prep = make_valid_mq_prepare(0, mat_rid.get_id(), mesh_rid.get_id(), GDEXTENSION_MATERIAL_QUALIFICATION_READ_CURRENT_V1, 0, 16);
						record_check("cancel_before_observe.prepare", ports.prepare_mq(req, &prep, payload, 16).native_error == OK);
						cancel_and_release_no_draw("cancel_before_observe", req);
					}
				}

				// 5b. Cancel after selection COMPLETE (before recipe requested).
				{
					GDExtensionProductionRequestRefV1 req = {};
					if (reserve_action7_request(3002, 16, req) == OK) {
						const auto prep = make_valid_mq_prepare(0, mat_rid.get_id(), mesh_rid.get_id(), GDEXTENSION_MATERIAL_QUALIFICATION_READ_CURRENT_V1, 0, 16);
						record_check("cancel_after_selection.prepare", ports.prepare_mq(req, &prep, payload, 16).native_error == OK);
						GDExtensionMaterialQualificationSelectionFactsV1 sel = {};
						for (uint32_t turn = 0; turn < 128; ++turn) {
							GDExtensionProductionObserveResultV3 obs = {};
							pump_single_observe(req, obs);
							if (ports.read_selection(req, 0, &sel).native_error == OK && sel.state == GDEXTENSION_MATERIAL_QUALIFICATION_COMPLETE_V1) {
								break;
							}
						}
						record_check("cancel_after_selection.selected", sel.state == GDEXTENSION_MATERIAL_QUALIFICATION_COMPLETE_V1);
						cancel_and_release_no_draw("cancel_after_selection", req);
					}
				}

				// 5c. Cancel after recipe OK (owns textures, framebuffer, buffers, fixture mesh, pipeline & uniform sets; RECORD never run).
				{
					GDExtensionProductionRequestRefV1 req = {};
					if (reserve_action7_request(3003, 16, req) == OK) {
						const auto prep = make_valid_mq_prepare(0, mat_rid.get_id(), mesh_rid.get_id(), GDEXTENSION_MATERIAL_QUALIFICATION_READ_CURRENT_V1, 0, 16);
						record_check("cancel_after_recipe.prepare", ports.prepare_mq(req, &prep, payload, 16).native_error == OK);
						GDExtensionMaterialQualificationSelectionFactsV1 sel = {};
						for (uint32_t turn = 0; turn < 128; ++turn) {
							GDExtensionProductionObserveResultV3 obs = {};
							pump_single_observe(req, obs);
							if (ports.read_selection(req, 0, &sel).native_error == OK && sel.state == GDEXTENSION_MATERIAL_QUALIFICATION_COMPLETE_V1) {
								break;
							}
						}
						GDExtensionMaterialQualificationFixedOracleV1 oracle = {};
						ports.prepare_recipe(req, 0, &oracle);
						bool ready = false;
						for (uint32_t turn = 0; turn < 128; ++turn) {
							GDExtensionProductionObserveResultV3 obs = {};
							pump_single_observe(req, obs);
							if (ports.prepare_recipe(req, 0, &oracle).native_error == OK) {
								ready = true;
								break;
							}
						}
						record_check("cancel_after_recipe.ready", ready);
						cancel_and_release_no_draw("cancel_after_recipe", req);
					}
				}

				// 5d. Cancel immediately after RECORD consumed (before read_mq; paid RELEASE drains in-flight graph/readback & K03 witnesses with zero scene draws!).
				{
					GDExtensionProductionRequestRefV1 req = {};
					if (reserve_action7_request(3004, 16, req) == OK) {
						const auto prep = make_valid_mq_prepare(0, mat_rid.get_id(), mesh_rid.get_id(), GDEXTENSION_MATERIAL_QUALIFICATION_READ_CURRENT_V1, 0, 16);
						record_check("cancel_after_record.prepare", ports.prepare_mq(req, &prep, payload, 16).native_error == OK);
						GDExtensionMaterialQualificationSelectionFactsV1 sel = {};
						for (uint32_t turn = 0; turn < 128; ++turn) {
							GDExtensionProductionObserveResultV3 obs = {};
							pump_single_observe(req, obs);
							if (ports.read_selection(req, 0, &sel).native_error == OK && sel.state == GDEXTENSION_MATERIAL_QUALIFICATION_COMPLETE_V1) {
								break;
							}
						}
						GDExtensionMaterialQualificationFixedOracleV1 oracle = {};
						ports.prepare_recipe(req, 0, &oracle);
						for (uint32_t turn = 0; turn < 128; ++turn) {
							GDExtensionProductionObserveResultV3 obs = {};
							pump_single_observe(req, obs);
							if (ports.prepare_recipe(req, 0, &oracle).native_error == OK) {
								break;
							}
						}
						active_record_req = req;
						record_call_result = { ERR_UNCONFIGURED, 0, 0, 0 };
						uint64_t ctrl = 0;
						record_check("cancel_after_record.schedule", ports.record_schedule(req, &ctrl) == OK);
						server->sync();
						GDExtensionProductionRecordReportV2 rec = {};
						record_check("cancel_after_record.consume", ports.record_consume(req, ctrl, &rec).native_error == OK && record_call_result.native_error == OK);
						cancel_and_release_no_draw("cancel_after_record", req);
					}
				}
			}

			// Scenario 6 ([P2 #2] Test 1): Live Metal mid-recipe ERR_BUSY idempotency (Step 1 & Step 5) + 2 concurrent interleaved action7 requests.
			{
				Ref<BoxMesh> box_a;
				box_a.instantiate();
				Ref<StandardMaterial3D> mat_a;
				mat_a.instantiate();
				mat_a->set_albedo(Color(0.25f, 0.75f, 0.35f, 1.0f));
				mat_a->set_cull_mode(BaseMaterial3D::CULL_BACK);

				Ref<QuadMesh> quad_b;
				quad_b.instantiate();
				Ref<Shader> sh_b;
				sh_b.instantiate();
				sh_b->set_code("shader_type spatial;\nrender_mode cull_disabled;\nvoid fragment() { ALBEDO = vec3(0.3, 0.6, 0.9); ROUGHNESS = 0.5; }\n");
				Ref<ShaderMaterial> mat_b;
				mat_b.instantiate();
				mat_b->set_shader(sh_b);

				static GDExtensionDeviceExit::QualificationTicket s_contention_ticket;
				auto rt_push_ticket = [&]() {
					server->call_on_render_thread(callable_mp_static(+[]() {
						if (RenderingDevice *rd = RenderingDevice::get_singleton()) {
							MutexLock rd_lock(rd->_thread_safe_);
							if (!rd->frames.is_empty() && !rd->frames[rd->frame].qualification_tickets.has(&s_contention_ticket)) {
								rd->frames[rd->frame].qualification_tickets.push_back(&s_contention_ticket);
							}
						}
					}));
					server->sync();
				};
				auto rt_pop_ticket = [&]() {
					server->call_on_render_thread(callable_mp_static(+[]() {
						if (RenderingDevice *rd = RenderingDevice::get_singleton()) {
							MutexLock rd_lock(rd->_thread_safe_);
							for (uint32_t fi = 0; fi < rd->frames.size(); ++fi) {
								rd->frames[fi].qualification_tickets.erase(&s_contention_ticket);
							}
						}
					}));
					server->sync();
				};
				auto rt_settle_fixture_only_and_repush = [&]() {
					server->call_on_render_thread(callable_mp_static(+[]() {
						if (RenderingDevice *rd = RenderingDevice::get_singleton()) {
							{
								MutexLock rd_lock(rd->_thread_safe_);
								for (uint32_t fi = 0; fi < rd->frames.size(); ++fi) {
									rd->frames[fi].qualification_tickets.erase(&s_contention_ticket);
								}
							}
							rd->_submit_transfer_workers();
							rd->_wait_for_transfer_workers();
							{
								MutexLock rd_lock(rd->_thread_safe_);
								if (!rd->frames.is_empty() && !rd->frames[rd->frame].qualification_tickets.has(&s_contention_ticket)) {
									rd->frames[rd->frame].qualification_tickets.push_back(&s_contention_ticket);
								}
							}
						}
					}));
					server->sync();
				};

				const uint8_t payload[16] = { 'b', 'u', 's', 'y', 'r', 'e', 't', 'r', 'y', 'm', 'q', '7', 't', 'e', 's', 't' };

				// 6a. Mid-Step-1 ERR_BUSY -> idempotent retry -> Mid-Step-5 ERR_BUSY -> idempotent retry -> COMPLETE -> exact depth -> RELEASE.
				{
					GDExtensionProductionRequestRefV1 req = {};
					const Error res_err = reserve_action7_request(4001, 16, req);
					record_check("mid_recipe_busy_complete.reserve", res_err == OK);
					if (res_err == OK) {
						const auto prep = make_valid_mq_prepare(0, mat_a->get_rid().get_id(), box_a->get_rid().get_id(), GDEXTENSION_MATERIAL_QUALIFICATION_READ_CURRENT_V1, 0, 16);
						record_check("mid_recipe_busy_complete.prepare", ports.prepare_mq(req, &prep, payload, 16).native_error == OK);
						GDExtensionMaterialQualificationSelectionFactsV1 sel = {};
						for (uint32_t turn = 0; turn < 128; ++turn) {
							GDExtensionProductionObserveResultV3 obs = {};
							pump_single_observe(req, obs);
							if (ports.read_selection(req, 0, &sel).native_error == OK && sel.state == GDEXTENSION_MATERIAL_QUALIFICATION_COMPLETE_V1) {
								break;
							}
						}
						record_check("mid_recipe_busy_complete.selection_ok", sel.state == GDEXTENSION_MATERIAL_QUALIFICATION_COMPLETE_V1);

						GDExtensionMaterialQualificationFixedOracleV1 oracle = {};
						record_check("mid_recipe_busy_complete.request_recipe", ports.prepare_recipe(req, 0, &oracle).native_error == ERR_BUSY);

						// Inject active ticket BEFORE Step 1 settles fixture_mesh_rid's transfer worker!
						rt_push_ticket();
						GDExtensionProductionObserveResultV3 obs1 = {};
						pump_single_observe(req, obs1);
						RID fixture_rid_1;
						uint32_t disp_count_1 = 0;
						bool step1_paused = false;
						{
							Registry::Lock reg_lock(enrollment.registry);
							ProductionRequest *pr = nullptr;
							RenderingDevice *prd = nullptr;
							if (enrollment.registry.production_request_locked(req, false, pr, prd) == OK && pr && pr->actions[0].material_qualification) {
								const auto &mq = *pr->actions[0].material_qualification;
								fixture_rid_1 = mq.fixture_mesh_rid;
								disp_count_1 = mq.disposal_use_count;
								step1_paused = mq.fixture_mesh_rid.is_valid() && !mq.fixture_mesh_settled && !mq.recipe_ready;
							}
						}
						record_check("mid_recipe_busy_complete.step1_busy_paused", step1_paused && disp_count_1 > 0 && ports.prepare_recipe(req, 0, &oracle).native_error == ERR_BUSY);

						// Retry Step 1 while ticket is STILL active: zero duplicate fixture mesh or disposals!
						GDExtensionProductionObserveResultV3 obs2 = {};
						pump_single_observe(req, obs2);
						bool step1_idempotent = false;
						{
							Registry::Lock reg_lock(enrollment.registry);
							ProductionRequest *pr = nullptr;
							RenderingDevice *prd = nullptr;
							if (enrollment.registry.production_request_locked(req, false, pr, prd) == OK && pr && pr->actions[0].material_qualification) {
								const auto &mq = *pr->actions[0].material_qualification;
								step1_idempotent = (mq.fixture_mesh_rid == fixture_rid_1 && !mq.fixture_mesh_settled && mq.disposal_use_count == disp_count_1);
							}
						}
						record_check("mid_recipe_busy_complete.step1_idempotent_on_busy", step1_idempotent && ports.prepare_recipe(req, 0, &oracle).native_error == ERR_BUSY);

						// Settle ONLY fixture_mesh_rid and re-push ticket so the next OBSERVE advances Steps 1..4 and hits genuine ERR_BUSY inside Step 5!
						rt_settle_fixture_only_and_repush();
						GDExtensionProductionObserveResultV3 obs3 = {};
						pump_single_observe(req, obs3);
						RID scene_buf_1;
						RID fb_rid_1;
						uint32_t disp_count_5 = 0;
						bool step5_paused = false;
						{
							Registry::Lock reg_lock(enrollment.registry);
							ProductionRequest *pr = nullptr;
							RenderingDevice *prd = nullptr;
							if (enrollment.registry.production_request_locked(req, false, pr, prd) == OK && pr && pr->actions[0].material_qualification) {
								const auto &mq = *pr->actions[0].material_qualification;
								scene_buf_1 = mq.owned_buffers[0];
								fb_rid_1 = mq.framebuffer_rid;
								disp_count_5 = mq.disposal_use_count;
								step5_paused = mq.fixture_mesh_settled && mq.framebuffer_rid.is_valid() && mq.owned_buffers[0].is_valid() && !mq.owned_buffer_settled[0] && !mq.recipe_ready;
							}
						}
						record_check("mid_recipe_busy_complete.step5_busy_paused", step5_paused && disp_count_5 > disp_count_1 && ports.prepare_recipe(req, 0, &oracle).native_error == ERR_BUSY);

						// Retry Step 5 while ticket is STILL active: zero duplicate buffers/framebuffers/disposals!
						GDExtensionProductionObserveResultV3 obs4 = {};
						pump_single_observe(req, obs4);
						bool step5_idempotent = false;
						{
							Registry::Lock reg_lock(enrollment.registry);
							ProductionRequest *pr = nullptr;
							RenderingDevice *prd = nullptr;
							if (enrollment.registry.production_request_locked(req, false, pr, prd) == OK && pr && pr->actions[0].material_qualification) {
								const auto &mq = *pr->actions[0].material_qualification;
								step5_idempotent = (mq.framebuffer_rid == fb_rid_1 && mq.owned_buffers[0] == scene_buf_1 && mq.disposal_use_count == disp_count_5 && !mq.recipe_ready);
							}
						}
						record_check("mid_recipe_busy_complete.step5_idempotent_on_busy", step5_idempotent && ports.prepare_recipe(req, 0, &oracle).native_error == ERR_BUSY);

						// Clear contention ticket and verify clean resume -> OK -> RECORD -> exact 32x32 depth -> RELEASE!
						rt_pop_ticket();
						bool resumed_ready = false;
						for (uint32_t turn = 0; turn < 64; ++turn) {
							GDExtensionProductionObserveResultV3 obs = {};
							pump_single_observe(req, obs);
							if (ports.prepare_recipe(req, 0, &oracle).native_error == OK) {
								resumed_ready = verify_mq_fixed_oracle_shape(oracle);
								break;
							}
						}
						record_check("mid_recipe_busy_complete.resumed_ready", resumed_ready);
						if (resumed_ready) {
							active_record_req = req;
							record_call_result = { ERR_UNCONFIGURED, 0, 0, 0 };
							uint64_t ctrl = 0;
							record_check("mid_recipe_busy_complete.record_schedule", ports.record_schedule(req, &ctrl) == OK);
							server->sync();
							GDExtensionProductionRecordReportV2 rec = {};
							record_check("mid_recipe_busy_complete.record_consume", ports.record_consume(req, ctrl, &rec).native_error == OK && record_call_result.native_error == OK);
							if (rec.flags & GDEXTENSION_PRODUCTION_RECORD_HAS_SEGMENT_V1) {
								GDExtensionDeviceExit::CallbackScope owner_entry(enrollment.reference);
								ports.ack_segment(req, rec.segment.segment, rec.segment.segment_revision);
							}
							GDExtensionMaterialQualificationResultMetadataV1 meta = {};
							uint32_t raw_words[1024] = {};
							bool read_ok = false;
							for (uint32_t turn = 0; turn < 128; ++turn) {
								GDExtensionProductionObserveResultV3 obs = {};
								pump_single_observe(req, obs);
								if (ports.read_mq(req, 0, &meta, 1000, raw_words, 1024).native_error == OK) {
									read_ok = true;
									break;
								}
							}
							record_check("mid_recipe_busy_complete.read_mq_ok", read_ok && meta.state == GDEXTENSION_MATERIAL_QUALIFICATION_COMPLETE_V1);
							if (read_ok) {
								verify_depth_oracle_words("mid_recipe_busy_complete", oracle, raw_words);
							}
						}
						cancel_and_release_no_draw("mid_recipe_busy_complete", req);
					}
					rt_pop_ticket();
				}

				// 6b. Cancel & paid RELEASE directly from Mid-Step-5 ERR_BUSY while concurrent QualificationTicket remains active!
				{
					GDExtensionProductionRequestRefV1 req = {};
					if (reserve_action7_request(4002, 16, req) == OK) {
						const auto prep = make_valid_mq_prepare(0, mat_a->get_rid().get_id(), box_a->get_rid().get_id(), GDEXTENSION_MATERIAL_QUALIFICATION_READ_CURRENT_V1, 0, 16);
						record_check("mid_step5_cancel_under_ticket.prepare", ports.prepare_mq(req, &prep, payload, 16).native_error == OK);
						GDExtensionMaterialQualificationSelectionFactsV1 sel = {};
						for (uint32_t turn = 0; turn < 128; ++turn) {
							GDExtensionProductionObserveResultV3 obs = {};
							pump_single_observe(req, obs);
							if (ports.read_selection(req, 0, &sel).native_error == OK && sel.state == GDEXTENSION_MATERIAL_QUALIFICATION_COMPLETE_V1) {
								break;
							}
						}
						GDExtensionMaterialQualificationFixedOracleV1 oracle = {};
						ports.prepare_recipe(req, 0, &oracle);
						rt_push_ticket();
						GDExtensionProductionObserveResultV3 obs1 = {};
						pump_single_observe(req, obs1);
						rt_settle_fixture_only_and_repush();
						GDExtensionProductionObserveResultV3 obs2 = {};
						pump_single_observe(req, obs2);
						record_check("mid_step5_cancel_under_ticket.at_step5_busy", ports.prepare_recipe(req, 0, &oracle).native_error == ERR_BUSY);
						// Release while s_contention_ticket is STILL active on RD!
						cancel_and_release_no_draw("mid_step5_cancel_under_ticket", req);
						rt_pop_ticket();
					}
				}

				// 6c. 2 concurrent action7 requests interleaving tight-budget OBSERVE / RECORD / RELEASE simultaneously.
				{
					GDExtensionProductionRequestRefV1 req_a = {};
					GDExtensionProductionRequestRefV1 req_b = {};
					const bool r_a = (reserve_action7_request(4003, 16, req_a) == OK);
					const bool r_b = (reserve_action7_request(4004, 16, req_b) == OK);
					record_check("concurrent_two_req.reserve_both", r_a && r_b);
					if (r_a && r_b) {
						const auto prep_a = make_valid_mq_prepare(0, mat_a->get_rid().get_id(), box_a->get_rid().get_id(), GDEXTENSION_MATERIAL_QUALIFICATION_READ_CURRENT_V1, 0, 16);
						const auto prep_b = make_valid_mq_prepare(0, mat_b->get_rid().get_id(), quad_b->get_rid().get_id(), GDEXTENSION_MATERIAL_QUALIFICATION_READ_CURRENT_V1, 0, 16);
						record_check("concurrent_two_req.prepare_both",
								ports.prepare_mq(req_a, &prep_a, payload, 16).native_error == OK &&
										ports.prepare_mq(req_b, &prep_b, payload, 16).native_error == OK);

						auto pump_tight = [&](GDExtensionProductionRequestRefV1 p_r) {
							GDExtensionProductionObserveResultV3 obs = {};
							const auto b1 = make_tight_budget(2, 6);
							ports.observe(p_r, &b1, &obs);
							server->sync();
							const auto b2 = make_tight_budget(2, 6);
							ports.observe(p_r, &b2, &obs);
						};

						GDExtensionMaterialQualificationSelectionFactsV1 sel_a = {};
						GDExtensionMaterialQualificationSelectionFactsV1 sel_b = {};
						for (uint32_t turn = 0; turn < 128; ++turn) {
							if (sel_a.state != GDEXTENSION_MATERIAL_QUALIFICATION_COMPLETE_V1) {
								pump_tight(req_a);
								ports.read_selection(req_a, 0, &sel_a);
							}
							if (sel_b.state != GDEXTENSION_MATERIAL_QUALIFICATION_COMPLETE_V1) {
								pump_tight(req_b);
								ports.read_selection(req_b, 0, &sel_b);
							}
							if (sel_a.state == GDEXTENSION_MATERIAL_QUALIFICATION_COMPLETE_V1 && sel_b.state == GDEXTENSION_MATERIAL_QUALIFICATION_COMPLETE_V1) {
								break;
							}
						}
						record_check("concurrent_two_req.selection_both",
								sel_a.state == GDEXTENSION_MATERIAL_QUALIFICATION_COMPLETE_V1 &&
										sel_b.state == GDEXTENSION_MATERIAL_QUALIFICATION_COMPLETE_V1);

						GDExtensionMaterialQualificationFixedOracleV1 oracle_a = {};
						GDExtensionMaterialQualificationFixedOracleV1 oracle_b = {};
						ports.prepare_recipe(req_a, 0, &oracle_a);
						ports.prepare_recipe(req_b, 0, &oracle_b);
						bool ready_a = false;
						bool ready_b = false;
						for (uint32_t turn = 0; turn < 128 && (!ready_a || !ready_b); ++turn) {
							if (!ready_a) {
								pump_tight(req_a);
								ready_a = (ports.prepare_recipe(req_a, 0, &oracle_a).native_error == OK);
							}
							if (!ready_b) {
								pump_tight(req_b);
								ready_b = (ports.prepare_recipe(req_b, 0, &oracle_b).native_error == OK);
							}
						}
						record_check("concurrent_two_req.recipe_both", ready_a && ready_b);

						active_record_req = req_a;
						active_record_req_b = req_b;
						record_call_result = { ERR_UNCONFIGURED, 0, 0, 0 };
						record_call_result_b = { ERR_UNCONFIGURED, 0, 0, 0 };
						uint64_t ctrl_a = 0;
						uint64_t ctrl_b = 0;
						record_check("concurrent_two_req.schedule_both",
								ports.record_schedule(req_a, &ctrl_a) == OK &&
										ports.record_schedule(req_b, &ctrl_b) == OK);
						server->sync();
						GDExtensionProductionRecordReportV2 rec_a = {};
						GDExtensionProductionRecordReportV2 rec_b = {};
						record_check("concurrent_two_req.consume_both",
								ports.record_consume(req_a, ctrl_a, &rec_a).native_error == OK &&
										ports.record_consume(req_b, ctrl_b, &rec_b).native_error == OK &&
										record_call_result.native_error == OK &&
										record_call_result_b.native_error == OK);
						{
							GDExtensionDeviceExit::CallbackScope owner_entry(enrollment.reference);
							if (rec_a.flags & GDEXTENSION_PRODUCTION_RECORD_HAS_SEGMENT_V1) {
								ports.ack_segment(req_a, rec_a.segment.segment, rec_a.segment.segment_revision);
							}
							if (rec_b.flags & GDEXTENSION_PRODUCTION_RECORD_HAS_SEGMENT_V1) {
								ports.ack_segment(req_b, rec_b.segment.segment, rec_b.segment.segment_revision);
							}
						}
						active_record_req_b = {};

						GDExtensionMaterialQualificationResultMetadataV1 meta_a = {};
						GDExtensionMaterialQualificationResultMetadataV1 meta_b = {};
						uint32_t words_a[1024] = {};
						uint32_t words_b[1024] = {};
						bool done_a = false;
						bool done_b = false;
						for (uint32_t turn = 0; turn < 256 && (!done_a || !done_b); ++turn) {
							if (!done_a) {
								pump_tight(req_a);
								done_a = (ports.read_mq(req_a, 0, &meta_a, 1000, words_a, 1024).native_error == OK);
							}
							if (!done_b) {
								pump_tight(req_b);
								done_b = (ports.read_mq(req_b, 0, &meta_b, 1000, words_b, 1024).native_error == OK);
							}
							if (!done_a || !done_b) {
								OS::get_singleton()->delay_usec(500);
							}
						}
						record_check("concurrent_two_req.gpu_complete_both", done_a && done_b);
						if (done_a) {
							verify_depth_oracle_words("concurrent_two_req_a", oracle_a, words_a);
						} else {
							record_check("concurrent_two_req_a.depth_oracle_exact_bits", false);
						}
						if (done_b) {
							verify_depth_oracle_words("concurrent_two_req_b", oracle_b, words_b);
						} else {
							record_check("concurrent_two_req_b.depth_oracle_exact_bits", false);
						}

						// Interleave tight-budget cancel + RELEASE across both requests.
						record_check("concurrent_two_req.cancel_both",
								ports.cancel(req_a).native_error == OK &&
										ports.cancel(req_b).native_error == OK);
						bool rel_a = false;
						bool rel_b = false;
						for (uint32_t turn = 0; turn < 256 && (!rel_a || !rel_b); ++turn) {
							GDExtensionProductionServiceBudgetV1 rem = {};
							GDExtensionProductionReportV2 rep = {};
							if (!rel_a) {
								const auto b1 = make_tight_budget(2, 6);
								const Error e1 = Error(ports.release(req_a, &b1, &rem, &rep).native_error);
								if (e1 == OK && !rep.service_pending) {
									rel_a = true;
								} else {
									server->sync();
									const auto b2 = make_tight_budget(2, 6);
									const Error e2 = Error(ports.release(req_a, &b2, &rem, &rep).native_error);
									if (e2 == OK && !rep.service_pending) {
										rel_a = true;
									}
								}
							}
							if (!rel_b) {
								const auto b1 = make_tight_budget(2, 6);
								const Error e1 = Error(ports.release(req_b, &b1, &rem, &rep).native_error);
								if (e1 == OK && !rep.service_pending) {
									rel_b = true;
								} else {
									server->sync();
									const auto b2 = make_tight_budget(2, 6);
									const Error e2 = Error(ports.release(req_b, &b2, &rem, &rep).native_error);
									if (e2 == OK && !rep.service_pending) {
										rel_b = true;
									}
								}
							}
							if (!rel_a || !rel_b) {
								OS::get_singleton()->delay_usec(300);
							}
						}
						record_check("concurrent_two_req.released_both", rel_a && rel_b);
						record_check("concurrent_two_req.rd_frames_drawn_unchanged", device->get_frames_drawn() == initial_rd_draws);
					}
				}
			}

			// Scenario 7 ([P2 #2] Test 3): Bounded-budget RELEASE under active concurrent QualificationTicket + pre-existing freed foreign engine resources.
			{
				// 1. Free unrelated scene & RD resources beforehand while set_render_loop_enabled(false) so foreign *_to_dispose_of lists are non-empty!
				{
					Ref<BoxMesh> foreign_mesh;
					foreign_mesh.instantiate();
					Ref<StandardMaterial3D> foreign_mat;
					foreign_mat.instantiate();
					foreign_mat->set_albedo(Color(0.9f, 0.4f, 0.2f, 1.0f));
				}
				static std::atomic<uint32_t> s_foreign_buf_disposals_before{ 0 };
				static std::atomic<uint32_t> s_foreign_tex_disposals_before{ 0 };
				s_foreign_buf_disposals_before.store(0, std::memory_order_relaxed);
				s_foreign_tex_disposals_before.store(0, std::memory_order_relaxed);
				server->call_on_render_thread(callable_mp_static(+[]() {
					if (RenderingDevice *rd = RenderingDevice::get_singleton()) {
						RID fbuf = rd->storage_buffer_create(256);
						RD::TextureFormat tf;
						tf.format = RD::DATA_FORMAT_R8G8B8A8_UNORM;
						tf.width = 8;
						tf.height = 8;
						tf.usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT;
						RID ftex = rd->texture_create(tf, RD::TextureView());
						rd->free_rid(fbuf);
						rd->free_rid(ftex);
						MutexLock rd_lock(rd->_thread_safe_);
						if (!rd->frames.is_empty()) {
							s_foreign_buf_disposals_before.store(uint32_t(rd->frames[rd->frame].buffers_to_dispose_of.size()), std::memory_order_release);
							s_foreign_tex_disposals_before.store(uint32_t(rd->frames[rd->frame].textures_to_dispose_of.size()), std::memory_order_release);
						}
					}
				}));
				server->sync();
				const uint32_t foreign_bufs_before = s_foreign_buf_disposals_before.load(std::memory_order_acquire);
				const uint32_t foreign_texs_before = s_foreign_tex_disposals_before.load(std::memory_order_acquire);
				record_check("foreign_disposal_and_ticket_release.foreign_queued", foreign_bufs_before >= 1 && foreign_texs_before >= 1);

				Ref<BoxMesh> box;
				box.instantiate();
				Ref<StandardMaterial3D> mat;
				mat.instantiate();
				mat->set_albedo(Color(0.5f, 0.5f, 0.8f, 1.0f));

				GDExtensionProductionRequestRefV1 req = {};
				const uint8_t payload[16] = { 'f', 'o', 'r', 'e', 'i', 'g', 'n', 'k', '0', '3', 't', 'i', 'c', 'k', 'e', 't' };
				if (reserve_action7_request(5001, 16, req) == OK) {
					const auto prep = make_valid_mq_prepare(0, mat->get_rid().get_id(), box->get_rid().get_id(), GDEXTENSION_MATERIAL_QUALIFICATION_READ_CURRENT_V1, 0, 16);
					record_check("foreign_disposal_and_ticket_release.prepare", ports.prepare_mq(req, &prep, payload, 16).native_error == OK);

					GDExtensionMaterialQualificationSelectionFactsV1 sel = {};
					for (uint32_t turn = 0; turn < 128; ++turn) {
						GDExtensionProductionObserveResultV3 obs = {};
						pump_single_observe(req, obs);
						if (ports.read_selection(req, 0, &sel).native_error == OK && sel.state == GDEXTENSION_MATERIAL_QUALIFICATION_COMPLETE_V1) {
							break;
						}
					}
					GDExtensionMaterialQualificationFixedOracleV1 oracle = {};
					ports.prepare_recipe(req, 0, &oracle);
					bool ready = false;
					for (uint32_t turn = 0; turn < 128; ++turn) {
						GDExtensionProductionObserveResultV3 obs = {};
						pump_single_observe(req, obs);
						if (ports.prepare_recipe(req, 0, &oracle).native_error == OK) {
							ready = true;
							break;
						}
					}
					record_check("foreign_disposal_and_ticket_release.recipe_ready", ready);

					active_record_req = req;
					record_call_result = { ERR_UNCONFIGURED, 0, 0, 0 };
					uint64_t ctrl = 0;
					record_check("foreign_disposal_and_ticket_release.schedule", ports.record_schedule(req, &ctrl) == OK);
					server->sync();
					GDExtensionProductionRecordReportV2 rec = {};
					record_check("foreign_disposal_and_ticket_release.consume", ports.record_consume(req, ctrl, &rec).native_error == OK && record_call_result.native_error == OK);
					if (rec.flags & GDEXTENSION_PRODUCTION_RECORD_HAS_SEGMENT_V1) {
						GDExtensionDeviceExit::CallbackScope owner_entry(enrollment.reference);
						ports.ack_segment(req, rec.segment.segment, rec.segment.segment_revision);
					}

					GDExtensionMaterialQualificationResultMetadataV1 meta = {};
					uint32_t words[1024] = {};
					bool done = false;
					for (uint32_t turn = 0; turn < 256; ++turn) {
						GDExtensionProductionObserveResultV3 obs = {};
						pump_single_observe(req, obs);
						if (ports.read_mq(req, 0, &meta, 1000, words, 1024).native_error == OK) {
							done = true;
							break;
						}
					}
					record_check("foreign_disposal_and_ticket_release.gpu_complete", done);
					if (done) {
						verify_depth_oracle_words("foreign_disposal_and_ticket_release", oracle, words);
					}

					// 2. Push an active second QualificationTicket onto RD BEFORE action7 RELEASE, then drive RELEASE under bounded tight budget!
					static GDExtensionDeviceExit::QualificationTicket s_release_ticket;
					server->call_on_render_thread(callable_mp_static(+[]() {
						if (RenderingDevice *rd = RenderingDevice::get_singleton()) {
							MutexLock rd_lock(rd->_thread_safe_);
							if (!rd->frames.is_empty() && !rd->frames[rd->frame].qualification_tickets.has(&s_release_ticket)) {
								rd->frames[rd->frame].qualification_tickets.push_back(&s_release_ticket);
							}
						}
					}));
					server->sync();

					record_check("foreign_disposal_and_ticket_release.cancel", ports.cancel(req).native_error == OK);
					GDExtensionProductionReportV2 rel_rep = {};
					bool reclaimed = false;
					for (uint32_t turn = 0; turn < 256; ++turn) {
						GDExtensionProductionServiceBudgetV1 rem = {};
						const auto b1 = make_tight_budget(2, 6);
						const Error r1 = Error(ports.release(req, &b1, &rem, &rel_rep).native_error);
						if (r1 != OK && r1 != ERR_BUSY) {
							break;
						}
						if (r1 == OK && !rel_rep.service_pending) {
							reclaimed = true;
							break;
						}
						server->sync();
						const auto b2 = make_tight_budget(2, 6);
						const Error r2 = Error(ports.release(req, &b2, &rem, &rel_rep).native_error);
						if (r2 != OK && r2 != ERR_BUSY) {
							break;
						}
						if (r2 == OK && !rel_rep.service_pending) {
							reclaimed = true;
							break;
						}
						OS::get_singleton()->delay_usec(300);
					}
					record_check("foreign_disposal_and_ticket_release.reclaimed_under_bounded_budget",
							reclaimed && rel_rep.service_pending == 0 && rel_rep.physical_returns_pending == 0);
					record_check("foreign_disposal_and_ticket_release.rd_frames_drawn_unchanged", device->get_frames_drawn() == initial_rd_draws);

					static std::atomic<uint32_t> s_foreign_buf_disposals_after{ 0 };
					static std::atomic<uint32_t> s_foreign_tex_disposals_after{ 0 };
					static std::atomic<bool> s_ticket_still_present{ false };
					s_foreign_buf_disposals_after.store(0, std::memory_order_relaxed);
					s_foreign_tex_disposals_after.store(0, std::memory_order_relaxed);
					s_ticket_still_present.store(false, std::memory_order_relaxed);
					server->call_on_render_thread(callable_mp_static(+[]() {
						if (RenderingDevice *rd = RenderingDevice::get_singleton()) {
							MutexLock rd_lock(rd->_thread_safe_);
							if (!rd->frames.is_empty()) {
								s_foreign_buf_disposals_after.store(uint32_t(rd->frames[rd->frame].buffers_to_dispose_of.size()), std::memory_order_release);
								s_foreign_tex_disposals_after.store(uint32_t(rd->frames[rd->frame].textures_to_dispose_of.size()), std::memory_order_release);
								s_ticket_still_present.store(rd->frames[rd->frame].qualification_tickets.has(&s_release_ticket), std::memory_order_release);
								for (uint32_t fi = 0; fi < rd->frames.size(); ++fi) {
									rd->frames[fi].qualification_tickets.erase(&s_release_ticket);
								}
							}
						}
					}));
					server->sync();
					record_check("foreign_disposal_and_ticket_release.foreign_untouched_and_ticket_intact",
							s_ticket_still_present.load(std::memory_order_acquire) &&
									s_foreign_buf_disposals_after.load(std::memory_order_acquire) >= foreign_bufs_before &&
									s_foreign_tex_disposals_after.load(std::memory_order_acquire) >= foreign_texs_before);
				}
			}

			// Scenario 8: CUSTOM0/CUSTOM1 Set 2 binding 2 roster planning (allowed_mask = 412) + depth_test_disabled pass vs depth_test_inverted & VERTEX write rejection!
			{
				static constexpr uint32_t kAllowedTransparentAuxMask = 412u; // ALPHA(4) | DISCARD(8) | TIME(16) | DEPTH_MODE(128) | FRAMEBUFFER_READ(256)
				Array q_arr;
				q_arr.resize(RSE::ARRAY_MAX);
				PackedVector3Array q_vtx;
				q_vtx.push_back(Vector3(-0.5f, -0.5f, 0.0f));
				q_vtx.push_back(Vector3(0.5f, -0.5f, 0.0f));
				q_vtx.push_back(Vector3(-0.5f, 0.5f, 0.0f));
				q_vtx.push_back(Vector3(0.5f, 0.5f, 0.0f));
				PackedInt32Array q_idx;
				q_idx.push_back(0);
				q_idx.push_back(1);
				q_idx.push_back(2);
				q_idx.push_back(2);
				q_idx.push_back(1);
				q_idx.push_back(3);
				q_arr[RSE::ARRAY_VERTEX] = q_vtx;
				q_arr[RSE::ARRAY_INDEX] = q_idx;
				Ref<ArrayMesh> fixture_quad;
				fixture_quad.instantiate();
				fixture_quad->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, q_arr);

				auto make_shader_mat = [](const String &p_code) -> Ref<ShaderMaterial> {
					Ref<Shader> sh;
					sh.instantiate();
					sh->set_code(p_code);
					Ref<ShaderMaterial> mat;
					mat.instantiate();
					mat->set_shader(sh);
					return mat;
				};

				Ref<ShaderMaterial> mat_soft_smoke = make_shader_mat(
						"shader_type spatial;\n"
						"render_mode unshaded, blend_mix, depth_draw_never, cull_disabled;\n"
						"uniform sampler2D depth_tex : hint_depth_texture;\n"
						"varying vec4 v_c0; varying vec4 v_c1;\n"
						"void vertex() { v_c0 = CUSTOM0; v_c1 = CUSTOM1; }\n"
						"void fragment() { float d = texture(depth_tex, SCREEN_UV).r; ALBEDO = v_c0.rgb + v_c1.rgb * d; ALPHA = v_c0.a; }\n");

				Ref<ShaderMaterial> mat_refract = make_shader_mat(
						"shader_type spatial;\n"
						"render_mode unshaded, blend_mix, depth_draw_never, cull_disabled;\n"
						"uniform sampler2D screen_tex : hint_screen_texture;\n"
						"uniform sampler2D depth_tex : hint_depth_texture;\n"
						"varying vec4 v_c0; varying vec4 v_c1;\n"
						"void vertex() { v_c0 = CUSTOM0; v_c1 = CUSTOM1; }\n"
						"void fragment() { ALBEDO = texture(screen_tex, SCREEN_UV + v_c0.xy * 0.01).rgb * v_c1.rgb; ALPHA = texture(depth_tex, SCREEN_UV).r * 0.5 + 0.25; }\n");

				Ref<ShaderMaterial> mat_icon_no_depth_test = make_shader_mat(
						"shader_type spatial;\n"
						"render_mode unshaded, blend_mix, depth_test_disabled, cull_disabled;\n"
						"varying vec4 v_c0; varying vec4 v_c1;\n"
						"void vertex() { v_c0 = CUSTOM0; v_c1 = CUSTOM1; }\n"
						"void fragment() { ALBEDO = v_c0.rgb + v_c1.rgb; ALPHA = v_c0.a; }\n");

				Ref<ShaderMaterial> mat_depth_inverted = make_shader_mat(
						"shader_type spatial;\n"
						"render_mode unshaded, blend_mix, depth_test_inverted, cull_disabled;\n"
						"varying vec4 v_c0;\n"
						"void vertex() { v_c0 = CUSTOM0; }\n"
						"void fragment() { ALBEDO = v_c0.rgb; ALPHA = 0.5; }\n");

				Ref<ShaderMaterial> mat_vtx_aux = make_shader_mat(
						"shader_type spatial;\n"
						"render_mode unshaded, blend_mix, cull_disabled;\n"
						"void vertex() { VERTEX.x += CUSTOM0.x; }\n"
						"void fragment() { ALBEDO = vec3(1.0); ALPHA = 0.5; }\n");

				// Negative Action-7 source-gate checks on depth_test_inverted (DEPTH_WRITE = 32) & VERTEX.x += CUSTOM0.x (VERTEX_WRITE = 1).
				run_source_gate_rejection("gate_shader_depth_test_inverted", 6001, mat_depth_inverted->get_rid(), fixture_quad->get_rid(), 1u << 5);
				run_source_gate_rejection("gate_shader_vertex_write_custom0", 6002, mat_vtx_aux->get_rid(), fixture_quad->get_rid(), 1u << 0);
			}
		}

		server->sync();
		enrollment.cleanup_owner(true);
		server->sync();

		const auto final_counts = singleton_counts();
		record_check("zero_singleton_leaks",
				final_counts.records == baseline_counts.records &&
						final_counts.views == baseline_counts.views &&
						final_counts.devices == baseline_counts.devices &&
						final_counts.device_refs == baseline_counts.device_refs &&
						final_counts.attachments == baseline_counts.attachments &&
						final_counts.private_refs == baseline_counts.private_refs &&
						final_counts.external_refs == baseline_counts.external_refs);
		record_check("total_scene_draws_remained_zero", Engine::get_singleton()->get_frames_drawn() == 0);
		record_check("total_rd_frames_drawn_unchanged", device->get_frames_drawn() == initial_rd_draws);

		// Post-Action7 renderer roster verification: CUSTOM0/CUSTOM1 Set 2 binding 2 roster planning (allowed_mask = 412)
		// + depth_test_disabled pass vs depth_test_inverted (32) & VERTEX write (1) rejection.
		{
			static constexpr uint32_t kAllowedTransparentAuxMask = 412u; // ALPHA(4) | DISCARD(8) | TIME(16) | DEPTH_MODE(128) | FRAMEBUFFER_READ(256)
			{
				Array q_arr;
				q_arr.resize(RSE::ARRAY_MAX);
				PackedVector3Array q_vtx;
				q_vtx.push_back(Vector3(-0.5f, -0.5f, 0.0f));
				q_vtx.push_back(Vector3(0.5f, -0.5f, 0.0f));
				q_vtx.push_back(Vector3(-0.5f, 0.5f, 0.0f));
				q_vtx.push_back(Vector3(0.5f, 0.5f, 0.0f));
				PackedInt32Array q_idx;
				q_idx.push_back(0);
				q_idx.push_back(1);
				q_idx.push_back(2);
				q_idx.push_back(2);
				q_idx.push_back(1);
				q_idx.push_back(3);
				q_arr[RSE::ARRAY_VERTEX] = q_vtx;
				q_arr[RSE::ARRAY_INDEX] = q_idx;
				Ref<ArrayMesh> fixture_quad;
				fixture_quad.instantiate();
				fixture_quad->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, q_arr);

				auto make_shader_mat = [](const String &p_code) -> Ref<ShaderMaterial> {
					Ref<Shader> sh;
					sh.instantiate();
					sh->set_code(p_code);
					Ref<ShaderMaterial> mat;
					mat.instantiate();
					mat->set_shader(sh);
					return mat;
				};

				Ref<ShaderMaterial> mat_soft_smoke = make_shader_mat(
						"shader_type spatial;\n"
						"render_mode unshaded, blend_mix, depth_draw_never, cull_disabled;\n"
						"uniform sampler2D depth_tex : hint_depth_texture;\n"
						"varying vec4 v_c0; varying vec4 v_c1;\n"
						"void vertex() { v_c0 = CUSTOM0; v_c1 = CUSTOM1; }\n"
						"void fragment() { float d = texture(depth_tex, SCREEN_UV).r; ALBEDO = v_c0.rgb + v_c1.rgb * d; ALPHA = v_c0.a; }\n");

				Ref<ShaderMaterial> mat_refract = make_shader_mat(
						"shader_type spatial;\n"
						"render_mode unshaded, blend_mix, depth_draw_never, cull_disabled;\n"
						"uniform sampler2D screen_tex : hint_screen_texture;\n"
						"uniform sampler2D depth_tex : hint_depth_texture;\n"
						"varying vec4 v_c0; varying vec4 v_c1;\n"
						"void vertex() { v_c0 = CUSTOM0; v_c1 = CUSTOM1; }\n"
						"void fragment() { ALBEDO = texture(screen_tex, SCREEN_UV + v_c0.xy * 0.01).rgb * v_c1.rgb; ALPHA = texture(depth_tex, SCREEN_UV).r * 0.5 + 0.25; }\n");

				Ref<ShaderMaterial> mat_icon_no_depth_test = make_shader_mat(
						"shader_type spatial;\n"
						"render_mode unshaded, blend_mix, depth_test_disabled, cull_disabled;\n"
						"varying vec4 v_c0; varying vec4 v_c1;\n"
						"void vertex() { v_c0 = CUSTOM0; v_c1 = CUSTOM1; }\n"
						"void fragment() { ALBEDO = v_c0.rgb + v_c1.rgb; ALPHA = v_c0.a; }\n");

				Ref<ShaderMaterial> mat_depth_inverted = make_shader_mat(
						"shader_type spatial;\n"
						"render_mode unshaded, blend_mix, depth_test_inverted, cull_disabled;\n"
						"varying vec4 v_c0;\n"
						"void vertex() { v_c0 = CUSTOM0; }\n"
						"void fragment() { ALBEDO = v_c0.rgb; ALPHA = 0.5; }\n");

				Ref<ShaderMaterial> mat_vtx_aux = make_shader_mat(
						"shader_type spatial;\n"
						"render_mode unshaded, blend_mix, cull_disabled;\n"
						"void vertex() { VERTEX.x += CUSTOM0.x; }\n"
						"void fragment() { ALBEDO = vec3(1.0); ALPHA = 0.5; }\n");

				struct RtRosterProbe {
					RID mesh;
					RID soft_smoke;
					RID refract;
					RID icon_disabled;
					RID depth_inverted;
					RID vtx_aux;
					bool smoke_ok = false;
					bool refract_ok = false;
					bool icon_ok = false;
					bool icon_gate_exact = false;
					bool aux_set2_binding2_found = false;
					bool inverted_rejected = false;
					bool vtx_rejected = false;
				} rp;
				rp.mesh = fixture_quad->get_rid();
				rp.soft_smoke = mat_soft_smoke->get_rid();
				rp.refract = mat_refract->get_rid();
				rp.icon_disabled = mat_icon_no_depth_test->get_rid();
				rp.depth_inverted = mat_depth_inverted->get_rid();
				rp.vtx_aux = mat_vtx_aux->get_rid();

				server->call_on_render_thread(callable_mp_static((+[](uint64_t p_rp_addr) {
					auto *p = reinterpret_cast<RtRosterProbe *>(uintptr_t(p_rp_addr));
					auto *rd = RenderingDevice::get_singleton();
					auto *ms = RendererRD::MeshStorage::get_singleton();
					auto *mats = RendererRD::MaterialStorage::get_singleton();
					auto *sr = RendererSceneRenderRD::get_singleton();
					if (!rd || !ms || !mats || !sr) {
						return;
					}
					mats->_update_queued_materials();
					const auto *const_ms = ms;
					(void)const_ms->mesh_get_surface(p->mesh, 0);
					if (auto *m = ms->mesh_owner.get_or_null(p->mesh)) {
						if (m->surface_count > 0 && m->surfaces[0]) {
							rd->_native_settle_transfer_worker(m->surfaces[0]->vertex_buffer);
							rd->_native_settle_transfer_worker(m->surfaces[0]->index_buffer);
							rd->_native_settle_transfer_worker(m->surfaces[0]->index_array);
						}
					}
					if (sr->ensure_native_material_recipe_sources_ready() != OK) {
						return;
					}

					Vector<RD::AttachmentFormat> atts;
					RD::AttachmentFormat c_af;
					c_af.format = RD::DATA_FORMAT_R8G8B8A8_UNORM;
					c_af.usage_flags = RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RD::TEXTURE_USAGE_SAMPLING_BIT;
					RD::AttachmentFormat d_af;
					d_af.format = RD::DATA_FORMAT_D32_SFLOAT;
					d_af.usage_flags = RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | RD::TEXTURE_USAGE_CAN_COPY_FROM_BIT;
					atts.push_back(c_af);
					atts.push_back(d_af);
					const RD::FramebufferFormatID fb_fmt = rd->framebuffer_format_create(atts);

					auto plan_one = [&](RID p_mat, bool &r_has_set2_b2, uint32_t &r_rej) -> Error {
						r_has_set2_b2 = false;
						r_rej = UINT32_MAX;
						if (auto *sd = mats->material_get_shader_data(p_mat)) {
							RendererRD::MaterialStorage::ShaderData::NativeInstance192ShaderFeatures feat;
							if (sd->get_native_instance192_shader_features(feat) == OK) {
								r_rej = feat.material_recipe_rejections;
							}
						}
						Error we = sr->warm_native_material_instance192_shader(p_mat, RSE::PRIMITIVE_TRIANGLES, kAllowedTransparentAuxMask);
						if (we != OK) {
							return we;
						}
						RendererRD::NativeMaterialRecipeRoster roster;
						Error pe = sr->plan_native_material_recipe(p_mat, p->mesh, 0, fb_fmt, roster, 256, kAllowedTransparentAuxMask);
						if (pe == OK) {
							r_has_set2_b2 = roster.has_binding(2u, 2u, RD::UNIFORM_TYPE_STORAGE_BUFFER);
							(void)roster.release();
						}
						return pe;
					};

					bool b2_smoke = false, b2_refract = false, b2_icon = false, b2_unused = false;
					uint32_t rej_smoke = 0, rej_refract = 0, rej_icon = 0, rej_inv = 0, rej_vtx = 0;
					p->smoke_ok = (plan_one(p->soft_smoke, b2_smoke, rej_smoke) == OK) && ((rej_smoke & ~kAllowedTransparentAuxMask) == 0u);
					p->refract_ok = (plan_one(p->refract, b2_refract, rej_refract) == OK) && ((rej_refract & ~kAllowedTransparentAuxMask) == 0u);
					p->icon_ok = (plan_one(p->icon_disabled, b2_icon, rej_icon) == OK);
					p->icon_gate_exact = ((rej_icon & 128u) == 128u) && ((rej_icon & 32u) == 0u) && ((rej_icon & ~kAllowedTransparentAuxMask) == 0u);
					p->aux_set2_binding2_found = b2_smoke && b2_refract && b2_icon;
					p->inverted_rejected = (plan_one(p->depth_inverted, b2_unused, rej_inv) == ERR_UNAVAILABLE) && ((rej_inv & 32u) == 32u);
					p->vtx_rejected = (plan_one(p->vtx_aux, b2_unused, rej_vtx) == ERR_UNAVAILABLE) && ((rej_vtx & 1u) == 1u);
				})).bind(uint64_t(uintptr_t(&rp))));
				server->sync();

				record_check("role9_soft_smoke_roster_ok", rp.smoke_ok);
				record_check("role9_refract_roster_ok", rp.refract_ok);
				record_check("role9_depth_test_disabled_icon_roster_ok", rp.icon_ok && rp.icon_gate_exact);
				record_check("role9_set2_binding2_reflected_in_roster", rp.aux_set2_binding2_found);
				record_check("role9_depth_test_inverted_rejected_32", rp.inverted_rejected);
				record_check("role9_vertex_write_custom0_rejected_1", rp.vtx_rejected);
			}

			server->call_on_render_thread(callable_mp_static(+[]() {
				if (RenderingDevice *rd = RenderingDevice::get_singleton()) {
					for (uint32_t f = 0; f < 4; ++f) {
						rd->_submit_transfer_workers();
						rd->_wait_for_transfer_workers();
						rd->swap_buffers(false);
					}
				}
			}));
			server->sync();
			const auto post_roster_counts = singleton_counts();
			record_check("zero_singleton_leaks_after_roster_probe",
					post_roster_counts.records == baseline_counts.records &&
							post_roster_counts.views == baseline_counts.views &&
							post_roster_counts.devices == baseline_counts.devices &&
							post_roster_counts.device_refs == baseline_counts.device_refs &&
							post_roster_counts.attachments == baseline_counts.attachments &&
							post_roster_counts.private_refs == baseline_counts.private_refs &&
							post_roster_counts.external_refs == baseline_counts.external_refs);
			record_check("post_roster_scene_draws_remained_zero", Engine::get_singleton()->get_frames_drawn() == 0);
		}

		out["pass"] = failures.is_empty();
		out["check_count"] = checks.size();
		out["checks"] = checks;
		out["failures"] = failures;
		out["diagnostics"] = diagnostics;
		out["frames_drawn"] = int64_t(Engine::get_singleton()->get_frames_drawn());
		out["rd_frames_drawn"] = int64_t(device->get_frames_drawn());
		out["initial_rd_frames_drawn"] = int64_t(initial_rd_draws);
		return out;
	}
};
