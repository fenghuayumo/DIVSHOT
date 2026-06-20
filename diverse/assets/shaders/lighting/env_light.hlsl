#ifndef LIGHTING_ENV_LIGHT_HLSL
#define LIGHTING_ENV_LIGHT_HLSL

#include "../inc/math.hlsl"
#include "../inc/monte_carlo.hlsl"

static const float ENVIRONMENT_LIGHT_SELECTION_PDF = 1.0;

// Match RTXPT default (SampleUI.cpp: EnvironmentMapDiffuseSampleMIPLevel = 2).
// Only mip 0 is unbiased; higher mips trade accuracy for stability on glossy-indirect paths.
#ifndef PT_ENVIRONMENT_DIFFUSE_SAMPLE_MIP_LEVEL
#define PT_ENVIRONMENT_DIFFUSE_SAMPLE_MIP_LEVEL 2.0
#endif

float environment_miss_sample_mip_level(uint diffuse_bounces)
{
    return (diffuse_bounces > 1) ? PT_ENVIRONMENT_DIFFUSE_SAMPLE_MIP_LEVEL : 0.0;
}

float environment_nee_sample_mip_level()
{
    return PT_ENVIRONMENT_DIFFUSE_SAMPLE_MIP_LEVEL;
}

struct EnvironmentLightSample
{
    float3 Li;
    float3 direction;
    float pdf;
    float distance;

    static EnvironmentLightSample invalid()
    {
        EnvironmentLightSample ret;
        ret.Li = 0.0.xxx;
        ret.direction = float3(0.0, 0.0, 1.0);
        ret.pdf = 0.0;
        ret.distance = FLT_MAX;
        return ret;
    }

    bool valid()
    {
        return pdf > 0.0 && any(Li > 0.0);
    }
};

/// Sample environment map light
float3 sample_environment_light(float3 direction, float mip_level)
{
    return sky_cube_tex.SampleLevel(sampler_llc, direction, mip_level).rgb;
}

float environment_uniform_pdf()
{
    return 1.0 / (4.0 * M_PI);
}

uint2 environment_pdf_mip_size(uint2 base_size, uint mip_level)
{
    return max(uint2(1, 1), base_size >> mip_level);
}

float environment_pdf_weight_at(uint2 texel, uint mip_level, uint2 mip_size)
{
    if (any(texel >= mip_size))
        return 0.0;

    return max(0.0, sky_pdf_tex.Load(int3(texel, mip_level)));
}

float environment_pdf_from_weight(float weight, uint2 base_size, uint mip_count)
{
    if (weight <= 0.0 || base_size.x == 0 || base_size.y == 0 || mip_count == 0)
        return 0.0;

    float root_weight = max(0.0, sky_pdf_tex.Load(int3(0, 0, mip_count - 1)));
    if (root_weight <= 0.0)
        return 0.0;

    float root_dim = (float)(1u << (mip_count - 1));
    float valid_texel_count = (float)(base_size.x * base_size.y);
    float padded_weight_sum = root_weight * root_dim * root_dim;
    return weight * valid_texel_count / max(padded_weight_sum * (4.0 * M_PI), 1e-8);
}

float environment_nee_pdf(float3 normal, float3 direction)
{
    uint width;
    uint height;
    uint mip_count;
    sky_pdf_tex.GetDimensions(0, width, height, mip_count);

    uint2 base_size = uint2(width, height);
    if (mip_count <= 1 || width == 0 || height == 0)
        return environment_uniform_pdf();

    float2 uv = inverse_equi_area_spherical_mapping(normalize(direction).yzx);
    uint2 texel = min((uint2)(uv * (float2)base_size), base_size - 1);
    float weight = environment_pdf_weight_at(texel, 0, base_size);
    return environment_pdf_from_weight(weight, base_size, mip_count);
}

