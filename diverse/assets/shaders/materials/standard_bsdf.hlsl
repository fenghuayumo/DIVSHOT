#ifndef MATERIALS_STANDARD_BSDF_HLSL
#define MATERIALS_STANDARD_BSDF_HLSL

#include "bsdf_types.hlsl"
#include "material_data.hlsl"
#include "../inc/gbuffer.hlsl"
#include "bxdf.hlsl"

struct StandardBSDF
{
    DiffuseReflectionLambertBxDF diffuse_reflection;
    DiffuseTransmissionLambertBxDF diffuse_transmission;
    FuzzReflectionBxDF fuzz_reflection;
    SpecularReflectionMicrofacetBxDF specular_reflection;
    SpecularReflectionTransmissionMicrofacetBxDF specular_reflection_transmission;

    float roughness;
    float alpha;
    uint active_lobes;

    float diffuse_transmission_mix;
    float specular_transmission_mix;

    float p_diffuse_reflection;
    float p_diffuse_transmission;
    float p_fuzz_reflection;
    float p_specular_reflection;
    float p_specular_reflection_transmission;

    bool has_lobe(LobeType type)
    {
        return lobe_has(active_lobes, type);
    }

    void normalize_sampling_weights()
    {
        float norm = p_diffuse_reflection
            + p_diffuse_transmission
            + p_fuzz_reflection
            + p_specular_reflection
            + p_specular_reflection_transmission;

        if (norm <= 0.0)
            return;

        float inv_norm = 1.0 / norm;
        p_diffuse_reflection *= inv_norm;
        p_diffuse_transmission *= inv_norm;
        p_fuzz_reflection *= inv_norm;
        p_specular_reflection *= inv_norm;
        p_specular_reflection_transmission *= inv_norm;
    }

    static uint get_lobes(StandardBSDFData data)
    {
        float alpha = data.roughness * data.roughness;
        bool is_delta = alpha < MATERIAL_MIN_GGX_ALPHA;

        float diffuse_trans = saturate(data.diffuse_transmission);
        float specular_trans = saturate(data.specular_transmission);

        uint lobes = is_delta ? (uint)LOBE_DELTA_REFLECTION : (uint)LOBE_SPECULAR_REFLECTION;
        if ((any(data.diffuse > 0.0) || data.fuzz_weight > 0.0) && specular_trans < 1.0)
        {
            if (diffuse_trans < 1.0)
                lobes |= (uint)LOBE_DIFFUSE_REFLECTION;
            if (diffuse_trans > 0.0)
                lobes |= (uint)LOBE_DIFFUSE_TRANSMISSION;
        }

        if (specular_trans > 0.0)
            lobes |= is_delta ? (uint)LOBE_DELTA_TRANSMISSION : (uint)LOBE_SPECULAR_TRANSMISSION;

        return lobes;
    }

