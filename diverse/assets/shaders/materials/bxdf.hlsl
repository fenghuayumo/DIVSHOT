#ifndef MATERIALS_BXDF_HLSL
#define MATERIALS_BXDF_HLSL

#include "../inc/math.hlsl"
#include "../inc/monte_carlo.hlsl"
#include "bsdf_types.hlsl"
#include "fresnel.hlsl"
#include "microfacet.hlsl"

static const float RTXPT_BSDF_MIN_COS_THETA = 1e-6;
static const float BSDF_ONE_MINUS_EPSILON = 0.99999994;

float material_luminance(float3 color)
{
    return max(0.0, dot(color, float3(0.2126, 0.7152, 0.0722)));
}

float material_average(float3 color)
{
    return (color.x + color.y + color.z) * (1.0 / 3.0);
}

float3 sample_cosine_hemisphere(float2 u, out float pdf)
{
    float4 sample = cosine_sample_hemi_sphere_concentric_pdf(u);
    pdf = sample.w;
    return sample.xyz;
}

struct StandardBSDFData
{
    float3 diffuse;
    float roughness;
    float3 specular;
    float metallic;
    float eta;
    float anisotropy;
    float3 transmission;
    float diffuse_transmission;
    float specular_transmission;
    float fuzz_weight;
    float3 fuzz_color;
    float fuzz_roughness;
    bool is_thin_surface;
};

struct DiffuseReflectionLambertBxDF
{
    float3 albedo;

    float3 eval(float3 wi, float3 wo)
    {
        if (min(wi.z, wo.z) < RTXPT_BSDF_MIN_COS_THETA)
            return 0.0.xxx;

        return albedo * M_FRAC_1_PI * wo.z;
    }

    bool sample(float3 wi, out float3 wo, out float pdf, out float3 weight, out uint lobe, out float lobe_p, float3 u)
    {
        wo = sample_cosine_hemisphere(u.xy, pdf);
        lobe = (uint)LOBE_DIFFUSE_REFLECTION;
        lobe_p = 1.0;

        if (min(wi.z, wo.z) < RTXPT_BSDF_MIN_COS_THETA)
        {
            weight = 0.0.xxx;
            lobe_p = 0.0;
            return false;
        }

        weight = albedo;
        return true;
    }

    float eval_pdf(float3 wi, float3 wo)
    {
        if (min(wi.z, wo.z) < RTXPT_BSDF_MIN_COS_THETA)
            return 0.0;

        return M_FRAC_1_PI * wo.z;
    }
};

struct DiffuseTransmissionLambertBxDF
{
    float3 albedo;

    float3 eval(float3 wi, float3 wo)
    {
        if (min(wi.z, -wo.z) < RTXPT_BSDF_MIN_COS_THETA)
            return 0.0.xxx;

        return albedo * M_FRAC_1_PI * -wo.z;
    }

    bool sample(float3 wi, out float3 wo, out float pdf, out float3 weight, out uint lobe, out float lobe_p, float3 u)
    {
        wo = sample_cosine_hemisphere(u.xy, pdf);
        wo.z = -wo.z;
        lobe = (uint)LOBE_DIFFUSE_TRANSMISSION;
        lobe_p = 1.0;

        if (min(wi.z, -wo.z) < RTXPT_BSDF_MIN_COS_THETA)
        {
            weight = 0.0.xxx;
            lobe_p = 0.0;
            return false;
        }

        weight = albedo;
        return true;
    }

    float eval_pdf(float3 wi, float3 wo)
    {
        if (min(wi.z, -wo.z) < RTXPT_BSDF_MIN_COS_THETA)
            return 0.0;

        return M_FRAC_1_PI * -wo.z;
    }
};

struct FuzzReflectionBxDF
{
    float3 color;
    float weight;
    float roughness;

    float3 eval_weight(float3 wi, float3 wo)
    {
        float3 h = normalize(wi + wo);
        float view_sheen = pow(saturate(1.0 - dot(wi, h)), lerp(6.0, 1.0, roughness));
        float grazing = pow(saturate(1.0 - min(wi.z, wo.z)), lerp(3.0, 0.8, roughness));
        return color * weight * max(view_sheen, grazing);
    }

