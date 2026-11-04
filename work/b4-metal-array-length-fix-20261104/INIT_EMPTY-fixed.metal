#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wmissing-prototypes"

#include <metal_stdlib>
#include <simd/simd.h>
#include <metal_atomic>

using namespace metal;

struct GuardScratch
{
    uint words[1];
};

struct GuardStatus
{
    uint words[1];
};

struct Params
{
    uint capacity;
    uint sequence_low;
    uint sequence_high;
    uint surface_count;
    uint record_base;
    uint record_count;
    uint reserved0;
    uint reserved1;
};

struct ProtectedCommands
{
    uint words[1];
};

struct SurfaceProof
{
    float4 current_mesh_min;
    float4 current_mesh_max;
    float4 current_displacement_min;
    float4 current_displacement_max;
    float4 previous_mesh_min;
    float4 previous_mesh_max;
    float4 previous_displacement_min;
    float4 previous_displacement_max;
    uint4 command;
};

struct GeometryProof
{
    float4 current_decl_min;
    float4 current_decl_max;
    float4 previous_decl_min;
    float4 previous_decl_max;
    float4 cull_min;
    float4 cull_max;
    float4 current_host_inverse_rows[3];
    float4 previous_host_inverse_rows[3];
    uint4 source_accessor;
    uint4 options;
    SurfaceProof surfaces[1];
};

struct OutputMetadata
{
    uint words[1];
};

struct SourceBacking
{
    uint words[1];
};

struct SurfaceProof_1
{
    float4 current_mesh_min;
    float4 current_mesh_max;
    float4 current_displacement_min;
    float4 current_displacement_max;
    float4 previous_mesh_min;
    float4 previous_mesh_max;
    float4 previous_displacement_min;
    float4 previous_displacement_max;
    uint4 command;
};

constant uint3 gl_WorkGroupSize [[maybe_unused]] = uint3(64u, 1u, 1u);

struct spvDescriptorSetBuffer0
{
    device OutputMetadata* __restrict output_meta [[id(0)]];
    device SourceBacking* __restrict source [[id(1)]];
    device GuardScratch* __restrict scratch [[id(2)]];
    device GuardStatus* __restrict status [[id(3)]];
    device ProtectedCommands* __restrict commands [[id(4)]];
    device GeometryProof* __restrict proof [[id(5)]];
    constant uint* spvBufferSizeConstants [[id(6)]];
};

static inline __attribute__((always_inline))
bool finite_normal_or_zero(thread const float& value)
{
    uint magnitude = as_type<uint>(value) & 2147483647u;
    bool _61 = magnitude < 2139095040u;
    bool _70;
    if (_61)
    {
        _70 = (magnitude == 0u) || (magnitude >= 8388608u);
    }
    else
    {
        _70 = _61;
    }
    return _70;
}

static inline __attribute__((always_inline))
bool finite_vec(thread const float3& value)
{
    float param = value.x;
    bool _76 = finite_normal_or_zero(param);
    bool _83;
    if (_76)
    {
        float param_1 = value.y;
        _83 = finite_normal_or_zero(param_1);
    }
    else
    {
        _83 = _76;
    }
    bool _91;
    if (_83)
    {
        float param_2 = value.z;
        _91 = finite_normal_or_zero(param_2);
    }
    else
    {
        _91 = _83;
    }
    return _91;
}

static inline __attribute__((always_inline))
bool bounds_shape(thread const float4& lower, thread const float4& upper, thread const bool& strict)
{
    float3 param = lower.xyz;
    bool _97 = finite_vec(param);
    bool _104;
    if (_97)
    {
        float3 param_1 = upper.xyz;
        _104 = finite_vec(param_1);
    }
    else
    {
        _104 = _97;
    }
    bool _112;
    if (_104)
    {
        _112 = as_type<uint>(lower.w) == 0u;
    }
    else
    {
        _112 = _104;
    }
    bool _119;
    if (_112)
    {
        _119 = as_type<uint>(upper.w) == 0u;
    }
    else
    {
        _119 = _112;
    }
    bool _141;
    if (_119)
    {
        bool _123;
        if (strict)
        {
            _123 = all(lower.xyz < upper.xyz);
        }
        else
        {
            _123 = all(lower.xyz <= upper.xyz);
        }
        _141 = _123;
    }
    else
    {
        _141 = _119;
    }
    return _141;
}

