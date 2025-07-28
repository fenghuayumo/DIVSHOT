#ifndef MONTE_CARLO_HLSL_
#define MONTE_CARLO_HLSL_

#include "math_const.hlsl"
#include "math.hlsl"
// Based on : [Clarberg 2008, "Fast Equal-Area Mapping of the (Hemi)Sphere using SIMD"]
// Fixed sign bit for UV.y == 0 and removed branch before division by using a small epsilon
// https://fileadmin.cs.lth.se/graphics/research/papers/2008/simdmapping/clarberg_simdmapping08_preprint.pdf
float3 equi_area_spherical_mapping(float2 UV)
{
    UV = 2 * UV - 1;
    float D = 1 - (abs(UV.x) + abs(UV.y));
    float R = 1 - abs(D);
    float Epsilon = 5.42101086243e-20; // 2^-64 (this avoids 0/0 without changing the rest of the mapping)
    float Phi = (M_PI / 4) * ((abs(UV.y) - abs(UV.x)) / (R + Epsilon) + 1);
    float F = R * sqrt(2 - R * R);
    return float3(
        F * sign(UV.x) * abs(cos(Phi)),
        F * sign(UV.y) * abs(sin(Phi)),
        sign(D) * (1 - R * R)
  );
}

// Based on: [Clarberg 2008, "Fast Equal-Area Mapping of the (Hemi)Sphere using SIMD"]
// Removed branch before division by using a small epsilon
// https://fileadmin.cs.lth.se/graphics/research/papers/2008/simdmapping/clarberg_simdmapping08_preprint.pdf
float2 inverse_equi_area_spherical_mapping(float3 Direction)
{
    float3 AbsDir = abs(Direction);
    float R = sqrt(1 - AbsDir.z);
    float Epsilon = 5.42101086243e-20; // 2^-64 (this avoids 0/0 without changing the rest of the mapping)
    float x = min(AbsDir.x, AbsDir.y) / (max(AbsDir.x, AbsDir.y) + Epsilon);

    // Coefficients for 6th degree minimax approximation of atan(x)*2/pi, x=[0,1].
    const float t1 = 0.406758566246788489601959989e-5f;
    const float t2 = 0.636226545274016134946890922156f;
    const float t3 = 0.61572017898280213493197203466e-2f;
    const float t4 = -0.247333733281268944196501420480f;
    const float t5 = 0.881770664775316294736387951347e-1f;
    const float t6 = 0.419038818029165735901852432784e-1f;
    const float t7 = -0.251390972343483509333252996350e-1f;

    // Polynomial approximation of atan(x)*2/pi
    float Phi = t6 + t7 * x;
    Phi = t5 + Phi * x;
    Phi = t4 + Phi * x;
    Phi = t3 + Phi * x;
    Phi = t2 + Phi * x;
    Phi = t1 + Phi * x;

    Phi = (AbsDir.x < AbsDir.y) ? 1 - Phi : Phi;
    float2 UV = float2(R - Phi * R, Phi * R);
    UV = (Direction.z < 0) ? 1 - UV.yx : UV;
    UV = asfloat(asuint(UV) ^ (asuint(Direction.xy) & 0x80000000u));
    return UV * 0.5 + 0.5;
}

float2 uniform_sample_disk(float2 E)
{
    float Theta = 2 * M_PI * E.x;
    float Radius = sqrt(E.y);
    return Radius * float2(cos(Theta), sin(Theta));
}

// Returns a point on the unit circle and a radius in z
float3 concentric_disk_sampling_helper(float2 E)
{
    float2 p = 2 * E - 1;
    float2 a = abs(p);
    float Lo = min(a.x, a.y);
    float Hi = max(a.x, a.y);
    float Epsilon = 5.42101086243e-20; // 2^-64 (this avoids 0/0 without changing the rest of the mapping)
    float Phi = (M_PI / 4) * (Lo / (Hi + Epsilon) + 2 * float(a.y >= a.x));
    float Radius = Hi;
    // copy sign bits from p
    const uint SignMask = 0x80000000;
    float2 Disk = asfloat((asuint(float2(cos(Phi), sin(Phi))) & ~SignMask) | (asuint(p) & SignMask));
    // return point on the circle as well as the radius
    return float3(Disk, Radius);
}