    void init(StandardBSDFData data, float3 shading_normal, float3 view_world)
    {
        roughness = clamp(data.roughness, 0.0, 1.0);
        alpha = roughness * roughness;
        alpha = alpha < MATERIAL_MIN_GGX_ALPHA ? 0.0 : max(alpha, MATERIAL_MIN_GGX_ALPHA);
        active_lobes = get_lobes(data);

        float3 transmission_albedo = data.is_thin_surface
            ? max(0.0.xxx, data.transmission)
            : sqrt(max(0.0.xxx, data.transmission));

        diffuse_reflection.albedo = max(0.0.xxx, data.diffuse);
        diffuse_transmission.albedo = transmission_albedo;

        fuzz_reflection.color = max(0.0.xxx, data.fuzz_color);
        fuzz_reflection.weight = max(0.0, data.fuzz_weight);
        fuzz_reflection.roughness = saturate(data.fuzz_roughness);

        specular_reflection.albedo = max(0.0.xxx, data.specular);
        specular_reflection.alpha = alpha;
        specular_reflection.anisotropy = data.anisotropy;
        specular_reflection.active_lobes = active_lobes;

        specular_reflection_transmission.transmission_albedo = transmission_albedo;
        specular_reflection_transmission.alpha = data.eta == 1.0 ? 0.0 : alpha;
        specular_reflection_transmission.eta = data.eta;
        specular_reflection_transmission.active_lobes = active_lobes;
        specular_reflection_transmission.is_thin_surface = data.is_thin_surface;

        diffuse_transmission_mix = saturate(data.diffuse_transmission);
        specular_transmission_mix = saturate(data.specular_transmission);

        float metallic_brdf = saturate(data.metallic) * (1.0 - specular_transmission_mix);
        float dielectric_bsdf = (1.0 - saturate(data.metallic)) * (1.0 - specular_transmission_mix);
        float specular_bsdf = specular_transmission_mix;

        float diffuse_weight = material_luminance(diffuse_reflection.albedo);
        float fuzz_weight = material_luminance(fuzz_reflection.color) * fuzz_reflection.weight;
        float specular_weight = material_luminance(fresnel_schlick(specular_reflection.albedo, 1.0, saturate(dot(view_world, shading_normal))));

        p_diffuse_reflection = has_lobe(LOBE_DIFFUSE_REFLECTION)
            ? diffuse_weight * dielectric_bsdf * (1.0 - diffuse_transmission_mix)
            : 0.0;
        p_diffuse_transmission = has_lobe(LOBE_DIFFUSE_TRANSMISSION)
            ? diffuse_weight * dielectric_bsdf * diffuse_transmission_mix
            : 0.0;
        p_fuzz_reflection = has_lobe(LOBE_DIFFUSE_REFLECTION)
            ? fuzz_weight * dielectric_bsdf * (1.0 - diffuse_transmission_mix)
            : 0.0;
        p_specular_reflection = (has_lobe(LOBE_SPECULAR_REFLECTION) || has_lobe(LOBE_DELTA_REFLECTION))
            ? specular_weight * (metallic_brdf + dielectric_bsdf)
            : 0.0;
        p_specular_reflection_transmission =
            (has_lobe(LOBE_SPECULAR_REFLECTION)
                || has_lobe(LOBE_DELTA_REFLECTION)
                || has_lobe(LOBE_SPECULAR_TRANSMISSION)
                || has_lobe(LOBE_DELTA_TRANSMISSION))
            ? specular_bsdf
            : 0.0;

        normalize_sampling_weights();
    }

    BsdfEvalResult evaluate(BsdfEvalData eval_data)
    {
        BsdfEvalResult result = BsdfEvalResult::invalid();
        float3 wi = eval_data.wo;
        float3 wo = eval_data.wi;

        if (p_diffuse_reflection > 0.0)
        {
            float3 v = (1.0 - specular_transmission_mix) * (1.0 - diffuse_transmission_mix) * diffuse_reflection.eval(wi, wo);
            result.diffuse_value += v;
            result.value += v;
        }
        if (p_diffuse_transmission > 0.0)
        {
            float3 v = (1.0 - specular_transmission_mix) * diffuse_transmission_mix * diffuse_transmission.eval(wi, wo);
            result.transmission_value += v;
            result.value += v;
        }
        if (p_fuzz_reflection > 0.0)
        {
            float3 v = (1.0 - specular_transmission_mix) * (1.0 - diffuse_transmission_mix) * fuzz_reflection.eval(wi, wo);
            result.fuzz_value += v;
            result.diffuse_value += v;
            result.value += v;
        }
        if (p_specular_reflection > 0.0)
        {
            float3 v = (1.0 - specular_transmission_mix) * specular_reflection.eval(wi, wo);
            result.specular_value += v;
            result.value += v;
        }
        if (p_specular_reflection_transmission > 0.0)
        {
            float3 v = specular_transmission_mix * specular_reflection_transmission.eval(wi, wo);
            if (wo.z > 0.0)
                result.specular_value += v;
            else
                result.transmission_value += v;
            result.value += v;
        }

        result.pdf = eval_pdf(wi, wo);
        return result;
    }

    float eval_pdf(float3 wi, float3 wo)
    {
        float pdf = 0.0;
        if (p_diffuse_reflection > 0.0)
            pdf += p_diffuse_reflection * diffuse_reflection.eval_pdf(wi, wo);
        if (p_diffuse_transmission > 0.0)
            pdf += p_diffuse_transmission * diffuse_transmission.eval_pdf(wi, wo);
        if (p_fuzz_reflection > 0.0)
            pdf += p_fuzz_reflection * fuzz_reflection.eval_pdf(wi, wo);
        if (p_specular_reflection > 0.0)
            pdf += p_specular_reflection * specular_reflection.eval_pdf(wi, wo);
        if (p_specular_reflection_transmission > 0.0)
            pdf += p_specular_reflection_transmission * specular_reflection_transmission.eval_pdf(wi, wo);
        return pdf;
    }

