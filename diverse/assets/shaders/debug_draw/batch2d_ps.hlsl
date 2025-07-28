#include "../inc/image.hlsl"
#include "../inc/samplers.hlsl"

struct DATA
{
    [[vk::location(0)]] float3 position : POSITION0;
    [[vk::location(1)]] float4 colour : COLOR0;
    [[vk::location(2)]] float2 tid : TEXCOORD0;
    [[vk::location(3)]] float2 uv : TEXCOORD1;
};

struct Ps_Out
{
    float4 colour : SV_Target0;
};

[[vk::binding(0)]] Texture2D<float4> textures[16];

#define GAMMA 2.2
#define DISABLE_GAMMA_CORRECTION 0

float4 GammaCorrectTexture(float4 samp)
{
#if DISABLE_GAMMA_CORRECTION
    return samp;
#endif
    return float4(pow(samp.rgb, float3(GAMMA)), samp.a);
}

float4 texture(Texture2D<float4> tex,float2 uv)
{
    return tex.SampleLevel(sampler_llr, uv, 0);
}

Ps_Out main(DATA fs_in)
{
    float4 texColor = fs_in.colour;
    if (fs_in.tid.x > 0.0)
    {

        switch (int(fs_in.tid.x - 0.5))
        {
        // tex.SampleLevel(linearSampler, float2(texPos0.x, texPos12.y), 0.0f)
        case 0: texColor *= GammaCorrectTexture(texture(textures[0], fs_in.uv)); break;
        case 1: texColor *= GammaCorrectTexture(texture(textures[1], fs_in.uv)); break;
        case 2: texColor *= GammaCorrectTexture(texture(textures[2], fs_in.uv)); break;
        case 3: texColor *= GammaCorrectTexture(texture(textures[3], fs_in.uv)); break;
        case 4: texColor *= GammaCorrectTexture(texture(textures[4], fs_in.uv)); break;
        case 5: texColor *= GammaCorrectTexture(texture(textures[5], fs_in.uv)); break;
        case 6: texColor *= GammaCorrectTexture(texture(textures[6], fs_in.uv)); break;
        case 7: texColor *= GammaCorrectTexture(texture(textures[7], fs_in.uv)); break;
        case 8: texColor *= GammaCorrectTexture(texture(textures[8], fs_in.uv)); break;
        case 9: texColor *= GammaCorrectTexture(texture(textures[9], fs_in.uv)); break;
        case 10: texColor *= GammaCorrectTexture(texture(textures[10], fs_in.uv)); break;
        case 11: texColor *= GammaCorrectTexture(texture(textures[11], fs_in.uv)); break;
        case 12: texColor *= GammaCorrectTexture(texture(textures[12], fs_in.uv)); break;
        case 13: texColor *= GammaCorrectTexture(texture(textures[13], fs_in.uv)); break;
        case 14: texColor *= GammaCorrectTexture(texture(textures[14], fs_in.uv)); break;
        case 15: texColor *= GammaCorrectTexture(texture(textures[15], fs_in.uv)); break;
        }
    }
    Ps_Out ps_out;
    ps_out.colour = texColor;
    return ps_out;
}
