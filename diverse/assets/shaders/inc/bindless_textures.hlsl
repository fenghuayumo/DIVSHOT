#ifndef BINDLESS_TEXTURES_HLSL
#define BINDLESS_TEXTURES_HLSL

[[vk::binding(6, 1)]] Texture2D bindless_textures[];
[[vk::binding(7, 1)]] StructuredBuffer<float4> bindless_texture_sizes;
// Pre-integrated FG texture for the GGX BRDF
static const uint BINDLESS_LUT_BRDF_FG = 0;

static const uint BINDLESS_LUT_BLUE_NOISE_256_LDR_RGBA_0 = 1;

static const uint BINDLESS_LUT_BEZOLD_BRUCKE = 2;

#endif
