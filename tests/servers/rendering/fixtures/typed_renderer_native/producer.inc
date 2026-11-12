// Included inside TestProductionRequest::Access, after SingletonEnrollment.
// Renderer fixture source producer only. No Runtime/U05/Host publication claim.
struct TypedRendererProducer {
	SingletonEnrollment enrollment;
	RenderingDevice *device = nullptr;
	RenderingServer *server = nullptr;
	RID mesh, shader, material;
	uint64_t meshes[1]{}, materials[1]{};
	GDExtensionOutputCreateInputV3 create_input{};
	GDExtensionOutputCreateResultV1 created{};
	GDExtensionStorageBackingV1 source{};
	GDExtensionStorageCreateRequestV1 source_origin{};
	GDExtensionProductionRequestRefV1 source_reader{};
	Request *source_reader_request = nullptr;
	GDExtensionProductionMetadataQuoteV1 source_reader_quote{};
	GDExtensionDeviceSessionResultV1 source_reader_quote_result{ ERR_UNCONFIGURED, 0, 0, 0 };
	GDExtensionDeviceSessionResultV1 source_reader_reserve_result{ ERR_UNCONFIGURED, 0, 0, 0 };
	GDExtensionDeviceSessionResultV1 source_reader_import_result{ ERR_UNCONFIGURED, 0, 0, 0 };
	Dictionary source_reader_reserve_snapshot, source_reader_import_snapshot;
	uint64_t source_attachment_command_slots = 0, source_attachment_frame_slots = 0;
	uint64_t source_producer_command_slots = 0, source_producer_frame_slots = 0;
	uint32_t source_pool_quote_current_commands = 0;
	uint32_t source_commands_before_append = 0, source_commands_after_append = 0;
	GDExtensionProductionCapacityV1 source_attachment_capacity{};
	uint64_t source_attachment_serial = 0;
	GDExtensionProductionReportV2 source_reader_release{};
	bool source_reader_quoted = false, source_import_verified = false;
	bool source_reader_reclaimed = false, qualification_retire_started = false;
	Error source_producer_import_error = ERR_UNCONFIGURED, source_reader_import_error = ERR_UNCONFIGURED;
	uint64_t source_reader_rid = 0, source_reader_backing_serial = 0, source_reader_readback_issued = 0;
	uint32_t source_reader_steps = 0, source_reader_graph_budget = 0;
	GDExtensionProductionRequestRefV1 request{};
	GDExtensionProductionRecordReportV2 receipt{};
	GDExtensionProductionAppendV2 appended{};
	Dictionary append_boundary_before, append_boundary_after;
	uint64_t control = 0, deadline = 0;
	uint32_t calls = 0;
	uint32_t geometry_readbacks = 0;
	Dictionary staged_geometry;
	Dictionary staged_material_cold_start;
	int32_t staged_index_worker_before = -2, staged_index_worker_after = -2;
	uint32_t staged_index_worker_polls = 0;
	Error staged_index_worker_check = ERR_UNCONFIGURED, staged_index_worker_poll = ERR_UNCONFIGURED;
	int32_t geometry_worker_before = -2, geometry_worker_after = -2;
	Error geometry_selection_error = ERR_UNCONFIGURED, geometry_source_error = ERR_UNCONFIGURED;
	Error geometry_worker_check_error = ERR_UNCONFIGURED;
	bool geometry_worker_pending = false;
	RendererRD::MeshStorage::NativeSelectionLease geometry_selection;
	RendererRD::MeshStorage::NativeSurfaceHold geometry_source;
	uint32_t material_output_pass_mask = 0;
	bool staged_transparent_profile() const {
		return staged_enabled && OS::get_singleton()->get_environment("GODOT_TEST_STAGED_TRANSPARENT") == "1";
	}
	uint32_t staged_selected_pass_mask() const {
		return staged_transparent_profile() ? RendererRD::ExternalOutputConfig::PASS_TRANSPARENT : RendererRD::ExternalOutputConfig::PASS_OPAQUE;
	}
	enum Preparation { GEOMETRY_WARMING, GEOMETRY_READY, ENROLLMENT_READY };
	Preparation preparation = GEOMETRY_WARMING;
	uint64_t warmup_start_draws = 0, warmup_start_epoch = 0;
	uint64_t geometry_ready_draws = 0;
	uint32_t geometry_worker_polls = 0;
	bool geometry_factory_ready = false;
	Error geometry_worker_poll_error = ERR_UNCONFIGURED;
	uint32_t stage = 0; // 0 storage create; 1 RECORD; 2 original source ready.
	Error error = OK;
	String detail;
	bool retained = true;
	bool prefix_requested = false, prefix_advancing = false, prefix_complete = false;
	bool prefix_storage_released = false, prefix_generation_consumed = false;
	bool prefix_particles_freed = false, prefix_metadata_returned = false;
	bool prefix_request_reclaimed = false, prefix_attachment_closed = false;
	uint32_t prefix_step = 0, prefix_turns = 0, prefix_withdraw_calls = 0;
	uint64_t source_record_context_frame = 0, prefix_withdraw_context_frame = 0;
	uint64_t prefix_deadline = 0, prefix_withdraw_control = 0;
	Error prefix_error = OK, prefix_service_error = OK;
	String prefix_detail;
	GDExtensionOutputStatusV1 prefix_withdraw_status{ ERR_UNCONFIGURED, 0, nullptr };
	GDExtensionProductionRecordReportV2 prefix_withdraw_receipt{};
	GDExtensionStorageRetireRefV1 prefix_retire{};
	GDExtensionStorageRetireReportV1 prefix_storage{};
	GDExtensionProductionOutputGenerationRefV1 prefix_expected_generation{};
	GDExtensionProductionOutputGenerationResultV1 prefix_generation{};
	GDExtensionProductionReportV2 prefix_release_report{};
	// Each semantic has its own original source word. This fixture is its producer.
	const uint32_t source_words[8] = { 1, 0, 0, 0, 0, 0, 0, 0 };

	#include "tests/servers/rendering/fixtures/typed_renderer_native/host_scene.inc"
	#include "tests/servers/rendering/fixtures/typed_renderer_native/staged_camera.inc"
	#include "tests/servers/rendering/fixtures/typed_renderer_native/selection_facts.inc"

	template <class T> static T port(const char *p_name) {
		return reinterpret_cast<T>(GDExtension::get_interface_function(p_name));
	}
	Error refuse(Error p_error, const String &p_detail) {
		if (error == OK && p_error != OK) { error = p_error; detail = p_detail; }
		return p_error;
	}
	Error status(const GDExtensionOutputStatusV1 &p_status, const String &p_step) {
		return refuse(Error(p_status.error), p_step + ":" + (p_status.detail ? String::utf8(p_status.detail, p_status.detail_length) : String()));
	}
	Dictionary snapshot_append_boundary(GDExtensionDeviceExit::ProductionPhysicalContextRef p_context,
			GDExtensionProductionRequestRefV1 p_request) {
		Dictionary result;
		Registry::Lock lock(enrollment.registry);
		Request *original = nullptr;
		RenderingDevice *actual_device = nullptr;
		const Error authenticated = enrollment.registry.production_physical_request_locked(p_context, p_request, original, actual_device, true);
		result["original_context_error"] = authenticated;
		if (authenticated != OK || !original || actual_device != device) { return result; }
		result["request_serial"] = int64_t(original->serial);
		result["contract_version"] = original->contract_version;
		result["frozen"] = original->frozen;
		result["revision"] = int64_t(original->revision);
		result["next_action"] = original->next_action;
		result["action_count"] = original->action_count;
		result["graph_count"] = original->graph_count;
		result["capture_inflight"] = original->capture_inflight;
		result["effects_pending"] = original->effects_pending;
		result["source_device_lost"] = original->source_device_lost;
		result["rd_recording_request_active"] = device->production_recording_request != nullptr;
		result["render_thread_matches"] = device->render_thread_id == Thread::get_caller_id();
		result["same_held_device"] = original->attachment->held_device == device;
		result["physical_permission_query_attempted"] = false;
		// Capture exists only inside the original append leaf. Do not manufacture
		// it or query _production_resource_permitted outside that real extent.
		Array cells;
		for (uint32_t i = 0; i < original->resource_count; ++i) {
			const auto &resource = original->resources[i];
			Dictionary row;
			row["slot"] = i;
			row["kind"] = resource.declaration.resource_kind;
			row["ownership"] = resource.declaration.ownership;
			row["original_slot"] = resource.declaration.original_slot;
			row["original_generation"] = int64_t(resource.declaration.original_generation);
			row["primary"] = int64_t(resource.primary.get_id());
			row["secondary"] = int64_t(resource.secondary.get_id());
			row["imported"] = resource.imported;
			row["physical_use_count"] = resource.use_count;
			row["metadata_created"] = resource.metadata_created;
			row["metadata_generation_bound"] = resource.metadata_generation_bound;
			row["metadata_returned"] = resource.metadata_returned;
			row["backing_serial"] = int64_t(resource.backing_serial);
			cells.push_back(row);
		}
		result["cells"] = cells;
		return result;
	}

