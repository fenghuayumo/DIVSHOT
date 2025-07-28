#include "../inc/frame_constants.hlsl"

struct VS_INPUT
{
    [[vk::location(0)]] float3 position : POSITION0;
    [[vk::location(1)]] float4 colour : COLOR0;
};

struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    [[vk::location(0)]] float4 colour : COLOR0;
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    output.position = mul(float4(input.position, 1.0), frame_constants.view_constants.world_to_clip);
    // output.positionVS = input.position;
    output.colour = input.colour;
    return output;
}