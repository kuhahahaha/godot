#!/usr/bin/env python3
"""Run snapshotted production plan body, compare to reflected production bindings.
CPU shims only cover RID/vector/uniform transport. This is not native RD execution.
A detected known counterexample exits 0; it is not a production PASS.
"""
from pathlib import Path
import re, subprocess, tempfile, json, hashlib
HERE=Path(__file__).resolve().parent
header=(HERE/'particles_external_guard.h').read_text()
source=(HERE/'particles_external_guard.cpp').read_text()
shader=(HERE/'particles_external_guard.glsl').read_text()
public=header.split('public:',1)[1].split('\nprivate:',1)[0]
method=source[source.index('Error ParticlesExternalGuard::plan_layout'):source.rindex('\n} // namespace')]
prefix='''#include <cstdint>
#include <algorithm>
#include <vector>
#include <iostream>
using Error=int; constexpr int OK=0, ERR_INVALID_PARAMETER=1;
struct RID { uint64_t id=0; bool is_valid()const{return id!=0;} bool is_null()const{return id==0;} bool operator==(RID b)const{return id==b.id;} };
template<class T> using Vector=std::vector<T>;
#define MIN(a,b) std::min(a,b)
struct RD { enum {UNIFORM_TYPE_STORAGE_BUFFER}; struct Uniform {uint32_t binding; RID rid; Uniform(int,uint32_t b,RID r):binding(b),rid(r){} }; };
class ParticlesExternalGuard { public:
'''
main='''
int main(){
 using G=ParticlesExternalGuard; G::PushConstant req;req.surface_count=1;
 G::Bindings bindings; uint64_t bytes[8]={0,0,32,16,64,32,20,368};
 for(unsigned i=0;i<8;++i){bindings.buffers[i].byte_length=bytes[i];if(bytes[i])bindings.buffers[i].rid.id=100+i;}
 G::GeometryProofHeader proof;proof.source_accessor[0]=1;proof.source_accessor[1]=3;proof.source_accessor[2]=1;proof.source_accessor[3]=1;
 G::LayoutPlan plan;int rc=G::plan_layout(req,bindings,proof,1,plan);
 std::cout<<"{\\"planner_rc\\":"<<rc<<",\\"provided_bindings\\":[";
 for(unsigned i=0;i<plan.uniforms.size();i++){if(i)std::cout<<",";std::cout<<plan.uniforms[i].binding;}
 std::cout<<"]}\\n";return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='b4-guard-review-') as d:
 d=Path(d); cpp=d/'plan.cpp'; binary=d/'plan'
 cpp.write_text(prefix+public+'\nstatic Error plan_layout(const PushConstant&,const Bindings&,const GeometryProofHeader&,uint32_t,LayoutPlan&);\n};\n'+method+main)
 subprocess.run(['clang++','-std=c++17',str(cpp),'-o',str(binary)],check=True,capture_output=True,text=True)
 result=subprocess.run([str(binary)],check=True,capture_output=True,text=True)
 observed=json.loads(result.stdout); assert observed['planner_rc']==0
 report={'execution':'CPU production method body with transport shims; glslang SPIR-V reflection','snapshot_sha256':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in HERE.glob('particles_external_guard.*')}, **observed,'variants':{}}
 for mode in ['INIT','VALIDATE','FINALIZE']:
  comp=d/(mode+'.comp');spv=d/(mode+'.spv')
  comp.write_text(shader.replace('#[compute]\n','').replace('#VERSION_DEFINES','#define MODE_'+mode))
  subprocess.run(['glslangValidator','-V',str(comp),'-o',str(spv)],check=True,capture_output=True,text=True)
  subprocess.run(['spirv-val',str(spv)],check=True,capture_output=True,text=True)
  assembly=subprocess.run(['spirv-dis',str(spv)],check=True,capture_output=True,text=True).stdout
  required=sorted({int(x) for x in re.findall(r'OpDecorate %\w+ Binding (\d+)',assembly)})
  missing=sorted(set(required)-set(observed['provided_bindings']))
  report['variants'][mode]={'reflected_required_bindings':required,'missing_bindings':missing}
  assert missing==[0,1], report
 report['known_counterexample_detected']=True;report['native_uniform_set_execution']=False
 print(json.dumps(report,indent=2))
