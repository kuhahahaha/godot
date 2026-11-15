// Included inside TypedRendererProducer. Draft implementation: genuine staged
// Core executor, qualification Unit, same original attachment. No Runtime owner.
#include "tests/servers/rendering/fixtures/typed_renderer_native/staged_writer_source.inc"
bool staged_enabled = false;
bool staged_bank_refused = false;
bool staged_bank_probe() const {
	return staged_enabled && OS::get_singleton()->get_environment("GODOT_TEST_STAGED_ACQUIRE_BANK_REFUSAL") == "1";
}
bool staged_genuine_prior() const {
	return staged_enabled && !staged_bank_probe() && OS::get_singleton()->get_environment("GODOT_TEST_STAGED_GENUINE_PRIOR") == "1";
}
Error staged_error = OK;
String staged_detail;
Vector<uint8_t> staged_program;
GDExtensionNativeUnitCreateRequestV1 staged_unit_origin{};
GDExtensionNativeUnitBackingV1 staged_unit{};
GDExtensionStorageCreateRequestV1 staged_control_origin{};
GDExtensionStorageBackingV1 staged_control{};
GDExtensionProductionActionV2 staged_actions[7]{};
GDExtensionProductionResourceV1 staged_resources[8]{};
GDExtensionProductionUseV1 staged_uses[12]{};
GDExtensionProductionReadbackSourceV1 staged_readbacks[2]{};
GDExtensionProductionUploadV2 staged_upload{};
GDExtensionProductionOutputDemandInputV1 staged_demand_input{};
GDExtensionProductionOutputGuardDemandV1 staged_demand{};
GDExtensionProductionMetadataQuoteV1 staged_quote{};
GDExtensionProductionOutputPlanV1 staged_plan{};
GDExtensionNormalRelayRefV1 staged_relay{};
uint32_t staged_compat_calls = 0;
static constexpr uint32_t STAGED_READ_CAPACITY = 1;
uint32_t staged_read_capacity() const { return staged_bank_probe() ? 0 : STAGED_READ_CAPACITY; }
static constexpr uint32_t STAGED_MAX_DRAW_GRAPHS = 2 * RenderingDevice::MAX_UNIFORM_SETS + 6;
// The original Acquire also uses its read-graph allowance to fund each preparation hold.
// The selected Mobile material closure measured 106 resources.
static constexpr uint32_t STAGED_PREPARATION_RESOURCE_CAPACITY = 106;
uint32_t staged_read_graph_capacity() const { return staged_bank_probe() ? 0 : STAGED_PREPARATION_RESOURCE_CAPACITY; }
Error staged_fail(Error value, const char *where) {
	if (value != OK && value != ERR_BUSY && staged_error == OK) { staged_error = value; staged_detail = where; }
	return value;
}
// CPU compiler -> actual current driver's native container; qualification still
// performs original byte/digest/flags/reflection checks before native creation.
Error compile_staged_program() {
	if (!staged_program.is_empty()) { return OK; }
	String diagnostic;
	const Vector<uint8_t> spirv = device->shader_compile_spirv_from_source(RenderingDevice::SHADER_STAGE_COMPUTE,
		String::utf8(STAGED_WRITER_SHADER), RenderingDevice::SHADER_LANGUAGE_GLSL, &diagnostic, false);
	if (spirv.is_empty() || !diagnostic.is_empty()) { return staged_fail(ERR_COMPILATION_FAILED, "staged_actual_spirv_compile"); }
	Vector<RenderingDevice::ShaderStageSPIRVData> stages;
	RenderingDevice::ShaderStageSPIRVData code;
	code.shader_stage = RenderingDevice::SHADER_STAGE_COMPUTE; code.spirv = spirv; stages.push_back(code);
	staged_program = device->shader_compile_binary_from_spirv_with_options(stages, "typed_staged_writer", RenderingDevice::SHADER_COMPILE_FLAG_DISABLE_FAST_MATH);
	if (staged_program.is_empty()) { return staged_fail(ERR_COMPILATION_FAILED, "staged_actual_native_container"); }
	staged_unit_origin.abi = Enrollment::ABI;
	staged_unit_origin.library_generation = enrollment.holder.generation;
	staged_unit_origin.preparation_generation = 1; staged_unit_origin.operation_serial = 1;
	staged_unit_origin.record_slot = 5; staged_unit_origin.physical_unit = 0;
	staged_unit_origin.expected_flags = RenderingDevice::SHADER_COMPILE_FLAG_DISABLE_FAST_MATH;
	staged_unit_origin.container_byte_count = staged_program.size();
	return staged_fail(CryptoCore::sha256(staged_program.ptr(), staged_program.size(), staged_unit_origin.container_digest), "staged_actual_container_digest");
}
Error fund_staged_pool(OrdinalPlan &pool, const OrdinalPlan &producer) {
	using namespace GDExtensionDeviceExit;
	// Same actual single flattened triangle surface and original metadata limits
	// already selected for CreateV3. No live generation demand is forged here.
	using Guard = RendererRD::ParticlesExternalGuard;
	ProductionOutputGuardBound guard;
	const uint64_t proof_bytes = sizeof(Guard::GeometryProofHeader) + sizeof(Guard::SurfaceProof);
	Error e = production_output_guard_bound(create_input.config.capacity, 1,
		device->limit_get(RenderingDevice::LIMIT_MAX_COMPUTE_WORKGROUP_COUNT_X), proof_bytes,
		device->upload_staging_buffers.block_size, guard);
	if (e != OK) { return staged_fail(e, "staged_pool_guard_bound"); }
	uint32_t upload_graphs = 0, guard_read_graphs = 0, record_read_graphs = 0;
	if ((e = production_staging_graph_bound(12, device->upload_staging_buffers.block_size, upload_graphs)) != OK ||
		(e = production_staging_graph_bound(32, device->download_staging_buffers.block_size, guard_read_graphs)) != OK ||
		(e = production_staging_graph_bound(192, device->download_staging_buffers.block_size, record_read_graphs)) != OK) { return staged_fail(e, "staged_pool_transfer_bounds"); }
	uint32_t instance192_resources = 0;
	if ((e = RendererRD::ExternalLinkedOwnerTest::staged_instance192_resource_count(create_input.config.capacity, 1, instance192_resources)) != OK) { return staged_fail(e, "staged_pool_instance192_layout"); }
	const uint64_t borrower_cells = staged_genuine_prior() ? 6 : 3;
	const uint64_t demand_use_cells = 1 + uint64_t(instance192_resources) + 1 + borrower_cells + guard.guard_owned_resources + 1 + 2 * uint64_t(STAGED_PREPARATION_RESOURCE_CAPACITY) + 6;
	const uint64_t output_graphs = uint64_t(upload_graphs) + 1 + guard.guard_graphs + STAGED_PREPARATION_RESOURCE_CAPACITY + guard_read_graphs + record_read_graphs;
	// Original demand formula: slot + resources/group + actual quoted borrower
	// headroom(view/surface/read) + guard group, then per-read graph/use rows.
	const uint64_t output_use_cells = 17 + demand_use_cells;
	if (staged_genuine_prior()) {
		const uint64_t successor_graphs = output_graphs + record_read_graphs;
		const uint64_t successor_use_cells = 24 + demand_use_cells;
		const uint64_t combined_graphs = output_graphs + successor_graphs;
		const uint64_t combined_uses = output_use_cells + successor_use_cells;
		if (combined_graphs > UINT32_MAX - 3 * uint64_t(producer.capacity.graph_memberships) ||
				combined_uses > UINT32_MAX - producer.capacity.resource_uses) {
			return staged_fail(ERR_PARAMETER_RANGE_ERROR, "staged_pool_bounds_overflow");
		}
		pool = producer;
		pool.attachment_requests = 3;
		pool.capacity.suboperations = producer.capacity.suboperations + 7 + 8;
		pool.capacity.resources = producer.capacity.resources + 8 + 10;
		pool.capacity.resource_uses = producer.capacity.resource_uses + uint32_t(combined_uses);
		pool.capacity.return_cells = producer.capacity.return_cells + 8 + 10;
		pool.capacity.retained_payload_bytes = producer.capacity.retained_payload_bytes + 108 + 108;
		pool.capacity.readbacks = 2 + 3;
		pool.capacity.readback_bytes = 224 + 416;
		pool.capacity.graph_memberships = 3 * producer.capacity.graph_memberships + uint32_t(combined_graphs);
		return staged_fail(enrollment.registry.production_membership_bound_locked(*device, pool.capacity.graph_memberships,
			device->transfer_worker_pool_max_size, pool.capacity.native_memberships), "staged_pool_original_native_bound");
	}
	if (output_graphs > UINT32_MAX - 2 * uint64_t(producer.capacity.graph_memberships) || output_use_cells > UINT32_MAX - producer.capacity.resource_uses) {
		return staged_fail(ERR_PARAMETER_RANGE_ERROR, "staged_pool_bounds_overflow");
	}
	pool = producer; pool.attachment_requests = 2;
	pool.capacity.suboperations = producer.capacity.suboperations + 7;
	pool.capacity.resources = producer.capacity.resources + 8;
	pool.capacity.resource_uses = producer.capacity.resource_uses + uint32_t(output_use_cells);
	pool.capacity.return_cells = producer.capacity.return_cells + 8;
	pool.capacity.retained_payload_bytes = producer.capacity.retained_payload_bytes + 108; // actual 96-byte push +12-byte control.
	pool.capacity.readbacks = 2; pool.capacity.readback_bytes = 224;
	pool.capacity.graph_memberships = 2 * producer.capacity.graph_memberships + uint32_t(output_graphs); // Paid late-prefix allowance retained.
	return staged_fail(enrollment.registry.production_membership_bound_locked(*device, pool.capacity.graph_memberships,
		device->transfer_worker_pool_max_size, pool.capacity.native_memberships), "staged_pool_original_native_bound");
}
void declare_staged_rows() {
	using namespace GDExtensionDeviceExit;
	staged_actions[0] = { 0, 11, 10, 0, GDEXTENSION_PRODUCTION_UPLOAD_V1, UINT32_MAX, 0, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX };
	staged_actions[1] = { 1, 11, 11, 0, GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1, UINT32_MAX, 1, 4, GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1, UINT32_MAX };
	staged_actions[2] = { 2, 11, 12, 0, GDEXTENSION_PRODUCTION_DISPATCH_V1, 5, 5, 5, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX };
	staged_actions[3] = { 3, 11, 13, 0, GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1, UINT32_MAX, 0, 0, GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1, UINT32_MAX };
	staged_actions[4] = { 4, 11, 14, 0, GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1, UINT32_MAX, 0, 0, GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1, UINT32_MAX };
	staged_actions[5] = { 5, 11, 15, 0, GDEXTENSION_PRODUCTION_READBACK_V1, UINT32_MAX, 10, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, 0 };
	staged_actions[6] = { 6, 11, 16, 0, GDEXTENSION_PRODUCTION_READBACK_V1, UINT32_MAX, 11, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, 1 };
	staged_resources[0] = { GDEXTENSION_PRODUCTION_STORAGE_V1, source_origin.allocation_slot, source_origin.part, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, source_origin.allocation_generation, source_origin.bytes, 0 };
	staged_resources[1] = { GDEXTENSION_PRODUCTION_STORAGE_V1, 9, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, 12, 0 };
	staged_resources[2] = { GDEXTENSION_PRODUCTION_STORAGE_V1, 1, 0, GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1, created.generation, 192, 0 };
	staged_resources[3] = { GDEXTENSION_PRODUCTION_STORAGE_V1, 2, 0, GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1, created.generation, 192, 0 };
	staged_resources[4] = { GDEXTENSION_PRODUCTION_STORAGE_V1, 7, 0, GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1, created.generation, 32, 0 };
	staged_resources[5] = { GDEXTENSION_PRODUCTION_UNIT_V1, 5, 0, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 1, 0, 0 };
	staged_resources[6] = { GDEXTENSION_PRODUCTION_SET_V1, 6, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, 0, 0 };
	staged_resources[7] = { GDEXTENSION_PRODUCTION_STORAGE_V1, 1, 0, GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1, created.generation, 32, 0 }; // Exact current-role origin for native Guard alias.
	staged_uses[0] = { 1, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 12, 0 };
	staged_uses[1] = { 0, GDEXTENSION_PRODUCTION_READ_V1, 0, 32, 0 };
	staged_uses[2] = { 2, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 192, 0 };
	staged_uses[3] = { 3, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 192, 0 };
	staged_uses[4] = { 4, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 32, 0 };
	staged_uses[5] = { 0, GDEXTENSION_PRODUCTION_READ_V1, 0, 32, 0 };
	staged_uses[6] = { 1, GDEXTENSION_PRODUCTION_READ_V1, 0, 12, 0 };
	staged_uses[7] = { 2, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 192, 0 };
	staged_uses[8] = { 3, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 192, 0 };
	staged_uses[9] = { 4, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 32, 0 };
	staged_uses[10] = { 7, GDEXTENSION_PRODUCTION_READ_V1, 0, 32, 0 };
	staged_uses[11] = { 2, GDEXTENSION_PRODUCTION_READ_V1, 0, 192, 0 };
	staged_readbacks[0] = { 7, GDEXTENSION_PRODUCTION_READBACK_NATIVE_OUTPUT_GUARD_V1, 5, 0, 32, { 3 }, {} };
	staged_readbacks[1] = { 2, GDEXTENSION_PRODUCTION_READBACK_REQUEST_WRITER_V1, 6, 0, 192, { 1 }, {} };
	staged_upload = { production_dto_abi<GDExtensionProductionUploadV2>(), 0, 11, 10, 0, 1, 0, 0, 12 };
	staged_demand_input = { production_dto_abi<GDExtensionProductionOutputDemandInputV1>(), request, created.particles, created.generation,
		1, 11, 3, 11, 4, 11, 11, 13, 14, staged_read_capacity(), staged_read_graph_capacity(), 1, 0 };
}
uint32_t staged_setup_step = 0;
bool staged_source_imported = false, staged_unit_imported = false, staged_setup_complete = false;
GDExtensionDeviceSessionResultV1 staged_quote_result{ ERR_UNCONFIGURED, 0, 0, 0 };
GDExtensionDeviceSessionResultV1 staged_reserve_result{ ERR_UNCONFIGURED, 0, 0, 0 };
Dictionary staged_program_snapshot, staged_reserve_snapshot, staged_resource_snapshot;
static void staged_camera_receiver(void *opaque, GDExtensionProductionAttachmentRefV1 attachment, const GDExtensionProductionCameraSampleV1 *sample) {
	using namespace GDExtensionDeviceExit;
	auto &self = *static_cast<TypedRendererProducer *>(opaque);
	++self.staged_compat_calls;
	const auto *scope = ProductionCameraReadySourceScope::get();
	// The legacy adopted leaf retains its real payload; it never records or
	// claims the formal Core executor facet. Staged recording uses Core only.
	if (!scope || scope->get_registry() != &self.enrollment.registry || scope->get_source().is_null() || !sample ||
		attachment.registry_instance != self.enrollment.attachment.registry_instance || attachment.handle_serial != self.enrollment.attachment.handle_serial) {
		self.staged_fail(ERR_UNAUTHORIZED, "staged_original_camera_relay_scope");
	}
}
Error enroll_staged_relay() {
	const auto relay = port<GDExtensionInterfaceDeviceSessionCameraReadyRelayV1>("device_session_camera_ready_relay_v1");
	const auto bind = port<GDExtensionInterfaceDeviceSessionCameraReadyBindRelayV1>("device_session_camera_ready_bind_relay_v1");
	if (!relay || !bind) { return staged_fail(ERR_UNAVAILABLE, "staged_original_camera_relay_ports"); }
	Error e = Error(relay(reinterpret_cast<GDExtensionConstTypePtr>(&host_normal_callable), &staged_camera_receiver, &staged_relay).native_error);
	if (e != OK) { return staged_fail(e, "staged_original_camera_relay"); }
	return staged_fail(Error(bind(enrollment.attachment, staged_relay).native_error), "staged_original_camera_relay_bind");
}
void snapshot_staged_resources(const char *scope) {
	using namespace GDExtensionDeviceExit;
	Registry::Lock lock(enrollment.registry);
	if (!host_original_request || host_original_request->serial != host_request.handle_serial) { return; }
	Dictionary result; result["scope"] = scope; result["request"] = int64_t(host_original_request->serial);
	result["frozen"] = host_original_request->frozen; result["static_sealed"] = host_original_request->native_static_sealed;
	result["next_action"] = host_original_request->next_action;
	Array rows;
	for (uint32_t i = 0; i < host_original_request->resource_count; ++i) {
		const auto &resource = host_original_request->resources[i]; Dictionary row;
		row["slot"] = i; row["kind"] = resource.declaration.resource_kind; row["ownership"] = resource.declaration.ownership;
		row["original_slot"] = resource.declaration.original_slot; row["generation"] = int64_t(resource.declaration.original_generation);
		row["imported"] = resource.imported; row["primary"] = int64_t(resource.primary.get_id()); row["secondary"] = int64_t(resource.secondary.get_id());
		row["backing_serial"] = int64_t(resource.backing_serial); row["use_count"] = resource.use_count;
		row["output_alias_prepared"] = resource.output_alias_prepared; row["output_set_deferred"] = resource.output_set_deferred;
		row["output_action_ordinal"] = int64_t(resource.output_action_ordinal); row["output_guard_readonly"] = resource.output_guard_readonly;
		bool action0_uses = false;
		const auto &action = host_original_request->actions[0].declaration;
		for (uint32_t u = 0; u < action.use_count; ++u) { action0_uses |= host_original_request->uses[action.first_use + u].declaration.resource_slot == i; }
		row["control_upload_action_uses"] = action0_uses; rows.push_back(row);
	}
	result["resources"] = rows; staged_resource_snapshot = result;
}
void advance_staged_reservation() {
	using namespace GDExtensionDeviceExit;
	if (staged_error != OK || staged_setup_complete) { return; }
	if (staged_setup_step == 0) {
		if (compile_staged_program() == OK) { ++staged_setup_step; }
		return;
	}
	if (staged_setup_step == 1) {
		const auto create = port<GDExtensionInterfaceDeviceSessionQualificationNativeUnitCreateV1>("device_session_qualification_native_unit_create_v1");
		if (!create) { staged_fail(ERR_UNAVAILABLE, "staged_qualification_unit_port"); return; }
		const Error e = Error(create(enrollment.qualification, &staged_unit_origin, staged_program.ptr(), staged_program.size(), &staged_unit).native_error);
		if (staged_fail(e, "staged_original_qualification_unit") != OK) { return; }
		if (staged_unit.state != 1 || staged_unit.native_error != OK || !staged_unit.unit.handle_serial || !staged_unit.shader_rid || !staged_unit.pipeline_rid ||
			staged_unit.effective_flags != RenderingDevice::SHADER_COMPILE_FLAG_DISABLE_FAST_MATH) { staged_fail(ERR_INVALID_DATA, "staged_original_unit_identity"); return; }
		staged_program_snapshot["container_bytes"] = staged_program.size(); staged_program_snapshot["effective_flags"] = staged_unit.effective_flags;
		staged_program_snapshot["unit"] = int64_t(staged_unit.unit.handle_serial); staged_program_snapshot["shader_rid"] = int64_t(staged_unit.shader_rid);
		staged_program_snapshot["pipeline_rid"] = int64_t(staged_unit.pipeline_rid); staged_program_snapshot["shader_serial"] = int64_t(staged_unit.shader_serial);
		staged_program_snapshot["pipeline_serial"] = int64_t(staged_unit.pipeline_serial);
		staged_program_snapshot["digest"] = String::hex_encode_buffer(staged_unit_origin.container_digest, 32);
		const auto *shader = device->shader_owner.get_or_null(RID::from_uint64(staged_unit.shader_rid));
		if (!shader || shader->pipeline_type != RenderingDevice::PIPELINE_TYPE_COMPUTE || shader->push_constant_size != 96 || shader->uniform_sets.size() != 1 ||
			shader->uniform_sets[0].size() != 5 || shader->compute_local_size[0] != 1 || shader->compute_local_size[1] != 1 || shader->compute_local_size[2] != 1) {
			staged_fail(ERR_INVALID_DATA, "staged_native_writer_reflection"); return;
		}
		Array reflected;
		for (uint32_t i = 0; i < 5; ++i) {
			const auto &binding = shader->uniform_sets[0][i];
			if (binding.binding != i || binding.type != RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER || binding.writable != (i >= 2)) {
				staged_fail(ERR_INVALID_DATA, "staged_native_writer_binding"); return;
			}
			Dictionary row;
			row["binding"] = binding.binding;
			row["type"] = binding.type;
			row["writable"] = binding.writable;
			row["reflected_length"] = binding.length;
			reflected.push_back(row);
		}
		staged_program_snapshot["bindings"] = reflected;
		staged_program_snapshot["push_bytes"] = shader->push_constant_size;
		staged_program_snapshot["local_size"] = Vector3i(shader->compute_local_size[0], shader->compute_local_size[1], shader->compute_local_size[2]);
		++staged_setup_step; return;
	}
	if (staged_setup_step == 2) {
		declare_staged_rows();
		const auto quote = port<GDExtensionInterfaceDeviceSessionProductionRequestRequirementsV2>("device_session_production_request_requirements_v2");
		if (!quote) { staged_fail(ERR_UNAVAILABLE, "staged_original_requirements_port"); return; }
		staged_quote_result = quote(enrollment.attachment, staged_actions, 7, staged_resources, 8, staged_uses, 12, &staged_upload, 1,
			staged_readbacks, 2, 5, 108, &staged_demand_input, 1, &staged_demand, 1, &staged_quote);
		if (staged_fail(Error(staged_quote_result.native_error), "staged_original_live_generation_quote") == OK) { ++staged_setup_step; }
		return;
	}
	if (staged_setup_step == 3) {
		const auto reserve = port<GDExtensionInterfaceDeviceSessionProductionReserveReadbackSourcesV1>("device_session_production_reserve_readback_sources_v1");
		if (!reserve) { staged_fail(ERR_UNAVAILABLE, "staged_original_reserve_port"); return; }
		auto operation = enrollment.request->operation;
		if (operation.owner_request_nonce > UINT64_MAX - 2) { staged_fail(ERR_PARAMETER_RANGE_ERROR, "staged_nonce_exhausted"); return; }
		operation.owner_request_nonce += 2; host_capacity = staged_quote.capacity;
		staged_reserve_snapshot = snapshot_source_reader_reserve(&host_capacity, true);
		staged_reserve_result = reserve(enrollment.attachment, &operation, &host_capacity, staged_actions, 7, staged_resources, 8,
			staged_uses, 12, staged_readbacks, 2, &host_request);
		if (staged_fail(Error(staged_reserve_result.native_error), "staged_original_reserve") != OK) { return; }
		RenderingDevice *actual = nullptr;
		{ Registry::Lock lock(enrollment.registry);
			const Error e = enrollment.registry.production_request_locked(host_request, false, host_original_request, actual);
			if (staged_fail(e, "staged_original_request") != OK) { return; }
			if (!host_original_request || actual != device || host_original_request == enrollment.request || host_original_request->attachment != enrollment.request->attachment) { staged_fail(ERR_INVALID_DATA, "staged_same_attachment"); return; }
		}
		++staged_setup_step; return;
	}
	if (staged_setup_step == 4) {
		const auto create = port<GDExtensionInterfaceDeviceSessionProductionStorageCreateV1>("device_session_production_storage_create_v1");
		if (!create) { staged_fail(ERR_UNAVAILABLE, "staged_control_factory_port"); return; }
		staged_control_origin = { Enrollment::ABI, 12, 9, 0, 1, 1, 0 };
		if (staged_fail(Error(create(host_request, 1, &staged_control_origin, &staged_control).native_error), "staged_original_control_create") == OK) { ++staged_setup_step; }
		return;
	}
	if (staged_setup_step == 5) {
		const auto import = port<GDExtensionInterfaceDeviceSessionProductionImportStorageV1>("device_session_production_import_storage_v1");
		if (!import) { staged_fail(ERR_UNAVAILABLE, "staged_source_import_port"); return; }
		if (staged_fail(Error(import(host_request, 0, &source).native_error), "staged_original_source_import") == OK) { staged_source_imported = true; ++staged_setup_step; }
		return;
	}
	if (staged_setup_step == 6) {
		const auto import = port<GDExtensionInterfaceDeviceSessionProductionImportUnitV1>("device_session_production_import_unit_v1");
		if (!import) { staged_fail(ERR_UNAVAILABLE, "staged_unit_import_port"); return; }
		if (staged_fail(Error(import(host_request, 5, &staged_unit).native_error), "staged_original_unit_import") != OK) { return; }
		staged_unit_imported = true;
		if (!staged_genuine_prior()) {
			staged_setup_complete = true;
			host_scene_step = 4; // Reuse actual RS Host instance + original ordinary preparation next.
			return;
		}
		++staged_setup_step;
		return;
	}
	if (staged_setup_step == 7) {
		if (compile_staged_prior_program() == OK) { ++staged_setup_step; }
		return;
	}
	if (staged_setup_step == 8) {
		const auto create = port<GDExtensionInterfaceDeviceSessionQualificationNativeUnitCreateV1>("device_session_qualification_native_unit_create_v1");
		if (!create) { staged_fail(ERR_UNAVAILABLE, "staged_prior_qualification_unit_port"); return; }
		const Error e = Error(create(enrollment.qualification, &staged_prior_unit_origin, staged_prior_program.ptr(), staged_prior_program.size(), &staged_prior_unit).native_error);
		if (staged_fail(e, "staged_prior_qualification_unit") != OK) { return; }
		if (staged_prior_unit.state != 1 || staged_prior_unit.native_error != OK || !staged_prior_unit.unit.handle_serial || !staged_prior_unit.shader_rid || !staged_prior_unit.pipeline_rid ||
			staged_prior_unit.effective_flags != RenderingDevice::SHADER_COMPILE_FLAG_DISABLE_FAST_MATH) { staged_fail(ERR_INVALID_DATA, "staged_prior_unit_identity"); return; }
		const auto *shader = device->shader_owner.get_or_null(RID::from_uint64(staged_prior_unit.shader_rid));
		if (!shader || shader->pipeline_type != RenderingDevice::PIPELINE_TYPE_COMPUTE || shader->push_constant_size != 96 || shader->uniform_sets.size() != 1 ||
			shader->uniform_sets[0].size() != 7 || shader->compute_local_size[0] != 1 || shader->compute_local_size[1] != 1 || shader->compute_local_size[2] != 1) {
			staged_fail(ERR_INVALID_DATA, "staged_prior_writer_reflection"); return;
		}
		for (uint32_t i = 0; i < 7; ++i) {
			const auto &binding = shader->uniform_sets[0][i];
			const bool expected_writable = (i >= 2 && i <= 4);
			if (binding.binding != i || binding.type != RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER || binding.writable != expected_writable) {
				staged_fail(ERR_INVALID_DATA, "staged_prior_writer_binding"); return;
			}
		}
		staged_prior_program_snapshot["container_bytes"] = staged_prior_program.size();
		staged_prior_program_snapshot["unit"] = int64_t(staged_prior_unit.unit.handle_serial);
		staged_prior_program_snapshot["shader_rid"] = int64_t(staged_prior_unit.shader_rid);
		staged_prior_program_snapshot["pipeline_rid"] = int64_t(staged_prior_unit.pipeline_rid);
		staged_prior_program_snapshot["shader_serial"] = int64_t(staged_prior_unit.shader_serial);
		staged_prior_program_snapshot["pipeline_serial"] = int64_t(staged_prior_unit.pipeline_serial);
		staged_prior_program_snapshot["digest"] = String::hex_encode_buffer(staged_prior_unit_origin.container_digest, 32);
		++staged_setup_step; return;
	}
	if (staged_setup_step == 9) {
		declare_staged_successor_rows();
		const auto quote = port<GDExtensionInterfaceDeviceSessionProductionRequestRequirementsV2>("device_session_production_request_requirements_v2");
		if (!quote) { staged_fail(ERR_UNAVAILABLE, "staged_successor_requirements_port"); return; }
		staged_successor_quote_result = quote(enrollment.attachment, staged_successor_actions, 8, staged_successor_resources, 10,
			staged_successor_uses, 17, &staged_successor_upload, 1, staged_successor_readbacks, 3, 7, 108,
			&staged_successor_demand_input, 1, &staged_successor_demand, 1, &staged_successor_quote);
		if (staged_fail(Error(staged_successor_quote_result.native_error), "staged_successor_live_generation_quote") == OK) { ++staged_setup_step; }
		return;
	}
	if (staged_setup_step == 10) {
		const auto reserve = port<GDExtensionInterfaceDeviceSessionProductionReserveReadbackSourcesV1>("device_session_production_reserve_readback_sources_v1");
		if (!reserve) { staged_fail(ERR_UNAVAILABLE, "staged_successor_reserve_port"); return; }
		auto operation = enrollment.request->operation;
		if (operation.owner_request_nonce > UINT64_MAX - 4) { staged_fail(ERR_PARAMETER_RANGE_ERROR, "staged_successor_nonce_exhausted"); return; }
		operation.owner_request_nonce += 4;
		staged_successor_capacity = staged_successor_quote.capacity;
		staged_successor_reserve_result = reserve(enrollment.attachment, &operation, &staged_successor_capacity, staged_successor_actions, 8,
			staged_successor_resources, 10, staged_successor_uses, 17, staged_successor_readbacks, 3, &staged_successor_request);
		if (staged_fail(Error(staged_successor_reserve_result.native_error), "staged_successor_reserve") != OK) { return; }
		RenderingDevice *actual = nullptr;
		{ Registry::Lock lock(enrollment.registry);
			const Error e = enrollment.registry.production_request_locked(staged_successor_request, false, staged_successor_original_request, actual);
			if (staged_fail(e, "staged_successor_original_request") != OK) { return; }
			if (!staged_successor_original_request || actual != device || staged_successor_original_request == enrollment.request ||
				staged_successor_original_request == host_original_request || staged_successor_original_request->attachment != enrollment.request->attachment) {
				staged_fail(ERR_INVALID_DATA, "staged_successor_same_attachment"); return;
			}
		}
		++staged_setup_step; return;
	}
	if (staged_setup_step == 11) {
		const auto create = port<GDExtensionInterfaceDeviceSessionProductionStorageCreateV1>("device_session_production_storage_create_v1");
		if (!create) { staged_fail(ERR_UNAVAILABLE, "staged_successor_control_factory_port"); return; }
		staged_successor_control_origin = { Enrollment::ABI, 12, 10, 0, 1, 1, 0 };
		if (staged_fail(Error(create(staged_successor_request, 1, &staged_successor_control_origin, &staged_successor_control).native_error), "staged_successor_control_create") == OK) { ++staged_setup_step; }
		return;
	}
	if (staged_setup_step == 12) {
		const auto import = port<GDExtensionInterfaceDeviceSessionProductionImportStorageV1>("device_session_production_import_storage_v1");
		if (!import) { staged_fail(ERR_UNAVAILABLE, "staged_successor_source_import_port"); return; }
		if (staged_fail(Error(import(staged_successor_request, 0, &source).native_error), "staged_successor_source_import") == OK) { staged_successor_source_imported = true; ++staged_setup_step; }
		return;
	}
	if (staged_setup_step == 13) {
		const auto import = port<GDExtensionInterfaceDeviceSessionProductionImportUnitV1>("device_session_production_import_unit_v1");
		if (!import) { staged_fail(ERR_UNAVAILABLE, "staged_successor_unit_import_port"); return; }
		if (staged_fail(Error(import(staged_successor_request, 5, &staged_prior_unit).native_error), "staged_successor_unit_import") != OK) { return; }
		staged_successor_unit_imported = true;
		staged_setup_complete = true;
		host_scene_step = 4; // Reuse actual RS Host instance + original ordinary preparation next.
	}
}

