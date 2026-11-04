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

struct CurrentRecords
{
    uint words[1];
};

struct PreviousRecords
{
    uint words[1];
};

struct spvDescriptorSetBuffer0
{
    device CurrentRecords* __restrict current_records [[id(0)]];
    device PreviousRecords* __restrict previous_records [[id(1)]];
    device OutputMetadata* __restrict output_meta [[id(2)]];
    device SourceBacking* __restrict source [[id(3)]];
    device GuardScratch* __restrict scratch [[id(4)]];
    device GuardStatus* __restrict status [[id(5)]];
    device ProtectedCommands* __restrict commands [[id(6)]];
    device GeometryProof* __restrict proof [[id(7)]];
    constant uint* spvBufferSizeConstants [[id(8)]];
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
    bool _225 = params.surface_count == 0u;
    bool _232;
    if (!_225)
    {
        _232 = params.surface_count > 8u;
    }
    else
    {
        _232 = _225;
    }
    bool _240;
    if (!_232)
    {
        _240 = params.capacity > 1048576u;
    }
    else
    {
        _240 = _232;
    }
    bool _247;
    if (!_240)
    {
        _247 = params.reserved0 != 0u;
    }
    else
    {
        _247 = _240;
    }
    bool _255;
    if (!_247)
    {
        _255 = params.reserved1 != 0u;
    }
    else
    {
        _255 = _247;
    }
    bool _266;
    if (!_255)
    {
        _266 = uint(int((commandsBufferSize - 0) / 4)) < (params.surface_count * 5u);
    }
    else
    {
        _266 = _255;
    }
    bool _284;
    if (!_266)
    {
        _284 = uint(int((proofBufferSize - 224) / 144)) < params.surface_count;
    }
    else
    {
        _284 = _266;
    }
    if (_284)
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
        bool _329 = scratch.words[0] > params.capacity;
        bool _338;
        if (!_329)
        {
            _338 = scratch.words[4] != params.capacity;
        }
        else
        {
            _338 = _329;
        }
        if (_338)
        {
            initial_fault |= 2u;
        }
        bool _345 = scratch.words[1] != 0u;
        bool _352;
        if (!_345)
        {
            _352 = scratch.words[2] != 0u;
        }
        else
        {
            _352 = _345;
        }
        bool _359;
        if (!_352)
        {
            _359 = scratch.words[5] != 1u;
        }
        else
        {
            _359 = _352;
        }
        bool _368;
        if (!_359)
        {
            _368 = scratch.words[6] != params.sequence_low;
        }
        else
        {
            _368 = _359;
        }
        bool _377;
        if (!_368)
        {
            _377 = scratch.words[7] != params.sequence_high;
        }
        else
        {
            _377 = _368;
        }
        if (_377)
        {
            initial_fault |= 4u;
        }
    }
    bool _391 = ((device uint*)&proof.source_accessor)[0u] >= uint(int((sourceBufferSize - 0) / 4));
    bool _401;
    if (!_391)
    {
        _401 = ((device uint*)&proof.source_accessor)[1u] >= uint(int((sourceBufferSize - 0) / 4));
    }
    else
    {
        _401 = _391;
    }
    if (_401)
    {
        initial_fault |= 1u;
    }
    else
    {
        uint raw_validity = source.words[((device uint*)&proof.source_accessor)[1u]];
        uint rule = ((device uint*)&proof.source_accessor)[2u];
        bool _419 = (rule == 1u) && (raw_validity != 0u);
        bool _428;
        if (!_419)
        {
            _428 = (rule == 2u) && (raw_validity == 0u);
        }
        else
        {
            _428 = _419;
        }
        if (!_428)
        {
            initial_fault |= 1u;
        }
        if (source.words[((device uint*)&proof.source_accessor)[0u]] != scratch.words[0])
        {
            initial_fault |= 2u;
        }
    }
    bool _447 = ((device uint*)&proof.source_accessor)[3u] < 1u;
    bool _454;
    if (!_447)
    {
        _454 = ((device uint*)&proof.source_accessor)[3u] > 2u;
    }
    else
    {
        _454 = _447;
    }
    bool _461;
    if (!_454)
    {
        _461 = ((device uint*)&proof.options)[0u] > 1u;
    }
    else
    {
        _461 = _454;
    }
    bool _473;
    if (!_461)
    {
        _473 = any(proof.options.yzw != uint3(0u));
    }
    else
    {
        _473 = _461;
    }
    bool _488;
    if (!_473)
    {
        float4 param = proof.current_decl_min;
        float4 param_1 = proof.current_decl_max;
        bool param_2 = true;
        _488 = !bounds_shape(param, param_1, param_2);
    }
    else
    {
        _488 = _473;
    }
    bool _501;
    if (!_488)
    {
        float4 param_3 = proof.previous_decl_min;
        float4 param_4 = proof.previous_decl_max;
        bool param_5 = true;
        _501 = !bounds_shape(param_3, param_4, param_5);
    }
    else
    {
        _501 = _488;
    }
    bool _514;
    if (!_501)
    {
        float4 param_6 = proof.cull_min;
        float4 param_7 = proof.cull_max;
        bool param_8 = true;
        _514 = !bounds_shape(param_6, param_7, param_8);
    }
    else
    {
        _514 = _501;
    }
    if (_514)
    {
        initial_fault |= 4u;
    }
    for (uint row = 0u; row < 3u; row++)
    {
        float4 current_row = proof.current_host_inverse_rows[row];
        float4 previous_row = proof.previous_host_inverse_rows[row];
        float3 param_9 = current_row.xyz;
        bool _539 = !finite_vec(param_9);
        bool _548;
        if (!_539)
        {
            float param_10 = current_row.w;
            _548 = !finite_normal_or_zero(param_10);
        }
        else
        {
            _548 = _539;
        }
        bool _557;
        if (!_548)
        {
            float3 param_11 = previous_row.xyz;
            _557 = !finite_vec(param_11);
        }
        else
        {
            _557 = _548;
        }
        bool _566;
        if (!_557)
        {
            float param_12 = previous_row.w;
            _566 = !finite_normal_or_zero(param_12);
        }
        else
        {
            _566 = _557;
        }
        if (_566)
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
        bool _624 = !bounds_shape(param_13, param_14, param_15);
        bool _637;
        if (!_624)
        {
            float4 param_16 = bound.previous_mesh_min;
            float4 param_17 = bound.previous_mesh_max;
            bool param_18 = false;
            _637 = !bounds_shape(param_16, param_17, param_18);
        }
        else
        {
            _637 = _624;
        }
        bool _650;
        if (!_637)
        {
            float4 param_19 = bound.current_displacement_min;
            float4 param_20 = bound.current_displacement_max;
            bool param_21 = false;
            _650 = !bounds_shape(param_19, param_20, param_21);
        }
        else
        {
            _650 = _637;
        }
        bool _663;
        if (!_650)
        {
            float4 param_22 = bound.previous_displacement_min;
            float4 param_23 = bound.previous_displacement_max;
            bool param_24 = false;
            _663 = !bounds_shape(param_22, param_23, param_24);
        }
        else
        {
            _663 = _650;
        }
        bool _670;
        if (!_663)
        {
            _670 = bound.command.x > 1u;
        }
        else
        {
            _670 = _663;
        }
        bool _677;
        if (!_670)
        {
            _677 = bound.command.y == 0u;
        }
        else
        {
            _677 = _670;
        }
        bool _689;
        if (!_677)
        {
            _689 = any(bound.command.zw != uint2(0u));
        }
        else
        {
            _689 = _677;
        }
        if (_689)
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
    constant uint& spvDescriptorSet0_scratchBufferSize = spvDescriptorSet0.spvBufferSizeConstants[4];
    constant uint& spvDescriptorSet0_statusBufferSize = spvDescriptorSet0.spvBufferSizeConstants[5];
    constant uint& spvDescriptorSet0_commandsBufferSize = spvDescriptorSet0.spvBufferSizeConstants[6];
    constant uint& spvDescriptorSet0_proofBufferSize = spvDescriptorSet0.spvBufferSizeConstants[7];
    constant uint& spvDescriptorSet0_output_metaBufferSize = spvDescriptorSet0.spvBufferSizeConstants[2];
    constant uint& spvDescriptorSet0_sourceBufferSize = spvDescriptorSet0.spvBufferSizeConstants[3];
    bool _709 = uint(int((spvDescriptorSet0_scratchBufferSize - 0) / 4)) < 16u;
    bool _717;
    if (!_709)
    {
        _717 = uint(int((spvDescriptorSet0_statusBufferSize - 0) / 4)) < 8u;
    }
    else
    {
        _717 = _709;
    }
    if (_717)
    {
        return;
    }
    if (gl_GlobalInvocationID.x == 0u)
    {
        initialize_guard((*spvDescriptorSet0.scratch), spvDescriptorSet0_scratchBufferSize, (*spvDescriptorSet0.status), spvDescriptorSet0_statusBufferSize, params, (*spvDescriptorSet0.commands), spvDescriptorSet0_commandsBufferSize, (*spvDescriptorSet0.proof), spvDescriptorSet0_proofBufferSize, (*spvDescriptorSet0.output_meta), spvDescriptorSet0_output_metaBufferSize, (*spvDescriptorSet0.source), spvDescriptorSet0_sourceBufferSize);
    }
}

