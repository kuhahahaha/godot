#include "drivers/metal/metal_buffer_size_table.h"
#include "thirdparty/spirv-cross/spirv_msl.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <vector>
#include <algorithm>
#include <cassert>
using namespace spirv_cross;
#include "source_slices.gen.h"
static std::vector<uint32_t> load(const char *path) {std::ifstream f(path,std::ios::binary);std::vector<char>b((std::istreambuf_iterator<char>(f)),{});assert(!b.empty()&&b.size()%4==0);std::vector<uint32_t>w(b.size()/4);std::memcpy(w.data(),b.data(),b.size());return w;}
struct Reflected {MSLResourceBinding binding;bool storage=false;};
int main(int argc,char **argv) {
 assert(argc==4||argc==5);
 assert(source_format_version==4 && source_format_version!=3);
 for(auto type:{RDC::UNIFORM_TYPE_STORAGE_BUFFER,RDC::UNIFORM_TYPE_STORAGE_BUFFER_DYNAMIC,RDC::UNIFORM_TYPE_UNIFORM_BUFFER,RDC::UNIFORM_TYPE_UNIFORM_BUFFER_DYNAMIC}){assert(source_array(type,0));assert(!source_array(type,1));assert(!source_array(type,2));}
 assert(source_array(RDC::UNIFORM_TYPE_TEXTURE,1));
 CompilerMSL compiler(load(argv[1]));auto stage=compiler.get_execution_model();
 CompilerMSL::Options opt;opt.set_msl_version(4,0);opt.argument_buffers=std::string(argv[2])!="direct";opt.argument_buffers_tier=CompilerMSL::Options::ArgumentBuffersTier::Tier2;opt.force_active_argument_buffer_resources=true;opt.pad_argument_buffer_resources=true;compiler.set_msl_options(opt);
 std::map<uint32_t,std::map<uint32_t,Reflected>> sets;
 auto reflect=[&](CompilerMSL &c) {
  auto res=c.get_shader_resources();
  auto add=[&](const auto &list,SPIRType::BaseType base,bool storage) {
   for(auto r:list) {
    auto type=c.get_type(r.type_id);uint32_t count=1;
    if(!source_array(storage?RDC::UNIFORM_TYPE_STORAGE_BUFFER:(base==SPIRType::Void?RDC::UNIFORM_TYPE_UNIFORM_BUFFER:RDC::UNIFORM_TYPE_TEXTURE),uint32_t(type.array.size()))){std::cerr<<"TARGET_REFUSAL buffer descriptor arrays unsupported by RD single-ID ABI\n";std::exit(3);}
    for(auto n:type.array)count*=n;
    Reflected entry;auto &b=entry.binding;b.desc_set=c.get_decoration(r.id,spv::DecorationDescriptorSet);b.binding=c.get_decoration(r.id,spv::DecorationBinding);b.basetype=base;b.count=count;entry.storage=storage;
    auto existing=sets[b.desc_set].find(b.binding);if(existing!=sets[b.desc_set].end())assert(existing->second.binding.basetype==b.basetype&&existing->second.binding.count==b.count);
    sets[b.desc_set][b.binding]=entry;
   }
  };
  add(res.storage_buffers,SPIRType::Void,true);add(res.uniform_buffers,SPIRType::Void,false);add(res.sampled_images,SPIRType::SampledImage,false);add(res.separate_images,SPIRType::Image,false);add(res.separate_samplers,SPIRType::Sampler,false);
 };
 reflect(compiler);if(argc==5){CompilerMSL other(load(argv[4]));reflect(other);}
 std::map<uint32_t,uint32_t> counts;
 for(auto &entry:sets) {
  uint32_t index=0;bool storage=false;
  for(auto &kv:entry.second) {
   auto b=kv.second.binding;b.stage=stage;storage|=kv.second.storage;
   if(b.basetype==SPIRType::SampledImage){b.msl_texture=index;index+=b.count;b.msl_sampler=index;index+=b.count;}
   else if(b.basetype==SPIRType::Image){b.msl_texture=index;index+=b.count;}
   else if(b.basetype==SPIRType::Sampler){b.msl_sampler=index;index+=b.count;}
   else {b.msl_buffer=index;index+=b.count;}
   compiler.add_msl_resource_binding(b);
  }
  counts[entry.first]=index;
  if(std::string(argv[2])=="fixed" && storage) {
   std::vector<MSLResourceBinding> auxiliary;assert(source_aux(opt,storage,index,entry.first,auxiliary));for(auto b:auxiliary){b.stage=stage;compiler.add_msl_resource_binding(b);}
  }
 }
 MSLResourceBinding pc;pc.stage=stage;pc.desc_set=ResourceBindingPushConstantDescriptorSet;pc.basetype=SPIRType::Void;pc.msl_buffer=sets.rbegin()->first+1;compiler.add_msl_resource_binding(pc);
 auto source=compiler.compile();std::ofstream(argv[3])<<source;
 if(!source_direct(opt,compiler)){std::cerr<<"TARGET_REFUSAL direct OpArrayLength requires unsupported auxiliary binding\n";return 4;}
 bool collision=source.find("Overlapping binding:")!=std::string::npos;
 if(collision){std::cerr<<"TARGET_RED auxiliary argument collides with a real resource\n";return 2;}
 if(compiler.needs_buffer_size_buffer())for(auto entry:counts){auto expected="spvBufferSizeConstants [[id("+std::to_string(entry.second)+")]]";assert(source.find(expected)!=std::string::npos);}
 std::cout<<"PASS repository SPIRV-Cross stage="<<int(stage)<<" sets="<<sets.size()<<" array_length="<<compiler.needs_buffer_size_buffer()<<"\n";
}
