#include "drivers/metal/metal_buffer_size_table.h"
#include <cassert>
#include <cstdio>
#include <vector>

static uint64_t load64(const uint8_t *p) {uint64_t x;std::memcpy(&x,p,8);return x;}
static uint32_t load32(const uint8_t *p) {uint32_t x;std::memcpy(&x,p,4);return x;}
int main() {
 MetalBufferSizeTable table;
 assert(!table.initialize(0));assert(!table.initialize(UINT32_MAX));
 assert(table.resource_count==0 && table.sizes_offset==0 && table.allocation_size==0);
 assert(!table.write_address(nullptr,0));assert(!table.write_length(nullptr,0,0));
 assert(table.initialize(178956969));
 assert(table.sizes_offset==1431655760 && table.allocation_size==2147483640);
 assert(!table.initialize(178956970));
 assert(table.resource_count==178956969 && table.sizes_offset==1431655760 && table.allocation_size==2147483640);
 for(uint32_t n=1;n<=1024;n++) {
  MetalBufferSizeTable t;assert(t.initialize(n));
  assert(t.resource_count==n && t.sizes_offset==8*n+8);
  const uint32_t unpadded=8*n+8+4*n;
  assert(t.allocation_size>=unpadded && t.allocation_size-unpadded<8 && t.allocation_size%8==0);
  std::vector<uint8_t> guarded(t.allocation_size+32,0xa5);uint8_t *data=guarded.data()+16;
  auto before=guarded;
  for(uint32_t i=0;i<n;i++)assert(t.write_length(data,i,i==0?0:(i==n-1?UINT32_MAX:17*i)));
  for(uint32_t i=0;i<n;i++)assert(load32(data+t.sizes_offset+4*i)==(i==0?0:(i==n-1?UINT32_MAX:17*i)));
  for(uint32_t i=0;i<16+8*n+8;i++)assert(guarded[i]==before[i]);
  for(uint32_t i=16+unpadded;i<guarded.size();i++)assert(guarded[i]==before[i]);
  before=guarded;
  assert(!t.write_length(data,n,0));assert(!t.write_length(data,0,uint64_t(UINT32_MAX)+1));assert(guarded==before);
  assert(t.write_address(data,0x100000,256));
  assert(load64(data+8*n)==0x100000+256+8*n+8);
  for(uint32_t i=0;i<guarded.size();i++)if(i<16+8*n||i>=16+8*n+8)assert(guarded[i]==before[i]);
 }
 MetalBufferSizeTable t;assert(t.initialize(17));std::vector<uint8_t> data(t.allocation_size,0x3c);
 const uint64_t numbers[]={0,1,7,8,16,24,256,4096,UINT64_MAX,UINT64_MAX-7,UINT64_MAX-15,UINT64_MAX-255};
 for(uint64_t base:numbers)for(uint64_t offset:numbers){
  auto before=data;
  const unsigned __int128 sum=static_cast<unsigned __int128>(base)+offset+t.sizes_offset;
  const bool expected=(base%8==0)&&(offset%8==0)&&sum<=UINT64_MAX;
  const bool actual=t.write_address(data.data(),base,offset);assert(actual==expected);
  if(!actual)assert(data==before);else assert(load64(data.data()+8*17)==uint64_t(sum));
 }
 // Resource indices, not source binding numbers: non-storage dense entries
 // remain untouched and independently allocated tables never share pointers.
 MetalBufferSizeTable sparse;assert(sparse.initialize(8));
 std::vector<uint8_t> a(sparse.allocation_size,0),b(sparse.allocation_size,0);
 assert(sparse.write_length(a.data(),0,20));assert(sparse.write_length(a.data(),6,40));
 assert(sparse.write_address(a.data(),0x2000,512));assert(sparse.write_address(b.data(),0x8000,1024));
 assert(load64(a.data()+64)==0x2248 && load64(b.data()+64)==0x8448);
 assert(load32(a.data()+72)==20 && load32(a.data()+96)==40);
 assert(load32(b.data()+72)==0);
 std::puts("PASS independent fixed largest boundary, 1024 canary layouts, 144 u128 address oracles, no partial rejected write, independent allocation pointers");
}
