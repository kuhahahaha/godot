/**************************************************************************/
/*  rendering_shader_container.cpp                                        */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "rendering_shader_container.h"

#include "core/crypto/crypto_core.h"
#include "core/io/compression.h"
#include "core/io/marshalls.h"
#include "servers/rendering/renderer_rd/shader_rd.h"

#include <thirdparty/spirv-reflect/spirv_reflect.h>

PackedByteArray ShaderCompilePolicy::library_key(const LibraryKeyInput &p_input) {
	PackedByteArray encoded;
	auto append = [&encoded](const uint8_t *p_bytes, uint64_t p_size) {
		const int64_t offset = encoded.size();
		encoded.resize(offset + p_size);
		if (p_size) {
			memcpy(encoded.ptrw() + offset, p_bytes, p_size);
		}
	};
	auto u32 = [&append](uint32_t p_value) {
		uint8_t bytes[4];
		encode_uint32(p_value, bytes);
		append(bytes, sizeof(bytes));
	};
	auto u64 = [&append](uint64_t p_value) {
		uint8_t bytes[8];
		encode_uint64(p_value, bytes);
		append(bytes, sizeof(bytes));
	};
	const char domain[] = "godot.metal.library.v1";
	u64(sizeof(domain) - 1);
	append(reinterpret_cast<const uint8_t *>(domain), sizeof(domain) - 1);
	u32(1);
	u32(p_input.compile_path_version);
	u32(p_input.stage);
	u64(p_input.source.size());
	append(p_input.source.ptr(), p_input.source.size());
	u32(p_input.requested_flags);
	u32(p_input.effective_disable);
	u32(p_input.msl_version);
	u32(p_input.position_invariance);
	u32(p_input.enable_logging);
	u32(p_input.compiled_platform);
	u32(p_input.compiled_gpu_family);
	u32(p_input.profile_min_os_version);
	u32(p_input.binary_os_min_version);
	u32(p_input.actual_platform);
	u32(p_input.actual_gpu_family);
	u32(p_input.actual_os_version);
	u32(p_input.origin);
	u64(p_input.library_size);
	append(p_input.metallib_sha256, sizeof(p_input.metallib_sha256));
	PackedByteArray result;
	result.resize(32);
	ERR_FAIL_COND_V(CryptoCore::sha256(encoded.ptr(), encoded.size(), result.ptrw()) != OK, PackedByteArray());
	return result;
}

bool ShaderCompilePolicy::flags_valid(BitField<RDC::ShaderCompileFlags> p_requested, BitField<RDC::ShaderCompileFlags> p_supported) {
	const uint64_t requested = static_cast<uint64_t>(p_requested);
	return (requested & ~uint64_t(RDC::SHADER_COMPILE_FLAG_DISABLE_FAST_MATH)) == 0 && (requested & ~uint64_t(p_supported)) == 0;
}

bool ShaderCompilePolicy::effective_disable(BitField<RDC::ShaderCompileFlags> p_requested, bool p_supports_fast_math) {
	return p_requested.has_flag(RDC::SHADER_COMPILE_FLAG_DISABLE_FAST_MATH) || !p_supports_fast_math;
}

static inline uint32_t aligned_to(uint32_t p_size, uint32_t p_alignment) {
	if (p_size % p_alignment) {
		return p_size + (p_alignment - (p_size % p_alignment));
	} else {
		return p_size;
	}
}

template <class T>
const T &RenderingShaderContainer::ReflectSymbol<T>::get_spv_reflect(RDC::ShaderStage p_stage) const {
	const T *info = _spv_reflect[get_index_for_stage(p_stage)];
	DEV_ASSERT(info != nullptr); // Caller is expected to specify valid shader stages
	return *info;
}

template <class T>
void RenderingShaderContainer::ReflectSymbol<T>::set_spv_reflect(RDC::ShaderStage p_stage, const T *p_spv) {
	stages.set_flag(1 << p_stage);
	_spv_reflect[get_index_for_stage(p_stage)] = p_spv;
}

RenderingShaderContainer::ReflectShaderStage::ReflectShaderStage() {
	_module = memnew(SpvReflectShaderModule);
	memset(_module, 0, sizeof(SpvReflectShaderModule));
}

RenderingShaderContainer::ReflectShaderStage::~ReflectShaderStage() {
	spvReflectDestroyShaderModule(_module);
	memdelete(_module);
	_module = nullptr;
}

const SpvReflectShaderModule &RenderingShaderContainer::ReflectShaderStage::module() const {
	return *_module;
}

const Span<uint32_t> RenderingShaderContainer::ReflectShaderStage::spirv() const {
	return _spirv_data.span().reinterpret<uint32_t>();
}

bool RenderingShaderContainer::_from_bytes_header_extra_data(ContainerReader &r_reader) {
	return true;
}

bool RenderingShaderContainer::_from_bytes_reflection_extra_data(ContainerReader &r_reader) {
	return true;
}

bool RenderingShaderContainer::_from_bytes_reflection_binding_uniform_extra_data_start(ContainerReader &r_reader) {
	return true;
}

bool RenderingShaderContainer::_from_bytes_reflection_binding_uniform_extra_data(ContainerReader &r_reader, uint32_t p_index) {
	return true;
}

bool RenderingShaderContainer::_from_bytes_reflection_specialization_extra_data_start(ContainerReader &r_reader) {
	return true;
}

bool RenderingShaderContainer::_from_bytes_reflection_specialization_extra_data(ContainerReader &r_reader, uint32_t p_index) {
	return true;
}

bool RenderingShaderContainer::_from_bytes_shader_extra_data_start(ContainerReader &r_reader) {
	return true;
}

bool RenderingShaderContainer::_from_bytes_shader_extra_data(ContainerReader &r_reader, uint32_t p_index) {
	return true;
}

bool RenderingShaderContainer::_from_bytes_footer_extra_data(ContainerReader &r_reader) {
	return true;
}

uint32_t RenderingShaderContainer::_to_bytes_header_extra_data(uint8_t *) const {
	return 0;
}

uint32_t RenderingShaderContainer::_to_bytes_reflection_extra_data(uint8_t *) const {
	return 0;
}

uint32_t RenderingShaderContainer::_to_bytes_reflection_binding_uniform_extra_data(uint8_t *, uint32_t) const {
	return 0;
}

uint32_t RenderingShaderContainer::_to_bytes_reflection_specialization_extra_data(uint8_t *, uint32_t) const {
	return 0;
}

uint32_t RenderingShaderContainer::_to_bytes_shader_extra_data(uint8_t *, uint32_t) const {
	return 0;
}

uint32_t RenderingShaderContainer::_to_bytes_footer_extra_data(uint8_t *) const {
	return 0;
}

void RenderingShaderContainer::_set_from_shader_reflection_post(const ReflectShader &p_shader) {
	// Do nothing.
}

