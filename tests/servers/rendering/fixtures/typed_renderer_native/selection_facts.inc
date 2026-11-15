// Independent metadata-only request on the existing R6 attachment.
enum SelectionFactsPhase {
	SELECTION_FACTS_RESERVE,
	SELECTION_FACTS_SCHEDULE,
	SELECTION_FACTS_CONSUME,
	SELECTION_FACTS_SEAL,
	SELECTION_FACTS_RELEASE,
	SELECTION_FACTS_DONE,
};
SelectionFactsPhase selection_facts_phase = SELECTION_FACTS_RESERVE;
GDExtensionProductionRequestRefV1 selection_facts_request{};
GDExtensionOutputMetadataSelectionRequirementsV2 selection_facts_requirements{};
GDExtensionOutputMetadataSelectionResultV2 selection_facts_prepared{};
GDExtensionProductionRecordReportV2 selection_facts_record{};
GDExtensionOutputDeviceFactsV2 selection_facts_device{};
RendererRD::MeshStorage::NativeSelectionSurfaceObservation selection_facts_mesh{};
RendererRD::MaterialStorage::NativeSelectionShaderObservation selection_facts_material{};
GDExtensionOutputSelectionFactsV2 selection_facts_rows[2]{};
Error selection_facts_call_errors[2] = { ERR_UNCONFIGURED, ERR_UNCONFIGURED };
bool selection_facts_match[2]{};
Error selection_facts_error = OK;
Error selection_facts_seal_error = ERR_UNCONFIGURED;
Error selection_facts_reclaim_lookup = ERR_UNCONFIGURED;
uint64_t selection_facts_control = 0, selection_facts_frame = 0;
uint64_t selection_facts_attachment = 0;
uint32_t selection_facts_calls = 0;
uint32_t selection_facts_mask(uint32_t index) const {
	return index == 0 ? staged_selected_pass_mask() : 0;
}

bool selection_facts_matches(const GDExtensionOutputSelectionFactsV2 &facts, uint32_t requested) const {
	const auto &reference = selection_facts_prepared.selection;
	const auto &observed = facts.observation;
	const auto &native = facts.device;
	if (facts.abi.abi_version != 2 || facts.abi.struct_size != sizeof(facts) || facts.abi.reserved ||
			facts.selection.request.registry_instance != reference.request.registry_instance ||
			facts.selection.request.handle_serial != reference.request.handle_serial ||
			facts.selection.resource_slot != reference.resource_slot || facts.selection.resource_generation != reference.resource_generation ||
			facts.selection.reserved || facts.profile != 1 || facts.surface != 0 ||
			native.abi.abi_version != 2 || native.abi.struct_size != sizeof(native) || native.abi.reserved ||
			native.registry_instance != selection_facts_device.registry_instance || native.device_epoch != selection_facts_device.device_epoch ||
			native.session != selection_facts_device.session || std::memcmp(native.device_tag.bytes, selection_facts_device.device_tag.bytes, sizeof(native.device_tag.bytes)) ||
			observed.abi.abi_version != 2 || observed.abi.struct_size != sizeof(observed) || observed.abi.reserved ||
			observed.mesh != mesh.get_id() || observed.mesh_revision != selection_facts_mesh.mesh_revision ||
			observed.native_surface != selection_facts_mesh.native_surface || observed.vertex_count != selection_facts_mesh.vertex_count ||
			observed.index_count != selection_facts_mesh.index_count || observed.primitive != selection_facts_mesh.primitive ||
			observed.format != selection_facts_mesh.format || observed.material != material.get_id() ||
			observed.material_generation != selection_facts_material.material_generation ||
			observed.shader_version != selection_facts_material.shader_version.get_id() || observed.shader_observation_error != OK ||
			observed.needs_inverse != selection_facts_material.needs_inverse || observed.zero_displacement != selection_facts_material.zero_displacement ||
			observed.material_recipe_rejections != selection_facts_material.material_recipe_rejections ||
			facts.pass_status.native_error != OK || facts.eligible_pass_mask != selection_facts_material.output_pass_mask ||
			facts.selected_pass_mask != requested || (facts.eligible_pass_mask & requested) != requested) {
		return false;
	}
	for (uint32_t axis = 0; axis < 3; ++axis) {
		if (observed.mesh_bounds.position[axis] != selection_facts_mesh.bounds.position[axis] ||
				observed.mesh_bounds.size[axis] != selection_facts_mesh.bounds.size[axis] ||
				facts.geometry_bounds.position[axis] != 0 || facts.geometry_bounds.size[axis] != 0) {
			return false;
		}
	}
	return true;
}

