#ifndef MATERIALS_OPENPBR_BSDF_HLSL
#define MATERIALS_OPENPBR_BSDF_HLSL

#include "../specular.hlsl"
#include "../diffuse.hlsl"
#include "../fuzz.hlsl"
#include "../fresnel.hlsl"
#include "../material_types.hlsl"

// Combined OpenPBR BSDF with specular, diffuse, and fuzz lobes

struct BrdfValue
{
    float3 value_over_pdf;
    float3 value;
    float pdf;
    float3 transmission_fraction;

    static BrdfValue invalid()
    {
        BrdfValue res;
        res.value_over_pdf = 0.0;
        res.pdf = 0.0;
        res.transmission_fraction = 0.0;
        res.value = 0.0;
        return res;
    }
};

struct BrdfSample : BrdfValue
{
    float3 wi;
    float approx_roughness;

    static BrdfSample invalid()
    {
        BrdfSample res;
        res.value_over_pdf = 0.0;
        res.pdf = 0.0;
        res.wi = float3(0.0, 0.0, -1.0);
        res.transmission_fraction = 0.0;
        res.approx_roughness = 0;
        res.value = 0.0;
        return res;
    }

    bool is_valid()
    {
        return wi.z > 1e-6;
    }
};

struct SpecularBrdfEnergyPreservation
{
    float3 preintegrated_reflection;
    float3 preintegrated_reflection_mult;
    float3 preintegrated_transmission_fraction;

    static SpecularBrdfEnergyPreservation from_brdf_ndotv(SpecularBrdf brdf, float ndotv)
    {
        SpecularBrdfEnergyPreservation res;
        // Simplified energy compensation
        // In full implementation, would use pre-integrated BRDF LUT
        float3 albedo = brdf.albedo;
        float roughness = brdf.roughness;

        // Approximate multi-scattering energy compensation
        float fresnel_avg = luminance(lerp(albedo, 1.0.xxx, 0.5));
        res.preintegrated_reflection = albedo * (1.0 + fresnel_avg * (1.0 - roughness));
        res.preintegrated_reflection_mult = saturate(res.preintegrated_reflection);
        res.preintegrated_transmission_fraction = 1.0.xxx - fresnel_schlick(albedo, 1.0, ndotv);

        return res;
    }
};

struct OpenPbrBsdf
{
    SpecularBrdf specular_brdf;
    FrostbiteDiffuseBrdf diffuse_brdf;
    FuzzBrdf fuzz_brdf;
    SpecularBrdfEnergyPreservation energy_preservation;

    static OpenPbrBsdf from_surface(MaterialProperties mat)
    {
        OpenPbrBsdf res;

        res.specular_brdf.roughness = mat.roughness;
        res.specular_brdf.albedo = mat.specular_f0;

        res.diffuse_brdf.albedo = mat.diffuse_albedo;
        res.diffuse_brdf.roughness = mat.roughness;

        res.fuzz_brdf.color = mat.fuzz_color;
        res.fuzz_brdf.weight = mat.fuzz_weight;
        res.fuzz_brdf.roughness = mat.fuzz_roughness;

        // Energy compensation (simplified, would use ndotv in full)
        res.energy_preservation = SpecularBrdfEnergyPreservation::from_brdf_ndotv(
            res.specular_brdf, 0.5);

        return res;
    }

    float3 evaluate(float3 wo, float3 wi)
    {
        if (wo.z <= 0.0 || wi.z <= 0.0)
            return 0.0;

        float3 diff = diffuse_brdf.evaluate(wo, wi);
        float3 spec = specular_brdf.albedo * energy_preservation.preintegrated_reflection_mult;
        float3 fuzz = fuzz_brdf.evaluate(wo, wi);

        return spec + diff * energy_preservation.preintegrated_transmission_fraction + fuzz;
    }

    BrdfSample sample(float3 wo, float3 urand)
    {
        // Sample based on energy weights
        float spec_wt = luminance(energy_preservation.preintegrated_reflection);
        float diffuse_wt = luminance(energy_preservation.preintegrated_transmission_fraction * diffuse_brdf.albedo);
        float fuzz_wt = luminance(fuzz_brdf.color) * fuzz_brdf.weight;

        float total_wt = max(1e-6, spec_wt + diffuse_wt + fuzz_wt);
        float spec_p = spec_wt / total_wt;
        float diffuse_p = diffuse_wt / total_wt;

        BrdfSample brdf_sample;
        float lobe_xi = urand.z;

        if (lobe_xi < diffuse_p)
        {
            // Sample diffuse
            float3 wi;
            float pdf_val;
            diffuse_brdf.sample(wo, urand.xy, wi, pdf_val);

            brdf_sample.wi = wi;
            brdf_sample.pdf = pdf_val * diffuse_p;
            brdf_sample.transmission_fraction = energy_preservation.preintegrated_transmission_fraction;
            brdf_sample.value = diffuse_brdf.evaluate(wo, wi) * brdf_sample.transmission_fraction;
            brdf_sample.value_over_pdf = brdf_sample.value / brdf_sample.pdf;
            brdf_sample.approx_roughness = 1.0;
        }
        else if (lobe_xi < diffuse_p + spec_p)
        {
            // Sample specular (simplified cosine-weighted for now)
            float phi = urand.x * M_TAU;
            float cos_theta = sqrt(max(0.0, 1.0 - urand.y));
            float sin_theta = sqrt(max(0.0, 1.0 - cos_theta * cos_theta));

            brdf_sample.wi = float3(cos(phi) * sin_theta, sin(phi) * sin_theta, cos_theta);
            brdf_sample.pdf = M_FRAC_1_PI * spec_p;
            brdf_sample.transmission_fraction = 0.0;
            brdf_sample.value = specular_brdf.albedo * energy_preservation.preintegrated_reflection_mult;
            brdf_sample.value_over_pdf = brdf_sample.value / brdf_sample.pdf;
            brdf_sample.approx_roughness = specular_brdf.roughness;
        }
        else
        {
            // Sample fuzz
            float3 wi;
            float pdf_val;
            fuzz_brdf.sample(wo, urand.xy, wi, pdf_val);

            brdf_sample.wi = wi;
            brdf_sample.pdf = pdf_val * (1.0 - diffuse_p - spec_p);
            brdf_sample.transmission_fraction = 0.0;
            brdf_sample.value = fuzz_brdf.evaluate(wo, wi);
            brdf_sample.value_over_pdf = brdf_sample.value / brdf_sample.pdf;
            brdf_sample.approx_roughness = fuzz_brdf.roughness;
        }

        return brdf_sample;
    }
};

#endif // MATERIALS_OPENPBR_BSDF_HLSL