static RenderingDeviceCommons::DataFormat spv_image_format_to_data_format(const SpvImageFormat p_format) {
	using RDC = RenderingDeviceCommons;
	switch (p_format) {
		case SpvImageFormatUnknown:
			return RDC::DATA_FORMAT_MAX;
		case SpvImageFormatRgba32f:
			return RDC::DATA_FORMAT_R32G32B32A32_SFLOAT;
		case SpvImageFormatRgba16f:
			return RDC::DATA_FORMAT_R16G16B16A16_SFLOAT;
		case SpvImageFormatR32f:
			return RDC::DATA_FORMAT_R32_SFLOAT;
		case SpvImageFormatRgba8:
			return RDC::DATA_FORMAT_R8G8B8A8_UNORM;
		case SpvImageFormatRgba8Snorm:
			return RDC::DATA_FORMAT_R8G8B8A8_SNORM;
		case SpvImageFormatRg32f:
			return RDC::DATA_FORMAT_R32G32_SFLOAT;
		case SpvImageFormatRg16f:
			return RDC::DATA_FORMAT_R16G16_SFLOAT;
		case SpvImageFormatR11fG11fB10f:
			return RDC::DATA_FORMAT_B10G11R11_UFLOAT_PACK32;
		case SpvImageFormatR16f:
			return RDC::DATA_FORMAT_R16_SFLOAT;
		case SpvImageFormatRgba16:
			return RDC::DATA_FORMAT_R16G16B16A16_UNORM;
		case SpvImageFormatRgb10A2:
			return RDC::DATA_FORMAT_A2B10G10R10_UNORM_PACK32;
		case SpvImageFormatRg16:
			return RDC::DATA_FORMAT_R16G16_UNORM;
		case SpvImageFormatRg8:
			return RDC::DATA_FORMAT_R8G8_UNORM;
		case SpvImageFormatR16:
			return RDC::DATA_FORMAT_R16_UNORM;
		case SpvImageFormatR8:
			return RDC::DATA_FORMAT_R8_UNORM;
		case SpvImageFormatRgba16Snorm:
			return RDC::DATA_FORMAT_R16G16B16A16_SNORM;
		case SpvImageFormatRg16Snorm:
			return RDC::DATA_FORMAT_R16G16_SNORM;
		case SpvImageFormatRg8Snorm:
			return RDC::DATA_FORMAT_R8G8_SNORM;
		case SpvImageFormatR16Snorm:
			return RDC::DATA_FORMAT_R16_SNORM;
		case SpvImageFormatR8Snorm:
			return RDC::DATA_FORMAT_R8_SNORM;
		case SpvImageFormatRgba32i:
			return RDC::DATA_FORMAT_R32G32B32A32_SINT;
		case SpvImageFormatRgba16i:
			return RDC::DATA_FORMAT_R16G16B16A16_SINT;
		case SpvImageFormatRgba8i:
			return RDC::DATA_FORMAT_R8G8B8A8_SINT;
		case SpvImageFormatR32i:
			return RDC::DATA_FORMAT_R32_SINT;
		case SpvImageFormatRg32i:
			return RDC::DATA_FORMAT_R32G32_SINT;
		case SpvImageFormatRg16i:
			return RDC::DATA_FORMAT_R16G16_SINT;
		case SpvImageFormatRg8i:
			return RDC::DATA_FORMAT_R8G8_SINT;
		case SpvImageFormatR16i:
			return RDC::DATA_FORMAT_R16_SINT;
		case SpvImageFormatR8i:
			return RDC::DATA_FORMAT_R8_SINT;
		case SpvImageFormatRgba32ui:
			return RDC::DATA_FORMAT_R32G32B32A32_UINT;
		case SpvImageFormatRgba16ui:
			return RDC::DATA_FORMAT_R16G16B16A16_UINT;
		case SpvImageFormatRgba8ui:
			return RDC::DATA_FORMAT_R8G8B8A8_UINT;
		case SpvImageFormatR32ui:
			return RDC::DATA_FORMAT_R32_UINT;
		case SpvImageFormatRgb10a2ui:
			return RDC::DATA_FORMAT_A2B10G10R10_UINT_PACK32;
		case SpvImageFormatRg32ui:
			return RDC::DATA_FORMAT_R32G32_UINT;
		case SpvImageFormatRg16ui:
			return RDC::DATA_FORMAT_R16G16_UINT;
		case SpvImageFormatRg8ui:
			return RDC::DATA_FORMAT_R8G8_UINT;
		case SpvImageFormatR16ui:
			return RDC::DATA_FORMAT_R16_UINT;
		case SpvImageFormatR8ui:
			return RDC::DATA_FORMAT_R8_UINT;
		case SpvImageFormatR64ui:
			return RDC::DATA_FORMAT_R64_UINT;
		case SpvImageFormatR64i:
			return RDC::DATA_FORMAT_R64_SINT;
		case SpvImageFormatMax:
			return RDC::DATA_FORMAT_MAX;
	}
	return RDC::DATA_FORMAT_MAX;
}