    float3 eval(float3 wi, float3 wo)
    {
        if (min(wi.z, wo.z) < RTXPT_BSDF_MIN_COS_THETA || weight <= 0.0)
            return 0.0.xxx;

        return eval_weight(wi, wo) * M_FRAC_1_PI * wo.z;
    }

    bool sample(float3 wi, out float3 wo, out float pdf, out float3 sample_weight, out uint lobe, out float lobe_p, float3 u)
    {
        wo = sample_cosine_hemisphere(u.xy, pdf);
        lobe = (uint)LOBE_DIFFUSE_REFLECTION;
        lobe_p = 1.0;

        if (min(wi.z, wo.z) < RTXPT_BSDF_MIN_COS_THETA || weight <= 0.0)
        {
            sample_weight = 0.0.xxx;
            lobe_p = 0.0;
            return false;
        }

        sample_weight = eval_weight(wi, wo);
        return true;
    }

    float eval_pdf(float3 wi, float3 wo)
    {
        if (min(wi.z, wo.z) < RTXPT_BSDF_MIN_COS_THETA || weight <= 0.0)
            return 0.0;

        return M_FRAC_1_PI * wo.z;
    }
};

struct SpecularReflectionMicrofacetBxDF
{
    float3 albedo;
    float alpha;
    float anisotropy;
    uint active_lobes;

    bool has_lobe(LobeType lobe)
    {
        return lobe_has(active_lobes, lobe);
    }

    float3 eval(float3 wi, float3 wo)
    {
        if (min(wi.z, wo.z) < RTXPT_BSDF_MIN_COS_THETA || alpha == 0.0 || !has_lobe(LOBE_SPECULAR_REFLECTION))
            return 0.0.xxx;

        float3 h = normalize(wi + wo);
        float wi_dot_h = dot(wi, h);
        float d = eval_ndf_ggx(alpha, h.z);
        float g = eval_masking_smith_ggx_correlated(alpha, wi.z, wo.z);
        float3 f = fresnel_schlick(albedo, 1.0, wi_dot_h);
        float3 ms = multi_scatter_specular_approx(alpha, wi.z, albedo);

        return ms * f * (d * g * 0.25 / max(wi.z, RTXPT_BSDF_MIN_COS_THETA));
    }

    bool sample(float3 wi, out float3 wo, out float pdf, out float3 weight, out uint lobe, out float lobe_p, float3 u)
    {
        wo = 0.0.xxx;
        weight = 0.0.xxx;
        pdf = 0.0;
        lobe = (uint)LOBE_SPECULAR_REFLECTION;
        lobe_p = 1.0;

        if (wi.z < RTXPT_BSDF_MIN_COS_THETA)
            return false;

        if (alpha == 0.0)
        {
            if (!has_lobe(LOBE_DELTA_REFLECTION))
                return false;

            wo = float3(-wi.x, -wi.y, wi.z);
            weight = fresnel_schlick(albedo, 1.0, wi.z);
            lobe = (uint)LOBE_DELTA_REFLECTION;
            return true;
        }

        if (!has_lobe(LOBE_SPECULAR_REFLECTION))
            return false;

        float3 h = sample_ggx_bvndf(alpha, wi, u.xy);
        float wi_dot_h = dot(wi, h);
        wo = 2.0 * wi_dot_h * h - wi;
        if (wo.z < RTXPT_BSDF_MIN_COS_THETA)
            return false;

        pdf = eval_pdf(wi, wo);
        weight = pdf > 0.0 ? eval(wi, wo) / pdf : 0.0.xxx;
        lobe = (uint)LOBE_SPECULAR_REFLECTION;
        return true;
    }

    float eval_pdf(float3 wi, float3 wo)
    {
        if (min(wi.z, wo.z) < RTXPT_BSDF_MIN_COS_THETA || alpha == 0.0 || !has_lobe(LOBE_SPECULAR_REFLECTION))
            return 0.0;

        float3 h = normalize(wi + wo);
        return eval_pdf_ggx_bvndf(alpha, wi, h);
    }
};

