
#include "../inc/frame_constants.hlsl"
struct PS_Out
{
    float4 color : SV_TARGET0;
    float depth : SV_Depth;
};

[[vk::binding(0)]] cbuffer _ {
    float4 u_CameraPos;
    float4 u_CameraForward;
    float u_Near;
    float u_Far;
    float u_MaxDistance;
    float p1;
};

struct PS_In
{
    float4 fragCoord : SV_Position;
    [[vk::location(0)]] float3 fragPosition : POSITION;
    [[vk::location(1)]] float2 fragTexCoord : TEXCOORD0;
    [[vk::location(2)]] float3 nearPoint : TEXCOORD1;
    [[vk::location(3)]] float3 farPoint : TEXCOORD2;
    [[vk::location(4)]] float2 inTexCoord : TEXCOORD3;
    [[vk::location(5)]] float3 inNormal : NORMAL;
    [[vk::location(6)]] float3 inTangent : TANGENT;
    [[vk::location(7)]] float3 inBitangent : BITANGENT;
    [[vk::location(8)]] float4 color : COLOR;
};

static const float step = 100.0f;
static const float subdivisions = 10.0f;

// Based on https://gist.github.com/bgolus/d49651f52b1dcf82f70421ba922ed064
float PristineGrid(float2 uv, float2 lineWidth)
{
    float2 ddxUV = ddx(uv);
    float2 ddyUV = ddy(uv);
    float4 uvDDXY = float4(ddxUV, ddyUV);
    float2 uvDeriv = float2(length(uvDDXY.xz), length(uvDDXY.yw));
    bool2 invertLine = lineWidth < float2(0.5, 0.5);
    float2 targetWidth = lerp(1.0 - lineWidth, lineWidth, invertLine);
    float2 drawWidth = clamp(targetWidth, uvDeriv, float2(0.5, 0.5));
    float2 lineAA = uvDeriv * 1.5;
    float2 gridUV = abs(frac(uv) * 2.0 - 1.0);
    gridUV = lerp(gridUV, 1.0 - gridUV, invertLine);
    float2 grid2 = smoothstep(drawWidth + lineAA, drawWidth - lineAA, gridUV);
    grid2 *= saturate(targetWidth / drawWidth);
    grid2 = lerp(grid2, targetWidth, saturate(uvDeriv * 2.0 - 1.0));
    grid2 = lerp(1.0 - grid2, grid2, invertLine);
    return lerp(grid2.x, 1.0, grid2.y);
}

 float4 PristineGrid2(float4 uv, float2 lineWidth)
 {
     float4 uvDDXY = float4(ddx(uv.xy), ddy(uv.xy));
     float2 uvDeriv = float2(length(uvDDXY.xz), length(uvDDXY.yw));
     float axisLineWidth = max(lineWidth.x, 0.08);
     float2 axisDrawWidth = max(float2(axisLineWidth, axisLineWidth), uvDeriv);
     float2 axisLineAA = uvDeriv * 1.5;
     float2 axisLines2 = smoothstep(axisDrawWidth + axisLineAA, axisDrawWidth - axisLineAA, abs(uv.zw * 2.0));
     axisLines2 *= saturate(axisLineWidth / axisDrawWidth);

     float div = max(2.0, round(lineWidth.x));
     float2 majorUVDeriv = uvDeriv / div;
     float majorLineWidth = lineWidth.x / div;
     float2 majorDrawWidth = clamp(majorLineWidth, majorUVDeriv.x, 0.5);
     float2 majorLineAA = majorUVDeriv * 1.5;
     float2 majorGridUV = abs(frac(uv.xy / div) * 2.0 - 1.0);
     majorGridUV = float2(1.0 - majorGridUV.x, 1.0 - majorGridUV.y);
     float2 majorAxisOffset = (1.0 - saturate(abs(uv.xy / div * 2.0))) * 2.0;
     majorGridUV += majorAxisOffset; // adjust UVs so center axis line is skipped
     float2 majorGrid2 = smoothstep(majorDrawWidth + majorLineAA, majorDrawWidth - majorLineAA, majorGridUV);
     majorGrid2 *= saturate(majorLineWidth / majorDrawWidth);
     majorGrid2 = saturate(majorGrid2 - axisLines2); // hack
     majorGrid2 = lerp(majorGrid2, float2(majorLineWidth, majorLineWidth), saturate(majorUVDeriv * 2.0 - 1.0));
     float minorLineWidth = min(lineWidth.x / 2.0, lineWidth.x);
     bool minorInvertLine = minorLineWidth > 0.5;
     float minorTargetWidth = minorInvertLine ? 1.0 - minorLineWidth : minorLineWidth;
     float2 minorDrawWidth = clamp(float2(minorTargetWidth, 0.0), uvDeriv, float2(0.5, 0.5));
     float2 minorLineAA = uvDeriv * 1.5;
     float2 minorGridUV = abs(frac(uv.xy) * 2.0 - 1.0);
     minorGridUV = minorInvertLine ? minorGridUV : 1.0 - minorGridUV;
     float2 minorMajorOffset = (1.0 - saturate((1.0 - abs(frac(uv.zw / div) * 2.0 - 1.0)) * div)) * 2.0;
     minorGridUV += minorMajorOffset; // adjust UVs so major division lines are skipped
     float2 minorGrid2 = smoothstep(minorDrawWidth + minorLineAA, minorDrawWidth - minorLineAA, minorGridUV);
     minorGrid2 *= saturate(minorTargetWidth / minorDrawWidth);
     minorGrid2 = saturate(minorGrid2 - axisLines2); // hack
     minorGrid2 = minorInvertLine ? 1.0 - minorGrid2 : minorGrid2;
     minorGrid2 = abs(uv.zw).x > 0.5 ? minorGrid2 : 0.0;
     float minorGrid = lerp(minorGrid2.x, 1.0, minorGrid2.y);
     float majorGrid = lerp(majorGrid2.x, 1.0, majorGrid2.y);
     float2 axisDashUV = abs(frac((uv.zw + axisLineWidth * 0.5) * 1.3) * 2.0 - 1.0) - 0.5;
     float2 axisDashDeriv = uvDeriv * 1.3 * 1.5;
     float2 axisDash = smoothstep(-axisDashDeriv, axisDashDeriv, axisDashUV);
     axisDash = uv.z < 0.0 ? axisDash : 1.0;
     float3 aAxisColor = lerp(float3(1.0, 0.0, 0.0), float3(1.0, 0.0, 1.0), axisDash.y);
     float3 bAxisColor = lerp(float3(1.0, 0.0, 0.0), float3(1.0, 1.0, 0.0), axisDash.x);
     float3 centerColor = float3(1.0, 0.0, 0.0);
     aAxisColor = lerp(aAxisColor, centerColor, axisLines2.y);
     float4 axisLines = float4(lerp(bAxisColor * axisLines2.y, aAxisColor, axisLines2.x), 1.0);
     float4 minorLineColor = float4(1.0, 0.0, 0.0, 0.5);
     float4 majorLineColor = float4(1.0, 1.0, 0.0, 0.5);
     float4 baseColor = float4(1.0, 0.0, 0.0, 0.5);
     float4 col = lerp(baseColor, minorLineColor, minorGrid * minorLineColor.a);
     col = lerp(col, majorLineColor, majorGrid * majorLineColor.a);
     col = col * (1.0 - axisLines.a) + axisLines;
     return col;
}

