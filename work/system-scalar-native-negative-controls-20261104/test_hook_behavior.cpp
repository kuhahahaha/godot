#include <cstdint>
#include <iostream>
constexpr int OK=0,FAILED=1;
struct Mutex{};struct MutexLock{MutexLock(Mutex&) {}};
struct Flag{bool value=false;bool load()const{return value;}};
struct Handle{uint64_t graph_instance=0,graph_frame=0;uint32_t kind=0;bool is_valid()const{return graph_instance&&graph_frame&&kind;}};
struct RID{uint64_t id=0;bool is_null()const{return id==0;}};
struct CheckedRecordAppend{enum class State{Rejected,Appended,Unknown};State state=State::Rejected;Handle handle;int error=OK;};
struct Ticket{
 bool system_scalar=true,test_system_append_unknown_injected=false,test_system_set_return_error_injected=false;
 CheckedRecordAppend body_append;Flag physically_retired;RID system_set;int system_set_return_error=OK;
 struct{Mutex mutex;uint64_t free_event=0;}system_set_physical;
 bool test_inject_system_append_unknown(){
  if (!system_scalar || test_system_append_unknown_injected || body_append.state != CheckedRecordAppend::State::Appended || !body_append.handle.is_valid()) return false;
  test_system_append_unknown_injected = true;
  body_append.state = CheckedRecordAppend::State::Unknown;
  body_append.handle = {};
  body_append.error = FAILED;
  return true;
 }
 bool test_inject_system_set_return_error(){
  if (!system_scalar || test_system_set_return_error_injected || !physically_retired.load() || system_set.is_null() || system_set_return_error != OK) return false;
  MutexLock lock(system_set_physical.mutex);
  if (system_set_physical.free_event) return false;
  test_system_set_return_error_injected = true;
  return true;
 }
};
int main(){int failed=0;auto check=[&](bool yes,const char*why){if(!yes){++failed;std::cout<<"FAIL "<<why<<'\n';}};
 Ticket t;check(!t.test_inject_system_append_unknown(),"unrecorded work is not injected");
 t.body_append.state=CheckedRecordAppend::State::Appended;check(!t.test_inject_system_append_unknown(),"missing graph is not claimed as original append");
 t.body_append.handle={11,12,1};check(t.test_inject_system_append_unknown(),"actual append observation is projected Unknown");
 check(t.body_append.state==CheckedRecordAppend::State::Unknown&&!t.body_append.handle.is_valid()&&t.body_append.error==FAILED,"projection keeps uncertainty and removes unusable handle");
 t.body_append.state=CheckedRecordAppend::State::Appended;t.body_append.handle={11,12,1};check(!t.test_inject_system_append_unknown()&&t.body_append.handle.is_valid(),"append injection fires once only");
 Ticket q;q.system_scalar=false;q.body_append=t.body_append;check(!q.test_inject_system_append_unknown(),"qualification append excluded");
 Ticket r;check(!r.test_inject_system_set_return_error(),"no set is not retirement");r.system_set.id=7;check(!r.test_inject_system_set_return_error(),"compute still pending excluded");r.physically_retired.value=true;
 r.system_set_physical.free_event=9;check(!r.test_inject_system_set_return_error(),"already free excluded");r.system_set_physical.free_event=0;check(r.test_inject_system_set_return_error(),"unfinished set retirement eligible");check(r.system_set_return_error==OK,"hook selects Error result without bypassing real sticky service");check(!r.test_inject_system_set_return_error(),"set failure injection fires once only");
 q.system_set.id=7;q.physically_retired.value=true;check(!q.test_inject_system_set_return_error(),"qualification retirement excluded");Ticket failed_set;failed_set.system_set.id=7;failed_set.physically_retired.value=true;failed_set.system_set_return_error=FAILED;check(!failed_set.test_inject_system_set_return_error(),"existing original error is not replaced by injection");
 std::cout<<"failures="<<failed<<'\n';return failed?1:0;
}
