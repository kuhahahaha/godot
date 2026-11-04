#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct A
{
    uint x[1];
};

struct spvDescriptorSetBuffer0
{
    const device A* a [[id(0)]];
    constant void* _m1_pad [[id(1)]];
    constant uint* spvBufferSizeConstants [[id(2)]];
};

struct main0_out
{
    float4 gl_Position [[position]];
};

vertex main0_out main0(constant spvDescriptorSetBuffer0& spvDescriptorSet0 [[buffer(0)]], constant uint* spvBufferSizeConstants [[buffer(25)]])
{
    main0_out out = {};
    constant uint& spvDescriptorSet0_aBufferSize = spvDescriptorSet0.spvBufferSizeConstants[0];
    out.gl_Position = float4(float(int((spvDescriptorSet0_aBufferSize - 0) / 4)), 0.0, 0.0, 1.0);
    return out;
}