float4 grid(float3 fragPos3D, float scale, bool drawAxis ) 
{
    float2 coord = fragPos3D.xz * scale;
	
    float2 derivative = fwidth(coord);
    float2 gridf = abs(frac(coord - 0.5) - 0.5) / derivative;
    float line_c = min(gridf.x, gridf.y);
    float minimumz = min(derivative.y, 1);
    float minimumx = min(derivative.x, 1);
    float4 colour = float4(0.35, 0.35, 0.35, 1.0 - min(line_c, 1.0));
    // z axis
    if(fragPos3D.x > -0.1 * minimumx && fragPos3D.x < 0.1 * minimumx)
        colour = float4(0.0, 0.0, 1.0, colour.w);
    // x axis
    if(fragPos3D.z > -0.1 * minimumz && fragPos3D.z < 0.1 * minimumz)
        colour = float4(1.0, 0.0, 0.0, colour.w);
    return colour;
}

float computeDepth(float3 pos, float4x4 fragView, float4x4 fragProj) {
    float4 clip_space_pos = mul(mul(float4(pos.xyz, 1.0), fragView), fragProj);
    return (clip_space_pos.z / clip_space_pos.w);
}

float computeLinearDepth(float3 pos, float4x4 fragView, float4x4 fragProj) {
    float4 clip_space_pos = mul(mul(float4(pos.xyz, 1.0), fragView), fragProj);
    float clip_space_depth = (clip_space_pos.z / clip_space_pos.w) * 2.0 - 1.0; // put back between -1 and 1
    float linearDepth = (2.0 * u_Near * u_Far) / (u_Far + u_Near - clip_space_depth * (u_Far - u_Near)); // get linear value between 0.01 and 100
    return linearDepth / u_Far; // normalize
}