float3 uniform_sample_cone(float2 urand, float cos_theta_max) {
    float cos_theta = (1.0 - urand.x) + urand.x * cos_theta_max;
    float sin_theta = sqrt(saturate(1.0 - cos_theta * cos_theta));
    float phi = urand.y * M_TAU;
    return float3(sin_theta * cos(phi), sin_theta * sin(phi), cos_theta);
}

float3 uniform_sample_hemisphere(float2 urand) {
    float phi = urand.y * M_TAU;
    float cos_theta = 1.0 - urand.x;
    float sin_theta = sqrt(1.0 - cos_theta * cos_theta);
    return float3(cos(phi) * sin_theta, sin(phi) * sin_theta, cos_theta);
}

float3 uniform_sample_sphere(float2 E)
{
    float Phi = 2 * M_PI * E.x;
    float CosTheta = 1 - 2 * E.y;
    float SinTheta = sqrt(1 - CosTheta * CosTheta);

    float3 H;
    H.x = SinTheta * cos(Phi);
    H.y = SinTheta * sin(Phi);
    H.z = CosTheta;

    return H;
}

float3 sample_sphere(float2 rand, out float solidAnglePdf)
{
    rand.y = rand.y * 2.0 - 1.0;

    float2 tangential = uniform_sample_disk(float2(rand.x, 1.0 - square(rand.y)));
    float elevation = rand.y;

    solidAnglePdf = 0.25f / M_PI;

    return float3(tangential.xy, elevation);
}

// For converting an area measure pdf to solid angle measure pdf
float pdfAtoW(float pdfA, float distance_, float cosTheta)
{
    return pdfA * square(distance_) / cosTheta;
}

// PDF = 1 / (4 * PI)
float4 uniform_sample_sphere_pdf(float2 E)
{
    float Phi = 2 * M_PI * E.x;
    float CosTheta = 1 - 2 * E.y;
    float SinTheta = sqrt(1 - CosTheta * CosTheta);

    float3 H;
    H.x = SinTheta * cos(Phi);
    H.y = SinTheta * sin(Phi);
    H.z = CosTheta;

    float PDF = 1.0 / (4 * M_PI);

    return float4(H, PDF);
}

// PDF = 1 / (2 * PI)
float4 uniform_sample_hemi_sphere_pdf(float2 E)
{
    float Phi = 2 * M_PI * E.x;
    float CosTheta = E.y;
    float SinTheta = sqrt(1 - CosTheta * CosTheta);

    float3 H;
    H.x = SinTheta * cos(Phi);
    H.y = SinTheta * sin(Phi);
    H.z = CosTheta;

    float PDF = 1.0 / (2 * M_PI);

    return float4(H, PDF);
}

// PDF = NoL / PI
float4 cosine_sample_hemi_sphere_pdf(float2 E)
{
    float Phi = 2 * M_PI * E.x;
    float CosTheta = sqrt(E.y);
    float SinTheta = sqrt(1 - CosTheta * CosTheta);

    float3 H;
    H.x = SinTheta * cos(Phi);
    H.y = SinTheta * sin(Phi);
    H.z = CosTheta;

    float PDF = CosTheta * (1.0 / M_PI);

    return float4(H, PDF);
}

// PDF = NoL / PI
float4 cosine_sample_hemi_sphere_concentric_pdf(float2 E)
{
    float3 Result = concentric_disk_sampling_helper(E);
    float SinTheta = Result.z;
    float CosTheta = sqrt(1 - SinTheta * SinTheta);
    return float4(Result.xy * SinTheta, CosTheta, CosTheta * (1.0 / M_PI));
}

