#include "../inc/frame_constants.hlsl"
#include "../inc/color/srgb.hlsl"
#include "../inc/bindless.hlsl"
#include "../inc/pack_unpack.hlsl"
#include "gaussian_common.hlsl"

struct PsIn
{
    [[vk::location(0)]] float2 uv : TEXCOORD0;
};

struct PsOut {
    float4 color : SV_TARGET0;
};

[[vk::binding(0)]] cbuffer _ {
    float4 color;
    uint width;
    uint height;
    float alphaCutoff;
    uint pad;
};
[[vk::binding(1)]] Texture2D<float4> outlineTexture;

PsOut outline_ps(PsIn ps)
{
    PsOut ps_out;
    int2 texel = int2(ps.uv * int2(width, height));
    // skip solid pixels
    float4 outlineColor = outlineTexture[texel];
    if (outlineColor.a > alphaCutoff) {
        discard;
    }

    for (int x = -2; x <= 2; x++) {
        for (int y = -2; y <= 2; y++) {
            if ((x != 0) && (y != 0) && outlineTexture[texel + int2(x, y)].a > alphaCutoff) {
                ps_out.color = float4(color.xyz, outlineColor.a);
                return ps_out;
            }
        }
    }

    discard;

    return ps_out;
}