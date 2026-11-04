// Generated verbatim source slices; mocks only provide surrounding state/types.
#define ERR_FAIL_COND_V_MSG(condition,result,message) do { if (condition) return result; } while (false)
namespace RDC { enum UniformType { UNIFORM_TYPE_STORAGE_BUFFER, UNIFORM_TYPE_STORAGE_BUFFER_DYNAMIC, UNIFORM_TYPE_UNIFORM_BUFFER, UNIFORM_TYPE_UNIFORM_BUFFER_DYNAMIC, UNIFORM_TYPE_TEXTURE }; }
static bool source_aux(spirv_cross::CompilerMSL::Options msl_options, bool has_storage_buffer, uint64_t next_arg_buffer_index, uint32_t idx_dset, std::vector<spirv_cross::MSLResourceBinding> &buffer_size_bindings) {
using namespace spirv_cross;
if (msl_options.argument_buffers && has_storage_buffer) {
				MetalBufferSizeTable layout;
				ERR_FAIL_COND_V_MSG(!layout.initialize(uint32_t(next_arg_buffer_index)), false, "Metal buffer-size table exceeds the argument allocation limit.");
				MSLResourceBinding auxiliary;
				auxiliary.desc_set = idx_dset;
				auxiliary.binding = kBufferSizeBufferBinding;
				auxiliary.basetype = SPIRType::UInt;
				auxiliary.count = 1;
				auxiliary.msl_buffer = layout.resource_count;
				buffer_size_bindings.push_back(auxiliary);
			}
return true;
}
static bool source_array(RDC::UniformType type, uint32_t dimensions) {
struct { struct { uint32_t dims_count; } array; } binding{{dimensions}};
bool is_storage_buffer = type == RDC::UNIFORM_TYPE_STORAGE_BUFFER || type == RDC::UNIFORM_TYPE_STORAGE_BUFFER_DYNAMIC;
ERR_FAIL_COND_V_MSG((is_storage_buffer || type == RDC::UNIFORM_TYPE_UNIFORM_BUFFER || type == RDC::UNIFORM_TYPE_UNIFORM_BUFFER_DYNAMIC) && binding.array.dims_count != 0,
						false, "Metal buffer descriptor arrays are not supported by the RenderingDevice binding ABI.");
return true;
}
static bool source_direct(const spirv_cross::CompilerMSL::Options &msl_options, const spirv_cross::CompilerMSL &compiler) {
ERR_FAIL_COND_V_MSG(!msl_options.argument_buffers && compiler.needs_buffer_size_buffer(), false,
				"Metal OpArrayLength requires Tier2 argument-buffer binding; direct binding is not supported.");
return true;
}
#undef ERR_FAIL_COND_V_MSG
constexpr uint32_t source_format_version = 4;