uint32_t staged_execution_step = 0, staged_batch_count = 0, staged_close_step = 0;
uint64_t staged_record_control = 0, staged_batch_cursor = 0, staged_cancel_control = 0;
uint64_t staged_cancel_frame = 0, staged_ack_frame = 0;
bool staged_native_ready = false, staged_record_consumed = false, staged_readbacks_verified = false;
bool staged_raster_verified = false;
Error staged_raster_error = OK;
String staged_raster_detail;
bool staged_cancel_entered = false, staged_relay_returned = false;
uint32_t staged_unread_after = UINT32_MAX;
uint64_t staged_pending_ack_after = UINT64_MAX;
GDExtensionDeviceSessionResultV1 staged_observe_api_result{ ERR_UNCONFIGURED, 0, 0, 0 };
GDExtensionDeviceSessionResultV1 staged_control_observe_api_result{ ERR_UNCONFIGURED, 0, 0, 0 };
GDExtensionProductionControlReceiptV1 staged_observe_receipt{}, staged_control_observe_receipt{};
bool staged_slot_owner_returned = false;

bool staged_observe_control_receipt(uint64_t expected, GDExtensionProductionControlReceiptV1 &receipt) {
	using namespace GDExtensionDeviceExit;
	const auto read = port<GDExtensionInterfaceDeviceSessionProductionControlReceiptV1>("device_session_production_control_receipt_v1");
	if (!read) { staged_fail(ERR_UNAVAILABLE, "staged_observe_control_receipt_port"); return false; }
	const Error error = Error(read(host_request, expected, &receipt).native_error);
	if (staged_fail(error, "staged_original_observe_control_receipt") != OK) { return false; }
	if (!production_dto_valid(receipt) || receipt.registry_instance != host_request.registry_instance || receipt.request_serial != host_request.handle_serial) {
		staged_fail(ERR_INVALID_DATA, "staged_observe_control_receipt_identity"); return false;
	}
	return true;
}

bool staged_consumed_observe_turn(const GDExtensionProductionControlReceiptV1 &before,
		const GDExtensionDeviceSessionResultV1 &result, GDExtensionProductionControlReceiptV1 &after) {
	// A fresh publication or an outstanding control can also return BUSY.
	// Require the exact previously outstanding OBSERVE to have returned to its owner.
	if (!before.control_serial || before.kind != GDEXTENSION_PRODUCTION_CONTROL_RECEIPT_KIND_OBSERVE_V1 ||
			before.state != GDEXTENSION_PRODUCTION_CONTROL_RECEIPT_RETURNED_V1) { return false; }
	if (!staged_observe_control_receipt(before.control_serial, after)) { return false; }
	if (after.state != GDEXTENSION_PRODUCTION_CONTROL_RECEIPT_OWNER_CONSUMED_V1) { return false; }
	if (after.kind != before.kind || after.control_serial != before.control_serial || !after.result_valid || after.control_result.native_error != result.native_error) {
		staged_fail(ERR_INVALID_DATA, "staged_observe_consumed_result_identity"); return false;
	}
	return true;
}
GDExtensionProductionServiceBudgetV1 staged_record_budget{};
GDExtensionProductionRecordReportV3 staged_record{};
GDExtensionProductionRecordReportV2 staged_cancel_receipt{};
GDExtensionProductionNativeBatchItemV1 staged_batch_item{};
GDExtensionProductionOutputPrepareV1 staged_bound_input{};
GDExtensionOutputAcquireResultV1 staged_acquire{};
GDExtensionOutputStatusV1 staged_cancel_status{ ERR_UNCONFIGURED, 0, nullptr };
GDExtensionProductionReportV2 staged_observe_report{};
GDExtensionStorageRetireRefV1 staged_control_retire{};
GDExtensionStorageRetireReportV1 staged_control_return{};
GDExtensionNativeUnitRetireRefV1 staged_unit_retire{};
GDExtensionNativeUnitRetireReportV1 staged_unit_return{};
GDExtensionCameraReadyCloseV1 staged_relay_close{}, staged_relay_first_close{};
Error staged_relay_first_error = ERR_UNCONFIGURED;
Array staged_results, staged_segments, staged_readback_rows;
Dictionary staged_capture_snapshot, staged_raster_snapshot, staged_first_segment_ack_busy;
Dictionary staged_scene_cull_snapshot;
Error staged_ack_error = ERR_UNCONFIGURED;
uint32_t staged_guard_words[8]{}, staged_record_words[48]{};

void staged_prepare_plan_input() {
	using namespace GDExtensionDeviceExit;
	staged_plan.abi = production_dto_abi<GDExtensionProductionOutputPlanV1>();
	auto &acquire = staged_plan.acquire;
	acquire.abi = production_dto_abi<GDExtensionProductionOutputPrepareV1>();
	acquire.action_ordinal = 1;
	acquire.canonical_suboperation = 11;
	acquire.source_recipe_substep = 11;
	acquire.source_resource_slot = 0;
	acquire.source_use_index = acquire.first_source_use = 1;
	acquire.source_use_count = 1;
	acquire.source_generation = source_origin.allocation_generation;
	acquire.read_capacity = staged_read_capacity();
	acquire.read_graph_capacity = staged_read_graph_capacity();
	acquire.roles[0] = { 1, 2, 2, 0, created.generation, 192 };
	acquire.roles[1] = { 2, 3, 3, 0, created.generation, 192 };
	acquire.roles[2] = { 7, 4, 4, 0, created.generation, 32 };
	acquire.input.abi = production_dto_abi<GDExtensionOutputAcquireInputV1>();
	acquire.input.particles = created.particles;
	acquire.input.content_version = 1;
	acquire.input.common.generation = created.generation;
	acquire.input.common.output_tag.bytes[0] = 1;
	acquire.input.common.view_tag.bytes[0] = 2;
	acquire.input.common.bounds = create_input.config.bounds;
	acquire.input.common.current_host_token = host_input.host_token;
	acquire.input.pass_mask = create_input.config.pass_mask;
	acquire.input.layout_digest = create_input.config.layout_digest;
	acquire.input.mapping_digest = create_input.config.mapping_digest;
	acquire.input.source_token.bytes[0] = 3;
	staged_plan.finalize_action_ordinal = 3;
	staged_plan.seal_action_ordinal = 4;
	staged_plan.finalize_canonical_suboperation = staged_plan.seal_canonical_suboperation = 11;
	staged_plan.finalize_source_recipe_substep = 13;
	staged_plan.seal_source_recipe_substep = 14;
	staged_plan.source_space = 1;
	for (uint32_t semantic = 0; semantic < 5; ++semantic) {
		staged_plan.reads[semantic] = { 0, 1, source_origin.allocation_generation, semantic };
	}
	staged_plan.guard_work_limit = staged_demand.guard_work;
	staged_plan.guard_dispatch_limit = staged_demand.guard_dispatches;
	staged_plan.guard_graph_limit = staged_demand.guard_graphs;
	staged_plan.guard_native_limit = staged_demand.guard_native_memberships;
	staged_plan.guard_owned_resource_limit = staged_demand.guard_owned_resources;
}

Error staged_ack_in_normal() {
	using namespace GDExtensionDeviceExit;
	const auto *entry = RegistrationContext::current;
	if (!entry || !entry->admitted || entry->level_phase != LevelPhase::NONE || !entry->frame_id || !entry->reference ||
		entry->reference->registry != &enrollment.registry || entry->reference->slot != enrollment.reference.slot || Registry::production_physical_frame) {
		return ERR_UNAUTHORIZED;
	}
	staged_ack_frame = entry->frame_id;
	const auto ack = port<GDExtensionInterfaceDeviceSessionProductionAckSegmentV1>("device_session_production_ack_segment_v1");
	return ack ? Error(ack(host_request, staged_batch_item.segment.segment, staged_batch_item.segment.segment_revision).native_error) : ERR_UNAVAILABLE;
}

