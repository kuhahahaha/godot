#include "drivers/metal/metal_buffer_size_table.h"
#include <cassert>
#include <cstdio>
#include <vector>
#include <algorithm>

static uint32_t u32(const std::vector<uint8_t> &bytes, uint32_t offset) { uint32_t v; std::memcpy(&v, bytes.data()+offset, 4); return v; }
static uint64_t u64(const std::vector<uint8_t> &bytes, uint32_t offset) { uint64_t v; std::memcpy(&v, bytes.data()+offset, 8); return v; }
int main() {
 MetalBufferSizeTable invalid;
 assert(!invalid.initialize(0)); assert(!invalid.initialize(UINT32_MAX));
 assert(!invalid.initialize(UINT32_MAX/8)); assert(invalid.resource_count==0);
 // Last representable allocation under INT32_MAX, and the first overrun.
 uint32_t largest=(INT32_MAX-8)/12;
 while (((uint64_t(largest)*12+8+7)&~uint64_t(7))>INT32_MAX) --largest;
 MetalBufferSizeTable boundary;
 assert(boundary.initialize(largest)); assert(!boundary.initialize(largest+1)); assert(boundary.resource_count==largest);
 // Binding gaps and mixed texture/sampler entries have already been mapped
 // to dense argument IDs; the table indexes these IDs, not binding numbers.
 MetalBufferSizeTable layout; assert(layout.initialize(8));
 assert(layout.resource_count==8 && layout.sizes_offset==72 && layout.allocation_size==104);
 std::vector<uint8_t> data(layout.allocation_size,0);
 assert(layout.write_length(data.data(),0,20)); assert(layout.write_length(data.data(),3,40));
 assert(layout.write_length(data.data(),7,UINT32_MAX));
 assert(u32(data,72)==20 && u32(data,84)==40 && u32(data,100)==UINT32_MAX);
 auto before=data; assert(!layout.write_length(data.data(),8,4)); assert(!layout.write_length(data.data(),2,uint64_t(UINT32_MAX)+1)); assert(data==before);
 assert(layout.write_address(data.data(),0x1000)); assert(u64(data,64)==0x1048);
 // Same layout across shaders independent of length usage and active stage.
 MetalBufferSizeTable no_length_creator, length_consumer;
 assert(no_length_creator.initialize(8)); assert(length_consumer.initialize(8));
 assert(no_length_creator.sizes_offset==length_consumer.sizes_offset);
 assert(u32(data,length_consumer.sizes_offset)==5*4);
 // Dynamic: original logical 20 bytes, rounded stride32, 3 frames. Length
 // remains5, while the native data pointer advances32 and the table self
 // pointer follows each argument allocation's base+offset, not old frames.
 std::vector<uint8_t> copy=data;
 assert(layout.write_address(copy.data(),0x2000,256)); assert(u64(copy,64)==0x2148);
 assert(u32(copy,72)==20 && 20/4==5 && 32/4!=5 && 96/4!=5);
 assert(layout.write_address(copy.data(),0x4000,512)); assert(u64(copy,64)==0x4248);
 before=copy; assert(!layout.write_address(copy.data(),UINT64_MAX-7));
 assert(!layout.write_address(copy.data(),8,UINT64_MAX-7));
 assert(!layout.write_address(copy.data(),0x1001)); assert(!layout.write_address(copy.data(),0x1000,1)); assert(copy==before);
 // An independent set must have its own pointer/table extent.
 MetalBufferSizeTable other; assert(other.initialize(2));
 std::vector<uint8_t> second(other.allocation_size,0);
 assert(other.write_length(second.data(),1,48)); assert(other.write_address(second.data(),0x8000));
 assert(u64(second,16)==0x8018 && u32(second,28)==48);
 std::puts("PASS checked layout boundaries, exact lengths, same-layout reuse, independent sets, dynamic self-pointer and overflow");
}