    void add_sample_pdf(inout float pdf, float3 wi, float3 wo, uint skip_lobe)
    {
        if (skip_lobe != (uint)LOBE_DIFFUSE_REFLECTION && p_diffuse_reflection > 0.0)
            pdf += p_diffuse_reflection * diffuse_reflection.eval_pdf(wi, wo);
        if (skip_lobe != (uint)LOBE_DIFFUSE_TRANSMISSION && p_diffuse_transmission > 0.0)
            pdf += p_diffuse_transmission * diffuse_transmission.eval_pdf(wi, wo);
        if (skip_lobe != 0x10000u && p_fuzz_reflection > 0.0)
            pdf += p_fuzz_reflection * fuzz_reflection.eval_pdf(wi, wo);
        if (skip_lobe != (uint)LOBE_SPECULAR_REFLECTION && p_specular_reflection > 0.0)
            pdf += p_specular_reflection * specular_reflection.eval_pdf(wi, wo);
        if (skip_lobe != 0x20000u && p_specular_reflection_transmission > 0.0)
            pdf += p_specular_reflection_transmission * specular_reflection_transmission.eval_pdf(wi, wo);
    }

    BsdfSampleResult sample(float3 wi, float3 u)
    {
        BsdfSampleResult result = BsdfSampleResult::invalid();

        float3 wo = 0.0.xxx;
        float pdf = 0.0;
        float3 weight = 0.0.xxx;
        uint lobe = (uint)LOBE_DIFFUSE_REFLECTION;
        float lobe_p = 0.0;
        bool valid = false;

        float u_select = u.z;
        uint selected = 0u;

        if (u_select < p_diffuse_reflection)
        {
            u.z = clamp(u_select / max(p_diffuse_reflection, 1e-8), 0.0, BSDF_ONE_MINUS_EPSILON);
            valid = diffuse_reflection.sample(wi, wo, pdf, weight, lobe, lobe_p, u);
            weight /= max(p_diffuse_reflection, 1e-8);
            weight *= (1.0 - specular_transmission_mix) * (1.0 - diffuse_transmission_mix);
            pdf *= p_diffuse_reflection;
            lobe_p *= p_diffuse_reflection;
            selected = (uint)LOBE_DIFFUSE_REFLECTION;
            add_sample_pdf(pdf, wi, wo, selected);
        }
        else if (u_select < p_diffuse_reflection + p_diffuse_transmission)
        {
            u.z = clamp((u_select - p_diffuse_reflection) / max(p_diffuse_transmission, 1e-8), 0.0, BSDF_ONE_MINUS_EPSILON);
            valid = diffuse_transmission.sample(wi, wo, pdf, weight, lobe, lobe_p, u);
            weight /= max(p_diffuse_transmission, 1e-8);
            weight *= (1.0 - specular_transmission_mix) * diffuse_transmission_mix;
            pdf *= p_diffuse_transmission;
            lobe_p *= p_diffuse_transmission;
            selected = (uint)LOBE_DIFFUSE_TRANSMISSION;
            add_sample_pdf(pdf, wi, wo, selected);
        }
        else if (u_select < p_diffuse_reflection + p_diffuse_transmission + p_fuzz_reflection)
        {
            u.z = clamp((u_select - (p_diffuse_reflection + p_diffuse_transmission)) / max(p_fuzz_reflection, 1e-8), 0.0, BSDF_ONE_MINUS_EPSILON);
            valid = fuzz_reflection.sample(wi, wo, pdf, weight, lobe, lobe_p, u);
            weight /= max(p_fuzz_reflection, 1e-8);
            weight *= (1.0 - specular_transmission_mix) * (1.0 - diffuse_transmission_mix);
            pdf *= p_fuzz_reflection;
            lobe_p *= p_fuzz_reflection;
            selected = 0x10000u;
            add_sample_pdf(pdf, wi, wo, selected);
        }
        else if (u_select < p_diffuse_reflection + p_diffuse_transmission + p_fuzz_reflection + p_specular_reflection)
        {
            u.z = clamp((u_select - (p_diffuse_reflection + p_diffuse_transmission + p_fuzz_reflection)) / max(p_specular_reflection, 1e-8), 0.0, BSDF_ONE_MINUS_EPSILON);
            valid = specular_reflection.sample(wi, wo, pdf, weight, lobe, lobe_p, u);
            weight /= max(p_specular_reflection, 1e-8);
            weight *= (1.0 - specular_transmission_mix);
            pdf *= p_specular_reflection;
            lobe_p *= p_specular_reflection;
            selected = (uint)LOBE_SPECULAR_REFLECTION;
            add_sample_pdf(pdf, wi, wo, selected);
        }
        else if (p_specular_reflection_transmission > 0.0)
        {
            u.z = clamp((u_select - (p_diffuse_reflection + p_diffuse_transmission + p_fuzz_reflection + p_specular_reflection)) / max(p_specular_reflection_transmission, 1e-8), 0.0, BSDF_ONE_MINUS_EPSILON);
            valid = specular_reflection_transmission.sample(wi, wo, pdf, weight, lobe, lobe_p, u);
            weight /= max(p_specular_reflection_transmission, 1e-8);
            weight *= specular_transmission_mix;
            pdf *= p_specular_reflection_transmission;
            lobe_p *= p_specular_reflection_transmission;
            selected = 0x20000u;
            add_sample_pdf(pdf, wi, wo, selected);
        }

        if (!valid)
            return BsdfSampleResult::invalid();

        if (lobe_has(lobe, LOBE_DELTA))
            pdf = 0.0;

        result.wi = wo;
        result.value = weight;
        result.pdf = pdf;
        result.lobe_value = 0.0.xxx;
        result.lobe_pdf = pdf;
        result.approx_roughness = roughness;
        result.selected_lobe = (LobeType)lobe;
        return result;
    }