void staged_snapshot_first_segment_ack_busy() {
	using namespace GDExtensionDeviceExit;
	Registry::Lock lock(enrollment.registry);
	auto &snapshot = staged_first_segment_ack_busy;
	snapshot["ack_error"] = staged_ack_error;
	snapshot["normal_frame"] = int64_t(staged_ack_frame);
	snapshot["batch_count"] = staged_batch_count;
	snapshot["batch_cursor"] = int64_t(staged_batch_item.cursor);
	snapshot["batch_flags"] = staged_batch_item.flags;
	snapshot["segment"] = int64_t(staged_batch_item.segment.segment);
	snapshot["segment_revision"] = int64_t(staged_batch_item.segment.segment_revision);
	snapshot["record_control"] = int64_t(staged_record_control);
	snapshot["expected_request"] = int64_t(host_request.handle_serial);
	const auto *request = host_original_request;
	const bool request_present = request && request->serial == host_request.handle_serial;
	snapshot["request_present"] = request_present;
	if (!request_present) {
		return;
	}
	snapshot["request"] = int64_t(request->serial);
	snapshot["request_revision"] = int64_t(request->revision);
	snapshot["control_serial"] = int64_t(request->control.serial);
	snapshot["control_state"] = uint32_t(request->control.state);
	snapshot["native_stage_control"] = int64_t(request->native_stage_control);
	snapshot["action_count"] = request->action_count;
	snapshot["next_action"] = request->next_action;
	snapshot["graph_count"] = request->graph_count;
	snapshot["native_count"] = request->native_count;
	snapshot["capture_inflight"] = request->capture_inflight;
	snapshot["continuation_inflight"] = request->continuation_inflight;
	snapshot["effects_pending"] = request->effects_pending;
	snapshot["source_device_lost"] = request->source_device_lost;
	snapshot["draw_list_open"] = request->draw_list_open;
	snapshot["compute_list_open"] = request->compute_list_open;
	snapshot["native_batch_enabled"] = request->native_batch_enabled;
	snapshot["native_batch_control"] = int64_t(request->native_batch_control);
	snapshot["native_batch_pending_ack"] = int64_t(request->native_batch_pending_ack);
	snapshot["native_prepared_unread"] = request->native_prepared_unread;
	snapshot["acknowledged_segment"] = int64_t(request->acknowledged_segment);
	snapshot["request_segment"] = int64_t(request->segment.segment);
	snapshot["request_segment_revision"] = int64_t(request->segment.segment_revision);

	auto *storage = RendererRD::ParticlesStorage::get_singleton();
	if (!storage || !request->actions || request->action_count <= 1) {
		snapshot["raster_snapshot_error"] = ERR_UNAVAILABLE;
		return;
	}
	const auto &acquire = request->actions[1];
	Dictionary raster;
	RID pipeline;
	uint32_t draw = UINT32_MAX, close = UINT32_MAX;
	snapshot["raster_snapshot_error"] = RendererRD::ExternalLinkedOwnerTest::staged_color_snapshot(*storage, RID::from_uint64(created.particles),
			acquire.output_slot, acquire.preinput_camera_source, raster, pipeline, draw, close);
	snapshot["raster"] = raster;
}

bool staged_read_batch_turn() {
	using namespace GDExtensionDeviceExit;
	if (staged_batch_item.flags & 2u) {
		Variant returned;
		Callable::CallError call_error;
		host_normal_callable.callp(nullptr, 0, returned, call_error);
		if (call_error.error != Callable::CallError::CALL_OK) { staged_fail(ERR_UNAUTHORIZED, "staged_ack_callable"); return false; }
		if (staged_ack_error == ERR_BUSY && staged_batch_count == 7 && staged_first_segment_ack_busy.is_empty()) {
			staged_snapshot_first_segment_ack_busy();
		}
		if (staged_fail(staged_ack_error, "staged_original_segment_ack") != OK) { return false; }
		Dictionary ack;
		ack["cursor"] = int64_t(staged_batch_item.cursor);
		ack["segment"] = int64_t(staged_batch_item.segment.segment);
		ack["revision"] = int64_t(staged_batch_item.segment.segment_revision);
		ack["normal_frame"] = int64_t(staged_ack_frame);
		ack["control"] = int64_t(staged_record_control);
		ack["request"] = int64_t(host_request.handle_serial);
		ack["ack_error"] = staged_ack_error;
		staged_segments.push_back(ack);
		staged_batch_item.flags &= ~2u;
		return false;
	}
	if (staged_batch_count && !staged_batch_cursor) {
		Registry::Lock lock(enrollment.registry);
		staged_unread_after = host_original_request->native_prepared_unread;
		staged_pending_ack_after = host_original_request->native_batch_pending_ack;
		const uint32_t expected_actions = staged_bank_probe() ? 2 : 7;
		if (staged_fail(staged_batch_count == expected_actions && !staged_unread_after && !staged_pending_ack_after ? OK : ERR_INVALID_DATA, "staged_incomplete_original_batch") != OK) { return false; }
		if (staged_bank_probe()) {
			staged_bank_refused = true;
			staged_close_step = 2; // No output lease exists to cancel.
		}
		return true;
	}
	const auto read = port<GDExtensionInterfaceDeviceSessionProductionNativeBatchReadV2>("device_session_production_native_batch_read_v2");
	if (!read) { staged_fail(ERR_UNAVAILABLE, "staged_batch_port"); return false; }
	staged_batch_item = {};
	if (staged_fail(Error(read(host_request, staged_record_control, staged_batch_cursor, &staged_batch_item).native_error), "staged_original_batch_read") != OK) { return false; }
	const auto &result = staged_batch_item.result;
	const auto &ref = result.ref;
	if (!(staged_batch_item.flags & 1u) || staged_batch_count >= 7 || ref.registry_instance != host_request.registry_instance ||
		ref.request_serial != host_request.handle_serial || ref.control_serial != staged_record_control || ref.action_ordinal != staged_batch_count ||
		ref.canonical_begin != host_original_request->operation.canonical_begin || ref.canonical_end != host_original_request->operation.canonical_end ||
		ref.canonical_suboperation != staged_actions[staged_batch_count].canonical_suboperation || ref.source_recipe_substep != staged_actions[staged_batch_count].source_recipe_substep ||
		result.action_kind != staged_actions[staged_batch_count].action_kind) {
		staged_fail(ERR_INVALID_DATA, "staged_batch_result_identity"); return false;
	}
	Dictionary row;
	row["ordinal"] = int64_t(ref.action_ordinal);
	row["canonical"] = int64_t(ref.canonical_suboperation);
	row["substep"] = ref.source_recipe_substep;
	row["request"] = int64_t(ref.request_serial);
	row["control"] = int64_t(ref.control_serial);
	row["registry_instance"] = int64_t(ref.registry_instance);
	row["canonical_begin"] = int64_t(ref.canonical_begin);
	row["canonical_end"] = int64_t(ref.canonical_end);
	row["action_kind"] = result.action_kind;
	row["cursor"] = int64_t(staged_batch_item.cursor);
	row["next_cursor"] = int64_t(staged_batch_item.next_cursor);
	row["flags"] = staged_batch_item.flags;
	const auto &original_append = host_original_request->actions[ref.action_ordinal].original_append;
	row["original_append_bound"] = original_append.operation_bound;
	row["original_append_graph_frame"] = int64_t(original_append.handle.graph_frame);
	row["original_append_graph_kind"] = original_append.handle.kind;
	row["original_append_nonce"] = int64_t(original_append.operation.owner_request_nonce);
	row["output_kind"] = result.output_kind;
	row["native_error"] = result.result.native_error;
	row["append_state"] = result.append.state;
	row["graph_fact_count"] = result.append.graph_fact_count;
	row["finalize_state"] = result.finalize.state;
	row["finalize_error"] = result.finalize.status.error;
	row["finalize_custody"] = result.finalize.custody;
	row["record_token"] = String::hex_encode_buffer(result.finalize.record_token.bytes, 32);
	row["seal_error"] = result.seal.status.error;
	row["seal_state"] = result.seal.state;
	row["packet_token"] = String::hex_encode_buffer(result.seal.packet_token.bytes, 32);
	if (result.output_kind == 1) {
		staged_acquire = result.acquire;
		row["acquire_error"] = staged_acquire.status.error;
		row["acquire_state"] = staged_acquire.state;
		row["acquire_request"] = int64_t(staged_acquire.request_serial);
		row["acquire_ordinal"] = int64_t(staged_acquire.action_ordinal);
		row["acquire_writable_count"] = staged_acquire.writable_count;
		row["acquire_custody"] = staged_acquire.custody;
		row["lease_token"] = String::hex_encode_buffer(staged_acquire.lease_token.bytes, 32);
		Array writables;
		for (uint32_t i = 0; i < MIN(staged_acquire.writable_count, 3u); ++i) {
			const auto &writable = staged_acquire.writables[i];
			Dictionary w;
			w["role"] = writable.role;
			w["rid"] = int64_t(writable.rid);
			w["resource_slot"] = staged_acquire.resource_slots[i];
			w["use_index"] = staged_acquire.use_indices[i];
			w["access"] = writable.access;
			w["generation"] = int64_t(writable.allocation_generation);
			w["byte_length"] = writable.byte_length;
			w["stride"] = writable.stride;
			writables.push_back(w);
		}
		row["writables"] = writables;
	}
	const GDExtensionOutputStatusV1 *status = nullptr;
	switch (result.output_kind) {
		case 1: status = &result.acquire.status; break;
		case 2: status = &result.finalize.status; break;
		case 3: status = &result.seal.status; break;
	}
	if (status) {
		Dictionary detail;
		detail["detail_length"] = status->detail_length;
		detail["detail_present"] = status->detail != nullptr;
		if (status->detail && status->detail_length) {
			detail["detail"] = String::utf8(status->detail, status->detail_length);
			detail["detail_hex"] = String::hex_encode_buffer(reinterpret_cast<const uint8_t *>(status->detail), status->detail_length);
		} else {
			detail["detail"] = String();
			detail["detail_hex"] = String();
		}
		row["output_status_detail"] = detail;
	}
	if (result.output_kind == 2) {
		Registry::Lock lock(enrollment.registry);
		const auto &finalizer = host_original_request->actions[ref.action_ordinal];
		const uint64_t acquire_ordinal = finalizer.output_acquire_action_ordinal;
		auto *storage = RendererRD::ParticlesStorage::get_singleton();
		if (storage && acquire_ordinal < ref.action_ordinal) {
			const auto &acquire = host_original_request->actions[acquire_ordinal];
			Dictionary snapshot;
			const Error observed = RendererRD::ExternalLinkedOwnerTest::staged_guard_snapshot(*storage, acquire.output_particles,
					acquire.output_slot, acquire.output_generation, enrollment.registry, host_request, acquire_ordinal, acquire.output_lease, snapshot);
			row["guard_snapshot_error"] = observed;
			if (observed == OK) {
				snapshot["guard_entered"] = acquire.output_guard_entered;
				snapshot["guard_complete"] = acquire.output_guard_complete;
				snapshot["guard_upload_bytes"] = acquire.output_guard_upload_bytes;
				snapshot["guard_dispatch_cursor"] = acquire.output_guard_dispatch_cursor;
				snapshot["prepared_scene_instance"] = int64_t(acquire.output_scene.instance);
				snapshot["prepared_scene_particles"] = int64_t(acquire.output_scene.particles);
				snapshot["prepared_scene_scenario"] = int64_t(acquire.output_scene.scenario);
				row["guard_snapshot"] = snapshot;
			}
		} else {
			row["guard_snapshot_error"] = ERR_UNAVAILABLE;
		}
	}
	staged_results.push_back(row);
	staged_batch_cursor = staged_batch_item.next_cursor;
	++staged_batch_count;
	if (staged_bank_probe() && result.output_kind == 1 && result.result.native_error == ERR_OUT_OF_MEMORY &&
			staged_acquire.status.error == ERR_OUT_OF_MEMORY && status && status->detail && String::utf8(status->detail, status->detail_length) == "output_global_preparation_bank" &&
			staged_acquire.custody == GDEXTENSION_OUTPUT_NO_CUSTODY_V1 && !staged_acquire.state && !staged_acquire.writable_count &&
			staged_record.record.result.native_error == OK && host_original_request->next_action == ref.action_ordinal &&
			host_original_request->output_owner_head == UINT32_MAX) {
		return false; // Drain the batch and ACK its actual upload prefix before closing.
	}
	if (result.result.native_error != OK) {
		// A returned BUSY is an actual action result, not an outstanding poll.
		staged_error = Error(result.result.native_error);
		staged_detail = "staged_original_core_action";
	}
	return false;
}

void staged_check_raster(const GDExtensionDeviceExit::ProductionActionCell &action, const Ref<RendererViewport::Viewport::CameraCapture> &capture) {
	auto *storage = RendererRD::ParticlesStorage::get_singleton();
	RID pipeline_rid;
	uint32_t draw = UINT32_MAX, close = UINT32_MAX;
	staged_raster_snapshot["request_graph_count"] = host_original_request->graph_count;
	staged_raster_snapshot["acquire_read_cursor"] = action.output_read_cursor;
	if (!storage) {
		staged_raster_error = ERR_UNAVAILABLE;
		staged_raster_detail = "original_particles_storage_missing";
		return;
	}
	staged_raster_error = RendererRD::ExternalLinkedOwnerTest::staged_color_snapshot(*storage, RID::from_uint64(created.particles),
			action.output_slot, capture, staged_raster_snapshot, pipeline_rid, draw, close);
	Array preparations = staged_raster_snapshot.get("original_preparation_rows", Array());
	for (int64_t i = 0; i < preparations.size(); ++i) {
		Dictionary row = preparations[i];
		const RID original_pipeline = RID::from_uint64(uint64_t(int64_t(row["pipeline"])));
		const auto *actual = original_pipeline.is_valid() ? device->render_pipeline_owner.get_or_null(original_pipeline) : nullptr;
		row["pipeline_live"] = actual && actual->driver_id;
		if (actual) {
			Array formats;
			for (uint32_t format : actual->set_formats) {
				formats.push_back(format);
			}
			row["pipeline_set_formats"] = formats;
			row["pipeline_set_count"] = actual->set_formats.size();
		}
	}
	if (staged_raster_error != OK) {
		staged_raster_detail = staged_raster_snapshot["snapshot_detail"];
		return;
	}
	if (draw >= host_original_request->graph_count || close >= host_original_request->graph_count) {
		staged_raster_error = ERR_PARAMETER_RANGE_ERROR;
		staged_raster_detail = "original_color_graph_index_out_of_range";
		return;
	}
	const auto *pipeline = device->render_pipeline_owner.get_or_null(pipeline_rid);
	if (!pipeline) {
		staged_raster_error = ERR_INVALID_DATA;
		staged_raster_detail = "staged_original_raster_pipeline";
		return;
	}
	const uint32_t required = 2 * uint32_t(pipeline->set_formats.size()) + 6;
	if (required > STAGED_MAX_DRAW_GRAPHS || uint32_t(int64_t(staged_raster_snapshot["graph_limit"])) != required ||
		action.output_read_graph_capacity != STAGED_PREPARATION_RESOURCE_CAPACITY || required > action.output_read_graph_capacity) {
		staged_raster_error = ERR_INVALID_DATA;
		staged_raster_detail = "staged_original_raster_graph_bound";
		return;
	}
	const auto &draw_handle = host_original_request->graphs[draw].handle;
	const auto &close_handle = host_original_request->graphs[close].handle;
	const auto handle_report = [](const auto &handle) {
		Dictionary value;
		value["graph_instance"] = int64_t(handle.graph_instance);
		value["graph_frame"] = int64_t(handle.graph_frame);
		value["kind"] = handle.kind;
		value["index"] = handle.index;
		value["offset"] = handle.offset;
		return value;
	};
	staged_raster_snapshot["draw_handle"] = handle_report(draw_handle);
	staged_raster_snapshot["close_handle"] = handle_report(close_handle);
	staged_raster_snapshot["set_count"] = int64_t(pipeline->set_formats.size());
	staged_raster_snapshot["required_graphs"] = required;
	staged_raster_snapshot["read_graph_capacity"] = action.output_read_graph_capacity;
	staged_raster_snapshot["read_capacity"] = action.output_read_capacity;
	if (draw_handle.kind != 3 || close_handle.kind != 7 || draw_handle.graph_instance != close_handle.graph_instance || draw_handle.graph_frame != close_handle.graph_frame) {
		staged_raster_error = ERR_INVALID_DATA;
		staged_raster_detail = "staged_original_color_close_handles";
		return;
	}
	staged_raster_verified = true;
}
void snapshot_staged_scene_cull(const GDExtensionDeviceExit::ProductionActionCell &action,
		const Ref<RendererViewport::Viewport::CameraCapture> &bound_capture) {
	if (!staged_scene_cull_snapshot.is_empty()) { return; }
	const auto box = [](const AABB &value) {
		Dictionary result;
		Array position, size;
		for (uint32_t axis = 0; axis < 3; ++axis) {
			position.push_back(value.position[axis]); size.push_back(value.size[axis]);
		}
		result["position"] = position; result["size"] = size;
		return result;
	};
	const auto affine = [](const Transform3D &value) {
		Array result;
		for (uint32_t row = 0; row < 3; ++row) {
			for (uint32_t column = 0; column < 3; ++column) { result.push_back(value.basis[row][column]); }
			result.push_back(value.origin[row]);
		}
		return result;
	};
	Dictionary snapshot;
	snapshot["phase"] = "after_original_record_consume_before_retirement";
	snapshot["request"] = int64_t(host_original_request->serial);
	snapshot["control"] = int64_t(staged_record_control);
	snapshot["action_ordinal"] = int64_t(action.declaration.action_ordinal);
	snapshot["request_frozen"] = host_original_request->frozen;
	snapshot["prepared"] = action.scene_cull_prepared;
	snapshot["error"] = action.scene_cull_error;
	snapshot["version"] = int64_t(action.scene_cull_version);
	snapshot["local"] = box(action.scene_cull_local);
	snapshot["world"] = box(action.scene_cull_world);
	snapshot["required_world"] = box(action.scene_cull_required_world);
	snapshot["instance"] = int64_t(action.output_scene.instance);
	snapshot["particles"] = int64_t(action.output_scene.particles);
	snapshot["base"] = int64_t(action.output_scene.particles);
	snapshot["scenario"] = int64_t(action.output_scene.scenario);
	snapshot["generation"] = int64_t(action.output_scene.generation);
	snapshot["output_slot"] = action.output_slot;
	snapshot["source_space"] = action.output_plan.source_space;
	Array declared_position, declared_size, host_affine;
	for (uint32_t axis = 0; axis < 3; ++axis) {
		declared_position.push_back(action.output_input.common.bounds.position[axis]);
		declared_size.push_back(action.output_input.common.bounds.size[axis]);
	}
	for (float value : action.preinput_host_transform.affine) { host_affine.push_back(value); }
	snapshot["declared_position"] = declared_position; snapshot["declared_size"] = declared_size;
	snapshot["host_affine"] = host_affine;
	const auto &capture = action.scene_cull_capture;
	Dictionary captured;
	captured["present"] = capture.is_valid();
	captured["same_bound_ref"] = capture.is_valid() && capture == bound_capture;
	if (capture.is_valid()) {
		const auto &token = capture->get_token();
		captured["viewport"] = int64_t(capture->get_viewport().get_id());
		captured["camera"] = int64_t(capture->get_invocation().camera.get_id());
		captured["view_id"] = int64_t(token.view_id); captured["view_epoch"] = int64_t(token.view_epoch);
		captured["capture_ordinal"] = int64_t(token.capture_ordinal);
		captured["present_seq"] = int64_t(capture->get_presentation_key().present_seq);
	}
	snapshot["capture"] = captured;
	RendererSceneCull::NativeParticleSceneFacts actual;
	auto *scene = static_cast<RendererSceneCull *>(RSG::scene);
	const Error observed = scene ? scene->instance_get_committed_particle_scene_facts(RID::from_uint64(action.output_scene.instance),
			RID::from_uint64(action.output_scene.particles), actual) : ERR_UNAVAILABLE;
	snapshot["actual_scene_error"] = observed;
	Dictionary facts;
	if (observed == OK) {
		facts["instance"] = int64_t(actual.instance.get_id()); facts["particles"] = int64_t(actual.particles.get_id());
		facts["scenario"] = int64_t(actual.scenario.get_id()); facts["version"] = int64_t(actual.version);
		facts["affine"] = affine(actual.transform); facts["local"] = box(actual.local_aabb); facts["world"] = box(actual.world_aabb);
		facts["base"] = int64_t(actual.particles.get_id());
		facts["query_committed"] = actual.query_committed;
		facts["query_generation"] = int64_t(actual.query_generation); facts["query_world"] = box(actual.query_world);
	}
	snapshot["actual_scene"] = facts;
	RendererRD::ExternalLinkedOwnerTest::SceneCullObservation accepted;
	auto *storage = RendererRD::ParticlesStorage::get_singleton();
	const Error slot_error = storage ? RendererRD::ExternalLinkedOwnerTest::read_scene_cull_observation(*storage,
			action.output_particles, action.output_slot, enrollment.registry, host_request, action.output_generation,
			action.declaration.action_ordinal, action.output_lease, accepted) : ERR_UNAVAILABLE;
	snapshot["accepted_slot_error"] = slot_error;
	Dictionary slot;
	if (slot_error == OK) {
		slot["bound"] = accepted.bound; slot["slot"] = accepted.slot;
		slot["request"] = int64_t(accepted.request.handle_serial); slot["registry_instance"] = int64_t(accepted.request.registry_instance);
		slot["generation"] = int64_t(accepted.generation); slot["lease"] = int64_t(accepted.lease); slot["action_ordinal"] = int64_t(accepted.ordinal);
		slot["request_matches"] = accepted.request_matches; slot["generation_matches"] = accepted.generation_matches;
		slot["lease_matches"] = accepted.lease_matches; slot["ordinal_matches"] = accepted.ordinal_matches;
		slot["version"] = int64_t(accepted.version); slot["local"] = box(accepted.local); slot["world"] = box(accepted.world);
		slot["instance"] = int64_t(accepted.instance.get_id()); slot["base"] = int64_t(accepted.base.get_id());
		slot["scenario"] = int64_t(accepted.scenario.get_id()); slot["viewport"] = int64_t(accepted.viewport.get_id());
	}
	// Bound is an original accepted snapshot, not a claim that the exited
	// pre-cull scope is still live. Reporting never revalidates or renews it.
	snapshot["accepted_slot"] = slot;
	staged_scene_cull_snapshot = snapshot;
}

