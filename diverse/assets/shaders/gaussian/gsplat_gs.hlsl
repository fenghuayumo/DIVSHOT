
struct VS_OUTPUT
{
    float4 position : SV_position;
    [[vk::location(0)]] float4 colour : COLOR0;
#ifdef AABB
    [[vk::location(1)]] float4 cov2d : COLOR1;
#else
    [[vk::location(1)]] float2 basicVector0 : TANGENT;
    [[vk::location(2)]] float2 basicVector1 : BITANGENT;
#endif
};

struct GS_OUTPUT
{
    float4 position : SV_position;
    [[vk::location(0)]] nointerpolation float4 colour : COLOR0;
#ifdef AABB
    [[vk::location(1)]] nointerpolation float3 cov2d : COLOR1;
    [[vk::location(2)]] float2 pix_direction : COLOR2;
#else
    [[vk::location(1)]] float2 vPosition : COLOR3;
#endif
};

[[vk::binding(0)]] cbuffer _ {
    float4 gs_translation;
    float4 gs_scaling;
    float4 gs_rotation;

    float point_size;
    uint surface_width;
    uint surface_height;
    uint padding;
};
static const float stddev = sqrt(8);
static const float minAlpha = 1.0 / 255.0;

float2 computeNDCOffset(float2 basisVector0, float2 basisVector1, float2 vPosition)
{
    float2 screen_pixel_size = 1.0 / float2(surface_width, surface_height);
    return (vPosition.x * basisVector0 + vPosition.y * basisVector1) * screen_pixel_size * 2.0;
}

float4 get_bounding_box(float2 direction,float radius_px)
{
    float2 screen_pixel_size = 1.0 / float2(surface_width, surface_height);
    float2 radius_ndc = screen_pixel_size * radius_px * 2;
    return float4(
        radius_ndc * direction,
        radius_px * direction
    );
}

[maxvertexcount(4)]
void main(point VS_OUTPUT input[1], inout TriangleStream<GS_OUTPUT> outStream)
{
    if (input[0].colour.a < minAlpha) return;
    float4 ndcCenter = input[0].position;

    float2 vPosition[4];
    vPosition[0] = float2(-1, -1);
    vPosition[1] = float2(-1, 1);
    vPosition[2] = float2(1, -1);
    vPosition[3] = float2(1, 1);

    // float2 basicVector0 = input[0].basicVector0;
    // float2 basicVector1 = input[0].basicVector1;
#ifdef AABB
    float4 cov2d = input[0].cov2d;
#endif
    GS_OUTPUT output;
    for (int i = 0; i < 4; ++i)
    {
#ifdef AABB
        float4 bb = get_bounding_box(vPosition[i], cov2d.w);
        output.pix_direction = bb.zw;
        output.cov2d = cov2d.xyz;
#else
        float2 bb = computeNDCOffset(input[0].basicVector0, input[0].basicVector1, vPosition[i]);
        output.vPosition = vPosition[i] * stddev;
#endif
        output.position = float4(ndcCenter.xy + bb.xy, ndcCenter.zw);
        output.colour = input[0].colour;
        outStream.Append(output);
    }
    outStream.RestartStrip();
}