struct SpecularReflectionTransmissionMicrofacetBxDF
{
    float3 transmission_albedo;
    float alpha;
    float eta;
    uint active_lobes;
    bool is_thin_surface;

    bool has_lobe(LobeType lobe)
    {
        return lobe_has(active_lobes, lobe);
    }

    float actual_eta(bool is_reflection)
    {
        return (is_thin_surface && !is_reflection) ? 1.0 : eta;
    }

    float3 half_vector(float3 wi, float3 wo, bool is_reflection, float eta_for_lobe)
    {
        float3 h = normalize(wo + wi * (is_reflection ? 1.0 : eta_for_lobe));
        return h * sign(h.z);
    }

    float3 eval(float3 wi, float3 wo)
    {
        if (min(wi.z, abs(wo.z)) < RTXPT_BSDF_MIN_COS_THETA || alpha == 0.0)
            return 0.0.xxx;

        bool is_reflection = wo.z > 0.0;
        bool has_reflection = has_lobe(LOBE_SPECULAR_REFLECTION);
        bool has_transmission = has_lobe(LOBE_SPECULAR_TRANSMISSION);
        if ((is_reflection && !has_reflection) || (!is_reflection && !has_transmission))
            return 0.0.xxx;

        float eta_for_lobe = actual_eta(is_reflection);
        float3 h = half_vector(wi, wo, is_reflection, eta_for_lobe);

        float wi_dot_h = dot(wi, h);
        float wo_dot_h = dot(wo, h);
        float d = eval_ndf_ggx(alpha, h.z);
        float g = eval_masking_smith_ggx_correlated(alpha, wi.z, abs(wo.z));
        float f = eval_fresnel_dielectric(eta_for_lobe, wi_dot_h);

        if (is_reflection)
            return f * d * g * 0.25 / max(wi.z, RTXPT_BSDF_MIN_COS_THETA);

        float sqrt_denom = wo_dot_h + eta_for_lobe * wi_dot_h;
        float t = eta_for_lobe * eta_for_lobe * wi_dot_h * wo_dot_h / (wi.z * sqrt_denom * sqrt_denom);
        return transmission_albedo * (1.0 - f) * d * g * abs(t);
    }

