from pathlib import Path
import re,hashlib,json
root=Path.cwd();here=root/'work/b4-metal-array-length-fix-20261104';p=root/'drivers/metal/rendering_shader_container_metal.cpp';s=p.read_text()
start=s.index('if (msl_options.argument_buffers && has_storage_buffer)');end=s.index('\n\t\t\tidx_dset++;',start);aux=s[start:end]
array=re.search(r'ERR_FAIL_COND_V_MSG\(\(is_storage_buffer.*?\);',s,re.S)[0]
direct=re.search(r'ERR_FAIL_COND_V_MSG\(!msl_options.argument_buffers && compiler.needs_buffer_size_buffer\(\).*?\);',s,re.S)[0]
version=int(re.search(r'FORMAT_VERSION = (\d+)',(root/'drivers/metal/rendering_shader_container_metal.h').read_text())[1])
text='''// Generated verbatim source slices; mocks only provide surrounding state/types.
#define ERR_FAIL_COND_V_MSG(condition,result,message) do { if (condition) return result; } while (false)
namespace RDC { enum UniformType { UNIFORM_TYPE_STORAGE_BUFFER, UNIFORM_TYPE_STORAGE_BUFFER_DYNAMIC, UNIFORM_TYPE_UNIFORM_BUFFER, UNIFORM_TYPE_UNIFORM_BUFFER_DYNAMIC, UNIFORM_TYPE_TEXTURE }; }
static bool source_aux(spirv_cross::CompilerMSL::Options msl_options, bool has_storage_buffer, uint64_t next_arg_buffer_index, uint32_t idx_dset, std::vector<spirv_cross::MSLResourceBinding> &buffer_size_bindings) {
using namespace spirv_cross;
'''+aux+'''
return true;
}
static bool source_array(RDC::UniformType type, uint32_t dimensions) {
struct { struct { uint32_t dims_count; } array; } binding{{dimensions}};
bool is_storage_buffer = type == RDC::UNIFORM_TYPE_STORAGE_BUFFER || type == RDC::UNIFORM_TYPE_STORAGE_BUFFER_DYNAMIC;
'''+array+'''
return true;
}
static bool source_direct(const spirv_cross::CompilerMSL::Options &msl_options, const spirv_cross::CompilerMSL &compiler) {
'''+direct+'''
return true;
}
#undef ERR_FAIL_COND_V_MSG
'''+f'constexpr uint32_t source_format_version = {version};\n'
(here/'source_slices.gen.h').write_text(text)
(here/'SOURCE_SLICES.json').write_text(json.dumps({'source':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'aux':aux,'descriptor_array':array,'direct':direct,'format_version':version},indent=2)+'\n')
