#include "../inc/frame_constants.hlsl"

struct PsIn
{
    [[vk::location(0)]] float3 worldPos : POSITION0;
    [[vk::location(1)]] float3 ellipsoidCenter : POSITION1;
    [[vk::location(2)]] float3 ellipsoidScale : POSITION2;
    [[vk::location(3)]] float3 ellipsoidRotation0 : POSITION3;
    [[vk::location(4)]] float3 ellipsoidRotation1 : POSITION4;
    [[vk::location(5)]] float3 ellipsoidRotation2 : POSITION5;
    [[vk::location(6)]] float4 colour : POSITION6;
};

struct PsOut {
    float4 color : SV_TARGET0;
    float depth : SV_Depth;
};

[[vk::binding(0)]] cbuffer _ {
    float4 gs_translation;
    float4 gs_scaling;
    float4 gs_rotation;

    float stage;
    float3 rayOrigin;
};

float3 closestEllipsoidIntersection(float3 rayDirection, out float3 normal, PsIn ps) 
{
    float3x3 R = float3x3(
        ps.ellipsoidRotation0,
        ps.ellipsoidRotation1,
        ps.ellipsoidRotation2
    );
    // Convert ray to ellipsoid space
    float3 localRayOrigin = mul(R, (rayOrigin - ps.ellipsoidCenter));
    float3 localRayDirection = normalize(mul(R,rayDirection));

    float3 oneover = 1.0f / float3(ps.ellipsoidScale);

    // Compute coefficients of quadratic equation
    double a = dot(localRayDirection * oneover, localRayDirection * oneover);
    double b = 2.0 * dot(localRayDirection * oneover, localRayOrigin * oneover);
    double c = dot(localRayOrigin * oneover, localRayOrigin * oneover) - 1.0;

    // Compute discriminant
    double discriminant = b * b - 4.0 * a * c;

    // If discriminant is negative, there is no intersection
    if (discriminant < 0.0) {
        return float3(0.0);
    }

    // Compute two possible solutions for t
    float t1 = float((-b - sqrt(discriminant)) / (2.0 * a));
    float t2 = float((-b + sqrt(discriminant)) / (2.0 * a));

    // Take the smaller positive solution as the closest intersection
    float t = min(t1, t2);

    // Compute intersection point in ellipsoid space
    float3 localIntersection = float3(localRayOrigin + t * localRayDirection);

    // Compute normal vector in ellipsoid space
    float3 localNormal = normalize(localIntersection / ps.ellipsoidScale);

    // Convert normal vector to world space
    normal = normalize(mul(localNormal, R));

    // Convert intersection point back to world space
    float3 intersection =  mul(localIntersection,R) + ps.ellipsoidCenter;

    return intersection;
}

void main(PsIn ps) 
{
    PsOut ps_out;
    if (ps.colour.w <= 0)
        discard;
    float3 dir = normalize(ps.worldPos - rayOrigin);

    float3 normal;
    float3 intersection = closestEllipsoidIntersection(dir, normal,ps);
    float align = max(0.4, dot(-dir, normal));

    float4x4 mvp = transpose(frame_constants.view_constants.world_to_clip);

    ps_out.color = float4(1, 0, 0, 1);

    if (length(intersection) == 0)
        discard;

    float4 newPos = mul(float4(intersection, 1), mvp);
    newPos /= newPos.w;

    ps_out.depth = newPos.z;

    float a = stage == 0 ? 1.0 : 0.05f;

    ps_out.color = float4(align * ps.colour.xyz, a);
}
