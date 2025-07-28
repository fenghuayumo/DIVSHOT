[[vk::binding(0)]] Texture2D<float4> current_tex;
[[vk::binding(1)]] Texture2D<float4> prev_tex;
[[vk::binding(2)]] RWTexture2D<float4> output_tex;

[numthreads(8, 8, 1)]
void main(uint2 px: SV_DispatchThreadID, uint2 px_within_group: SV_GroupThreadID, uint2 group_id: SV_GroupID) {
    float4 prev = prev_tex[px];
    float4 cur = current_tex[px];

    float tsc = 1 + prev.w;
    float lrp = 1 / max(1.0, tsc);
    cur.rgb /= max(1.0, cur.w);
    float3 output = max(0.0.xxx, lerp(prev.rgb, cur.rgb, lrp));
    output_tex[px] = float4(output, max(1, tsc));
}