struct VS_OUTPUT
{
    float4 position : SV_position;
    [[vk::location(0)]] float4 colour : COLOR0;
    [[vk::location(1)]] float2 pointSize : POINTSZIE;
};

struct GS_OUTPUT
{
    float4 position : SV_position;
    [[vk::location(0)]] float4 colour : COLOR0;
};

[maxvertexcount(6)]
void main(point VS_OUTPUT input[1], inout TriangleStream<GS_OUTPUT> outStream)
{
    float2 pointSize = input[0].pointSize;
    float2 halfSize = pointSize * 0.5f;
    // 创建四边形的四个顶点
    float3 quad[6];
    quad[0] = input[0].position.xyz + float3(-halfSize.x, halfSize.y, 0);
    quad[1] = input[0].position.xyz + float3(halfSize.x, halfSize.y, 0);
    quad[2] = input[0].position.xyz + float3(halfSize.x, -halfSize.y, 0);

    quad[3] = input[0].position.xyz + float3(-halfSize.x, halfSize.y, 0);
    quad[4] = input[0].position.xyz + float3(halfSize.x, -halfSize.y, 0);
    quad[5] = input[0].position.xyz + float3(-halfSize.x, -halfSize.y, 0);

    // 输出两个三角形
    GS_OUTPUT output;
    for (int i = 0; i < 2; ++i)
    {
        output.position = float4(quad[i * 3 + 0], input[0].position.w);
        output.colour = input[0].colour;
        outStream.Append(output);

        output.position = float4(quad[i * 3 + 1], input[0].position.w);
        output.colour = input[0].colour;
        outStream.Append(output);

        output.position = float4(quad[i * 3 + 2], input[0].position.w);
        output.colour = input[0].colour;
        outStream.Append(output);

        outStream.RestartStrip();
    }
}