void receive_selection_facts(GDExtensionDeviceExit::ProductionPhysicalContextRef context, GDExtensionProductionRequestRefV1 selected_request) {
	if (++selection_facts_calls != 1 || selected_request.registry_instance != selection_facts_request.registry_instance) {
		selection_facts_error = ERR_INVALID_DATA;
		return;
	}
	selection_facts_frame = context.frame_serial;
	const auto prepare = port<GDExtensionInterfaceDeviceSessionOutputMetadataSelectionPrepareV2>("device_session_output_metadata_selection_prepare_v2");
	const auto read = port<GDExtensionInterfaceDeviceSessionOutputSelectionFactsV2>("device_session_output_selection_facts_v2");
	if (!prepare || !read) {
		selection_facts_error = ERR_UNAVAILABLE;
		return;
	}
	const GDExtensionOutputMetadataSelectionPrepareV2 input{ { 2, sizeof(input), 0 }, 0, 0, { meshes, 1, 0 }, { materials, 1, 0 } };
	selection_facts_error = Error(prepare(context, selected_request, &input, &selection_facts_prepared).native_error);
	if (selection_facts_error != OK) {
		return; // The original partial allocation, if present, is still released.
	}
	const auto &reference = selection_facts_prepared.selection;
	if (!selection_facts_frame || selection_facts_prepared.status.native_error != OK ||
			selection_facts_prepared.retained_meshes != 1 || selection_facts_prepared.retained_materials != 1 ||
			reference.request.registry_instance != selected_request.registry_instance ||
			reference.request.handle_serial != selected_request.handle_serial || reference.resource_slot != 0 ||
			reference.reserved || reference.resource_generation != 1) {
		selection_facts_error = ERR_INVALID_DATA;
		return;
	}
	for (uint32_t index = 0; index < 2; ++index) {
		const GDExtensionOutputSelectionFactsInputV2 query{ { 2, sizeof(query), 0 }, 1, 0, selection_facts_mask(index), 0 };
		auto &facts = selection_facts_rows[index];
		const Error result = Error(read(enrollment.attachment, reference, &query, &facts).native_error);
		selection_facts_call_errors[index] = result;
		selection_facts_match[index] = result == OK && selection_facts_matches(facts, selection_facts_mask(index));
		if (!selection_facts_match[index] && selection_facts_error == OK) {
			selection_facts_error = result == OK ? ERR_INVALID_DATA : result;
		}
	}
}

