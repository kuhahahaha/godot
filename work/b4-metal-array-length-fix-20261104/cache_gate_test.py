from pathlib import Path
import re,struct,json,hashlib,subprocess
root=Path.cwd();here=root/'work/b4-metal-array-length-fix-20261104'
h=(root/'servers/rendering/rendering_shader_container.h').read_text();cpp=(root/'servers/rendering/rendering_shader_container.cpp').read_text();mh=(root/'drivers/metal/rendering_shader_container_metal.h').read_text()
struct_body=re.search(r'\tstruct ContainerHeader \{.*?\n\t\};',h,re.S)[0]
gates=[re.search(r'ERR_FAIL_COND_V\(header.magic_number.*?;',cpp)[0],re.search(r'ERR_FAIL_COND_V\(header.format !=.*?;',cpp)[0]]
version=int(re.search(r'FORMAT_VERSION = (\d+)',mh)[1])
source='''#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>
#include <cassert>
#include <iostream>
#define ERR_FAIL_COND_V(c,v) do { if(c)return v; }while(false)
constexpr uint32_t CONTAINER_MAGIC_NUMBER=0x43535247, CONTAINER_VERSION=3;
uint32_t _format(){return 0x42424242;}
'''+f'uint32_t _format_version(){{return {version};}}\n'+struct_body+'''\nbool header_gate(ContainerHeader header){
'''+ '\n'.join(gates)+'''
return true;
}
int main(int argc,char**argv){assert(argc==2);std::ifstream f(argv[1],std::ios::binary);std::vector<char>b((std::istreambuf_iterator<char>(f)),{});assert(b.size()>=sizeof(ContainerHeader));ContainerHeader header;std::memcpy(&header,b.data(),sizeof(header));assert(header.format_version==3);assert(!header_gate(header));header.format_version=4;assert(header_gate(header));header.format_version=5;assert(!header_gate(header));std::cout<<"PASS real format3 container rejected by exact production header gate; synthetic format4 header accepted only at gate, not full deserialization\\n";}
'''
file=here/'cache_gate_test.cpp';file.write_text(source);exe=here/'cache_gate_test'
commands=[['clang++','-std=c++17','-Wall','-Wextra','-Werror',str(file),'-o',str(exe)],[str(exe),str(root/'work/b4-guard-metal-diagnosis-20261104/INIT.metalbin')]]
rows=[]
for command in commands:
 p=subprocess.run(command,capture_output=True,text=True);rows.append({'command':command,'returncode':p.returncode,'stdout':p.stdout,'stderr':p.stderr});assert p.returncode==0,p.stderr
(here/'CACHE_GATE_RESULTS.json').write_text(json.dumps({'rows':rows,'scope':'verbatim production prefix gate, not complete native container deserialization','source_gates':gates,'new_format':version},indent=2)+'\n')
print(rows[-1]['stdout'])