    static float dielectric_f0_from_ior(float ior)
    {
        const float f = (ior - 1.0) / (ior + 1.0);
        return f * f;
    }

    static StandardBSDFData data_from_surface(MaterialData material, GbufferData gbuffer)
    {
        StandardBSDFData data;
        float metalness = saturate(gbuffer.metalness);
        float dielectric_f0 = dielectric_f0_from_ior(max(1.0, material.specular_ior));
        float3 base_color = max(0.0.xxx, gbuffer.albedo);

        data.diffuse = base_color;
        data.roughness = saturate(gbuffer.roughness);
        data.specular = lerp(
            dielectric_f0 * max(0.0, material.specular_weight) * max(0.0.xxx, material.specular_color.rgb),
            base_color,
            metalness);
        data.metallic = metalness;
        data.eta = 1.0 / max(1.0, material.specular_ior);
        data.anisotropy = material.anisotropy;
        data.transmission = base_color * max(0.0.xxx, material.transmission_color.rgb);
        data.diffuse_transmission = 0.0;
        data.specular_transmission = saturate(material.transmission_weight) * (1.0 - metalness);
        data.fuzz_weight = max(0.0, material.fuzz_weight);
        data.fuzz_color = max(0.0.xxx, material.fuzz_color.rgb);
        data.fuzz_roughness = max(0.0, material.fuzz_roughness);
        data.is_thin_surface = (material.material_flags & MATERIAL_FLAG_THIN_SURFACE) != 0;
        return data;
    }

    static StandardBSDFData data_from_material(MaterialData material)
    {
        GbufferData gbuffer = GbufferData::create_zero();
        gbuffer.albedo = material.base_color_mult.rgb;
        gbuffer.roughness = material.roughness_mult;
        gbuffer.metalness = material.metalness_factor;
        return data_from_surface(material, gbuffer);
    }

    static StandardBSDF from_surface(MaterialData material, GbufferData gbuffer, float3 shading_normal_ws, float3 view_ws)
    {
        StandardBSDF bsdf;
        bsdf.init(data_from_surface(material, gbuffer), shading_normal_ws, view_ws);
        return bsdf;
    }

    static StandardBSDF from_surface(MaterialData material, GbufferData gbuffer)
    {
        return from_surface(material, gbuffer, gbuffer.normal, gbuffer.normal);
    }

    static StandardBSDF from_material(MaterialData material)
    {
        StandardBSDF bsdf;
        bsdf.init(data_from_material(material), float3(0.0, 0.0, 1.0), float3(0.0, 0.0, 1.0));
        return bsdf;
    }
};

#endif // MATERIALS_STANDARD_BSDF_HLSL
