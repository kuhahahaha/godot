#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct O
{
    uint x[1];
};

struct A
{
    uint4 prefix;
    uint x[1];
};

struct B
{
    float4 x[1];
};

constant uint3 gl_WorkGroupSize [[maybe_unused]] = uint3(1u);

struct spvDescriptorSetBuffer0
{
    device A* a [[id(0)]];
    constant uint* spvBufferSizeConstants [[id(1)]];
};

struct spvDescriptorSetBuffer1
{
    device B* b [[id(0)]];
    device O* o [[id(1)]];
    constant uint* spvBufferSizeConstants [[id(2)]];
};

kernel void main0(constant spvDescriptorSetBuffer0& spvDescriptorSet0 [[buffer(0)]], constant spvDescriptorSetBuffer1& spvDescriptorSet1 [[buffer(1)]], constant uint* spvBufferSizeConstants [[buffer(25)]])
{
    constant uint& spvDescriptorSet1_oBufferSize = spvDescriptorSet1.spvBufferSizeConstants[1];
    constant uint& spvDescriptorSet0_aBufferSize = spvDescriptorSet0.spvBufferSizeConstants[0];
    constant uint& spvDescriptorSet1_bBufferSize = spvDescriptorSet1.spvBufferSizeConstants[0];
    (*spvDescriptorSet1.o).x[0] = uint(int((spvDescriptorSet0_aBufferSize - 16) / 4));
    (*spvDescriptorSet1.o).x[1] = uint(int((spvDescriptorSet1_bBufferSize - 0) / 16));
    (*spvDescriptorSet1.o).x[2] = uint(int((spvDescriptorSet1_oBufferSize - 0) / 4));
    (*spvDescriptorSet1.o).x[3] = (*spvDescriptorSet0.a).x[0] + uint(((device float*)&(*spvDescriptorSet1.b).x[0])[0u]);
}