bool staged_capture_and_raster() {
	using namespace GDExtensionDeviceExit;
	const auto read = port<GDExtensionInterfaceDeviceSessionProductionReadStagedOutputInputV2>("device_session_production_read_staged_output_input_v2");
	if (!read) { staged_fail(ERR_UNAVAILABLE, "staged_capture_read_port"); return false; }
	if (staged_fail(Error(read(host_request, 1, &staged_bound_input).native_error), "staged_original_capture_read") != OK) { return false; }
	const auto &common = staged_bound_input.input.common;
	Registry::Lock lock(enrollment.registry);
	const auto &action = host_original_request->actions[1];
	const auto &capture = action.preinput_camera_source;
	if (capture.is_null() || capture->get_viewport() != host_viewport || capture->get_invocation().camera != host_camera ||
		capture->get_token().view_id != common.current_camera.view_id || capture->get_token().view_epoch != common.current_camera.view_epoch ||
		capture->get_token().capture_ordinal != common.current_camera.capture_ordinal || capture->get_presentation_key().present_seq != common.present_seq || !common.present_seq || staged_compat_calls) {
		staged_fail(ERR_INVALID_DATA, "staged_actual_core_capture_identity"); return false;
	}
	staged_capture_snapshot["view_id"] = int64_t(common.current_camera.view_id);
	staged_capture_snapshot["view_epoch"] = int64_t(common.current_camera.view_epoch);
	staged_capture_snapshot["capture_ordinal"] = int64_t(common.current_camera.capture_ordinal);
	staged_capture_snapshot["present_seq"] = int64_t(common.present_seq);
	staged_capture_snapshot["viewport"] = int64_t(capture->get_viewport().get_id());
	staged_capture_snapshot["camera"] = int64_t(capture->get_invocation().camera.get_id());
	staged_capture_snapshot["core_executor"] = host_original_request->native_prepared_executor;
	staged_capture_snapshot["static_sealed"] = host_original_request->native_static_sealed;
	staged_capture_snapshot["frozen"] = host_original_request->frozen;
	// Freeze original Action/Scene values while the request is still alive.
	// report_staged only copies this Dictionary after original reclamation.
	snapshot_staged_scene_cull(action, capture);
	// A failed raster snapshot cannot certify a draw. The exact capture
	// and original readbacks still have independent OBSERVE and COPY gates.
	staged_check_raster(action, capture);
	return true;
}

Vector<uint8_t> staged_prior_program;
GDExtensionNativeUnitCreateRequestV1 staged_prior_unit_origin{};
GDExtensionNativeUnitBackingV1 staged_prior_unit{};
GDExtensionStorageCreateRequestV1 staged_successor_control_origin{};
GDExtensionStorageBackingV1 staged_successor_control{};
GDExtensionProductionActionV2 staged_successor_actions[8]{};
GDExtensionProductionResourceV1 staged_successor_resources[10]{};
GDExtensionProductionUseV1 staged_successor_uses[17]{};
GDExtensionProductionReadbackSourceV1 staged_successor_readbacks[3]{};
GDExtensionProductionUploadV2 staged_successor_upload{};
GDExtensionProductionOutputDemandInputV1 staged_successor_demand_input{};
GDExtensionProductionOutputGuardDemandV1 staged_successor_demand{};
GDExtensionProductionMetadataQuoteV1 staged_successor_quote{};
GDExtensionProductionCapacityV1 staged_successor_capacity{};
GDExtensionDeviceSessionResultV1 staged_successor_quote_result{ ERR_UNCONFIGURED, 0, 0, 0 };
GDExtensionDeviceSessionResultV1 staged_successor_reserve_result{ ERR_UNCONFIGURED, 0, 0, 0 };
GDExtensionProductionRequestRefV1 staged_successor_request{};
Request *staged_successor_original_request = nullptr;
bool staged_successor_source_imported = false, staged_successor_unit_imported = false;
bool staged_successor_normal_prepare_active = false, staged_successor_ack_active = false;
Error staged_successor_ack_error = ERR_UNCONFIGURED;
Transform3D staged_successor_transform;
GDExtensionProductionHostTransformInputV1 staged_successor_host_input{};
GDExtensionProductionOutputSceneInputV1 staged_successor_scene_input{};
GDExtensionProductionHostTransformV1 staged_successor_host_read{};
GDExtensionDeviceSessionResultV1 staged_successor_host_prepare_result{ ERR_UNCONFIGURED, 0, 0, 0 };
GDExtensionDeviceSessionResultV1 staged_successor_scene_result{ ERR_UNCONFIGURED, 0, 0, 0 };
GDExtensionDeviceSessionResultV1 staged_successor_view_result{ ERR_UNCONFIGURED, 0, 0, 0 };
GDExtensionDeviceSessionResultV1 staged_successor_host_read_result{ ERR_UNCONFIGURED, 0, 0, 0 };
GDExtensionOutputConsumedBorrowResultV2 staged_exported_borrow{};
GDExtensionOutputConsumedProofV2 staged_exported_proof{};
bool staged_proof_prepared = false, staged_proof_exported = false, staged_proof_returned = false;
GDExtensionProductionOutputPlanV2 staged_successor_plan2{};
GDExtensionProductionServiceBudgetV1 staged_successor_record_budget{};
uint64_t staged_successor_record_control = 0, staged_successor_batch_cursor = 0, staged_successor_cancel_control = 0;
uint64_t staged_successor_cancel_frame = 0, staged_successor_ack_frame = 0;
uint32_t staged_successor_batch_count = 0, staged_successor_unread_after = UINT32_MAX;
uint64_t staged_successor_pending_ack_after = UINT64_MAX;
bool staged_successor_native_ready = false, staged_successor_record_consumed = false, staged_successor_readbacks_verified = false;
bool staged_successor_raster_verified = false, staged_successor_cancel_entered = false;
Error staged_successor_raster_error = OK;
String staged_successor_raster_detail;
GDExtensionProductionRecordReportV3 staged_successor_record{};
GDExtensionProductionRecordReportV2 staged_successor_cancel_receipt{};
GDExtensionProductionNativeBatchItemV1 staged_successor_batch_item{};
GDExtensionProductionOutputPrepareV1 staged_successor_bound_input{};
GDExtensionOutputAcquireResultV1 staged_successor_acquire{};
GDExtensionOutputStatusV1 staged_successor_cancel_status{ ERR_UNCONFIGURED, 0, nullptr };
GDExtensionProductionReportV2 staged_successor_observe_report{}, staged_successor_release_report{}, staged_proof_return_report{};
GDExtensionDeviceSessionResultV1 staged_successor_observe_api_result{ ERR_UNCONFIGURED, 0, 0, 0 };
GDExtensionDeviceSessionResultV1 staged_successor_control_observe_api_result{ ERR_UNCONFIGURED, 0, 0, 0 };
GDExtensionProductionControlReceiptV1 staged_successor_observe_receipt{}, staged_successor_control_observe_receipt{};
GDExtensionStorageRetireRefV1 staged_successor_control_retire{};
GDExtensionStorageRetireReportV1 staged_successor_control_return{};
GDExtensionNativeUnitRetireRefV1 staged_prior_unit_retire{};
GDExtensionNativeUnitRetireReportV1 staged_prior_unit_return{};
uint32_t staged_successor_close_step = 0;
bool staged_successor_slot_owner_returned = false, staged_successor_reclaimed = false, staged_successor_closed = false;
bool staged_prior_unit_released = false;
uint32_t staged_successor_guard_words[8]{}, staged_successor_current_words[48]{}, staged_successor_previous_words[48]{};
Dictionary staged_prior_program_snapshot, staged_successor_capture_snapshot, staged_successor_scene_cull_snapshot, staged_successor_raster_snapshot;
Array staged_successor_results, staged_successor_segments, staged_successor_readback_rows;

Error compile_staged_prior_program() {
	if (!staged_prior_program.is_empty()) { return OK; }
	String diagnostic;
	const Vector<uint8_t> spirv = device->shader_compile_spirv_from_source(RenderingDevice::SHADER_STAGE_COMPUTE,
		String::utf8(STAGED_WRITER_WITH_PREVIOUS_SHADER), RenderingDevice::SHADER_LANGUAGE_GLSL, &diagnostic, false);
	if (spirv.is_empty() || !diagnostic.is_empty()) { return staged_fail(ERR_COMPILATION_FAILED, "staged_prior_spirv_compile"); }
	Vector<RenderingDevice::ShaderStageSPIRVData> stages;
	RenderingDevice::ShaderStageSPIRVData code;
	code.shader_stage = RenderingDevice::SHADER_STAGE_COMPUTE; code.spirv = spirv; stages.push_back(code);
	staged_prior_program = device->shader_compile_binary_from_spirv_with_options(stages, "typed_staged_writer_with_previous", RenderingDevice::SHADER_COMPILE_FLAG_DISABLE_FAST_MATH);
	if (staged_prior_program.is_empty()) { return staged_fail(ERR_COMPILATION_FAILED, "staged_prior_native_container"); }
	staged_prior_unit_origin.abi = Enrollment::ABI;
	staged_prior_unit_origin.library_generation = enrollment.holder.generation;
	staged_prior_unit_origin.preparation_generation = 1; staged_prior_unit_origin.operation_serial = 2;
	staged_prior_unit_origin.record_slot = 7; staged_prior_unit_origin.physical_unit = 0;
	staged_prior_unit_origin.expected_flags = RenderingDevice::SHADER_COMPILE_FLAG_DISABLE_FAST_MATH;
	staged_prior_unit_origin.container_byte_count = staged_prior_program.size();
	return staged_fail(CryptoCore::sha256(staged_prior_program.ptr(), staged_prior_program.size(), staged_prior_unit_origin.container_digest), "staged_prior_container_digest");
}

void declare_staged_successor_rows() {
	using namespace GDExtensionDeviceExit;
	staged_successor_actions[0] = { 0, 11, 10, 0, GDEXTENSION_PRODUCTION_UPLOAD_V1, UINT32_MAX, 0, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX };
	staged_successor_actions[1] = { 1, 11, 11, 0, GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1, UINT32_MAX, 1, 6, GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1, UINT32_MAX };
	staged_successor_actions[2] = { 2, 11, 12, 0, GDEXTENSION_PRODUCTION_DISPATCH_V1, 5, 7, 7, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX };
	staged_successor_actions[3] = { 3, 11, 13, 0, GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1, UINT32_MAX, 0, 0, GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1, UINT32_MAX };
	staged_successor_actions[4] = { 4, 11, 14, 0, GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1, UINT32_MAX, 0, 0, GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1, UINT32_MAX };
	staged_successor_actions[5] = { 5, 11, 15, 0, GDEXTENSION_PRODUCTION_READBACK_V1, UINT32_MAX, 14, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, 0 };
	staged_successor_actions[6] = { 6, 11, 16, 0, GDEXTENSION_PRODUCTION_READBACK_V1, UINT32_MAX, 15, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, 1 };
	staged_successor_actions[7] = { 7, 11, 17, 0, GDEXTENSION_PRODUCTION_READBACK_V1, UINT32_MAX, 16, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, 2 };
	staged_successor_resources[0] = { GDEXTENSION_PRODUCTION_STORAGE_V1, source_origin.allocation_slot, source_origin.part, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, source_origin.allocation_generation, source_origin.bytes, 0 };
	staged_successor_resources[1] = { GDEXTENSION_PRODUCTION_STORAGE_V1, 10, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, 12, 0 };
	staged_successor_resources[2] = { GDEXTENSION_PRODUCTION_STORAGE_V1, 1, 0, GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1, created.generation, 192, 0 };
	staged_successor_resources[3] = { GDEXTENSION_PRODUCTION_STORAGE_V1, 2, 0, GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1, created.generation, 192, 0 };
	staged_successor_resources[4] = { GDEXTENSION_PRODUCTION_STORAGE_V1, 7, 0, GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1, created.generation, 32, 0 };
	staged_successor_resources[5] = { GDEXTENSION_PRODUCTION_UNIT_V1, 7, 0, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, 1, 0, 0 };
	staged_successor_resources[6] = { GDEXTENSION_PRODUCTION_SET_V1, 6, 0, GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, 0, 0 };
	staged_successor_resources[7] = { GDEXTENSION_PRODUCTION_STORAGE_V1, 1, 0, GDEXTENSION_PRODUCTION_OUTPUT_ALIAS_V1, created.generation, 32, 0 };
	staged_successor_resources[8] = { GDEXTENSION_PRODUCTION_STORAGE_V1, 1, 0, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, created.generation, 192, 0 };
	staged_successor_resources[9] = { GDEXTENSION_PRODUCTION_STORAGE_V1, 1, 0, GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, created.generation, 32, 0 };
	staged_successor_uses[0] = { 1, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 12, 0 };
	staged_successor_uses[1] = { 0, GDEXTENSION_PRODUCTION_READ_V1, 0, 32, 0 };
	staged_successor_uses[2] = { 8, GDEXTENSION_PRODUCTION_READ_V1, 0, 192, 0 };
	staged_successor_uses[3] = { 9, GDEXTENSION_PRODUCTION_READ_V1, 0, 32, 0 };
	staged_successor_uses[4] = { 2, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 192, 0 };
	staged_successor_uses[5] = { 3, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 192, 0 };
	staged_successor_uses[6] = { 4, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 32, 0 };
	staged_successor_uses[7] = { 0, GDEXTENSION_PRODUCTION_READ_V1, 0, 32, 0 };
	staged_successor_uses[8] = { 1, GDEXTENSION_PRODUCTION_READ_V1, 0, 12, 0 };
	staged_successor_uses[9] = { 2, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 192, 0 };
	staged_successor_uses[10] = { 3, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 192, 0 };
	staged_successor_uses[11] = { 4, GDEXTENSION_PRODUCTION_WRITE_V1, 0, 32, 0 };
	staged_successor_uses[12] = { 8, GDEXTENSION_PRODUCTION_READ_V1, 0, 192, 0 };
	staged_successor_uses[13] = { 9, GDEXTENSION_PRODUCTION_READ_V1, 0, 32, 0 };
	staged_successor_uses[14] = { 7, GDEXTENSION_PRODUCTION_READ_V1, 0, 32, 0 };
	staged_successor_uses[15] = { 2, GDEXTENSION_PRODUCTION_READ_V1, 0, 192, 0 };
	staged_successor_uses[16] = { 3, GDEXTENSION_PRODUCTION_READ_V1, 0, 192, 0 };
	staged_successor_readbacks[0] = { 7, GDEXTENSION_PRODUCTION_READBACK_NATIVE_OUTPUT_GUARD_V1, 5, 0, 32, { 3 }, {} };
	staged_successor_readbacks[1] = { 2, GDEXTENSION_PRODUCTION_READBACK_REQUEST_WRITER_V1, 6, 0, 192, { 1 }, {} };
	staged_successor_readbacks[2] = { 3, GDEXTENSION_PRODUCTION_READBACK_REQUEST_WRITER_V1, 7, 0, 192, { 1 }, {} };
	staged_successor_upload = { production_dto_abi<GDExtensionProductionUploadV2>(), 0, 11, 10, 0, 1, 0, 0, 12 };
	staged_successor_demand_input = { production_dto_abi<GDExtensionProductionOutputDemandInputV1>(), request, created.particles, created.generation,
		1, 11, 3, 11, 4, 11, 11, 13, 14, staged_read_capacity(), staged_read_graph_capacity(), 1, 0 };
}

void prepare_successor_host_normal() {
	using namespace GDExtensionDeviceExit;
	const auto *entry = RegistrationContext::current;
	const bool same = entry && entry->admitted && entry->reference && entry->reference->registry == &enrollment.registry &&
		entry->reference->slot == enrollment.reference.slot && entry->level_phase == LevelPhase::NONE && entry->frame_id;
	if (!same || Registry::production_physical_frame || !staged_successor_original_request || staged_successor_original_request->frozen || staged_successor_original_request->append_effect_entered) {
		staged_fail(ERR_UNAUTHORIZED, "staged_successor_normal_entry"); return;
	}
	const auto prepare = port<GDExtensionInterfaceDeviceSessionProductionPrepareHostTransformV1>("device_session_production_prepare_host_transform_v1");
	const auto scene = port<GDExtensionInterfaceDeviceSessionProductionPrepareOutputSceneV1>("device_session_production_prepare_output_scene_v1");
	const auto view = port<GDExtensionInterfaceDeviceSessionProductionPrepareHostViewV1>("device_session_production_prepare_host_view_v1");
	const auto read = port<GDExtensionInterfaceDeviceSessionProductionReadHostTransformV1>("device_session_production_read_host_transform_v1");
	if (!prepare || !scene || !view || !read) { staged_fail(ERR_UNAVAILABLE, "staged_successor_host_ports"); return; }
	staged_successor_host_prepare_result = prepare(staged_successor_request, 1, &staged_successor_host_input);
	if (staged_fail(Error(staged_successor_host_prepare_result.native_error), "staged_successor_prepare_transform") != OK) { return; }
	staged_successor_scene_result = scene(staged_successor_request, 1, &staged_successor_scene_input);
	if (staged_fail(Error(staged_successor_scene_result.native_error), "staged_successor_prepare_scene") != OK) { return; }
	const GDExtensionProductionHostViewInputV1 view_input{ production_dto_abi<GDExtensionProductionHostViewInputV1>(), host_viewport.get_id(), host_camera.get_id() };
	staged_successor_view_result = view(staged_successor_request, 1, &view_input);
	if (staged_fail(Error(staged_successor_view_result.native_error), "staged_successor_prepare_view") != OK) { return; }
	staged_successor_host_read_result = read(staged_successor_request, 1, &staged_successor_host_read);
	if (staged_fail(Error(staged_successor_host_read_result.native_error), "staged_successor_read_transform") != OK) { return; }
	if (memcmp(staged_successor_host_read.affine, staged_successor_host_input.affine, sizeof(staged_successor_host_input.affine)) ||
		memcmp(&staged_successor_host_read.host_token, &staged_successor_host_input.host_token, sizeof(staged_successor_host_input.host_token))) {
		staged_fail(ERR_INVALID_DATA, "staged_successor_read_exact_transform");
	}
}

void staged_prepare_successor_plan_input() {
	using namespace GDExtensionDeviceExit;
	staged_successor_plan2 = {};
	staged_successor_plan2.abi = production_dto_abi<GDExtensionProductionOutputPlanV2>();
	auto &plan = staged_successor_plan2.plan;
	plan.abi = production_dto_abi<GDExtensionProductionOutputPlanV1>();
	auto &acquire = plan.acquire;
	acquire.abi = production_dto_abi<GDExtensionProductionOutputPrepareV1>();
	acquire.action_ordinal = 1;
	acquire.canonical_suboperation = 11;
	acquire.source_recipe_substep = 11;
	acquire.source_resource_slot = 0;
	acquire.source_use_index = acquire.first_source_use = 1;
	acquire.source_use_count = 3;
	acquire.source_generation = source_origin.allocation_generation;
	acquire.read_capacity = staged_read_capacity();
	acquire.read_graph_capacity = staged_read_graph_capacity();
	acquire.roles[0] = { 1, 2, 4, 0, created.generation, 192 };
	acquire.roles[1] = { 2, 3, 5, 0, created.generation, 192 };
	acquire.roles[2] = { 7, 4, 6, 0, created.generation, 32 };
	acquire.input.abi = production_dto_abi<GDExtensionOutputAcquireInputV1>();
	acquire.input.particles = created.particles;
	acquire.input.content_version = 2;
	acquire.input.common.generation = created.generation;
	acquire.input.common.history_reset = 0;
	acquire.input.common.has_previous_consumed_tuple = 1;
	acquire.input.common.output_tag.bytes[0] = 2;
	acquire.input.common.view_tag.bytes[0] = 2;
	acquire.input.common.bounds = create_input.config.bounds;
	acquire.input.common.current_host_token = staged_successor_host_input.host_token;
	acquire.input.common.previous_consumed_tuple_token = staged_exported_proof.ref.original_color_token;
	acquire.input.pass_mask = create_input.config.pass_mask;
	acquire.input.layout_digest = create_input.config.layout_digest;
	acquire.input.mapping_digest = create_input.config.mapping_digest;
	acquire.input.source_token.bytes[0] = 4;
	plan.finalize_action_ordinal = 3;
	plan.seal_action_ordinal = 4;
	plan.finalize_canonical_suboperation = plan.seal_canonical_suboperation = 11;
	plan.finalize_source_recipe_substep = 13;
	plan.seal_source_recipe_substep = 14;
	plan.source_space = 1;
	for (uint32_t semantic = 0; semantic < 5; ++semantic) {
		plan.reads[semantic] = { 0, 1, source_origin.allocation_generation, semantic };
	}
	plan.guard_work_limit = staged_successor_demand.guard_work;
	plan.guard_dispatch_limit = staged_successor_demand.guard_dispatches;
	plan.guard_graph_limit = staged_successor_demand.guard_graphs;
	plan.guard_native_limit = staged_successor_demand.guard_native_memberships;
	plan.guard_owned_resource_limit = staged_successor_demand.guard_owned_resources;
	staged_successor_plan2.predecessor.abi = production_dto_abi<GDExtensionProductionOutputPredecessorV2>();
	staged_successor_plan2.predecessor.proof = staged_exported_proof.ref;
	staged_successor_plan2.predecessor.resource_slot = 8;
	staged_successor_plan2.predecessor.use_index = 2;
}