	static void receive(void *p_data, GDExtensionDeviceExit::ProductionPhysicalContextRef p_context,
			GDExtensionProductionRequestRefV1 p_request, const GDExtensionProductionOperationV1 *) {
		auto &self = *static_cast<TypedRendererProducer *>(p_data);
		if (self.selection_facts_request.handle_serial && p_request.handle_serial == self.selection_facts_request.handle_serial) {
			self.receive_selection_facts(p_context, p_request);
			return;
		}
		if (self.staged_enabled && p_request.handle_serial == self.host_request.handle_serial) {
			self.staged_receive_cancel(p_context, p_request);
			return;
		}
		if (self.prefix_requested) {
			if (++self.prefix_withdraw_calls != 1 || p_request.registry_instance != self.request.registry_instance ||
					p_request.handle_serial != self.request.handle_serial || !self.created.particles || !self.created.generation) {
				self.prefix_fail(ERR_INVALID_DATA, "prefix_withdraw_receiver_identity"); return;
			}
			self.prefix_withdraw_context_frame = p_context.frame_serial;
			const auto withdraw = port<GDExtensionInterfaceDeviceSessionOutputWithdrawV1>("device_session_output_withdraw_v1");
			if (!withdraw) { self.prefix_fail(ERR_UNAVAILABLE, "prefix_withdraw_port"); return; }
			const GDExtensionOutputGenerationInputV1 input{ GDExtensionDeviceExit::production_dto_abi<GDExtensionOutputGenerationInputV1>(),
				self.created.particles, self.created.generation };
			self.prefix_withdraw_status = withdraw(p_context, p_request, &input);
			return; // Never repeat source Create/upload or change its original receipt.
		}
		self.source_record_context_frame = p_context.frame_serial;
		if (++self.calls != 1 || p_request.registry_instance != self.request.registry_instance || p_request.handle_serial != self.request.handle_serial) {
			self.refuse(ERR_INVALID_DATA, "producer_record_identity"); return;
		}
		const auto create = port<GDExtensionInterfaceDeviceSessionOutputCreateV3>("device_session_output_create_v3");
		const auto revision = port<GDExtensionInterfaceDeviceSessionProductionReadRevisionV1>("device_session_production_read_revision_v1");
		const auto append = port<GDExtensionInterfaceDeviceSessionProductionAppendV2>("device_session_production_append_v2");
		if (!create || !revision || !append) { self.refuse(ERR_UNAVAILABLE, "producer_record_ports"); return; }
		create(p_context, p_request, &self.create_input, &self.created);
		if (self.status(self.created.status, "create_v3") != OK) { return; }
		if (!self.created.particles || !self.created.generation) { self.refuse(ERR_INVALID_DATA, "create_v3_empty_result"); return; }
		uint64_t original_revision = 0;
		auto result = revision(p_context, p_request, &original_revision);
		if (self.refuse(Error(result.native_error), "source_read_revision") != OK) { return; }
		self.append_boundary_before = self.snapshot_append_boundary(p_context, p_request);
		self.source_commands_before_append = self.device->draw_graph.qualification_command_count();
		result = append(p_context, p_request, 0, original_revision, &self.appended);
		self.source_commands_after_append = self.device->draw_graph.qualification_command_count();
		self.append_boundary_after = self.snapshot_append_boundary(p_context, p_request);
		if (self.refuse(Error(result.native_error), "source_checked_upload") != OK) { return; }
		if (self.appended.state != GDEXTENSION_PRODUCTION_APPENDED_V1) { self.refuse(ERR_INVALID_DATA, "source_upload_not_appended"); }
	}

	Error begin() {
		using namespace GDExtensionDeviceExit;
		device = RenderingDevice::get_singleton();
		server = RenderingServer::get_singleton();
		auto *owner = Object::cast_to<RenderingServerDefault>(server);
		if (!device || !owner || !server->is_on_render_thread() || !device->is_main_instance || server->get_rendering_device() != device) {
			return refuse(ERR_UNAUTHORIZED, "main_rd_owner_required");
		}
		if (device->draw_list.active || device->compute_list.active || device->raytracing_list.active) {
			return refuse(ERR_BUSY, "outside_native_lists_required");
		}
		deadline = OS::get_singleton()->get_ticks_usec() + 20000000;
		mesh = server->mesh_create();
		Array arrays;
		arrays.resize(RSE::ARRAY_MAX);
		PackedVector3Array vertices;
		vertices.push_back(Vector3(-0.5f, -0.5f, 0));
		vertices.push_back(Vector3(0.5f, -0.5f, 0));
		vertices.push_back(Vector3(0, 0.5f, 0));
		arrays[RSE::ARRAY_VERTEX] = vertices;
		if (staged_enabled) {
			PackedInt32Array indices;
			indices.push_back(0);
			indices.push_back(1);
			indices.push_back(2);
			arrays[RSE::ARRAY_INDEX] = indices;
		}
		server->mesh_add_surface_from_arrays(mesh, RSE::PRIMITIVE_TRIANGLES, arrays);
		if (server->mesh_get_surface_count(mesh) != 1) { return refuse(ERR_CANT_CREATE, "triangle_surface"); }
		warmup_start_draws = Engine::get_singleton()->get_frames_drawn();
		warmup_start_epoch = device->frames[device->frame].qualification_epoch;
		return OK;
	}

	Error poll_original_geometry_worker(RenderingDevice::Buffer &buffer, uint32_t &polls) {
		// Pre-enrollment ordinary owner work only; no borrowed production funds,
		// Scene witnesses, injected budget or alternate device can enter this path.
		if (!server->is_on_render_thread() || RenderingDevice::get_singleton() != device || !device->driver || !device->is_main_instance || !device->native_immutable_is_render_thread() ||
				request.handle_serial || enrollment.attachment.handle_serial || device->production_entity_registry.load(std::memory_order_acquire) ||
				device->native_disposal_budget || device->native_disposal_active.load(std::memory_order_acquire) ||
				device->production_finalization_started.load(std::memory_order_acquire) || device->native_finalization_returned) { return ERR_UNAUTHORIZED; }
		const Error recording = device->_recording_error();
		if (recording != OK) { return recording; }
		if (device->draw_list.active || device->compute_list.active || device->raytracing_list.active || device->draw_graph.has_driver_callbacks()) { return ERR_BUSY; }
		if (buffer.transfer_worker_index < 0) { return OK; }
		const uint32_t index = uint32_t(buffer.transfer_worker_index);
		const uint64_t operation = buffer.transfer_worker_operation;
		struct Unlock { BinaryMutex &mutex; ~Unlock() { mutex.unlock(); } };
		if (!device->transfer_worker_pool_mutex.try_lock()) { return ERR_BUSY; }
		Unlock pool_unlock{ device->transfer_worker_pool_mutex };
		if (index >= device->transfer_worker_pool_size || index >= uint32_t(device->transfer_worker_pool.size()) || !device->transfer_worker_pool[index]) { return ERR_INVALID_DATA; }
		auto *worker = device->transfer_worker_pool[index];
		if (!worker->thread_mutex.try_lock()) { return ERR_BUSY; }
		Unlock thread_unlock{ worker->thread_mutex };
		if (OS::get_singleton()->get_ticks_usec() >= deadline) { return ERR_TIMEOUT; }
		if (worker->index != index || !operation) { return ERR_INVALID_DATA; }
		if (!worker->operations_mutex.try_lock()) { return ERR_BUSY; }
		const bool original = operation <= worker->operations_counter;
		const bool already_processed = operation <= worker->operations_processed;
		const bool submitted = !worker->recording && !worker->submission_pending && worker->submitted &&
				operation <= worker->operations_submitted && worker->operations_submitted <= worker->operations_counter;
		worker->operations_mutex.unlock();
		if (!original) { return ERR_INVALID_DATA; }
		if (worker->poisoned) { return worker->completion_error != OK ? worker->completion_error : FAILED; }
		if (already_processed) { return OK; }
		if (!submitted) { return ERR_BUSY; } // Normal renderer frame owns submission.
		++polls;
		// This exact original method retains all observer/submission/fence/
		// production-join/admission/recycle gates; it alone writes processed.
		if (!device->_wait_for_transfer_worker(worker, false)) {
			return worker->completion_error != OK ? worker->completion_error : worker->poisoned ? FAILED : ERR_BUSY;
		}
		return OK;
	}

