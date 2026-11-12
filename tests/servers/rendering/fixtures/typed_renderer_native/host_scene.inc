// Included inside TypedRendererProducer. One original enrollment/attachment only.
// Physical Host/scene preparation, stopping before output plan or Acquire.
bool host_scene_enabled = false, host_scene_prepared = false, host_scene_closed = false, host_scene_advancing = false;
bool host_request_reclaimed = false, host_instance_freed = false, host_normal_released = false;
bool host_scene_validation_performed = false, host_scene_cull_validation_deferred = false;
uint32_t host_scene_step = 0, host_normal_calls = 0, host_normal_frees = 0;
uint64_t host_scene_deadline = 0, host_node_object_id = 0;
Error host_scene_error = OK;
Error host_scene_fact_error = ERR_UNCONFIGURED;
String host_scene_detail;
RID host_viewport, host_camera, host_scenario, host_instance;
Transform3D host_global_transform;
Callable host_normal_callable;
GDExtensionProductionRequestRefV1 host_request{};
Request *host_original_request = nullptr;
GDExtensionProductionCapacityV1 host_capacity{};
GDExtensionProductionReportV2 host_release_report{};
GDExtensionProductionHostTransformInputV1 host_input{};
GDExtensionProductionOutputSceneInputV1 host_scene_input{};
GDExtensionProductionHostTransformV1 host_read{};
GDExtensionDeviceSessionResultV1 host_prepare_result{ ERR_UNCONFIGURED, 0, 0, 0 };
GDExtensionDeviceSessionResultV1 host_scene_result{ ERR_UNCONFIGURED, 0, 0, 0 };
GDExtensionDeviceSessionResultV1 host_view_result{ ERR_UNCONFIGURED, 0, 0, 0 };
GDExtensionDeviceSessionResultV1 host_read_result{ ERR_UNCONFIGURED, 0, 0, 0 };
Dictionary host_normal_snapshot, host_request_snapshot, host_committed_snapshot, host_reserve_snapshot, host_prepared_snapshot;
Array host_close_order;
GDExtensionDeviceSessionResultV1 host_reserve_result{ ERR_UNCONFIGURED, 0, 0, 0 };
GDExtensionDeviceSessionResultV1 host_import_result{ ERR_UNCONFIGURED, 0, 0, 0 };
Error host_scene_validate_error = ERR_UNCONFIGURED;