Error staged_ack_successor_in_normal() {
	using namespace GDExtensionDeviceExit;
	const auto *entry = RegistrationContext::current;
	if (!entry || !entry->admitted || entry->level_phase != LevelPhase::NONE || !entry->frame_id || !entry->reference ||
		entry->reference->registry != &enrollment.registry || entry->reference->slot != enrollment.reference.slot || Registry::production_physical_frame) {
		return ERR_UNAUTHORIZED;
	}
	staged_successor_ack_frame = entry->frame_id;
	const auto ack = port<GDExtensionInterfaceDeviceSessionProductionAckSegmentV1>("device_session_production_ack_segment_v1");
	return ack ? Error(ack(staged_successor_request, staged_successor_batch_item.segment.segment, staged_successor_batch_item.segment.segment_revision).native_error) : ERR_UNAVAILABLE;
}

bool staged_observe_successor_control_receipt(uint64_t expected, GDExtensionProductionControlReceiptV1 &receipt) {
	using namespace GDExtensionDeviceExit;
	const auto read = port<GDExtensionInterfaceDeviceSessionProductionControlReceiptV1>("device_session_production_control_receipt_v1");
	if (!read) { staged_fail(ERR_UNAVAILABLE, "staged_successor_observe_receipt_port"); return false; }
	const Error error = Error(read(staged_successor_request, expected, &receipt).native_error);
	if (staged_fail(error, "staged_successor_observe_receipt") != OK) { return false; }
	if (!production_dto_valid(receipt) || receipt.registry_instance != staged_successor_request.registry_instance || receipt.request_serial != staged_successor_request.handle_serial) {
		staged_fail(ERR_INVALID_DATA, "staged_successor_observe_receipt_identity"); return false;
	}
	return true;
}

bool staged_consumed_successor_observe_turn(const GDExtensionProductionControlReceiptV1 &before,
		const GDExtensionDeviceSessionResultV1 &result, GDExtensionProductionControlReceiptV1 &after) {
	if (!before.control_serial || before.kind != GDEXTENSION_PRODUCTION_CONTROL_RECEIPT_KIND_OBSERVE_V1 ||
			before.state != GDEXTENSION_PRODUCTION_CONTROL_RECEIPT_RETURNED_V1) { return false; }
	if (!staged_observe_successor_control_receipt(before.control_serial, after)) { return false; }
	if (after.state != GDEXTENSION_PRODUCTION_CONTROL_RECEIPT_OWNER_CONSUMED_V1) { return false; }
	if (after.kind != before.kind || after.control_serial != before.control_serial || !after.result_valid || after.control_result.native_error != result.native_error) {
		staged_fail(ERR_INVALID_DATA, "staged_successor_observe_consumed_identity"); return false;
	}
	return true;
}

bool staged_read_successor_batch_turn() {
	using namespace GDExtensionDeviceExit;
	if (staged_successor_batch_item.flags & 2u) {
		Variant returned;
		Callable::CallError call_error;
		staged_successor_ack_active = true;
		host_normal_callable.callp(nullptr, 0, returned, call_error);
		staged_successor_ack_active = false;
		if (call_error.error != Callable::CallError::CALL_OK) { staged_fail(ERR_UNAUTHORIZED, "staged_successor_ack_callable"); return false; }
		if (staged_fail(staged_successor_ack_error, "staged_successor_segment_ack") != OK) { return false; }
		Dictionary ack;
		ack["cursor"] = int64_t(staged_successor_batch_item.cursor);
		ack["segment"] = int64_t(staged_successor_batch_item.segment.segment);
		ack["revision"] = int64_t(staged_successor_batch_item.segment.segment_revision);
		ack["normal_frame"] = int64_t(staged_successor_ack_frame);
		ack["control"] = int64_t(staged_successor_record_control);
		ack["request"] = int64_t(staged_successor_request.handle_serial);
		ack["ack_error"] = staged_successor_ack_error;
		staged_successor_segments.push_back(ack);
		staged_successor_batch_item.flags &= ~2u;
		return false;
	}
	if (staged_successor_batch_count && !staged_successor_batch_cursor) {
		Registry::Lock lock(enrollment.registry);
		staged_successor_unread_after = staged_successor_original_request->native_prepared_unread;
		staged_successor_pending_ack_after = staged_successor_original_request->native_batch_pending_ack;
		if (staged_fail(staged_successor_batch_count == 8 && !staged_successor_unread_after && !staged_successor_pending_ack_after ? OK : ERR_INVALID_DATA, "staged_successor_incomplete_batch") != OK) { return false; }
		return true;
	}
	const auto read = port<GDExtensionInterfaceDeviceSessionProductionNativeBatchReadV2>("device_session_production_native_batch_read_v2");
	if (!read) { staged_fail(ERR_UNAVAILABLE, "staged_successor_batch_port"); return false; }
	staged_successor_batch_item = {};
	if (staged_fail(Error(read(staged_successor_request, staged_successor_record_control, staged_successor_batch_cursor, &staged_successor_batch_item).native_error), "staged_successor_batch_read") != OK) { return false; }
	const auto &result = staged_successor_batch_item.result;
	const auto &ref = result.ref;
	if (!(staged_successor_batch_item.flags & 1u) || staged_successor_batch_count >= 8 || ref.registry_instance != staged_successor_request.registry_instance ||
		ref.request_serial != staged_successor_request.handle_serial || ref.control_serial != staged_successor_record_control || ref.action_ordinal != staged_successor_batch_count ||
		ref.canonical_begin != staged_successor_original_request->operation.canonical_begin || ref.canonical_end != staged_successor_original_request->operation.canonical_end ||
		ref.canonical_suboperation != staged_successor_actions[staged_successor_batch_count].canonical_suboperation ||
		ref.source_recipe_substep != staged_successor_actions[staged_successor_batch_count].source_recipe_substep ||
		result.action_kind != staged_successor_actions[staged_successor_batch_count].action_kind) {
		staged_fail(ERR_INVALID_DATA, "staged_successor_batch_result_identity"); return false;
	}
	Dictionary row;
	row["ordinal"] = int64_t(ref.action_ordinal);
	row["action_kind"] = result.action_kind;
	row["output_kind"] = result.output_kind;
	row["native_error"] = result.result.native_error;
	if (result.output_kind == 1) {
		staged_successor_acquire = result.acquire;
		row["acquire_error"] = staged_successor_acquire.status.error;
		row["acquire_writable_count"] = staged_successor_acquire.writable_count;
		row["lease_token"] = String::hex_encode_buffer(staged_successor_acquire.lease_token.bytes, 32);
	}
	if (result.output_kind == 2) {
		row["finalize_error"] = result.finalize.status.error;
		row["record_token"] = String::hex_encode_buffer(result.finalize.record_token.bytes, 32);
	}
	if (result.output_kind == 3) {
		row["seal_error"] = result.seal.status.error;
		row["packet_token"] = String::hex_encode_buffer(result.seal.packet_token.bytes, 32);
	}
	staged_successor_results.push_back(row);
	staged_successor_batch_cursor = staged_successor_batch_item.next_cursor;
	++staged_successor_batch_count;
	if (result.result.native_error != OK) {
		staged_error = Error(result.result.native_error);
		staged_detail = "staged_successor_core_action";
	}
	return false;
}

bool staged_successor_capture_and_raster() {
	using namespace GDExtensionDeviceExit;
	const auto read = port<GDExtensionInterfaceDeviceSessionProductionReadStagedOutputInputV2>("device_session_production_read_staged_output_input_v2");
	if (!read) { staged_fail(ERR_UNAVAILABLE, "staged_successor_capture_read_port"); return false; }
	if (staged_fail(Error(read(staged_successor_request, 1, &staged_successor_bound_input).native_error), "staged_successor_capture_read") != OK) { return false; }
	const auto &common = staged_successor_bound_input.input.common;
	const auto &r0_common = staged_bound_input.input.common;
	Registry::Lock lock(enrollment.registry);
	const auto &action = staged_successor_original_request->actions[1];
	const auto &capture = action.preinput_camera_source;
	if (capture.is_null() || capture->get_viewport() != host_viewport || capture->get_invocation().camera != host_camera ||
		capture->get_token().view_id != common.current_camera.view_id || capture->get_token().view_epoch != common.current_camera.view_epoch ||
		capture->get_token().capture_ordinal != common.current_camera.capture_ordinal || capture->get_presentation_key().present_seq != common.present_seq ||
		common.present_seq <= r0_common.present_seq || common.current_camera.view_id != r0_common.current_camera.view_id ||
		common.current_camera.view_epoch != r0_common.current_camera.view_epoch || common.current_camera.capture_ordinal <= r0_common.current_camera.capture_ordinal ||
		!common.has_actual_previous_camera || common.actual_previous_camera.view_id != r0_common.current_camera.view_id ||
		common.actual_previous_camera.view_epoch != r0_common.current_camera.view_epoch ||
		common.actual_previous_camera.capture_ordinal != r0_common.current_camera.capture_ordinal ||
		!common.has_previous_consumed_tuple || common.history_reset != 0 ||
		memcmp(common.previous_consumed_tuple_token.bytes, staged_exported_proof.ref.original_color_token.bytes, 32) != 0 || staged_compat_calls) {
		staged_fail(ERR_INVALID_DATA, "staged_successor_capture_history_identity"); return false;
	}
	staged_successor_capture_snapshot["view_id"] = int64_t(common.current_camera.view_id);
	staged_successor_capture_snapshot["view_epoch"] = int64_t(common.current_camera.view_epoch);
	staged_successor_capture_snapshot["capture_ordinal"] = int64_t(common.current_camera.capture_ordinal);
	staged_successor_capture_snapshot["present_seq"] = int64_t(common.present_seq);
	staged_successor_capture_snapshot["has_actual_previous_camera"] = common.has_actual_previous_camera;
	staged_successor_capture_snapshot["actual_previous_capture_ordinal"] = int64_t(common.actual_previous_camera.capture_ordinal);
	staged_successor_capture_snapshot["has_previous_consumed_tuple"] = common.has_previous_consumed_tuple;
	staged_successor_capture_snapshot["viewport"] = int64_t(capture->get_viewport().get_id());
	staged_successor_capture_snapshot["camera"] = int64_t(capture->get_invocation().camera.get_id());
	staged_successor_capture_snapshot["output_slot"] = action.output_slot;
	if (!action.scene_cull_prepared || action.scene_cull_error != OK) {
		staged_fail(ERR_INVALID_DATA, "staged_successor_scene_cull_not_prepared"); return false;
	}
	const auto box = [](const AABB &value) {
		Dictionary result;
		Array position, size;
		for (uint32_t axis = 0; axis < 3; ++axis) { position.push_back(value.position[axis]); size.push_back(value.size[axis]); }
		result["position"] = position; result["size"] = size;
		return result;
	};
	staged_successor_scene_cull_snapshot["prepared"] = action.scene_cull_prepared;
	staged_successor_scene_cull_snapshot["error"] = action.scene_cull_error;
	staged_successor_scene_cull_snapshot["output_slot"] = action.output_slot;
	staged_successor_scene_cull_snapshot["local"] = box(action.scene_cull_local);
	staged_successor_scene_cull_snapshot["world"] = box(action.scene_cull_world);
	staged_successor_scene_cull_snapshot["required_world"] = box(action.scene_cull_required_world);
	auto *storage = RendererRD::ParticlesStorage::get_singleton();
	RID pipeline_rid;
	uint32_t draw = UINT32_MAX, close = UINT32_MAX;
	if (!storage) {
		staged_successor_raster_error = ERR_UNAVAILABLE;
		staged_successor_raster_detail = "successor_particles_storage_missing";
		return true;
	}
	staged_successor_raster_error = RendererRD::ExternalLinkedOwnerTest::staged_color_snapshot(*storage, RID::from_uint64(created.particles),
			action.output_slot, capture, staged_successor_raster_snapshot, pipeline_rid, draw, close);
	if (staged_successor_raster_error != OK) {
		staged_successor_raster_detail = staged_successor_raster_snapshot.get("snapshot_detail", String());
		return true;
	}
	if (draw >= staged_successor_original_request->graph_count || close >= staged_successor_original_request->graph_count) {
		staged_successor_raster_error = ERR_PARAMETER_RANGE_ERROR;
		staged_successor_raster_detail = "successor_color_graph_index_out_of_range";
		return true;
	}
	const auto &draw_handle = staged_successor_original_request->graphs[draw].handle;
	const auto &close_handle = staged_successor_original_request->graphs[close].handle;
	if (draw_handle.kind != 3 || close_handle.kind != 7 || draw_handle.graph_instance != close_handle.graph_instance || draw_handle.graph_frame != close_handle.graph_frame) {
		staged_successor_raster_error = ERR_INVALID_DATA;
		staged_successor_raster_detail = "successor_color_close_handles";
		return true;
	}
	staged_successor_raster_verified = true;
	return true;
}

void staged_receive_successor_cancel(GDExtensionDeviceExit::ProductionPhysicalContextRef context, GDExtensionProductionRequestRefV1 ref) {
	using namespace GDExtensionDeviceExit;
	if (!staged_successor_cancel_control || staged_successor_cancel_entered || ref.registry_instance != staged_successor_request.registry_instance || ref.handle_serial != staged_successor_request.handle_serial) {
		staged_fail(ERR_UNAUTHORIZED, "staged_successor_cancel_receiver_identity"); return;
	}
	staged_successor_cancel_entered = true;
	staged_successor_cancel_frame = context.frame_serial;
	const auto cancel = port<GDExtensionInterfaceDeviceSessionOutputCancelV1>("device_session_output_cancel_v1");
	if (!cancel) { staged_fail(ERR_UNAVAILABLE, "staged_successor_slot_cancel_port"); return; }
	const GDExtensionOutputCancelInputV1 input{ production_dto_abi<GDExtensionOutputCancelInputV1>(), created.particles, staged_successor_acquire.lease_token };
	staged_successor_cancel_status = cancel(context, ref, &input);
}