	Error warm_geometry() {
		// New vertex data belongs to MeshStorage's original transfer worker.
		// Native source holds deliberately reject a still-associated worker.
		// Preserve actual worker-check Errors before the getter can collapse a
		// refusal into an empty vector. A still-pending original upload is legal:
		// the ordinary getter performs its actual flush/wait before mapping.
		auto *mesh_storage = RendererRD::MeshStorage::get_singleton();
		if (!mesh_storage) { return refuse(ERR_UNAVAILABLE, "triangle_original_mesh_owner"); }
		const auto *original_mesh = mesh_storage->mesh_owner.get_or_null(mesh);
		if (!original_mesh || original_mesh->surface_count != 1) { return refuse(ERR_INVALID_DATA, "triangle_original_surface"); }
		const auto *surface = original_mesh->surfaces[0];
		const RID original_vertices = surface->vertex_buffer;
		auto *index_buffer = staged_enabled ? device->index_buffer_owner.get_or_null(surface->index_buffer) : nullptr;
		auto *index_array = staged_enabled ? device->index_array_owner.get_or_null(surface->index_array) : nullptr;
		if (staged_enabled) {
			staged_geometry["vertex_buffer"] = int64_t(original_vertices.get_id());
			staged_geometry["index_buffer"] = int64_t(surface->index_buffer.get_id());
			staged_geometry["index_array"] = int64_t(surface->index_array.get_id());
			if (!index_buffer || !index_array || index_array->indices != 3 || index_array->offset != 0 ||
					index_array->driver_id != index_buffer->driver_id || index_array->format != RenderingDevice::INDEX_BUFFER_FORMAT_UINT16) {
				return refuse(ERR_INVALID_DATA, "triangle_original_index_pair");
			}
			if (staged_index_worker_before == -2) { staged_index_worker_before = index_buffer->transfer_worker_index; }
		}
		auto *original_buffer = device->vertex_buffer_owner.get_or_null(original_vertices);
		if (!original_buffer) { return refuse(ERR_INVALID_DATA, "triangle_original_vertex_buffer"); }
		if (geometry_worker_before == -2) { geometry_worker_before = original_buffer->transfer_worker_index; }
		// Register this exact original operation as a real draw dependency before
		// polling. A later normal owner frame submits it; pending is still legal.
		geometry_worker_check_error = device->_check_transfer_worker_buffer(original_buffer);
		geometry_worker_after = original_buffer->transfer_worker_index;
		if (geometry_worker_check_error == ERR_BUSY) { return ERR_BUSY; }
		if (geometry_worker_check_error != OK) { return refuse(geometry_worker_check_error, "triangle_original_dependency_check"); }
		if (index_buffer) {
			staged_index_worker_check = device->_check_transfer_worker_buffer(index_buffer);
			staged_index_worker_after = index_buffer->transfer_worker_index;
			if (staged_index_worker_check == ERR_BUSY) { return ERR_BUSY; }
			if (staged_index_worker_check != OK) { return refuse(staged_index_worker_check, "triangle_original_index_dependency"); }
			staged_index_worker_poll = poll_original_geometry_worker(*index_buffer, staged_index_worker_polls);
			if (staged_index_worker_poll == ERR_BUSY) { geometry_worker_pending = true; return ERR_BUSY; }
			if (staged_index_worker_poll != OK) { return refuse(staged_index_worker_poll, "triangle_original_index_worker_poll"); }
		}
		geometry_worker_poll_error = poll_original_geometry_worker(*original_buffer, geometry_worker_polls);
		if (geometry_worker_poll_error == ERR_BUSY) { geometry_worker_pending = true; return ERR_BUSY; }
		if (geometry_worker_poll_error != OK) { return refuse(geometry_worker_poll_error, "triangle_original_worker_poll"); }
		const RendererRD::MeshStorage *readback_owner = mesh_storage;
		const float expected_vertices[9] = { -0.5f, -0.5f, 0, 0.5f, -0.5f, 0, 0, 0.5f, 0 };
		const uint16_t expected_indices[3] = { 0, 1, 2 };
		while (geometry_readbacks < 2) {
			geometry_worker_check_error = device->_check_transfer_worker_buffer(original_buffer);
			geometry_worker_after = original_buffer->transfer_worker_index;
			if (geometry_worker_check_error == ERR_BUSY) { return ERR_BUSY; }
			if (geometry_worker_check_error != OK) { return refuse(geometry_worker_check_error, "triangle_original_transfer_check"); }
			// A const owner unambiguously selects the SurfaceData(int) getter.
			const auto actual = readback_owner->mesh_get_surface(mesh, 0);
			if (actual.vertex_data.is_empty() || (staged_enabled && actual.index_data.is_empty())) {
				const Error native_error = device->_recording_error();
				if (native_error == OK || native_error == ERR_BUSY) { return ERR_BUSY; }
				return refuse(native_error, "triangle_original_readback_unavailable_preparation");
			}
			if (actual.vertex_count != 3 || actual.index_count != (staged_enabled ? 3u : 0u) || actual.primitive != RSE::PRIMITIVE_TRIANGLES ||
					uint64_t(actual.vertex_data.size()) != sizeof(expected_vertices) || memcmp(actual.vertex_data.ptr(), expected_vertices, sizeof(expected_vertices)) != 0) {
				const Error native_error = device->_recording_error();
				return refuse(native_error != OK ? native_error : ERR_INVALID_DATA, "triangle_original_upload_readback");
			}
			if (staged_enabled) {
				staged_geometry["index_count"] = actual.index_count;
				staged_geometry["index_bytes"] = actual.index_data.size();
				staged_geometry["index_bytes_hex"] = String::hex_encode_buffer(actual.index_data.ptr(), actual.index_data.size());
				if (actual.index_data.size() != sizeof(expected_indices) || memcmp(actual.index_data.ptr(), expected_indices, sizeof(expected_indices)) != 0) {
					return refuse(ERR_INVALID_DATA, "triangle_original_index_upload_readback");
				}
				staged_geometry["index_readbacks"] = geometry_readbacks + 1;
			}
			++geometry_readbacks;
		}
		original_buffer = device->vertex_buffer_owner.get_or_null(original_vertices);
		if (!original_buffer) { return refuse(ERR_INVALID_DATA, "triangle_original_vertex_lost"); }
		geometry_worker_check_error = device->_check_transfer_worker_buffer(original_buffer);
		geometry_worker_after = original_buffer->transfer_worker_index;
		if (geometry_worker_check_error == ERR_BUSY) { return ERR_BUSY; }
		if (geometry_worker_check_error != OK) { return refuse(geometry_worker_check_error, "triangle_original_transfer_check_after_readback"); }
		if (geometry_worker_after >= 0) {
			geometry_worker_pending = true;
			return ERR_BUSY;
		}
		if (index_buffer) {
			staged_index_worker_check = device->_check_transfer_worker_buffer(index_buffer);
			staged_index_worker_after = index_buffer->transfer_worker_index;
			if (staged_index_worker_check == ERR_BUSY) { return ERR_BUSY; }
			if (staged_index_worker_check != OK) { return refuse(staged_index_worker_check, "triangle_original_index_check_after_readback"); }
			const Error array_dependency = device->_check_transfer_worker_index_array(index_array);
			staged_geometry["index_array_worker"] = index_array->transfer_worker_index;
			staged_geometry["index_array_worker_check"] = array_dependency;
			if (array_dependency == ERR_BUSY) { return ERR_BUSY; }
			if (array_dependency != OK) { return refuse(array_dependency, "triangle_original_index_array_dependency"); }
			if (staged_index_worker_after >= 0 || index_array->transfer_worker_index >= 0) {
				geometry_worker_pending = true;
				return ERR_BUSY;
			}
		}
		if (!geometry_factory_ready) {
			geometry_selection_error = mesh_storage->mesh_selection_acquire(mesh, geometry_selection);
			if (geometry_selection_error == ERR_BUSY) { return ERR_BUSY; }
			if (geometry_selection_error != OK) { return refuse(geometry_selection_error, "triangle_native_selection"); }
			if (staged_enabled) {
				const Error observed = mesh_storage->mesh_selection_get_surface_observation(geometry_selection, 0, selection_facts_mesh);
				if (observed != OK) {
					const Error returned = mesh_storage->mesh_selection_release(geometry_selection);
					return refuse(returned == OK ? observed : returned, "selection_facts_original_mesh");
				}
			}
			geometry_source_error = mesh_storage->mesh_surface_source_hold_acquire(mesh, 0, geometry_source);
			if (geometry_source_error == ERR_BUSY) {
				const Error returned = mesh_storage->mesh_selection_release(geometry_selection);
				return returned == OK ? ERR_BUSY : refuse(returned, "triangle_native_selection_retry_return");
			}
			if (geometry_source_error != OK) { return refuse(geometry_source_error, "triangle_native_source_hold"); }
			geometry_factory_ready = true;
		}
		if (geometry_source.is_active()) {
			const Error returned = mesh_storage->mesh_surface_hold_release(geometry_source);
			if (returned == ERR_BUSY) { return ERR_BUSY; }
			if (returned != OK) { return refuse(returned, "triangle_native_source_return"); }
		}
		if (geometry_selection.is_active()) {
			const Error returned = mesh_storage->mesh_selection_release(geometry_selection);
			if (returned == ERR_BUSY) { return ERR_BUSY; }
			if (returned != OK) { return refuse(returned, "triangle_native_selection_return"); }
		}

		geometry_worker_pending = false;
		geometry_ready_draws = Engine::get_singleton()->get_frames_drawn();
		preparation = GEOMETRY_READY;
		return OK;
	}

	Error observe_staged_material_startup(RendererRD::MaterialStorage &materials) {
		using RendererSceneRenderImplementation::SceneShaderForwardMobile;
		if (GDExtensionDeviceExit::Registry::production_physical_frame) {
			return refuse(ERR_BUSY, "staged_material_cold_start_physical_frame");
		}
		const uint64_t generation = materials.material_get_generation(material);
		RendererRD::MaterialStorage::MaterialData *original = nullptr;
		const Error observed = materials.material_get_native_preparation_data(material, generation, original);
		if (observed != OK || !original) {
			return refuse(observed == OK ? ERR_UNAVAILABLE : observed, "staged_material_cold_start_original");
		}
		const uint32_t backend = original->get_native_recipe_backend();
		if (backend == 2) {
			// The live Clustered material uses its renderer's pre-list preparation.
			staged_material_cold_start["material"] = int64_t(material.get_id());
			staged_material_cold_start["material_generation"] = int64_t(generation);
			staged_material_cold_start["backend"] = backend;
			staged_material_cold_start["mobile_observation_skipped"] = true;
			return OK;
		}
		if (backend != 1) {
			return refuse(ERR_UNAVAILABLE, "staged_material_cold_start_original");
		}
		const auto *mobile = static_cast<const SceneShaderForwardMobile::MaterialData *>(original);
		const auto *shader_data = mobile->shader_data;
		if (!shader_data || !shader_data->instance192_shader_supported()) {
			return refuse(ERR_UNAVAILABLE, "staged_material_cold_start_shader");
		}
		// The staged fixture's mono viewport has no lightmap or deferred fog.
		constexpr auto version = SceneShaderForwardMobile::SHADER_VERSION_COLOR_PASS;
		staged_material_cold_start["material"] = int64_t(material.get_id());
		staged_material_cold_start["material_generation"] = int64_t(generation);
		staged_material_cold_start["version"] = uint32_t(version);
		staged_material_cold_start["fp16"] = device->has_feature(RD::SUPPORTS_HALF_FLOAT);
		RID before_shader;
		uint64_t before_mask = 0;
		const Error before = shader_data->get_native_shader_if_ready(version, false, false, true, before_shader, before_mask);
		staged_material_cold_start["before_error"] = before;
		staged_material_cold_start["before_shader"] = int64_t(before_shader.get_id());
		staged_material_cold_start["before_input_mask"] = int64_t(before_mask);
		if (before != OK && before != ERR_UNAVAILABLE && before != ERR_BUSY) {
			return refuse(before, "staged_material_cold_start_before");
		}
		return OK;
	}