Error RenderingShaderContainer::reflect_spirv(const String &p_shader_name, Span<RDC::ShaderStageSPIRVData> p_spirv, ReflectShader &r_shader) {
	ReflectShader &reflection = r_shader;

	shader_name = p_shader_name.utf8();

	const uint32_t spirv_size = p_spirv.size() + 0;

	LocalVector<ReflectShaderStage> &r_refl = r_shader.shader_stages;
	r_refl.resize(spirv_size);

	bool pipeline_type_detected = false;
	for (uint32_t i = 0; i < spirv_size; i++) {
		RDC::ShaderStage stage = p_spirv[i].shader_stage;
		RDC::ShaderStage stage_flag = (RDC::ShaderStage)(1 << stage);
		r_refl[i].shader_stage = stage;
		r_refl[i]._spirv_data = p_spirv[i].spirv;

		RDC::PipelineType pipeline_type = {};
		switch (stage) {
			case RDC::SHADER_STAGE_VERTEX:
			case RDC::SHADER_STAGE_FRAGMENT:
			case RDC::SHADER_STAGE_TESSELATION_CONTROL:
			case RDC::SHADER_STAGE_TESSELATION_EVALUATION:
				pipeline_type = RDC::PIPELINE_TYPE_RASTERIZATION;
				break;
			case RDC::SHADER_STAGE_COMPUTE:
				pipeline_type = RDC::PIPELINE_TYPE_COMPUTE;
				break;
			case RDC::SHADER_STAGE_RAYGEN:
			case RDC::SHADER_STAGE_ANY_HIT:
			case RDC::SHADER_STAGE_CLOSEST_HIT:
			case RDC::SHADER_STAGE_MISS:
			case RDC::SHADER_STAGE_INTERSECTION:
				pipeline_type = RDC::PIPELINE_TYPE_RAYTRACING;
				break;
			default:
				DEV_ASSERT(false && "Unknown shader stage.");
		}

		if (pipeline_type_detected) {
			ERR_FAIL_COND_V_MSG(r_shader.pipeline_type != pipeline_type, FAILED, "Shader stages of different pipeline types cannot be mixed in the same shader container.");
		} else {
			r_shader.pipeline_type = pipeline_type;
			pipeline_type_detected = true;
		}

		const Vector<uint64_t> &dynamic_buffers = p_spirv[i].dynamic_buffers;

		if (stage == RDC::SHADER_STAGE_COMPUTE) {
			ERR_FAIL_COND_V_MSG(spirv_size != 1, FAILED,
					"Compute shaders can only receive one stage, dedicated to compute.");
		}
		ERR_FAIL_COND_V_MSG(reflection.stages_bits.has_flag(stage_flag), FAILED,
				"Stage " + String(RDC::SHADER_STAGE_NAMES[stage]) + " submitted more than once.");
		reflection.stages_bits.set_flag(stage_flag);

		// We make all raytracing stages visible to make our lives easier when creating raytracing pipelines.
		// This makes no practical difference in current graphics drivers, since Vulkan is the outlier.
		BitField<RDC::ShaderStage> uniform_stage_flags;
		if (pipeline_type == RDC::PIPELINE_TYPE_RAYTRACING) {
			uniform_stage_flags = RDC::SHADER_STAGE_RAYGEN |
					RDC::SHADER_STAGE_ANY_HIT |
					RDC::SHADER_STAGE_CLOSEST_HIT |
					RDC::SHADER_STAGE_MISS |
					RDC::SHADER_STAGE_INTERSECTION;
		} else {
			uniform_stage_flags = stage_flag;
		}

		{
			SpvReflectShaderModule &module = *r_refl.ptr()[i]._module;
			const uint8_t *spirv = p_spirv[i].spirv.ptr();
			SpvReflectResult result = spvReflectCreateShaderModule2(SPV_REFLECT_MODULE_FLAG_NO_COPY, p_spirv[i].spirv.size(), spirv, &module);
			ERR_FAIL_COND_V_MSG(result != SPV_REFLECT_RESULT_SUCCESS, FAILED,
					"Reflection of SPIR-V shader stage '" + String(RDC::SHADER_STAGE_NAMES[p_spirv[i].shader_stage]) + "' failed parsing shader.");

			for (uint32_t j = 0; j < module.capability_count; j++) {
				if (module.capabilities[j].value == SpvCapabilityMultiView) {
					reflection.has_multiview = true;
					break;
				}
			}

			if (reflection.is_compute()) {
				reflection.compute_local_size[0] = module.entry_points->local_size.x;
				reflection.compute_local_size[1] = module.entry_points->local_size.y;
				reflection.compute_local_size[2] = module.entry_points->local_size.z;
			}
			uint32_t binding_count = 0;
			result = spvReflectEnumerateDescriptorBindings(&module, &binding_count, nullptr);
			ERR_FAIL_COND_V_MSG(result != SPV_REFLECT_RESULT_SUCCESS, FAILED,
					"Reflection of SPIR-V shader stage '" + String(RDC::SHADER_STAGE_NAMES[p_spirv[i].shader_stage]) + "' failed enumerating descriptor bindings.");

			if (binding_count > 0) {
				// Parse bindings.

				Vector<SpvReflectDescriptorBinding *> bindings;
				bindings.resize(binding_count);
				result = spvReflectEnumerateDescriptorBindings(&module, &binding_count, bindings.ptrw());

				ERR_FAIL_COND_V_MSG(result != SPV_REFLECT_RESULT_SUCCESS, FAILED,
						"Reflection of SPIR-V shader stage '" + String(RDC::SHADER_STAGE_NAMES[p_spirv[i].shader_stage]) + "' failed getting descriptor bindings.");

				for (uint32_t j = 0; j < binding_count; j++) {
					const SpvReflectDescriptorBinding &binding = *bindings[j];

					ReflectUniform uniform;
					uniform.set_spv_reflect(stage, &binding);

					bool need_array_dimensions = false;
					bool need_block_size = false;
					bool may_be_writable = false;
					bool is_image = false;

					switch (binding.descriptor_type) {
						case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER: {
							uniform.type = RDC::UNIFORM_TYPE_SAMPLER;
							need_array_dimensions = true;
						} break;
						case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: {
							uniform.type = RDC::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE;
							need_array_dimensions = true;
							is_image = true;
						} break;
						case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE: {
							uniform.type = RDC::UNIFORM_TYPE_TEXTURE;
							need_array_dimensions = true;
							is_image = true;
						} break;
						case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE: {
							uniform.type = RDC::UNIFORM_TYPE_IMAGE;
							need_array_dimensions = true;
							may_be_writable = true;
							is_image = true;
						} break;
						case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER: {
							uniform.type = RDC::UNIFORM_TYPE_TEXTURE_BUFFER;
							need_array_dimensions = true;
							is_image = true;
						} break;
						case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER: {
							uniform.type = RDC::UNIFORM_TYPE_IMAGE_BUFFER;
							need_array_dimensions = true;
							may_be_writable = true;
							is_image = true;
						} break;
						case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER: {
							const uint64_t key = ShaderRD::DynamicBuffer::encode(binding.set, binding.binding);
							if (dynamic_buffers.has(key)) {
								uniform.type = RDC::UNIFORM_TYPE_UNIFORM_BUFFER_DYNAMIC;
								reflection.has_dynamic_buffers = true;
							} else {
								uniform.type = RDC::UNIFORM_TYPE_UNIFORM_BUFFER;
							}
							need_block_size = true;
						} break;
						case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER: {
							const uint64_t key = ShaderRD::DynamicBuffer::encode(binding.set, binding.binding);
							if (dynamic_buffers.has(key)) {
								uniform.type = RDC::UNIFORM_TYPE_STORAGE_BUFFER_DYNAMIC;
								reflection.has_dynamic_buffers = true;
							} else {
								uniform.type = RDC::UNIFORM_TYPE_STORAGE_BUFFER;
							}
							need_block_size = true;
							may_be_writable = true;
						} break;
						case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC: {
							ERR_PRINT("Dynamic uniform buffer not supported.");
							continue;
						} break;
						case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC: {
							ERR_PRINT("Dynamic storage buffer not supported.");
							continue;
						} break;
						case SPV_REFLECT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT: {
							uniform.type = RDC::UNIFORM_TYPE_INPUT_ATTACHMENT;
							need_array_dimensions = true;
							is_image = true;
						} break;
						case SPV_REFLECT_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR: {
							uniform.type = RDC::UNIFORM_TYPE_ACCELERATION_STRUCTURE;
						} break;
					}

					if (need_array_dimensions) {
						uniform.length = 1;
						for (uint32_t k = 0; k < binding.array.dims_count; k++) {
							uniform.length *= binding.array.dims[k];
						}
					} else if (need_block_size) {
						uniform.length = binding.block.size;
					} else {
						uniform.length = 0;
					}

					if (may_be_writable) {
						if (binding.descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE) {
							uniform.writable = !(binding.decoration_flags & SPV_REFLECT_DECORATION_NON_WRITABLE);
						} else {
							uniform.writable = !(binding.decoration_flags & SPV_REFLECT_DECORATION_NON_WRITABLE) && !(binding.block.decoration_flags & SPV_REFLECT_DECORATION_NON_WRITABLE);
						}
					} else {
						uniform.writable = false;
					}

					if (is_image) {
						uniform.image.format = spv_image_format_to_data_format(binding.image.image_format);
					}

					uniform.binding = binding.binding;
					uint32_t set = binding.set;

					ERR_FAIL_COND_V_MSG(set >= RDC::MAX_UNIFORM_SETS, FAILED,
							"On shader stage '" + String(RDC::SHADER_STAGE_NAMES[stage]) + "', uniform '" + binding.name + "' uses a set (" + itos(set) + ") index larger than what is supported (" + itos(RDC::MAX_UNIFORM_SETS) + ").");

					if (set < (uint32_t)reflection.uniform_sets.size()) {
						// Check if this already exists.
						bool exists = false;
						for (uint32_t k = 0; k < reflection.uniform_sets[set].size(); k++) {
							if (reflection.uniform_sets[set][k].binding == uniform.binding) {
								// Already exists, verify that it's the same type.
								ERR_FAIL_COND_V_MSG(reflection.uniform_sets[set][k].type != uniform.type, FAILED,
										"On shader stage '" + String(RDC::SHADER_STAGE_NAMES[stage]) + "', uniform '" + binding.name + "' trying to reuse location for set=" + itos(set) + ", binding=" + itos(uniform.binding) + " with different uniform type.");

								// Also, verify that it's the same size.
								ERR_FAIL_COND_V_MSG(reflection.uniform_sets[set][k].length != uniform.length, FAILED,
										"On shader stage '" + String(RDC::SHADER_STAGE_NAMES[stage]) + "', uniform '" + binding.name + "' trying to reuse location for set=" + itos(set) + ", binding=" + itos(uniform.binding) + " with different uniform size.");

								// Also, verify that it has the same writability.
								ERR_FAIL_COND_V_MSG(reflection.uniform_sets[set][k].writable != uniform.writable, FAILED,
										"On shader stage '" + String(RDC::SHADER_STAGE_NAMES[stage]) + "', uniform '" + binding.name + "' trying to reuse location for set=" + itos(set) + ", binding=" + itos(uniform.binding) + " with different writability.");

								// Just append stage mask and return.
								reflection.uniform_sets[set][k].stages.set_flag(uniform_stage_flags);
								exists = true;
								break;
							}
						}

						if (exists) {
							continue; // Merged.
						}
					}

					uniform.stages.set_flag(uniform_stage_flags);

					if (set >= (uint32_t)reflection.uniform_sets.size()) {
						reflection.uniform_sets.resize(set + 1);
					}

					reflection.uniform_sets[set].push_back(uniform);
				}
			}

			{
				// Specialization constants.

				uint32_t sc_count = 0;
				result = spvReflectEnumerateSpecializationConstants(&module, &sc_count, nullptr);
				ERR_FAIL_COND_V_MSG(result != SPV_REFLECT_RESULT_SUCCESS, FAILED,
						"Reflection of SPIR-V shader stage '" + String(RDC::SHADER_STAGE_NAMES[p_spirv[i].shader_stage]) + "' failed enumerating specialization constants.");

				if (sc_count) {
					Vector<SpvReflectSpecializationConstant *> spec_constants;
					spec_constants.resize(sc_count);

					result = spvReflectEnumerateSpecializationConstants(&module, &sc_count, spec_constants.ptrw());
					ERR_FAIL_COND_V_MSG(result != SPV_REFLECT_RESULT_SUCCESS, FAILED,
							"Reflection of SPIR-V shader stage '" + String(RDC::SHADER_STAGE_NAMES[p_spirv[i].shader_stage]) + "' failed obtaining specialization constants.");

					for (uint32_t j = 0; j < sc_count; j++) {
						int32_t existing = -1;
						ReflectSpecializationConstant sconst;
						SpvReflectSpecializationConstant *spc = spec_constants[j];
						sconst.set_spv_reflect(stage, spc);

						if (spc->default_value_size != 4) {
							ERR_FAIL_V_MSG(FAILED, vformat("Reflection of SPIR-V shader stage '%s' failed because the specialization constant #%d's default value is not 4 bytes long (%d) and is currently not supported.", RDC::SHADER_STAGE_NAMES[p_spirv[i].shader_stage], spc->constant_id, spc->default_value_size));
						}

						sconst.constant_id = spc->constant_id;
						sconst.int_value = 0; // Clear previous value JIC.

						switch (spc->type_description->op) {
							case SpvOpTypeBool:
								sconst.type = RDC::PIPELINE_SPECIALIZATION_CONSTANT_TYPE_BOOL;
								sconst.bool_value = *(uint32_t *)(spc->default_value);
								break;
							case SpvOpTypeInt:
								sconst.type = RDC::PIPELINE_SPECIALIZATION_CONSTANT_TYPE_INT;
								sconst.int_value = *(uint32_t *)(spc->default_value);
								break;
							case SpvOpTypeFloat:
								sconst.type = RDC::PIPELINE_SPECIALIZATION_CONSTANT_TYPE_FLOAT;
								sconst.float_value = *(float *)(spc->default_value);
								break;
							default:
								ERR_FAIL_V_MSG(FAILED, vformat("Reflection of SPIR-V shader stage '%s' failed because the specialization constant #%d does not use a known operation (%d).", RDC::SHADER_STAGE_NAMES[p_spirv[i].shader_stage], spc->constant_id, spc->type_description->op));
								break;
						}

						sconst.stages.set_flag(stage_flag);

						for (uint32_t k = 0; k < reflection.specialization_constants.size(); k++) {
							if (reflection.specialization_constants[k].constant_id == sconst.constant_id) {
								ERR_FAIL_COND_V_MSG(reflection.specialization_constants[k].type != sconst.type, FAILED, "More than one specialization constant used for id (" + itos(sconst.constant_id) + "), but their types differ.");
								ERR_FAIL_COND_V_MSG(reflection.specialization_constants[k].int_value != sconst.int_value, FAILED, "More than one specialization constant used for id (" + itos(sconst.constant_id) + "), but their default values differ.");
								existing = k;
								break;
							}
						}

						if (existing >= 0) {
							reflection.specialization_constants[existing].stages.set_flag(stage_flag);
						} else {
							reflection.specialization_constants.push_back(sconst);
						}
					}

					reflection.specialization_constants.sort();
				}
			}

			if (stage == RDC::SHADER_STAGE_VERTEX || stage == RDC::SHADER_STAGE_FRAGMENT) {
				uint32_t iv_count = 0;
				result = spvReflectEnumerateInputVariables(&module, &iv_count, nullptr);
				ERR_FAIL_COND_V_MSG(result != SPV_REFLECT_RESULT_SUCCESS, FAILED,
						"Reflection of SPIR-V shader stage '" + String(RDC::SHADER_STAGE_NAMES[p_spirv[i].shader_stage]) + "' failed enumerating input variables.");

				if (iv_count) {
					Vector<SpvReflectInterfaceVariable *> input_vars;
					input_vars.resize(iv_count);

					result = spvReflectEnumerateInputVariables(&module, &iv_count, input_vars.ptrw());
					ERR_FAIL_COND_V_MSG(result != SPV_REFLECT_RESULT_SUCCESS, FAILED,
							"Reflection of SPIR-V shader stage '" + String(RDC::SHADER_STAGE_NAMES[p_spirv[i].shader_stage]) + "' failed obtaining input variables.");

					for (const SpvReflectInterfaceVariable *v : input_vars) {
						if (!v) {
							continue;
						}
						if (stage == RDC::SHADER_STAGE_VERTEX) {
							if (v->decoration_flags == 0) { // Regular input.
								reflection.vertex_input_mask |= (((uint64_t)1) << v->location);
							}
						}
						if (v->built_in == SpvBuiltInViewIndex) {
							reflection.has_multiview = true;
						}
					}
				}
			}

			if (stage == RDC::SHADER_STAGE_FRAGMENT) {
				uint32_t ov_count = 0;
				result = spvReflectEnumerateOutputVariables(&module, &ov_count, nullptr);
				ERR_FAIL_COND_V_MSG(result != SPV_REFLECT_RESULT_SUCCESS, FAILED,
						"Reflection of SPIR-V shader stage '" + String(RDC::SHADER_STAGE_NAMES[p_spirv[i].shader_stage]) + "' failed enumerating output variables.");

				if (ov_count) {
					Vector<SpvReflectInterfaceVariable *> output_vars;
					output_vars.resize(ov_count);

					result = spvReflectEnumerateOutputVariables(&module, &ov_count, output_vars.ptrw());
					ERR_FAIL_COND_V_MSG(result != SPV_REFLECT_RESULT_SUCCESS, FAILED,
							"Reflection of SPIR-V shader stage '" + String(RDC::SHADER_STAGE_NAMES[p_spirv[i].shader_stage]) + "' failed obtaining output variables.");

					for (const SpvReflectInterfaceVariable *refvar : output_vars) {
						if (!refvar) {
							continue;
						}
						if (refvar->built_in != SpvBuiltInFragDepth) {
							reflection.fragment_output_mask |= 1 << refvar->location;
						}
					}
				}
			}

			uint32_t pc_count = 0;
			result = spvReflectEnumeratePushConstantBlocks(&module, &pc_count, nullptr);
			ERR_FAIL_COND_V_MSG(result != SPV_REFLECT_RESULT_SUCCESS, FAILED,
					"Reflection of SPIR-V shader stage '" + String(RDC::SHADER_STAGE_NAMES[p_spirv[i].shader_stage]) + "' failed enumerating push constants.");

			if (pc_count) {
				ERR_FAIL_COND_V_MSG(pc_count > 1, FAILED,
						"Reflection of SPIR-V shader stage '" + String(RDC::SHADER_STAGE_NAMES[p_spirv[i].shader_stage]) + "': Only one push constant is supported, which should be the same across shader stages.");

				Vector<SpvReflectBlockVariable *> pconstants;
				pconstants.resize(pc_count);
				result = spvReflectEnumeratePushConstantBlocks(&module, &pc_count, pconstants.ptrw());
				ERR_FAIL_COND_V_MSG(result != SPV_REFLECT_RESULT_SUCCESS, FAILED,
						"Reflection of SPIR-V shader stage '" + String(RDC::SHADER_STAGE_NAMES[p_spirv[i].shader_stage]) + "' failed obtaining push constants.");
#if 0
				if (pconstants[0] == nullptr) {
					Ref<FileAccess> f = FileAccess::open("res://popo.spv", FileAccess::WRITE);
					f->store_buffer((const uint8_t *)&SpirV[0], SpirV.size() * sizeof(uint32_t));
				}
#endif

				ERR_FAIL_COND_V_MSG(reflection.push_constant_size && reflection.push_constant_size != pconstants[0]->size, FAILED,
						"Reflection of SPIR-V shader stage '" + String(RDC::SHADER_STAGE_NAMES[p_spirv[i].shader_stage]) + "': Push constant block must be the same across shader stages.");

				reflection.push_constant_size = pconstants[0]->size;
				reflection.push_constant_stages.set_flag(uniform_stage_flags);

				//print_line("Stage: " + String(RDC::SHADER_STAGE_NAMES[stage]) + " push constant of size=" + itos(push_constant.push_constant_size));
			}
		}
	}

	// Sort all uniform_sets by binding.
	for (uint32_t i = 0; i < reflection.uniform_sets.size(); i++) {
		reflection.uniform_sets[i].sort();
	}

	set_from_shader_reflection(reflection);

	return OK;
}