// PDF = NoL / PI
float4 cosine_sample_hemi_sphere(float2 E, float3 N)
{
    float3 H = uniform_sample_sphere(E).xyz;
    H = normalize(N + H);

    float PDF = dot(H, N) * (1.0 / M_PI);

    return float4(H, PDF);
}

float4 uniform_sample_cone_pdf(float2 E, float CosThetaMax)
{
    float Phi = 2 * M_PI * E.x;
    float CosTheta = lerp(CosThetaMax, 1, E.y);
    float SinTheta = sqrt(1 - CosTheta * CosTheta);

    float3 L;
    L.x = SinTheta * cos(Phi);
    L.y = SinTheta * sin(Phi);
    L.z = CosTheta;

    float PDF = 1.0 / (2 * M_PI * (1 - CosThetaMax));

    return float4(L, PDF);
}

// Same as the function above, but uses SinThetaMax^2 as the parameter
// so that the solid angle can be computed more accurately for very small angles
// The caller is expected to ensure that SinThetaMax2 is <= 1
float4 uniform_sample_cone_robust(float2 E, float SinThetaMax2)
{
    float Phi = 2 * M_PI * E.x;
    // The expression 1-sqrt(1-x) is susceptible to catastrophic cancelation.
    // Instead, use a series expansion about 0 which is accurate within 10^-7
    // and much more numerically stable.
    float OneMinusCosThetaMax = SinThetaMax2 < 0.01 ? SinThetaMax2 * (0.5 + 0.125 * SinThetaMax2) : 1 - sqrt(1 - SinThetaMax2);

    float CosTheta = 1 - OneMinusCosThetaMax * E.y;
    float SinTheta = sqrt(1 - CosTheta * CosTheta);

    float3 L;
    L.x = SinTheta * cos(Phi);
    L.y = SinTheta * sin(Phi);
    L.z = CosTheta;
    float PDF = 1.0 / (2 * M_PI * OneMinusCosThetaMax);

    return float4(L, PDF);
}

float uniform_cone_solid_angle(float SinThetaMax2)
{
    float OneMinusCosThetaMax = SinThetaMax2 < 0.01 ? SinThetaMax2 * (0.5 + 0.125 * SinThetaMax2) : 1 - sqrt(1 - SinThetaMax2);
    return 2 * M_PI * OneMinusCosThetaMax;
}

// Same as the function above, but uses a concentric mapping
float4 uniform_sample_cone_concentric_robust(float2 E, float SinThetaMax2)
{
    // The expression 1-sqrt(1-x) is susceptible to catastrophic cancelation.
    // Instead, use a series expansion about 0 which is accurate within 10^-7
    // and much more numerically stable.
    float OneMinusCosThetaMax = SinThetaMax2 < 0.01 ? SinThetaMax2 * (0.5 + 0.125 * SinThetaMax2) : 1 - sqrt(1 - SinThetaMax2);
    float3 Result = concentric_disk_sampling_helper(E);
    float SinTheta = Result.z * sqrt(SinThetaMax2);
    float CosTheta = sqrt(1 - SinTheta * SinTheta);

    float3 L = float3(Result.xy * SinTheta, CosTheta);
    float PDF = 1.0 / (2 * M_PI * OneMinusCosThetaMax);

    return float4(L, PDF);
}

float3 uniform_sample_triangle(float2 rndSample)
{
    float sqrtx = sqrt(rndSample.x);

    return float3(
        1 - sqrtx,
        sqrtx * (1 - rndSample.y),
        sqrtx * rndSample.y);
}

float3 uniform_sample_cos_hemi_sphere(float2 random, out float solidAnglePdf)
{
    float2 tangential = uniform_sample_disk(random);
    float elevation = sqrt(saturate(1.0 - random.y));

    solidAnglePdf = elevation / M_PI;

    return float3(tangential.xy, elevation);
}

#endif