#ifndef MATERIALS_SPECULAR_HLSL
#define MATERIALS_SPECULAR_HLSL

#include "../inc/math.hlsl"

// Specular BRDF evaluation and sampling (GGX with VNDF)
// Based on Frostbite/PBR course notes

struct SpecularBrdf
{
    float roughness;
    float3 albedo;

    static float ggx_ndf(float a2, float cos_theta)
    {
        float denom_sqrt = cos_theta * cos_theta * (a2 - 1.0) + 1.0;
        return a2 / (M_PI * denom_sqrt * denom_sqrt);
    }

    static float pdf_ggx(float a2, float cos_theta)
    {
        return ggx_ndf(a2, cos_theta) * cos_theta;
    }

    static float pdf_ggx_vndf(float a2, float3 wo, float3 h)
    {
        float g1 = smith_ggx1(wo.z, a2);
        float d = ggx_ndf(a2, h.z);
        return g1 * d * max(0.0, dot(wo, h)) / wo.z;
    }

    static float smith_ggx1(float ndotv, float a2)
    {
        float tan2_v = (1.0 - ndotv * ndotv) / (ndotv * ndotv);
        return 2.0 / (1.0 + sqrt(1.0 + a2 * tan2_v));
    }

    static float smith_ggx_correlated(float ndotv, float ndotl, float a2)
    {
        float lambda_v = ndotl * sqrt((-ndotv * a2 + ndotv) * ndotv + a2);
        float lambda_l = ndotv * sqrt((-ndotl * a2 + ndotl) * ndotl + a2);
        return 2.0 * ndotl * ndotv / (lambda_v + lambda_l);
    }

    struct SmithShadowingMasking
    {
        float g;
        float g_over_g1_wo;

        static SmithShadowingMasking eval(float ndotv, float ndotl, float a2)
        {
            SmithShadowingMasking res;
            res.g = smith_ggx_correlated(ndotv, ndotl, a2);
            res.g_over_g1_wo = res.g / smith_ggx1(ndotv, a2);
            return res;
        }
    };

    struct NdfSample
    {
        float3 m;
        float pdf;
    };

    // VNDF sampling for GGX distribution
    // From https://jcgt.org/published/0007/04/01/paper.pdf
    NdfSample sample_vndf_ggx(float alpha, float3 wo, float2 urand)
    {
        float alpha_x = alpha;
        float alpha_y = alpha;
        float a2 = alpha_x * alpha_y;

        // Transform view vector to hemisphere configuration
        float3 vh = normalize(float3(alpha_x * wo.x, alpha_y * wo.y, wo.z));

        // Construct orthonormal basis
        float3 t1 = (vh.z < 0.9999) ? normalize(cross(float3(0, 0, 1), vh)) : float3(1, 0, 0);
        float3 t2 = cross(vh, t1);

        // Parameterization of projected area
        float r = sqrt(urand.x);
        float phi = 2.0 * M_PI * urand.y;
        float t1_val = r * cos(phi);
        float t2_val = r * sin(phi);
        float s = 0.5 * (1.0 + vh.z);
        t2_val = (1.0 - s) * sqrt(max(0.0, 1.0 - t1_val * t1_val)) + s * t2_val;

        // Reproject onto hemisphere
        float3 nh = t1_val * t1 + t2_val * t2 + sqrt(max(0.0, 1.0 - t1_val * t1_val - t2_val * t2_val)) * vh;

        // Transform back to ellipsoid configuration (half vector)
        float3 h = normalize(float3(alpha_x * nh.x, alpha_y * nh.y, max(0.0, nh.z)));
        float pdf = pdf_ggx_vndf(a2, wo, h);

        NdfSample res;
        res.m = h;
        res.pdf = pdf;
        return res;
    }
};

#endif // MATERIALS_SPECULAR_HLSL
