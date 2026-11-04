#include <cstdint>
#include <iostream>
#include <vector>
#include <algorithm>
template<class T> struct Vec : std::vector<T> { bool is_empty()const{return this->empty();} };
using Error=int;
constexpr Error OK=0, FAILED=1, ERR_BUSY=2, ERR_PARAMETER_RANGE_ERROR=3, ERR_UNAVAILABLE=4;
struct CheckedRecordAppend { enum class State { Rejected, Appended, Unknown }; State state=State::Rejected; Error error=OK; };
struct RenderingDeviceDriver;
struct FakeRD { RenderingDeviceDriver *driver=nullptr; struct { uint32_t commands=0; uint32_t qualification_command_count(){return commands;} } draw_graph; uint32_t transfer_worker_pool_max_size=0; struct {uint32_t block_size=32;} download_staging_buffers; bool unpinned=false; uint64_t next_qualification_object_serial=1; template<class T> void _qualification_unpin_resources(T*){unpinned=true;} };
struct Flag { bool value=false; void store(bool b){value=b;} operator bool()const{return value;} Flag& operator=(bool b){value=b;return *this;} };
struct QualificationTicket {
 static constexpr uint32_t FACT_CAPACITY=4096;
 struct Upload { bool attempted=false; struct { bool consumed=false; Vec<int> segments; } progress; };
 struct Image {bool upload_attempted=false;};
 bool system_scalar=true,recording=true,mapping_failed=false,observation_dirty=false,append_failed=false,prepared=false,continuation_pending=false;
 Flag admission_closed,physically_retired;
 uint32_t frame_refs=0; Vec<int> graphs,natives;Vec<Upload> uploads;Vec<Image*> texture_uploads;
 CheckedRecordAppend body_append;
 struct {Error native_error=OK;} report;
 void finish_system_append(Error p_error) { recording = false;
		if (p_error != OK) { append_failed = true; if (report.native_error == OK) report.native_error = p_error; }
		 }
};
struct RenderingDeviceDriver {uint32_t bound=0,calls=0,main=0,workers=0,frames=0;
 uint32_t qualification_native_call_bound_for_profile(uint32_t m,uint32_t w,uint32_t f){++calls;main=m;workers=w;frames=f;return bound;}
};
Error qualification_source_slot_bound(uint64_t graphs,uint32_t current,uint32_t workers,uint32_t capacity,uint32_t &slots){
	slots = 0;
	if (graphs > capacity || graphs == UINT64_MAX) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	const uint64_t frames = graphs + 1;
	const uint64_t base = uint64_t(current) + frames;
	if (base > capacity || workers > (capacity - base) / frames) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	slots = uint32_t(base + uint64_t(workers) * frames);
	return OK;
 }
