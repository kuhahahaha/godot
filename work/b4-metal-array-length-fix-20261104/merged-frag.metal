#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct B
{
    uint x[1];
};

struct spvDescriptorSetBuffer0
{
    constant void* _m0_pad [[id(0)]];
    const device B* b [[id(1)]];
    constant uint* spvBufferSizeConstants [[id(2)]];
};

struct main0_out
{
    float4 color [[color(0)]];
};

fragment main0_out main0(constant spvDescriptorSetBuffer0& spvDescriptorSet0 [[buffer(0)]], constant uint* spvBufferSizeConstants [[buffer(25)]])
{
    main0_out out = {};
    constant uint& spvDescriptorSet0_bBufferSize = spvDescriptorSet0.spvBufferSizeConstants[1];
    out.color = float4(float(int((spvDescriptorSet0_bBufferSize - 0) / 4)));
    return out;
}

