/**************************************************************************/
/*  gdextension_qualification_ticket.cpp                                  */
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

#include "core/crypto/crypto_core.h"
#include "core/extension/gdextension.h"
#include "core/extension/gdextension_qualification_internal.h"
#include "core/os/os.h"
#include "core/os/thread.h"
#include "servers/rendering/rendering_server_default.h"

namespace GDExtensionDeviceExit {
namespace {
GDExtensionDeviceSessionResultV1 result(Error error,uint32_t reason=0){return {int32_t(error),reason,0,0};}
template<class T> bool aligned(const T *p){return p && uintptr_t(p)%alignof(T)==0;}
bool separate(const void *a,size_t as,const void *b,size_t bs){auto x=uintptr_t(a),y=uintptr_t(b);return x<=y?as<=y-x:bs<=x-y;}
struct RdUnlock {Mutex *mutex=nullptr;~RdUnlock(){if(mutex)mutex->unlock();}};
bool valid_abi(const GDExtensionDeviceExitAbi &abi){return abi.abi_version==1 && abi.struct_size==sizeof(abi) && !abi.reserved;}
bool add_resource(LocalVector<RID> &resources,RID rid){for(RID existing:resources)if(existing==rid)return true;if(resources.size()>=128)return false;resources.push_back(rid);return true;}
Error qualification_native_capacity(RenderingDeviceDriver &driver, uint32_t current_commands, uint64_t graph_bound, uint32_t workers) {
	const uint64_t capacity = QualificationTicket::FACT_CAPACITY;
	if (graph_bound > capacity) return ERR_PARAMETER_RANGE_ERROR;
	// Each declared graph can consume a separate continuation frame. Include
	// one initial failure/membership frame and a fresh hidden-worker set for
	// each of those frames, rather than counting workers only once.
	uint32_t slots = 0;
	const Error arithmetic = qualification_source_slot_bound(graph_bound, current_commands, workers, uint32_t(capacity), slots);
	if (arithmetic != OK) {
		return arithmetic;
	}
	// Worker submissions have no wait semaphores at the original RD callsite.
	// Keep every worker in every frame; expose that call shape to the backend.
	const uint32_t frame_bound = uint32_t(graph_bound + 1);
	const uint32_t main_slots = current_commands + frame_bound;
	const uint32_t native_bound = driver.qualification_native_call_bound_for_profile(main_slots, slots - main_slots, frame_bound);
#ifdef TESTS_ENABLED
	if (OS::get_singleton()->has_environment("GODOT_QUALIFICATION_CAPACITY_TRACE")) {
		print_line(vformat("QCAP native current=%d graphs=%d workers=%d frames=%d main_slots=%d worker_slots=%d total_slots=%d generic_bound=%d profile_bound=%d capacity=%d", current_commands, graph_bound, workers, frame_bound, main_slots, slots - uint32_t(main_slots), slots, driver.qualification_native_call_bound(slots), native_bound, capacity));
	}
#endif
	if (!native_bound) return ERR_UNAVAILABLE;
	if (native_bound > capacity - graph_bound) return ERR_PARAMETER_RANGE_ERROR;
	return OK;
}

void add_graph(QualificationTicket *ticket,const RDG::RecordHandle &handle,uint32_t semantic,uint32_t frame,uint64_t epoch,uint64_t backing=0,uint64_t offset=0,uint64_t size=0) {
 if(!handle.is_valid() || ticket->facts.size()>=QualificationTicket::FACT_CAPACITY){ticket->mapping_failed=true;return;}
 GDExtensionQualificationFactV1 fact={};fact.graph_instance=handle.graph_instance;fact.graph_frame=handle.graph_frame;fact.graph_kind=handle.kind;fact.graph_index=handle.index;fact.graph_offset=handle.offset;
 fact.semantic_kind=semantic;fact.command_slot=UINT32_MAX;fact.parent_fact=UINT32_MAX;fact.phase=1;fact.source_backing_serial=backing;fact.source_offset=offset;fact.source_size=size;
 ticket->graphs.push_back({handle,ticket->facts.size(),frame,0,epoch,false,false});ticket->facts.push_back(fact);ticket->observation_dirty=true;
}
} //namespace
Error Registry::qualification_typed_upload_capacity(QualificationTicket *ticket, RenderingDevice *rd, uint32_t rows) {
	uint64_t bound = 4 + (ticket->preparation.expected_result_bytes + 31) / 32 + rows;
	for (const auto &upload : ticket->uploads) {
		bound += (upload.plan.byte_size + 31) / 32;
	}
	for (auto *image : ticket->texture_uploads) {
		bound += image->upload.max_segments;
	}
	return qualification_native_capacity(*rd->driver, rd->draw_graph.qualification_command_count(), bound, rd->transfer_worker_pool_max_size);
}
void Registry::qualification_add_upload_graph(QualificationTicket *ticket, RenderingDevice *rd, uint64_t instance, uint64_t frame, uint32_t kind, uint32_t index, uint32_t offset, uint64_t serial, uint64_t begin, uint64_t size) {
	add_graph(ticket, { instance, frame, kind, index, offset }, 6, rd->frame, rd->frames[rd->frame].qualification_epoch, serial, begin, size);
	rd->_qualification_add_frame_ticket(ticket);
}
Error Registry::qualification_ticket_locked(QualificationBridge *bridge,GDExtensionQualificationTicketRefV1 ref,QualificationTicket *&ticket) {
 if(ref.registry_instance!=instance || !ref.handle_serial)return ERR_INVALID_DATA;
 for(auto *existing:bridge->tickets)if(existing && existing->serial==ref.handle_serial && !existing->acknowledged){ticket=existing;return OK;}
 return ERR_DOES_NOT_EXIST;
}
Error Registry::qualification_record_preflight(QualificationBridge *bridge, RenderingDevice *rd, const GDExtensionQualificationRecordV1 &request, LocalVector<RID> &resources, uint64_t capture_set1, uint64_t body_set1, bool p_allow_empty) {
	if (!valid_abi(request.abi) || !request.record_generation || !request.request_nonce || request.library_generation != records[bridge->library].snapshot.library_generation || !request.qualification_session_serial || !request.attempt || (!p_allow_empty && !request.sample_count) || request.sample_count > 512 || request.range_end < request.range_begin || request.range_end - request.range_begin != request.sample_count || request.body_groups_x != (p_allow_empty ? MAX(1u, (request.sample_count + 63) / 64) : (request.sample_count + 63) / 64) || !request.readback_size || request.readback_size > 16384 || !request.source_allocation_generation) {
		return ERR_INVALID_PARAMETER;
	}
	if (p_allow_empty && !request.sample_count && (request.range_begin || request.range_end)) {
		return ERR_INVALID_PARAMETER;
	}
	if (!rd->driver->native_call_observation_supported()) {
		return ERR_UNAVAILABLE;
	}
	const uint64_t graph_bound = (request.readback_size + 31) / 32 + 2;
	uint32_t body_push[4], capture_push[4];
	memcpy(body_push, request.body_push, 16);
	memcpy(capture_push, request.capture_push, 16);
	if (body_push[1] != request.sample_count || capture_push[1] != request.sample_count) {
		return ERR_INVALID_PARAMETER;
	}
	const Error capacity_error = qualification_native_capacity(*rd->driver, rd->draw_graph.qualification_command_count(), graph_bound, rd->transfer_worker_pool_max_size);
	if (capacity_error != OK) {
		return capacity_error;
	}
	if (request.body_groups_x > rd->driver->limit_get(RenderingDevice::LIMIT_MAX_COMPUTE_WORKGROUP_COUNT_X)) {
		return ERR_INVALID_PARAMETER;
	}
	const uint64_t pipelines[] = { request.capture_pipeline, request.body_pipeline };
	const uint64_t sets0[] = { request.capture_set, request.body_set }, sets1[] = { capture_set1, body_set1 };
	for (uint32_t i = 0; i < 2; ++i) {
		RID pipeline_rid = RID::from_uint64(pipelines[i]);
		auto *pipeline = rd->compute_pipeline_owner.get_or_null(pipeline_rid);
		const uint32_t set_count = sets1[i] ? 2 : 1;
		if (!pipeline || !pipeline->driver_id || pipeline->push_constant_size != 16 || pipeline->set_formats.size() != set_count) {
			return ERR_INVALID_DATA;
		}
		if (i == 1 && (pipeline->local_group_size[0] != 64 || pipeline->local_group_size[1] != 1 || pipeline->local_group_size[2] != 1))
			return ERR_INVALID_DATA;
		if (!add_resource(resources, pipeline_rid) || !add_resource(resources, pipeline->shader)) {
			return ERR_PARAMETER_RANGE_ERROR;
		}
		for (uint32_t set_index = 0; set_index < set_count; ++set_index) {
			RID set_rid = RID::from_uint64(set_index ? sets1[i] : sets0[i]);
			auto *set = rd->uniform_set_owner.get_or_null(set_rid);
			if (!set || !set->driver_id || !pipeline->set_formats[set_index] || pipeline->set_formats[set_index] != set->format || set->shader_set != set_index) {
				return ERR_INVALID_DATA;
			}
			if (!set->shared_textures_to_update.is_empty() || !set->pending_clear_textures.is_empty() || !set->acceleration_structures.is_empty()) {
				return ERR_UNAVAILABLE;
			}
			if (!add_resource(resources, set_rid)) {
				return ERR_PARAMETER_RANGE_ERROR;
			}
			auto *dependencies = rd->reverse_dependency_map.getptr(set_rid);
			if (!dependencies)
				return ERR_INVALID_DATA;
			for (RID dependency : *dependencies) {
				if (rd->shader_owner.owns(dependency)) {
					if (!add_resource(resources, dependency))
						return ERR_PARAMETER_RANGE_ERROR;
					continue;
				}
				auto *texture = rd->texture_owner.get_or_null(dependency);
				auto *sampler = rd->sampler_owner.get_or_null(dependency);
				if (texture || sampler) {
					// A numeric RID or a same-numbered return cell is not the original
					// method resource owner. Authenticate the typed child before admitting
					// it; the later pin operation installs the physical reverse references.
					auto *physical = texture ? texture->qualification_return : sampler->qualification_return;
					auto matches = [&](const auto *child) { return child && &child->physical == physical && !child->creating && !child->retiring && !child->acknowledged && child->serial && child->rid == dependency.get_id() && child->object_serial && child->object_serial == child->physical.serial && child->physical.rid == child->rid && child->physical.bridge == bridge->serial && child->physical.child == child->serial; };
					bool original = false;
					if (texture) {
						for (auto *child : bridge->textures) {
							if (matches(child)) {
								original = true;
								break;
							}
						}
					}
					if (sampler) {
						for (auto *child : bridge->samplers) {
							if (matches(child)) {
								original = true;
								break;
							}
						}
					}
					if (!original) {
						return ERR_UNAUTHORIZED;
					}
					// Shared fallback updates, pending clears, and legacy transfer workers
					// require effects outside this original checked record operation.
					if (texture && (!texture->driver_id || texture->owner.is_valid() || texture->shared_fallback || texture->pending_clear || texture->transfer_worker_index >= 0)) {
						return ERR_UNAVAILABLE;
					}
					if (sampler && !sampler->driver_id) {
						return ERR_UNAVAILABLE;
					}
				} else {
					auto *buffer = rd->storage_buffer_owner.get_or_null(dependency);
					if (!buffer) {
						buffer = rd->uniform_buffer_owner.get_or_null(dependency);
					}
					if (!buffer || !buffer->driver_id || buffer->usage.has_flag(RDD::BUFFER_USAGE_DYNAMIC_PERSISTENT_BIT) || buffer->transfer_worker_index >= 0) {
						return ERR_UNAVAILABLE;
					}
				}
				if (!add_resource(resources, dependency))
					return ERR_PARAMETER_RANGE_ERROR;
			}
		}
	}
	auto *buffer = rd->storage_buffer_owner.get_or_null(RID::from_uint64(request.readback_buffer));
	if (!buffer || !buffer->driver_id || !buffer->backing_serial || buffer->backing_serial != request.observed_backing_serial || request.readback_offset > buffer->size || request.readback_size > buffer->size - request.readback_offset || !buffer->usage.has_flag(RDD::BUFFER_USAGE_TRANSFER_FROM_BIT) || buffer->transfer_worker_index >= 0)
		return ERR_INVALID_DATA;
	if (!add_resource(resources, RID::from_uint64(request.readback_buffer)))
		return ERR_PARAMETER_RANGE_ERROR;
	for (RID rid : resources) {
		auto *stored = rd->storage_buffer_owner.get_or_null(rid);
		auto *pipeline = rd->compute_pipeline_owner.get_or_null(rid);
		auto *shader = rd->shader_owner.get_or_null(rid);
		auto *texture = rd->texture_owner.get_or_null(rid);
		auto *sampler = rd->sampler_owner.get_or_null(rid);
		auto *cell = stored ? stored->qualification_return : pipeline ? pipeline->qualification_return
				: shader											  ? shader->qualification_return
				: texture											  ? texture->qualification_return
				: sampler											  ? sampler->qualification_return
																	  : nullptr;
		if (cell && (cell->bridge != bridge->serial || cell->use_closed.load()))
			return ERR_UNAUTHORIZED;
	}
	return OK;
}
void Registry::qualification_record_append(QualificationTicket *ticket,RenderingDevice *rd) {
 const auto &request=ticket->origin;
 rd->qualification_recording=true;
 auto capture=rd->compute_list_begin();
 if(capture!=RenderingDevice::INVALID_ID) {
  rd->compute_list_bind_compute_pipeline(capture,RID::from_uint64(request.capture_pipeline));
  rd->compute_list_bind_uniform_set(capture,RID::from_uint64(request.capture_set),0);
  if (ticket->multi_readback && ticket->range_origin.capture_set1) {
	  rd->compute_list_bind_uniform_set(capture, RID::from_uint64(ticket->range_origin.capture_set1), 1);
  }
  rd->compute_list_set_push_constant(capture,request.capture_push,16);
  ticket->capture=rd->_qualification_dispatch(capture,1,1,1);rd->compute_list_end();
 }
 {MutexLock lock(mutex);add_graph(ticket,ticket->capture,1,rd->frame,rd->frames[rd->frame].qualification_epoch);rd->_qualification_add_frame_ticket(ticket);}
 if(ticket->capture.is_valid()) {
  rd->draw_graph.add_synchronization();auto body=rd->compute_list_begin();
  if(body!=RenderingDevice::INVALID_ID) {
   rd->compute_list_bind_compute_pipeline(body,RID::from_uint64(request.body_pipeline));
   rd->compute_list_bind_uniform_set(body,RID::from_uint64(request.body_set),0);
   if (ticket->multi_readback && ticket->range_origin.body_set1) {
	   rd->compute_list_bind_uniform_set(body, RID::from_uint64(ticket->range_origin.body_set1), 1);
   }
   rd->compute_list_set_push_constant(body,request.body_push,16);
   ticket->body=rd->_qualification_dispatch(body,request.body_groups_x,1,1);rd->compute_list_end();
  }
  {MutexLock lock(mutex);add_graph(ticket,ticket->body,2,rd->frame,rd->frames[rd->frame].qualification_epoch);}
 }
 Error first_error = ticket->body.is_valid() ? OK : FAILED;
 if (ticket->body.is_valid()) {
	 for (uint32_t i = 0; i < ticket->readback_count(); ++i) {
		 {
			 MutexLock lock(mutex);
			 if (ticket->admission_closed.load()) {
				 break;
			 }
		 }
		 const auto range = ticket->readback_range(i);
		 auto &owned = ticket->readback_result(i);
		 const Error error = rd->_buffer_get_data_async(RID::from_uint64(range.readback_buffer), Callable(), range.readback_offset, range.readback_size, owned);
		 {
			 MutexLock lock(mutex);
			 auto &joined = ticket->readback_cursor(i);
			 for (; joined < owned->copies.size(); ++joined) {
				 const auto &copy = owned->copies[joined];
				 add_graph(ticket, copy.handle, 3, rd->frame, rd->frames[rd->frame].qualification_epoch, range.observed_backing_serial, copy.region.src_offset, copy.region.size);
			 }
		 }
		 if (error != OK && error != ERR_BUSY) {
			 first_error = error;
			 break;
		 }
	 }
 }
 rd->qualification_recording=false;
 MutexLock lock(mutex);
 ticket->recorded=ticket->capture.is_valid() && ticket->body.is_valid();ticket->recording=false;
 if (first_error != OK) {
	 ticket->append_failed = true;
	 if (ticket->report.native_error == OK) {
		 ticket->report.native_error = first_error;
	 }
 }
 qualification_ticket_publish(ticket,rd);
}
void Registry::qualification_ticket_publish(QualificationTicket *ticket,RenderingDevice *rd) {
 if(ticket->report.state==8)return;
 auto next=ticket->report;
 for (uint32_t i = 0; i < ticket->readback_count(); ++i) {
	 const auto &owned = ticket->readback_result(i);
	 if (owned.is_valid() && owned->error != OK) {
		 ticket->append_failed = true;
		 if (next.native_error == OK) {
			 next.native_error = owned->error;
			 next.native_code = owned->native_code;
		 }
		 next.phase = 5;
	 }
 }
 next.required_count=ticket->facts.size();next.known_count=0;next.possible_count=0;next.fact_count=ticket->facts.size();
 bool sealed=true,returned=true,mapped_all=true,native_success=true,native_failed=false,native_unknown=false,has_native=!ticket->natives.is_empty();
 for(auto &graph:ticket->graphs) {
  auto &fact=ticket->facts[graph.fact];sealed=sealed&&graph.sealed;returned=returned&&graph.returned;
  bool mapped=false,success=true;uint64_t first=0,completion_serial=0;
  for(const auto &native:ticket->natives)if(native.frame==graph.frame && native.epoch==graph.epoch && (native.slot==UINT32_MAX || native.slot==fact.command_slot)) {
   success=success&&ticket->facts[native.fact].completion==2;
   if(native.slot==fact.command_slot){mapped=true;if(!first){first=native.source.serial;completion_serial=native.source.completion_serial;fact.backend_queue=native.source.queue_index;}}
  }
  if(mapped){fact.native_serial=first;fact.completion_serial=completion_serial;fact.issue=success?1:2;fact.completion=success?2:1;fact.phase=success?4:3;}
  mapped_all=mapped_all&&mapped;
  ++next.known_count;
 }
 for(const auto &native:ticket->natives) {
  const auto &fact=ticket->facts[native.fact];const bool success=fact.issue==1 && fact.completion==2 && !fact.error;
  native_success=native_success&&success;native_failed=native_failed||fact.completion==3;native_unknown=native_unknown||fact.completion==4;
  if(fact.issue==2 || fact.completion==4)++next.possible_count;else ++next.known_count;
 }
 if(ticket->mapping_failed)++next.possible_count;
 next.owned_size=0;
 if (ticket->result.is_valid() && ticket->result->issued_size) {
	 next.readback_request = ticket->readback_serial(0);
 }
 next.issue_fact=ticket->natives.is_empty()?0:native_success?1:2;
 const bool sources_returned=mapped_all && returned && !ticket->frame_refs && native_success && !ticket->mapping_failed && (ticket->graphs.is_empty() || has_native);
 bool live_upload_claim = false;
 for (const auto &upload : ticket->uploads) {
  live_upload_claim = live_upload_claim || upload.attempted || upload.progress.consumed || !upload.progress.segments.is_empty();
 }
 for (auto *image : ticket->texture_uploads) {
	 live_upload_claim = live_upload_claim || image->upload_attempted;
 }
 const bool no_effect=ticket->graphs.is_empty() && ticket->natives.is_empty() && !ticket->mapping_failed && !ticket->recording && !ticket->frame_refs;
 if(no_effect) {next.state=0;next.phase=0;}
 else if(ticket->mapping_failed || native_unknown) {next.state=6;next.phase=3;}
 else if(native_failed) {next.state=7;next.phase=4;}
 else if(ticket->append_failed) {next.state=5;}
 else if(!sealed) {next.state=2;next.phase=2;}
 else if(ticket->natives.is_empty()) {next.state=3;next.phase=3;}
 else {next.state=4;next.phase=4;}
 bool result_complete = ticket->recorded && ticket->ordering_closed && ticket->readback_count() > 0;
 for (uint32_t i = 0; i < ticket->readback_count(); ++i) {
	 result_complete = result_complete && ticket->readback_complete(i);
 }
 if(sources_returned && result_complete && !ticket->append_failed && !ticket->admission_closed) {
	 next.state = 8;
	 next.phase = 5;
	 next.owned_size = ticket->multi_readback ? ticket->range_origin.readback_size : ticket->result->bytes.size();
 }
 // A logical copy/append failure closes future appends. Physical ownership
 // can return independently only after every exact native source succeeded
 // and all original graph/frame/copy obligations have returned. Unknown maps
 // and native failures never satisfy sources_returned.
 if(ticket->append_failed)ticket->admission_closed=true;
 if(ticket->admission_closed)ticket->continuation_pending=false;
 const bool failed_prefix_settled=ticket->append_failed && sources_returned && !ticket->recording;
 const bool cancelled_upload_settled=ticket->prepared && ticket->admission_closed && sources_returned && !ticket->recording;
 if(cancelled_upload_settled){next.state=7;next.phase=6;next.owned_size=0;}
 // A prepared ticket whose upload has already attempted an effect still owns
 // its original writer claim when the checked call returns with no graph
 // prefix (for example ERR_BUSY before the first segment). It must remain
 // non-retired until cancellation or a settled failure releases that claim.
 if((no_effect && !live_upload_claim) || next.state==8 || cancelled_upload_settled || failed_prefix_settled) {
  rd->_qualification_unpin_resources(ticket);ticket->physically_retired.store(true);
 }
 if(ticket->observation_dirty || memcmp(&next,&ticket->report,sizeof(next))) {
  if(next.revision==UINT64_MAX){next.state=6;next.owned_size=0;ticket->mapping_failed=true;ticket->physically_retired.store(false);}
  else ++next.revision;
  ticket->report=next;ticket->observation_dirty=false;
 }
}
GDExtensionDeviceSessionResultV1 Registry::qualification_record(GDExtensionQualificationBridgeRefV1 ref,const GDExtensionQualificationRecordV1 *request,GDExtensionQualificationTicketRefV1 *out,GDExtensionQualificationReportV1 *report) {
 if(!aligned(request)||!aligned(out)||!aligned(report)||!separate(request,sizeof(*request),out,sizeof(*out))||!separate(request,sizeof(*request),report,sizeof(*report))||!separate(out,sizeof(*out),report,sizeof(*report))||out->registry_instance||out->handle_serial)return result(ERR_INVALID_PARAMETER,2);
 *out={};*report={};QualificationBridge *bridge=nullptr;RenderingDevice *rd=nullptr;QualificationTicket *ticket=nullptr;LocalVector<RID> resources;RdUnlock rd_unlock;
 {
  MutexLock lock(mutex);Error error=qualification_bridge_locked(ref,true,true,bridge,rd,6);if(error!=OK)return result(error,error==ERR_UNAUTHORIZED?15:16);
  // Exact replay reads the original ticket, without re-admitting its resources
  // against a later RD window or appending the same work again.
  for(auto *existing:bridge->tickets)if(existing && existing->origin.record_generation==request->record_generation && existing->origin.request_nonce==request->request_nonce) {
   if(existing->acknowledged)return result(ERR_DOES_NOT_EXIST,17);
   if(existing->prepared || memcmp(&existing->origin,request,sizeof(*request)))return result(ERR_INVALID_DATA,16);
   if(existing->recording)return result(ERR_BUSY,19);
   *out={instance,existing->serial};*report=existing->report;return result(OK);
  }
  if(!rd->_thread_safe_.try_lock())return result(ERR_BUSY,19);rd_unlock.mutex=&rd->_thread_safe_;
  error=qualification_record_preflight(bridge,rd,*request,resources);if(error!=OK)return result(error,error==ERR_UNAVAILABLE?18:error==ERR_PARAMETER_RANGE_ERROR?19:2);
  uint32_t slot=0;for(;slot<QualificationBridge::CHILD_CAPACITY && bridge->tickets[slot];++slot){}
  const uint32_t refs_needed=bridge->cleanup_refs_reserved?1:3;
  if(slot==QualificationBridge::CHILD_CAPACITY || private_limit<refs_needed || records[bridge->library].private_refs>private_limit-refs_needed || !qualification_serial_capacity(2))return result(ERR_PARAMETER_RANGE_ERROR,19);
  ticket=memnew(QualificationTicket);ticket->origin=*request;ticket->bridge_serial=bridge->serial;ticket->library=bridge->library;
  ticket->facts.reserve(QualificationTicket::FACT_CAPACITY);ticket->graphs.reserve(QualificationTicket::FACT_CAPACITY);ticket->natives.reserve(QualificationTicket::FACT_CAPACITY);ticket->resource_pins.reserve(128);ticket->physical_uses.reserve(128);
  ticket->result.instantiate();error=ticket->result->bytes.resize(request->readback_size);if(error!=OK){memdelete(ticket);return result(error,19);}
  ticket->result->qualification_owned = true;
  qualification_serials.take(ticket->serial);
  qualification_serials.take(ticket->result->request_serial);
  ticket->original_readback_serial = ticket->result->request_serial;
  auto &r=ticket->report;r.abi={1,sizeof(GDExtensionDeviceExitAbi),0};r.revision=1;r.record_generation=request->record_generation;r.request_nonce=request->request_nonce;r.source_backing_serial=request->observed_backing_serial;r.source_offset=request->readback_offset;r.source_size=request->readback_size;memcpy(r.request_digest,request->request_digest,32);memcpy(r.unit_digest,request->unit_digest,32);
  if(!bridge->cleanup_refs_reserved){records[bridge->library].private_refs+=2;bridge->cleanup_refs_reserved=true;}
  bridge->tickets[slot]=ticket;++bridge->children;++records[bridge->library].private_refs;
  if(rd->_recording_error()!=OK || rd->draw_list.active || rd->compute_list.active || rd->raytracing_list.active || rd->draw_graph.has_driver_callbacks()) {
   qualification_ticket_publish(ticket,rd);*out={instance,ticket->serial};*report=ticket->report;return result(OK);
  }
  error=rd->_qualification_pin_resources(ticket,resources);
  if(error!=OK) {
   bridge->tickets[slot]=nullptr;--bridge->children;--records[bridge->library].private_refs;memdelete(ticket);qualification_trim_cleanup_ref_credits_locked(bridge);return result(error,16);
  }
  ticket->recording=true;
 }
 qualification_record_append(ticket,rd);
 MutexLock lock(mutex);*out={instance,ticket->serial};*report=ticket->report;return result(OK);
}
void Registry::qualification_ticket_step(QualificationTicket *ticket,RenderingDevice *rd) {
 if(ticket->physically_retired.load() && !ticket->continuation_pending)return;
 // Continuations share the original nonwaiting RD maintenance window.
 // Hold it through graph/copy membership and frame-return publication.
 if(!rd->_thread_safe_.try_lock())return;
 RdUnlock rd_unlock;rd_unlock.mutex=&rd->_thread_safe_;
 if(rd->_recording_error()!=OK || rd->draw_list.active || rd->compute_list.active || rd->raytracing_list.active || rd->frame_wait_draining || rd->draw_graph.has_driver_callbacks())return;
 if (!ticket->append_failed && !ticket->admission_closed && ticket->recorded && !ticket->mapping_failed) {
	 // A later-frame continuation must not read an output until the original
	 // Body has physically returned with its exact successful native sources.
	 const bool body_ready = !ticket->multi_readback || rd->draw_graph.contains_record(ticket->body) || ticket->body_returned_successfully();
	 if (body_ready) {
		 for (uint32_t i = 0; i < ticket->readback_count(); ++i) {
			 const auto range = ticket->readback_range(i);
			 auto &owned = ticket->readback_result(i);
			 if (owned.is_null() || owned->error != OK || owned->issued_size >= range.readback_size) {
				 continue;
			 }
			 {
				 MutexLock lock(mutex);
				 if (ticket->admission_closed.load()) {
					 break;
				 }
				 ticket->recording = true;
			 }
			 rd->qualification_recording = true;
			 const Error error = rd->_buffer_get_data_async(RID::from_uint64(range.readback_buffer), Callable(), range.readback_offset, range.readback_size, owned);
			 rd->qualification_recording = false;
			 MutexLock lock(mutex);
			 ticket->recording = false;
			 auto &joined = ticket->readback_cursor(i);
			 for (; joined < owned->copies.size(); ++joined) {
				 const auto &copy = owned->copies[joined];
				 add_graph(ticket, copy.handle, 3, rd->frame, rd->frames[rd->frame].qualification_epoch, range.observed_backing_serial, copy.region.src_offset, copy.region.size);
			 }
			 bool has_frame_graph = false;
			 for (const auto &graph : ticket->graphs) {
				 if (graph.frame == uint32_t(rd->frame) && graph.epoch == rd->frames[rd->frame].qualification_epoch) {
					 has_frame_graph = true;
					 break;
				 }
			 }
			 if (has_frame_graph) {
				 rd->_qualification_add_frame_ticket(ticket);
			 }
			 if (error != OK && error != ERR_BUSY) {
				 ticket->append_failed = true;
				 if (ticket->report.native_error == OK) {
					 ticket->report.native_error = error;
				 }
				 break;
			 }
		 }
	 }
 }
 for(uint32_t index=0;index<rd->frames.size();++index) {
  bool member=false;for(auto *entry:rd->frames[index].qualification_tickets)if(entry==ticket){member=true;break;}
  if(!member || !rd->frames[index].fence_signaled)continue;
  const Error error=rd->_stall_for_frame(index,true);
  if (error != OK && error != ERR_BUSY) {
	  MutexLock lock(mutex);
	  ticket->append_failed = true;
	  if (ticket->report.native_error == OK) {
		  ticket->report.native_error = error;
		  if (rd->frame_wait_native_code_valid.load()) {
			  ticket->report.native_code = rd->frame_wait_native_code.load();
		  }
	  }
  }
 }
 MutexLock lock(mutex);qualification_ticket_publish(ticket,rd);
}
GDExtensionDeviceSessionResultV1 Registry::qualification_observe(GDExtensionQualificationBridgeRefV1 ref,GDExtensionQualificationTicketRefV1 ticket_ref,const GDExtensionQualificationBudgetV1 *budget,GDExtensionQualificationReportV1 *out) {
 if(!aligned(budget)||!aligned(out)||!separate(budget,sizeof(*budget),out,sizeof(*out)))return result(ERR_INVALID_PARAMETER,2);
 *out={};if(!valid_abi(budget->abi)||budget->reserved||budget->max_passes<1||budget->max_passes>32||budget->max_micros<1||budget->max_micros>5000)return result(ERR_INVALID_PARAMETER,2);
 {
	 MutexLock lock(mutex);
	 QualificationBridge *bridge = nullptr;
	 RenderingDevice *rd = nullptr;
	 QualificationTicket *ticket = nullptr;
	 Error error = qualification_bridge_locked(ref, false, false, bridge, rd);
	 if (error != OK) {
		 return result(error, 17);
	 }
	 error = qualification_ticket_locked(bridge, ticket_ref, ticket);
	 if (error != OK) {
		 return result(error, 17);
	 }
	 if (ticket->multi_readback) {
		 return result(ERR_INVALID_DATA, 16);
	 }
 }
 Error error=qualification_schedule(ref,ticket_ref.handle_serial,3,*budget);if(error!=OK)return result(error,17);
 MutexLock lock(mutex);QualificationBridge *bridge=nullptr;RenderingDevice *rd=nullptr;QualificationTicket *ticket=nullptr;error=qualification_bridge_locked(ref,false,false,bridge,rd);if(error!=OK)return result(error,17);error=qualification_ticket_locked(bridge,ticket_ref,ticket);if(error!=OK)return result(error,17);*out=ticket->report;return result(OK);
}
GDExtensionDeviceSessionResultV1 Registry::qualification_facts(GDExtensionQualificationBridgeRefV1 ref,GDExtensionQualificationTicketRefV1 ticket_ref,uint64_t revision,uint32_t first,uint32_t capacity,GDExtensionQualificationFactV1 *facts,uint32_t *written) {
 if(!aligned(written))return result(ERR_INVALID_PARAMETER,2);
 const uint64_t fact_bytes=uint64_t(capacity)*sizeof(*facts);
 if (facts && (fact_bytes > SIZE_MAX || !separate(facts, 1, written, sizeof(*written)) || !separate(facts, size_t(fact_bytes), written, sizeof(*written)))) {
	 return result(ERR_INVALID_PARAMETER, 2);
 }
 *written=0;
 if(!aligned(facts)||!capacity||capacity>64||first>UINT32_MAX-capacity)return result(ERR_INVALID_PARAMETER,2);
 MutexLock lock(mutex);QualificationBridge *bridge=nullptr;RenderingDevice *rd=nullptr;QualificationTicket *ticket=nullptr;Error error=qualification_bridge_locked(ref,false,false,bridge,rd);if(error!=OK)return result(error,17);error=qualification_ticket_locked(bridge,ticket_ref,ticket);if(error!=OK)return result(error,17);
 if(!revision || revision!=ticket->report.revision || first>ticket->facts.size())return result(ERR_INVALID_DATA,17);
 const uint32_t count=MIN(capacity,ticket->facts.size()-first);for(uint32_t i=0;i<count;++i)facts[i]=ticket->facts[first+i];*written=count;return result(OK);
}
GDExtensionDeviceSessionResultV1 Registry::qualification_copy(GDExtensionQualificationBridgeRefV1 ref,GDExtensionQualificationTicketRefV1 ticket_ref,uint64_t revision,void *destination,uint32_t capacity,uint32_t *written) {
 if(!aligned(written))return result(ERR_INVALID_PARAMETER,2);
 if (destination && (!separate(destination, 1, written, sizeof(*written)) || !separate(destination, capacity, written, sizeof(*written)))) {
	 return result(ERR_INVALID_PARAMETER, 2);
 }
 *written=0;
 if(!destination||!capacity||capacity>16384)return result(ERR_INVALID_PARAMETER,2);
 MutexLock lock(mutex);QualificationBridge *bridge=nullptr;RenderingDevice *rd=nullptr;QualificationTicket *ticket=nullptr;Error error=qualification_bridge_locked(ref,false,false,bridge,rd);if(error!=OK)return result(error,17);error=qualification_ticket_locked(bridge,ticket_ref,ticket);if(error!=OK)return result(error,17);
 if (!ticket->multi_readback) {
	 if (!revision || revision != ticket->report.revision || ticket->report.state != 8 || !ticket->physically_retired.load() || ticket->result.is_null() || !ticket->readback_complete(0) || capacity != ticket->report.owned_size || capacity != ticket->result->bytes.size()) {
		 return result(ERR_BUSY, 16);
	 }
	 memcpy(destination, ticket->result->bytes.ptr(), capacity);
	 *written = capacity;
	 return result(OK);
 }
 if (!revision || revision != ticket->report.revision || ticket->report.state != 8 || !ticket->physically_retired.load() || capacity != ticket->report.owned_size) {
	 return result(ERR_BUSY, 16);
 }
 for (uint32_t i = 0; i < ticket->readback_count(); ++i) {
	 const auto range = ticket->readback_range(i);
	 if (!ticket->readback_complete(i) || range.result_offset > capacity || range.readback_size > capacity - range.result_offset) {
		 return result(ERR_BUSY, 16);
	 }
 }
 for (uint32_t i = 0; i < ticket->readback_count(); ++i) {
	 const auto range = ticket->readback_range(i);
	 const auto &owned = ticket->readback_result(i);
	 memcpy(static_cast<uint8_t *>(destination) + range.result_offset, owned->bytes.ptr(), range.readback_size);
 }
 *written = capacity;
 return result(OK);
}
GDExtensionDeviceSessionResultV1 Registry::qualification_release(GDExtensionQualificationBridgeRefV1 ref,GDExtensionQualificationTicketRefV1 ticket_ref) {
 MutexLock lock(mutex);QualificationBridge *bridge=nullptr;RenderingDevice *rd=nullptr;QualificationTicket *ticket=nullptr;Error error=qualification_bridge_locked(ref,false,false,bridge,rd);if(error!=OK)return result(error,17);error=qualification_ticket_locked(bridge,ticket_ref,ticket);if(error!=OK)return result(error,17);
 if(ticket->accepted_rt_refs || ticket->recording || ticket->frame_refs || !ticket->physically_retired.load())return result(ERR_BUSY,16);
 if(ticket->queued)return result(ERR_BUSY,19);
 // Writers of physical_uses and cell.uses hold both Registry and RD locks;
 // the Registry lock alone permits this bounded read-only consistency scan.
 // Validate an address against original bridge children before dereferencing.
 auto valid_reverse_edges=[&]() {
  auto forward_count=[&](QualificationPhysicalReturn *cell) {uint32_t count=0;for(auto *entry:ticket->physical_uses)if(entry==cell)++count;return count;};
  auto check_cell=[&](QualificationPhysicalReturn *cell,uint64_t child_serial) {
   uint32_t reverse=0;for(auto *entry:cell->uses)if(entry==ticket)++reverse;
   const uint32_t forward=forward_count(cell);
   return reverse==forward && reverse<=1 && (!reverse || (cell->bridge==bridge->serial && cell->child==child_serial));
  };
  for(auto *cell:ticket->physical_uses) {
   if(!cell || forward_count(cell)!=1)return false;
   bool original=false;
   for(auto *child:bridge->storage)if(child && &child->physical==cell)original=true;
   for(auto *child:bridge->textures)if(child && &child->physical==cell)original=true;
   for(auto *child:bridge->samplers)if(child && &child->physical==cell)original=true;
   for (auto *child : bridge->sets) {
	   if (child && &child->physical == cell) {
		   original = true;
	   }
   }
   for(auto *child:bridge->units)if(child && (&child->shader==cell || &child->pipeline==cell))original=true;
   if(!original)return false;
  }
  for(auto *child:bridge->storage)if(child && !check_cell(&child->physical,child->serial))return false;
  for(auto *child:bridge->textures)if(child && !check_cell(&child->physical,child->serial))return false;
  for(auto *child:bridge->samplers)if(child && !check_cell(&child->physical,child->serial))return false;
  for (auto *child : bridge->sets) {
	  if (child && !check_cell(&child->physical, child->serial)) {
		  return false;
	  }
  }
  for(auto *child:bridge->units)if(child && (!check_cell(&child->shader,child->serial) || !check_cell(&child->pipeline,child->serial)))return false;
  return true;
 };
 if(!valid_reverse_edges())return result(ERR_INVALID_DATA,16);
 const bool has_reverse_edges=!ticket->physical_uses.is_empty();
 if(has_reverse_edges) {
  if(!rd->_thread_safe_.try_lock())return result(ERR_BUSY,19);
  rd->_thread_safe_.unlock();
 }
 for(auto &member:ticket->natives)if(member.retained && !member.owner_released) {
		 error = release_native_source(*rd->driver, member);
		 if (error != OK) {
			 return result(error, error == ERR_BUSY ? 19 : 17);
		 }
 }
 RdUnlock rd_unlock;
 if(has_reverse_edges) {
  if(!rd->_thread_safe_.try_lock())return result(ERR_BUSY,19);rd_unlock.mutex=&rd->_thread_safe_;
  if(!valid_reverse_edges())return result(ERR_INVALID_DATA,16);
  for(auto *cell:ticket->physical_uses)CRASH_COND(!cell->uses.erase(ticket));
  ticket->physical_uses.clear();
 }
 ticket->result.unref();
 for (auto &owned : ticket->additional_results) {
	 owned.unref();
 }
 ticket->admission_closed = true;
 ticket->acknowledged = true;
 --bridge->children;
 --records[bridge->library].private_refs;
 qualification_trim_cleanup_ref_credits_locked(bridge);
 return result(OK);
}
GDExtensionDeviceSessionResultV1 Registry::qualification_prepare_uploads(GDExtensionQualificationBridgeRefV1 ref, const GDExtensionQualificationUploadPrepareV1 *request, const GDExtensionQualificationUploadPlanV1 *plans, uint32_t plan_count, GDExtensionQualificationTicketRefV1 *out, GDExtensionQualificationReportV1 *report, bool p_multi_readback) {
	if (!aligned(request) || !aligned(out) || !aligned(report) || !aligned(plans) || !plan_count || plan_count > 128 || !separate(request, sizeof(*request), plans, sizeof(*plans) * plan_count) || !separate(request, sizeof(*request), out, sizeof(*out)) || !separate(request, sizeof(*request), report, sizeof(*report)) || !separate(plans, sizeof(*plans) * plan_count, out, sizeof(*out)) || !separate(plans, sizeof(*plans) * plan_count, report, sizeof(*report)) || !separate(out, sizeof(*out), report, sizeof(*report)) || out->registry_instance || out->handle_serial)
		return result(ERR_INVALID_PARAMETER, 2);
	*out = {};
	*report = {};
	if (!valid_abi(request->abi) || request->reserved || request->upload_count != plan_count || !request->record_generation || !request->request_nonce || !request->qualification_session_serial || !request->parent_serial || !request->reservation_serial || !request->attempt || !request->max_segments || request->max_segments > 4096 || !request->expected_result_bytes || request->expected_result_bytes > 16384)
		return result(ERR_INVALID_PARAMETER, 2);
	RdUnlock rd_unlock;
	MutexLock lock(mutex);
	QualificationBridge *bridge = nullptr;
	RenderingDevice *rd = nullptr;
	Error error = qualification_bridge_locked(ref, true, true, bridge, rd, p_multi_readback ? 11 : 7);
	if (error != OK) {
		return result(error, error == ERR_UNAUTHORIZED ? 15 : 16);
	}
	if (request->library_generation != records[bridge->library].snapshot.library_generation)
		return result(ERR_INVALID_DATA, 17);
	uint32_t indices[128] = {};
	bool present[128] = {};
	uint64_t segments = 0;
	for (uint32_t i = 0; i < plan_count; ++i) {
		const auto &plan = plans[i];
		if (!valid_abi(plan.abi) || plan.reserved || plan.upload_index >= plan_count || present[plan.upload_index] || plan.role >= GDEXTENSION_QUALIFICATION_UPLOAD_ROLE_COUNT_V1 || plan.owner_kind < GDEXTENSION_QUALIFICATION_UPLOAD_OWNER_E05_V1 || plan.owner_kind > GDEXTENSION_QUALIFICATION_UPLOAD_OWNER_E24_V1 || !plan.byte_size || plan.byte_size > UINT32_MAX || plan.byte_offset > UINT32_MAX || plan.byte_size > UINT32_MAX - plan.byte_offset)
			return result(ERR_INVALID_PARAMETER, 2);
		present[plan.upload_index] = true;
		indices[plan.upload_index] = i;
		segments += (plan.byte_size + 31) / 32;
		if (segments > request->max_segments)
			return result(ERR_PARAMETER_RANGE_ERROR, 19);
	}
	for (auto *ticket : bridge->tickets)
		if (ticket && ticket->origin.record_generation == request->record_generation && ticket->origin.request_nonce == request->request_nonce) {
			if (ticket->acknowledged)
				return result(ERR_DOES_NOT_EXIST, 17);
			if (!ticket->prepared || ticket->multi_readback != p_multi_readback || memcmp(&ticket->preparation, request, sizeof(*request))) {
				return result(ERR_INVALID_DATA, 16);
			}
			for (uint32_t i = 0; i < plan_count; ++i)
				if (memcmp(&ticket->uploads[i].plan, &plans[indices[i]], sizeof(*plans)))
					return result(ERR_INVALID_DATA, 16);
			*out = { instance, ticket->serial };
			*report = ticket->report;
			return result(OK);
		}
	if (!rd->_thread_safe_.try_lock())
		return result(ERR_BUSY, 19);
	rd_unlock.mutex = &rd->_thread_safe_;
	if (!rd->driver->native_call_observation_supported())
		return result(ERR_UNAVAILABLE, 18);
	for (uint32_t i = 0; i < plan_count; ++i) {
		error = qualification_upload_destination(bridge, rd, plans[i]);
#ifdef TESTS_ENABLED
		if (OS::get_singleton()->has_environment("GODOT_QUALIFICATION_CAPACITY_TRACE")) {
			print_line(vformat("QCAP destination plan=%d role=%d bytes=%d native_error=%d", i, plans[i].role, plans[i].byte_size, int(error)));
		}
#endif
		if (error != OK)
			return result(error, error == ERR_UNAVAILABLE ? 18 : 2);
	}
	const uint64_t graph_bound = segments + (request->expected_result_bytes + 31) / 32 + 2 + (p_multi_readback ? 2 : 0);
	error = qualification_native_capacity(*rd->driver, rd->draw_graph.qualification_command_count(), graph_bound, rd->transfer_worker_pool_max_size);
	if (error != OK)
		return result(error, error == ERR_UNAVAILABLE ? 18 : 19);
	uint32_t slot = 0;
	for (; slot < QualificationBridge::CHILD_CAPACITY && bridge->tickets[slot]; ++slot) {
	}
	const uint32_t refs_needed = bridge->cleanup_refs_reserved ? 1 : 3;
	if (slot == QualificationBridge::CHILD_CAPACITY || private_limit < refs_needed || records[bridge->library].private_refs > private_limit - refs_needed || !qualification_serial_capacity(p_multi_readback ? 4 : 2)) {
		return result(ERR_PARAMETER_RANGE_ERROR, 19);
	}
	auto *ticket = memnew(QualificationTicket);
	ticket->prepared = true;
	ticket->multi_readback = p_multi_readback;
	ticket->preparation = *request;
	ticket->bridge_serial = bridge->serial;
	ticket->library = bridge->library;
	ticket->origin.record_generation = request->record_generation;
	ticket->origin.request_nonce = request->request_nonce;
	ticket->facts.reserve(QualificationTicket::FACT_CAPACITY);
	ticket->graphs.reserve(QualificationTicket::FACT_CAPACITY);
	ticket->natives.reserve(QualificationTicket::FACT_CAPACITY);
	ticket->resource_pins.reserve(128);
	ticket->physical_uses.reserve(128);
	ticket->uploads.resize(plan_count);
	ticket->result.instantiate();
	error = ticket->result->bytes.resize(request->expected_result_bytes);
	if (error != OK) {
		memdelete(ticket);
		return result(error, 19);
	}
	ticket->result->qualification_owned = true;
	qualification_serials.take(ticket->serial);
	qualification_serials.take(ticket->result->request_serial);
	ticket->original_readback_serial = ticket->result->request_serial;
	if (p_multi_readback) {
		for (auto &serial : ticket->additional_readback_serials) {
			qualification_serials.take(serial);
		}
	}
	for (uint32_t i = 0; i < plan_count; ++i) {
		auto &upload = ticket->uploads[i];
		upload.plan = plans[indices[i]];
		upload.progress.max_segments = (upload.plan.byte_size + 31) / 32;
		upload.progress.segments.reserve(upload.progress.max_segments);
	}
	auto &r = ticket->report;
	r.abi = { 1, sizeof(GDExtensionDeviceExitAbi), 0 };
	r.revision = 1;
	r.record_generation = request->record_generation;
	r.request_nonce = request->request_nonce;
	memcpy(r.request_digest, request->request_digest, 32);
	memcpy(r.unit_digest, request->unit_digest, 32);
	if (!bridge->cleanup_refs_reserved) {
		records[bridge->library].private_refs += 2;
		bridge->cleanup_refs_reserved = true;
	}
	bridge->tickets[slot] = ticket;
	++bridge->children;
	++records[bridge->library].private_refs;
	ticket->physically_retired = true;
	*out = { instance, ticket->serial };
	*report = r;
	return result(OK);
}
Error Registry::qualification_upload_destination(QualificationBridge *bridge,RenderingDevice *rd,const GDExtensionQualificationUploadPlanV1 &plan) {
 auto *buffer=rd->storage_buffer_owner.get_or_null(RID::from_uint64(plan.rid));
 if(!buffer || !buffer->driver_id || !buffer->backing_serial || buffer->backing_serial!=plan.backing_serial || plan.byte_offset>buffer->size || plan.byte_size>buffer->size-plan.byte_offset || !buffer->usage.has_flag(RDD::BUFFER_USAGE_TRANSFER_TO_BIT))return ERR_INVALID_DATA;
 if(buffer->usage.has_flag(RDD::BUFFER_USAGE_DYNAMIC_PERSISTENT_BIT) || buffer->transfer_worker_index>=0)return ERR_UNAVAILABLE;
 if(plan.owner_kind==GDEXTENSION_QUALIFICATION_UPLOAD_OWNER_E05_V1) {
  if(plan.e05_backing.registry_instance!=instance || !plan.e05_backing.handle_serial)return ERR_INVALID_DATA;
  bool found=false;for(auto *child:bridge->storage)if(child && child->serial==plan.e05_backing.handle_serial && !child->acknowledged && !child->retiring && child->backing.rid==plan.rid && child->backing.backing_serial==plan.backing_serial && buffer->qualification_return==&child->physical){found=true;break;}
  if(!found)return ERR_UNAUTHORIZED;
 } else if(plan.e05_backing.registry_instance || plan.e05_backing.handle_serial || buffer->qualification_return)return ERR_UNAUTHORIZED;
 return OK;
}
GDExtensionDeviceSessionResultV1 Registry::qualification_upload(GDExtensionQualificationBridgeRefV1 ref,GDExtensionQualificationTicketRefV1 ticket_ref,const GDExtensionQualificationUploadRequestV1 *request,const void *bytes,uint32_t byte_count,GDExtensionQualificationUploadSegmentV1 *segments,uint32_t capacity,GDExtensionQualificationUploadReportV1 *out) {
 if(!aligned(request)||!aligned(out)||!aligned(segments)||!bytes||!byte_count||!capacity||capacity>4096||!separate(request,sizeof(*request),bytes,byte_count)||!separate(request,sizeof(*request),out,sizeof(*out))||!separate(bytes,byte_count,out,sizeof(*out))||!separate(segments,sizeof(*segments)*capacity,out,sizeof(*out))||!separate(request,sizeof(*request),segments,sizeof(*segments)*capacity)||!separate(bytes,byte_count,segments,sizeof(*segments)*capacity))return result(ERR_INVALID_PARAMETER,2);
 *out={};memset(segments,0,sizeof(*segments)*capacity);
 if(!valid_abi(request->abi)||request->reserved||request->byte_size!=byte_count||!request->expected_revision)return result(ERR_INVALID_PARAMETER,2);
 QualificationBridge *bridge=nullptr;RenderingDevice *rd=nullptr;QualificationTicket *ticket=nullptr;QualificationTicket::Upload *upload=nullptr;Vector<uint8_t> copied;RdUnlock rd_unlock;
 auto copy_report=[&](){*out=upload->report;out->out_written=upload->progress.segments.size();for(uint32_t i=0;i<upload->progress.segments.size();++i){const auto &s=upload->progress.segments[i];auto &d=segments[i];d.abi={1,sizeof(GDExtensionDeviceExitAbi),0};d.upload_index=upload->plan.upload_index;d.segment_index=i;d.state=s.handle.is_valid()?1:2;d.byte_begin=upload->plan.byte_offset+s.begin;d.byte_end=upload->plan.byte_offset+s.end;d.graph_instance=s.handle.graph_instance;d.graph_frame=s.handle.graph_frame;d.graph_kind=s.handle.kind;d.graph_index=s.handle.index;d.graph_offset=s.handle.offset;d.backing_serial=upload->plan.backing_serial;}};
 {
  MutexLock lock(mutex);Error error=qualification_bridge_locked(ref,true,true,bridge,rd,8);if(error!=OK)return result(error,error==ERR_UNAUTHORIZED?15:16);
  error=qualification_ticket_locked(bridge,ticket_ref,ticket);if(error!=OK)return result(error,17);
  if(!ticket->prepared || ticket->admission_closed || ticket->recording || ticket->recorded || ticket->mapping_failed || ticket->append_failed)return result(ERR_BUSY,16);
  if(request->upload_index>=ticket->uploads.size())return result(ERR_INVALID_PARAMETER,2);upload=&ticket->uploads[request->upload_index];const auto &plan=upload->plan;
  if(request->record_generation!=ticket->preparation.record_generation || request->parent_serial!=ticket->preparation.parent_serial || request->rid!=plan.rid || request->backing_serial!=plan.backing_serial || request->byte_offset!=plan.byte_offset || request->byte_size!=plan.byte_size || request->role!=plan.role || request->owner_kind!=plan.owner_kind || memcmp(&request->e05_backing,&plan.e05_backing,sizeof(plan.e05_backing)) || memcmp(request->payload_digest,plan.payload_digest,32))return result(ERR_INVALID_DATA,16);
  if(capacity<upload->progress.max_segments)return result(ERR_PARAMETER_RANGE_ERROR,19);
  error=copied.resize(byte_count);if(error!=OK)return result(error,19);memcpy(copied.ptrw(),bytes,byte_count);uint8_t digest[32];error=CryptoCore::sha256(copied.ptr(),byte_count,digest);if(error!=OK)return result(error,16);if(memcmp(digest,plan.payload_digest,32))return result(ERR_INVALID_DATA,16);
  if(upload->attempted && request->expected_revision==upload->last_request_revision){copy_report();return result(OK);}
  if(request->expected_revision!=ticket->report.revision)return result(ERR_INVALID_DATA,17);
  if(upload->failed)return result(ERR_BUSY,16);
  if(!rd->_thread_safe_.try_lock())return result(ERR_BUSY,19);rd_unlock.mutex=&rd->_thread_safe_;
  error=qualification_upload_destination(bridge,rd,plan);if(error!=OK)return result(error,16);
  if(upload->progress.consumed==plan.byte_size){copy_report();return result(OK);}
  if(rd->_recording_error()!=OK || rd->draw_list.active || rd->compute_list.active || rd->raytracing_list.active || rd->frame_wait_draining || rd->draw_graph.has_driver_callbacks())return result(ERR_BUSY,19);
  LocalVector<RID> resources;resources.push_back(RID::from_uint64(plan.rid));error=rd->_qualification_pin_resources(ticket,resources);if(error!=OK)return result(error,16);
  ticket->recording=true;upload->attempted=true;upload->last_request_revision=request->expected_revision;
 }
 const uint32_t previous=upload->progress.segments.size();rd->qualification_recording=true;
 Error error=rd->_buffer_update(rd->storage_buffer_owner.get_or_null(RID::from_uint64(upload->plan.rid)),RID::from_uint64(upload->plan.rid),upload->plan.byte_offset,upload->plan.byte_size,copied.ptr(),&upload->progress);
 rd->qualification_recording=false;
 MutexLock lock(mutex);ticket->recording=false;
 ticket->continuation_pending=error==ERR_BUSY;
 for(uint32_t i=previous;i<upload->progress.segments.size();++i){const auto &segment=upload->progress.segments[i];add_graph(ticket,segment.handle,6,rd->frame,rd->frames[rd->frame].qualification_epoch,upload->plan.backing_serial,upload->plan.byte_offset+segment.begin,segment.end-segment.begin);}
 if(upload->progress.segments.size()>previous)rd->_qualification_add_frame_ticket(ticket);
 if (error != OK && error != ERR_BUSY) {
	 upload->failed = true;
	 ticket->append_failed = true;
	 if (ticket->report.native_error == OK) {
		 ticket->report.native_error = error;
	 }
 }
 ticket->observation_dirty=true;qualification_ticket_publish(ticket,rd);
 auto &r=upload->report;r={};r.abi={1,sizeof(GDExtensionDeviceExitAbi),0};r.revision=ticket->report.revision;r.record_generation=request->record_generation;r.parent_serial=request->parent_serial;r.upload_index=request->upload_index;r.native_error=error==ERR_BUSY?OK:error;r.state=ticket->mapping_failed?3:error==ERR_BUSY?4:error!=OK?(upload->progress.segments.is_empty()?0:2):1;
 r.known_count=upload->progress.segments.size();r.appended_begin=upload->plan.byte_offset;r.appended_end=upload->plan.byte_offset+upload->progress.consumed;r.unappended_begin=r.appended_end;r.unappended_end=upload->plan.byte_offset+upload->plan.byte_size;
 copy_report();return result(OK);
}
GDExtensionDeviceSessionResultV1 Registry::qualification_record_prepared(GDExtensionQualificationBridgeRefV1 ref,GDExtensionQualificationTicketRefV1 ticket_ref,const GDExtensionQualificationRecordV1 *request,GDExtensionQualificationReportV1 *out) {
 if(!aligned(request)||!aligned(out)||!separate(request,sizeof(*request),out,sizeof(*out)))return result(ERR_INVALID_PARAMETER,2);*out={};
 QualificationBridge *bridge=nullptr;RenderingDevice *rd=nullptr;QualificationTicket *ticket=nullptr;RdUnlock rd_unlock;
 {
  MutexLock lock(mutex);Error error=qualification_bridge_locked(ref,true,true,bridge,rd,9);if(error!=OK)return result(error,error==ERR_UNAUTHORIZED?15:16);
  error=qualification_ticket_locked(bridge,ticket_ref,ticket);if(error!=OK)return result(error,17);
  if (!ticket->prepared || ticket->multi_readback || ticket->admission_closed || ticket->recording || ticket->mapping_failed || ticket->append_failed) {
	  return result(ERR_BUSY, 16);
  }
  if(ticket->recorded){if(memcmp(&ticket->origin,request,sizeof(*request)))return result(ERR_INVALID_DATA,16);*out=ticket->report;return result(OK);}
  const auto &p=ticket->preparation;
  if(request->record_generation!=p.record_generation || request->request_nonce!=p.request_nonce || request->library_generation!=p.library_generation || request->qualification_session_serial!=p.qualification_session_serial || request->vector_ordinal!=p.vector_ordinal || request->attempt!=p.attempt || request->readback_size!=p.expected_result_bytes || memcmp(request->request_digest,p.request_digest,32) || memcmp(request->unit_digest,p.unit_digest,32))return result(ERR_INVALID_DATA,16);
  for(const auto &upload:ticket->uploads)if(!upload.attempted || upload.failed || upload.progress.consumed!=upload.plan.byte_size || upload.report.state!=1 || upload.report.possible_count)return result(ERR_BUSY,16);
  if(!rd->_thread_safe_.try_lock())return result(ERR_BUSY,19);rd_unlock.mutex=&rd->_thread_safe_;
  // Preserve the full original upload+record plan at prepared admission;
  // counting only remaining readback would shrink its cumulative reservation.
  uint64_t graph_bound=(p.expected_result_bytes+31)/32+2;
  for(const auto &upload:ticket->uploads)graph_bound+=(upload.plan.byte_size+31)/32;
  error=qualification_native_capacity(*rd->driver,rd->draw_graph.qualification_command_count(),graph_bound,rd->transfer_worker_pool_max_size);
  if(error!=OK)return result(error,error==ERR_UNAVAILABLE?18:19);
  LocalVector<RID> resources;error=qualification_record_preflight(bridge,rd,*request,resources);if(error!=OK)return result(error,error==ERR_UNAVAILABLE?18:error==ERR_PARAMETER_RANGE_ERROR?19:2);
  if(rd->_recording_error()!=OK || rd->draw_list.active || rd->compute_list.active || rd->raytracing_list.active || rd->frame_wait_draining || rd->draw_graph.has_driver_callbacks())return result(ERR_BUSY,19);
  // A previous-frame upload must already have its exact successful source and
  // returned staging. Current graph uploads get a consumed synchronization edge.
  for(const auto &graph:ticket->graphs)if(ticket->facts[graph.fact].semantic_kind==6 && (graph.frame!=uint32_t(rd->frame) || graph.epoch!=rd->frames[rd->frame].qualification_epoch)) {
   if(!graph.returned || ticket->facts[graph.fact].completion!=2)return result(ERR_BUSY,19);
  }
  error=rd->_qualification_pin_resources(ticket,resources);if(error!=OK)return result(error,16);
  ticket->origin=*request;ticket->report.source_backing_serial=request->observed_backing_serial;ticket->report.source_offset=request->readback_offset;ticket->report.source_size=request->readback_size;ticket->recording=true;
 }
 rd->draw_graph.add_synchronization();qualification_record_append(ticket,rd);
 MutexLock lock(mutex);*out=ticket->report;return result(OK);
}

GDExtensionDeviceSessionResultV1 Registry::qualification_cancel(GDExtensionQualificationBridgeRefV1 ref,GDExtensionQualificationTicketRefV1 ticket_ref) {
 {
  MutexLock lock(mutex);QualificationBridge *bridge=nullptr;RenderingDevice *rd=nullptr;QualificationTicket *ticket=nullptr;
  Error error=qualification_bridge_locked(ref,false,false,bridge,rd);if(error!=OK)return result(error,error==ERR_UNAUTHORIZED?15:17);
  error=qualification_ticket_locked(bridge,ticket_ref,ticket);if(error!=OK)return result(ERR_DOES_NOT_EXIST,17);
  if(!ticket->prepared)return result(ERR_BUSY,16);
  if(ticket->stop_requested)return result(OK);
  if(ticket->report.state==8 || (ticket->admission_closed && ticket->physically_retired.load()))return result(ERR_BUSY,16);
  // This is the only owner stop linearization point. Already admitted bounded
  // RT work keeps its ticket and pins; no owner code touches its graph here.
  ticket->stop_requested=true;ticket->admission_closed=true;ticket->continuation_pending=false;
  ticket->physically_retired.store(false);
 }
 const GDExtensionQualificationBudgetV1 budget{{1,sizeof(GDExtensionDeviceExitAbi),0},1,1000,0};
 const Error error=qualification_schedule(ref,ticket_ref.handle_serial,3,budget);
 return result(error,error==ERR_UNAUTHORIZED?15:error!=OK?17:0);
}

void Registry::qualification_report_v2(const QualificationTicket *ticket, GDExtensionQualificationReportV2 &out) const {
	out = {};
	memcpy(&out, &ticket->report, offsetof(GDExtensionQualificationReportV1, readback_request));
	out.abi.abi_version = 2;
	out.readback_count = ticket->range_origin.readback_count;
	out.readback_size = ticket->range_origin.readback_size;
	for (uint32_t i = 0; i < out.readback_count; ++i) {
		out.readbacks[i].source = ticket->range_origin.readbacks[i];
		const auto &owned = ticket->readback_result(i);
		if (owned.is_valid()) {
			out.readbacks[i].readback_request = ticket->readback_serial(i);
		}
	}
	out.owned_size = ticket->report.owned_size;
	memcpy(out.request_digest, ticket->report.request_digest, 32);
	memcpy(out.unit_digest, ticket->report.unit_digest, 32);
}
GDExtensionDeviceSessionResultV1 Registry::qualification_prepare_uploads_v2(GDExtensionQualificationBridgeRefV1 ref, const GDExtensionQualificationUploadPrepareV1 *request, const GDExtensionQualificationUploadPlanV1 *plans, uint32_t count, GDExtensionQualificationTicketRefV1 *out, GDExtensionQualificationReportV2 *report) {
	if (!aligned(request) || !aligned(plans) || !aligned(out) || !aligned(report) || !count || count > 128 || !separate(request, sizeof(*request), report, sizeof(*report)) || !separate(plans, sizeof(*plans) * count, report, sizeof(*report)) || !separate(out, sizeof(*out), report, sizeof(*report))) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	*report = {};
	GDExtensionQualificationReportV1 old = {};
	const auto status = qualification_prepare_uploads(ref, request, plans, count, out, &old, true);
	if (status.native_error != OK) {
		return status;
	}
	MutexLock lock(mutex);
	QualificationBridge *bridge = nullptr;
	RenderingDevice *rd = nullptr;
	QualificationTicket *ticket = nullptr;
	Error error = qualification_bridge_locked(ref, true, false, bridge, rd);
	if (error != OK) {
		return result(error, 16);
	}
	error = qualification_ticket_locked(bridge, *out, ticket);
	if (error != OK) {
		return result(error, 17);
	}
	qualification_report_v2(ticket, *report);
	return result(OK);
}
GDExtensionDeviceSessionResultV1 Registry::qualification_record_prepared_v2(GDExtensionQualificationBridgeRefV1 ref, GDExtensionQualificationTicketRefV1 ticket_ref, const GDExtensionQualificationRecordV2 *request, GDExtensionQualificationReportV2 *out) {
	if (!aligned(request) || !aligned(out) || !separate(request, sizeof(*request), out, sizeof(*out))) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	*out = {};
	const auto descriptor_error = validate_qualification_record_v2(*request);
	if (descriptor_error != OK) {
		return result(descriptor_error, 2);
	}
	QualificationBridge *bridge = nullptr;
	RenderingDevice *rd = nullptr;
	QualificationTicket *ticket = nullptr;
	RdUnlock rd_unlock;
	{
		MutexLock lock(mutex);
		Error error = qualification_bridge_locked(ref, true, true, bridge, rd, 10);
		if (error != OK) {
			return result(error, error == ERR_UNAUTHORIZED ? 15 : 16);
		}
		error = qualification_ticket_locked(bridge, ticket_ref, ticket);
		if (error != OK) {
			return result(error, 17);
		}
		if (!ticket->prepared || !ticket->multi_readback || ticket->admission_closed || ticket->recording || ticket->mapping_failed || ticket->append_failed) {
			return result(ERR_BUSY, 16);
		}
		if (ticket->recorded) {
			if (memcmp(&ticket->range_origin, request, sizeof(*request))) {
				return result(ERR_INVALID_DATA, 16);
			}
			qualification_report_v2(ticket, *out);
			return result(OK);
		}
		const auto &p = ticket->preparation;
		if (request->record_generation != p.record_generation || request->request_nonce != p.request_nonce || request->library_generation != p.library_generation || request->qualification_session_serial != p.qualification_session_serial || request->vector_ordinal != p.vector_ordinal || request->attempt != p.attempt || request->readback_size != p.expected_result_bytes || memcmp(request->request_digest, p.request_digest, 32) || memcmp(request->unit_digest, p.unit_digest, 32)) {
			return result(ERR_INVALID_DATA, 16);
		}
		for (const auto &upload : ticket->uploads) {
			if (!upload.attempted || upload.failed || upload.progress.consumed != upload.plan.byte_size || upload.report.state != 1 || upload.report.possible_count) {
				return result(ERR_BUSY, 16);
			}
		}
		if (!rd->_thread_safe_.try_lock()) {
			return result(ERR_BUSY, 19);
		}
		rd_unlock.mutex = &rd->_thread_safe_;
		uint64_t graph_bound = 2;
		for (uint32_t i = 0; i < request->readback_count; ++i) {
			graph_bound += (request->readbacks[i].readback_size + 31) / 32;
		}
		for (const auto &upload : ticket->uploads) {
			graph_bound += (upload.plan.byte_size + 31) / 32;
		}
		for (auto *image : ticket->texture_uploads) {
			graph_bound += image->upload.max_segments;
		}
		error = qualification_native_capacity(*rd->driver, rd->draw_graph.qualification_command_count(), graph_bound, rd->transfer_worker_pool_max_size);
		if (error != OK) {
			return result(error, error == ERR_UNAVAILABLE ? 18 : 19);
		}
		const auto legacy = qualification_record_v1(*request);
		LocalVector<RID> resources;
		error = qualification_record_preflight(bridge, rd, legacy, resources, request->capture_set1, request->body_set1, true);
		if (error != OK) {
			return result(error, error == ERR_UNAVAILABLE ? 18 : error == ERR_PARAMETER_RANGE_ERROR ? 19
																									: 2);
		}
		for (uint32_t i = 0; i < request->readback_count; ++i) {
			const auto &range = request->readbacks[i];
			RID rid = RID::from_uint64(range.readback_buffer);
			auto *buffer = rd->storage_buffer_owner.get_or_null(rid);
			uint32_t upload_role;
			switch (range.role) {
				case GDEXTENSION_QUALIFICATION_READBACK_NUMERIC_RAW_OUTPUT_V1:
					upload_role = GDEXTENSION_QUALIFICATION_UPLOAD_ROLE_RAW_OUTPUT_V1;
					break;
				case GDEXTENSION_QUALIFICATION_READBACK_METHOD_RAW_RESULTS_V1:
					upload_role = GDEXTENSION_QUALIFICATION_UPLOAD_ROLE_METHOD_RAW_RESULTS_V1;
					break;
				case GDEXTENSION_QUALIFICATION_READBACK_METHOD_DESTINATION_V1:
					upload_role = GDEXTENSION_QUALIFICATION_UPLOAD_ROLE_METHOD_RESOURCE_V1;
					break;
				case GDEXTENSION_QUALIFICATION_READBACK_METHOD_FAULT_PLANE_V1:
					upload_role = GDEXTENSION_QUALIFICATION_UPLOAD_ROLE_METHOD_FAULT_PLANE_V1;
					break;
				case GDEXTENSION_QUALIFICATION_READBACK_METHOD_RESULT_EPILOGUE_V1:
					upload_role = GDEXTENSION_QUALIFICATION_UPLOAD_ROLE_METHOD_RESULT_EPILOGUE_V1;
					break;
				default:
					return result(ERR_INVALID_PARAMETER, 2);
			}
			const bool method_range = range.role != GDEXTENSION_QUALIFICATION_READBACK_NUMERIC_RAW_OUTPUT_V1;
			const GDExtensionQualificationUploadPlanV1 *original_plan = nullptr;
			for (const auto &upload : ticket->uploads) {
				const auto &plan = upload.plan;
				// METHOD_RESOURCE identifies the logical Store destination; the
				// native allocation still belongs to its original E05 issuer.
				if (plan.role == upload_role && plan.rid == range.readback_buffer && plan.backing_serial == range.observed_backing_serial && plan.byte_offset == range.readback_offset && plan.byte_size == range.readback_size && (!method_range || plan.owner_kind == GDEXTENSION_QUALIFICATION_UPLOAD_OWNER_E05_V1)) {
					original_plan = &plan;
					break;
				}
			}
			if (!original_plan) {
				return result(ERR_UNAUTHORIZED, 16);
			}
			if (!buffer || !buffer->driver_id || buffer->backing_serial != range.observed_backing_serial || range.readback_offset > buffer->size || range.readback_size > buffer->size - range.readback_offset || !buffer->usage.has_flag(RDD::BUFFER_USAGE_TRANSFER_FROM_BIT) || buffer->transfer_worker_index >= 0 || buffer->usage.has_flag(RDD::BUFFER_USAGE_DYNAMIC_PERSISTENT_BIT)) {
				return result(ERR_INVALID_DATA, 2);
			}
			if (method_range && !buffer->qualification_return) {
				return result(ERR_UNAUTHORIZED, 16);
			}
			if (buffer->qualification_return) {
				bool original = false;
				for (auto *child : bridge->storage) {
					if (child && &child->physical == buffer->qualification_return && original_plan->e05_backing.registry_instance == instance && original_plan->e05_backing.handle_serial == child->serial && !child->creating && !child->retiring && !child->acknowledged && child->origin.allocation_generation == range.source_allocation_generation && child->backing.rid == range.readback_buffer && child->backing.backing_serial == range.observed_backing_serial && child->physical.bridge == bridge->serial && !child->physical.use_closed.load()) {
						original = true;
						break;
					}
				}
				if (!original) {
					return result(ERR_UNAUTHORIZED, 16);
				}
			}
			if (!add_resource(resources, rid)) {
				return result(ERR_PARAMETER_RANGE_ERROR, 19);
			}
		}
		if (rd->_recording_error() != OK || rd->draw_list.active || rd->compute_list.active || rd->raytracing_list.active || rd->frame_wait_draining || rd->draw_graph.has_driver_callbacks()) {
			return result(ERR_BUSY, 19);
		}
		for (const auto &graph : ticket->graphs) {
			if (ticket->facts[graph.fact].semantic_kind == 6 && !rd->draw_graph.contains_record(graph.handle)) {
				if (!graph.returned || ticket->facts[graph.fact].issue != 1 || ticket->facts[graph.fact].completion != 2 || ticket->facts[graph.fact].error) {
					return result(ERR_BUSY, 19);
				}
			}
		}
		error = qualification_typed_dependencies(bridge, rd, ticket, *request, resources);
		if (error != OK) {
			return result(error, 16);
		}
		Ref<RenderingDevice::BufferGetDataResult> owned[3];
		for (uint32_t i = 0; i < request->readback_count; ++i) {
			owned[i].instantiate();
			error = owned[i]->bytes.resize(request->readbacks[i].readback_size);
			if (error != OK) {
				return result(error, 19);
			}
			owned[i]->qualification_owned = true;
			owned[i]->request_serial = ticket->readback_serial(i);
		}
		error = rd->_qualification_pin_resources(ticket, resources);
		if (error != OK) {
			return result(error, 16);
		}
		ticket->origin = legacy;
		ticket->range_origin = *request;
		ticket->result = owned[0];
		for (uint32_t i = 1; i < request->readback_count; ++i) {
			ticket->additional_results[i - 1] = owned[i];
		}
		ticket->report.source_backing_serial = legacy.observed_backing_serial;
		ticket->report.source_offset = legacy.readback_offset;
		ticket->report.source_size = legacy.readback_size;
		ticket->recording = true;
	}
	rd->draw_graph.add_synchronization();
	qualification_record_append(ticket, rd);
	MutexLock lock(mutex);
	qualification_report_v2(ticket, *out);
	return result(OK);
}
GDExtensionDeviceSessionResultV1 Registry::qualification_observe_v2(GDExtensionQualificationBridgeRefV1 ref, GDExtensionQualificationTicketRefV1 ticket_ref, const GDExtensionQualificationBudgetV1 *budget, GDExtensionQualificationReportV2 *out) {
	if (!aligned(budget) || !aligned(out) || !separate(budget, sizeof(*budget), out, sizeof(*out))) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	*out = {};
	if (!valid_abi(budget->abi) || budget->reserved || budget->max_passes < 1 || budget->max_passes > 32 || budget->max_micros < 1 || budget->max_micros > 5000) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	{
		MutexLock lock(mutex);
		QualificationBridge *bridge = nullptr;
		RenderingDevice *rd = nullptr;
		QualificationTicket *ticket = nullptr;
		Error error = qualification_bridge_locked(ref, false, false, bridge, rd);
		if (error != OK) {
			return result(error, 17);
		}
		error = qualification_ticket_locked(bridge, ticket_ref, ticket);
		if (error != OK) {
			return result(error, 17);
		}
		if (!ticket->multi_readback) {
			return result(ERR_INVALID_DATA, 16);
		}
	}
	const Error error = qualification_schedule(ref, ticket_ref.handle_serial, 3, *budget);
	if (error != OK) {
		return result(error, 17);
	}
	MutexLock lock(mutex);
	QualificationBridge *bridge = nullptr;
	RenderingDevice *rd = nullptr;
	QualificationTicket *ticket = nullptr;
	Error observed = qualification_bridge_locked(ref, false, false, bridge, rd);
	if (observed != OK) {
		return result(observed, 17);
	}
	observed = qualification_ticket_locked(bridge, ticket_ref, ticket);
	if (observed != OK) {
		return result(observed, 17);
	}
	qualification_report_v2(ticket, *out);
	return result(OK);
}
} // namespace GDExtensionDeviceExit