	Error finish_begin() {
		using namespace GDExtensionDeviceExit;
		if (server->is_render_loop_enabled() || device->draw_list.active || device->compute_list.active || device->raytracing_list.active) { return ERR_BUSY; }
		auto *owner = Object::cast_to<RenderingServerDefault>(server);
		if (!owner || preparation != GEOMETRY_READY || request.handle_serial) { return refuse(ERR_INVALID_DATA, "producer_enrollment_phase"); }
		shader = server->shader_create_from_code(staged_enabled
				? (staged_transparent_profile()
						? "shader_type spatial; render_mode unshaded, cull_disabled; void fragment() { ALBEDO = COLOR.rgb; ALPHA = 0.5; }"
						: "shader_type spatial; render_mode unshaded, cull_disabled; void fragment() { ALBEDO = COLOR.rgb; }")
				: "shader_type spatial; render_mode unshaded, cull_disabled; void fragment() { ALBEDO = vec3(1.0, 0.125, 0.0625); }");
		material = server->material_create();
		server->material_set_shader(material, shader);
		auto *material_storage = RendererRD::MaterialStorage::get_singleton();
		if (!material_storage) { return refuse(ERR_UNAVAILABLE, "native_material_storage"); }
		material_storage->_update_queued_materials();
		if (staged_enabled && observe_staged_material_startup(*material_storage) != OK) { return error; }
		RendererRD::MaterialStorage::NativeSelectionLease selected;
		Error selected_error = material_storage->material_selection_acquire(material, selected, material_storage->material_get_generation(material));
		if (selected_error != OK) { return refuse(selected_error, "compiled_material_selection"); }
		RendererRD::MaterialStorage::NativeSelectionShaderObservation observed;
		const Error observed_error = material_storage->material_selection_get_shader_observation(selected, observed);
		const Error returned = material_storage->material_selection_release(selected);
		if (observed_error != OK) { return refuse(observed_error, "compiled_material_observation"); }
		if (returned != OK) { return refuse(returned, "compiled_material_selection_return"); }
		const uint32_t expected_recipe = staged_transparent_profile() ? RendererRD::MaterialStorage::ShaderData::MATERIAL_RECIPE_ALPHA : 0;
		if (observed.shader_version.is_null() || observed.material_recipe_rejections != expected_recipe) { return refuse(ERR_UNAVAILABLE, "compiled_material_recipe"); }
		material_output_pass_mask = observed.output_pass_mask;
		if (staged_enabled) { selection_facts_material = observed; }
		if (!(material_output_pass_mask & staged_selected_pass_mask()) ||
				(staged_transparent_profile() && material_output_pass_mask != (RendererRD::ExternalOutputConfig::PASS_TRANSPARENT | RendererRD::ExternalOutputConfig::PASS_PROBE))) {
			return refuse(ERR_UNAVAILABLE, "compiled_material_selected_pass");
		}

		meshes[0] = mesh.get_id(); materials[0] = material.get_id();
		create_input.abi = { 3, sizeof(create_input), 0 };
		auto &config = create_input.config;
		config.abi = production_dto_abi<GDExtensionOutputCreateInputV1>();
		config.profile = config.capacity = config.max_views = config.max_inflight = config.space = 1;
		config.pass_mask = material_output_pass_mask & staged_selected_pass_mask();
		config.surfaces = { meshes, 1, 0 }; config.surface_materials = { materials, 1, 0 };
		for (uint32_t axis = 0; axis < 3; ++axis) { config.bounds.position[axis] = -2; config.bounds.size[axis] = 4; }
		// Opaque fixture correlation data, never used as source/view authority.
		config.producer_tag.bytes[0] = 1; config.device_tag.bytes[0] = 2;
		config.layout_digest.bytes[0] = 3; config.mapping_digest.bytes[0] = 4; config.numeric_profile_digest.bytes[0] = 5;
		GDExtensionOutputMetadataQueryInputV1 query{};
		query.abi = production_dto_abi<GDExtensionOutputMetadataQueryInputV1>(); query.config = config;
		query.limits = { production_dto_abi<GDExtensionOutputMetadataLimitsV1>(), 1, staged_read_capacity(), 1, 0 };
		GDExtensionOutputMetadataQueryResultV1 quote{};
		const auto metadata = port<GDExtensionInterfaceDeviceSessionOutputMetadataQueryV1>("device_session_output_metadata_query_v1");
		if (!metadata) { return refuse(ERR_UNAVAILABLE, "metadata_query_port"); }
		metadata(&query, &quote);
		if (status(quote.status, "metadata_query") != OK) { return error; }
		create_input.requirements = quote.requirements; create_input.metadata_resource_slot = 0;

		// Explicit fixture allowance. Original allocator computes all native closure.
		OrdinalPlan plan;
		plan.capacity = {};
		plan.capacity.abi = production_dto_abi<GDExtensionProductionCapacityV1>();
		plan.capacity.requests = 1;
		plan.capacity.suboperations = 1;
		plan.capacity.resources = 2;
		plan.capacity.resource_uses = 256;
		plan.capacity.graph_memberships = 16;
		plan.capacity.return_cells = 2;
		plan.capacity.retained_payload_bytes = sizeof(source_words);
		plan.capacity.readback_bytes = 0;
		plan.action_count = 0; plan.resource_count = plan.use_count = plan.readback_count = 0;
		{
			Registry::Lock registry_lock(enrollment.registry); MutexLock rd_lock(device->_thread_safe_);
			const Error bounded = enrollment.registry.production_membership_bound_locked(*device, plan.capacity.graph_memberships, device->transfer_worker_pool_max_size, plan.capacity.native_memberships);
			if (bounded != OK) { return refuse(bounded, "producer_native_bound"); }
		}
		enrollment.server = owner;
		OrdinalPlan pool = plan;
		pool.attachment_requests = 2;
		++pool.capacity.suboperations;
		++pool.capacity.resources; ++pool.capacity.resource_uses; ++pool.capacity.return_cells;
		pool.capacity.readbacks = 1; pool.capacity.readback_bytes = sizeof(uint32_t);
		{
			Registry::Lock registry_lock(enrollment.registry); MutexLock rd_lock(device->_thread_safe_);
			const Error graph_bound = production_staging_graph_bound(sizeof(uint32_t), device->download_staging_buffers.block_size, source_reader_graph_budget);
			if (graph_bound != OK || !source_reader_graph_budget || source_reader_graph_budget > UINT32_MAX - pool.capacity.graph_memberships) {
				return refuse(graph_bound != OK ? graph_bound : ERR_PARAMETER_RANGE_ERROR, "source_reader_staging_bound");
			}
			pool.capacity.graph_memberships += source_reader_graph_budget;
			// Host mode prepays a separate future command-prefix allowance. The
			// immutable pool remains guarded; the reader slot is actually reclaimed.
			if (staged_enabled) {
				const Error funded = fund_staged_pool(pool, plan);
				if (funded != OK) { return funded; }
			} else if (host_scene_enabled) {
				pool.capacity.graph_memberships += plan.capacity.graph_memberships;
			}
			source_pool_quote_current_commands = device->draw_graph.qualification_command_count();
			const Error bounded = enrollment.registry.production_membership_bound_locked(*device, pool.capacity.graph_memberships, device->transfer_worker_pool_max_size, pool.capacity.native_memberships);
			if (bounded != OK) { return refuse(bounded, "source_reader_native_bound"); }
		}
		// Existing helper's main-pair branch. Deferred reserve creates no SET.
		const Error admitted = enrollment.initialize_owner(1, false, true, &receive, this, false, 1, false,
				+[](void *, GDExtensionProductionAttachmentRefV1) {}, false, false, false, false, &pool, device, true);
		if (admitted != OK) { enrollment.frame.leave(); return refuse(admitted, "fixture_shared_device_enrollment"); }
		if (staged_enabled) {
			const auto facts = port<GDExtensionInterfaceDeviceSessionOutputDeviceFactsV2>("device_session_output_device_facts_v2");
			const Error exported = facts ? Error(facts(enrollment.context, enrollment.session, &selection_facts_device).native_error) : ERR_UNAVAILABLE;
			if (exported != OK) { enrollment.frame.leave(); return refuse(exported, "selection_facts_original_device"); }
		}
		const auto qualification_open = port<GDExtensionInterfaceDeviceSessionQualificationOpenV1>("device_session_qualification_open_v1");
		if (!qualification_open) { enrollment.frame.leave(); return refuse(ERR_UNAVAILABLE, "source_qualification_open_port"); }
		const auto qualified = qualification_open(&Enrollment::ABI, enrollment.ready, enrollment.context, enrollment.session,
			{ enrollment.instance, enrollment.holder.handle_serial }, &enrollment.qualification);
		if (qualified.native_error != OK) { enrollment.frame.leave(); return refuse(Error(qualified.native_error), "source_original_qualification_open"); }
		const GDExtensionProductionActionV2 action{ 0, 11, 7, 0, GDEXTENSION_PRODUCTION_UPLOAD_V1, UINT32_MAX, 0, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, UINT32_MAX };
		GDExtensionProductionResourceV1 resources[2]{};
		resources[0].resource_kind = GDEXTENSION_PRODUCTION_OUTPUT_METADATA_V1;
		resources[0].ownership = GDEXTENSION_PRODUCTION_CREATED_BY_OWNER_V1;
		resources[0].original_generation = 1;
		resources[0].physical_bytes = quote.requirements.required_metadata_bytes;
		resources[1].resource_kind = GDEXTENSION_PRODUCTION_STORAGE_V1;
		resources[1].ownership = GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1;
		resources[1].original_slot = 1;
		resources[1].original_generation = 1;
		resources[1].physical_bytes = sizeof(source_words);
		source_origin.abi = Enrollment::ABI;
		source_origin.bytes = resources[1].physical_bytes;
		source_origin.allocation_slot = resources[1].original_slot;
		source_origin.part = resources[1].original_part;
		source_origin.allocation_generation = resources[1].original_generation;
		source_origin.create_attempt = 1;
		if (source_origin.allocation_slot != resources[1].original_slot || source_origin.part != resources[1].original_part ||
				source_origin.allocation_generation != resources[1].original_generation || source_origin.bytes != resources[1].physical_bytes ||
				plan.capacity.retained_payload_bytes < sizeof(source_words) || plan.capacity.readback_bytes != 0) {
			enrollment.frame.leave(); return refuse(ERR_INVALID_DATA, "producer_source_contract");
		}
		const GDExtensionProductionUseV1 use{ 1, GDEXTENSION_PRODUCTION_WRITE_V1, 0, sizeof(source_words), 0 };
		const auto reserve = port<GDExtensionInterfaceDeviceSessionProductionReserveV2>("device_session_production_reserve_v2");
		if (!reserve) { enrollment.frame.leave(); return refuse(ERR_UNAVAILABLE, "producer_reserve_port"); }
		const auto reserved = reserve(enrollment.attachment, &plan.operation, &plan.capacity, &action, 1, resources, 2, &use, 1, nullptr, 0, &request);
		if (reserved.native_error != OK) { enrollment.frame.leave(); return refuse(Error(reserved.native_error), "producer_reserve"); }
		Error found = OK;
		RenderingDevice *actual = nullptr;
		{
			Registry::Lock lock(enrollment.registry);
			found = enrollment.registry.production_request_locked(request, false, enrollment.request, actual);
			if (found == OK && enrollment.request && enrollment.request->attachment) {
				source_attachment_serial = enrollment.request->attachment->serial;
				source_attachment_capacity = enrollment.request->attachment->quote.capacity;
				source_attachment_command_slots = enrollment.request->attachment->quote.graph_slots;
				source_attachment_frame_slots = enrollment.request->attachment->quote.graph_frames;
				source_producer_command_slots = enrollment.request->command_count;
				source_producer_frame_slots = enrollment.request->frame_count;
			}
		}
		if (found != OK || actual != device) { enrollment.frame.leave(); return refuse(found != OK ? found : ERR_INVALID_DATA, "producer_original_request"); }
		if (host_scene_enabled) {
			const Error adopted = adopt_host_callable();
			if (adopted != OK) { enrollment.frame.leave(); return refuse(adopted, "host_original_callable_adoption"); }
			if (staged_enabled) {
				const Error relay = enroll_staged_relay();
				if (relay != OK) { enrollment.frame.leave(); return refuse(relay, "staged_original_relay_adoption"); }
			}
		}
		enrollment.frame.leave(); // Same no-retained-TLS contract as HostViewGpuHarness.
		preparation = ENROLLMENT_READY;
		return OK;
	}

