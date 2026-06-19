#ifndef MATERIALS_STANDARD_BSDF_HLSL
#define MATERIALS_STANDARD_BSDF_HLSL

#include "bsdf_types.hlsl"
#include "material_data.hlsl"
#include "../inc/math.hlsl"

/// Standard BSDF implementation
/// Multi-lobe BSDF compatible with RTXPT's StandardBSDF
struct StandardBSDF
{
    // === Lobe weights ===
    float diffuse_weight;
    float specular_weight;
    float transmission_weight;
    float fuzz_weight;
    float coat_weight;

    // === Material parameters ===
    float roughness;
    float anisotropy;
    float ior;
    float3 base_color;
    float3 specular_f0;
    float fuzz_roughness;

    // === Lobe configuration ===
    uint active_lobes;

    // === Helper functions ===
    bool has_lobe(LobeType type)
    {
        return (active_lobes & (uint)type) != 0;
    }

    float active_lobe_weight(LobeType type, float weight)
    {
        return has_lobe(type) ? max(0.0, weight) : 0.0;
    }

    float compute_total_weight()
    {
        return active_lobe_weight(LOBE_DIFFUSE_REFLECTION, diffuse_weight)
            + active_lobe_weight(LOBE_SPECULAR_REFLECTION, specular_weight)
            + active_lobe_weight(LOBE_SPECULAR_TRANSMISSION, transmission_weight)
            + active_lobe_weight(LOBE_FUZZ, fuzz_weight)
            + active_lobe_weight(LOBE_COAT, coat_weight);
    }

    float ggx_ndf(float ndoth, float alpha2)
    {
        float denom = ndoth * ndoth * (alpha2 - 1.0) + 1.0;
        return alpha2 / max(M_PI * denom * denom, 1e-8);
    }

    float smith_g1_ggx(float ndotw, float alpha2)
    {
        float c = saturate(ndotw);
        if (c <= 0.0)
            return 0.0;

        float c2 = c * c;
        float tan2 = max(0.0, (1.0 - c2) / max(c2, 1e-8));
        return 2.0 / (1.0 + sqrt(1.0 + alpha2 * tan2));
    }

    float lobe_selection_pdf(LobeType type, float weight)
    {
        float total_weight = compute_total_weight();
        if (total_weight <= 0.0 || !has_lobe(type))
            return 0.0;

        return max(0.0, weight) / total_weight;
    }

    // === Lambert diffuse BRDF ===
    BsdfLobeResult evaluate_diffuse_lambert(BsdfEvalData data)
    {
        BsdfLobeResult result;
        result.value = base_color * M_FRAC_1_PI;
        result.pdf = data.ndotl * M_FRAC_1_PI;
        result.weight = diffuse_weight;
        return result;
    }

    BsdfSampleResult sample_diffuse_lambert(float3 wo, float2 urand)
    {
        BsdfSampleResult result;
        result.selected_lobe = LOBE_DIFFUSE_REFLECTION;

        // Cosine-weighted hemisphere sampling
        float phi = urand.x * M_TAU;
        float cos_theta = sqrt(max(0.0, 1.0 - urand.y));
        float sin_theta = sqrt(max(0.0, 1.0 - cos_theta * cos_theta));

        result.wi = float3(cos(phi) * sin_theta, sin(phi) * sin_theta, cos_theta);
        result.pdf = cos_theta * M_FRAC_1_PI;
        result.lobe_value = base_color * M_FRAC_1_PI;
        result.value = result.lobe_value / max(result.pdf, 1e-8);
        result.approx_roughness = 1.0;
        result.lobe_pdf = result.pdf;

        return result;
    }

    // === GGX specular BRDF ===
    BsdfLobeResult evaluate_specular_ggx(BsdfEvalData data)
    {
        BsdfLobeResult result;

        if (data.ndotl <= 0.0 || data.ndotv <= 0.0)
        {
            return BsdfLobeResult::invalid();
        }

        float alpha = roughness * roughness;
        float alpha2 = alpha * alpha;

        // GGX NDF
        float ndoth = saturate(data.half_vector.z);
        float d = ggx_ndf(ndoth, alpha2);

        // Smith masking-shadowing
        float g1_wo = smith_g1_ggx(data.ndotv, alpha2);
        float g1_wi = smith_g1_ggx(data.ndotl, alpha2);
        float g = g1_wo * g1_wi;

        // Fresnel (Schlick approximation)
        float vdoth = saturate(dot(data.wo, data.half_vector));
        float3 f = specular_f0 + (1.0 - specular_f0) * pow(1.0 - vdoth, 5.0);

        // Combined BRDF
        result.value = d * g * f / max(4.0 * data.ndotv * data.ndotl, 1e-8);

        // PDF with VNDF reflection sampling, with the half-vector reflection Jacobian applied.
        result.pdf = d * g1_wo / max(4.0 * data.ndotv, 1e-8);

        result.weight = specular_weight;
        return result;
    }

