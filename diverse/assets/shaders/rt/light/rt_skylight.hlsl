
#ifndef RT_SKYLIGHT_HLSL
#define RT_SKYLIGHT_HLSL
// #include "../inc/math_const.hlsl"
// #include "../inc/monte_carlo.hlsl"
// #include "rt_light_common.hlsl"

#ifndef USE_HIERARCHICAL_IMPORTANCE_SAMPLING
#define USE_HIERARCHICAL_IMPORTANCE_SAMPLING 1
#endif

// shader parameters for skylight integration

// [[vk::binding(40)]] Texture2D sky_light_tex;
// [[vk::binding(41)]] Texture2D<float> sky_light_pdf;
// [[vk::binding(42)]] SamplerState sky_light_texSampler;

// float sky_light_inv_res;
// int sky_light_mip_count;

float skyLight_estimate()
{
    // cancels out constant factor in pdf below
#if USE_HIERARCHICAL_IMPORTANCE_SAMPLING
    return 4 * M_PI * sky_light_pdf.Load(int3(0, 0, sky_light_mip_count - 1));
#else
    return 4 * M_PI;
#endif
}

// Returns: radiance and Pdf for the specified direction
float4 skylight_eval_Light(float3 Dir)
{
    // NOTE: assumes direction is normalized
    float2 uv = inverse_equi_area_spherical_mapping(Dir.yzx);
    float4 result = sky_light_tex.SampleLevel(sampler_llc, uv, 0);
    float3 radiance = result.xyz;
#if USE_HIERARCHICAL_IMPORTANCE_SAMPLING
    float Pdf = result.w > 0 ? result.w / (4 * M_PI * sky_light_pdf.Load(int3(0, 0, sky_light_mip_count - 1))) : 0.0;
#else
    float Pdf = 1.0 / (4.0 * PI);
#endif
    return float4(radiance, Pdf);
}

struct SkyLightSample {
    float3 direction;
    float3 radiance;
    float pdf;
};

SkyLightSample skyLight_sample_light(float2 rand_sample)
{
#if USE_HIERARCHICAL_IMPORTANCE_SAMPLING
    float2 uv = rand_sample;

    int3 px = int3(0, 0, sky_light_mip_count - 2);
    for (; px.z >= 0; px.z--)
    {
        px.xy *= 2;
        // TODO: would be nice to have GatherRed available to do this in a single lookup ...
        float P00 = sky_light_pdf.Load(px + int3(0, 0, 0));
        float P10 = sky_light_pdf.Load(px + int3(1, 0, 0));
        float P01 = sky_light_pdf.Load(px + int3(0, 1, 0));
        float P11 = sky_light_pdf.Load(px + int3(1, 1, 0));

        float L = P00 + P01;
        float R = P10 + P11;

        float ProbX = L / (L + R);
        if (uv.x < ProbX)
        {
            uv.x /= ProbX;
            float ProbY = P00 / L;
            if (uv.y < ProbY)
            {
                uv.y /= ProbY;
            }
            else
            {
                px.y++;
                uv.y = (uv.y - ProbY) / (1 - ProbY);
            }
        }
        else
        {
            px.x++;
            uv.x = (uv.x - ProbX) / (1 - ProbX);
            float ProbY = P10 / R;
            if (uv.y < ProbY)
            {
                uv.y /= ProbY;
            }
            else
            {
                px.y++;
                uv.y = (uv.y - ProbY) / (1 - ProbY);
            }
        }
    }

    px.z = 0;
    float4 result = sky_light_tex.Load(px);
    float3 radiance = result.xyz;
    float out_pdf = result.w / (4 * M_PI * sky_light_pdf.Load(int3(0, 0, sky_light_mip_count - 1)));

    uv = (float2(px.xy) + uv) * sky_light_inv_res.xy;

    SkyLightSample sample;
    sample.direction = equi_area_spherical_mapping(uv).zxy;
    sample.radiance = radiance;
    sample.pdf = out_pdf;
    return sample;
#else
    SkyLightSample sample;

    sample.direction = uniform_sample_sphere(rand_sample).xyz;
    sample.pdf = 1.0 / (4.0 * M_PI);
    float2 uv = inverse_equi_area_spherical_mapping(sample.direction.yzx);
    sample.radiance = sky_light_tex.SampleLevel(sampler_llc, uv, 0).xyz;
    return sample;
#endif
}

// By default, assume MIS is performed in the integrator.
// However in some cases like RTGI we may want to have _some_ basic MIS
// without introducing complexity in the integrator.
#ifndef PATHTRACING_SKY_MIS
#define PATHTRACING_SKY_MIS 0
#endif

float skylight_estimate_light(
    int light_id,
    float3 ws_normal,
    float3 ws_pos
)
{
#if PATHTRACING_SKY_MIS == 0
    return skyLight_estimate();
#else
    return 2 * PI;
#endif
}

LightHit skylight_trace_light(RayDesc Ray, int LightId)
{
    if (Ray.TMax == POSITIVE_INFINITY)
    {
        float4 result = skylight_eval_Light(Ray.Direction);
        return create_light_hit(result.xyz, result.w, POSITIVE_INFINITY);
    }
    return null_light_hit();
}

LightSample skylight_sample_light(
    int LightId,
    float2 rand_sample,
    float3 ws_normal,
    float3 ws_pos
)
{
#if PATHTRACING_SKY_MIS == 0
    SkyLightSample sample = skyLight_sample_light(rand_sample);
    return create_light_sample(sample.radiance / sample.pdf, sample.pdf, sample.direction, POSITIVE_INFINITY);
#else
    // account for the fact that with MIS compensation, we could have a 0 probability of choosing the sky
    float skylight_sampling_strategy_pdf = skylight_estimate() > 0.0 ? 0.5 : 0.0;

    // Do a simple one-sample MIS between the skylight and cosine sampling on the assumption that
    // the shading loop is only sampling the light and not doing its own MIS.
    float3 skylight_radiance = 0;
    float3 direction = 0;
    float cosine_pdf = 0;
    float sky_light_pdf = 0;
    [[branch]]
    if (rand_sample.x < skylight_sampling_strategy_pdf)
    {
        rand_sample.x /= skylight_sampling_strategy_pdf;

        SkyLightSample sky_sample = skylight_sample_light(rand_sample);

        skylight_radiance = sky_sample.radiance;
        direction = sky_sample.direction;
        sky_light_pdf = sky_sample.Pdf;
        cosine_pdf = saturate(dot(ws_normal, direction)) / PI;
    }
    else
    {
        rand_sample.x = (rand_sample.x - skylight_sampling_strategy_pdf) / (1.0 - skylight_sampling_strategy_pdf);

        float4 cos_sample = cosine_sample_hemisphere(rand_sample, ws_normal);

        direction = cos_sample.xyz;
        cosine_pdf = cos_sample.w;

        float4 sky_eval = skylight_eval_Light(direction);
        skylight_radiance = SkyEval.xyz;
        sky_light_pdf = SkyEval.w;
        
    }
    float3 position = ws_pos + direction * SKY_DIST;
    float pdf = lerp(cosine_pdf, sky_light_pdf, skylight_sampling_strategy_pdf);

    return create_light_sample(skylight_radiance, pdf, position, -direction);
#endif
}

#endif