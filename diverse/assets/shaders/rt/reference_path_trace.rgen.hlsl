#include "../inc/binding.hlsl"
#include "../inc/math.hlsl"
#include "../inc/samplers.hlsl"
#include "../inc/frame_constants.hlsl"
#include "../inc/gbuffer.hlsl"
#include "../inc/rt.hlsl"
#include "../inc/random.hlsl"
#include "../inc/bindless.hlsl"
#include "../materials/standard_bsdf.hlsl"
#include "../lighting/sun_light.hlsl"

DS_RT_ACCELERATION(0, DS_DESCRIPTOR_SET_ACCELERATION) RaytracingAccelerationStructure acceleration_structure;

DS_RESOURCE(0) RWTexture2D<float4> output_tex;
DS_RESOURCE(1) RWTexture2D<float> depth_tex;
DS_RESOURCE(2) TextureCube<float4> sky_cube_tex;

#include "../lighting/env_light.hlsl"

static const uint MAX_EYE_PATH_LENGTH = 8;
static const uint RUSSIAN_ROULETTE_START_PATH_LENGTH = 3;
static const float REFERENCE_PT_FIREFLY_FILTER_THRESHOLD = 256.0;

float dielectric_f0_from_eta(float eta)
{
    float f = (eta - 1.0) / (eta + 1.0);
    return f * f;
}

StandardBSDF make_surface_bsdf(MaterialData material, GbufferData gbuffer)
{
    StandardBSDF bsdf;

    bsdf.base_color = max(0.0.xxx, gbuffer.albedo);
    bsdf.roughness = clamp(gbuffer.roughness, 1e-4, 1.0);
    bsdf.anisotropy = material.anisotropy;
    bsdf.ior = max(1.0, material.specular_ior);
    bsdf.fuzz_roughness = max(1e-4, material.fuzz_roughness);

    float dielectric_f0 = dielectric_f0_from_eta(bsdf.ior);
    float3 dielectric_specular = dielectric_f0 * material.specular_weight * material.specular_color.rgb;
    bsdf.specular_f0 = lerp(dielectric_specular, bsdf.base_color, saturate(gbuffer.metalness));

    // Transmission is represented in MaterialData, but the current BSDF sampler
    // does not implement a transmission lobe yet. Keep the ABI ready without
    // allowing random lobe selection to pick an unsupported path.
    bsdf.transmission_weight = 0.0;
    bsdf.coat_weight = 0.0;
    bsdf.diffuse_weight = saturate(1.0 - gbuffer.metalness) * saturate(1.0 - material.transmission_weight);
    bsdf.specular_weight = max(0.0, material.specular_weight);
    bsdf.fuzz_weight = max(0.0, material.fuzz_weight);

    bsdf.active_lobes = (uint)LOBE_NONE;
    if (bsdf.diffuse_weight > 0.0)
        bsdf.active_lobes = bsdf.active_lobes | (uint)LOBE_DIFFUSE_REFLECTION;
    if (bsdf.specular_weight > 0.0)
        bsdf.active_lobes = bsdf.active_lobes | (uint)LOBE_SPECULAR_REFLECTION;
    if (bsdf.fuzz_weight > 0.0)
        bsdf.active_lobes = bsdf.active_lobes | (uint)LOBE_FUZZ;

    return bsdf;
}

SunLight make_frame_sun()
{
    SunLight sun = SunLight::make();
    sun.direction = normalize(frame_constants.sun_direction.xyz);
    sun.color = 20.0 * frame_constants.sun_color_multiplier.rgb * frame_constants.pre_exposure;
    sun.angular_radius = max(acos(saturate(frame_constants.sun_angular_radius_cos)), 1e-4);
    sun.distance = FLT_MAX;
    return sun;
}

bool trace_visibility(RayDesc ray)
{
    ShadowRayPayload shadow_payload = ShadowRayPayload::new_hit();
    TraceRay(
        acceleration_structure,
        RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER,
        0xff,
        1,
        0,
        1,
        ray,
        shadow_payload);

    return !shadow_payload.is_shadowed;
}