void advance_selection_facts(const GDExtensionProductionServiceBudgetV1 &budget) {
	using namespace GDExtensionDeviceExit;
	switch (selection_facts_phase) {
		case SELECTION_FACTS_RESERVE: {
			const auto query = port<GDExtensionInterfaceDeviceSessionOutputMetadataSelectionQueryV2>("device_session_output_metadata_selection_query_v2");
			const auto reserve = port<GDExtensionInterfaceDeviceSessionProductionReserveV2>("device_session_production_reserve_v2");
			if (!query || !reserve) {
				host_fail(ERR_UNAVAILABLE, "selection_facts_reserve_ports");
				return;
			}
			const GDExtensionOutputMetadataSelectionQueryV2 input{ { 2, sizeof(input), 0 }, 1, 1 };
			if (!host_result(Error(query(&input, &selection_facts_requirements).native_error), "selection_facts_original_quote")) {
				return;
			}
			const auto &requirements = selection_facts_requirements;
			auto operation = enrollment.request->operation;
			if (requirements.abi.abi_version != 2 || requirements.abi.struct_size != sizeof(requirements) ||
					requirements.abi.reserved || !requirements.physical_bytes || !requirements.alignment ||
					requirements.mesh_count != 1 || requirements.material_count != 1 || requirements.reserved ||
					operation.owner_request_nonce > UINT64_MAX - 3 ||
					create_input.config.pass_mask != selection_facts_mask(0)) {
				host_fail(ERR_INVALID_DATA, "selection_facts_original_quote_identity");
				return;
			}
			operation.owner_request_nonce += 3;
			const GDExtensionProductionCapacityV1 capacity{ production_dto_abi<GDExtensionProductionCapacityV1>(), 1, 0, 1, 1, 0, 0, 0, 1, 0, 0 };
			const GDExtensionProductionResourceV1 resource{ GDEXTENSION_PRODUCTION_OUTPUT_METADATA_V1, 0, 0,
				GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1, 1, requirements.physical_bytes, 0 };
			if (host_result(Error(reserve(enrollment.attachment, &operation, &capacity, nullptr, 0, &resource, 1, nullptr, 0, nullptr, 0,
									&selection_facts_request).native_error), "selection_facts_original_reserve")) {
				selection_facts_attachment = enrollment.attachment.handle_serial;
				selection_facts_phase = SELECTION_FACTS_SCHEDULE;
			}
			return;
		}
		case SELECTION_FACTS_SCHEDULE: {
			const auto schedule = port<GDExtensionInterfaceDeviceSessionProductionRecordScheduleV1>("device_session_production_record_schedule_v1");
			if (host_result(schedule ? Error(schedule(selection_facts_request, &selection_facts_control)) : ERR_UNAVAILABLE,
						"selection_facts_original_schedule")) {
				selection_facts_phase = SELECTION_FACTS_CONSUME;
			}
			return;
		}
		case SELECTION_FACTS_CONSUME: {
			const auto consume = port<GDExtensionInterfaceDeviceSessionProductionRecordConsumeV2>("device_session_production_record_consume_v2");
			if (!host_result(consume ? Error(consume(selection_facts_request, selection_facts_control, &selection_facts_record).native_error) : ERR_UNAVAILABLE,
						"selection_facts_original_consume")) {
				return;
			}
			const auto &record = selection_facts_record;
			const auto &report = record.report;
			if (selection_facts_error == OK && (record.request_serial != selection_facts_request.handle_serial ||
					record.control_serial != selection_facts_control || record.result.native_error != OK || selection_facts_calls != 1 ||
					(record.flags & (GDEXTENSION_PRODUCTION_RECORD_HAS_APPEND_V1 | GDEXTENSION_PRODUCTION_RECORD_HAS_SEGMENT_V1)) ||
					report.action_count || report.recorded_action_end || report.fact_count || report.known_obligations ||
					report.possible_obligations || report.physical_returns_pending != 1)) {
				selection_facts_error = ERR_INVALID_DATA;
			}
			selection_facts_phase = SELECTION_FACTS_SEAL;
			return;
		}
		case SELECTION_FACTS_SEAL: {
			if (selection_facts_prepared.selection.request.handle_serial) {
				const auto seal = port<GDExtensionInterfaceDeviceSessionOutputMetadataSelectionReturnV2>("device_session_output_metadata_selection_return_v2");
				selection_facts_seal_error = seal ? Error(seal(enrollment.attachment, selection_facts_prepared.selection).native_error) : ERR_UNAVAILABLE;
				// This closes reads. The live slab remains owed until original RELEASE.
				if (selection_facts_error == OK && selection_facts_seal_error != ERR_BUSY) {
					selection_facts_error = ERR_INVALID_DATA;
				}
			}
			selection_facts_phase = SELECTION_FACTS_RELEASE;
			return;
		}
		case SELECTION_FACTS_RELEASE: {
			const auto release = port<GDExtensionInterfaceDeviceSessionProductionReleaseV2>("device_session_production_release_v2");
			GDExtensionProductionServiceBudgetV1 remaining{};
			GDExtensionProductionReportV2 report{};
			if (!host_result(release ? Error(release(selection_facts_request, &budget, &remaining, &report).native_error) : ERR_UNAVAILABLE,
						"selection_facts_original_release")) {
				return;
			}
			Request *live = nullptr;
			RenderingDevice *rd = nullptr;
			{
				Registry::Lock lock(enrollment.registry);
				selection_facts_reclaim_lookup = enrollment.registry.production_request_locked(selection_facts_request, false, live, rd);
			}
			if (report.service_pending || selection_facts_reclaim_lookup != ERR_DOES_NOT_EXIST || live) {
				host_fail(ERR_INVALID_DATA, "selection_facts_original_reclaim");
				return;
			}
			selection_facts_phase = SELECTION_FACTS_DONE;
			if (selection_facts_error != OK) {
				host_fail(selection_facts_error == ERR_BUSY ? ERR_INVALID_DATA : selection_facts_error, "selection_facts_original_observation");
			}
			return;
		}
		case SELECTION_FACTS_DONE:
			return;
	}
}