void RenderingShaderContainer::set_from_shader_reflection(const ReflectShader &p_reflection) {
	reflection_binding_set_uniforms_count.clear();
	reflection_binding_set_uniforms_data.clear();
	reflection_specialization_data.clear();
	reflection_shader_stages.clear();

	reflection_data.vertex_input_mask = p_reflection.vertex_input_mask;
	reflection_data.fragment_output_mask = p_reflection.fragment_output_mask;
	reflection_data.specialization_constants_count = p_reflection.specialization_constants.size();
	reflection_data.pipeline_type = p_reflection.pipeline_type;
	reflection_data.has_multiview = p_reflection.has_multiview;
	reflection_data.has_dynamic_buffers = p_reflection.has_dynamic_buffers;
	reflection_data.compute_local_size[0] = p_reflection.compute_local_size[0];
	reflection_data.compute_local_size[1] = p_reflection.compute_local_size[1];
	reflection_data.compute_local_size[2] = p_reflection.compute_local_size[2];
	reflection_data.set_count = p_reflection.uniform_sets.size();
	reflection_data.push_constant_size = p_reflection.push_constant_size;
	reflection_data.push_constant_stages_mask = uint32_t(p_reflection.push_constant_stages);
	reflection_data.shader_name_len = shader_name.length();

	ReflectionBindingData binding_data;
	for (const ReflectDescriptorSet &uniform_set : p_reflection.uniform_sets) {
		for (const ReflectUniform &uniform : uniform_set) {
			binding_data.type = uint32_t(uniform.type);
			binding_data.binding = uniform.binding;
			binding_data.stages = uint32_t(uniform.stages);
			binding_data.length = uniform.length;
			binding_data.writable = uint32_t(uniform.writable);
			reflection_binding_set_uniforms_data.push_back(binding_data);
		}

		reflection_binding_set_uniforms_count.push_back(uniform_set.size());
	}

	ReflectionSpecializationData specialization_data;
	for (const ReflectSpecializationConstant &spec : p_reflection.specialization_constants) {
		specialization_data.type = uint32_t(spec.type);
		specialization_data.constant_id = spec.constant_id;
		specialization_data.int_value = spec.int_value;
		specialization_data.stage_flags = uint32_t(spec.stages);
		reflection_specialization_data.push_back(specialization_data);
	}

	for (uint32_t i = 0; i < RDC::SHADER_STAGE_MAX; i++) {
		if (p_reflection.stages_bits.has_flag(RDC::ShaderStage(1U << i))) {
			reflection_shader_stages.push_back(RDC::ShaderStage(i));
		}
	}

	reflection_data.stage_count = reflection_shader_stages.size();

	_set_from_shader_reflection_post(p_reflection);
}