int roundToPowerOfTen(float n)
{
    return int(pow(10.0, floor( (1 / log(10)) * log(n))));
}

PS_Out main(PS_In ps_in)
{
    float t = -ps_in.nearPoint.y / (ps_in.farPoint.y - ps_in.nearPoint.y);
    if (t < 0.)
        discard;
    PS_Out ps_out;
    float3 fragPos3D = ps_in.nearPoint + t * (ps_in.farPoint - ps_in.nearPoint);

    float4x4 fragView = transpose(frame_constants.view_constants.world_to_view);
    float4x4 fragProj = transpose(frame_constants.view_constants.view_to_clip);
    ps_out.depth = computeDepth(fragPos3D, fragView, fragProj);

    float linearDepth = computeLinearDepth(fragPos3D, fragView, fragProj);
    float fading = max(0, (0.5 - linearDepth));
	
    float decreaseDistance = u_Far * 1.5;
    float3 pseudoViewPos = float3(u_CameraPos.x, fragPos3D.y, u_CameraPos.z);
	
    float dist, angleFade;
    /* if persp */
    if (fragProj[3][3] == 0.0) 
    {
        float3 viewvec = u_CameraPos.xyz - fragPos3D;
        dist = length(viewvec);
        viewvec /= dist;
        
        float angle;
        angle = viewvec.y;
        angle = 1.0 - abs(angle);
        angle *= angle;
        angleFade= 1.0 - angle * angle;
        angleFade*= 1.0 - smoothstep(0.0, u_MaxDistance, dist - u_MaxDistance);
    }
    else {
        dist = ps_in.fragCoord.z * 2.0 - 1.0;
        /* Avoid fading in +Z direction in camera view (see T70193). */
        dist = clamp(dist, 0.0, 1.0);// abs(dist);
        angleFade = 1.0 - smoothstep(0.0, 0.5, dist - 0.5);
        dist = 1.0; /* avoid branch after */
        
    }
	
    float distanceToCamera = abs(u_CameraPos.y - fragPos3D.y);
    int powerOfTen = roundToPowerOfTen(distanceToCamera);
    powerOfTen = max(1, powerOfTen);
    float divs = 1.0 / float(powerOfTen);
    float secondFade = smoothstep(1 / divs, 1 / divs * 10, distanceToCamera);

    // float4 grid1 = grid(fragPos3D, divs, true);
    // float4 grid2 = grid(fragPos3D, divs, true);

    // //TODO: Fade second grid in distance
    // secondFade*= smoothstep(u_Far / 0.5, u_Far, distanceToCamera);
    // grid2.a *= secondFade;
    // grid1.a *= (1 - secondFade);

    // ps_out.color = grid1 + grid2; // adding multiple resolution for the grid
    // ps_out.color.a = max(grid1.a, grid2.a);

    float2 uv = fragPos3D.xz * divs;
    float grid1 = PristineGrid(uv, float2(0.01,0.01) * (1 - secondFade));
    uv = fragPos3D.xz * (divs / 10.0);
    float grid2 = PristineGrid(uv, float2(0.01, 0.01) * secondFade);
	
    grid2 *= secondFade;
    grid1 *= (1 - secondFade);

    ps_out.color = float4(float3(0.45, 0.45, 0.45), grid1 + grid2);
    ps_out.color.w = max(grid1, grid2);
    ps_out.color *= float(t > 0);
    ps_out.color.a *= angleFade;

    return ps_out;
}