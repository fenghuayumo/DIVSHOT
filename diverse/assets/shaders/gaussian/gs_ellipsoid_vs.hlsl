#include "../inc/frame_constants.hlsl"
#include "../inc/bindless.hlsl"
#include "gaussian_common.hlsl"

[[vk::binding(0)]] cbuffer _ {
    float4 gs_translation;
    float4 gs_scaling;
    float4 gs_rotation;

    float stage;
    float3 rayOrigin;
};
[[vk::binding(1)]] StructuredBuffer<Gaussian> gaussians_buffer;
[[vk::binding(2)]] StructuredBuffer<GaussianColor> gaussians_color;

struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    [[vk::location(0)]] float3 worldPos : POSITION0;
    [[vk::location(1)]] float3 ellipsoidCenter : POSITION1;
    [[vk::location(2)]] float3 ellipsoidScale : POSITION2;
    [[vk::location(3)]] float3 ellipsoidRotation0 : POSITION3;
    [[vk::location(4)]] float3 ellipsoidRotation1 : POSITION4;
    [[vk::location(5)]] float3 ellipsoidRotation2 : POSITION5;
    [[vk::location(6)]] float4 colour : POSITION6;
};

float3x3 quatTofloat3x3(float4 q) {
  float qx = q.y;
  float qy = q.z;
  float qz = q.w;
  float qw = q.x;

  float qxx = qx * qx;
  float qyy = qy * qy;
  float qzz = qz * qz;
  float qxz = qx * qz;
  float qxy = qx * qy;
  float qyw = qy * qw;
  float qzw = qz * qw;
  float qyz = qy * qz;
  float qxw = qx * qw;

  return float3x3(
    float3(1.0 - 2.0 * (qyy + qzz), 2.0 * (qxy - qzw), 2.0 * (qxz + qyw)),
    float3(2.0 * (qxy + qzw), 1.0 - 2.0 * (qxx + qzz), 2.0 * (qyz - qxw)),
    float3(2.0 * (qxz - qyw), 2.0 * (qyz + qxw), 1.0 - 2.0 * (qxx + qyy))
  );
}

const float3 boxVertices[8] = float3[8](
    float3(-1, -1, -1),
    float3(-1, -1,  1),
    float3(-1,  1, -1),
    float3(-1,  1,  1),
    float3( 1, -1, -1),
    float3( 1, -1,  1),
    float3( 1,  1, -1),
    float3( 1,  1,  1)
);

const int boxIndices[36] = int[36](
    0, 1, 2, 1, 3, 2,
    4, 6, 5, 5, 6, 7,
    0, 2, 4, 4, 2, 6,
    1, 5, 3, 5, 7, 3,
    0, 4, 1, 4, 5, 1,
    2, 3, 6, 3, 7, 6
);

static const float minAlpha = 0.2;//1.0 / 255.0;
static const float k0 = 0.282094791773878f;

float3x3 quatToRotation(float4 q)
{
    float r = q.x;
    float x = q.y;
    float y = q.z;
    float z = q.w;
    float3x3 R = float3x3(
        1.f - 2.f * (y * y + z * z), 2.f * (x * y - r * z), 2.f * (x * z + r * y),
        2.f * (x * y + r * z), 1.f - 2.f * (x * x + z * z), 2.f * (y * z - r * x),
        2.f * (x * z - r * y), 2.f * (y * z + r * x), 1.f - 2.f * (x * x + y * y)
	);

    return R;
}

VS_OUTPUT main(uint vertexID: SV_VertexID, uint instanceID: SV_InstanceID)
{
    VS_OUTPUT output;
    Gaussian gaussian = gaussians_buffer[instanceID];
    float4 rotation_scale = gaussian.rotation_scale;
    float4 scale_opacity = unpack_half4(rotation_scale.zw);

    // rotation matrix
    float4 q = unpack_half4(rotation_scale.xy);
    float3 s = scale_opacity.xyz * gs_scaling.xyz * saturate(gs_scaling.w);

    float3 ellipsoidScale = scale_opacity.xyz;
    ellipsoidScale = 2 * ellipsoidScale;

    float3x3 ellipsoidRotation = quatToRotation(q);
    int vertexIndex = boxIndices[vertexID];
    float3 world_pos = mul((ellipsoidScale * boxVertices[vertexIndex]), ellipsoidRotation);
    world_pos += gaussian.position.xyz;

    float4x4 o2w = float4x4(
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        gs_translation.x, gs_translation.y, gs_translation.z, 1
        );
    float4x4 scale = float4x4(
        gs_scaling.x, 0, 0, 0,
        0, gs_scaling.y, 0, 0,
        0, 0, gs_scaling.z, 0,
        0, 0, 0, 1
        );
    // get rotation transform form gs_rotation quaternion
    float4x4 gs_rot_mat = float4x4(
        1 - 2 * gs_rotation.y * gs_rotation.y - 2 * gs_rotation.z * gs_rotation.z, 2 * gs_rotation.x * gs_rotation.y - 2 * gs_rotation.z * gs_rotation.w, 2 * gs_rotation.x * gs_rotation.z + 2 * gs_rotation.y * gs_rotation.w, 0,
        2 * gs_rotation.x * gs_rotation.y + 2 * gs_rotation.z * gs_rotation.w, 1 - 2 * gs_rotation.x * gs_rotation.x - 2 * gs_rotation.z * gs_rotation.z, 2 * gs_rotation.y * gs_rotation.z - 2 * gs_rotation.x * gs_rotation.w, 0,
        2 * gs_rotation.x * gs_rotation.z - 2 * gs_rotation.y * gs_rotation.w, 2 * gs_rotation.y * gs_rotation.z + 2 * gs_rotation.x * gs_rotation.w, 1 - 2 * gs_rotation.x * gs_rotation.x - 2 * gs_rotation.y * gs_rotation.y, 0,
        0, 0, 0, 1
        );
    o2w = mul(gs_rot_mat, o2w);
    o2w = mul(scale, o2w);

    float4 trans_world_pos = mul(float4(world_pos, 1.0), o2w);
    float4x4 view = frame_constants.view_constants.world_to_view;
    float4x4 proj = frame_constants.view_constants.view_to_clip;
    float3 p_view = mul(trans_world_pos, view).xyz;
    // perspective projection
    float4 p_hom = mul(float4(p_view, 1.0), proj);
    float p_w = 1.0f / (p_hom.w + 0.0000001f);
    float4 p_proj = p_hom * p_w;

    float4 harmonics_0 = gaussians_color[instanceID].harmonics[0];
    harmonics_0 = unpack_half4(harmonics_0.xy);
    float3 color = harmonics_0.xyz * k0;

    output.colour = float4(color, 1);
    if ((stage == 0 && scale_opacity.w < minAlpha) || (stage == 1 && scale_opacity.w >= minAlpha))
        output.position = float4(0, 0, 0, 0);
    else
        output.position = p_proj;
}