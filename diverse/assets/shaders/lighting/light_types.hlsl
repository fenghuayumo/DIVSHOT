#ifndef LIGHTING_LIGHT_TYPES_HLSL
#define LIGHTING_LIGHT_TYPES_HLSL

#include "../inc/math.hlsl"

/// Direct-light sample used by NEE, ReSTIR DI/GI, and hybrid shading.
struct LightSample
{
    float3 Li;
    float3 direction;
    float distance;
    uint light_index;
    float selection_pdf;
    float solid_angle_pdf;
    bool light_sampleable_by_bsdf;

    static LightSample invalid()
    {
        LightSample ret;
        ret.Li = 0.0.xxx;
        ret.direction = 0.0.xxx;
        ret.distance = 0.0;
        ret.light_index = 0xffffffff;
        ret.selection_pdf = 0.0;
        ret.solid_angle_pdf = 0.0;
        ret.light_sampleable_by_bsdf = false;
        return ret;
    }

    bool valid()
    {
        return any(Li > 0.0);
    }

    float combined_pdf()
    {
        return selection_pdf * solid_angle_pdf;
    }
};

/// Weighted reservoir for NEE-AT / ReSTIR-style candidate filtering.
struct NeeReservoir
{
    LightSample sample;
    float weight_sum;
    float selected_weight;

    static NeeReservoir make()
    {
        NeeReservoir ret;
        ret.sample = LightSample::invalid();
        ret.weight_sum = 0.0;
        ret.selected_weight = 0.0;
        return ret;
    }

    void add(float random_value, LightSample candidate, float candidate_weight)
    {
        if (candidate_weight <= 0.0)
            return;

        weight_sum += candidate_weight;
        if (random_value < saturate(candidate_weight / max(weight_sum, 1e-8)))
        {
            sample = candidate;
            selected_weight = candidate_weight;
        }
    }

    bool valid()
    {
        return weight_sum > 0.0 && selected_weight > 0.0 && sample.valid();
    }
};

float nee_balance_mis(float this_pdf, float other_pdf)
{
    return this_pdf / max(this_pdf + other_pdf, 1e-8);
}

#endif // LIGHTING_LIGHT_TYPES_HLSL