bool RenderingShaderContainer::set_code_from_spirv(const String &p_shader_name, Span<RDC::ShaderStageSPIRVData> p_spirv, BitField<RDC::ShaderCompileFlags> p_flags) {
	ERR_FAIL_COND_V(!ShaderCompilePolicy::flags_valid(p_flags, RDC::SHADER_COMPILE_FLAG_DISABLE_FAST_MATH), false);
	ERR_FAIL_COND_V(p_spirv.is_empty(), false);
	compile_flags = p_flags;
	ReflectShader shader;
	ERR_FAIL_COND_V(reflect_spirv(p_shader_name, p_spirv, shader) != OK, false);
	return _set_code_from_spirv(shader);
}

RenderingDeviceCommons::ShaderReflection RenderingShaderContainer::get_shader_reflection() const {
	RDC::ShaderReflection shader_refl;
	shader_refl.push_constant_size = reflection_data.push_constant_size;
	shader_refl.push_constant_stages = reflection_data.push_constant_stages_mask;
	shader_refl.vertex_input_mask = reflection_data.vertex_input_mask;
	shader_refl.fragment_output_mask = reflection_data.fragment_output_mask;
	shader_refl.pipeline_type = reflection_data.pipeline_type;
	shader_refl.has_multiview = reflection_data.has_multiview;
	shader_refl.has_dynamic_buffers = reflection_data.has_dynamic_buffers;
	shader_refl.compute_local_size[0] = reflection_data.compute_local_size[0];
	shader_refl.compute_local_size[1] = reflection_data.compute_local_size[1];
	shader_refl.compute_local_size[2] = reflection_data.compute_local_size[2];
	shader_refl.uniform_sets.resize(reflection_data.set_count);
	shader_refl.specialization_constants.resize(reflection_data.specialization_constants_count);
	shader_refl.stages_vector.resize(reflection_data.stage_count);

	DEV_ASSERT(reflection_binding_set_uniforms_count.size() == reflection_data.set_count && "The amount of elements in the reflection and the shader container can't be different.");
	uint32_t uniform_index = 0;
	for (uint32_t i = 0; i < reflection_data.set_count; i++) {
		Vector<RDC::ShaderUniform> &uniform_set = shader_refl.uniform_sets.ptrw()[i];
		uint32_t uniforms_count = reflection_binding_set_uniforms_count[i];
		uniform_set.resize(uniforms_count);
		for (uint32_t j = 0; j < uniforms_count; j++) {
			const ReflectionBindingData &binding = reflection_binding_set_uniforms_data[uniform_index++];
			RDC::ShaderUniform &uniform = uniform_set.ptrw()[j];
			uniform.type = RDC::UniformType(binding.type);
			uniform.writable = binding.writable;
			uniform.length = binding.length;
			uniform.binding = binding.binding;
			uniform.stages = binding.stages;
		}
	}

	shader_refl.specialization_constants.resize(reflection_data.specialization_constants_count);
	for (uint32_t i = 0; i < reflection_data.specialization_constants_count; i++) {
		const ReflectionSpecializationData &spec = reflection_specialization_data[i];
		RDC::ShaderSpecializationConstant &sc = shader_refl.specialization_constants.ptrw()[i];
		sc.type = RDC::PipelineSpecializationConstantType(spec.type);
		sc.constant_id = spec.constant_id;
		sc.int_value = spec.int_value;
		sc.stages = spec.stage_flags;
	}

	shader_refl.stages_vector.resize(reflection_data.stage_count);
	for (uint32_t i = 0; i < reflection_data.stage_count; i++) {
		shader_refl.stages_vector.set(i, reflection_shader_stages[i]);
		shader_refl.stages_bits.set_flag(RDC::ShaderStage(1U << reflection_shader_stages[i]));
	}

	return shader_refl;
}