Error host_fail(Error value, const char *where) {
	if (value != OK && value != ERR_BUSY && host_scene_error == OK) { host_scene_error = value; host_scene_detail = where; }
	return value;
}
bool host_result(Error value, const char *where) { return host_fail(value, where) == OK; }
static void host_callable_free(void *opaque) { auto &self = *static_cast<TypedRendererProducer *>(opaque); ++self.host_normal_frees; self.host_close_order.push_back("normal_callable_free_callback"); }
static void host_callable_call(void *opaque, const GDExtensionConstVariantPtr *, GDExtensionInt,
		GDExtensionVariantPtr returned, GDExtensionCallError *call_error) {
	auto &self = *static_cast<TypedRendererProducer *>(opaque);
	if (self.staged_enabled && (self.staged_batch_item.flags & 2u)) {
		self.staged_ack_error = self.staged_ack_in_normal();
	} else {
		self.prepare_host_normal();
	}
	*reinterpret_cast<Variant *>(returned) = Variant();
	*call_error = { GDEXTENSION_CALL_OK, 0, 0 };
}
Error adopt_host_callable() {
	using namespace GDExtensionDeviceExit;
	const auto *entry = RegistrationContext::current;
	if (!entry || entry != &enrollment.frame || !entry->admitted || !entry->registration_allowed ||
			entry->level_phase != LevelPhase::SCENE_READY || entry->reference != &enrollment.reference) { return ERR_UNAUTHORIZED; }
	// Same existing ComponentScope Callable entry as HostViewGpuHarness; this
	// does not enroll a Runtime owner or create a second Registry/attachment.
	{ Registry::Lock lock(enrollment.registry); enrollment.registry.records[enrollment.reference.slot].guard.entry_classes |= 1u << 8; }
	GDExtensionCallableCustomInfo2 info{};
	info.callable_userdata = this; info.call_func = &host_callable_call; info.free_func = &host_callable_free;
	info.token = enrollment.registry.records[enrollment.reference.slot].original_owner;
	const auto create = port<GDExtensionInterfaceCallableCustomCreate2>("callable_custom_create2");
	if (!create) { return ERR_UNAVAILABLE; }
	create(reinterpret_cast<GDExtensionUninitializedTypePtr>(&host_normal_callable), &info);
	return host_normal_callable.is_custom() ? OK : ERR_UNAVAILABLE;
}
void prepare_host_normal() {
	using namespace GDExtensionDeviceExit;
	++host_normal_calls;
	const auto *entry = RegistrationContext::current;
	const bool same = entry && entry->admitted && entry->reference && entry->reference->registry == &enrollment.registry &&
		entry->reference->slot == enrollment.reference.slot && entry->level_phase == LevelPhase::NONE && entry->frame_id;
	host_normal_snapshot["admitted"] = entry && entry->admitted;
	host_normal_snapshot["level_none"] = entry && entry->level_phase == LevelPhase::NONE;
	host_normal_snapshot["same_record"] = same;
	host_normal_snapshot["frame"] = entry ? int64_t(entry->frame_id) : int64_t(0);
	host_normal_snapshot["record_slot"] = entry && entry->reference ? int64_t(entry->reference->slot) : int64_t(-1);
	host_normal_snapshot["expected_record_slot"] = int64_t(enrollment.reference.slot);
	host_normal_snapshot["registry_instance"] = int64_t(enrollment.instance);
	host_normal_snapshot["caller_thread"] = int64_t(Thread::get_caller_id());
	host_normal_snapshot["owner_thread"] = int64_t(enrollment.registry.records[enrollment.reference.slot].owner_thread);
	host_normal_snapshot["physical_record_frame"] = Registry::production_physical_frame != nullptr;
	if (!same || Registry::production_physical_frame || !host_original_request || host_original_request->frozen || host_original_request->append_effect_entered) {
		host_fail(ERR_UNAUTHORIZED, "host_original_normal_entry"); return;
	}
	const auto prepare = port<GDExtensionInterfaceDeviceSessionProductionPrepareHostTransformV1>("device_session_production_prepare_host_transform_v1");
	const auto scene = port<GDExtensionInterfaceDeviceSessionProductionPrepareOutputSceneV1>("device_session_production_prepare_output_scene_v1");
	const auto view = port<GDExtensionInterfaceDeviceSessionProductionPrepareHostViewV1>("device_session_production_prepare_host_view_v1");
	const auto read = port<GDExtensionInterfaceDeviceSessionProductionReadHostTransformV1>("device_session_production_read_host_transform_v1");
	if (!prepare || !scene || !view || !read) { host_fail(ERR_UNAVAILABLE, "host_original_ports"); return; }
	const uint64_t ordinal = staged_enabled ? 1 : 0;
	host_prepare_result = prepare(host_request, ordinal, &host_input);
	if (!host_result(Error(host_prepare_result.native_error), "host_prepare_transform")) { return; }
	host_scene_result = scene(host_request, ordinal, &host_scene_input);
	if (!host_result(Error(host_scene_result.native_error), "host_prepare_scene")) { return; }
	const GDExtensionProductionHostViewInputV1 view_input{ production_dto_abi<GDExtensionProductionHostViewInputV1>(), host_viewport.get_id(), host_camera.get_id() };
	host_view_result = view(host_request, ordinal, &view_input);
	if (!host_result(Error(host_view_result.native_error), "host_prepare_view")) { return; }
	host_read_result = read(host_request, ordinal, &host_read);
	if (!host_result(Error(host_read_result.native_error), "host_read_transform")) { return; }
	if (memcmp(host_read.affine, host_input.affine, sizeof(host_input.affine)) || memcmp(&host_read.host_token, &host_input.host_token, sizeof(host_input.host_token))) {
		host_fail(ERR_INVALID_DATA, "host_read_exact_transform");
	}
}
void advance_host_scene(bool close_requested) {
	using namespace GDExtensionDeviceExit;
	if (host_scene_error != OK || host_scene_closed || host_scene_advancing) { return; }
	host_scene_advancing = true;
	struct ReturnHostAdvance { bool &active; ~ReturnHostAdvance() { active = false; } } return_advance{ host_scene_advancing };
	if (!host_scene_deadline) { host_scene_deadline = OS::get_singleton()->get_ticks_usec() + (staged_enabled ? 60000000 : 20000000); }
	if (!server || !server->is_on_render_thread() || RenderingDevice::get_singleton() != device ||
		device->draw_list.active || device->compute_list.active || device->raytracing_list.active) { host_fail(ERR_UNAUTHORIZED, "host_original_rt"); return; }
	if (OS::get_singleton()->get_ticks_usec() >= host_scene_deadline) { host_fail(ERR_TIMEOUT, "host_deadline_custody_retained"); return; }
	const GDExtensionProductionServiceBudgetV1 budget{ production_dto_abi<GDExtensionProductionServiceBudgetV1>(), host_scene_deadline, 1, 8, 8, 0 };
	if (host_scene_step == 0) {
		const auto cancel = port<GDExtensionInterfaceDeviceSessionProductionCancelV1>("device_session_production_cancel_v1");
		if (!cancel) { host_fail(ERR_UNAVAILABLE, "host_reader_cancel_port"); return; }
		if (host_result(Error(cancel(source_reader).native_error), "host_reader_cancel")) { ++host_scene_step; }
		return;
	}
	if (host_scene_step == 1) {
		const auto release = port<GDExtensionInterfaceDeviceSessionProductionReleaseV2>("device_session_production_release_v2");
		if (!release) { host_fail(ERR_UNAVAILABLE, "host_reader_release_port"); return; }
		GDExtensionProductionServiceBudgetV1 remaining{};
		if (!host_result(Error(release(source_reader, &budget, &remaining, &source_reader_release).native_error), "host_reader_release")) { return; }
		if (source_reader_release.service_pending || !source_reader_request || source_reader_request->serial) { host_fail(ERR_INVALID_DATA, "host_reader_not_reclaimed"); return; }
		source_reader_request = nullptr; source_reader_reclaimed = true; host_close_order.push_back("reader_original_request_reclaimed"); ++host_scene_step; return;
	}
	if (staged_enabled && staged_error != OK) { return; }
	if (staged_enabled && host_scene_step == 2) {
		if (selection_facts_phase != SELECTION_FACTS_DONE) { advance_selection_facts(budget); return; }
		advance_staged_reservation();
		return;
	}
	if (host_scene_step == 2) {
		const auto reserve = port<GDExtensionInterfaceDeviceSessionProductionReserveV2>("device_session_production_reserve_v2");
		if (!reserve) { host_fail(ERR_UNAVAILABLE, "host_reserve_port"); return; }
		const GDExtensionProductionActionV2 action{ 0, 11, 10, 0, GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1, UINT32_MAX, 0, 1, GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1, UINT32_MAX };
		const GDExtensionProductionResourceV1 resource{ GDEXTENSION_PRODUCTION_STORAGE_V1, source_origin.allocation_slot, source_origin.part,
			GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, source_origin.allocation_generation, source_origin.bytes, 0 };
		const GDExtensionProductionUseV1 use{ 0, GDEXTENSION_PRODUCTION_READ_V1, 0, sizeof(source_words), 0 };
		host_capacity = { production_dto_abi<GDExtensionProductionCapacityV1>(), 1, 1, 1, 1, 1, 0, 0, 1, 0, 0 };
		{ Registry::Lock lock(enrollment.registry); MutexLock rd_lock(device->_thread_safe_);
			const Error bounded = enrollment.registry.production_membership_bound_locked(*device, 1, device->transfer_worker_pool_max_size, host_capacity.native_memberships);
			if (!host_result(bounded, "host_native_capacity")) { return; }
		}
		auto operation = enrollment.request->operation;
		if (operation.owner_request_nonce > UINT64_MAX - 2) { host_fail(ERR_PARAMETER_RANGE_ERROR, "host_nonce_exhausted"); return; }
		operation.owner_request_nonce += 2;
		host_reserve_snapshot = snapshot_source_reader_reserve(&host_capacity, true);
		host_reserve_result = reserve(enrollment.attachment, &operation, &host_capacity, &action, 1, &resource, 1, &use, 1, nullptr, 0, &host_request);
		if (!host_result(Error(host_reserve_result.native_error), "host_original_reserve")) { return; }
		RenderingDevice *actual = nullptr;
		{ Registry::Lock lock(enrollment.registry);
			const Error found = enrollment.registry.production_request_locked(host_request, false, host_original_request, actual);
			if (!host_result(found, "host_original_request")) { return; }
			if (!host_original_request || actual != device || host_original_request == enrollment.request || host_original_request->attachment != enrollment.request->attachment) { host_fail(ERR_INVALID_DATA, "host_request_same_attachment"); return; }
		}
		++host_scene_step; return;
	}
	if (host_scene_step == 3) {
		const auto import = port<GDExtensionInterfaceDeviceSessionProductionImportStorageV1>("device_session_production_import_storage_v1");
		if (!import) { host_fail(ERR_UNAVAILABLE, "host_import_port"); return; }
		host_import_result = import(host_request, 0, &source);
		if (!host_result(Error(host_import_result.native_error), "host_original_source_import")) { return; }
		{ Registry::Lock lock(enrollment.registry);
			const auto &resource = host_original_request->resources[0];
			const auto &action = host_original_request->actions[0];
			const bool exact = !host_original_request->frozen && !host_original_request->append_effect_entered && !host_original_request->next_action &&
				!host_original_request->graph_count && !host_original_request->native_count && resource.imported && resource.use_count == 1 &&
				resource.primary.get_id() == source.rid && resource.backing_serial == source.backing_serial && action.declaration.action_kind == GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1;
			host_request_snapshot["serial"] = int64_t(host_original_request->serial);
			host_request_snapshot["attachment"] = int64_t(host_original_request->attachment->serial);
			host_request_snapshot["producer_attachment"] = int64_t(enrollment.request->attachment->serial);
			host_request_snapshot["source_rid"] = int64_t(resource.primary.get_id());
			host_request_snapshot["source_backing_serial"] = int64_t(resource.backing_serial);
			host_request_snapshot["source_imported"] = resource.imported;
			host_request_snapshot["source_use_count"] = resource.use_count;
			host_request_snapshot["frozen"] = host_original_request->frozen;
			host_request_snapshot["append_effect_entered"] = host_original_request->append_effect_entered;
			host_request_snapshot["action_kind"] = action.declaration.action_kind;
			host_request_snapshot["resource_count"] = host_original_request->resource_count; host_request_snapshot["readback_count"] = host_original_request->readback_count;
			host_request_snapshot["next_action"] = host_original_request->next_action;
			host_request_snapshot["graph_count"] = host_original_request->graph_count;
			host_request_snapshot["native_count"] = host_original_request->native_count;
			if (!exact) { host_fail(ERR_INVALID_DATA, "host_source_identity"); return; }
		}
		++host_scene_step; return;
	}
	if (host_scene_step == 4) {
		auto *scene = static_cast<RendererSceneCull *>(RSG::scene);
		const auto *viewport = RSG::viewport ? RSG::viewport->viewport_owner.get_or_null(host_viewport) : nullptr;
		if (!scene || !viewport || viewport->camera != host_camera || viewport->scenario != host_scenario || viewport->render_buffers.is_null() ||
			!Object::cast_to<RenderSceneBuffersRD>(viewport->render_buffers.ptr()) || !scene->is_camera(host_camera) || !host_node_object_id || !host_global_transform.is_finite()) {
			host_fail(ERR_INVALID_DATA, "host_actual_viewport_camera_scenario"); return;
		}
		host_input.abi = production_dto_abi<GDExtensionProductionHostTransformInputV1>();
		for (uint32_t row = 0; row < 3; ++row) {
			for (uint32_t column = 0; column < 3; ++column) { host_input.affine[row * 4 + column] = float(host_global_transform.basis[row][column]); }
			host_input.affine[row * 4 + 3] = float(host_global_transform.origin[row]);
		}
		for (uint32_t i = 0; i < 32; ++i) { host_input.host_token.bytes[i] = uint8_t(source.backing.handle_serial + i + 1); } // Correlation only.
		host_instance = server->instance_create2(RID::from_uint64(created.particles), host_scenario);
		if (host_instance.is_null()) { host_fail(ERR_CANT_CREATE, "host_actual_instance_create"); return; }
		server->instance_set_transform(host_instance, host_global_transform);
		host_scene_input = { production_dto_abi<GDExtensionProductionOutputSceneInputV1>(), host_instance.get_id(), created.particles, created.generation, host_scenario.get_id(), host_input };
		++host_scene_step; return;
	}
	if (host_scene_step == 5) {
		Variant returned; Callable::CallError call_error;
		host_normal_callable.callp(nullptr, 0, returned, call_error); // Actual original adoption owns the transient NORMAL entry.
		if (call_error.error != Callable::CallError::CALL_OK) { host_fail(ERR_UNAUTHORIZED, "host_callable_admission"); return; }
		if (host_scene_error != OK || host_prepare_result.native_error == ERR_BUSY || host_scene_result.native_error == ERR_BUSY || host_view_result.native_error == ERR_BUSY || host_read_result.native_error == ERR_BUSY) { return; }
		auto *scene = static_cast<RendererSceneCull *>(RSG::scene);
		scene->update_dirty_instances(); // Explicit ordinary owner commit; getter never manufactures it.
		RendererSceneCull::NativeParticleSceneFacts facts;
		const Error observed = scene->instance_get_committed_particle_scene_facts(host_instance, RID::from_uint64(created.particles), facts);
		host_scene_fact_error = observed;
		if (!host_result(observed, "host_original_scene_commit")) { return; }
		if (staged_enabled) {
			// This is original Scene/Host preparation, before stage and camera.
			// The selected pre-cull transaction must establish cull coverage;
			// original Acquire/Finalize/Seal validate it inside that real window.
			host_scene_cull_validation_deferred = true;
		} else {
			GDExtensionOutputAcquireInputV1 witness{}; // Original nonstaged fact-validation path.
			witness.abi = production_dto_abi<GDExtensionOutputAcquireInputV1>();
			witness.particles = created.particles; witness.common.generation = created.generation; witness.common.current_host_token = host_input.host_token;
			host_scene_validation_performed = true;
			{ Registry::Lock lock(enrollment.registry); host_scene_validate_error = enrollment.registry.production_output_scene_validate_locked(host_original_request->actions[0], witness); }
			if (!host_result(host_scene_validate_error, "host_original_committed_validation")) { return; }
		}
		if (facts.instance != host_instance || facts.particles.get_id() != created.particles || facts.scenario != host_scenario || facts.transform != host_global_transform || !facts.local_aabb.has_volume() || !facts.world_aabb.has_volume()) { host_fail(ERR_INVALID_DATA, "host_committed_tuple"); return; }
		host_committed_snapshot["instance"] = int64_t(facts.instance.get_id()); host_committed_snapshot["particles"] = int64_t(facts.particles.get_id());
		host_committed_snapshot["scenario"] = int64_t(facts.scenario.get_id()); host_committed_snapshot["version"] = int64_t(facts.version);
		PackedFloat32Array affine;
		for (uint32_t row = 0; row < 3; ++row) { for (uint32_t column = 0; column < 3; ++column) { affine.push_back(float(facts.transform.basis[row][column])); } affine.push_back(float(facts.transform.origin[row])); }
		host_committed_snapshot["affine"] = affine;
		host_committed_snapshot["local_has_volume"] = facts.local_aabb.has_volume(); host_committed_snapshot["world_has_volume"] = facts.world_aabb.has_volume();
		{ Registry::Lock lock(enrollment.registry);
			const auto &action = host_original_request->actions[staged_enabled ? 1 : 0];
			host_prepared_snapshot["request"] = int64_t(host_original_request->serial); host_prepared_snapshot["attachment"] = int64_t(host_original_request->attachment->serial);
			host_prepared_snapshot["host_prepared"] = action.preinput_host_prepared; host_prepared_snapshot["scene_prepared"] = action.output_scene_prepared;
			host_prepared_snapshot["view_prepared"] = action.host_view_prepared; host_prepared_snapshot["action_prepared"] = action.prepared;
			host_prepared_snapshot["output_plan_prepared"] = action.output_plan_prepared; host_prepared_snapshot["frozen"] = host_original_request->frozen;
			host_prepared_snapshot["instance"] = int64_t(action.output_scene.instance); host_prepared_snapshot["scenario"] = int64_t(action.output_scene.scenario);
			host_prepared_snapshot["particles"] = int64_t(action.output_scene.particles); host_prepared_snapshot["generation"] = int64_t(action.output_scene.generation);
		}
		host_scene_prepared = true; ++host_scene_step; return;
	}
	if (staged_enabled) {
		if (!staged_readbacks_verified && !staged_bank_refused) { advance_staged_execution(); }
		else if (close_requested) { advance_staged_close(); }
		return;
	}
	if (!close_requested) { return; }
	if (host_scene_step == 6) {
		const auto cancel = port<GDExtensionInterfaceDeviceSessionProductionCancelV1>("device_session_production_cancel_v1");
		if (!cancel) { host_fail(ERR_UNAVAILABLE, "host_cancel_port"); return; }
		if (host_result(Error(cancel(host_request).native_error), "host_request_cancel")) { ++host_scene_step; }
		return;
	}
	if (host_scene_step == 7) {
		const auto release = port<GDExtensionInterfaceDeviceSessionProductionReleaseV2>("device_session_production_release_v2");
		if (!release) { host_fail(ERR_UNAVAILABLE, "host_release_port"); return; }
		GDExtensionProductionServiceBudgetV1 remaining{};
		if (!host_result(Error(release(host_request, &budget, &remaining, &host_release_report).native_error), "host_request_release")) { return; }
		if (host_release_report.service_pending || !host_original_request || host_original_request->serial) { host_fail(ERR_INVALID_DATA, "host_request_not_reclaimed"); return; }
		host_original_request = nullptr; host_request_reclaimed = true; host_close_order.push_back("host_original_request_reclaimed"); ++host_scene_step; return;
	}
	if (host_scene_step == 8) {
		server->free_rid(host_instance);
		RendererSceneCull::NativeParticleSceneFacts stale;
		if (static_cast<RendererSceneCull *>(RSG::scene)->instance_get_committed_particle_scene_facts(host_instance, RID::from_uint64(created.particles), stale) != ERR_INVALID_PARAMETER) { host_fail(ERR_INVALID_DATA, "host_instance_not_freed"); return; }
		host_instance_freed = true; host_close_order.push_back("scene_instance_original_free");
		host_normal_callable = Callable(); // Original free callback/cleanup; before helper or producer destructor.
		if (host_normal_frees != 1) { host_fail(ERR_INVALID_DATA, "host_callable_not_returned"); return; }
		host_normal_released = true; host_scene_closed = true; ++host_scene_step;
	}
}
void report_host_scene(Dictionary &result) const {
	const auto dto = [](const GDExtensionDeviceSessionResultV1 &value) { Dictionary d; d["error"] = value.native_error; d["reason"] = value.reason; d["expected"] = int64_t(value.expected); d["actual"] = int64_t(value.actual); return d; };
	result["host_scene_enabled"] = host_scene_enabled; result["host_scene_step"] = host_scene_step;
	result["host_scene_error"] = host_scene_error; result["host_scene_detail"] = host_scene_detail;
	result["host_scene_prepared"] = host_scene_prepared; result["host_scene_ready"] = host_scene_prepared && !host_scene_closed;
	result["host_scene_closed"] = host_scene_closed; result["host_scene_stop_before_acquire"] = true;
	result["host_normal_calls"] = host_normal_calls; result["host_normal_frees"] = host_normal_frees; result["host_normal_released"] = host_normal_released;
	result["host_normal_snapshot"] = host_normal_snapshot.duplicate(true); result["host_request_snapshot"] = host_request_snapshot.duplicate(true);
	result["host_committed_snapshot"] = host_committed_snapshot.duplicate(true);
	result["host_reserve_snapshot"] = host_reserve_snapshot.duplicate(true); result["host_prepared_snapshot"] = host_prepared_snapshot.duplicate(true);
	result["host_reserve_result"] = dto(host_reserve_result); result["host_import_result"] = dto(host_import_result);
	result["host_close_order"] = host_close_order.duplicate(true);
	result["host_capacity_graphs"] = host_capacity.graph_memberships; result["host_capacity_native"] = host_capacity.native_memberships;
	result["host_request_serial"] = int64_t(host_request.handle_serial); result["host_request_reclaimed"] = host_request_reclaimed;
	result["host_request_release_pending"] = host_release_report.service_pending; result["host_request_release_cause"] = host_release_report.cause.native_error;
	result["host_instance"] = int64_t(host_instance.get_id()); result["host_instance_freed"] = host_instance_freed;
	result["host_particles"] = int64_t(created.particles); result["host_generation"] = int64_t(created.generation);
	result["host_viewport"] = int64_t(host_viewport.get_id()); result["host_camera"] = int64_t(host_camera.get_id()); result["host_scenario"] = int64_t(host_scenario.get_id());
	result["host_node_object_id"] = int64_t(host_node_object_id);
	PackedFloat32Array affine; for (float value : host_input.affine) { affine.push_back(value); } result["host_global_affine"] = affine;
	result["host_prepare_result"] = dto(host_prepare_result); result["host_scene_result"] = dto(host_scene_result);
	result["host_view_result"] = dto(host_view_result); result["host_read_result"] = dto(host_read_result);
	result["host_scene_validate_error"] = host_scene_validate_error;
	result["host_scene_fact_error"] = host_scene_fact_error;
	result["host_scene_validation_performed"] = host_scene_validation_performed;
	result["host_scene_cull_validation_deferred"] = host_scene_cull_validation_deferred;
	PackedFloat32Array read_affine; for (float value : host_read.affine) { read_affine.push_back(value); } result["host_read_affine"] = read_affine;
	result["output_acquired"] = false; result["records_written"] = false; result["finalized"] = false; result["renderer_drawn"] = false;
	result["host_camera_capture_verified"] = false; result["host_view_association_received"] = false;
}