Dictionary selection_facts_report() const {
	Dictionary result;
	result["attachment"] = int64_t(selection_facts_attachment);
	result["request"] = int64_t(selection_facts_request.handle_serial);
	result["source_request"] = int64_t(request.handle_serial);
	result["control"] = int64_t(selection_facts_control);
	result["frame"] = int64_t(selection_facts_frame);
	result["receiver_calls"] = selection_facts_calls;
	result["metadata_bytes"] = int64_t(selection_facts_requirements.physical_bytes);
	result["prepare_error"] = selection_facts_prepared.status.native_error;
	result["retained_meshes"] = selection_facts_prepared.retained_meshes;
	result["retained_materials"] = selection_facts_prepared.retained_materials;
	result["selection_slot"] = selection_facts_prepared.selection.resource_slot;
	result["selection_generation"] = int64_t(selection_facts_prepared.selection.resource_generation);
	result["record_error"] = selection_facts_record.result.native_error;
	result["record_request"] = int64_t(selection_facts_record.request_serial);
	result["record_control"] = int64_t(selection_facts_record.control_serial);
	result["record_graph_facts"] = selection_facts_record.report.fact_count;
	result["record_actions"] = int64_t(selection_facts_record.report.action_count);
	result["record_native_obligations"] = selection_facts_record.report.known_obligations;
	result["record_physical_returns_pending"] = selection_facts_record.report.physical_returns_pending;
	result["seal_error"] = selection_facts_seal_error;
	result["reclaim_lookup"] = selection_facts_reclaim_lookup;
	result["reclaimed"] = selection_facts_phase == SELECTION_FACTS_DONE;
	result["error"] = selection_facts_error;
	result["device_tag"] = String::hex_encode_buffer(selection_facts_device.device_tag.bytes, sizeof(selection_facts_device.device_tag.bytes));
	Array observations;
	for (uint32_t index = 0; index < 2; ++index) {
		const auto &facts = selection_facts_rows[index];
		const auto &observed = facts.observation;
		Dictionary row;
		row["api_error"] = selection_facts_call_errors[index];
		row["matches_original"] = selection_facts_match[index];
		row["profile"] = facts.profile;
		row["surface"] = facts.surface;
		row["requested"] = selection_facts_mask(index);
		row["eligible"] = facts.eligible_pass_mask;
		row["selected"] = facts.selected_pass_mask;
		row["pass_error"] = facts.pass_status.native_error;
		row["registry"] = int64_t(facts.device.registry_instance);
		row["device_epoch"] = int64_t(facts.device.device_epoch);
		row["session"] = int64_t(facts.device.session);
		row["device_tag"] = String::hex_encode_buffer(facts.device.device_tag.bytes, sizeof(facts.device.device_tag.bytes));
		row["mesh"] = int64_t(observed.mesh);
		row["mesh_revision"] = int64_t(observed.mesh_revision);
		row["native_surface"] = observed.native_surface;
		row["vertex_count"] = observed.vertex_count;
		row["index_count"] = observed.index_count;
		row["primitive"] = observed.primitive;
		row["format"] = int64_t(observed.format);
		Array mesh_position, mesh_size, geometry_position, geometry_size;
		for (uint32_t axis = 0; axis < 3; ++axis) {
			mesh_position.push_back(observed.mesh_bounds.position[axis]);
			mesh_size.push_back(observed.mesh_bounds.size[axis]);
			geometry_position.push_back(facts.geometry_bounds.position[axis]);
			geometry_size.push_back(facts.geometry_bounds.size[axis]);
		}
		row["mesh_position"] = mesh_position;
		row["mesh_size"] = mesh_size;
		row["geometry_position"] = geometry_position;
		row["geometry_size"] = geometry_size;
		row["material"] = int64_t(observed.material);
		row["material_generation"] = int64_t(observed.material_generation);
		row["shader_version"] = int64_t(observed.shader_version);
		row["shader_error"] = observed.shader_observation_error;
		row["material_recipe_rejections"] = observed.material_recipe_rejections;
		observations.push_back(row);
	}
	result["observations"] = observations;
	return result;
}