bool RenderingShaderContainer::from_bytes(const PackedByteArray &p_bytes) {
#ifdef TESTS_ENABLED
	loaded_container_digest = String();
	loaded_container_size = 0;
#endif
	ContainerReader reader(p_bytes.ptr(), p_bytes.size());
	ContainerHeader header;
	ERR_FAIL_COND_V(!reader.read(&header, 8), false);
	ERR_FAIL_COND_V(header.magic_number != CONTAINER_MAGIC_NUMBER || header.version != CONTAINER_VERSION, false);
	ERR_FAIL_COND_V(!reader.read(reinterpret_cast<uint8_t *>(&header) + 8, sizeof(header) - 8), false);
	ERR_FAIL_COND_V(header.format != _format() || header.format_version != _format_version(), false);
	ERR_FAIL_COND_V(!ShaderCompilePolicy::flags_valid(header.requested_compile_flags, RDC::SHADER_COMPILE_FLAG_DISABLE_FAST_MATH), false);
	ERR_FAIL_COND_V(header.shader_count == 0 || header.shader_count > RDC::SHADER_STAGE_MAX, false);
	compile_flags = header.requested_compile_flags;
	ERR_FAIL_COND_V(!_from_bytes_header_extra_data(reader), false);
	ERR_FAIL_COND_V(header.shader_count > reader.remaining() / sizeof(ShaderHeader), false);
	shaders.resize(header.shader_count);
	ERR_FAIL_COND_V(!reader.read(&reflection_data, sizeof(reflection_data)), false);
	ERR_FAIL_COND_V(reflection_data.stage_count != header.shader_count, false);
	ERR_FAIL_COND_V(reflection_data.set_count > reader.remaining() / sizeof(uint32_t), false);
	ERR_FAIL_COND_V(reflection_data.specialization_constants_count > reader.remaining() / sizeof(ReflectionSpecializationData), false);
	ERR_FAIL_COND_V(reflection_data.pipeline_type > RDC::PIPELINE_TYPE_RAYTRACING, false);
	ERR_FAIL_COND_V(!_from_bytes_reflection_extra_data(reader), false);
	ERR_FAIL_COND_V(reflection_data.shader_name_len > reader.remaining() || reflection_data.shader_name_len > INT32_MAX, false);
	Vector<uint8_t> name_bytes;
	name_bytes.resize(reflection_data.shader_name_len);
	ERR_FAIL_COND_V(!reader.read(name_bytes.ptrw(), name_bytes.size()) || !reader.align4(), false);
	String decoded_name;
	if (!name_bytes.is_empty()) {
		ERR_FAIL_COND_V(decoded_name.append_utf8(reinterpret_cast<const char *>(name_bytes.ptr()), name_bytes.size()) != OK, false);
	}
	shader_name = decoded_name.utf8();
	// Reject normalization (including a leading BOM or embedded NUL) rather than
	// retaining an input length that no longer describes the decoded buffer.
	ERR_FAIL_COND_V(shader_name.length() != name_bytes.size(), false);
	ERR_FAIL_COND_V(!name_bytes.is_empty() && memcmp(shader_name.ptr(), name_bytes.ptr(), name_bytes.size()) != 0, false);
	ERR_FAIL_COND_V(reflection_data.set_count > reader.remaining() / sizeof(uint32_t), false);
	reflection_binding_set_uniforms_count.resize(reflection_data.set_count);
	reflection_binding_set_uniforms_data.clear();
	uint32_t uniform_index = 0;
	for (uint32_t i = 0; i < reflection_data.set_count; i++) {
		uint32_t count = 0;
		ERR_FAIL_COND_V(!reader.read(&count, sizeof(count)), false);
		ERR_FAIL_COND_V(count > reader.remaining() / sizeof(ReflectionBindingData) || uint64_t(uniform_index) + count > INT32_MAX, false);
		reflection_binding_set_uniforms_count.write[i] = count;
		reflection_binding_set_uniforms_data.resize(uint64_t(uniform_index) + count);
		ERR_FAIL_COND_V(!_from_bytes_reflection_binding_uniform_extra_data_start(reader), false);
		for (uint32_t j = 0; j < count; j++, uniform_index++) {
			ERR_FAIL_COND_V(!reader.read(&reflection_binding_set_uniforms_data.write[uniform_index], sizeof(ReflectionBindingData)), false);
			ERR_FAIL_COND_V(!_from_bytes_reflection_binding_uniform_extra_data(reader, uniform_index), false);
		}
	}
	ERR_FAIL_COND_V(reflection_data.specialization_constants_count > reader.remaining() / sizeof(ReflectionSpecializationData), false);
	reflection_specialization_data.resize(reflection_data.specialization_constants_count);
	ERR_FAIL_COND_V(!_from_bytes_reflection_specialization_extra_data_start(reader), false);
	for (uint32_t i = 0; i < reflection_data.specialization_constants_count; i++) {
		ERR_FAIL_COND_V(!reader.read(&reflection_specialization_data.write[i], sizeof(ReflectionSpecializationData)), false);
		ERR_FAIL_COND_V(!_from_bytes_reflection_specialization_extra_data(reader, i), false);
	}
	ERR_FAIL_COND_V(reflection_data.stage_count > reader.remaining() / sizeof(RDC::ShaderStage), false);
	reflection_shader_stages.resize(reflection_data.stage_count);
	ERR_FAIL_COND_V(!_from_bytes_shader_extra_data_start(reader), false);
	ERR_FAIL_COND_V(!reader.read(reflection_shader_stages.ptrw(), uint64_t(reflection_data.stage_count) * sizeof(RDC::ShaderStage)), false);
	uint32_t reflected_mask = 0;
	for (RDC::ShaderStage stage : reflection_shader_stages) {
		ERR_FAIL_COND_V(uint32_t(stage) >= RDC::SHADER_STAGE_MAX, false);
		ERR_FAIL_COND_V(reflected_mask & (1U << stage), false);
		reflected_mask |= 1U << stage;
	}
	uint32_t shader_mask = 0;
	for (uint32_t i = 0; i < header.shader_count; i++) {
		ShaderHeader shader_header;
		ERR_FAIL_COND_V(!reader.read(&shader_header, sizeof(shader_header)), false);
		ERR_FAIL_COND_V(shader_header.shader_stage >= RDC::SHADER_STAGE_MAX, false);
		ERR_FAIL_COND_V(shader_mask & (1U << shader_header.shader_stage), false);
		shader_mask |= 1U << shader_header.shader_stage;
		ERR_FAIL_COND_V(shader_header.code_compressed_size > reader.remaining() || shader_header.code_compressed_size > INT32_MAX, false);
		Shader &shader = shaders.write[i];
		shader.shader_stage = RDC::ShaderStage(shader_header.shader_stage);
		shader.code_compression_flags = shader_header.code_compression_flags;
		shader.code_decompressed_size = shader_header.code_decompressed_size;
		shader.code_compressed_bytes.resize(shader_header.code_compressed_size);
		ERR_FAIL_COND_V(!reader.read(shader.code_compressed_bytes.ptrw(), shader_header.code_compressed_size) || !reader.align4(), false);
		ERR_FAIL_COND_V(!_from_bytes_shader_extra_data(reader, i), false);
	}
	ERR_FAIL_COND_V(shader_mask != reflected_mask, false);
	ERR_FAIL_COND_V(!_from_bytes_footer_extra_data(reader), false);
	ERR_FAIL_COND_V(reader.remaining() != 0, false);
#ifdef TESTS_ENABLED
	uint8_t digest[32];
	ERR_FAIL_COND_V(CryptoCore::sha256(p_bytes.ptr(), p_bytes.size(), digest) != OK, false);
	loaded_container_digest = String::hex_encode_buffer(digest, sizeof(digest));
	loaded_container_size = p_bytes.size();
#endif
	return true;
}

