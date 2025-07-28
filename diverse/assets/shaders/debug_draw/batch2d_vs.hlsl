#include "../inc/frame_constants.hlsl"
struct VS_INPUT
{
    float3 position : POSITION0;
    float4 uv : TEXCOORD0;
    float2 tid : TEXCOORD1;
    float4 color : COLOR0;
};

struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    [[vk::location(0)]] float3 positionVS : POSITION0;
    [[vk::location(1)]] float4 color : COLOR0;
    [[vk::location(2)]] float2 tid : TEXCOORD0;
    [[vk::location(3)]] float2 uv : TEXCOORD1;
};



VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;

    output.position = mul(float4(input.position, 1.0), frame_constants.view_constants.world_to_clip);
    output.positionVS = input.position;
    output.color = input.color;
    output.tid = input.tid;
    output.uv = input.uv.xy;

    return output;
}