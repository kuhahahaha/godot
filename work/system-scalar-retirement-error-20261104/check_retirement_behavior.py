"""Run real service/observe/release/set-return decision bodies with scalar stand-ins.
No engine link, native completion, or GPU runtime is simulated as qualification.
Pass a git revision to execute those frozen production bodies for red evidence.
"""
import pathlib, subprocess, sys, re
root=pathlib.Path(__file__).resolve().parents[2]
out=pathlib.Path(__file__).resolve().parent
revision=sys.argv[1] if len(sys.argv)>1 else None

def read(path):
    if revision:
        return subprocess.check_output(['git','show',revision+':'+path],cwd=root,text=True)
    return (root/path).read_text()
def body(text,marker):
    start=text.index('{',text.index(marker));end=start+1;depth=1
    while depth:
        if text[end]=='{': depth+=1
        if text[end]=='}': depth-=1
        end+=1
    return text[start+1:end-1]

system=read('core/extension/gdextension_system_scalar.cpp')
internal=read('core/extension/gdextension_qualification_internal.h')
qualification=read('core/extension/gdextension_qualification.cpp')
ticket=read('core/extension/gdextension_qualification_ticket.cpp')
remember=body(internal,'void remember_system_set_return_error(') if 'void remember_system_set_return_error(' in internal else ''
start=qualification.index('const Error error=', qualification.index('void Registry::qualification_service('))
service=qualification[start:qualification.index('if(error!=OK)break;',start)]
observe=body(system,'Registry::system_scalar_observe(')
start=observe.index('bool found = false;')
preobserve=observe[start:observe.index('if (!found)',start)]+'return scalar_result(OK);'
start=observe.rindex('for (auto *ticket : bridge->tickets)')
postobserve=observe[start:]
release=body(ticket,'Registry::qualification_release(')
release=release[:release.index('// Writers of physical_uses')]
setstep=body(system,'Registry::system_scalar_set_return_step(')
cpp=r'''#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>
using Error=int;constexpr Error OK=0,FAILED=1,ERR_BUSY=2,ERR_INVALID_DATA=3,ERR_DOES_NOT_EXIST=4;
struct Mutex{bool try_lock(){return true;}void unlock(){}};
struct MutexLock{explicit MutexLock(Mutex&) {}};
struct ScalarUnlock{Mutex* mutex=nullptr;~ScalarUnlock(){if(mutex)mutex->unlock();}};
struct Flag{bool value=false;bool load()const{return value;}void store(bool b){value=b;}};
struct RID{uint64_t id=0;bool is_null()const{return !id;}bool is_valid()const{return id!=0;}};
struct Physical{Mutex mutex;uint64_t free_event=0;bool event_capacity_reserved=false,enqueued=false;Flag use_closed;struct {uint32_t value=0;uint32_t load()const{return value;}} dependent_children;};
struct Report{uint32_t state=8;uint64_t revision=23;int64_t native_code=-41;Error native_error=OK;};
struct QualificationTicket{
 bool system_scalar=true,system_set_return_requested=false,acknowledged=false,recording=false,queued=false,append_failed=false;
 uint32_t accepted_rt_refs=0,frame_refs=0;uint64_t serial=17;Flag physically_retired{true};RID system_set{7};Physical system_set_physical;
 Error system_set_return_error=OK;Report report,system_report;
 void remember_system_set_return_error(Error p_error){REMEMBER}
};
struct Result{Error native_error;uint32_t reason;uint64_t expected;uint64_t actual;};
Result scalar_result(Error error,uint32_t reason=0){return {error,reason,0,0};}Result result(Error error,uint32_t reason=0){return scalar_result(error,reason);}
void scalar_report(const QualificationTicket* ticket,Report*out){*out=ticket->system_report;}
struct RenderingDevice{
 QualificationTicket *ticket=nullptr;Error maintenance_error=OK;uint32_t frees=0;bool frame_wait_draining=false,qualification_retiring=false;
 Mutex _thread_safe_;struct{bool active=false;}draw_list,compute_list,raytracing_list;struct{bool has_driver_callbacks(){return false;}}draw_graph;
 struct{uint64_t qualification_epoch=1;}frames[1];uint32_t frame=0;bool free_during_maintenance=false;
 struct Set{Physical *qualification_return=nullptr;uint32_t qualification_users=0;}set;
 struct Owner{Set *set=nullptr;bool live=true;Set* get_or_null(RID){return live?set:nullptr;}bool owns(RID){return live;}}uniform_set_owner;
 Error _qualification_maintenance(){if(free_during_maintenance)ticket->system_set_physical.free_event=9;return maintenance_error;}
 Error _recording_error(){return OK;}bool _qualification_free_event_available(bool){return true;}
 void free_rid(RID){++frees;uniform_set_owner.live=false;ticket->system_set_physical.enqueued=true;}
};
struct Resource{uint64_t serial=0;Error maintenance_error=OK;struct{uint32_t state=0,return_status=0,reason=0;Error native_error=OK;uint64_t revision=0,report_revision=0;}report;};
using QualificationResource=Resource;
struct QualificationBridge{std::vector<QualificationTicket*>tickets;std::vector<Resource*>storage,units;};
struct Ref{uint64_t registry_instance=71,handle_serial=17;};
struct Registry{
 Mutex mutex;uint64_t instance=71;QualificationBridge bridge_storage;RenderingDevice device;
 uint32_t native_release_frontier=0,ack_frontier=0;
 Registry(QualificationTicket* t){bridge_storage.tickets.push_back(t);device.ticket=t;device.set.qualification_return=&t->system_set_physical;device.uniform_set_owner.set=&device.set;}
 Error qualification_bridge_locked(Ref,bool,bool,QualificationBridge*&b,RenderingDevice*&rd){b=&bridge_storage;rd=&device;return OK;}
 Error qualification_ticket_locked(QualificationBridge*b,Ref,QualificationTicket*&t){t=b->tickets[0];return OK;}
 Error qualification_resource_locked(QualificationBridge*,Ref,uint32_t,QualificationResource*&){return ERR_DOES_NOT_EXIST;}
 void qualification_retire_step(QualificationBridge*,RenderingDevice*,uint64_t,uint32_t){}
 void qualification_ticket_step(QualificationTicket*,RenderingDevice*){}
 void qualification_ticket_publish(QualificationTicket*,RenderingDevice*){}
 void system_scalar_set_return_step(QualificationTicket* ticket,RenderingDevice* rd){SETSTEP}
 void service(Error injected){auto* bridge=&bridge_storage;auto* rd=&device;device.maintenance_error=injected;uint32_t kind=3;uint64_t serial=17;SERVICE}
 Result preobserve(Report*out){auto*bridge=&bridge_storage;Ref ticket_ref;PREOBSERVE}
 Result postobserve(Report*out){auto*bridge=&bridge_storage;Ref ticket_ref;POSTOBSERVE}
 Result release(){Ref ref,ticket_ref;bool p_system_scalar=true;RELEASE
 ++native_release_frontier;
 if(ticket->system_set.is_valid()&&!ticket->system_set_physical.free_event)return result(ERR_BUSY,16);
 ++ack_frontier;ticket->acknowledged=true;return result(OK);
 }
};
bool same_report(const Report&a,const Report&b){return a.state==b.state&&a.revision==b.revision&&a.native_code==b.native_code&&a.native_error==b.native_error;}
int main(){uint32_t failures=0;auto check=[&](bool yes,const char*why){if(!yes){++failures;std::cout<<"FAIL "<<why<<'\n';}};
 QualificationTicket t;Registry r(&t);const Report before=t.system_report;
 r.service(FAILED);
 check(t.system_set_return_error==FAILED,"maintenance failure retained after compute completion");
 check(r.device.frees==0,"maintenance failure prevents another set free enqueue");
 r.service(ERR_INVALID_DATA);r.service(ERR_BUSY);r.service(OK);
 check(t.system_set_return_error==FAILED,"later error Busy or success cannot clear first failure");
 Report observed{};observed.state=0;observed.revision=0;observed.native_code=0;auto pre=r.preobserve(&observed);
 check(pre.native_error==FAILED,"observe surfaces stored failure before scheduling");
 check(same_report(observed,before),"observe error still returns original completed report");
 auto post=r.postobserve(&observed);check(post.native_error==FAILED,"observe after service surfaces stored failure");
 auto released=r.release();check(released.native_error==FAILED,"release returns stored error instead of Busy");
 check(r.native_release_frontier==0&&r.ack_frontier==0&&!t.acknowledged,"failed release changes no native pins or ACK");
 check(pre.expected==0&&pre.actual==0&&post.expected==0&&post.actual==0&&released.expected==0&&released.actual==0,"Error-only result preserves zero expected/actual slots");
 check(same_report(t.report,before)&&same_report(t.system_report,before),"report revision and original native code remain immutable");
 // A queued free remains quarantined on error; a later real free is not an
 // automatic error reset or permission to ACK.
 QualificationTicket queued;queued.system_set_return_requested=true;queued.system_set_physical.enqueued=true;Registry qr(&queued);qr.service(FAILED);queued.system_set_physical.free_event=19;
 check(queued.system_set_return_error==FAILED&&qr.release().native_error==FAILED&&!queued.acknowledged,"queued-free error stays sticky after physical return");
 QualificationTicket freed;freed.system_set_physical.free_event=9;freed.system_set_return_requested=true;Registry fr(&freed);fr.service(FAILED);
 check(freed.system_set_return_error==OK,"already-free set is not polluted by unrelated maintenance failure");
 QualificationTicket during;during.system_set_return_requested=true;Registry dr(&during);dr.device.free_during_maintenance=true;dr.service(FAILED);
 check(during.system_set_return_error==OK,"set freed by maintenance is not retroactively faulted");
 QualificationTicket healthy;healthy.system_set_return_requested=true;Registry hr(&healthy);hr.service(ERR_BUSY);
 check(healthy.system_set_return_error==OK&&hr.release().native_error==ERR_BUSY,"ordinary pending retirement remains Busy without sticky failure");
 QualificationTicket corrupt;Registry cr(&corrupt);cr.device.uniform_set_owner.live=false;cr.system_scalar_set_return_step(&corrupt,&cr.device);
 check(corrupt.system_set_return_error==ERR_INVALID_DATA,"actual set-owner mismatch becomes persistent retirement error");
 std::cout<<"failures="<<failures<<'\n';return failures?1:0;
}
'''
for key,value in [('REMEMBER',remember),('SETSTEP',setstep),('SERVICE',service),('PREOBSERVE',preobserve),('POSTOBSERVE',postobserve),('RELEASE',release)]:cpp=re.sub(r'\b'+key+r'\b',lambda _:value,cpp)
(out/'retirement_behavior.cpp').write_text('\n'.join(line.rstrip() for line in cpp.splitlines())+'\n')
subprocess.run(['clang++','-std=c++17','-O0',str(out/'retirement_behavior.cpp'),'-o',str(out/'retirement_behavior')],check=True)
sys.exit(subprocess.run([str(out/'retirement_behavior')]).returncode)