void advance_staged_execution() {
	using namespace GDExtensionDeviceExit;
	if (staged_execution_step == 0) {
		const auto bind = port<GDExtensionInterfaceDeviceSessionCameraReadyBindRequestV1>("device_session_camera_ready_bind_request_v1");
		if (!bind) { staged_fail(ERR_UNAVAILABLE, "staged_bind_camera_port"); return; }
		if (staged_fail(Error(bind(host_request, 1, staged_relay, host_viewport.get_id()).native_error), "staged_original_bind_camera") == OK) { ++staged_execution_step; }
		return;
	}
	const GDExtensionProductionServiceBudgetV1 turn{ production_dto_abi<GDExtensionProductionServiceBudgetV1>(), host_scene_deadline, 1, 16, 16, 0 };
	if (staged_execution_step == 1) {
		const auto observe = port<GDExtensionInterfaceDeviceSessionProductionObserveV2>("device_session_production_observe_v2");
		if (!observe) { staged_fail(ERR_UNAVAILABLE, "staged_subscribe_observe_port"); return; }
		GDExtensionProductionReportV2 report{};
		const Error e = Error(observe(host_request, &turn, &report).native_error);
		if (staged_fail(e, "staged_original_subscribe_observe") != OK) { return; }
		Registry::Lock lock(enrollment.registry);
		const auto &action = host_original_request->actions[1];
		if (action.host_view_received && !action.camera_ready_subscriber.is_closed() && !host_original_request->capture_inflight &&
			host_original_request->control.state == ProductionControlState::OWNER_CONSUMED) { ++staged_execution_step; }
		return;
	}
	if (staged_execution_step == 2) {
		const auto stage = port<GDExtensionInterfaceDeviceSessionProductionStageOutputPlanV2>("device_session_production_stage_output_plan_v2");
		if (!stage) { staged_fail(ERR_UNAVAILABLE, "staged_output_plan_port"); return; }
		staged_prepare_plan_input();
		if (!staged_record_budget.deadline_micros) { staged_record_budget = { production_dto_abi<GDExtensionProductionServiceBudgetV1>(), host_scene_deadline, 1, 64, 64, 0 }; }
		if (staged_fail(Error(stage(host_request, &staged_plan, &staged_record_budget).native_error), "staged_original_output_plan") != OK) { return; }
		if (staged_genuine_prior()) {
			const auto prepare_consumed = port<GDExtensionInterfaceDeviceSessionOutputConsumedPrepareV2>("device_session_output_consumed_prepare_v2");
			if (!prepare_consumed) { staged_fail(ERR_UNAVAILABLE, "staged_consumed_prepare_port"); return; }
			const GDExtensionOutputConsumedSelectorV2 selector{ production_dto_abi<GDExtensionOutputConsumedSelectorV2>(), 1 };
			if (staged_fail(Error(prepare_consumed(host_request, &selector).native_error), "staged_original_consumed_prepare") != OK) { return; }
			staged_proof_prepared = true;
		}
		++staged_execution_step;
		return;
	}
	if (staged_execution_step == 3) {
		const auto create = port<GDExtensionInterfaceDeviceSessionProductionSetCreateV1>("device_session_production_set_create_v1");
		if (!create) { staged_fail(ERR_UNAVAILABLE, "staged_set_port"); return; }
		GDExtensionProductionSetBindingV1 bindings[5]{};
		for (uint32_t slot = 0; slot < 5; ++slot) {
			bindings[slot] = { production_dto_abi<GDExtensionProductionSetBindingV1>(), 0, slot, 0, slot, 0, staged_resources[slot].physical_bytes };
		}
		if (staged_fail(Error(create(host_request, 6, 5, 0, bindings, 5).native_error), "staged_original_deferred_set") == OK) { ++staged_execution_step; }
		return;
	}
	if (staged_execution_step == 4) {
		const auto prepare = port<GDExtensionInterfaceDeviceSessionProductionPrepareUploadV2>("device_session_production_prepare_upload_v2");
		if (!prepare) { staged_fail(ERR_UNAVAILABLE, "staged_control_upload_port"); return; }
		const uint32_t unbound_camera_control[3]{};
		if (staged_fail(Error(prepare(host_request, &staged_upload, unbound_camera_control, sizeof(unbound_camera_control)).native_error), "staged_original_control_prepare") == OK) { ++staged_execution_step; }
		return;
	}
	if (staged_execution_step == 5) {
		const auto prepare = port<GDExtensionInterfaceDeviceSessionProductionPrepareDispatchV2>("device_session_production_prepare_dispatch_v2");
		if (!prepare) { staged_fail(ERR_UNAVAILABLE, "staged_dispatch_port"); return; }
		float push[24];
		memcpy(push, host_read.affine, sizeof(host_read.affine));
		memcpy(push + 12, host_read.inverse, sizeof(host_read.inverse));
		const GDExtensionProductionDispatchV2 dispatch{ production_dto_abi<GDExtensionProductionDispatchV2>(), 2, 11, 12, 0, 5, 6, 1, 1, 1, 1, sizeof(push), 0 };
		if (staged_fail(Error(prepare(host_request, &dispatch, push, sizeof(push)).native_error), "staged_original_dispatch_prepare") == OK) { ++staged_execution_step; }
		return;
	}
	if (staged_execution_step == 6 || staged_execution_step == 7) {
		const auto prepare = port<GDExtensionInterfaceDeviceSessionProductionPrepareReadbackV2>("device_session_production_prepare_readback_v2");
		if (!prepare) { staged_fail(ERR_UNAVAILABLE, "staged_readback_prepare_port"); return; }
		const uint32_t slot = staged_execution_step - 6;
		if (staged_fail(Error(prepare(host_request, 5 + slot, slot).native_error), "staged_original_readback_prepare") == OK) { ++staged_execution_step; }
		return;
	}
	if (staged_execution_step == 8) {
		const auto bind = port<GDExtensionInterfaceDeviceSessionProductionStageCameraPayloadV2>("device_session_production_stage_camera_payload_v2");
		if (!bind) { staged_fail(ERR_UNAVAILABLE, "staged_camera_payload_port"); return; }
		const GDExtensionProductionCameraPayloadV1 payload{ production_dto_abi<GDExtensionProductionCameraPayloadV1>(), 1, UINT64_MAX, UINT64_MAX, 0, 1, 0 };
		if (staged_fail(Error(bind(host_request, &payload).native_error), "staged_original_camera_payload") == OK) { ++staged_execution_step; }
		return;
	}
	if (staged_execution_step == 9) {
		const auto prepare = port<GDExtensionInterfaceDeviceSessionProductionPrepareNativeRecordV2>("device_session_production_prepare_native_record_v2");
		if (!prepare) { staged_fail(ERR_UNAVAILABLE, "staged_core_executor_port"); return; }
		if (staged_fail(Error(prepare(host_request).native_error), "staged_original_core_executor") == OK) { ++staged_execution_step; }
		return;
	}
	if (staged_execution_step == 10) {
		const auto batch = port<GDExtensionInterfaceDeviceSessionProductionPrepareNativeBatchV2>("device_session_production_prepare_native_batch_v2");
		if (!batch) { staged_fail(ERR_UNAVAILABLE, "staged_native_batch_port"); return; }
		if (staged_fail(Error(batch(host_request).native_error), "staged_original_native_batch") == OK) { ++staged_execution_step; }
		return;
	}
	if (staged_execution_step == 11) {
		const auto schedule = port<GDExtensionInterfaceDeviceSessionProductionRecordScheduleV1>("device_session_production_record_schedule_v1");
		if (!schedule) { staged_fail(ERR_UNAVAILABLE, "staged_record_schedule_port"); return; }
		snapshot_staged_resources("before_selected_capture_control_upload");
		if (staged_fail(Error(schedule(host_request, &staged_record_control)), "staged_original_record_schedule") != OK) { return; }
		staged_native_ready = true;
		++staged_execution_step;
		return;
	}
	if (staged_execution_step == 12) {
		const auto consume = port<GDExtensionInterfaceDeviceSessionProductionRecordConsumeV3>("device_session_production_record_consume_v3");
		if (!consume) { staged_fail(ERR_UNAVAILABLE, "staged_record_consume_port"); return; }
		const Error e = Error(consume(host_request, staged_record_control, &staged_record).native_error);
		if (staged_fail(e, "staged_original_record_consume") != OK) { return; }
		if (staged_record.record.request_serial != host_request.handle_serial || staged_record.record.control_serial != staged_record_control) { staged_fail(ERR_INVALID_DATA, "staged_record_receipt_identity"); return; }
		staged_record_consumed = true;
		staged_native_ready = false;
		++staged_execution_step;
		return;
	}
	if (staged_execution_step == 13) {
		if (staged_read_batch_turn()) { ++staged_execution_step; }
		return;
	}
	if (staged_execution_step == 14) {
		if (staged_record.record.result.native_error != OK) {
			staged_error = Error(staged_record.record.result.native_error);
			staged_detail = "staged_original_record_result";
			return;
		}
		if (staged_capture_and_raster()) { ++staged_execution_step; }
		return;
	}
	if (staged_execution_step == 15) {
		const auto observe = port<GDExtensionInterfaceDeviceSessionProductionObserveV2>("device_session_production_observe_v2");
		if (!observe) { staged_fail(ERR_UNAVAILABLE, "staged_readback_observe_port"); return; }
		GDExtensionProductionControlReceiptV1 before{};
		if (!staged_observe_control_receipt(0, before)) { return; }
		staged_observe_report = {};
		staged_observe_api_result = observe(host_request, &turn, &staged_observe_report);
		const Error e = Error(staged_observe_api_result.native_error);
		if (e != OK && e != ERR_BUSY) { staged_fail(e, "staged_original_readback_observe"); return; }
		if (staged_observe_report.cause.native_error != OK && staged_observe_report.cause.native_error != ERR_BUSY) {
			staged_fail(Error(staged_observe_report.cause.native_error), "staged_original_readback_cause"); return;
		}
		if (!staged_consumed_observe_turn(before, staged_observe_api_result, staged_observe_receipt)) { return; }
		// An adopted slot may still own its physical returns while both original
		// READBACK deliveries are complete. COPY revalidates this exact revision.
		// When continuing to a second presentation on main RD, the FIFO owner
		// must also finish its bounded disposal continuation so _draw() can resume
		// the next rendering frame rather than deferring viewport execution.
		Registry::Lock lock(enrollment.registry);
		if (production_dto_valid(staged_observe_report) && staged_observe_report.revision && host_original_request &&
				host_original_request->serial == host_request.handle_serial && host_original_request->revision == staged_observe_report.revision &&
				staged_observe_report.action_count == 7 && staged_observe_report.recorded_action_end == 7 &&
				staged_observe_report.ready_readbacks == 2 && !staged_observe_report.possible_obligations &&
				staged_observe_report.complete_obligations == staged_observe_report.known_obligations &&
				(staged_observe_report.cause.native_error == OK || staged_observe_report.physical_returns_pending) &&
				(!staged_genuine_prior() || (!host_original_request->maintenance_serial && !RenderingDevice::get_singleton()->native_disposal_active.load(std::memory_order_acquire)))) {
			++staged_execution_step;
		}
		return;
	}
	if (staged_execution_step == 16 || staged_execution_step == 17) {
		const auto copy = port<GDExtensionInterfaceDeviceSessionProductionCopyV1>("device_session_production_copy_v1");
		if (!copy) { staged_fail(ERR_UNAVAILABLE, "staged_readback_copy_port"); return; }
		const uint32_t slot = staged_execution_step - 16;
		uint64_t written = 0;
		const uint64_t bytes = slot ? sizeof(staged_record_words) : sizeof(staged_guard_words);
		const auto copied = copy(host_request, staged_observe_report.revision, slot, slot ? staged_record_words : staged_guard_words, bytes, &written);
		if (staged_fail(Error(copied.native_error), "staged_original_readback_copy") != OK) { return; }
		if (written != bytes) { staged_fail(ERR_INVALID_DATA, "staged_original_readback_extent"); return; }
		Dictionary row;
		row["slot"] = slot;
		row["source_kind"] = staged_readbacks[slot].source_kind;
		row["resource_slot"] = staged_readbacks[slot].resource_slot;
		{
			Registry::Lock lock(enrollment.registry);
			const auto &original = host_original_request->readbacks[slot];
			const auto &resource = host_original_request->resources[original.declaration.resource_slot];
			row["producer_action_ordinal"] = int64_t(original.declaration.producer_action_ordinal);
			row["byte_offset"] = int64_t(original.declaration.byte_offset);
			row["source_rid"] = int64_t(resource.primary.get_id());
			row["source_backing_serial"] = int64_t(original.source.backing_serial);
			row["issued"] = int64_t(original.source.issued);
			row["completed"] = int64_t(original.source.completed);
			row["returned"] = int64_t(original.source.returned);
			row["owned_bytes"] = int64_t(original.source.owned_bytes);
			row["delivered"] = original.source.delivered;
		}
		row["copy_action_ordinal"] = int64_t(staged_readbacks[slot].copy_action_ordinal);
		row["byte_size"] = int64_t(bytes);
		row["written"] = int64_t(written);
		row["revision"] = int64_t(staged_observe_report.revision);
		row["copy_error"] = copied.native_error;
		staged_readback_rows.push_back(row);
		++staged_execution_step;
		if (slot) {
			const uint64_t sequence = staged_bound_input.input.common.present_seq;
			const bool guard = !staged_guard_words[0] && staged_guard_words[1] == 1 && staged_guard_words[4] == uint32_t(sequence) && staged_guard_words[5] == uint32_t(sequence >> 32);
			uint32_t expected_record[48]{};
			memcpy(expected_record, host_read.affine, sizeof(host_read.affine));
			memcpy(expected_record + 24, host_read.inverse, sizeof(host_read.inverse));
			for (uint32_t row = 0; row < 3; ++row) {
				for (uint32_t col = 0; col < 3; ++col) {
					memcpy(expected_record + 12 + row * 4 + col, &host_read.inverse[col * 4 + row], sizeof(uint32_t));
				}
			}
			const float color[4] = { 0.75f, 0.25f, 0.125f, 1.0f };
			memcpy(expected_record + 36, color, sizeof(color));
			expected_record[44] = expected_record[46] = 1;
			const bool record = memcmp(expected_record, staged_record_words, sizeof(expected_record)) == 0;
			if (!guard || !record) { staged_fail(ERR_INVALID_DATA, "staged_original_gpu_words"); return; }
			staged_readbacks_verified = true;
		}
		return;
	}
	if (!staged_genuine_prior()) { return; }
	if (staged_execution_step == 18) {
		const auto borrow = port<GDExtensionInterfaceDeviceSessionOutputConsumedBorrowV2>("device_session_output_consumed_borrow_v2");
		const auto read_proof = port<GDExtensionInterfaceDeviceSessionOutputConsumedReadV2>("device_session_output_consumed_read_v2");
		if (!borrow || !read_proof) { staged_fail(ERR_UNAVAILABLE, "staged_consumed_borrow_ports"); return; }
		const GDExtensionOutputConsumedSelectorV2 selector{ production_dto_abi<GDExtensionOutputConsumedSelectorV2>(), 1 };
		staged_exported_borrow = {};
		const Error b_err = Error(borrow(host_request, &selector, &turn, &staged_exported_borrow).native_error);
		if (staged_fail(b_err, "staged_original_consumed_borrow") != OK) { return; }
		if (!production_dto_valid(staged_exported_borrow) || !staged_exported_borrow.proof.ref.borrow_control_serial) {
			staged_fail(ERR_INVALID_DATA, "staged_consumed_borrow_projection"); return;
		}
		staged_exported_proof = {};
		const Error r_err = Error(read_proof(host_request, &staged_exported_borrow.proof.ref, &staged_exported_proof).native_error);
		if (staged_fail(r_err, "staged_original_consumed_read") != OK) { return; }
		if (!production_dto_valid(staged_exported_proof) || staged_exported_proof.ref.particles != created.particles ||
			staged_exported_proof.ref.generation != created.generation || staged_exported_proof.ref.request_serial != host_request.handle_serial ||
			staged_exported_proof.original_acquire.common.present_seq != staged_bound_input.input.common.present_seq) {
			staged_fail(ERR_INVALID_DATA, "staged_consumed_proof_identity"); return;
		}
		staged_proof_exported = true;
		++staged_execution_step;
		return;
	}
	if (staged_execution_step == 19) {
		staged_successor_transform = Transform3D(Basis::from_scale(Vector3(1.5f, 1.0f, 0.5f)), Vector3(0.125f, 0.0f, 0.0f));
		staged_successor_host_input = {};
		staged_successor_host_input.abi = production_dto_abi<GDExtensionProductionHostTransformInputV1>();
		for (uint32_t row = 0; row < 3; ++row) {
			for (uint32_t column = 0; column < 3; ++column) {
				staged_successor_host_input.affine[row * 4 + column] = float(staged_successor_transform.basis[row][column]);
			}
			staged_successor_host_input.affine[row * 4 + 3] = float(staged_successor_transform.origin[row]);
		}
		for (uint32_t i = 0; i < 32; ++i) {
			staged_successor_host_input.host_token.bytes[i] = uint8_t(source.backing.handle_serial + i + 65);
		}
		server->instance_set_transform(host_instance, staged_successor_transform);
		staged_successor_scene_input = { production_dto_abi<GDExtensionProductionOutputSceneInputV1>(), host_instance.get_id(), created.particles, created.generation, host_scenario.get_id(), staged_successor_host_input };
		Variant returned;
		Callable::CallError call_error;
		staged_successor_normal_prepare_active = true;
		host_normal_callable.callp(nullptr, 0, returned, call_error);
		staged_successor_normal_prepare_active = false;
		if (call_error.error != Callable::CallError::CALL_OK) { staged_fail(ERR_UNAUTHORIZED, "staged_successor_callable_admission"); return; }
		if (staged_error != OK || staged_successor_host_prepare_result.native_error == ERR_BUSY || staged_successor_scene_result.native_error == ERR_BUSY ||
			staged_successor_view_result.native_error == ERR_BUSY || staged_successor_host_read_result.native_error == ERR_BUSY) { return; }
		auto *scene = static_cast<RendererSceneCull *>(RSG::scene);
		scene->update_dirty_instances();
		RendererSceneCull::NativeParticleSceneFacts facts;
		if (staged_fail(scene->instance_get_committed_particle_scene_facts(host_instance, RID::from_uint64(created.particles), facts), "staged_successor_scene_commit") != OK) { return; }
		if (facts.instance != host_instance || facts.transform != staged_successor_transform) { staged_fail(ERR_INVALID_DATA, "staged_successor_committed_tuple"); return; }
		++staged_execution_step;
		return;
	}
	if (staged_execution_step == 20) {
		const auto stage = port<GDExtensionInterfaceDeviceSessionProductionStageOutputPlanWithHistoryGuardV2>("device_session_production_stage_output_plan_with_history_guard_v2");
		if (!stage) { staged_fail(ERR_UNAVAILABLE, "staged_successor_plan_guard_port"); return; }
		staged_prepare_successor_plan_input();
		if (!staged_successor_record_budget.deadline_micros) {
			staged_successor_record_budget = { production_dto_abi<GDExtensionProductionServiceBudgetV1>(), host_scene_deadline, 1, 64, 64, 0 };
		}
		const Error e = Error(stage(staged_successor_request, &staged_successor_plan2, 9, &staged_successor_record_budget, &turn).native_error);
		if (staged_fail(e, "staged_successor_output_plan_with_history_guard") == OK) { ++staged_execution_step; }
		return;
	}
	if (staged_execution_step == 21) {
		const auto bind = port<GDExtensionInterfaceDeviceSessionCameraReadyBindRequestV1>("device_session_camera_ready_bind_request_v1");
		if (!bind) { staged_fail(ERR_UNAVAILABLE, "staged_successor_bind_camera_port"); return; }
		if (staged_fail(Error(bind(staged_successor_request, 1, staged_relay, host_viewport.get_id()).native_error), "staged_successor_bind_camera") == OK) { ++staged_execution_step; }
		return;
	}
	if (staged_execution_step == 22) {
		const auto observe = port<GDExtensionInterfaceDeviceSessionProductionObserveV2>("device_session_production_observe_v2");
		if (!observe) { staged_fail(ERR_UNAVAILABLE, "staged_successor_subscribe_observe_port"); return; }
		GDExtensionProductionReportV2 report{};
		const Error e = Error(observe(staged_successor_request, &turn, &report).native_error);
		if (e != OK && e != ERR_BUSY) { staged_fail(e, "staged_successor_subscribe_observe"); return; }
		Registry::Lock lock(enrollment.registry);
		const auto &action = staged_successor_original_request->actions[1];
		if (action.host_view_received && !action.camera_ready_subscriber.is_closed() &&
			!staged_successor_original_request->capture_inflight && !staged_successor_original_request->maintenance_serial &&
			!RenderingDevice::get_singleton()->native_disposal_active.load(std::memory_order_acquire) &&
			staged_successor_original_request->control.state == ProductionControlState::OWNER_CONSUMED) {
			++staged_execution_step;
		}
		return;
	}
	if (staged_execution_step == 23) {
		const auto create = port<GDExtensionInterfaceDeviceSessionProductionSetCreateV1>("device_session_production_set_create_v1");
		if (!create) { staged_fail(ERR_UNAVAILABLE, "staged_successor_set_port"); return; }
		const uint32_t resource_slots[7] = { 0, 1, 2, 3, 4, 8, 9 };
		GDExtensionProductionSetBindingV1 bindings[7]{};
		for (uint32_t b = 0; b < 7; ++b) {
			const uint32_t rslot = resource_slots[b];
			bindings[b] = { production_dto_abi<GDExtensionProductionSetBindingV1>(), 0, b, 0, rslot, 0, staged_successor_resources[rslot].physical_bytes };
		}
		if (staged_fail(Error(create(staged_successor_request, 6, 5, 0, bindings, 7).native_error), "staged_successor_deferred_set") == OK) { ++staged_execution_step; }
		return;
	}
	if (staged_execution_step == 24) {
		const auto prepare = port<GDExtensionInterfaceDeviceSessionProductionPrepareUploadV2>("device_session_production_prepare_upload_v2");
		if (!prepare) { staged_fail(ERR_UNAVAILABLE, "staged_successor_control_upload_port"); return; }
		const uint32_t unbound_camera_control[3]{};
		if (staged_fail(Error(prepare(staged_successor_request, &staged_successor_upload, unbound_camera_control, sizeof(unbound_camera_control)).native_error), "staged_successor_control_prepare") == OK) { ++staged_execution_step; }
		return;
	}
	if (staged_execution_step == 25) {
		const auto prepare = port<GDExtensionInterfaceDeviceSessionProductionPrepareDispatchV2>("device_session_production_prepare_dispatch_v2");
		if (!prepare) { staged_fail(ERR_UNAVAILABLE, "staged_successor_dispatch_port"); return; }
		float push[24];
		memcpy(push, staged_successor_host_read.affine, sizeof(staged_successor_host_read.affine));
		memcpy(push + 12, staged_successor_host_read.inverse, sizeof(staged_successor_host_read.inverse));
		const GDExtensionProductionDispatchV2 dispatch{ production_dto_abi<GDExtensionProductionDispatchV2>(), 2, 11, 12, 0, 5, 6, 1, 1, 1, 1, sizeof(push), 0 };
		if (staged_fail(Error(prepare(staged_successor_request, &dispatch, push, sizeof(push)).native_error), "staged_successor_dispatch_prepare") == OK) { ++staged_execution_step; }
		return;
	}
	if (staged_execution_step >= 26 && staged_execution_step <= 28) {
		const auto prepare = port<GDExtensionInterfaceDeviceSessionProductionPrepareReadbackV2>("device_session_production_prepare_readback_v2");
		if (!prepare) { staged_fail(ERR_UNAVAILABLE, "staged_successor_readback_prepare_port"); return; }
		const uint32_t slot = staged_execution_step - 26;
		if (staged_fail(Error(prepare(staged_successor_request, 5 + slot, slot).native_error), "staged_successor_readback_prepare") == OK) { ++staged_execution_step; }
		return;
	}
	if (staged_execution_step == 29) {
		const auto bind = port<GDExtensionInterfaceDeviceSessionProductionStageCameraPayloadV2>("device_session_production_stage_camera_payload_v2");
		if (!bind) { staged_fail(ERR_UNAVAILABLE, "staged_successor_camera_payload_port"); return; }
		const GDExtensionProductionCameraPayloadV1 payload{ production_dto_abi<GDExtensionProductionCameraPayloadV1>(), 1, UINT64_MAX, UINT64_MAX, 0, 1, 0 };
		if (staged_fail(Error(bind(staged_successor_request, &payload).native_error), "staged_successor_camera_payload") == OK) { ++staged_execution_step; }
		return;
	}
	if (staged_execution_step == 30) {
		const auto prepare = port<GDExtensionInterfaceDeviceSessionProductionPrepareNativeRecordV2>("device_session_production_prepare_native_record_v2");
		if (!prepare) { staged_fail(ERR_UNAVAILABLE, "staged_successor_core_executor_port"); return; }
		if (staged_fail(Error(prepare(staged_successor_request).native_error), "staged_successor_core_executor") == OK) { ++staged_execution_step; }
		return;
	}
	if (staged_execution_step == 31) {
		const auto batch = port<GDExtensionInterfaceDeviceSessionProductionPrepareNativeBatchV2>("device_session_production_prepare_native_batch_v2");
		if (!batch) { staged_fail(ERR_UNAVAILABLE, "staged_successor_native_batch_port"); return; }
		if (staged_fail(Error(batch(staged_successor_request).native_error), "staged_successor_native_batch") == OK) { ++staged_execution_step; }
		return;
	}
	if (staged_execution_step == 32) {
		const auto schedule = port<GDExtensionInterfaceDeviceSessionProductionRecordScheduleV1>("device_session_production_record_schedule_v1");
		if (!schedule) { staged_fail(ERR_UNAVAILABLE, "staged_successor_record_schedule_port"); return; }
		if (staged_fail(Error(schedule(staged_successor_request, &staged_successor_record_control)), "staged_successor_record_schedule") != OK) { return; }
		staged_successor_native_ready = true;
		++staged_execution_step;
		return;
	}
	if (staged_execution_step == 33) {
		const auto consume = port<GDExtensionInterfaceDeviceSessionProductionRecordConsumeV3>("device_session_production_record_consume_v3");
		if (!consume) { staged_fail(ERR_UNAVAILABLE, "staged_successor_record_consume_port"); return; }
		const Error e = Error(consume(staged_successor_request, staged_successor_record_control, &staged_successor_record).native_error);
		if (staged_fail(e, "staged_successor_record_consume") != OK) { return; }
		if (staged_successor_record.record.request_serial != staged_successor_request.handle_serial || staged_successor_record.record.control_serial != staged_successor_record_control) {
			staged_fail(ERR_INVALID_DATA, "staged_successor_record_receipt_identity"); return;
		}
		staged_successor_record_consumed = true;
		staged_successor_native_ready = false;
		++staged_execution_step;
		return;
	}
	if (staged_execution_step == 34) {
		if (staged_read_successor_batch_turn()) { ++staged_execution_step; }
		return;
	}
	if (staged_execution_step == 35) {
		if (staged_successor_record.record.result.native_error != OK) {
			staged_error = Error(staged_successor_record.record.result.native_error);
			staged_detail = "staged_successor_record_result";
			return;
		}
		if (staged_successor_capture_and_raster()) { ++staged_execution_step; }
		return;
	}
	if (staged_execution_step == 36) {
		const auto observe = port<GDExtensionInterfaceDeviceSessionProductionObserveV2>("device_session_production_observe_v2");
		if (!observe) { staged_fail(ERR_UNAVAILABLE, "staged_successor_readback_observe_port"); return; }
		GDExtensionProductionControlReceiptV1 before{};
		if (!staged_observe_successor_control_receipt(0, before)) { return; }
		staged_successor_observe_report = {};
		staged_successor_observe_api_result = observe(staged_successor_request, &turn, &staged_successor_observe_report);
		const Error e = Error(staged_successor_observe_api_result.native_error);
		if (e != OK && e != ERR_BUSY) { staged_fail(e, "staged_successor_readback_observe"); return; }
		if (staged_successor_observe_report.cause.native_error != OK && staged_successor_observe_report.cause.native_error != ERR_BUSY) {
			staged_fail(Error(staged_successor_observe_report.cause.native_error), "staged_successor_readback_cause"); return;
		}
		if (!staged_consumed_successor_observe_turn(before, staged_successor_observe_api_result, staged_successor_observe_receipt)) { return; }
		Registry::Lock lock(enrollment.registry);
		if (production_dto_valid(staged_successor_observe_report) && staged_successor_observe_report.revision && staged_successor_original_request &&
				staged_successor_original_request->serial == staged_successor_request.handle_serial &&
				staged_successor_original_request->revision == staged_successor_observe_report.revision &&
				staged_successor_observe_report.action_count == 8 && staged_successor_observe_report.recorded_action_end == 8 &&
				staged_successor_observe_report.ready_readbacks == 3 && !staged_successor_observe_report.possible_obligations &&
				staged_successor_observe_report.complete_obligations == staged_successor_observe_report.known_obligations &&
				!staged_successor_original_request->maintenance_serial &&
				!RenderingDevice::get_singleton()->native_disposal_active.load(std::memory_order_acquire) &&
				(staged_successor_observe_report.cause.native_error == OK || staged_successor_observe_report.physical_returns_pending)) {
			++staged_execution_step;
		}
		return;
	}
	if (staged_execution_step >= 37 && staged_execution_step <= 39) {
		const auto copy = port<GDExtensionInterfaceDeviceSessionProductionCopyV1>("device_session_production_copy_v1");
		if (!copy) { staged_fail(ERR_UNAVAILABLE, "staged_successor_readback_copy_port"); return; }
		const uint32_t slot = staged_execution_step - 37;
		uint64_t written = 0;
		void *dst = slot == 0 ? static_cast<void *>(staged_successor_guard_words) : (slot == 1 ? static_cast<void *>(staged_successor_current_words) : static_cast<void *>(staged_successor_previous_words));
		const uint64_t bytes = slot == 0 ? sizeof(staged_successor_guard_words) : sizeof(staged_successor_current_words);
		const auto copied = copy(staged_successor_request, staged_successor_observe_report.revision, slot, dst, bytes, &written);
		if (staged_fail(Error(copied.native_error), "staged_successor_readback_copy") != OK) { return; }
		if (written != bytes) { staged_fail(ERR_INVALID_DATA, "staged_successor_readback_extent"); return; }
		Dictionary row;
		row["slot"] = slot;
		row["source_kind"] = staged_successor_readbacks[slot].source_kind;
		row["resource_slot"] = staged_successor_readbacks[slot].resource_slot;
		row["byte_size"] = int64_t(bytes);
		row["written"] = int64_t(written);
		row["revision"] = int64_t(staged_successor_observe_report.revision);
		row["copy_error"] = copied.native_error;
		staged_successor_readback_rows.push_back(row);
		++staged_execution_step;
		if (slot == 2) {
			const uint64_t sequence = staged_successor_bound_input.input.common.present_seq;
			const bool guard = !staged_successor_guard_words[0] && staged_successor_guard_words[1] == 1 &&
				staged_successor_guard_words[4] == uint32_t(sequence) && staged_successor_guard_words[5] == uint32_t(sequence >> 32);
			uint32_t expected_current[48]{};
			memcpy(expected_current, staged_successor_host_read.affine, sizeof(staged_successor_host_read.affine));
			memcpy(expected_current + 24, staged_successor_host_read.inverse, sizeof(staged_successor_host_read.inverse));
			for (uint32_t r = 0; r < 3; ++r) {
				for (uint32_t c = 0; c < 3; ++c) {
					memcpy(expected_current + 12 + r * 4 + c, &staged_successor_host_read.inverse[c * 4 + r], sizeof(uint32_t));
				}
			}
			const float color[4] = { 0.75f, 0.25f, 0.125f, 1.0f };
			memcpy(expected_current + 36, color, sizeof(color));
			expected_current[44] = 1;
			expected_current[46] = 3; // VALID | HISTORY
			const bool current_ok = memcmp(expected_current, staged_successor_current_words, sizeof(expected_current)) == 0;
			const bool previous_ok = memcmp(staged_record_words, staged_successor_previous_words, sizeof(staged_record_words)) == 0;
			const bool distinct_ok = memcmp(staged_successor_current_words, staged_successor_previous_words, sizeof(staged_successor_current_words)) != 0;
			if (!guard || !current_ok || !previous_ok || !distinct_ok) {
				staged_fail(ERR_INVALID_DATA, "staged_successor_gpu_history_words"); return;
			}
			staged_successor_readbacks_verified = true;
		}
	}
}

