"""Execute TESTS-only production projection hooks with scalar stand-ins.
This proves hook gates only; the native runner must independently prove real
append/compute/completion and retained owners. No engine or GPU is run here.
"""
from pathlib import Path
import re,subprocess
root=Path(__file__).resolve().parents[2];out=Path(__file__).resolve().parent
source=(root/'core/extension/gdextension_qualification_internal.h').read_text()
def body(marker):
 start=source.index('{',source.index(marker));end=start+1;depth=1
 while depth:
  if source[end]=='{':depth+=1
  if source[end]=='}':depth-=1
  end+=1
 return source[start+1:end-1]
cpp=r'''#include <cstdint>
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
 bool test_inject_system_append_unknown(){APPEND}
 bool test_inject_system_set_return_error(){RETIRE}
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
'''
for key,value in [('APPEND',body('bool test_inject_system_append_unknown(')),('RETIRE',body('bool test_inject_system_set_return_error('))]:cpp=re.sub(r'\b'+key+r'\b',lambda _:value,cpp)
(out/'test_hook_behavior.cpp').write_text(cpp)
subprocess.run(['clang++','-std=c++17','-O0',str(out/'test_hook_behavior.cpp'),'-o',str(out/'test_hook_behavior')],check=True)
raise SystemExit(subprocess.run([str(out/'test_hook_behavior')]).returncode)