	bool qualification_source_matches() const {
		// Original factory + successful public import authenticates the child.
		// Observe the buffer's real return cell without exposing bridge internals.
		const auto *buffer = device->storage_buffer_owner.get_or_null(RID::from_uint64(source.rid));
		const auto *physical = buffer ? buffer->qualification_return : nullptr;
		return source.backing.registry_instance == enrollment.instance && source.backing.handle_serial && source.state == 1 && source.native_error == OK &&
			source_producer_import_error == OK && source.byte_size == sizeof(source_words) && buffer && buffer->backing_serial == source.backing_serial &&
			buffer->size == source.byte_size && physical && physical->bridge == enrollment.qualification.handle_serial && physical->child == source.backing.handle_serial &&
			physical->rid == source.rid && physical->serial == source.backing_serial && !physical->use_closed.load() && !physical->enqueued;
	}
	Dictionary snapshot_source_reader_reserve(const GDExtensionProductionCapacityV1 *offered = nullptr, bool native_output = false) {
		const auto &capacity = offered ? *offered : source_reader_quote.capacity;
		using namespace GDExtensionDeviceExit;
		Dictionary result;
		result["scope"] = "read_only_original_layout_and_free_slices_not_admission_authority";
		Registry::Lock lock(enrollment.registry);
		ProductionAttachment *attachment = nullptr; RenderingDevice *actual = nullptr;
		const Error authenticated = enrollment.registry.production_attachment_locked(enrollment.attachment, false, attachment, actual);
		result["attachment_lookup_error"] = authenticated;
		if (authenticated != OK || !attachment || actual != device) { return result; }
		result["attachment_serial"] = int64_t(attachment->serial);
		result["pool_command_slots"] = int64_t(attachment->quote.graph_slots);
		result["pool_frame_slots"] = int64_t(attachment->quote.graph_frames);
		const auto largest_free = [](const auto *cells, uint32_t count) {
			uint32_t longest = 0, run = 0;
			for (uint32_t i = 0; i < count; ++i) {
				run = cells[i].request ? 0 : run + 1;
				longest = MAX(longest, run);
			}
			return longest;
		};
		uint32_t free_requests = 0;
		for (uint32_t i = 0; i < attachment->quote.capacity.requests; ++i) { free_requests += !attachment->requests[i].serial; }
		result["free_requests"] = free_requests;
		result["free_actions"] = largest_free(attachment->actions, attachment->quote.capacity.suboperations);
		result["free_resources"] = largest_free(attachment->resources, attachment->quote.capacity.resources);
		result["free_uses"] = largest_free(attachment->uses, attachment->quote.capacity.resource_uses);
		result["free_readbacks"] = largest_free(attachment->readbacks, attachment->quote.capacity.readbacks);
		result["free_graphs"] = largest_free(attachment->graphs, attachment->quote.capacity.graph_memberships);
		result["free_natives"] = largest_free(attachment->natives, attachment->quote.capacity.native_memberships);
		result["free_returns"] = largest_free(attachment->returns, attachment->quote.capacity.return_cells);
		result["free_commands"] = largest_free(attachment->commands, uint32_t(attachment->quote.graph_slots));
		result["free_frames"] = largest_free(attachment->frames, uint32_t(attachment->quote.graph_frames));
		if (!device->_thread_safe_.try_lock()) { result["rd_snapshot_error"] = ERR_BUSY; return result; }
		struct Unlock { Mutex &mutex; ~Unlock() { mutex.unlock(); } } unlock{ device->_thread_safe_ };
		result["rd_snapshot_error"] = OK;
		const uint32_t current = device->draw_graph.qualification_command_count();
		const uint32_t workers = native_output ? device->transfer_worker_pool_max_size : MIN(1u, device->transfer_worker_pool_max_size); // Original native-output vs one-storage READBACK profile.
		result["current_commands"] = current;
		result["reader_worker_bound"] = workers;
		result["max_workers"] = device->transfer_worker_pool_max_size;
		uint32_t native_bound = 0;
		const Error bounded = enrollment.registry.production_membership_bound_locked(*device, capacity.graph_memberships, workers, native_bound);
		result["native_bound_error"] = bounded;
		result["native_bound"] = native_bound;
		result["native_offered"] = capacity.native_memberships;
		GDExtensionProductionMetadataQuoteV1 layout{};
		const Error sized = Registry::production_layout(capacity, layout, current, device->transfer_worker_pool_max_size);
		result["request_layout_error"] = sized;
		result["request_layout_command_slots"] = int64_t(layout.graph_slots);
		result["request_layout_frame_slots"] = int64_t(layout.graph_frames);
		return result;
	}
	void advance_source_import_probe() {
		using namespace GDExtensionDeviceExit;
		if (source_import_verified || error != OK) { return; }
		if (!server->is_on_render_thread() || RenderingDevice::get_singleton() != device || !qualification_source_matches()) {
			refuse(ERR_UNAUTHORIZED, "source_reader_original_owner"); return;
		}
		if (OS::get_singleton()->get_ticks_usec() >= deadline) { refuse(ERR_TIMEOUT, "source_reader_import_deadline_retained"); return; }
		const GDExtensionProductionActionV2 action{ 0, 11, 9, 0, GDEXTENSION_PRODUCTION_READBACK_V1, UINT32_MAX, 0, 1, GDEXTENSION_PRODUCTION_LIST_CLOSE_AFTER_V1, 0 };
		const GDExtensionProductionResourceV1 resource{ GDEXTENSION_PRODUCTION_STORAGE_V1, source_origin.allocation_slot, source_origin.part,
			GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1, source_origin.allocation_generation, source_origin.bytes, 0 };
		const GDExtensionProductionUseV1 use{ 0, GDEXTENSION_PRODUCTION_READ_V1, 0, sizeof(uint32_t), 0 };
		const GDExtensionProductionReadbackSourceV1 incoming{ 0, GDEXTENSION_PRODUCTION_READBACK_RETAINED_INCOMING_V1, 0, 0,
			sizeof(uint32_t), {}, { source_origin.allocation_generation, source.backing_serial } };
		if (!source_reader_quoted) {
			const auto quote = port<GDExtensionInterfaceDeviceSessionProductionRequestRequirementsV1>("device_session_production_request_requirements_v1");
			if (!quote) { refuse(ERR_UNAVAILABLE, "source_reader_quote_port"); return; }
			source_reader_quote_result = quote(enrollment.attachment, &action, 1, &resource, 1, &use, 1, nullptr, 0, &incoming, 1, 0, 0, &source_reader_quote);
			const Error result = Error(source_reader_quote_result.native_error);
			if (result == ERR_BUSY) { return; }
			if (refuse(result, "source_reader_original_quote") != OK) { return; }
			source_reader_quoted = true; ++source_reader_steps; return;
		}
		if (!source_reader.handle_serial) {
			const auto reserve = port<GDExtensionInterfaceDeviceSessionProductionReserveReadbackSourcesV1>("device_session_production_reserve_readback_sources_v1");
			if (!reserve) { refuse(ERR_UNAVAILABLE, "source_reader_reserve_port"); return; }
			auto operation = enrollment.request->operation;
			if (operation.owner_request_nonce == UINT64_MAX) { refuse(ERR_PARAMETER_RANGE_ERROR, "source_reader_nonce_exhausted"); return; }
			++operation.owner_request_nonce;
			source_reader_reserve_snapshot = snapshot_source_reader_reserve();
			source_reader_reserve_result = reserve(enrollment.attachment, &operation, &source_reader_quote.capacity, &action, 1, &resource, 1, &use, 1,
				&incoming, 1, &source_reader);
			const Error result = Error(source_reader_reserve_result.native_error);
			if (result == ERR_BUSY) { return; }
			if (refuse(result, "source_reader_original_reserve") != OK) { return; }
			++source_reader_steps; return;
		}
		const auto import = port<GDExtensionInterfaceDeviceSessionProductionImportStorageV1>("device_session_production_import_storage_v1");
		if (!import) { refuse(ERR_UNAVAILABLE, "source_reader_import_port"); return; }
		source_reader_import_result = import(source_reader, 0, &source);
		source_reader_import_error = Error(source_reader_import_result.native_error);
		if (source_reader_import_error == ERR_BUSY) { return; }
		if (refuse(source_reader_import_error, "source_reader_original_import") != OK) { return; }
		Error found = OK; RenderingDevice *actual = nullptr; bool exact = false;
		{
			Registry::Lock lock(enrollment.registry);
			found = enrollment.registry.production_request_locked(source_reader, false, source_reader_request, actual);
			if (found == OK && source_reader_request && source_reader_request != enrollment.request && actual == device &&
					source_reader_request->attachment == enrollment.request->attachment && source_reader_request->resource_count == 1) {
				const auto &read = source_reader_request->resources[0];
				source_reader_import_snapshot["request_serial"] = int64_t(source_reader_request->serial);
				source_reader_import_snapshot["attachment_serial"] = int64_t(source_reader_request->attachment->serial);
				source_reader_import_snapshot["frozen"] = source_reader_request->frozen;
				source_reader_import_snapshot["append_effect_entered"] = source_reader_request->append_effect_entered;
				source_reader_import_snapshot["next_action"] = source_reader_request->next_action;
				source_reader_import_snapshot["graph_count"] = source_reader_request->graph_count;
				source_reader_import_snapshot["native_count"] = source_reader_request->native_count;
				source_reader_import_snapshot["resource_use_count"] = read.use_count;
				source_reader_import_snapshot["resource_imported"] = read.imported;
				source_reader_import_snapshot["resource_ownership"] = read.declaration.ownership;
				source_reader_import_snapshot["resource_rid"] = int64_t(read.primary.get_id());
				source_reader_import_snapshot["resource_backing_serial"] = int64_t(read.backing_serial);
				source_reader_rid = read.primary.get_id(); source_reader_backing_serial = read.backing_serial;
				if (source_reader_request->readback_count == 1) { source_reader_readback_issued = source_reader_request->readbacks[0].source.issued; }
				exact = read.request == source_reader_request && read.request_serial == source_reader.handle_serial && read.imported && read.use_count == 1 &&
					read.declaration.ownership == GDEXTENSION_PRODUCTION_RETAINED_PREPARED_V1 && source_reader_rid == source.rid &&
					source_reader_backing_serial == source.backing_serial && !source_reader_request->frozen && !source_reader_request->append_effect_entered &&
					source_reader_request->next_action == 0 && source_reader_request->graph_count == 0 && source_reader_request->native_count == 0 &&
					source_reader_request->readback_count == 1 && !source_reader_request->readbacks[0].source.issued;
			}
		}
		if (!exact) { refuse(found != OK ? found : ERR_INVALID_DATA, "source_reader_import_identity"); return; }
		source_import_verified = true; ++source_reader_steps;
	}