void staged_receive_cancel(GDExtensionDeviceExit::ProductionPhysicalContextRef context, GDExtensionProductionRequestRefV1 ref) {
	using namespace GDExtensionDeviceExit;
	if (!staged_cancel_control || staged_cancel_entered || ref.registry_instance != host_request.registry_instance || ref.handle_serial != host_request.handle_serial) {
		staged_fail(ERR_UNAUTHORIZED, "staged_original_cancel_receiver_identity"); return;
	}
	staged_cancel_entered = true;
	staged_cancel_frame = context.frame_serial;
	const auto cancel = port<GDExtensionInterfaceDeviceSessionOutputCancelV1>("device_session_output_cancel_v1");
	if (!cancel) { staged_fail(ERR_UNAVAILABLE, "staged_original_slot_cancel_port"); return; }
	const GDExtensionOutputCancelInputV1 input{ production_dto_abi<GDExtensionOutputCancelInputV1>(), created.particles, staged_acquire.lease_token };
	staged_cancel_status = cancel(context, ref, &input);
}

void advance_staged_close() {
	using namespace GDExtensionDeviceExit;
	const GDExtensionProductionServiceBudgetV1 budget{ production_dto_abi<GDExtensionProductionServiceBudgetV1>(), host_scene_deadline, 1, 16, 16, 0 };
	if (staged_genuine_prior() && !staged_successor_closed) {
		if (staged_successor_close_step == 0) {
			const auto ret = port<GDExtensionInterfaceDeviceSessionOutputConsumedReturnV2>("device_session_output_consumed_return_v2");
			if (!ret) { staged_fail(ERR_UNAVAILABLE, "staged_consumed_return_port"); return; }
			staged_proof_return_report = {};
			const Error e = Error(ret(host_request, &staged_exported_proof.ref, &budget, &staged_proof_return_report).native_error);
			if (staged_fail(e, "staged_original_consumed_return") != OK) { return; }
			{
				Registry::Lock lock(enrollment.registry);
				if (!host_original_request || !host_original_request->actions[1].consumed_proof_returned) {
					staged_fail(ERR_INVALID_DATA, "staged_consumed_proof_not_returned"); return;
				}
			}
			staged_proof_returned = true;
			++staged_successor_close_step;
			return;
		}
		if (staged_successor_close_step == 1) {
			const auto schedule = port<GDExtensionInterfaceDeviceSessionProductionOutputCancelScheduleV1>("device_session_production_output_cancel_schedule_v1");
			if (!schedule) { staged_fail(ERR_UNAVAILABLE, "staged_successor_slot_cancel_schedule_port"); return; }
			if (staged_fail(Error(schedule(staged_successor_request, &budget, &staged_successor_cancel_control)), "staged_successor_slot_cancel_schedule") == OK) { ++staged_successor_close_step; }
			return;
		}
		if (staged_successor_close_step == 2) {
			const auto consume = port<GDExtensionInterfaceDeviceSessionProductionRecordConsumeV2>("device_session_production_record_consume_v2");
			if (!consume) { staged_fail(ERR_UNAVAILABLE, "staged_successor_slot_cancel_consume_port"); return; }
			if (staged_fail(Error(consume(staged_successor_request, staged_successor_cancel_control, &staged_successor_cancel_receipt).native_error), "staged_successor_slot_cancel_consume") != OK) { return; }
			if (!staged_successor_cancel_entered || !staged_successor_cancel_frame || staged_successor_cancel_receipt.request_serial != staged_successor_request.handle_serial ||
				staged_successor_cancel_receipt.control_serial != staged_successor_cancel_control) {
				staged_fail(ERR_INVALID_DATA, "staged_successor_cancel_receipt_identity"); return;
			}
			if (staged_successor_cancel_receipt.result.native_error != OK || staged_successor_cancel_status.error != OK) {
				staged_error = Error(staged_successor_cancel_receipt.result.native_error ? staged_successor_cancel_receipt.result.native_error : staged_successor_cancel_status.error);
				staged_detail = "staged_successor_slot_cancel_return";
				return;
			}
			++staged_successor_close_step;
			return;
		}
		if (staged_successor_close_step == 3) {
			const auto cancel = port<GDExtensionInterfaceDeviceSessionProductionCancelV1>("device_session_production_cancel_v1");
			if (!cancel) { staged_fail(ERR_UNAVAILABLE, "staged_successor_request_cancel_port"); return; }
			if (staged_fail(Error(cancel(staged_successor_request).native_error), "staged_successor_request_cancel") == OK) { ++staged_successor_close_step; }
			return;
		}
		if (staged_successor_close_step == 4) {
			const auto retire = port<GDExtensionInterfaceDeviceSessionProductionStorageRetireV1>("device_session_production_storage_retire_v1");
			if (!retire) { staged_fail(ERR_UNAVAILABLE, "staged_successor_control_retire_port"); return; }
			const GDExtensionStorageRetireRequestV1 origin{ Enrollment::ABI, staged_successor_control.backing, staged_successor_control.rid, staged_successor_control.backing_serial, 10, 0, 1, 1, 0 };
			if (staged_fail(Error(retire(staged_successor_request, 1, &origin, &staged_successor_control_retire, &staged_successor_control_return).native_error), "staged_successor_control_retire") == OK) { ++staged_successor_close_step; }
			return;
		}
		if (staged_successor_close_step == 5) {
			const auto observe = port<GDExtensionInterfaceDeviceSessionProductionStorageObserveV1>("device_session_production_storage_observe_v1");
			const auto release = port<GDExtensionInterfaceDeviceSessionProductionStorageReleaseV1>("device_session_production_storage_release_v1");
			if (!observe || !release) { staged_fail(ERR_UNAVAILABLE, "staged_successor_control_return_ports"); return; }
			GDExtensionProductionControlReceiptV1 before{};
			if (!staged_observe_successor_control_receipt(0, before)) { return; }
			staged_successor_control_observe_api_result = observe(staged_successor_request, staged_successor_control_retire, &budget, &staged_successor_control_return);
			const Error e = Error(staged_successor_control_observe_api_result.native_error);
			if (e != OK && e != ERR_BUSY) { staged_fail(e, "staged_successor_control_observe"); return; }
			if (!staged_consumed_successor_observe_turn(before, staged_successor_control_observe_api_result, staged_successor_control_observe_receipt) || staged_successor_control_return.state != 1) { return; }
			if (staged_successor_control_return.return_status != 1 || !staged_successor_control_return.event_seq ||
				staged_successor_control_return.rid != staged_successor_control.rid ||
				staged_successor_control_return.backing_serial != staged_successor_control.backing_serial ||
				staged_successor_control_return.retire_cell_serial != staged_successor_control_retire.handle_serial) {
				staged_fail(ERR_INVALID_DATA, "staged_successor_control_return_identity"); return;
			}
			{
				Registry::Lock lock(enrollment.registry);
				if (!staged_successor_original_request || staged_successor_original_request->serial != staged_successor_request.handle_serial ||
						staged_successor_original_request->control.serial != staged_successor_control_observe_receipt.control_serial ||
						staged_successor_original_request->control.state != ProductionControlState::OWNER_CONSUMED) {
					staged_fail(ERR_INVALID_DATA, "staged_successor_owner_observe_identity"); return;
				}
				if (staged_successor_original_request->output_owner_head != UINT32_MAX) { return; }
				for (uint32_t i = 0; i < staged_successor_original_request->capacity.resource_uses; ++i) {
					if (staged_successor_original_request->uses[i].output_consumer) { return; }
				}
				staged_successor_slot_owner_returned = true;
			}
			if (staged_fail(Error(release(staged_successor_request, staged_successor_control_retire).native_error), "staged_successor_control_release") == OK) { ++staged_successor_close_step; }
			return;
		}
		if (staged_successor_close_step == 6) {
			const auto release = port<GDExtensionInterfaceDeviceSessionProductionReleaseV2>("device_session_production_release_v2");
			if (!release) { staged_fail(ERR_UNAVAILABLE, "staged_successor_request_release_port"); return; }
			GDExtensionProductionServiceBudgetV1 remaining{};
			if (staged_fail(Error(release(staged_successor_request, &budget, &remaining, &staged_successor_release_report).native_error), "staged_successor_request_release") != OK) { return; }
			if (staged_successor_release_report.service_pending || !staged_successor_original_request || staged_successor_original_request->serial) {
				staged_fail(ERR_INVALID_DATA, "staged_successor_request_reclaim"); return;
			}
			staged_successor_original_request = nullptr;
			staged_successor_reclaimed = true;
			staged_successor_closed = true;
			host_close_order.push_back("successor_original_request_reclaimed");
			++staged_successor_close_step;
			return;
		}
	}
	if (staged_close_step == 0) {
		const auto schedule = port<GDExtensionInterfaceDeviceSessionProductionOutputCancelScheduleV1>("device_session_production_output_cancel_schedule_v1");
		if (!schedule) { staged_fail(ERR_UNAVAILABLE, "staged_slot_cancel_schedule_port"); return; }
		if (staged_fail(Error(schedule(host_request, &budget, &staged_cancel_control)), "staged_original_slot_cancel_schedule") == OK) { ++staged_close_step; }
		return;
	}
	if (staged_close_step == 1) {
		const auto consume = port<GDExtensionInterfaceDeviceSessionProductionRecordConsumeV2>("device_session_production_record_consume_v2");
		if (!consume) { staged_fail(ERR_UNAVAILABLE, "staged_slot_cancel_consume_port"); return; }
		if (staged_fail(Error(consume(host_request, staged_cancel_control, &staged_cancel_receipt).native_error), "staged_original_slot_cancel_consume") != OK) { return; }
		if (!staged_cancel_entered || !staged_cancel_frame || staged_cancel_receipt.request_serial != host_request.handle_serial || staged_cancel_receipt.control_serial != staged_cancel_control) {
			staged_fail(ERR_INVALID_DATA, "staged_cancel_receipt_identity"); return;
		}
		if (staged_cancel_receipt.result.native_error != OK || staged_cancel_status.error != OK) {
			staged_error = Error(staged_cancel_receipt.result.native_error ? staged_cancel_receipt.result.native_error : staged_cancel_status.error);
			staged_detail = "staged_original_slot_cancel_return";
			return;
		}
		++staged_close_step;
		return;
	}
	if (staged_close_step == 2) {
		const auto cancel = port<GDExtensionInterfaceDeviceSessionProductionCancelV1>("device_session_production_cancel_v1");
		if (!cancel) { staged_fail(ERR_UNAVAILABLE, "staged_request_cancel_port"); return; }
		if (staged_fail(Error(cancel(host_request).native_error), "staged_original_request_cancel") == OK) { ++staged_close_step; }
		return;
	}
	if (staged_close_step == 3) {
		const auto retire = port<GDExtensionInterfaceDeviceSessionProductionStorageRetireV1>("device_session_production_storage_retire_v1");
		if (!retire) { staged_fail(ERR_UNAVAILABLE, "staged_control_retire_port"); return; }
		const GDExtensionStorageRetireRequestV1 origin{ Enrollment::ABI, staged_control.backing, staged_control.rid, staged_control.backing_serial, 9, 0, 1, 1, 0 };
		if (staged_fail(Error(retire(host_request, 1, &origin, &staged_control_retire, &staged_control_return).native_error), "staged_original_control_retire") == OK) { ++staged_close_step; }
		return;
	}
	if (staged_close_step == 4) {
		const auto observe = port<GDExtensionInterfaceDeviceSessionProductionStorageObserveV1>("device_session_production_storage_observe_v1");
		const auto release = port<GDExtensionInterfaceDeviceSessionProductionStorageReleaseV1>("device_session_production_storage_release_v1");
		if (!observe || !release) { staged_fail(ERR_UNAVAILABLE, "staged_control_return_ports"); return; }
		GDExtensionProductionControlReceiptV1 before{};
		if (!staged_observe_control_receipt(0, before)) { return; }
		staged_control_observe_api_result = observe(host_request, staged_control_retire, &budget, &staged_control_return);
		const Error e = Error(staged_control_observe_api_result.native_error);
		if (e != OK && e != ERR_BUSY) { staged_fail(e, "staged_original_control_observe"); return; }
		if (!staged_consumed_observe_turn(before, staged_control_observe_api_result, staged_control_observe_receipt) || staged_control_return.state != 1) { return; }
		if (staged_control_return.return_status != 1 || !staged_control_return.event_seq || staged_control_return.rid != staged_control.rid ||
			staged_control_return.backing_serial != staged_control.backing_serial || staged_control_return.retire_cell_serial != staged_control_retire.handle_serial) {
			staged_fail(ERR_INVALID_DATA, "staged_control_return_identity"); return;
		}
		{
			Registry::Lock lock(enrollment.registry);
			if (!host_original_request || host_original_request->serial != host_request.handle_serial ||
					host_original_request->control.serial != staged_control_observe_receipt.control_serial ||
					host_original_request->control.state != ProductionControlState::OWNER_CONSUMED) {
				staged_fail(ERR_INVALID_DATA, "staged_output_owner_observe_identity"); return;
			}
			if (host_original_request->output_owner_head != UINT32_MAX) { return; }
			for (uint32_t i = 0; i < host_original_request->capacity.resource_uses; ++i) {
				if (host_original_request->uses[i].output_consumer) { return; }
			}
			staged_slot_owner_returned = true;
		}
		if (staged_fail(Error(release(host_request, staged_control_retire).native_error), "staged_original_control_release") == OK) { ++staged_close_step; }
		return;
	}
	if (staged_close_step == 5) {
		const auto release = port<GDExtensionInterfaceDeviceSessionProductionReleaseV2>("device_session_production_release_v2");
		if (!release) { staged_fail(ERR_UNAVAILABLE, "staged_request_release_port"); return; }
		GDExtensionProductionServiceBudgetV1 remaining{};
		if (staged_fail(Error(release(host_request, &budget, &remaining, &host_release_report).native_error), "staged_original_request_release") != OK) { return; }
		if (host_release_report.service_pending || !host_original_request || host_original_request->serial) { staged_fail(ERR_INVALID_DATA, "staged_original_request_reclaim"); return; }
		host_original_request = nullptr;
		host_request_reclaimed = true;
		host_close_order.push_back("host_original_request_reclaimed");
		++staged_close_step;
		return;
	}
	if (staged_close_step == 6) {
		const auto retire = port<GDExtensionInterfaceDeviceSessionQualificationNativeUnitRetireV1>("device_session_qualification_native_unit_retire_v1");
		if (!retire) { staged_fail(ERR_UNAVAILABLE, "staged_unit_retire_port"); return; }
		if (staged_genuine_prior() && !staged_prior_unit_retire.handle_serial) {
			const GDExtensionNativeUnitRetireRequestV1 prior_origin{ Enrollment::ABI, staged_prior_unit.unit, 1, 0 };
			if (staged_fail(Error(retire(enrollment.qualification, &prior_origin, &staged_prior_unit_retire, &staged_prior_unit_return).native_error), "staged_prior_unit_retire") != OK) { return; }
		}
		const GDExtensionNativeUnitRetireRequestV1 origin{ Enrollment::ABI, staged_unit.unit, 1, 0 };
		if (staged_fail(Error(retire(enrollment.qualification, &origin, &staged_unit_retire, &staged_unit_return).native_error), "staged_original_unit_retire") == OK) { ++staged_close_step; }
		return;
	}
	if (staged_close_step == 7) {
		const auto observe = port<GDExtensionInterfaceDeviceSessionQualificationNativeUnitRetireObserveV1>("device_session_qualification_native_unit_retire_observe_v1");
		const auto release = port<GDExtensionInterfaceDeviceSessionQualificationNativeUnitRetireReleaseV1>("device_session_qualification_native_unit_retire_release_v1");
		if (!observe || !release) { staged_fail(ERR_UNAVAILABLE, "staged_unit_return_ports"); return; }
		const GDExtensionQualificationBudgetV1 units{ Enrollment::ABI, 1, 1000, 0 };
		if (staged_genuine_prior() && !staged_prior_unit_released) {
			if (staged_fail(Error(observe(enrollment.qualification, staged_prior_unit_retire, &units, &staged_prior_unit_return).native_error), "staged_prior_unit_observe") != OK || staged_prior_unit_return.state != 1) { return; }
			if (staged_prior_unit_return.return_status != 1 || staged_prior_unit_return.freed_mask != 3 || staged_prior_unit_return.shader_rid != staged_prior_unit.shader_rid ||
				staged_prior_unit_return.pipeline_rid != staged_prior_unit.pipeline_rid || staged_prior_unit_return.shader_serial != staged_prior_unit.shader_serial ||
				staged_prior_unit_return.pipeline_serial != staged_prior_unit.pipeline_serial || !staged_prior_unit_return.shader_free_event_seq || !staged_prior_unit_return.pipeline_free_event_seq) {
				staged_fail(ERR_INVALID_DATA, "staged_prior_unit_return_identity"); return;
			}
			if (staged_fail(Error(release(enrollment.qualification, staged_prior_unit_retire).native_error), "staged_prior_unit_release") != OK) { return; }
			staged_prior_unit_released = true;
		}
		if (staged_fail(Error(observe(enrollment.qualification, staged_unit_retire, &units, &staged_unit_return).native_error), "staged_original_unit_observe") != OK || staged_unit_return.state != 1) { return; }
		if (staged_unit_return.return_status != 1 || staged_unit_return.freed_mask != 3 || staged_unit_return.shader_rid != staged_unit.shader_rid || staged_unit_return.pipeline_rid != staged_unit.pipeline_rid ||
			staged_unit_return.shader_serial != staged_unit.shader_serial || staged_unit_return.pipeline_serial != staged_unit.pipeline_serial ||
			!staged_unit_return.shader_free_event_seq || !staged_unit_return.pipeline_free_event_seq) {
			staged_fail(ERR_INVALID_DATA, "staged_unit_return_identity"); return;
		}
		if (staged_fail(Error(release(enrollment.qualification, staged_unit_retire).native_error), "staged_original_unit_release") == OK) { ++staged_close_step; }
		return;
	}
	if (staged_close_step == 8) {
		server->free_rid(host_instance);
		RendererSceneCull::NativeParticleSceneFacts stale;
		if (static_cast<RendererSceneCull *>(RSG::scene)->instance_get_committed_particle_scene_facts(host_instance, RID::from_uint64(created.particles), stale) != ERR_INVALID_PARAMETER) {
			staged_fail(ERR_INVALID_DATA, "staged_scene_instance_free"); return;
		}
		host_instance_freed = true;
		host_close_order.push_back("scene_instance_original_free");
		++staged_close_step;
		return;
	}
	const auto close = port<GDExtensionInterfaceDeviceSessionCameraReadyCloseV1>("device_session_camera_ready_close_v1");
	if (!close) { staged_fail(ERR_UNAVAILABLE, "staged_relay_close_port"); return; }
	if (staged_close_step == 9) {
		const Error e = Error(close(enrollment.attachment, staged_relay, &staged_relay_close).native_error);
		staged_relay_first_close = staged_relay_close;
		staged_relay_first_error = e;
		if (e != ERR_BUSY || !staged_relay_close.entry_closed || staged_relay_close.active_deliveries) {
			staged_fail(e == OK || e == ERR_BUSY ? ERR_INVALID_DATA : e, "staged_relay_first_close_identity"); return;
		}
		host_normal_callable = Callable();
		if (host_normal_frees != 1) { staged_fail(ERR_INVALID_DATA, "staged_original_callable_return"); return; }
		host_normal_released = true;
		++staged_close_step;
		return;
	}
	if (staged_fail(Error(close(enrollment.attachment, staged_relay, &staged_relay_close).native_error), "staged_original_relay_return") != OK) { return; }
	if (!staged_relay_close.relay_cell_released || !staged_relay_close.relay_holder_closed || staged_relay_close.active_deliveries) {
		staged_fail(ERR_INVALID_DATA, "staged_relay_return_identity"); return;
	}
	staged_relay_returned = true;
	host_scene_closed = true;
	++staged_close_step;
}

