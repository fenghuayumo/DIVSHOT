#ifndef MATERIALS_MICROFACET_HLSL
#define MATERIALS_MICROFACET_HLSL

#include "../inc/math.hlsl"

static const float MATERIAL_MIN_GGX_ALPHA = 0.0064;

float material_ggx_alpha_from_roughness(float roughness)
{
    return max(roughness * roughness, MATERIAL_MIN_GGX_ALPHA);
}

float eval_ndf_ggx(float alpha, float cos_theta)
{
    float a2 = alpha * alpha;
    float d = ((cos_theta * a2 - cos_theta) * cos_theta + 1.0);
    return a2 / max(d * d * M_PI, 1e-8);
}

float eval_g1_ggx(float alpha_sqr, float cos_theta)
{
    if (cos_theta <= 0.0)
        return 0.0;

    float cos_theta_sqr = cos_theta * cos_theta;
    float tan_theta_sqr = max(1.0 - cos_theta_sqr, 0.0) / max(cos_theta_sqr, 1e-8);
    return 2.0 / (1.0 + sqrt(1.0 + alpha_sqr * tan_theta_sqr));
}

float eval_lambda_ggx(float alpha_sqr, float cos_theta)
{
    if (cos_theta <= 0.0)
        return 0.0;

    float cos_theta_sqr = cos_theta * cos_theta;
    float tan_theta_sqr = max(1.0 - cos_theta_sqr, 0.0) / max(cos_theta_sqr, 1e-8);
    return 0.5 * (-1.0 + sqrt(1.0 + alpha_sqr * tan_theta_sqr));
}

float eval_masking_smith_ggx_correlated(float alpha, float cos_theta_i, float cos_theta_o)
{
    float alpha_sqr = alpha * alpha;
    float lambda_i = eval_lambda_ggx(alpha_sqr, cos_theta_i);
    float lambda_o = eval_lambda_ggx(alpha_sqr, cos_theta_o);
    return 1.0 / (1.0 + lambda_i + lambda_o);
}

float eval_pdf_ggx_vndf(float alpha, float3 wi, float3 h)
{
    float g1 = eval_g1_ggx(alpha * alpha, wi.z);
    float d = eval_ndf_ggx(alpha, h.z);
    return g1 * d / max(4.0 * wi.z, 1e-8);
}

float eval_pdf_ggx_bvndf(float alpha, float3 wi, float3 h)
{
    float2 alpha2 = alpha.xx;
    float d = eval_ndf_ggx(alpha, h.z);
    float2 ai = alpha2 * wi.xy;
    float len2 = dot(ai, ai);
    float t = sqrt(len2 + wi.z * wi.z);

    float a = saturate(min(alpha2.x, alpha2.y));
    float s = 1.0 + length(wi.xy);
    float a2 = a * a;
    float s2 = s * s;
    float k = (1.0 - a2) * s2 / max(s2 + a2 * wi.z * wi.z, 1e-8);
    return d / max(2.0 * (k * wi.z + t), 1e-8);
}

float3 sample_ggx_vndf(float alpha, float3 wi, float2 u)
{
    float alpha_x = alpha;
    float alpha_y = alpha;

    float3 vh = normalize(float3(alpha_x * wi.x, alpha_y * wi.y, wi.z));
    float lensq = vh.x * vh.x + vh.y * vh.y;
    float3 t1 = lensq > 0.0002 ? float3(-vh.y, vh.x, 0.0) * rsqrt(lensq) : float3(1.0, 0.0, 0.0);
    float3 t2 = cross(vh, t1);

    float r = sqrt(u.x);
    float phi = M_TAU * u.y;
    float p1 = r * cos(phi);
    float p2 = r * sin(phi);
    float s = 0.5 * (1.0 + vh.z);
    p2 = (1.0 - s) * sqrt(max(0.0, 1.0 - p1 * p1)) + s * p2;

    float3 nh = p1 * t1 + p2 * t2 + sqrt(max(0.0, 1.0 - p1 * p1 - p2 * p2)) * vh;
    return normalize(float3(alpha_x * nh.x, alpha_y * nh.y, max(0.0, nh.z)));
}

float3 sample_ggx_bvndf(float alpha, float3 wi, float2 u)
{
    float2 alpha2 = alpha.xx;
    float3 wi_std = normalize(float3(wi.xy * alpha2, wi.z));

    float phi = M_TAU * u.x;
    float a = saturate(min(alpha2.x, alpha2.y));
    float s = 1.0 + length(wi.xy);
    float a2 = a * a;
    float s2 = s * s;
    float k = (1.0 - a2) * s2 / max(s2 + a2 * wi.z * wi.z, 1e-8);
    float b = k * wi_std.z;
    float z = mad(1.0 - u.y, 1.0 + b, -b);
    float sin_theta = sqrt(saturate(1.0 - z * z));
    float3 o_std = float3(sin_theta * cos(phi), sin_theta * sin(phi), z);
    float3 m_std = wi_std + o_std;

    return normalize(float3(m_std.xy * alpha2, m_std.z));
}

float ems_approx(float alpha, float ndv)
{
    float a2 = alpha * alpha;
    float a4 = a2 * a2;
    float nv0 = 0.2 * a2;
    float nv1 = 0.32 * a2 + 1.94 * a4;
    return lerp(nv0, nv1, ndv);
}

float3 multi_scatter_specular_approx(float alpha, float ndv, float3 f0)
{
    return 1.0 + f0 * ems_approx(alpha, ndv);
}

#endif // MATERIALS_MICROFACET_HLSL