	void prefix_fail(Error p_error, const String &p_step) {
		if (prefix_error == OK) { prefix_error = p_error; prefix_detail = p_step; }
	}
	bool prefix_result(Error p_error, const String &p_step) {
		prefix_service_error = p_error;
		if (p_error == ERR_BUSY) { return false; }
		if (p_error != OK) { prefix_fail(p_error, p_step); return false; }
		return true;
	}
	void advance_prefix_retirement() {
		using namespace GDExtensionDeviceExit;
		if (prefix_complete || prefix_error != OK || prefix_advancing) { return; }
		prefix_advancing = true;
		struct ReturnAdvance { bool &active; ~ReturnAdvance() { active = false; } } return_advance{ prefix_advancing };
		if (!server || !server->is_on_render_thread() || RenderingDevice::get_singleton() != device) {
			prefix_fail(ERR_UNAUTHORIZED, "prefix_original_owner_required"); return;
		}
		if (OS::get_singleton()->get_ticks_usec() >= prefix_deadline) {
			prefix_fail(ERR_TIMEOUT, "prefix_deadline_custody_retained"); return;
		}
		++prefix_turns;
		const GDExtensionProductionServiceBudgetV1 budget{ production_dto_abi<GDExtensionProductionServiceBudgetV1>(), prefix_deadline, 1, 8, 8, 0 };
		if (prefix_step == 0) {
			// Reuse the consumed original request and its actual RECORD issuer.
			// New work is sealed only after this cleanup-only receiver returned.
			const auto schedule = port<GDExtensionInterfaceDeviceSessionProductionRecordScheduleV1>("device_session_production_record_schedule_v1");
			if (!schedule) { prefix_fail(ERR_UNAVAILABLE, "prefix_record_schedule_port"); return; }
			if (prefix_result(Error(schedule(request, &prefix_withdraw_control)), "prefix_withdraw_schedule")) { ++prefix_step; }
			return;
		}
		if (prefix_step == 1) {
			const auto consume = port<GDExtensionInterfaceDeviceSessionProductionRecordConsumeV2>("device_session_production_record_consume_v2");
			if (!consume) { prefix_fail(ERR_UNAVAILABLE, "prefix_record_consume_port"); return; }
			if (!prefix_result(Error(consume(request, prefix_withdraw_control, &prefix_withdraw_receipt).native_error), "prefix_withdraw_consume")) { return; }
			if (prefix_withdraw_receipt.request_serial != request.handle_serial || prefix_withdraw_receipt.control_serial != prefix_withdraw_control ||
					prefix_withdraw_control <= control || prefix_withdraw_calls != 1 || calls != 1 ||
					!prefix_withdraw_context_frame || prefix_withdraw_context_frame == source_record_context_frame) {
				prefix_fail(ERR_INVALID_DATA, "prefix_withdraw_receipt_identity"); return;
			}
			// An immutable non-OK return must not be replayed as another attempt.
			if (prefix_withdraw_receipt.result.native_error != OK) { prefix_fail(Error(prefix_withdraw_receipt.result.native_error), "prefix_withdraw_record_result"); return; }
			if (prefix_withdraw_status.error != OK) { prefix_fail(Error(prefix_withdraw_status.error), "prefix_withdraw_result"); return; }
			++prefix_step; return;
		}
		if (prefix_step == 2) {
			const auto cancel = port<GDExtensionInterfaceDeviceSessionProductionCancelV1>("device_session_production_cancel_v1");
			if (!cancel) { prefix_fail(ERR_UNAVAILABLE, "prefix_cancel_port"); return; }
			if (!source_reader_reclaimed && !prefix_result(Error(cancel(source_reader).native_error), "prefix_reader_cancel")) { return; }
			if (prefix_result(Error(cancel(request).native_error), "prefix_cancel")) { ++prefix_step; }
			return;
		}
		if (prefix_step == 3) {
			if (source_reader_reclaimed) { ++prefix_step; return; }
			const auto release = port<GDExtensionInterfaceDeviceSessionProductionReleaseV2>("device_session_production_release_v2");
			if (!release) { prefix_fail(ERR_UNAVAILABLE, "prefix_reader_release_port"); return; }
			GDExtensionProductionServiceBudgetV1 remaining{};
			if (!prefix_result(Error(release(source_reader, &budget, &remaining, &source_reader_release).native_error), "prefix_reader_release")) { return; }
			if (source_reader_release.service_pending || !source_reader_request || source_reader_request->serial != 0) {
				prefix_fail(ERR_INVALID_DATA, "prefix_reader_not_reclaimed"); return;
			}
			source_reader_request = nullptr; source_reader_reclaimed = true; ++prefix_step; return;
		}
		if (prefix_step == 4) {
			const auto observe = port<GDExtensionInterfaceDeviceSessionProductionObserveV3>("device_session_production_observe_v3");
			const auto returned = port<GDExtensionInterfaceDeviceSessionProductionOutputReturnConsumeV1>("device_session_production_output_return_consume_v1");
			if (!observe || !returned) { prefix_fail(ERR_UNAVAILABLE, "prefix_observe_ports"); return; }
			GDExtensionProductionObserveResultV3 observed{};
			const Error observed_error = Error(observe(request, &budget, &observed).native_error);
			prefix_service_error = observed_error;
			if (observed_error != OK && observed_error != ERR_BUSY) { prefix_fail(observed_error, "prefix_observe"); return; }
			if (!observed.output_generation.native_released) { return; }
			const auto &terminal = observed.output_generation;
			if (!source_reader_reclaimed || terminal.registry_instance != request.registry_instance || terminal.request_serial != request.handle_serial ||
					terminal.particles != created.particles || terminal.generation != created.generation || !terminal.root_serial || terminal.terminal_kind != 1) {
				prefix_fail(ERR_INVALID_DATA, "prefix_generation_terminal_identity"); return;
			}
			prefix_expected_generation = { production_dto_abi<GDExtensionProductionOutputGenerationRefV1>(), terminal.registry_instance,
				terminal.request_serial, terminal.control_serial, terminal.root_serial, terminal.particles, terminal.generation };
			if (!prefix_result(Error(returned(request, &prefix_expected_generation, &prefix_generation).native_error), "prefix_generation_consume")) { return; }
			if (!prefix_generation.native_released || prefix_generation.terminal_kind != 1 ||
					prefix_generation.registry_instance != prefix_expected_generation.registry_instance || prefix_generation.request_serial != prefix_expected_generation.request_serial ||
					prefix_generation.control_serial != prefix_expected_generation.control_serial || prefix_generation.root_serial != prefix_expected_generation.root_serial ||
					prefix_generation.particles != prefix_expected_generation.particles || prefix_generation.generation != prefix_expected_generation.generation) {
				prefix_fail(ERR_INVALID_DATA, "prefix_generation_consumed_identity"); return;
			}
			if (staged_enabled && (prefix_generation.unused_slots != uint32_t(staged_bank_refused) || prefix_generation.returned_slots != uint32_t(!staged_bank_refused) ||
					terminal.unused_slots != prefix_generation.unused_slots || terminal.returned_slots != prefix_generation.returned_slots)) {
				prefix_fail(ERR_INVALID_DATA, "staged_generation_returned_slot_count"); return;
			}
			prefix_generation_consumed = true; ++prefix_step; return;
		}
		if (prefix_step == 5) {
			// Public generations remain alive after OBSERVE's native terminal.
			// Consume that terminal before the original RS frees this exact RID.
			auto *storage = RendererRD::ParticlesStorage::get_singleton();
			const RID original_particles = RID::from_uint64(created.particles);
			// The exact consumed GenerationReleased terminal is the original
			// state proof; do not expose or manufacture a private owner state.
			if (!storage || !storage->owns_particles(original_particles) || !prefix_generation_consumed ||
					!prefix_generation.native_released || prefix_generation.terminal_kind != 1 ||
					prefix_generation.particles != created.particles || prefix_generation.generation != created.generation) {
				prefix_fail(ERR_INVALID_DATA, "prefix_public_generation_not_released"); return;
			}
			server->free_rid(original_particles);
			if (storage->owns_particles(original_particles)) { prefix_fail(ERR_BUSY, "prefix_public_rid_not_freed"); return; }
			prefix_particles_freed = true;
			{
				Registry::Lock lock(enrollment.registry);
				const auto &metadata = enrollment.request->resources[0];
				prefix_metadata_returned = metadata.request == enrollment.request && metadata.request_serial == request.handle_serial &&
					metadata.metadata_returned && !metadata.metadata_data && !metadata.metadata_consumer &&
					metadata.metadata_consumed_control_serial == prefix_generation.control_serial && metadata.metadata_root_serial == prefix_generation.root_serial &&
					metadata.metadata_consumed_particles == created.particles && metadata.metadata_consumed_generation == created.generation;
			}
			if (!prefix_metadata_returned) { prefix_fail(ERR_INVALID_DATA, "prefix_metadata_not_returned"); return; }
			++prefix_step; return;
		}
		if (prefix_step == 6) {
			if (!prefix_request_reclaimed) {
				const auto release = port<GDExtensionInterfaceDeviceSessionProductionReleaseV2>("device_session_production_release_v2");
				if (!release) { prefix_fail(ERR_UNAVAILABLE, "prefix_release_port"); return; }
				GDExtensionProductionServiceBudgetV1 remaining{};
				if (!prefix_result(Error(release(request, &budget, &remaining, &prefix_release_report).native_error), "prefix_request_release")) { return; }
				if (prefix_release_report.service_pending || !enrollment.request || enrollment.request->serial != 0) {
					prefix_fail(ERR_INVALID_DATA, "prefix_request_not_reclaimed"); return;
				}
				enrollment.request = nullptr; prefix_request_reclaimed = true; return;
			}
			// Qualification owns this source. Both actual original request releases
			// must detach their physical-use edges before the real child can retire.
			const auto retire = port<GDExtensionInterfaceDeviceSessionQualificationStorageRetireV1>("device_session_qualification_storage_retire_v1");
			const auto observe = port<GDExtensionInterfaceDeviceSessionQualificationStorageRetireObserveV1>("device_session_qualification_storage_retire_observe_v1");
			const auto release = port<GDExtensionInterfaceDeviceSessionQualificationStorageRetireReleaseV1>("device_session_qualification_storage_retire_release_v1");
			if (!retire || !observe || !release) { prefix_fail(ERR_UNAVAILABLE, "prefix_qualification_retire_ports"); return; }
			if (!qualification_retire_started) {
				const GDExtensionStorageRetireRequestV1 origin{ Enrollment::ABI, source.backing, source.rid, source.backing_serial,
					source_origin.allocation_slot, source_origin.part, source_origin.allocation_generation, 1, 0 };
				if (!prefix_result(Error(retire(enrollment.qualification, &origin, &prefix_retire, &prefix_storage).native_error), "prefix_qualification_retire")) { return; }
				qualification_retire_started = true; return;
			}
			const GDExtensionQualificationBudgetV1 qualification_budget{ Enrollment::ABI, 1, 1000, 0 };
			if (!prefix_result(Error(observe(enrollment.qualification, prefix_retire, &qualification_budget, &prefix_storage).native_error), "prefix_qualification_observe")) { return; }
			if (prefix_storage.state != 1) { return; }
			if (prefix_storage.return_status != 1 || !prefix_storage.event_seq || prefix_storage.rid != source.rid ||
					prefix_storage.backing_serial != source.backing_serial || prefix_storage.retire_cell_serial != prefix_retire.handle_serial ||
					prefix_storage.allocation_generation != source_origin.allocation_generation || prefix_storage.retire_attempt != 1 ||
					prefix_storage.reason || prefix_storage.native_error != OK || prefix_storage.native_code) {
				prefix_fail(ERR_INVALID_DATA, "prefix_qualification_return_identity"); return;
			}
			if (!prefix_result(Error(release(enrollment.qualification, prefix_retire).native_error), "prefix_qualification_release")) { return; }
			prefix_storage_released = true; ++prefix_step; return;
		}
		if (prefix_step == 7) {
			const auto close = port<GDExtensionInterfaceDeviceSessionProductionCloseV1>("device_session_production_close_v1");
			if (!close) { prefix_fail(ERR_UNAVAILABLE, "prefix_close_port"); return; }
			if (!prefix_result(Error(close(enrollment.attachment).native_error), "prefix_attachment_close")) { return; }
			enrollment.attachment = {}; prefix_attachment_closed = true; ++prefix_step; return;
		}
		if (prefix_step == 8) {
			enrollment.server = nullptr; // No recursive RT sync from original cleanup.
			enrollment.cleanup_owner(true, stdout);
			if (!enrollment.owner_cleaned) { prefix_fail(ERR_BUSY, "prefix_enrollment_not_returned"); return; }
			if (material.is_valid()) { server->free_rid(material); material = RID(); }
			if (shader.is_valid()) { server->free_rid(shader); shader = RID(); }
			if (mesh.is_valid()) { server->free_rid(mesh); mesh = RID(); }
			retained = false; prefix_complete = true; ++prefix_step;
		}
	}