GbufferPathVertex trace_gbuffer(RayDesc ray, RayCone ray_cone, uint path_length, bool cull_back_faces)
{
    GbufferRayPayload payload = GbufferRayPayload::new_miss();
    payload.ray_cone = ray_cone;
    payload.path_length = path_length;

    uint trace_flags = 0;
    if (cull_back_faces)
        trace_flags |= RAY_FLAG_CULL_BACK_FACING_TRIANGLES;

    TraceRay(acceleration_structure, trace_flags, 0xff, 0, 0, 0, ray, payload);

    GbufferPathVertex result;
    result.is_hit = payload.is_hit();
    result.gbuffer_packed = payload.gbuffer_packed;
    result.ray_t = payload.t;
    result.position = ray.Origin + ray.Direction * payload.t;
    result.material_id = payload.material_id;
    return result;
}

float3 evaluate_sun_nee(
    StandardBSDF bsdf,
    float3x3 tangent_to_world,
    float3 position,
    float3 normal,
    float3 wo,
    inout Random rng)
{
    SunLight sun = make_frame_sun();
    LightSample light_sample = sun.sample(float2(uniform_rand_float(rng), uniform_rand_float(rng)));
    light_sample.Li = sun.evaluate();

    float3 wi = mul(light_sample.direction, tangent_to_world);
    if (wi.z <= 0.0)
        return 0.0.xxx;

    RayDesc shadow_ray = new_ray(offset_position(position, normal), light_sample.direction, 1e-3, FLT_MAX);
    if (!trace_visibility(shadow_ray))
        return 0.0.xxx;

    BsdfEvalData eval_data = BsdfEvalData::create(wo, wi, float3(0, 0, 1), float3(0, 0, 1));
    BsdfEvalResult eval = bsdf.evaluate(eval_data);
    return eval.value * light_sample.Li * wi.z;
}

float3 evaluate_environment_nee(
    StandardBSDF bsdf,
    float3x3 tangent_to_world,
    float3 position,
    float3 normal,
    float3 wo,
    float firefly_filter_k,
    inout Random rng)
{
    EnvironmentLightSample light_sample = sample_environment_light_nee(
        normal,
        float2(uniform_rand_float(rng), uniform_rand_float(rng)));

    if (!light_sample.valid())
        return 0.0.xxx;

    float3 wi = mul(light_sample.direction, tangent_to_world);
    if (wi.z <= 0.0)
        return 0.0.xxx;

    RayDesc shadow_ray = new_ray(offset_position(position, normal), light_sample.direction, 1e-3, FLT_MAX);
    if (!trace_visibility(shadow_ray))
        return 0.0.xxx;

    BsdfEvalData eval_data = BsdfEvalData::create(wo, wi, float3(0, 0, 1), float3(0, 0, 1));
    BsdfEvalResult eval = bsdf.evaluate(eval_data);
    if (eval.pdf <= 0.0 || !any(eval.value > 0.0))
        return 0.0.xxx;

    float mis_weight = nee_balance_mis(light_sample.pdf, eval.pdf);
    float3 contribution = eval.value * light_sample.Li * wi.z * mis_weight / max(light_sample.pdf, 1e-8);
    return apply_firefly_filter(contribution, REFERENCE_PT_FIREFLY_FILTER_THRESHOLD, firefly_filter_k);
}

float update_firefly_filter_k(float current_k, float bounce_pdf)
{
    if (bounce_pdf <= 0.0)
        return current_k;

    float angle = 2.0 * acos(max(-1.0, 1.0 - (1.0 / bounce_pdf) / (2.0 * M_PI)));
    float p = 32.0 / (32.0 + angle * angle);
    return max(1e-5, current_k * p);
}

bool valid_radiance(float3 v)
{
    return all(v >= 0.0) && all(v < 1e20);
}

