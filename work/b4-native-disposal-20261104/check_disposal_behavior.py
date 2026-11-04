#!/usr/bin/env python3
"""Execute real cell decision bodies and real pending-free return-site loops.
Only Ref storage, driver and queue are stand-ins; not linked engine/GPU evidence.
"""
from pathlib import Path
import subprocess,sys
root=Path(__file__).resolve().parents[2]
source=(root/'servers/rendering/rendering_device.cpp').read_text()
header=(root/'servers/rendering/native_disposal_witness.h').read_text()
mode=sys.argv[1] if len(sys.argv)>1 else 'green'
src=subprocess.check_output(['git','show','8355b9a7ae2fb5e0ceaaa8453021865df16add62:servers/rendering/rendering_device.cpp'],cwd=root,text=True) if mode=='red' else source
def balanced(s,start):
 b=s.index('{',start);depth=1;end=b+1
 while depth:
  depth+=(s[end]=='{')-(s[end]=='}');end+=1
 return s[start:end]
def loop(label):
 return balanced(src,src.index('while (',src.index(label,src.index('void RenderingDevice::_free_pending_resources('))))
cell=balanced(header,header.index('class Cell : public RefCounted'))+';'
cell=cell.replace('class Cell : public RefCounted','struct Cell')
returned=balanced(source,source.index('void RenderingDevice::_native_disposal_returned(')).replace('RenderingDevice::','')
text='''#include <cstdio>
#include <cstdint>
#include <deque>
#include <memory>
enum class State : uint8_t { EMPTY, LIVE, ENQUEUED, RETURNED, UNKNOWN };
'''+cell+'''
template<class T>struct Ref {std::shared_ptr<T> p=std::make_shared<T>();bool is_valid()const{return bool(p);}T*operator->()const{return p.get();}};
struct NativeDisposalWitness {using State=::State;Ref<Cell> cell;};
struct ID {uint64_t id;ID(uint64_t v):id(v){}operator uint64_t()const{return id;}};
struct Buffer {ID driver_id=19;int qualification_return=0;NativeDisposalWitness native_disposal;uint64_t native_disposal_rid=7,backing_serial=11;int size=128;};
struct UniformSet {ID driver_id=23;int qualification_return=0;std::deque<int*> qualification_dependencies;NativeDisposalWitness native_disposal;uint64_t native_disposal_rid=8,native_disposal_serial=12;};
template<class T>struct Queue {struct Node {T value;T&get(){return value;}};std::deque<Node> q;Node*front(){return q.empty()?nullptr:&q.front();}void pop_front(){q.pop_front();}};
int failures=0;void check(bool b){if(!b)++failures;}
struct Driver {Cell*active=nullptr;int frees=0;void buffer_free(ID){check(active->state==State::ENQUEUED);++frees;}void uniform_set_free(ID){check(active->state==State::ENQUEUED);++frees;}};
struct Frame {Queue<Buffer> buffers_to_dispose_of;Queue<UniformSet> uniform_sets_to_dispose_of;};
struct Device {Frame frames[1];Driver*driver;int buffer_memory=128;uint64_t get_instance_id()const{return 9;}void _qualification_freed(int){}
'''+returned+'''
void buffers(int p_frame){
'''+loop('// Buffers.')+'''}
void sets(int p_frame){
'''+loop('// Uniform sets.')+'''} };
void initialize(Cell &c,uint64_t rid,uint64_t serial,uint64_t backing){c.device=9;c.rid=rid;c.serial=serial;c.backing=backing;check(c.enqueue(rid,serial,backing));}
int main(){
Driver d;Device rd;rd.driver=&d;
Buffer b;auto bw=b.native_disposal;initialize(*bw.cell.p,7,11,19);rd.frames[0].buffers_to_dispose_of.q.push_back({b});d.active=bw.cell.p.get();rd.buffers(0);check(bw.cell->state==State::RETURNED);check(d.frees==1);
UniformSet s;auto sw=s.native_disposal;initialize(*sw.cell.p,8,12,23);rd.frames[0].uniform_sets_to_dispose_of.q.push_back({s});d.active=sw.cell.p.get();rd.sets(0);check(sw.cell->state==State::RETURNED);check(d.frees==2);
// No observer or original resource is required to keep the queued cell alive.
std::weak_ptr<Cell> lifetime;
{Buffer temporary;initialize(*temporary.native_disposal.cell.p,7,11,19);lifetime=temporary.native_disposal.cell.p;rd.frames[0].buffers_to_dispose_of.q.push_back({temporary});}
check(!lifetime.expired());d.active=lifetime.lock().get();rd.buffers(0);check(lifetime.expired());
// Exact identity and a real enqueue are necessary. Unknown is never repaired.
for(int field=0;field<3;++field){Cell bad;initialize(bad,7,11,19);bad.returned(7+(field==0),11+(field==1),19+(field==2));check(bad.state==State::UNKNOWN);bad.returned(7,11,19);check(bad.state==State::UNKNOWN);}
Cell live;live.rid=7;live.serial=11;live.backing=19;live.returned(7,11,19);check(live.state==State::UNKNOWN);
Cell duplicate;initialize(duplicate,7,11,19);check(!duplicate.enqueue(7,11,19));check(duplicate.state==State::UNKNOWN);
printf("return_site_and_cell_failures=%d\\n",failures);return failures?1:0;}
'''
# No qualification dependency exists in these resource fixtures.
text=text.replace('for(auto *cell:uniform_set->qualification_dependencies) { CRASH_COND(!cell->dependent_children.load());cell->dependent_children.fetch_sub(1); }','')
if mode=='premature':
 for obj,access in [('buffer','.'),('uniform_set','->')]:
  call='driver->'+('buffer_free(buffer.driver_id);' if obj=='buffer' else 'uniform_set_free(uniform_set->driver_id);')
  start=text.index(call);nstart=text.index('_native_disposal_returned(',start);end=text.index(';',nstart)+1
  notification=text[nstart:end];text=text[:start]+notification+'\n'+call+text[end:]
out=Path(__file__).parent
cpp=out/(mode+'_return_sites.cpp');exe=out/'return_sites';cpp.write_text('\n'.join(line.rstrip() for line in text.splitlines())+'\n')
subprocess.run(['clang++','-std=c++17',str(cpp),'-o',str(exe)],check=True)
sys.exit(subprocess.run([str(exe)]).returncode)