	Dictionary advance(bool p_retire_prefix = false) {
		using namespace GDExtensionDeviceExit;
		if (host_scene_enabled && stage == 2 && error == OK && !prefix_requested && !host_scene_closed) {
			advance_host_scene(p_retire_prefix); return report();
		}
		if (host_scene_enabled && host_scene_error != OK) { return report(); }
		if (p_retire_prefix && !prefix_requested) {
			if (stage != 2 || error != OK || !created.particles || !created.generation || calls != 1 ||
					receipt.request_serial != request.handle_serial || receipt.control_serial != control || appended.state != GDEXTENSION_PRODUCTION_APPENDED_V1 || !source_import_verified) {
				prefix_fail(ERR_UNCONFIGURED, "prefix_source_not_recorded"); return report();
			}
			prefix_requested = true; prefix_deadline = OS::get_singleton()->get_ticks_usec() + 20000000;
		}
		if (prefix_requested) { advance_prefix_retirement(); return report(); }
		if (error != OK || stage == 2) { return report(); }
		if (!server || !server->is_on_render_thread() || RenderingDevice::get_singleton() != device) { refuse(ERR_UNAUTHORIZED, "producer_advance_owner"); return report(); }
		if (OS::get_singleton()->get_ticks_usec() >= deadline) { refuse(ERR_TIMEOUT, "producer_deadline_retained"); return report(); }
		if (preparation == GEOMETRY_WARMING) {
			if (device->frames[device->frame].qualification_epoch == warmup_start_epoch ||
					device->draw_list.active || device->compute_list.active || device->raytracing_list.active) { return report(); }
			const Error warmed = warm_geometry();
			if (warmed != OK && warmed != ERR_BUSY) { refuse(warmed, "producer_geometry_warmup"); }
			return report(); // Publish readiness before the caller stops render loop.
		}
		if (preparation == GEOMETRY_READY) {
			if (server->is_render_loop_enabled()) { return report(); }
			const Error enrolled = finish_begin();
			if (enrolled != OK && enrolled != ERR_BUSY) { refuse(enrolled, "producer_finish_begin"); }
			return report(); // Accepted source starts on a later outside-list call.
		}
		if (stage == 0) {
			const auto storage_create = port<GDExtensionInterfaceDeviceSessionQualificationStorageCreateV1>("device_session_qualification_storage_create_v1");
			const auto import = port<GDExtensionInterfaceDeviceSessionProductionImportStorageV1>("device_session_production_import_storage_v1");
			const auto upload = port<GDExtensionInterfaceDeviceSessionProductionPrepareUploadV2>("device_session_production_prepare_upload_v2");
			const auto schedule = port<GDExtensionInterfaceDeviceSessionProductionRecordScheduleV1>("device_session_production_record_schedule_v1");
			if (!storage_create || !import || !upload || !schedule) { refuse(ERR_UNAVAILABLE, "producer_source_ports"); return report(); }
			if (!source.backing.handle_serial) {
				const auto stored = storage_create(enrollment.qualification, &source_origin, &source);
				if (stored.native_error == ERR_BUSY) { return report(); }
				if (refuse(Error(stored.native_error), "source_qualification_storage_create") != OK) { return report(); }
			}
			if (source_producer_import_error != OK) {
				source_producer_import_error = Error(import(request, 1, &source).native_error);
				if (source_producer_import_error == ERR_BUSY) { return report(); }
				if (refuse(source_producer_import_error, "source_producer_original_import") != OK) { return report(); }
			}
			// Both requests reserve their original command slices before checked
			// source upload grows the RD prefix. Import is not source readiness.
			if (!source_import_verified) { advance_source_import_probe(); return report(); }
			const GDExtensionProductionUploadV2 write{ production_dto_abi<GDExtensionProductionUploadV2>(), 0, 11, 7, 0, 1, 0, 0, sizeof(source_words) };
			const auto prepared = upload(request, &write, source_words, sizeof(source_words));
			if (refuse(Error(prepared.native_error), "source_prepare_upload") != OK) { return report(); }
			if (refuse(Error(schedule(request, &control)), "producer_record_schedule") != OK) { return report(); }
			stage = 1; return report();
		}
		const auto consume = port<GDExtensionInterfaceDeviceSessionProductionRecordConsumeV2>("device_session_production_record_consume_v2");
		if (!consume) { refuse(ERR_UNAVAILABLE, "producer_consume_port"); return report(); }
		const auto consumed = consume(request, control, &receipt);
		if (consumed.native_error == ERR_BUSY) { return report(); }
		if (refuse(Error(consumed.native_error), "producer_record_consume") != OK) { return report(); }
		if (refuse(Error(receipt.result.native_error), "producer_record_result") != OK) { return report(); }
		if (receipt.request_serial != request.handle_serial || receipt.control_serial != control || calls != 1 || !created.particles || !created.generation || appended.state != GDEXTENSION_PRODUCTION_APPENDED_V1) {
			refuse(ERR_INVALID_DATA, "producer_receipt_identity"); return report();
		}
		const auto &append = enrollment.request->actions[0].original_append;
		if (!append.handle.is_valid() || !append.operation_bound || append.operation.owner_request_nonce != enrollment.request->operation.owner_request_nonce || append.operation.record_generation != enrollment.request->operation.record_generation) {
			refuse(ERR_INVALID_DATA, "producer_checked_original_append"); return report();
		}
		const auto &original_source = enrollment.request->resources[1];
		const auto *backing = device->storage_buffer_owner.get_or_null(RID::from_uint64(source.rid));
		if (original_source.request != enrollment.request || original_source.request_serial != request.handle_serial ||
				original_source.primary.get_id() != source.rid || original_source.backing_serial != source.backing_serial ||
				!original_source.imported || original_source.use_count != 1 ||
				!backing || !qualification_source_matches() || backing->backing_serial != source.backing_serial || backing->size != sizeof(source_words)) {
			refuse(ERR_INVALID_DATA, "producer_original_physical_source"); return report();
		}
		stage = 2;
		return report();
	}
	Dictionary report() const {
		Dictionary result;
		report_host_scene(result);
		if (staged_enabled) {
			report_staged(result);
			result["staged_material_cold_start"] = staged_material_cold_start;
			result["staged_selection_facts"] = selection_facts_report();
			result["staged_transparent_profile"] = staged_transparent_profile();
			result["staged_material_pass_mask"] = material_output_pass_mask;
			result["staged_create_pass_mask"] = create_input.config.pass_mask;
			result["staged_material_recipe"] = selection_facts_material.material_recipe_rejections;
		}
		result["authority"] = "renderer-fixture-enrollment-only-not-runtime-u05";
		result["stage"] = stage; result["error"] = error; result["detail"] = detail;
		result["geometry_readbacks"] = geometry_readbacks;
		result["geometry_transfer_worker_before"] = geometry_worker_before;
		result["geometry_transfer_worker_after"] = geometry_worker_after;
		result["geometry_worker_check_error"] = geometry_worker_check_error;
		result["geometry_worker_pending_preparation"] = geometry_worker_pending;
		result["geometry_native_selection_error"] = geometry_selection_error;
		result["geometry_native_source_error"] = geometry_source_error;
		result["compiled_material_output_pass_mask"] = material_output_pass_mask;
		result["preparation_phase"] = preparation == GEOMETRY_WARMING ? "geometry_warming" : preparation == GEOMETRY_READY ? "geometry_ready" : "enrollment_ready";
		result["geometry_ready"] = preparation != GEOMETRY_WARMING;
		result["enrollment_ready"] = preparation == ENROLLMENT_READY;
		result["warmup_start_draws"] = int64_t(warmup_start_draws);
		result["warmup_start_epoch"] = int64_t(warmup_start_epoch);
		result["geometry_ready_draws"] = int64_t(geometry_ready_draws);
		result["geometry_worker_polls"] = geometry_worker_polls;
		result["geometry_worker_poll_error"] = geometry_worker_poll_error;
		if (staged_enabled) {
			Dictionary indexed = staged_geometry.duplicate(true);
			indexed["index_worker_before"] = staged_index_worker_before;
			indexed["index_worker_after"] = staged_index_worker_after;
			indexed["index_worker_check_error"] = staged_index_worker_check;
			indexed["index_worker_poll_error"] = staged_index_worker_poll;
			indexed["index_worker_polls"] = staged_index_worker_polls;
			result["staged_geometry"] = indexed;
		}
		result["source_recorded"] = stage == 2 && error == OK;
		result["source_ready_for_original_import"] = stage == 2 && error == OK && !prefix_requested && source_import_verified &&
			receipt.request_serial == request.handle_serial && receipt.control_serial == control && calls == 1 &&
			appended.state == GDEXTENSION_PRODUCTION_APPENDED_V1;
		result["source_family"] = "original_qualification_bridge_child";
		result["component_scope"] = true; result["runtime_u05"] = false;
		result["source_import_verified"] = source_import_verified;
		result["source_import_probe_only"] = true; result["output_request_prepared"] = false;
		result["source_producer_import_error"] = source_producer_import_error;
		result["source_reader_import_error"] = source_reader_import_error;
		result["source_reader_request_serial"] = int64_t(source_reader.handle_serial);
		result["source_reader_rid"] = int64_t(source_reader_rid);
		result["source_reader_backing_serial"] = int64_t(source_reader_backing_serial);
		result["source_reader_import_steps"] = source_reader_steps;
		result["source_reader_reclaimed"] = source_reader_reclaimed;
		result["source_reader_release_pending"] = source_reader_release.service_pending;
		result["source_reader_release_cause"] = source_reader_release.cause.native_error;
		result["source_reader_same_attachment"] = source_import_verified;
		result["source_reader_readback_issued"] = bool(source_reader_readback_issued);
		result["source_attachment_serial"] = int64_t(source_attachment_serial);
		result["source_attachment_requests"] = source_attachment_capacity.requests;
		result["source_attachment_graphs"] = source_attachment_capacity.graph_memberships;
		result["source_attachment_native_memberships"] = source_attachment_capacity.native_memberships;
		result["source_reader_reserve_snapshot"] = source_reader_reserve_snapshot.duplicate(true);
		result["source_reader_import_snapshot"] = source_reader_import_snapshot.duplicate(true);
		const auto service_result = [](const GDExtensionDeviceSessionResultV1 &value) {
			Dictionary result;
			result["native_error"] = value.native_error; result["reason"] = value.reason;
			result["expected"] = int64_t(value.expected); result["actual"] = int64_t(value.actual);
			return result;
		};
		result["source_reader_quote_result"] = service_result(source_reader_quote_result);
		result["source_reader_reserve_result"] = service_result(source_reader_reserve_result);
		result["source_reader_import_result"] = service_result(source_reader_import_result);
		result["source_pool_quote_current_commands"] = source_pool_quote_current_commands;
		result["source_attachment_command_slots"] = int64_t(source_attachment_command_slots);
		result["source_attachment_frame_slots"] = int64_t(source_attachment_frame_slots);
		result["source_producer_command_slots"] = int64_t(source_producer_command_slots);
		result["source_producer_frame_slots"] = int64_t(source_producer_frame_slots);
		result["source_commands_before_append"] = source_commands_before_append;
		result["source_commands_after_append"] = source_commands_after_append;
		result["source_reader_quote_graph_slots"] = int64_t(source_reader_quote.graph_slots);
		result["source_reader_quote_graph_frames"] = int64_t(source_reader_quote.graph_frames);
		result["source_reader_quote_graphs"] = source_reader_quote.capacity.graph_memberships;
		result["source_reader_quote_native_memberships"] = source_reader_quote.capacity.native_memberships;
		result["source_reader_quote_readback_bytes"] = int64_t(source_reader_quote.capacity.readback_bytes);
		result["source_qualification_retire_started"] = qualification_retire_started;
		result["source_qualification_bridge_serial"] = int64_t(enrollment.qualification.handle_serial);
		result["source_qualification_child_serial"] = int64_t(source.backing.handle_serial);
		result["request_serial"] = int64_t(request.handle_serial); result["control_serial"] = int64_t(control);
		result["receipt_request_serial"] = int64_t(receipt.request_serial); result["receipt_control_serial"] = int64_t(receipt.control_serial);
		result["append_boundary_before"] = append_boundary_before.duplicate(true);
		result["append_boundary_after"] = append_boundary_after.duplicate(true);
		result["last_append_state"] = appended.state;
		result["last_append_cause"] = appended.cause.native_error;
		result["last_append_graph_fact_count"] = appended.graph_fact_count;
		result["particles"] = int64_t(created.particles); result["generation"] = int64_t(created.generation);
		result["source_rid"] = int64_t(source.rid); result["source_backing_serial"] = int64_t(source.backing_serial);
		result["source_semantic_words"] = PackedInt32Array({ 1, 0, 0, 0, 0 });
		result["source_semantic_word_offsets"] = PackedInt32Array({ 0, 1, 2, 3, 4 });
		result["source_semantic_contract"] = "Count=1; CountValidity/AttributesValidity/DenseIdsValidity/InstanceGate=fault==0";
		result["diagnostic_custody_retained"] = retained;
		result["source_record_context_frame"] = int64_t(source_record_context_frame);
		result["prefix_retire_requested"] = prefix_requested;
		result["prefix_retire_step"] = prefix_step; result["prefix_retire_turns"] = prefix_turns;
		result["prefix_retire_error"] = prefix_error; result["prefix_retire_detail"] = prefix_detail;
		result["prefix_retire_service_error"] = prefix_service_error;
		result["prefix_retire_complete"] = prefix_complete;
		result["prefix_withdraw_control"] = int64_t(prefix_withdraw_control);
		result["prefix_withdraw_calls"] = prefix_withdraw_calls;
		result["prefix_withdraw_context_frame"] = int64_t(prefix_withdraw_context_frame);
		result["prefix_withdraw_status"] = prefix_withdraw_status.error;
		result["prefix_withdraw_record_result"] = prefix_withdraw_receipt.result.native_error;
		result["prefix_withdraw_receipt_request_serial"] = int64_t(prefix_withdraw_receipt.request_serial);
		result["prefix_withdraw_receipt_control_serial"] = int64_t(prefix_withdraw_receipt.control_serial);
		result["prefix_storage_state"] = prefix_storage.state;
		result["prefix_storage_return_status"] = prefix_storage.return_status;
		result["prefix_storage_rid"] = int64_t(prefix_storage.rid);
		result["prefix_storage_backing_serial"] = int64_t(prefix_storage.backing_serial);
		result["prefix_storage_retire_cell_serial"] = int64_t(prefix_storage.retire_cell_serial);
		result["prefix_storage_frame_slot"] = prefix_storage.frame_slot;
		result["prefix_storage_frame_epoch"] = int64_t(prefix_storage.frame_epoch);
		result["prefix_storage_event_seq"] = int64_t(prefix_storage.event_seq);
		result["prefix_storage_allocation_generation"] = int64_t(prefix_storage.allocation_generation);
		result["prefix_storage_retire_attempt"] = int64_t(prefix_storage.retire_attempt);
		result["prefix_storage_native_error"] = prefix_storage.native_error;
		result["prefix_storage_released"] = prefix_storage_released;
		const auto generation_tuple = [](const auto &value) {
			Dictionary tuple;
			tuple["registry_instance"] = int64_t(value.registry_instance); tuple["request_serial"] = int64_t(value.request_serial);
			tuple["control_serial"] = int64_t(value.control_serial); tuple["root_serial"] = int64_t(value.root_serial);
			tuple["particles"] = int64_t(value.particles); tuple["generation"] = int64_t(value.generation);
			return tuple;
		};
		result["prefix_generation_expected"] = generation_tuple(prefix_expected_generation);
		Dictionary terminal = generation_tuple(prefix_generation);
		terminal["native_released"] = prefix_generation.native_released;
		terminal["terminal_kind"] = prefix_generation.terminal_kind;
		terminal["unused_slots"] = prefix_generation.unused_slots;
		terminal["returned_slots"] = prefix_generation.returned_slots;
		terminal["known_obligations"] = prefix_generation.root_source.known_obligations;
		terminal["possible_obligations"] = prefix_generation.root_source.possible_obligations;
		terminal["complete_obligations"] = prefix_generation.root_source.complete_obligations;
		terminal["physical_returns_pending"] = prefix_generation.root_source.physical_returns_pending;
		result["prefix_generation_terminal"] = terminal;
		result["prefix_generation_consumed"] = prefix_generation_consumed;
		result["prefix_particles_freed"] = prefix_particles_freed;
		result["prefix_metadata_returned"] = prefix_metadata_returned;
		result["prefix_release_pending"] = prefix_release_report.service_pending;
		result["prefix_release_cause"] = prefix_release_report.cause.native_error;
		result["prefix_request_reclaimed"] = prefix_request_reclaimed;
		result["prefix_attachment_closed"] = prefix_attachment_closed;
		result["prefix_enrollment_returned"] = enrollment.owner_cleaned;
		// The bridge sets these only after the real embedded RD destructor returns.
		result["prefix_object_destroyed"] = false; result["prefix_physical_retirement"] = false;
		result["full_acceptance"] = false;
		// Source upload is an authentic native recording, not GPU completion or output acceptance.
		result["complete"] = false; result["pass"] = false;
		result["visible_output"] = false; result["sealed"] = false; result["physical_retirement"] = false;
		result["next_stage"] = prefix_requested ? "qualification source prefix retirement selected; full lifecycle requires a fresh source continuation" : "same-attachment physical source imported by probe; reclaim probe before real output request/Host/scene/camera/Acquire/Records/Finalize/Seal";
		return result;
	}
};
