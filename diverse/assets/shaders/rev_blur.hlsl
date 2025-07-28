#include "inc/samplers.hlsl"

[[vk::binding(0)]] Texture2D<float4> input_tail_tex;
[[vk::binding(1)]] Texture2D<float4> input_tex;
[[vk::binding(2)]] RWTexture2D<float4> output_tex;
[[vk::binding(3)]] cbuffer _ {
    uint output_extent_x;
    uint output_extent_y;
    float self_weight;
    float padding;
};

[numthreads(8, 8, 1)]
void main(in uint2 px: SV_DispatchThreadID)
{
    // float4 pyramid_col = input_tail_tex.SampleLevel(,)
    if (px.x >= output_extent_x || px.y >= output_extent_y)
        return;
    float4 pyramid_col = input_tail_tex[px];
    float2 inv_size = float2(1.0, 1.0) / float2(output_extent_x, output_extent_y);
    float4 self_col = 0;
    if (true) {
        const int k = 1;
        for (int y = -k; y <= k; y++) {
            for (int x = -k; x <= k; x++) {
                float2 uv = (float2(px.x, px.y) + 0.5 + float2(x, y)) * inv_size;
                float4 sample = input_tex.SampleLevel(sampler_lnc, uv, 0);
                self_col += sample;
            }
        }
        self_col /= ((2 * k + 1) * (2 * k + 1)) ;
    }
    else {
        float2 uv = (float2(px.x, px.y) + 0.5) * inv_size;
        self_col = input_tex.SampleLevel(sampler_lnc, uv, 0);
    }
    float exponential_falloff = 0.6;
    output_tex[px] = lerp(self_col, pyramid_col, self_weight * exponential_falloff);
}