#pragma clang diagnostic ignored "-Wincompatible-pointer-types-discards-qualifiers"

#include <metal_stdlib>
#include <simd/simd.h>

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

constant uint3 gl_WorkGroupSize [[maybe_unused]] = uint3(64u, 1u, 1u);

struct CurrentRecords
{
    uint words[1];
};

struct PreviousRecords
{
    uint words[1];
};

struct OutputMetadata
{
    uint words[1];
};

struct SourceBacking
{
    uint words[1];
};

struct spvDescriptorSetBuffer0
{
    constant uint* spvBufferSizeConstants [[id(0)]];
    // Overlapping binding: device CurrentRecords* __restrict current_records [[id(0)]];
    device PreviousRecords* __restrict previous_records [[id(1)]];
    device OutputMetadata* __restrict output_meta [[id(2)]];
    device SourceBacking* __restrict source [[id(3)]];
    device GuardScratch* __restrict scratch [[id(4)]];
    device GuardStatus* __restrict status [[id(5)]];
    device ProtectedCommands* __restrict commands [[id(6)]];
    device GeometryProof* __restrict proof [[id(7)]];
};

kernel void main0(constant spvDescriptorSetBuffer0& spvDescriptorSet0 [[buffer(0)]], constant uint* spvBufferSizeConstants [[buffer(25)]], constant Params& params [[buffer(1)]], uint3 gl_GlobalInvocationID [[thread_position_in_grid]])
{
    constant uint& spvDescriptorSet0_scratchBufferSize = spvDescriptorSet0.spvBufferSizeConstants[4];
    constant uint& spvDescriptorSet0_statusBufferSize = spvDescriptorSet0.spvBufferSizeConstants[5];
    device auto &current_records = *reinterpret_cast<device CurrentRecords* __restrict constant &>(spvDescriptorSet0.spvBufferSizeConstants);
    bool _17 = uint(int((spvDescriptorSet0_scratchBufferSize - 0) / 4)) < 16u;
    bool _30;
    if (!_17)
    {
        _30 = uint(int((spvDescriptorSet0_statusBufferSize - 0) / 4)) < 8u;
    }
    else
    {
        _30 = _17;
    }
    if (_30)
    {
        return;
    }
    if (gl_GlobalInvocationID.x != 0u)
    {
        return;
    }
    uint bits = (*spvDescriptorSet0.scratch).words[8];
    (*spvDescriptorSet0.status).words[0] = bits;
    uint _57;
    if (bits == 0u)
    {
        _57 = (*spvDescriptorSet0.scratch).words[0];
    }
    else
    {
        _57 = 0u;
    }
    (*spvDescriptorSet0.status).words[1] = _57;
    (*spvDescriptorSet0.status).words[2] = 0u;
    (*spvDescriptorSet0.status).words[3] = 0u;
    bool _70 = bits == 0u;
    bool _76;
    if (_70)
    {
        _76 = (*spvDescriptorSet0.scratch).words[0] != 0u;
    }
    else
    {
        _76 = _70;
    }
    if (_76)
    {
        for (uint i = 0u; i < params.surface_count; i++)
        {
            (*spvDescriptorSet0.commands).words[i * 5u] = ((device uint*)&(*spvDescriptorSet0.proof).surfaces[i].command)[1u];
            (*spvDescriptorSet0.commands).words[(i * 5u) + 1u] = (*spvDescriptorSet0.scratch).words[0];
        }
    }
}

