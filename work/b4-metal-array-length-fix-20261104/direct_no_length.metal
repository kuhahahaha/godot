#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct O
{
    uint x[1];
};

constant uint3 gl_WorkGroupSize [[maybe_unused]] = uint3(1u);

kernel void main0(device O& o [[buffer(4)]], array<texture2d<float>, 2> tex [[texture(0)]], array<sampler, 2> texSmplr [[sampler(2)]])
{
    o.x[0] = 3u + uint(tex[1].sample(texSmplr[1], float2(0.25), level(0.0)).x);
}