void report_staged(Dictionary &result) const {
	result["staged_enabled"] = staged_enabled;
	result["staged_bank_probe"] = staged_bank_probe();
	result["staged_bank_refused"] = staged_bank_refused;
	if (staged_bank_probe()) {
		result["staged_bank_record_error"] = staged_record.record.result.native_error;
		result["staged_bank_unread_after"] = staged_unread_after;
		result["staged_bank_pending_ack_after"] = int64_t(staged_pending_ack_after);
	}
	result["host_scene_stop_before_acquire"] = false;
	result["staged_step"] = staged_execution_step;
	result["staged_setup_step"] = staged_setup_step;
	result["staged_close_step"] = staged_close_step;
	result["staged_error"] = staged_error;
	result["staged_detail"] = staged_detail;
	result["staged_native_ready"] = staged_native_ready;
	result["staged_record_consumed"] = staged_record_consumed;
	result["staged_record_error"] = staged_record.record.result.native_error;
	Dictionary receipt;
	receipt["request"] = int64_t(staged_record.record.request_serial);
	receipt["control"] = int64_t(staged_record.record.control_serial);
	receipt["native_error"] = staged_record.record.result.native_error;
	receipt["offered_controls"] = staged_record.offered.controls;
	receipt["offered_passes"] = staged_record.offered.passes;
	receipt["offered_receipts"] = staged_record.offered.receipts;
	receipt["remaining_controls"] = staged_record.remaining.controls;
	receipt["remaining_passes"] = staged_record.remaining.passes;
	receipt["remaining_receipts"] = staged_record.remaining.receipts;
	receipt["controls_used"] = staged_record.controls_used;
	receipt["passes_used"] = staged_record.passes_used;
	receipt["receipts_used"] = staged_record.receipts_used;
	result["staged_record_receipt"] = receipt;
	result["staged_unread_after"] = staged_unread_after;
	result["staged_pending_ack_after"] = int64_t(staged_pending_ack_after);
	result["staged_control"] = int64_t(staged_record_control);
	result["staged_compat_calls"] = staged_compat_calls;
	result["staged_program"] = staged_program_snapshot.duplicate(true);
	result["staged_resource_snapshot"] = staged_resource_snapshot.duplicate(true);
	result["staged_reserve_snapshot"] = staged_reserve_snapshot.duplicate(true);
	result["staged_capture"] = staged_capture_snapshot.duplicate(true);
	result["staged_scene_cull"] = staged_scene_cull_snapshot.duplicate(true);
	result["staged_raster"] = staged_raster_snapshot.duplicate(true);
	result["staged_raster_error"] = staged_raster_error;
	result["staged_raster_detail"] = staged_raster_detail;
	result["staged_actions"] = staged_results.duplicate(true);
	result["staged_segments"] = staged_segments.duplicate(true);
	if (!staged_first_segment_ack_busy.is_empty()) {
		result["staged_first_segment_ack_busy"] = staged_first_segment_ack_busy.duplicate(true);
	}
	result["staged_readbacks"] = staged_readback_rows.duplicate(true);
	result["output_acquired"] = staged_batch_count > 1 && staged_acquire.status.error == OK && staged_acquire.writable_count == 3;
	result["records_written"] = staged_readbacks_verified;
	result["finalized"] = staged_batch_count > 3 && int64_t(Dictionary(staged_results[3])["finalize_error"]) == OK;
	result["staged_seal_recorded"] = staged_batch_count > 4 && int64_t(Dictionary(staged_results[4])["seal_error"]) == OK;
	result["renderer_drawn"] = staged_raster_verified;
	Dictionary observed;
	observed["api_error"] = staged_observe_api_result.native_error;
	observed["control_serial"] = int64_t(staged_observe_receipt.control_serial);
	observed["control_state"] = staged_observe_receipt.state;
	observed["control_kind"] = staged_observe_receipt.kind;
	observed["control_result_valid"] = staged_observe_receipt.result_valid;
	observed["control_result_error"] = staged_observe_receipt.control_result.native_error;
	observed["control_result_reason"] = staged_observe_receipt.control_result.reason;
	observed["revision"] = int64_t(staged_observe_report.revision);
	observed["cause"] = staged_observe_report.cause.native_error;
	observed["ready_readbacks"] = staged_observe_report.ready_readbacks;
	observed["known_obligations"] = staged_observe_report.known_obligations;
	observed["possible_obligations"] = staged_observe_report.possible_obligations;
	observed["complete_obligations"] = staged_observe_report.complete_obligations;
	observed["physical_returns_pending"] = staged_observe_report.physical_returns_pending;
	observed["action_count"] = int64_t(staged_observe_report.action_count);
	observed["recorded_action_end"] = int64_t(staged_observe_report.recorded_action_end);
	result["staged_observe_report"] = observed;
	Dictionary cleanup;
	cleanup["cancel_control"] = int64_t(staged_cancel_control);
	cleanup["cancel_frame"] = int64_t(staged_cancel_frame);
	cleanup["cancel_receipt_request"] = int64_t(staged_cancel_receipt.request_serial);
	cleanup["cancel_receipt_control"] = int64_t(staged_cancel_receipt.control_serial);
	cleanup["cancel_receipt_error"] = staged_cancel_receipt.result.native_error;
	cleanup["cancel_status_error"] = staged_cancel_status.error;
	cleanup["control_observe_api_error"] = staged_control_observe_api_result.native_error;
	cleanup["control_observe_serial"] = int64_t(staged_control_observe_receipt.control_serial);
	cleanup["control_observe_state"] = staged_control_observe_receipt.state;
	cleanup["control_observe_result_error"] = staged_control_observe_receipt.control_result.native_error;
	cleanup["output_slot_owner_returned"] = staged_slot_owner_returned;
	cleanup["control_state"] = staged_control_return.state;
	cleanup["control_return_status"] = staged_control_return.return_status;
	cleanup["control_event"] = int64_t(staged_control_return.event_seq);
	cleanup["control_epoch"] = int64_t(staged_control_return.frame_epoch);
	cleanup["control_retire_cell"] = int64_t(staged_control_return.retire_cell_serial);
	cleanup["control_rid"] = int64_t(staged_control_return.rid);
	cleanup["control_backing"] = int64_t(staged_control_return.backing_serial);
	cleanup["control_attempt"] = int64_t(staged_control_return.retire_attempt);
	cleanup["unit_state"] = staged_unit_return.state;
	cleanup["unit_return_status"] = staged_unit_return.return_status;
	cleanup["unit_freed_mask"] = staged_unit_return.freed_mask;
	cleanup["unit_shader_rid"] = int64_t(staged_unit_return.shader_rid);
	cleanup["unit_pipeline_rid"] = int64_t(staged_unit_return.pipeline_rid);
	cleanup["unit_shader_serial"] = int64_t(staged_unit_return.shader_serial);
	cleanup["unit_pipeline_serial"] = int64_t(staged_unit_return.pipeline_serial);
	cleanup["unit_shader_event"] = int64_t(staged_unit_return.shader_free_event_seq);
	cleanup["unit_pipeline_event"] = int64_t(staged_unit_return.pipeline_free_event_seq);
	cleanup["relay_first_error"] = staged_relay_first_error;
	cleanup["relay_first_entry_closed"] = staged_relay_first_close.entry_closed;
	cleanup["relay_first_active"] = staged_relay_first_close.active_deliveries;
	cleanup["relay_holder_closed"] = staged_relay_close.relay_holder_closed;
	cleanup["relay_cell_released"] = staged_relay_close.relay_cell_released;
	cleanup["relay_active"] = staged_relay_close.active_deliveries;
	result["staged_cleanup"] = cleanup;
	const auto dto = [](const auto &value) {
		Dictionary result;
		result["native_error"] = value.native_error;
		result["reason"] = value.reason;
		result["expected"] = int64_t(value.expected);
		result["actual"] = int64_t(value.actual);
		return result;
	};
	result["staged_quote_result"] = dto(staged_quote_result);
	result["staged_reserve_result"] = dto(staged_reserve_result);
	result["staged_quote_graphs"] = staged_quote.capacity.graph_memberships;
	result["staged_quote_native"] = staged_quote.capacity.native_memberships;
	result["staged_quote_uses"] = staged_quote.capacity.resource_uses;
	result["staged_quote_readback_bytes"] = int64_t(staged_quote.capacity.readback_bytes);
	result["staged_quote_payload_bytes"] = int64_t(staged_quote.capacity.retained_payload_bytes);
	result["staged_demand_guard_graphs"] = staged_demand.guard_graphs;
	result["staged_demand_read_graphs"] = staged_demand.read_graphs;
	Array declarations;
	for (const auto &action : staged_actions) {
		Dictionary row;
		row["ordinal"] = int64_t(action.action_ordinal);
		row["kind"] = action.action_kind;
		row["canonical"] = int64_t(action.canonical_suboperation);
		row["substep"] = action.source_recipe_substep;
		declarations.push_back(row);
	}
	result["staged_declarations"] = declarations;
	Array inverse;
	for (float value : host_read.inverse) { inverse.push_back(value); }
	result["staged_host_inverse"] = inverse;
	// JSON floats lose the sign of zero. Preserve the original writer push bits.
	Array affine_words, inverse_words;
	for (uint32_t i = 0; i < 12; ++i) {
		uint32_t affine_word = 0, inverse_word = 0;
		memcpy(&affine_word, &host_read.affine[i], sizeof(affine_word));
		memcpy(&inverse_word, &host_read.inverse[i], sizeof(inverse_word));
		affine_words.push_back(int64_t(affine_word));
		inverse_words.push_back(int64_t(inverse_word));
	}
	result["staged_host_affine_words"] = affine_words;
	result["staged_host_inverse_words"] = inverse_words;
	result["staged_source_imported"] = staged_source_imported;
	result["staged_unit_imported"] = staged_unit_imported;
	result["staged_relay_returned"] = staged_relay_returned;
	result["raw_guard_status_verified"] = staged_readbacks_verified;
	result["records_gpu_readback_verified"] = staged_readbacks_verified;
	Array guard_words, record_words;
	if (staged_readback_rows.size() > 0) { for (uint32_t word : staged_guard_words) { guard_words.push_back(int64_t(word)); } }
	if (staged_readback_rows.size() > 1) { for (uint32_t word : staged_record_words) { record_words.push_back(int64_t(word)); } }
	result["staged_guard_words"] = guard_words;
	result["staged_record_words"] = record_words;
	result["staged_genuine_prior_enabled"] = staged_genuine_prior();
	result["staged_successor_native_ready"] = staged_successor_native_ready;
	result["staged_successor_readbacks_verified"] = staged_successor_readbacks_verified;
	result["staged_successor_raster_error"] = staged_successor_raster_error;
	result["staged_successor_raster_detail"] = staged_successor_raster_detail;
	result["staged_successor_raster_verified"] = staged_successor_raster_verified;
	if (staged_genuine_prior()) {
		Dictionary prior;
		prior["proof_prepared"] = staged_proof_prepared;
		prior["proof_exported"] = staged_proof_exported;
		prior["proof_returned"] = staged_proof_returned;
		prior["proof_borrow_control"] = int64_t(staged_exported_proof.ref.borrow_control_serial);
		prior["proof_slot_index"] = staged_exported_proof.ref.slot_index;
		prior["proof_view_cell_index"] = staged_exported_proof.ref.view_cell_index;
		prior["proof_color_token"] = String::hex_encode_buffer(staged_exported_proof.ref.original_color_token.bytes, 32);
		prior["prior_program"] = staged_prior_program_snapshot.duplicate(true);
		prior["successor_request"] = int64_t(staged_successor_request.handle_serial);
		prior["successor_record_control"] = int64_t(staged_successor_record_control);
		prior["successor_record_consumed"] = staged_successor_record_consumed;
		prior["successor_record_error"] = staged_successor_record.record.result.native_error;
		if (staged_successor_original_request) {
			Registry::Lock lock(enrollment.registry);
			prior["successor_control_state"] = int64_t(uint32_t(staged_successor_original_request->control.state));
			prior["successor_scene_waiting"] = staged_successor_original_request->control.scene_waiting;
			prior["successor_subscriber_last_error"] = int64_t(staged_successor_original_request->actions[1].camera_ready_subscriber.get_last_error());
			prior["successor_scene_cull_error"] = int64_t(staged_successor_original_request->actions[1].scene_cull_error);
			prior["successor_scene_cull_prepared"] = staged_successor_original_request->actions[1].scene_cull_prepared;
		}
		prior["successor_unread_after"] = staged_successor_unread_after;
		prior["successor_pending_ack_after"] = int64_t(staged_successor_pending_ack_after);
		prior["successor_capture"] = staged_successor_capture_snapshot.duplicate(true);
		prior["successor_scene_cull"] = staged_successor_scene_cull_snapshot.duplicate(true);
		prior["successor_raster"] = staged_successor_raster_snapshot.duplicate(true);
		prior["successor_actions"] = staged_successor_results.duplicate(true);
		prior["successor_segments"] = staged_successor_segments.duplicate(true);
		prior["successor_readbacks"] = staged_successor_readback_rows.duplicate(true);
		prior["successor_slot_owner_returned"] = staged_successor_slot_owner_returned;
		prior["successor_reclaimed"] = staged_successor_reclaimed;
		prior["successor_closed"] = staged_successor_closed;
		prior["prior_unit_released"] = staged_prior_unit_released;
		prior["previous_equals_r0_current"] = staged_successor_readbacks_verified && memcmp(staged_record_words, staged_successor_previous_words, sizeof(staged_record_words)) == 0;
		prior["current_distinct_from_previous"] = staged_successor_readbacks_verified && memcmp(staged_successor_current_words, staged_successor_previous_words, sizeof(staged_successor_current_words)) != 0;
		Array s_guard, s_curr, s_prev;
		if (staged_successor_readback_rows.size() > 0) { for (uint32_t w : staged_successor_guard_words) { s_guard.push_back(int64_t(w)); } }
		if (staged_successor_readback_rows.size() > 1) { for (uint32_t w : staged_successor_current_words) { s_curr.push_back(int64_t(w)); } }
		if (staged_successor_readback_rows.size() > 2) { for (uint32_t w : staged_successor_previous_words) { s_prev.push_back(int64_t(w)); } }
		prior["successor_guard_words"] = s_guard;
		prior["successor_current_words"] = s_curr;
		prior["successor_previous_words"] = s_prev;
		result["staged_genuine_prior"] = prior;
	}
}