EnvironmentLightSample sample_environment_light_nee(float3 normal, float2 urand, float mip_level)
{
    EnvironmentLightSample ret;

    uint width;
    uint height;
    uint mip_count;
    sky_pdf_tex.GetDimensions(0, width, height, mip_count);

    if (mip_count <= 1 || width == 0 || height == 0)
    {
        ret.direction = uniform_sample_sphere(urand);
        ret.pdf = environment_uniform_pdf() * ENVIRONMENT_LIGHT_SELECTION_PDF;
        ret.distance = FLT_MAX;
        ret.Li = sample_environment_light(ret.direction, mip_level);
        return ret;
    }

    uint2 base_size = uint2(width, height);
    uint2 texel = uint2(0, 0);
    float2 sub_texel = urand;

    for (int mip_level = (int)mip_count - 2; mip_level >= 0; --mip_level)
    {
        texel *= 2;
        uint mip = (uint)mip_level;
        uint2 mip_size = environment_pdf_mip_size(base_size, mip);

        float w00 = environment_pdf_weight_at(texel + uint2(0, 0), mip, mip_size);
        float w10 = environment_pdf_weight_at(texel + uint2(1, 0), mip, mip_size);
        float w01 = environment_pdf_weight_at(texel + uint2(0, 1), mip, mip_size);
        float w11 = environment_pdf_weight_at(texel + uint2(1, 1), mip, mip_size);
        float weight_sum = w00 + w10 + w01 + w11;
        if (weight_sum <= 0.0)
            return EnvironmentLightSample::invalid();

        float left_weight = w00 + w01;
        float right_weight = w10 + w11;
        float select_left = left_weight / weight_sum;
        bool go_left = sub_texel.x < select_left || right_weight <= 0.0;

        if (go_left)
        {
            sub_texel.x = select_left > 0.0 ? sub_texel.x / select_left : sub_texel.x;
            float select_bottom = left_weight > 0.0 ? w00 / left_weight : 1.0;
            bool go_bottom = sub_texel.y < select_bottom || w01 <= 0.0;
            if (go_bottom)
            {
                sub_texel.y = select_bottom > 0.0 ? sub_texel.y / select_bottom : sub_texel.y;
            }
            else
            {
                texel.y += 1;
                sub_texel.y = (sub_texel.y - select_bottom) / max(1.0 - select_bottom, 1e-8);
            }
        }
        else
        {
            texel.x += 1;
            sub_texel.x = (sub_texel.x - select_left) / max(1.0 - select_left, 1e-8);
            float select_bottom = right_weight > 0.0 ? w10 / right_weight : 1.0;
            bool go_bottom = sub_texel.y < select_bottom || w11 <= 0.0;
            if (go_bottom)
            {
                sub_texel.y = select_bottom > 0.0 ? sub_texel.y / select_bottom : sub_texel.y;
            }
            else
            {
                texel.y += 1;
                sub_texel.y = (sub_texel.y - select_bottom) / max(1.0 - select_bottom, 1e-8);
            }
        }
    }

    texel = min(texel, base_size - 1);
    float2 uv = ((float2)texel + saturate(sub_texel)) / (float2)base_size;
    ret.direction = normalize(equi_area_spherical_mapping(uv).zxy);
    ret.pdf = environment_nee_pdf(normal, ret.direction) * ENVIRONMENT_LIGHT_SELECTION_PDF;
    ret.distance = FLT_MAX;
    ret.Li = sample_environment_light(ret.direction, mip_level);
    return ret;
}

float3 apply_firefly_filter(float3 signal_in, float threshold, float filter_k)
{
    float max_signal = (signal_in.x + signal_in.y + signal_in.z) * (1.0 / 3.0);
    float cap = threshold * max(filter_k, 1e-5);
    if (max_signal > cap)
        signal_in *= cap / max(max_signal, 1e-8);

    return signal_in;
}

#endif // LIGHTING_ENV_LIGHT_HLSL