[shader("raygeneration")]
void main()
{
    const uint2 px = DispatchRaysIndex().xy;
    const uint2 dims = DispatchRaysDimensions().xy;

    Random rng = make_random(px, frame_constants.frame_index);

    float4 prev = output_tex[px];
    float2 jitter = float2(uniform_rand_float(rng), uniform_rand_float(rng)) - 0.5.xx;
    float2 uv = (float2(px) + 0.5.xx + jitter) / float2(dims);

    ViewRayContext view_ray_context = ViewRayContext::from_uv(uv);
    RayDesc ray = new_ray(
        view_ray_context.ray_origin_ws(),
        view_ray_context.ray_dir_ws(),
        0.0,
        FLT_MAX);

    RayCone ray_cone = pixel_ray_cone_from_image_height(dims.y);
    ray_cone.spread_angle *= 0.3;

    float depth = FARZ;
    float3 throughput = 1.0.xxx;
    float3 radiance = 0.0.xxx;
    float previous_bsdf_pdf = 0.0;
    float previous_environment_nee_pdf = 0.0;
    float firefly_filter_k = 1.0;

    [loop]
    for (uint path_length = 0; path_length < MAX_EYE_PATH_LENGTH; ++path_length)
    {
        GbufferPathVertex hit = trace_gbuffer(ray, ray_cone, path_length, false);

        if (!hit.is_hit)
        {
            float mis_weight = 1.0;
            if (previous_bsdf_pdf > 0.0 && previous_environment_nee_pdf > 0.0)
                mis_weight = nee_balance_mis(previous_bsdf_pdf, previous_environment_nee_pdf);

            float3 environment_emission = sample_environment_light(ray.Direction) * mis_weight;
            environment_emission = apply_firefly_filter(
                environment_emission,
                REFERENCE_PT_FIREFLY_FILTER_THRESHOLD,
                firefly_filter_k);
            radiance += throughput * environment_emission;
            break;
        }

        ray_cone = ray_cone.propagate(0.0, hit.ray_t);

        GbufferData gbuffer = hit.gbuffer_packed.unpack();
        if (dot(gbuffer.normal, ray.Direction) > 0.0)
            gbuffer.normal *= -1.0;

        if (path_length == 0)
        {
            float4 clip = mul(float4(hit.position, 1.0), frame_constants.view_constants.world_to_clip);
            depth = clip.z / clip.w;
        }

        radiance += throughput * gbuffer.emissive;

        if (hit.material_id == 0xffffffff)
            break;

        MaterialData material = materials_buffer[hit.material_id];
        StandardBSDF bsdf = make_surface_bsdf(material, gbuffer);
        if (bsdf.active_lobes == (uint)LOBE_NONE)
            break;

        float3x3 tangent_to_world = build_orthonormal_basis(gbuffer.normal);
        float3 wo = mul(-ray.Direction, tangent_to_world);
        if (wo.z <= 0.0)
        {
            wo.z = abs(wo.z);
            wo = normalize(wo);
        }

        radiance += throughput * evaluate_sun_nee(
            bsdf,
            tangent_to_world,
            hit.position,
            gbuffer.normal,
            wo,
            rng);

        radiance += throughput * evaluate_environment_nee(
            bsdf,
            tangent_to_world,
            hit.position,
            gbuffer.normal,
            wo,
            firefly_filter_k,
            rng);

        BsdfSampleResult bsdf_sample = bsdf.sample(
            wo,
            float3(
                uniform_rand_float(rng),
                uniform_rand_float(rng),
                uniform_rand_float(rng)));

        if (!bsdf_sample.is_valid())
            break;

        float cos_theta = max(0.0, bsdf_sample.wi.z);
        throughput *= bsdf_sample.value * cos_theta;

        if (!valid_radiance(throughput) || max3(throughput.x, throughput.y, throughput.z) <= 1e-5)
            break;

        float3 next_direction = normalize(mul(tangent_to_world, bsdf_sample.wi));
        previous_bsdf_pdf = bsdf_sample.pdf;
        previous_environment_nee_pdf = environment_nee_pdf(gbuffer.normal, next_direction);
        firefly_filter_k = update_firefly_filter_k(firefly_filter_k, bsdf_sample.pdf);
        ray = new_ray(offset_position(hit.position, gbuffer.normal), next_direction, 1e-3, FLT_MAX);

        if (path_length >= RUSSIAN_ROULETTE_START_PATH_LENGTH)
        {
            float continue_p = saturate(max3(throughput.x, throughput.y, throughput.z));
            if (uniform_rand_float(rng) > continue_p)
                break;
            throughput /= max(continue_p, 1e-3);
        }
    }

    if (!valid_radiance(radiance))
        radiance = 0.0.xxx;

    float sample_count = prev.w + 1.0;
    float3 accumulated = lerp(prev.rgb, radiance, 1.0 / sample_count);

    output_tex[px] = float4(accumulated, sample_count);
    depth_tex[px] = depth;
}