PackedByteArray RenderingShaderContainer::to_bytes() const {
	ERR_FAIL_COND_V(reflection_data.shader_name_len != uint32_t(shader_name.length()), PackedByteArray());
	// Compute the exact size the container will require for writing everything out.
	const uint64_t alignment = sizeof(uint32_t);
	uint64_t total_size = 0;
	total_size += sizeof(ContainerHeader) + _to_bytes_header_extra_data(nullptr);
	total_size += sizeof(ReflectionData) + _to_bytes_reflection_extra_data(nullptr);
	total_size += aligned_to(reflection_data.shader_name_len, alignment);
	total_size += reflection_binding_set_uniforms_count.size() * sizeof(uint32_t);
	total_size += reflection_binding_set_uniforms_data.size() * sizeof(ReflectionBindingData);
	total_size += reflection_specialization_data.size() * sizeof(ReflectionSpecializationData);
	total_size += reflection_shader_stages.size() * sizeof(RDC::ShaderStage);

	for (uint32_t i = 0; i < reflection_binding_set_uniforms_data.size(); i++) {
		total_size += _to_bytes_reflection_binding_uniform_extra_data(nullptr, i);
	}

	for (uint32_t i = 0; i < reflection_specialization_data.size(); i++) {
		total_size += _to_bytes_reflection_specialization_extra_data(nullptr, i);
	}

	for (uint32_t i = 0; i < shaders.size(); i++) {
		total_size += sizeof(ShaderHeader);
		total_size += shaders[i].code_compressed_bytes.size();
		total_size = aligned_to(total_size, alignment);
		total_size += _to_bytes_shader_extra_data(nullptr, i);
	}

	total_size += _to_bytes_footer_extra_data(nullptr);

	// Create the array that will hold all of the data.
	PackedByteArray bytes;
	bytes.resize_initialized(total_size);

	// Write out the data to the array.
	uint64_t bytes_offset = 0;
	uint8_t *bytes_ptr = bytes.ptrw();
	ContainerHeader &container_header = *(ContainerHeader *)(&bytes_ptr[bytes_offset]);
	container_header.magic_number = CONTAINER_MAGIC_NUMBER;
	container_header.version = CONTAINER_VERSION;
	container_header.format = _format();
	container_header.format_version = _format_version();
	container_header.shader_count = shaders.size();
	container_header.requested_compile_flags = static_cast<uint32_t>(compile_flags);
	bytes_offset += sizeof(ContainerHeader);
	bytes_offset += _to_bytes_header_extra_data(&bytes_ptr[bytes_offset]);

	memcpy(&bytes_ptr[bytes_offset], &reflection_data, sizeof(ReflectionData));
	bytes_offset += sizeof(ReflectionData);
	bytes_offset += _to_bytes_reflection_extra_data(&bytes_ptr[bytes_offset]);

	if (shader_name.size() > 0) {
		memcpy(&bytes_ptr[bytes_offset], shader_name.ptr(), reflection_data.shader_name_len);
		bytes_offset = aligned_to(bytes_offset + reflection_data.shader_name_len, alignment);
	}

	uint32_t uniform_index = 0;
	for (uint32_t uniform_count : reflection_binding_set_uniforms_count) {
		memcpy(&bytes_ptr[bytes_offset], &uniform_count, sizeof(uniform_count));
		bytes_offset += sizeof(uint32_t);

		for (uint32_t i = 0; i < uniform_count; i++) {
			memcpy(&bytes_ptr[bytes_offset], &reflection_binding_set_uniforms_data[uniform_index], sizeof(ReflectionBindingData));
			bytes_offset += sizeof(ReflectionBindingData);
			bytes_offset += _to_bytes_reflection_binding_uniform_extra_data(&bytes_ptr[bytes_offset], uniform_index);
			uniform_index++;
		}
	}

	for (uint32_t i = 0; i < reflection_specialization_data.size(); i++) {
		memcpy(&bytes_ptr[bytes_offset], &reflection_specialization_data.ptr()[i], sizeof(ReflectionSpecializationData));
		bytes_offset += sizeof(ReflectionSpecializationData);
		bytes_offset += _to_bytes_reflection_specialization_extra_data(&bytes_ptr[bytes_offset], i);
	}

	if (!reflection_shader_stages.is_empty()) {
		uint32_t stage_count = reflection_shader_stages.size();
		memcpy(&bytes_ptr[bytes_offset], reflection_shader_stages.ptr(), stage_count * sizeof(RDC::ShaderStage));
		bytes_offset += stage_count * sizeof(RDC::ShaderStage);
	}

	for (uint32_t i = 0; i < shaders.size(); i++) {
		const Shader &shader = shaders[i];
		ShaderHeader &header = *(ShaderHeader *)(&bytes.ptr()[bytes_offset]);
		header.shader_stage = shader.shader_stage;
		header.code_compressed_size = uint32_t(shader.code_compressed_bytes.size());
		header.code_compression_flags = shader.code_compression_flags;
		header.code_decompressed_size = shader.code_decompressed_size;
		bytes_offset += sizeof(ShaderHeader);
		memcpy(&bytes.ptrw()[bytes_offset], shader.code_compressed_bytes.ptr(), shader.code_compressed_bytes.size());
		bytes_offset = aligned_to(bytes_offset + shader.code_compressed_bytes.size(), alignment);
		bytes_offset += _to_bytes_shader_extra_data(&bytes_ptr[bytes_offset], i);
	}

	bytes_offset += _to_bytes_footer_extra_data(&bytes_ptr[bytes_offset]);

	ERR_FAIL_COND_V_MSG(bytes_offset != total_size, PackedByteArray(), "Amount of bytes written does not match the amount of bytes reserved for the container.");
#ifdef TESTS_ENABLED
	Dictionary record;
	record["shader_name"] = String::utf8(shader_name.get_data());
	record["phase"] = "container_encode";
	record["required_flags"] = static_cast<uint64_t>(compile_flags);
	ShaderCompileAudit::emit(record);
#endif
	return bytes;
}