    BsdfSampleResult sample_specular_ggx_vndf(float3 wo, float2 urand)
    {
        BsdfSampleResult result;
        result.selected_lobe = LOBE_SPECULAR_REFLECTION;

        float alpha = roughness * roughness;

        // VNDF sampling for GGX
        float3 vh = normalize(float3(alpha * wo.x, alpha * wo.y, wo.z));

        float3 t1 = (vh.z < 0.9999) ? normalize(cross(float3(0, 0, 1), vh)) : float3(1, 0, 0);
        float3 t2 = cross(vh, t1);

        float r = sqrt(urand.x);
        float phi = 2.0 * M_PI * urand.y;
        float t1_val = r * cos(phi);
        float t2_val = r * sin(phi);
        float s = 0.5 * (1.0 + vh.z);
        t2_val = (1.0 - s) * sqrt(max(0.0, 1.0 - t1_val * t1_val)) + s * t2_val;

        float3 nh = t1_val * t1 + t2_val * t2 + sqrt(max(0.0, 1.0 - t1_val * t1_val - t2_val * t2_val)) * vh;
        float3 h = normalize(float3(alpha * nh.x, alpha * nh.y, max(0.0, nh.z)));

        // Reflect wo around h
        result.wi = reflect(-wo, h);

        // Check valid sample
        if (result.wi.z <= 1e-6 || wo.z <= 1e-6)
        {
            return BsdfSampleResult::invalid();
        }

        // Compute PDF
        float alpha2 = alpha * alpha;
        float g1_wo = smith_g1_ggx(wo.z, alpha2);
        float d = ggx_ndf(saturate(h.z), alpha2);
        result.pdf = d * g1_wo / max(4.0 * wo.z, 1e-8);

        // Compute value
        BsdfEvalData eval_data = BsdfEvalData::create(wo, result.wi, float3(0, 0, 1), float3(0, 0, 1));
        BsdfLobeResult lobe_result = evaluate_specular_ggx(eval_data);

        result.lobe_value = lobe_result.value;
        result.value = result.lobe_value / max(result.pdf, 1e-8);
        result.approx_roughness = roughness;
        result.lobe_pdf = result.pdf;

        return result;
    }

    // === Fuzz (Charlie) BRDF ===
    BsdfLobeResult evaluate_fuzz_charlie(BsdfEvalData data)
    {
        BsdfLobeResult result;

        if (data.ndotl <= 0.0 || data.ndotv <= 0.0)
        {
            return BsdfLobeResult::invalid();
        }

        // Simplified Charlie/sheen BRDF
        float ndoth = saturate(data.half_vector.z);
        float sheen = pow(ndoth, fuzz_roughness) * (1.0 + fuzz_roughness);

        // View-dependent sheen intensity
        float view_factor = pow(1.0 - data.ndotv, 3.0);
        float light_factor = pow(1.0 - data.ndotl, 3.0);

        result.value = sheen * view_factor * light_factor * M_FRAC_1_PI;
        result.pdf = data.ndotl * M_FRAC_1_PI;
        result.weight = fuzz_weight;

        return result;
    }

    BsdfSampleResult sample_fuzz_charlie(float3 wo, float2 urand)
    {
        BsdfSampleResult result;
        result.selected_lobe = LOBE_FUZZ;

        // Cosine-weighted sampling keeps the lobe PDF consistent with evaluate().
        float phi = urand.x * M_TAU;
        float cos_theta = sqrt(max(0.0, 1.0 - urand.y));
        float sin_theta = sqrt(max(0.0, 1.0 - cos_theta * cos_theta));

        result.wi = float3(cos(phi) * sin_theta, sin(phi) * sin_theta, cos_theta);
        result.pdf = cos_theta * M_FRAC_1_PI;
        result.lobe_value = pow(cos_theta, fuzz_roughness) * M_FRAC_1_PI;
        result.value = result.lobe_value / max(result.pdf, 1e-8);
        result.approx_roughness = fuzz_roughness;
        result.lobe_pdf = result.pdf;

        return result;
    }

