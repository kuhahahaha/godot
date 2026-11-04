#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>
#include <cassert>
#include <iostream>
#define ERR_FAIL_COND_V(c,v) do { if(c)return v; }while(false)
constexpr uint32_t CONTAINER_MAGIC_NUMBER=0x43535247, CONTAINER_VERSION=3;
uint32_t _format(){return 0x42424242;}
uint32_t _format_version(){return 4;}
	struct ContainerHeader {
		uint32_t magic_number = 0;
		uint32_t version = 0;
		uint32_t format = 0;
		uint32_t format_version = 0;
		uint32_t shader_count = 0;
		uint32_t requested_compile_flags = 0;
	};
bool header_gate(ContainerHeader header){
ERR_FAIL_COND_V(header.magic_number != CONTAINER_MAGIC_NUMBER || header.version != CONTAINER_VERSION, false);
ERR_FAIL_COND_V(header.format != _format() || header.format_version != _format_version(), false);
return true;
}
int main(int argc,char**argv){assert(argc==2);std::ifstream f(argv[1],std::ios::binary);std::vector<char>b((std::istreambuf_iterator<char>(f)),{});assert(b.size()>=sizeof(ContainerHeader));ContainerHeader header;std::memcpy(&header,b.data(),sizeof(header));assert(header.format_version==3);assert(!header_gate(header));header.format_version=4;assert(header_gate(header));header.format_version=5;assert(!header_gate(header));std::cout<<"PASS real format3 container rejected by exact production header gate; synthetic format4 header accepted only at gate, not full deserialization\n";}
