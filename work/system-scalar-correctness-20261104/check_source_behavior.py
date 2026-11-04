"""Execute original production decision bodies with scalar stand-ins, not an engine/GPU test.
The baseline adapter extracts the existing inline admission/append blocks; after
refactoring it executes their production helper bodies. Assertions are behavioral.
"""
import pathlib, subprocess, sys, re
root = pathlib.Path(sys.argv[1]).resolve() if len(sys.argv)>1 else pathlib.Path(__file__).resolve().parents[2]
out = pathlib.Path(__file__).resolve().parent

def body(text, marker):
    at=text.index('{', text.index(marker)); end=at+1; depth=1
    while depth:
        if text[end]=='{': depth+=1
        if text[end]=='}': depth-=1
        end+=1
    return text[at+1:end-1]

system=(root/'core/extension/gdextension_system_scalar.cpp').read_text()
internal=(root/'core/extension/gdextension_qualification_internal.h').read_text()
ticket=(root/'core/extension/gdextension_qualification_ticket.cpp').read_text()
source=(root/'core/extension/gdextension_record_source.cpp').read_text()
capacity=body(ticket, 'Error qualification_native_capacity(')
arithmetic=body(source, 'Error qualification_source_slot_bound(')
if 'void finish_system_append(' in internal:
    finish=body(internal, 'void finish_system_append(')
else:
    start=system.index('ticket->recording = false;', system.index('rd->qualification_recording = false;'))
    finish=system[start:system.index('qualification_ticket_publish(ticket, rd);', start)].replace('ticket->','').replace('append_error','p_error')
if 'Error system_scalar_native_capacity(' in system:
    scalar_capacity=body(system,'Error system_scalar_native_capacity(')
else:
    # Baseline performs no native/fact admission at this point. Execute its
    # actual last admission block, with sufficient metadata/serial capacity.
    start=system.index('uint32_t slot = 0;')
    scalar_capacity=system[start:system.index('ticket = memnew(', start)]+'\nreturn OK;'
admission_start=system.index('uint32_t slot = 0;')
admission=system[admission_start:system.index('ticket = memnew(', admission_start)]
# These are original publish predicates, executed after the original finish
# decision. No graph/native/frame facts are invented for an unknown append.
start=ticket.index('const bool sources_returned=')
stop=ticket.index('if(no_effect)',start)
predicates=ticket[start:stop]
start=ticket.index('if(ticket->append_failed)ticket->admission_closed=true;')
stop=ticket.index('if(ticket->observation_dirty ||',start)
retirement=ticket[start:stop]

cpp=r'''#include <cstdint>
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
 void finish_system_append(Error p_error) { FINISH }
};
struct RenderingDeviceDriver {uint32_t bound=0,calls=0,main=0,workers=0,frames=0;
 uint32_t qualification_native_call_bound_for_profile(uint32_t m,uint32_t w,uint32_t f){++calls;main=m;workers=w;frames=f;return bound;}
};
Error qualification_source_slot_bound(uint64_t graphs,uint32_t current,uint32_t workers,uint32_t capacity,uint32_t &slots){ ARITHMETIC }
Error qualification_native_capacity(RenderingDeviceDriver &driver,uint32_t current_commands,uint64_t graph_bound,uint32_t workers){ CAPACITY }
struct QualificationBridge { static constexpr uint32_t CHILD_CAPACITY=2;void* tickets[2]={};bool cleanup_refs_reserved=false;uint32_t library=0;};
QualificationBridge storage,*bridge=&storage;FakeRD rd_storage,*rd=&rd_storage;constexpr uint32_t private_limit=100;
struct {uint32_t private_refs=0;} records[1];
bool qualification_serial_capacity(uint32_t){return true;} Error scalar_result(Error error,int){return error;}
Error system_scalar_native_capacity(RenderingDeviceDriver &driver,uint32_t current_commands,uint32_t workers,uint32_t bytes,uint32_t staging_block_bytes){ SCALAR_CAPACITY }
uint32_t admitted_publications=0;
Error actual_admission(RenderingDeviceDriver &driver,uint32_t current_commands,uint32_t workers,uint32_t bytes,uint32_t staging_block_bytes) {
 rd->driver=&driver;rd->draw_graph.commands=current_commands;rd->transfer_worker_pool_max_size=workers;rd->download_staging_buffers.block_size=staging_block_bytes;Error error=OK;
 ADMISSION
 ++admitted_publications;
 return OK;
}
bool retired(QualificationTicket *ticket,FakeRD* rd) {
 bool mapped_all=true,returned=true,native_success=true,has_native=false;
 struct {int state=0;int phase=0;int owned_size=0;} next;
 PREDICATES
 RETIREMENT
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
'''
for key,value in [('FINISH',finish),('ARITHMETIC',arithmetic),('CAPACITY',capacity),('SCALAR_CAPACITY',scalar_capacity),('ADMISSION',admission),('PREDICATES',predicates),('RETIREMENT',retirement)]: cpp=re.sub(r"\b"+key+r"\b",lambda _:value,cpp)
(out/'source_behavior.cpp').write_text('\n'.join(line.rstrip() for line in cpp.splitlines())+'\n')
subprocess.run(['clang++','-std=c++17','-O0',str(out/'source_behavior.cpp'),'-o',str(out/'source_behavior')],check=True)
sys.exit(subprocess.run([str(out/'source_behavior')]).returncode)