    // === Complete BSDF evaluation ===
    BsdfEvalResult evaluate(BsdfEvalData data)
    {
        BsdfEvalResult result = BsdfEvalResult::invalid();
        float total_weight = compute_total_weight();
        if (total_weight <= 0.0)
            return result;

        if (has_lobe(LOBE_DIFFUSE_REFLECTION))
        {
            BsdfLobeResult diffuse = evaluate_diffuse_lambert(data);
            result.diffuse_value = diffuse.value * diffuse_weight;
            result.value += result.diffuse_value;
            result.pdf += diffuse.pdf * lobe_selection_pdf(LOBE_DIFFUSE_REFLECTION, diffuse_weight);
        }

        if (has_lobe(LOBE_SPECULAR_REFLECTION))
        {
            BsdfLobeResult specular = evaluate_specular_ggx(data);
            result.specular_value = specular.value * specular_weight;
            result.value += result.specular_value;
            result.pdf += specular.pdf * lobe_selection_pdf(LOBE_SPECULAR_REFLECTION, specular_weight);
        }

        if (has_lobe(LOBE_FUZZ))
        {
            BsdfLobeResult fuzz = evaluate_fuzz_charlie(data);
            result.fuzz_value = fuzz.value * fuzz_weight;
            result.value += result.fuzz_value;
            result.pdf += fuzz.pdf * lobe_selection_pdf(LOBE_FUZZ, fuzz_weight);
        }

        return result;
    }

    // === BSDF sampling with lobe selection ===
    BsdfSampleResult sample(float3 wo, float3 urand)
    {
        // Compute lobe probabilities
        float total_weight = compute_total_weight();
        if (total_weight <= 0.0)
        {
            return BsdfSampleResult::invalid();
        }

        float diffuse_p = active_lobe_weight(LOBE_DIFFUSE_REFLECTION, diffuse_weight) / total_weight;
        float specular_p = active_lobe_weight(LOBE_SPECULAR_REFLECTION, specular_weight) / total_weight;
        float fuzz_p = active_lobe_weight(LOBE_FUZZ, fuzz_weight) / total_weight;

        BsdfSampleResult result;
        float lobe_xi = urand.z;

        if (diffuse_p > 0.0 && lobe_xi < diffuse_p)
        {
            result = sample_diffuse_lambert(wo, urand.xy);
        }
        else if (specular_p > 0.0 && lobe_xi < diffuse_p + specular_p)
        {
            result = sample_specular_ggx_vndf(wo, urand.xy);
        }
        else if (fuzz_p > 0.0)
        {
            result = sample_fuzz_charlie(wo, urand.xy);
        }
        else
        {
            return BsdfSampleResult::invalid();
        }

        if (!result.is_valid())
            return BsdfSampleResult::invalid();

        BsdfEvalData eval_data = BsdfEvalData::create(wo, result.wi, float3(0, 0, 1), float3(0, 0, 1));
        BsdfEvalResult eval = evaluate(eval_data);
        if (eval.pdf <= 0.0 || !any(eval.value > 0.0))
            return BsdfSampleResult::invalid();

        result.lobe_value = eval.value;
        result.pdf = eval.pdf;
        result.value = eval.value / max(eval.pdf, 1e-8);

        return result;
    }

    // === Create from MaterialData ===
    static StandardBSDF from_material(MaterialData mat)
    {
        StandardBSDF bsdf;

        // Extract weights from material
        bsdf.diffuse_weight = saturate(1.0 - mat.metalness_factor) * saturate(1.0 - mat.transmission_weight);
        bsdf.specular_weight = mat.specular_weight;
        bsdf.transmission_weight = mat.transmission_weight;
        bsdf.fuzz_weight = mat.fuzz_weight;
        bsdf.coat_weight = 0.0;  // TODO: add coat support

        // Extract material parameters
        bsdf.roughness = mat.roughness_mult;
        bsdf.anisotropy = mat.anisotropy;
        bsdf.ior = mat.specular_ior;
        bsdf.base_color = mat.base_color_mult.rgb;
        bsdf.specular_f0 = mat.specular_color.rgb * dielectric_f0_from_ior(mat.specular_ior);
        bsdf.fuzz_roughness = mat.fuzz_roughness;

        // Configure active lobes
        bsdf.active_lobes = (uint)LOBE_DIFFUSE_REFLECTION | (uint)LOBE_SPECULAR_REFLECTION;
        if (mat.fuzz_weight > 0.0)
            bsdf.active_lobes = bsdf.active_lobes | (uint)LOBE_FUZZ;
        if (mat.transmission_weight > 0.0)
            bsdf.active_lobes = bsdf.active_lobes | (uint)LOBE_SPECULAR_TRANSMISSION;

        return bsdf;
    }

    static float dielectric_f0_from_ior(float ior)
    {
        const float f = (ior - 1.0) / (ior + 1.0);
        return f * f;
    }
};

#endif // MATERIALS_STANDARD_BSDF_HLSL
