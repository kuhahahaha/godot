#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct O
{
    uint x[1];
};

constant uint3 gl_WorkGroupSize [[maybe_unused]] = uint3(1u);

struct spvDescriptorSetBuffer0
{
    array<texture2d<float>, 2> tex [[id(0)]];
    array<sampler, 2> texSmplr [[id(2)]];
    device O* o [[id(4)]];
    constant uint* spvBufferSizeConstants [[id(5)]];
};

kernel void main0(constant spvDescriptorSetBuffer0& spvDescriptorSet0 [[buffer(0)]], constant uint* spvBufferSizeConstants [[buffer(25)]])
{
    constant uint& spvDescriptorSet0_oBufferSize = spvDescriptorSet0.spvBufferSizeConstants[4];
    (*spvDescriptorSet0.o).x[0] = uint(int((spvDescriptorSet0_oBufferSize - 0) / 4)) + uint(spvDescriptorSet0.tex[1].sample(spvDescriptorSet0.texSmplr[1], float2(0.25), level(0.0)).x);
}

