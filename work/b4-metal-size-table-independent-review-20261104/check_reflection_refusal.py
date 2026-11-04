#!/usr/bin/env python3
"""Actual SPIR-V reflection + exact extracted production refusal expression.
This is not execution of the full Metal container or native backend.
"""
from pathlib import Path
import tempfile,subprocess,json,hashlib
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[1]
FALLBACK=Path('/Users/macbookair/Documents/Codex/2026-10-20/godot-ange/worktrees/vulkan-qualification-profile-20261104')
source=(ROOT/'drivers/metal/rendering_shader_container_metal.cpp').read_text()
prefix='ERR_FAIL_COND_V_MSG((is_storage_buffer || type == RDC::UNIFORM_TYPE_UNIFORM_BUFFER'
start=source.index(prefix)+len('ERR_FAIL_COND_V_MSG(')
expression=source[start:source.index(',\n',start)]
assert 'binding.array.dims_count != 0' in expression
program=r'''
#include "thirdparty/spirv-reflect/spirv_reflect.h"
#include <vector>
#include <fstream>
#include <iterator>
#include <cstdio>
#include <cstdlib>
namespace RDC {enum UniformType{OTHER,UNIFORM_TYPE_UNIFORM_BUFFER,UNIFORM_TYPE_UNIFORM_BUFFER_DYNAMIC,UNIFORM_TYPE_STORAGE_BUFFER,UNIFORM_TYPE_STORAGE_BUFFER_DYNAMIC};}
int main(int argc,char**argv){
 if(argc!=3)return 4;
 std::ifstream f(argv[1],std::ios::binary);std::vector<char> bytes((std::istreambuf_iterator<char>(f)),{});
 SpvReflectShaderModule module{};if(spvReflectCreateShaderModule(bytes.size(),bytes.data(),&module)!=SPV_REFLECT_RESULT_SUCCESS)return 5;
 uint32_t count=0;spvReflectEnumerateDescriptorBindings(&module,&count,nullptr);std::vector<SpvReflectDescriptorBinding*> bindings(count);spvReflectEnumerateDescriptorBindings(&module,&count,bindings.data());
 bool reject=false;
 for(auto *pointer:bindings){const auto &binding=*pointer;RDC::UniformType type=RDC::OTHER;
 if(binding.descriptor_type==SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER)type=RDC::UNIFORM_TYPE_STORAGE_BUFFER;
 if(binding.descriptor_type==SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER)type=RDC::UNIFORM_TYPE_UNIFORM_BUFFER;
 const bool is_storage_buffer=type==RDC::UNIFORM_TYPE_STORAGE_BUFFER||type==RDC::UNIFORM_TYPE_STORAGE_BUFFER_DYNAMIC;
 const bool current=PRODUCTION_EXPRESSION;
 std::printf("binding=%u descriptor_array_dims=%u rejected=%d\n",binding.binding,binding.array.dims_count,int(current));reject|=current;
 }
 spvReflectDestroyShaderModule(&module);
 if(reject!=(std::atoi(argv[2])!=0)){std::fprintf(stderr,"TARGET_REFUSAL_ORACLE_MISMATCH\n");return 1;}return 0;
}
'''
fixtures=[
 ('runtime_member_not_descriptor_array','layout(set=0,binding=0,std430) buffer T{uint values[];}a;','a.values[0]',False),
 ('storage_descriptor_array_one','layout(set=0,binding=0,std430) buffer T{uint values[];}a[1];','a[0].values[0]',True),
 ('storage_descriptor_array_two','layout(set=0,binding=0,std430) buffer T{uint values[];}a[2];','a[1].values[0]',True),
 ('uniform_descriptor_array','layout(set=0,binding=0,std140) uniform T{uvec4 values;}a[2];','a[1].values.x',True)]
rows=[]
with tempfile.TemporaryDirectory(prefix='b4-reflection-control-') as d:
 d=Path(d);obj=d/'reflect.o'
 cmd=['clang','-std=c11','-I'+str(FALLBACK),'-c',str(FALLBACK/'thirdparty/spirv-reflect/spirv_reflect.c'),'-o',str(obj)]
 subprocess.run(cmd,check=True,capture_output=True,text=True)
 for mode in ['production','mutation']:
  expr=expression if mode=='production' else expression.replace('binding.array.dims_count != 0','false')
  cpp=d/(mode+'.cpp');cpp.write_text(program.replace('PRODUCTION_EXPRESSION',expr));binary=d/mode
  subprocess.run(['clang++','-std=c++17','-I'+str(FALLBACK),str(cpp),str(obj),'-o',str(binary)],check=True,capture_output=True,text=True)
  for name,decl,value,rejected in fixtures:
   if mode=='mutation' and not rejected:continue
   comp=d/(name+'.comp');comp.write_text('#version 450\nlayout(local_size_x=1)in;\n'+decl+'\nlayout(set=0,binding=7,std430)buffer O{uint words[];}o;void main(){o.words[0]='+value+';}\n')
   spv=comp.with_suffix('.spv');subprocess.run(['glslangValidator','-V',str(comp),'-o',str(spv)],check=True,capture_output=True,text=True);subprocess.run(['spirv-val',str(spv)],check=True)
   run=subprocess.run([str(binary),str(spv),str(int(rejected))],capture_output=True,text=True)
   passed=run.returncode==0 if mode=='production' else run.returncode==1 and 'TARGET_REFUSAL_ORACLE_MISMATCH' in run.stderr
   rows.append({'mode':mode,'case':name,'expected_refusal':rejected,'rc':run.returncode,'oracle_pass':passed,'stdout':run.stdout,'stderr':run.stderr,'shader':comp.read_text(),'spirv_sha256':hashlib.sha256(spv.read_bytes()).hexdigest()});assert passed,rows[-1]
report={'production_expression':expression,'production_expression_sha256':hashlib.sha256(expression.encode()).hexdigest(),'rows':rows,'full_container_executed':False,'native_metal_executed':False}
(HERE/'REFLECTION_REFUSAL_RESULTS.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