Error qualification_native_capacity(RenderingDeviceDriver &driver,uint32_t current_commands,uint64_t graph_bound,uint32_t workers){
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
struct QualificationBridge { static constexpr uint32_t CHILD_CAPACITY=2;void* tickets[2]={};bool cleanup_refs_reserved=false;uint32_t library=0;};
QualificationBridge storage,*bridge=&storage;FakeRD rd_storage,*rd=&rd_storage;constexpr uint32_t private_limit=100;
struct {uint32_t private_refs=0;} records[1];
bool qualification_serial_capacity(uint32_t){return true;} Error scalar_result(Error error,int){return error;}
Error system_scalar_native_capacity(RenderingDeviceDriver &driver,uint32_t current_commands,uint32_t workers,uint32_t bytes,uint32_t staging_block_bytes){ uint32_t slot = 0;
		for (; slot < QualificationBridge::CHILD_CAPACITY && bridge->tickets[slot]; ++slot) {}
		const uint32_t refs_needed = bridge->cleanup_refs_reserved ? 1 : 3;
		if (slot == QualificationBridge::CHILD_CAPACITY || private_limit < refs_needed || records[bridge->library].private_refs > private_limit - refs_needed || !qualification_serial_capacity(2) || !rd->next_qualification_object_serial || rd->next_qualification_object_serial == UINT64_MAX) return scalar_result(ERR_PARAMETER_RANGE_ERROR, 19);

return OK; }
uint32_t admitted_publications=0;
Error actual_admission(RenderingDeviceDriver &driver,uint32_t current_commands,uint32_t workers,uint32_t bytes,uint32_t staging_block_bytes) {
 rd->driver=&driver;rd->draw_graph.commands=current_commands;rd->transfer_worker_pool_max_size=workers;rd->download_staging_buffers.block_size=staging_block_bytes;Error error=OK;
 uint32_t slot = 0;
		for (; slot < QualificationBridge::CHILD_CAPACITY && bridge->tickets[slot]; ++slot) {}
		const uint32_t refs_needed = bridge->cleanup_refs_reserved ? 1 : 3;
		if (slot == QualificationBridge::CHILD_CAPACITY || private_limit < refs_needed || records[bridge->library].private_refs > private_limit - refs_needed || !qualification_serial_capacity(2) || !rd->next_qualification_object_serial || rd->next_qualification_object_serial == UINT64_MAX) return scalar_result(ERR_PARAMETER_RANGE_ERROR, 19);

 ++admitted_publications;
 return OK;
}
bool retired(QualificationTicket *ticket,FakeRD* rd) {
 bool mapped_all=true,returned=true,native_success=true,has_native=false;
 struct {int state=0;int phase=0;int owned_size=0;} next;
 const bool sources_returned=mapped_all && returned && !ticket->frame_refs && native_success && !ticket->mapping_failed && (ticket->graphs.is_empty() || has_native);
 bool live_upload_claim = false;
 for (const auto &upload : ticket->uploads) {
  live_upload_claim = live_upload_claim || upload.attempted || upload.progress.consumed || !upload.progress.segments.is_empty();
 }
 for (auto *image : ticket->texture_uploads) {
	 live_upload_claim = live_upload_claim || image->upload_attempted;
 }
 const bool no_effect=ticket->graphs.is_empty() && ticket->natives.is_empty() && !ticket->mapping_failed && !ticket->recording && !ticket->frame_refs;

 if(ticket->append_failed)ticket->admission_closed=true;
 if(ticket->admission_closed)ticket->continuation_pending=false;
 const bool failed_prefix_settled=ticket->append_failed && sources_returned && !ticket->recording;
 const bool cancelled_upload_settled=(ticket->prepared || ticket->system_scalar) && ticket->admission_closed && (sources_returned || (no_effect && !live_upload_claim)) && !ticket->recording;
 if(cancelled_upload_settled){next.state=7;next.phase=6;next.owned_size=0;}
 // A prepared ticket whose upload has already attempted an effect still owns
 // its original writer claim when the checked call returns with no graph
 // prefix (for example ERR_BUSY before the first segment). It must remain
 // non-retired until cancellation or a settled failure releases that claim.
 if((no_effect && !live_upload_claim) || next.state==8 || cancelled_upload_settled || failed_prefix_settled) {
  rd->_qualification_unpin_resources(ticket);ticket->physically_retired.store(true);
 }

 return ticket->physically_retired;
}
int main(){int failures=0;auto check=[&](bool ok,const char* text){if(!ok){std::cout<<"FAIL "<<text<<'\n';++failures;}};
 QualificationTicket q;q.body_append.state=CheckedRecordAppend::State::Unknown;q.body_append.error=FAILED;q.finish_system_append(FAILED);
 FakeRD owner;check(q.mapping_failed,"Unknown marks missing mapping");check(!retired(&q,&owner),"Unknown does not retire without native facts");check(!owner.unpinned,"Unknown retains resource pins");check(q.admission_closed&&!q.continuation_pending,"Unknown closes future continuation");
 q.finish_system_append(ERR_BUSY);check(q.report.native_error==FAILED,"first native cause survives retry");
 QualificationTicket known;known.body_append.state=CheckedRecordAppend::State::Rejected;known.finish_system_append(ERR_BUSY);FakeRD known_owner;check(retired(&known,&known_owner),"known no-effect rejection may retire");
 RenderingDeviceDriver d;d.bound=4094;check(actual_admission(d,7,2,36,32)==ERR_PARAMETER_RANGE_ERROR,"over native+graph bound refuses admission");
 check(admitted_publications==0,"refusal precedes ticket/serial publication");
 check(d.calls==1&&d.main==11&&d.workers==8&&d.frames==4,"original driver observes complete continuation profile");
 d.bound=4093;check(actual_admission(d,7,2,36,32)==OK,"last fitting native bound admitted");
 d.bound=0;check(actual_admission(d,7,2,36,32)==ERR_UNAVAILABLE,"unsupported driver bound refuses admission");
 auto calls=d.calls;check(actual_admission(d,7,2,36,16)==ERR_UNAVAILABLE,"sub32 staging cannot use copy bound");check(d.calls==calls,"invalid staging rejected before driver");
 check(actual_admission(d,UINT32_MAX,2,36,32)==ERR_PARAMETER_RANGE_ERROR,"slot arithmetic cannot wrap");
 d.bound=3583;check(actual_admission(d,0,0,16384,32)==OK,"16KiB last fitting native bound");d.bound=3584;check(actual_admission(d,0,0,16384,32)==ERR_PARAMETER_RANGE_ERROR,"16KiB one-over native bound");
 std::cout<<"failures="<<failures<<'\n';return failures?1:0;
}