bool RenderingShaderContainer::compress_code(const uint8_t *p_decompressed_bytes, uint32_t p_decompressed_size, uint8_t *p_compressed_bytes, uint32_t *r_compressed_size, uint32_t *r_compressed_flags) const {
	DEV_ASSERT(p_decompressed_bytes != nullptr);
	DEV_ASSERT(p_decompressed_size > 0);
	DEV_ASSERT(p_compressed_bytes != nullptr);
	DEV_ASSERT(r_compressed_size != nullptr);
	DEV_ASSERT(r_compressed_flags != nullptr);

	*r_compressed_flags = 0;

	PackedByteArray zstd_bytes;
	const int64_t zstd_max_bytes = Compression::get_max_compressed_buffer_size(p_decompressed_size, Compression::MODE_ZSTD);
	zstd_bytes.resize(zstd_max_bytes);

	const int64_t zstd_size = Compression::compress(zstd_bytes.ptrw(), p_decompressed_bytes, p_decompressed_size, Compression::MODE_ZSTD);
	if (zstd_size > 0 && (uint32_t)(zstd_size) < p_decompressed_size) {
		// Only choose Zstd if it results in actual compression.
		memcpy(p_compressed_bytes, zstd_bytes.ptr(), zstd_size);
		*r_compressed_size = zstd_size;
		*r_compressed_flags |= COMPRESSION_FLAG_ZSTD;
	} else {
		// Just copy the input to the output directly.
		memcpy(p_compressed_bytes, p_decompressed_bytes, p_decompressed_size);
		*r_compressed_size = p_decompressed_size;
	}

	return true;
}

bool RenderingShaderContainer::decompress_code(const uint8_t *p_compressed_bytes, uint32_t p_compressed_size, uint32_t p_compressed_flags, uint8_t *p_decompressed_bytes, uint32_t p_decompressed_size) const {
	DEV_ASSERT(p_compressed_bytes != nullptr);
	DEV_ASSERT(p_compressed_size > 0);
	DEV_ASSERT(p_decompressed_bytes != nullptr);
	DEV_ASSERT(p_decompressed_size > 0);

	bool uses_zstd = p_compressed_flags & COMPRESSION_FLAG_ZSTD;
	if (uses_zstd) {
		if (Compression::decompress(p_decompressed_bytes, p_decompressed_size, p_compressed_bytes, p_compressed_size, Compression::MODE_ZSTD) != int64_t(p_decompressed_size)) {
			ERR_FAIL_V_MSG(false, "Malformed zstd input for decompressing shader code.");
		}
	} else {
		ERR_FAIL_COND_V_MSG(p_compressed_size != p_decompressed_size, false, "Raw shader code size does not match the declared output size.");
		memcpy(p_decompressed_bytes, p_compressed_bytes, p_decompressed_size);
	}

	return true;
}

RenderingShaderContainer::RenderingShaderContainer() {}

RenderingShaderContainer::~RenderingShaderContainer() {}

#ifdef TESTS_ENABLED
#include "core/io/json.h"
#include "core/os/os.h"

#include <cstdio>

ShaderCompileAudit::ShaderCompileAudit(const String &p_path, OpenFile p_open_file, void *p_context) :
		path(p_path), open_file(p_open_file), context(p_context) {
	if (!open_file) {
		open_file = [](const String &p_name, FileAccess::ModeFlags p_mode, Error *r_error, void *) { return FileAccess::open(p_name, p_mode, r_error); };
	}
}

void ShaderCompileAudit::fail() {
	failed = true;
	ERR_PRINT("Shader compile audit failed; subsequent records are disabled.");
}

bool ShaderCompileAudit::has_failed() {
	MutexLock lock(mutex);
	return failed;
}

uint64_t ShaderCompileAudit::append(const Dictionary &p_record) {
	MutexLock lock(mutex);
	if (failed || path.is_empty()) {
		return 0;
	}
	const String name = p_record.get("shader_name", "");
	if (!name.begins_with("e06:")) {
		return 0;
	}
	if (name.utf8().length() > 256) {
		fail();
		return 0;
	}
	if (!initialized) {
		if (!path.is_absolute_path() || path.begins_with("res://") || path.begins_with("user://")) {
			fail();
			return 0;
		}
#ifdef WINDOWS_ENABLED
		FILE *reserved = _wfopen(reinterpret_cast<const wchar_t *>(path.utf16().get_data()), L"wbx");
#else
		FILE *reserved = fopen(path.utf8().get_data(), "wbx");
#endif
		if (!reserved) {
			fail();
			return 0;
		}
		if (fclose(reserved) != 0) {
			fail();
			return 0;
		}
		initialized = true;
	}
	Dictionary record = p_record.duplicate();
	record["schema_version"] = 1;
	record["seq"] = rows + 1;
	for (const String &field : { String("stage"), String("phase"), String("source_digest"), String("library_key"), String("required_flags"), String("effective_disable"), String("compile_path"), String("actual_options"), String("cache_hit") }) {
		if (!record.has(field)) {
			record[field] = Variant();
		}
	}
	CharString line = (JSON::stringify(record) + "\n").utf8();
	const uint64_t length = line.length();
	if (rows >= 4096 || length > 4194304 - bytes) {
		fail();
		return 0;
	}
	Error error = OK;
	Ref<FileAccess> writer = open_file(path, FileAccess::READ_WRITE, &error, context);
	if (error != OK || writer.is_null() || writer->get_length() != bytes) {
		fail();
		return 0;
	}
	writer->seek(bytes);
	if (!writer->store_buffer(reinterpret_cast<const uint8_t *>(line.get_data()), length)) {
		fail();
		return 0;
	}
	writer->flush();
	if (writer->get_error() != OK) {
		fail();
		return 0;
	}
	writer.unref();
	Ref<FileAccess> reader = open_file(path, FileAccess::READ, &error, context);
	if (error != OK || reader.is_null() || reader->get_length() != bytes + length) {
		fail();
		return 0;
	}
	reader->seek(bytes);
	PackedByteArray readback;
	readback.resize(length);
	if (reader->get_buffer(readback.ptrw(), length) != length || reader->get_error() != OK || memcmp(readback.ptr(), line.get_data(), length) != 0) {
		fail();
		return 0;
	}
	bytes += length;
	return ++rows;
}

uint64_t ShaderCompileAudit::emit(const Dictionary &p_record) {
	static ShaderCompileAudit sink(OS::get_singleton()->get_environment("GODOT_SHADER_COMPILE_AUDIT_FILE"));
	return sink.append(p_record);
}
#endif
