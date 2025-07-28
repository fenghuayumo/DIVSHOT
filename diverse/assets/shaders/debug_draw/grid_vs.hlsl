#include "../inc/frame_constants.hlsl"

struct VS_INPUT
{
    [[vk::location(0)]] float3 inPosition : POSITION;
    [[vk::location(1)]] float4 inColour : COLOR;
    [[vk::location(2)]] float2 inTexCoord : TEXCOORD;
    [[vk::location(3)]] float3 inNormal : NORMAL;
    [[vk::location(4)]] float3 inTangent : TANGENT;
    [[vk::location(5)]] float3 inBitangent : BITANGENT;
};

struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    [[vk::location(0)]] float3 fragPosition : POSITION;
    [[vk::location(1)]] float2 fragTexCoord : TEXCOORD0;
    [[vk::location(2)]] float3 nearPoint : TEXCOORD1;
    [[vk::location(3)]] float3 farPoint : TEXCOORD2;
    [[vk::location(4)]] float2 inTexCoord : TEXCOORD3;
    [[vk::location(5)]] float3 inNormal : NORMAL;
    [[vk::location(6)]] float3 inTangent : TANGENT;
    [[vk::location(7)]] float3 inBitangent : BITANGENT;
    [[vk::location(8)]] float4 color : COLOR;
    // [[vk::location(4)]] float4x4 fragView : TEXCOORD3;
    // [[vk::location(8)]] float4x4 fragProj : TEXCOORD4;
};

float3 gridPlane[6] = {
    float3(1, 1, 0), float3(-1, -1, 0), float3(-1, 1, 0),
    float3(-1, -1, 0), float3(1, 1, 0), float3(1, -1, 0)
};

float3 UnprojectPoint(float x, float y, float z, float4x4 viewInv, float4x4 projInv)
{
	float4 unprojectedPoint = mul(mul(float4(x, y, z, 1.0), viewInv), projInv);
	return unprojectedPoint.xyz / unprojectedPoint.w;
}

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;

    float4 position = float4(input.inPosition, 1.0); // * u_MVP;
    // output.position = mul(position, transpose(frame_constants.view_constants.world_to_clip));
	output.fragPosition = input.inPosition;

    output.color = input.inColour; //SPV vertex layout incorrect when inColour not used
	output.fragTexCoord = input.inTexCoord;

	float4x4 viewInv = frame_constants.view_constants.view_to_world;
	float4x4 projInv = frame_constants.view_constants.clip_to_view;
    float3 p = position.xyz; // gridPlane[gl_VertexIndex].xyz;
    output.nearPoint = UnprojectPoint(p.x, p.y, 0.0, viewInv, projInv);
    output.farPoint = UnprojectPoint(p.x, p.y, 1.0, viewInv, projInv);
	output.position = float4(p.xyz, 1.0);

    output.inBitangent = input.inBitangent;
    output.inNormal = input.inNormal;
    output.inTexCoord = input.inTexCoord;
    output.inTangent = input.inTangent;

	return output;
}