static inline __attribute__((always_inline))
void fault(thread const uint& bits, thread const uint& ordinal, device GuardScratch& __restrict scratch, constant uint& scratchBufferSize)
{
    uint _49 = atomic_fetch_or_explicit((device atomic_uint*)&scratch.words[8], bits, memory_order_relaxed);
    uint _53 = atomic_fetch_min_explicit((device atomic_uint*)&scratch.words[9], ordinal, memory_order_relaxed);
}

static inline __attribute__((always_inline))
void initialize_guard(device GuardScratch& __restrict scratch, constant uint& scratchBufferSize, device GuardStatus& __restrict status, constant uint& statusBufferSize, constant Params& params, device ProtectedCommands& __restrict commands, constant uint& commandsBufferSize, device GeometryProof& __restrict proof, constant uint& proofBufferSize, device OutputMetadata& __restrict output_meta, constant uint& output_metaBufferSize, device SourceBacking& __restrict source, constant uint& sourceBufferSize)
{
    for (uint i = 0u; i < 16u; i++)
    {
        scratch.words[i] = 0u;
    }
    scratch.words[9] = 4294967295u;
    for (uint i_1 = 0u; i_1 < 8u; i_1++)
    {
        status.words[i_1] = 0u;
    }
    status.words[4] = params.sequence_low;
    status.words[5] = params.sequence_high;
    status.words[6] = 1u;
    uint i_2 = 0u;
    for (;;)
    {
        bool _205 = i_2 < (min(params.surface_count, 8u) * 5u);
        bool _217;
        if (_205)
        {
            _217 = i_2 < uint(int((commandsBufferSize - 0) / 4));
        }
        else
        {
            _217 = _205;
        }
        if (_217)
        {
            commands.words[i_2] = 0u;
            i_2++;
            continue;
        }
        else
        {
            break;
        }
    }
    uint initial_fault = 0u;
    if (params.capacity != 0u)
    {
        initial_fault |= 2u;
    }
    bool _232 = params.surface_count == 0u;
    bool _239;
    if (!_232)
    {
        _239 = params.surface_count > 8u;
    }
    else
    {
        _239 = _232;
    }
    bool _247;
    if (!_239)
    {
        _247 = params.capacity > 1048576u;
    }
    else
    {
        _247 = _239;
    }
    bool _254;
    if (!_247)
    {
        _254 = params.reserved0 != 0u;
    }
    else
    {
        _254 = _247;
    }
    bool _262;
    if (!_254)
    {
        _262 = params.reserved1 != 0u;
    }
    else
    {
        _262 = _254;
    }
    bool _273;
    if (!_262)
    {
        _273 = uint(int((commandsBufferSize - 0) / 4)) < (params.surface_count * 5u);
    }
    else
    {
        _273 = _262;
    }
    bool _291;
    if (!_273)
    {
        _291 = uint(int((proofBufferSize - 224) / 144)) < params.surface_count;
    }
    else
    {
        _291 = _273;
    }
    if (_291)
    {
        initial_fault |= 4u;
    }
    if (uint(int((output_metaBufferSize - 0) / 4)) < 8u)
    {
        initial_fault |= 4u;
    }
    else
    {
        for (uint i_3 = 0u; i_3 < 8u; i_3++)
        {
            scratch.words[i_3] = output_meta.words[i_3];
        }
        if (scratch.words[3] != 0u)
        {
            initial_fault |= 1u;
        }
        bool _336 = scratch.words[0] > params.capacity;
        bool _345;
        if (!_336)
        {
            _345 = scratch.words[4] != params.capacity;
        }
        else
        {
            _345 = _336;
        }
        if (_345)
        {
            initial_fault |= 2u;
        }
        bool _352 = scratch.words[1] != 0u;
        bool _359;
        if (!_352)
        {
            _359 = scratch.words[2] != 0u;
        }
        else
        {
            _359 = _352;
        }
        bool _366;
        if (!_359)
        {
            _366 = scratch.words[5] != 1u;
        }
        else
        {
            _366 = _359;
        }
        bool _375;
        if (!_366)
        {
            _375 = scratch.words[6] != params.sequence_low;
        }
        else
        {
            _375 = _366;
        }
        bool _384;
        if (!_375)
        {
            _384 = scratch.words[7] != params.sequence_high;
        }
        else
        {
            _384 = _375;
        }
        if (_384)
        {
            initial_fault |= 4u;
        }
    }
    bool _398 = ((device uint*)&proof.source_accessor)[0u] >= uint(int((sourceBufferSize - 0) / 4));
    bool _408;
    if (!_398)
    {
        _408 = ((device uint*)&proof.source_accessor)[1u] >= uint(int((sourceBufferSize - 0) / 4));
    }
    else
    {
        _408 = _398;
    }
    if (_408)
    {
        initial_fault |= 1u;
    }
    else
    {
        uint raw_validity = source.words[((device uint*)&proof.source_accessor)[1u]];
        uint rule = ((device uint*)&proof.source_accessor)[2u];
        bool _426 = (rule == 1u) && (raw_validity != 0u);
        bool _435;
        if (!_426)
        {
            _435 = (rule == 2u) && (raw_validity == 0u);
        }
        else
        {
            _435 = _426;
        }
        if (!_435)
        {
            initial_fault |= 1u;
        }
        if (source.words[((device uint*)&proof.source_accessor)[0u]] != scratch.words[0])
        {
            initial_fault |= 2u;
        }
    }
    bool _454 = ((device uint*)&proof.source_accessor)[3u] < 1u;
    bool _461;
    if (!_454)
    {
        _461 = ((device uint*)&proof.source_accessor)[3u] > 2u;
    }
    else
    {
        _461 = _454;
    }
    bool _468;
    if (!_461)
    {
        _468 = ((device uint*)&proof.options)[0u] > 1u;
    }
    else
    {
        _468 = _461;
    }
    bool _480;
    if (!_468)
    {
        _480 = any(proof.options.yzw != uint3(0u));
    }
    else
    {
        _480 = _468;
    }
    bool _495;
    if (!_480)
    {
        float4 param = proof.current_decl_min;
        float4 param_1 = proof.current_decl_max;
        bool param_2 = true;
        _495 = !bounds_shape(param, param_1, param_2);
    }
    else
    {
        _495 = _480;
    }
    bool _508;
    if (!_495)
    {
        float4 param_3 = proof.previous_decl_min;
        float4 param_4 = proof.previous_decl_max;
        bool param_5 = true;
        _508 = !bounds_shape(param_3, param_4, param_5);
    }
    else
    {
        _508 = _495;
    }
    bool _521;
    if (!_508)
    {
        float4 param_6 = proof.cull_min;
        float4 param_7 = proof.cull_max;
        bool param_8 = true;
        _521 = !bounds_shape(param_6, param_7, param_8);
    }
    else
    {
        _521 = _508;
    }
    if (_521)
    {
        initial_fault |= 4u;
    }
    for (uint row = 0u; row < 3u; row++)
    {
        float4 current_row = proof.current_host_inverse_rows[row];
        float4 previous_row = proof.previous_host_inverse_rows[row];
        float3 param_9 = current_row.xyz;
        bool _546 = !finite_vec(param_9);
        bool _555;
        if (!_546)
        {
            float param_10 = current_row.w;
            _555 = !finite_normal_or_zero(param_10);
        }
        else
        {
            _555 = _546;
        }
        bool _564;
        if (!_555)
        {
            float3 param_11 = previous_row.xyz;
            _564 = !finite_vec(param_11);
        }
        else
        {
            _564 = _555;
        }
        bool _573;
        if (!_564)
        {
            float param_12 = previous_row.w;
            _573 = !finite_normal_or_zero(param_12);
        }
        else
        {
            _573 = _564;
        }
        if (_573)
        {
            initial_fault |= 16u;
        }
    }
    SurfaceProof_1 bound;
    for (uint i_4 = 0u; i_4 < min(min(params.surface_count, 8u), uint(int((proofBufferSize - 224) / 144))); i_4++)
    {
        bound.current_mesh_min = proof.surfaces[i_4].current_mesh_min;
        bound.current_mesh_max = proof.surfaces[i_4].current_mesh_max;
        bound.current_displacement_min = proof.surfaces[i_4].current_displacement_min;
        bound.current_displacement_max = proof.surfaces[i_4].current_displacement_max;
        bound.previous_mesh_min = proof.surfaces[i_4].previous_mesh_min;
        bound.previous_mesh_max = proof.surfaces[i_4].previous_mesh_max;
        bound.previous_displacement_min = proof.surfaces[i_4].previous_displacement_min;
        bound.previous_displacement_max = proof.surfaces[i_4].previous_displacement_max;
        bound.command = proof.surfaces[i_4].command;
        float4 param_13 = bound.current_mesh_min;
        float4 param_14 = bound.current_mesh_max;
        bool param_15 = false;
        bool _631 = !bounds_shape(param_13, param_14, param_15);
        bool _644;
        if (!_631)
        {
            float4 param_16 = bound.previous_mesh_min;
            float4 param_17 = bound.previous_mesh_max;
            bool param_18 = false;
            _644 = !bounds_shape(param_16, param_17, param_18);
        }
        else
        {
            _644 = _631;
        }
        bool _657;
        if (!_644)
        {
            float4 param_19 = bound.current_displacement_min;
            float4 param_20 = bound.current_displacement_max;
            bool param_21 = false;
            _657 = !bounds_shape(param_19, param_20, param_21);
        }
        else
        {
            _657 = _644;
        }
        bool _670;
        if (!_657)
        {
            float4 param_22 = bound.previous_displacement_min;
            float4 param_23 = bound.previous_displacement_max;
            bool param_24 = false;
            _670 = !bounds_shape(param_22, param_23, param_24);
        }
        else
        {
            _670 = _657;
        }
        bool _677;
        if (!_670)
        {
            _677 = bound.command.x > 1u;
        }
        else
        {
            _677 = _670;
        }
        bool _684;
        if (!_677)
        {
            _684 = bound.command.y == 0u;
        }
        else
        {
            _684 = _677;
        }
        bool _696;
        if (!_684)
        {
            _696 = any(bound.command.zw != uint2(0u));
        }
        else
        {
            _696 = _684;
        }
        if (_696)
        {
            initial_fault |= 4u;
        }
    }
    scratch.words[10] = initial_fault;
    if (initial_fault != 0u)
    {
        uint param_25 = initial_fault;
        uint param_26 = 0u;
        fault(param_25, param_26, scratch, scratchBufferSize);
    }
}

