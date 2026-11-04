#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <vector>
using uint = uint32_t;
using std::min; using std::max; using std::isnan; using std::isinf;
struct vec2 { float x,y; vec2(float v=0):x(v),y(v){} vec2(float a,float b):x(a),y(b){} };
struct vec3 { float x,y,z; vec3(float v=0):x(v),y(v),z(v){} vec3(float a,float b,float c):x(a),y(b),z(c){} };
struct vec4 { float x,y,z,w; vec4(float v=0):x(v),y(v),z(v),w(v){} vec4(float a,float b,float c,float d):x(a),y(b),z(c),w(d){} vec3 xyz()const{return vec3(x,y,z);} };
struct uvec2 { uint x,y; uvec2(uint v=0):x(v),y(v){} uvec2(uint a,uint b):x(a),y(b){} };
struct uvec3 { uint x,y,z; uvec3(uint v=0):x(v),y(v),z(v){} uvec3(uint a,uint b,uint c):x(a),y(b),z(c){} };
struct uvec4 { uint x,y,z,w; uvec4(uint v=0):x(v),y(v),z(v),w(v){} uvec4(uint a,uint b,uint c,uint d):x(a),y(b),z(c),w(d){} uvec3 yzw()const{return uvec3(y,z,w);} uvec2 zw()const{return uvec2(z,w);} };
uint floatBitsToUint(float v){uint r; std::memcpy(&r,&v,4);return r;}
float uintBitsToFloat(uint v){float r;std::memcpy(&r,&v,4);return r;}
vec4 uintBitsToFloat(uvec4 v){return vec4(uintBitsToFloat(v.x),uintBitsToFloat(v.y),uintBitsToFloat(v.z),uintBitsToFloat(v.w));}
struct Bool2 {bool x,y;};struct Bool3 {bool x,y,z;};
bool any(Bool2 v){return v.x||v.y;} bool any(Bool3 v){return v.x||v.y||v.z;} bool all(Bool3 v){return v.x&&v.y&&v.z;}
Bool2 isnan(vec2 v){return {std::isnan(v.x),std::isnan(v.y)};} Bool2 isinf(vec2 v){return {std::isinf(v.x),std::isinf(v.y)};}
Bool3 lessThan(vec3 a,vec3 b){return {a.x<b.x,a.y<b.y,a.z<b.z};}
Bool3 lessThanEqual(vec3 a,vec3 b){return {a.x<=b.x,a.y<=b.y,a.z<=b.z};}
Bool2 notEqual(uvec2 a,uvec2 b){return {a.x!=b.x,a.y!=b.y};} Bool3 notEqual(uvec3 a,uvec3 b){return {a.x!=b.x,a.y!=b.y,a.z!=b.z};}
void atomicOr(uint &target,uint value){target|=value;} void atomicMin(uint &target,uint value){target=min(target,value);}
template<class T>struct Array:std::vector<T>{using std::vector<T>::operator=;int length()const{return int(this->size());}};
struct Buffer{Array<uint>words;}current_records,previous_records,output_meta,source,scratch,status,commands;
// PRODUCTION_SURFACE
struct Proof {
 vec4 current_decl_min,current_decl_max,previous_decl_min,previous_decl_max,cull_min,cull_max;
 vec4 current_host_inverse_rows[3],previous_host_inverse_rows[3];
 uvec4 source_accessor,options;Array<SurfaceProof>surfaces;
}proof;
struct Params{uint capacity,sequence_low,sequence_high,surface_count,record_base,record_count,reserved0,reserved1;}params;
struct {uint x;}gl_GlobalInvocationID;
// PRODUCTION_FUNCTIONS
void healthy(uint actual=1,uint capacity=2){
 params={capacity,55,0,1,0,capacity,0,0};
 current_records.words.assign(capacity*48,0);previous_records.words.assign(capacity*48,0);
 for(uint i=0;i<capacity;i++){
  auto &w=current_records.words;uint b=i*48;
  for(uint o:{0u,5u,10u,12u,17u,22u,24u,29u,34u})w[b+o]=floatBitsToUint(1);
  w[b+44]=i;w[b+45]=23;w[b+46]=17;
 }
 previous_records.words=current_records.words;
 output_meta.words={actual,0,0,0,capacity,1,55,0};source.words={actual,1};
 scratch.words.assign(16,0xdeadbeef);status.words.assign(8,0xdeadbeef);commands.words.assign(5,0xdeadbeef);
 proof={};proof.current_decl_min=proof.previous_decl_min=proof.cull_min=vec4(-1,-1,-1,0);
 proof.current_decl_max=proof.previous_decl_max=proof.cull_max=vec4(1,1,1,0);
 for(uint i=0;i<3;i++){proof.current_host_inverse_rows[i]=vec4(i==0,i==1,i==2,0);proof.previous_host_inverse_rows[i]=proof.current_host_inverse_rows[i];}
 proof.source_accessor=uvec4(0,1,1,1);proof.options=uvec4(1,0,0,0);
 SurfaceProof s{};s.current_mesh_min=s.previous_mesh_min=vec4(-.25,-.25,0,0);
 s.current_mesh_max=s.previous_mesh_max=vec4(.25,.25,0,0);s.command=uvec4(1,6,0,0);proof.surfaces={s};
}
void execute(bool capture=true){
 gl_GlobalInvocationID.x=0;if(capture)shader_INIT();
 for(uint i=0;i<params.record_count;i++){gl_GlobalInvocationID.x=i;shader_VALIDATE();}
 gl_GlobalInvocationID.x=0;shader_FINALIZE();
}
void dispatch_chunk(uint base,uint count,bool reverse=false){
 params.record_base=base;params.record_count=count;
 // Exercise rounded-up workgroups, including every out-of-chunk lane.
 uint padded=(count+63u)/64u*64u;
 for(uint lane=0;lane<padded;lane++){
  gl_GlobalInvocationID.x=reverse?padded-1u-lane:lane;shader_VALIDATE();
 }
}
void execute_chunks(bool reverse=false){
 gl_GlobalInvocationID.x=0;shader_INIT();
 uint capacity=params.capacity;
 uint chunks=(capacity+63u)/64u;
 for(uint i=0;i<chunks;i++){
  uint chunk=reverse?chunks-1u-i:i;
  uint base=chunk*64u;dispatch_chunk(base,min(64u,capacity-base),reverse);
 }
 gl_GlobalInvocationID.x=0;shader_FINALIZE();
}
void add_nonindexed_surface(){
 SurfaceProof second=proof.surfaces[0];second.command=uvec4(0,4,0,0);
 proof.surfaces.push_back(second);params.surface_count=2;commands.words.assign(10,0xdeadbeef);
}
uint checks=0;
void expect(uint bits,uint count,const char *name){
 checks++;if(status.words[0]!=bits||status.words[1]!=count){std::cerr<<name<<" expected "<<bits<<","<<count<<" got "<<status.words[0]<<","<<status.words[1]<<"\n";std::abort();}
 if(bits||count==0)for(uint w:commands.words)assert(w==0);
 else{
  assert(commands.words.size()==params.surface_count*5u);
  for(uint surface=0;surface<params.surface_count;surface++){
   assert(commands.words[surface*5u]==proof.surfaces[surface].command.y);
   assert(commands.words[surface*5u+1u]==count);
   for(uint i=2;i<5;i++)assert(commands.words[surface*5u+i]==0);
  }
 }
}
int main(){
 healthy();execute();expect(0,1,"healthy");
 healthy(0);execute();expect(0,0,"source zero");
 healthy(0,0);execute();expect(0,0,"capacity zero component guard");
 healthy();output_meta.words[0]=3;execute();expect(2,0,"count exceeds envelope and source");
 healthy();source.words[0]=2;execute();expect(2,0,"source count mismatch");
 healthy();source.words[1]=0;execute();expect(1,0,"invalid source");
 healthy();output_meta.words[3]=1;execute();expect(1,0,"producer fault");
 healthy();output_meta.words[5]=2;execute();expect(4,0,"metadata version");
 healthy();output_meta.words[7]=1;execute();expect(4,0,"metadata sequence");
 healthy();current_records.words[0]=0x7f800000;execute();expect(16,0,"infinite transform");
 healthy();current_records.words[40]=1;execute();expect(16,0,"subnormal custom");
 healthy();current_records.words[46]|=32;execute();expect(16,0,"reserved flags");
 healthy();current_records.words[47]=1;execute();expect(16,0,"reserved word");
 healthy();current_records.words[3]=floatBitsToUint(2);execute();expect(64,0,"current bounds escape");
 healthy();current_records.words[7]=floatBitsToUint(2);execute();expect(64,0,"y bounds escape");
 healthy();current_records.words[46]|=2;previous_records.words[3]=floatBitsToUint(2);execute();expect(64,0,"valid previous escape");assert(scratch.words[9]==8);
 healthy();previous_records.words.clear();execute();expect(0,1,"reset never reads poisoned previous");
 healthy();current_records.words[46]|=2;previous_records.words[45]++;execute();expect(32,0,"previous identity mismatch");
 healthy();current_records.words[46]|=2;proof.options.x=0;execute();expect(32,0,"history not permitted");
 healthy();gl_GlobalInvocationID.x=0;shader_INIT();output_meta.words[0]=999;execute(false);expect(0,1,"metadata captured immutable");
 healthy();proof.source_accessor.w=2; // Local shear C=Host, matching inverse.
 current_records.words[1]=floatBitsToUint(1);proof.current_host_inverse_rows[0]=vec4(1,-1,0,0);
 execute();expect(0,1,"local shear per corner inverse");
 healthy();proof.source_accessor.w=2;current_records.words[0]=floatBitsToUint(-2);proof.current_host_inverse_rows[0]=vec4(-.5,0,0,0);
 current_records.words[46]|=4;execute();expect(0,1,"local mirror");
 healthy();params.record_base=2;params.record_count=1;execute();expect(2,0,"chunk overflow");
 healthy(2);current_records.words[0]=0x7fc00000;current_records.words[48+3]=floatBitsToUint(2);execute();expect(80,0,"all lanes accumulate faults");assert(scratch.words[9]==0);
 // Distinct pinned indexed/nonindexed native constants share padded slots.
 healthy();add_nonindexed_surface();execute();expect(0,1,"indexed and nonindexed surfaces");
 assert(commands.words[0]==6&&commands.words[5]==4);assert(proof.surfaces[0].command.x==1&&proof.surfaces[1].command.x==0);
 healthy(0);add_nonindexed_surface();execute();expect(0,0,"multiple surfaces source zero");
 healthy();add_nonindexed_surface();proof.surfaces[1].current_displacement_min.x=2;proof.surfaces[1].current_displacement_max.x=2;
 execute();expect(64,0,"second surface fault zeroes every command");assert(scratch.words[9]==1);
 healthy(64,64);execute_chunks();expect(0,64,"exact one-workgroup capacity");
 healthy(65,65);execute_chunks();expect(0,65,"last partial chunk capacity");
 healthy(129,129);execute_chunks();expect(0,129,"three chunks include last record");
 healthy(65,129);for(uint i=65;i<129;i++)current_records.words[i*48]=0x7fc00000;
 execute_chunks();expect(0,65,"chunked validation never fetches inactive tails");
 healthy(129,129);current_records.words[64*48]=0x7fc00000;current_records.words[128*48+3]=floatBitsToUint(2);
 execute_chunks();expect(80,0,"cross-chunk faults forward order");uint forward_first=scratch.words[9];assert(forward_first==64*16);
 healthy(129,129);current_records.words[64*48]=0x7fc00000;current_records.words[128*48+3]=floatBitsToUint(2);
 execute_chunks(true);expect(80,0,"cross-chunk faults reverse lanes and chunks");assert(scratch.words[9]==forward_first);
 healthy(2);current_records.words[0]=0x7fc00000;current_records.words[48+3]=floatBitsToUint(2);
 gl_GlobalInvocationID.x=0;shader_INIT();dispatch_chunk(0,2,true);gl_GlobalInvocationID.x=0;shader_FINALIZE();
 expect(80,0,"same chunk reverse lanes preserve OR and lowest failure");assert(scratch.words[9]==0);
 healthy();proof.source_accessor.z=2;source.words[1]=0;execute();expect(0,1,"rule2 zero source fault");
 healthy();proof.source_accessor.z=2;source.words[1]=1;execute();expect(1,0,"rule2 nonzero source fault");
 healthy();proof.source_accessor.z=2;source.words[1]=0xffffffff;execute();expect(1,0,"rule2 all-bits source fault");
 healthy();source.words[1]=42;execute();expect(0,1,"rule1 noncanonical positive initialized word");
 // The exact production interval routines must include exact-real products.
 bool valid=true;vec2 p=multiply_interval(vec2(.25),vec2(2),valid);assert(valid&&p.x<=.5&&p.y>=.5);checks++;
 valid=true;multiply_interval(vec2(std::numeric_limits<float>::max()),vec2(2),valid);assert(!valid);checks++;
 std::cout<<checks<<" CPU production-control/interval cases PASS (sequential, no GPU claim)\n";
}
