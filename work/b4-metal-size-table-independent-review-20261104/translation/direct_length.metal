#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct O
{
    uint x[1];
};

constant uint3 gl_WorkGroupSize [[maybe_unused]] = uint3(1u);

kernel void main0(constant uint* spvBufferSizeConstants [[buffer(25)]], device O& o [[buffer(4)]], array<texture2d<float>, 2> tex [[texture(0)]], array<sampler, 2> texSmplr [[sampler(2)]])
{
    constant uint& oBufferSize = spvBufferSizeConstants[4];
    o.x[0] = uint(int((oBufferSize - 0) / 4)) + uint(tex[1].sample(texSmplr[1], float2(0.25), level(0.0)).x);
}

