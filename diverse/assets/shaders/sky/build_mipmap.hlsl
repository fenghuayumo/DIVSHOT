#include "../inc/binding.hlsl"

DS_RESOURCE(0) RWTexture2D<float> dst_mip_tex;
DS_RESOURCE(1) Texture2D<float> src_mip_tex;

DS_CBUFFER(2) cbuffer _
{
    uint2 dst_mip_size;
    uint2 src_mip_size;
};

[numthreads(8, 8, 1)]
void main(uint3 px : SV_DispatchThreadID)
{
    if (any(px.xy >= dst_mip_size))
        return;

    uint2 src_base = px.xy * 2;
    float weight = 0.0;

    if (all(src_base + uint2(0, 0) < src_mip_size))
        weight += src_mip_tex.Load(int3(src_base + uint2(0, 0), 0));
    if (all(src_base + uint2(1, 0) < src_mip_size))
        weight += src_mip_tex.Load(int3(src_base + uint2(1, 0), 0));
    if (all(src_base + uint2(0, 1) < src_mip_size))
        weight += src_mip_tex.Load(int3(src_base + uint2(0, 1), 0));
    if (all(src_base + uint2(1, 1) < src_mip_size))
        weight += src_mip_tex.Load(int3(src_base + uint2(1, 1), 0));

    // Keep the same padded-square average convention used by the RTXDI PDF code.
    dst_mip_tex[px.xy] = weight * 0.25;
}