    bool sample(float3 wi, out float3 wo, out float pdf, out float3 weight, out uint lobe, out float lobe_p, float3 u)
    {
        wo = 0.0.xxx;
        weight = 0.0.xxx;
        pdf = 0.0;
        lobe = (uint)LOBE_SPECULAR_REFLECTION;
        lobe_p = 1.0;

        if (wi.z < RTXPT_BSDF_MIN_COS_THETA)
            return false;

        float lobe_sample = u.z;
        bool has_reflection = has_lobe(LOBE_SPECULAR_REFLECTION) || has_lobe(LOBE_DELTA_REFLECTION);
        bool has_transmission = has_lobe(LOBE_SPECULAR_TRANSMISSION) || has_lobe(LOBE_DELTA_TRANSMISSION);
        if (!(has_reflection || has_transmission))
            return false;

        if (alpha == 0.0)
        {
            has_reflection = has_lobe(LOBE_DELTA_REFLECTION);
            has_transmission = has_lobe(LOBE_DELTA_TRANSMISSION);
            if (!(has_reflection || has_transmission))
                return false;

            float cos_theta_t;
            float f = eval_fresnel_dielectric(eta, wi.z, cos_theta_t);
            bool is_reflection = has_reflection;
            if (has_reflection && has_transmission)
            {
                is_reflection = lobe_sample < f;
                lobe_p = is_reflection ? f : (1.0 - f);
            }
            else if (has_transmission && f == 1.0)
            {
                return false;
            }

            float eta_for_lobe = eta;
            if (is_thin_surface && !is_reflection)
            {
                eta_for_lobe = 1.0;
                f = eval_fresnel_dielectric(eta_for_lobe, wi.z, cos_theta_t);
            }

            weight = is_reflection ? 1.0.xxx : transmission_albedo;
            if (!(has_reflection && has_transmission))
                weight *= is_reflection ? f.xxx : (1.0 - f).xxx;
            wo = is_reflection ? float3(-wi.x, -wi.y, wi.z) : float3(-wi.x * eta_for_lobe, -wi.y * eta_for_lobe, -cos_theta_t);
            lobe = is_reflection ? (uint)LOBE_DELTA_REFLECTION : (uint)LOBE_DELTA_TRANSMISSION;

            return abs(wo.z) >= RTXPT_BSDF_MIN_COS_THETA && ((wo.z > 0.0) == is_reflection);
        }

        float3 h = sample_ggx_bvndf(alpha, wi, u.xy);
        float wi_dot_h = dot(wi, h);

        float cos_theta_t;
        float f = eval_fresnel_dielectric(eta, wi_dot_h, cos_theta_t);

        bool is_reflection = has_lobe(LOBE_SPECULAR_REFLECTION);
        if (has_lobe(LOBE_SPECULAR_REFLECTION) && has_lobe(LOBE_SPECULAR_TRANSMISSION))
            is_reflection = lobe_sample < f;
        else if (has_lobe(LOBE_SPECULAR_TRANSMISSION) && f == 1.0)
            return false;

        float eta_for_lobe = eta;
        if (is_thin_surface && !is_reflection)
        {
            eta_for_lobe = 1.0;
            f = eval_fresnel_dielectric(eta_for_lobe, wi.z, cos_theta_t);
        }

        wo = is_reflection
            ? (2.0 * wi_dot_h * h - wi)
            : ((eta_for_lobe * wi_dot_h - cos_theta_t) * h - eta_for_lobe * wi);

        if (abs(wo.z) < RTXPT_BSDF_MIN_COS_THETA || ((wo.z > 0.0) != is_reflection))
            return false;

        lobe = is_reflection ? (uint)LOBE_SPECULAR_REFLECTION : (uint)LOBE_SPECULAR_TRANSMISSION;
        pdf = eval_pdf(wi, wo);
        weight = pdf > 0.0 ? eval(wi, wo) / pdf : 0.0.xxx;
        return true;
    }

    float eval_pdf(float3 wi, float3 wo)
    {
        if (min(wi.z, abs(wo.z)) < RTXPT_BSDF_MIN_COS_THETA || alpha == 0.0)
            return 0.0;

        bool is_reflection = wo.z > 0.0;
        bool has_reflection = has_lobe(LOBE_SPECULAR_REFLECTION);
        bool has_transmission = has_lobe(LOBE_SPECULAR_TRANSMISSION);
        if ((is_reflection && !has_reflection) || (!is_reflection && !has_transmission))
            return 0.0;

        float eta_for_lobe = actual_eta(is_reflection);
        float3 h = half_vector(wi, wo, is_reflection, eta_for_lobe);

        float wi_dot_h = dot(wi, h);
        float wo_dot_h = dot(wo, h);
        float f = eval_fresnel_dielectric(eta_for_lobe, wi_dot_h);
        float pdf = eval_pdf_ggx_bvndf(alpha, wi, h);

        if (is_reflection)
        {
            if (wo_dot_h <= 0.0)
                return 0.0;
            pdf *= wi_dot_h / max(wo_dot_h, RTXPT_BSDF_MIN_COS_THETA);
        }
        else
        {
            if (wo_dot_h > 0.0)
                return 0.0;
            pdf *= wi_dot_h * 4.0;
            float sqrt_denom = wo_dot_h + eta_for_lobe * wi_dot_h;
            float denom = sqrt_denom * sqrt_denom;
            pdf *= abs(wo_dot_h) / max(denom, 1e-8);
        }

        if (has_reflection && has_transmission)
            pdf *= is_reflection ? f : (1.0 - f);

        return clamp(pdf, 0.0, FLT_MAX);
    }
};

#endif // MATERIALS_BXDF_HLSL