kernel void main0(constant spvDescriptorSetBuffer0& spvDescriptorSet0 [[buffer(0)]], constant uint* spvBufferSizeConstants [[buffer(25)]], constant Params& params [[buffer(1)]], uint3 gl_GlobalInvocationID [[thread_position_in_grid]])
{
    constant uint& spvDescriptorSet0_scratchBufferSize = spvDescriptorSet0.spvBufferSizeConstants[2];
    constant uint& spvDescriptorSet0_statusBufferSize = spvDescriptorSet0.spvBufferSizeConstants[3];
    constant uint& spvDescriptorSet0_commandsBufferSize = spvDescriptorSet0.spvBufferSizeConstants[4];
    constant uint& spvDescriptorSet0_proofBufferSize = spvDescriptorSet0.spvBufferSizeConstants[5];
    constant uint& spvDescriptorSet0_output_metaBufferSize = spvDescriptorSet0.spvBufferSizeConstants[0];
    constant uint& spvDescriptorSet0_sourceBufferSize = spvDescriptorSet0.spvBufferSizeConstants[1];
    bool _716 = uint(int((spvDescriptorSet0_scratchBufferSize - 0) / 4)) < 16u;
    bool _724;
    if (!_716)
    {
        _724 = uint(int((spvDescriptorSet0_statusBufferSize - 0) / 4)) < 8u;
    }
    else
    {
        _724 = _716;
    }
    if (_724)
    {
        return;
    }
    if (gl_GlobalInvocationID.x == 0u)
    {
        initialize_guard((*spvDescriptorSet0.scratch), spvDescriptorSet0_scratchBufferSize, (*spvDescriptorSet0.status), spvDescriptorSet0_statusBufferSize, params, (*spvDescriptorSet0.commands), spvDescriptorSet0_commandsBufferSize, (*spvDescriptorSet0.proof), spvDescriptorSet0_proofBufferSize, (*spvDescriptorSet0.output_meta), spvDescriptorSet0_output_metaBufferSize, (*spvDescriptorSet0.source), spvDescriptorSet0_sourceBufferSize);
    }
}

