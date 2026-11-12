// Fixture-only: original SingletonEnrollment scaffolding supplies typed entry
// custody. It does not prove Runtime HostControlService/U05 qualification.
struct HostViewGpuHarness {
 SingletonEnrollment enrollment;
 RenderingDevice *device = nullptr;
 RID viewport, camera;
 Callable callable;
 GDExtensionNormalRelayRefV1 wire{};
 GDExtensionProductionRequestRefV1 ref{};
 GDExtensionProductionHostViewAssociationV1 association{};
 GDExtensionProductionCameraSampleV1 sample{};
 Dictionary checks;
 uint64_t control = 0;
 uint32_t stage = 0, calls = 0, frees = 0;
 bool replacement = false, closing = false;
 Error owner_read = ERR_UNAVAILABLE;
 GDExtensionInterfaceDeviceSessionProductionReadHostViewV1 read_view = nullptr;
 void check(const char *name, bool value) { checks[name] = value; }
 template <class T> static T runtime_port(const char *name) {
  // Ordinary --script execution has no doctest Context. The original
  // ordinal_port CHECK_MESSAGE constructs AssertData even for a valid entry.
  return reinterpret_cast<T>(GDExtension::get_interface_function(name));
 }
 static void ordinary(void *opaque, const GDExtensionConstVariantPtr *, GDExtensionInt, GDExtensionVariantPtr returned, GDExtensionCallError *error) {
  auto &self = *static_cast<HostViewGpuHarness *>(opaque);
  self.owner_read = Error(self.read_view(self.ref, 0, &self.association).native_error);
  *reinterpret_cast<Variant *>(returned) = Variant();
  *error = { GDEXTENSION_CALL_OK, 0, 0 };
 }
 static void free_callable(void *opaque) { ++static_cast<HostViewGpuHarness *>(opaque)->frees; }
 static void typed(void *opaque, GDExtensionProductionAttachmentRefV1 attachment, const GDExtensionProductionCameraSampleV1 *p_sample) {
  using namespace GDExtensionDeviceExit;
  auto &self = *static_cast<HostViewGpuHarness *>(opaque);
  ++self.calls;
  const auto *scope = ProductionCameraReadySourceScope::get();
  self.check("typed_original_scope", scope && scope->get_registry() == &self.enrollment.registry && scope->get_source().is_valid() && attachment.handle_serial == self.enrollment.attachment.handle_serial);
  if (!scope || scope->get_source().is_null() || !p_sample) { return; }
  const auto &capture = scope->get_source();
  self.sample = *p_sample;
  const auto &action = self.enrollment.request->actions[0];
  self.check("paid_action_retained_before_leaf", action.preinput_camera_source == capture && !memcmp(&action.preinput_camera_sample, p_sample, sizeof(*p_sample)));
  self.check("real_camera_invocation", capture->get_viewport() == self.viewport && capture->get_invocation().camera == self.camera && capture->get_invocation().render_buffers.is_valid());
  self.check("real_rd_buffers", Object::cast_to<RenderSceneBuffersRD>(capture->get_invocation().render_buffers.ptr()) != nullptr);
  const auto *live = RSG::viewport->viewport_owner.get_or_null(self.viewport);
  self.check("production_cache_exact_source", live && live->captured_camera == capture);
  self.check("native_issued_key", live && live->native_presentation_issued && live->presentation_registered && live->presentation_key.view_id == p_sample->camera.view_id && live->presentation_key.view_epoch == p_sample->camera.view_epoch && live->presentation_key.present_seq == p_sample->present_seq);
  self.check("real_capture_complete_key", p_sample->camera.view_id && p_sample->camera.view_epoch && p_sample->camera.capture_ordinal && p_sample->present_seq && capture->get_token().capture_ordinal == p_sample->camera.capture_ordinal && capture->get_presentation_key().present_seq == p_sample->present_seq);
  self.check("not_a_record_permission", Registry::production_physical_frame == nullptr && !action.prepared && !self.enrollment.request->frozen);
 }
 void invoke_owner_read() {
  Variant returned;
  Callable::CallError error;
  callable.callp(nullptr, 0, returned, error); // Original adopted normal entry.
  check("read_normal_entry", error.error == Callable::CallError::CALL_OK);
 }
 Error queue(GDExtensionDeviceExit::ProductionControlKind kind) {
  using namespace GDExtensionDeviceExit;
  const GDExtensionProductionServiceBudgetV1 budget{ production_dto_abi<GDExtensionProductionServiceBudgetV1>(), OS::get_singleton()->get_ticks_usec() + 10000000, 1, 1, 1, 0 };
  Error error = enrollment.registry.production_control_accept(ref, kind, &control, &budget);
  return error == OK ? enrollment.registry.production_control_schedule(ref, control) : error;
 }
 Error begin(RID p_viewport, RID p_camera, bool p_replacement) {
  using namespace GDExtensionDeviceExit;
  viewport = p_viewport; camera = p_camera; replacement = p_replacement;
  device = RenderingDevice::get_singleton();
  auto *server = Object::cast_to<RenderingServerDefault>(RenderingServer::get_singleton());
  if (!device || !server || !server->is_on_render_thread() || !device->native_immutable_is_render_thread() || !RSG::viewport || !RSG::scene) { return ERR_UNAVAILABLE; }
  auto *live = RSG::viewport->viewport_owner.get_or_null(viewport);
  if (!live || live->camera != camera || live->presentation_registered || !RSG::scene->is_camera(camera)) { return ERR_INVALID_DATA; }
  enrollment.server = server;
  OrdinalPlan plan;
  plan.capacity = { production_dto_abi<GDExtensionProductionCapacityV1>(), 1, 1, 0, 0, 1, 0, 0, 0, 0, 0 };
  { MutexLock registry_lock(enrollment.registry.mutex); MutexLock rd_lock(device->_thread_safe_);
   Error bounded = enrollment.registry.production_membership_bound_locked(*device, 1, device->transfer_worker_pool_max_size, plan.capacity.native_memberships);
   if (bounded != OK) { return bounded; }
  }
  plan.action_count = 1; plan.resource_count = plan.use_count = plan.readback_count = 0;
  plan.actions[0] = { 0, 11, 7, 0, GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1, UINT32_MAX, 0, 0, GDEXTENSION_PRODUCTION_LIST_CONTINUE_V1, UINT32_MAX };
  Error error = enrollment.initialize_owner(1, false, false, +[](void *, ProductionPhysicalContextRef, GDExtensionProductionRequestRefV1, const GDExtensionProductionOperationV1 *) {}, nullptr, false, 1, false, +[](void *, GDExtensionProductionAttachmentRefV1) {}, false, false, false, false, &plan, device);
  if (error != OK) { return error; }
  ref = { enrollment.instance, enrollment.request->serial };
  { MutexLock lock(enrollment.registry.mutex); enrollment.registry.records[enrollment.reference.slot].guard.entry_classes |= 1u << 8; }
  GDExtensionCallableCustomInfo2 info{};
  info.callable_userdata = this; info.call_func = &ordinary; info.free_func = &free_callable;
  info.token = enrollment.registry.records[enrollment.reference.slot].original_owner;
  const auto create = reinterpret_cast<GDExtensionInterfaceCallableCustomCreate2>(GDExtension::get_interface_function("callable_custom_create2"));
  const auto relay = runtime_port<GDExtensionInterfaceDeviceSessionCameraReadyRelayV1>("device_session_camera_ready_relay_v1");
  const auto bind = runtime_port<GDExtensionInterfaceDeviceSessionCameraReadyBindRelayV1>("device_session_camera_ready_bind_relay_v1");
  const auto bind_request = runtime_port<GDExtensionInterfaceDeviceSessionCameraReadyBindRequestV1>("device_session_camera_ready_bind_request_v1");
  const auto prepare = runtime_port<GDExtensionInterfaceDeviceSessionProductionPrepareHostViewV1>("device_session_production_prepare_host_view_v1");
  read_view = runtime_port<GDExtensionInterfaceDeviceSessionProductionReadHostViewV1>("device_session_production_read_host_view_v1");
  if (!create || !relay || !bind || !bind_request || !prepare || !read_view) { return ERR_UNAVAILABLE; }
  create(reinterpret_cast<GDExtensionUninitializedTypePtr>(&callable), &info);
  error = Error(relay(reinterpret_cast<GDExtensionConstTypePtr>(&callable), &typed, &wire).native_error);
  if (error != OK) { return error; }
  error = Error(bind(enrollment.attachment, wire).native_error);
  if (error != OK) { return error; }
  const GDExtensionProductionHostViewInputV1 input{ production_dto_abi<GDExtensionProductionHostViewInputV1>(), viewport.get_id(), camera.get_id() };
  error = Error(prepare(ref, 0, &input).native_error);
  if (error != OK) { return error; }
  check("prepare_retry_exact", prepare(ref, 0, &input).native_error == OK);
  check("prepare_no_native_key", !live->presentation_registered && live->captured_camera.is_null());
  error = Error(bind_request(ref, 0, wire, viewport.get_id()).native_error);
  if (error != OK) { return error; }
  check("paid_embedded_subscriber", enrollment.request->actions[0].request == enrollment.request && enrollment.request->actions[0].declaration.action_ordinal == 0 && live->camera_source_subscriber_count == 0);
  enrollment.frame.leave(); // No fixture TLS survives between native frames.
  return queue(ProductionControlKind::OBSERVE);
 }
 void request_close() { closing = true; }
 Dictionary advance() {
  using namespace GDExtensionDeviceExit;
  if (stage == 0 || stage == 3) {
   ProductionControlOutput output{};
   const Error consumed = enrollment.registry.production_control_consume(ref, control, &output);
   if (consumed == ERR_BUSY) { return report(); }
   CRASH_COND_MSG(consumed != OK, "HostView native fixture control consume failed");
   if (stage == 0) {
    check("observe_actual_rt_install", output.service_usage.passes == 1 && output.service_usage.receipts == 1 && RSG::viewport->viewport_owner.get_or_null(viewport)->camera_source_subscribers == &enrollment.request->actions[0].camera_ready_subscriber);
    invoke_owner_read();
    check("association_before_capture", owner_read == OK && association.viewport_rid == viewport.get_id() && association.camera_rid == camera.get_id() && association.view_id && association.view_epoch && !association.present_seq && !association.capture_ordinal && calls == 0);
    stage = 1;
   } else if (output.result.native_error == OK && output.owner_return.native_error == OK) {
    // owner_return is zero-initialized when RT RELEASE returns BUSY. Only
    // the successful RT result plus actual owner reclaim completes this turn.
    CRASH_COND_MSG(output.request_serial != ref.handle_serial || output.control_serial != control || !enrollment.request || enrollment.request->serial != 0, "HostView RELEASE did not reclaim the original request");
    check("release_original_request_reclaimed", true);
    std::fprintf(stderr, "HOST_VIEW_RELEASE result=%d owner=%d\n", int(output.result.native_error), int(output.owner_return.native_error));
    enrollment.runtime_cleanup_snapshot("release_consumed");
    enrollment.request = nullptr;
    GDExtensionCameraReadyCloseV1 closed{};
    const auto close = runtime_port<GDExtensionInterfaceDeviceSessionCameraReadyCloseV1>("device_session_camera_ready_close_v1");
    CRASH_COND(!close);
    const Error closing_result = Error(close(enrollment.attachment, wire, &closed).native_error);
    check("relay_close_waits_callable", closing_result == ERR_BUSY && closed.entry_closed && !closed.active_deliveries);
    enrollment.runtime_cleanup_snapshot("after_relay_entry_close");
    callable = Callable();
    enrollment.runtime_cleanup_snapshot("after_callable_release");
    check("relay_callable_freed", frees == 1);
    const Error closed_result = Error(close(enrollment.attachment, wire, &closed).native_error);
    check("relay_actual_close", closed_result == OK && closed.relay_cell_released && closed.relay_holder_closed);
    CRASH_COND(closed_result != OK);
    enrollment.runtime_cleanup_snapshot("after_relay_close");
    enrollment.server = nullptr; // Do not sync from currently executing RT.
    enrollment.cleanup_owner(true);
    check("original_enrollment_returned", enrollment.owner_cleaned);
    stage = 4;
    return report();
   } else {
    // A fresh bounded control continues the same live request; preserve its
    // pointer until successful consume has actually reset the original slot.
    CRASH_COND_MSG(output.request_serial != ref.handle_serial || output.control_serial != control || !enrollment.request || enrollment.request->serial != ref.handle_serial, "HostView RELEASE retry lost the original request");
    CRASH_COND_MSG((output.result.native_error != OK && output.result.native_error != ERR_BUSY) || (output.result.native_error == OK && output.owner_return.native_error != ERR_BUSY), "HostView RELEASE returned a terminal error");
    CRASH_COND(queue(ProductionControlKind::RELEASE) != OK);
   }
  }
  if (stage == 1 && calls) {
   invoke_owner_read();
   check("association_exact_sample", owner_read == OK && calls == 1 && association.view_id == sample.camera.view_id && association.view_epoch == sample.camera.view_epoch && association.present_seq == sample.present_seq && association.capture_ordinal == sample.camera.capture_ordinal);
   stage = 2;
  }
  if (stage == 2 && closing) {
   if (replacement) {
    association.capture_ordinal = 999;
    invoke_owner_read();
    check("replacement_closes_association", owner_read == ERR_UNAVAILABLE && association.capture_ordinal == 999 && enrollment.request->actions[0].camera_ready_subscriber.is_closed());
    check("replacement_preserves_paid_sample", enrollment.request->actions[0].preinput_camera_source.is_valid() && !memcmp(&enrollment.request->actions[0].preinput_camera_sample, &sample, sizeof(sample)) && calls == 1);
   }
   check("cancel_original_request", enrollment.registry.production_cancel(ref).native_error == OK);
   association.capture_ordinal = 999;
   invoke_owner_read();
   check("cancel_closes_read", owner_read == ERR_BUSY && association.capture_ordinal == 999);
   CRASH_COND(queue(ProductionControlKind::RELEASE) != OK);
   stage = 3;
  }
  return report();
 }
 Dictionary report() const {
  Dictionary result;
  result["checks"] = checks; result["stage"] = stage; result["calls"] = calls;
  result["authority"] = "fixture-enrollment-only-not-runtime-u05";
  result["view_id"] = int64_t(sample.camera.view_id); result["view_epoch"] = int64_t(sample.camera.view_epoch);
  result["present_seq"] = int64_t(sample.present_seq); result["capture_ordinal"] = int64_t(sample.camera.capture_ordinal);
  bool passed = stage == 4;
  for (const Variant &key : checks.keys()) { passed &= bool(checks[key]); }
  result["pass"] = passed;
  return result;
 }
};
