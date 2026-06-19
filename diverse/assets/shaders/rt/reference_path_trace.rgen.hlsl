#include "../inc/binding.hlsl"
#include "../inc/math.hlsl"
#include "../inc/samplers.hlsl"
#include "../inc/frame_constants.hlsl"
#include "../inc/gbuffer.hlsl"
#include "../inc/rt.hlsl"
#include "../inc/random.hlsl"
#include "../inc/bindless.hlsl"
#include "../inc/color/srgb.hlsl"
#include "../materials/standard_bsdf.hlsl"
#include "../lighting/sun_light.hlsl"

#ifndef IES_SAMPLER
#define IES_SAMPLER sampler_llc
#endif

#ifndef ENVIRONMENT_SAMPLER
#define ENVIRONMENT_SAMPLER sampler_llc
#endif

#include "../inc/lights/light_common.hlsl"

DS_RT_ACCELERATION(0, DS_DESCRIPTOR_SET_ACCELERATION) RaytracingAccelerationStructure acceleration_structure;

DS_RESOURCE(0) RWTexture2D<float4> output_tex;
DS_RESOURCE(1) RWTexture2D<float> depth_tex;
DS_RESOURCE(2) TextureCube<float4> sky_cube_tex;
DS_RESOURCE(3) Texture2D<float> sky_pdf_tex;

#include "../lighting/env_light.hlsl"

static const uint MAX_EYE_PATH_LENGTH = 8;
static const uint RUSSIAN_ROULETTE_START_PATH_LENGTH = 3;
static const uint REFERENCE_PT_NEE_CANDIDATE_COUNT = 8;
static const float REFERENCE_PT_FIREFLY_FILTER_THRESHOLD = 256.0;
static const float SPECULAR_ROUGHNESS_DIFFUSE_THRESHOLD = 0.25;

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
    bsdf.fuzz_color = max(0.0.xxx, material.fuzz_color.rgb);
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
    if (light_sample.solid_angle_pdf <= 0.0)
        return 0.0.xxx;

    float3 wi = mul(light_sample.direction, tangent_to_world);
    if (wi.z <= 0.0)
        return 0.0.xxx;

    RayDesc shadow_ray = new_ray(offset_position(position, normal), light_sample.direction, 1e-3, FLT_MAX);
    if (!trace_visibility(shadow_ray))
        return 0.0.xxx;

    BsdfEvalData eval_data = BsdfEvalData::create(wo, wi, float3(0, 0, 1), float3(0, 0, 1));
    BsdfEvalResult eval = bsdf.evaluate(eval_data);

    // Directional lights are not BSDF-sampleable (RTXPT: LightSampleableByBSDF = false),
    // so MIS vs BSDF is 1.0 — the BSDF cannot importance-sample the solar disk.
    return eval.value * light_sample.Li * wi.z / max(light_sample.solid_angle_pdf, 1e-8);
}

bool light_type_sampleable_by_bsdf(PolymorphicLightType light_type)
{
    // All area lights with finite surface area can be sampled by BSDF
    // Triangle, Sphere, Cylinder, Disk, Rect lights have actual area
    // Point and Directional lights are delta distributions (infinite radiance / zero area)
    // Environment light is also a delta/infinite case
    return light_type == PolymorphicLightType::kTriangle
        || light_type == PolymorphicLightType::kSphere
        || light_type == PolymorphicLightType::kCylinder
        || light_type == PolymorphicLightType::kDisk
        || light_type == PolymorphicLightType::kRect;
}

bool light_type_handled_by_reference_pt_scene_nee(PolymorphicLightType light_type)
{
    // Directional lights are represented by the frame sun path. Environment is
    // handled as a sky-cube NEEAT candidate because the current environment
    // LightShaderData does not carry the resolved cube texture.
    return light_type != PolymorphicLightType::kDirectional
        && light_type != PolymorphicLightType::kEnvironment;
}

float combined_light_pdf(LightSample light_sample)
{
    return light_sample.selection_pdf * light_sample.solid_angle_pdf;
}

LightSample sample_scene_light(
    uint light_index,
    float3 position,
    float2 urand,
    float selection_pdf)
{
    LightSample ret = LightSample::make();
    PolymorphicLightInfo light_info = scene_lights_dyn[light_index];
    PolymorphicLightType light_type = getLightType(light_info);

    if (!light_type_handled_by_reference_pt_scene_nee(light_type))
        return ret;

    PolymorphicLightSample polymorphic_sample = PolymorphicLight::calcSample(light_info, urand, position);
    if (polymorphic_sample.solidAnglePdf <= 0.0 || !any(polymorphic_sample.radiance > 0.0))
        return ret;

    float3 to_light = polymorphic_sample.position - position;
    float distance_to_light = length(to_light);
    if (distance_to_light <= 1e-4)
        return ret;

    ret.Li = polymorphic_sample.radiance;
    ret.direction = to_light / distance_to_light;
    ret.distance = distance_to_light;
    ret.light_index = light_index;
    ret.selection_pdf = selection_pdf;
    ret.solid_angle_pdf = polymorphic_sample.solidAnglePdf;
    ret.light_sampleable_by_bsdf = light_type_sampleable_by_bsdf(light_type);
    ret.from_local_distribution = false;
    return ret;
}

LightSample sample_environment_light_candidate(
    float3 normal,
    float2 urand,
    float selection_pdf)
{
    LightSample ret = LightSample::make();
    EnvironmentLightSample environment_sample = sample_environment_light_nee(
        normal,
        urand,
        environment_nee_sample_mip_level());
    if (!environment_sample.valid())
        return ret;

    ret.Li = environment_sample.Li;
    ret.direction = environment_sample.direction;
    ret.distance = FLT_MAX;
    ret.light_index = 0xffffffff;
    ret.selection_pdf = selection_pdf;
    ret.solid_angle_pdf = environment_sample.pdf;
    ret.light_sampleable_by_bsdf = true;
    ret.from_local_distribution = false;
    return ret;
}

struct NeeAtReservoir
{
    LightSample sample;
    float weight_sum;
    float selected_weight;

    static NeeAtReservoir make()
    {
        NeeAtReservoir ret;
        ret.sample = LightSample::make();
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
        return weight_sum > 0.0 && selected_weight > 0.0 && sample.Valid();
    }
};

float3 evaluate_lights_neeat(
    StandardBSDF bsdf,
    float3x3 tangent_to_world,
    float3 position,
    float3 normal,
    float3 wo,
    float firefly_filter_k,
    inout Random rng)
{
    uint scene_light_count = frame_constants.scene_lights_count;
    uint direct_light_choice_count = scene_light_count + 1;
    if (direct_light_choice_count == 0)
        return 0.0.xxx;

    float light_selection_pdf = 1.0 / (float)direct_light_choice_count;
    NeeAtReservoir reservoir = NeeAtReservoir::make();

    [unroll]
    for (uint candidate_index = 0; candidate_index < REFERENCE_PT_NEE_CANDIDATE_COUNT; ++candidate_index)
    {
        float light_selector = uniform_rand_float(rng);
        uint light_choice = min((uint)(light_selector * direct_light_choice_count), direct_light_choice_count - 1);

        LightSample candidate;
        if (light_choice < scene_light_count)
        {
            candidate = sample_scene_light(
                light_choice,
                position,
                float2(uniform_rand_float(rng), uniform_rand_float(rng)),
                light_selection_pdf);
        }
        else
        {
            candidate = sample_environment_light_candidate(
                normal,
                float2(uniform_rand_float(rng), uniform_rand_float(rng)),
                light_selection_pdf);
        }

        float light_pdf = combined_light_pdf(candidate);
        if (light_pdf <= 0.0)
            continue;

        float3 wi = mul(candidate.direction, tangent_to_world);
        if (wi.z <= 0.0)
            continue;

        BsdfEvalData eval_data = BsdfEvalData::create(wo, wi, float3(0, 0, 1), float3(0, 0, 1));
        BsdfEvalResult eval = bsdf.evaluate(eval_data);
        if (eval.pdf <= 0.0 || !any(eval.value > 0.0))
            continue;

        float candidate_weight = max3(candidate.Li.x, candidate.Li.y, candidate.Li.z)
            * eval.pdf
            / max(light_pdf, 1e-8);
        reservoir.add(uniform_rand_float(rng), candidate, candidate_weight);
    }

    if (!reservoir.valid())
        return 0.0.xxx;

    LightSample light_sample = reservoir.sample;
    float light_pdf = combined_light_pdf(light_sample);
    if (light_pdf <= 0.0)
        return 0.0.xxx;

    float3 wi = mul(light_sample.direction, tangent_to_world);
    if (wi.z <= 0.0)
        return 0.0.xxx;

    RayDesc shadow_ray = new_ray(
        offset_position(position, normal),
        light_sample.direction,
        1e-3,
        max(1e-3, light_sample.distance * 0.9985));
    if (!trace_visibility(shadow_ray))
        return 0.0.xxx;

    BsdfEvalData eval_data = BsdfEvalData::create(wo, wi, float3(0, 0, 1), float3(0, 0, 1));
    BsdfEvalResult eval = bsdf.evaluate(eval_data);
    if (eval.pdf <= 0.0 || !any(eval.value > 0.0))
        return 0.0.xxx;

    float mis_weight = light_sample.light_sampleable_by_bsdf
        ? nee_balance_mis(light_pdf, eval.pdf)
        : 1.0;

    float neeat_correction = reservoir.weight_sum
        / max(reservoir.selected_weight * (float)REFERENCE_PT_NEE_CANDIDATE_COUNT, 1e-8);
    float3 contribution = eval.value
        * light_sample.Li
        * wi.z
        * mis_weight
        * neeat_correction
        / max(light_pdf, 1e-8);

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
    uint diffuse_bounces = 0;

    [loop]
    for (uint path_length = 0; path_length < MAX_EYE_PATH_LENGTH; ++path_length)
    {
        GbufferPathVertex hit = trace_gbuffer(ray, ray_cone, path_length, false);

        if (!hit.is_hit)
        {
            float mis_weight = 1.0;
            if (previous_bsdf_pdf > 0.0 && previous_environment_nee_pdf > 0.0)
                mis_weight = nee_balance_mis(previous_bsdf_pdf, previous_environment_nee_pdf);

            float3 environment_emission = sample_environment_light(
                ray.Direction,
                environment_miss_sample_mip_level(diffuse_bounces)) * mis_weight;
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

        radiance += throughput * evaluate_lights_neeat(
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
        previous_environment_nee_pdf = environment_nee_pdf(gbuffer.normal, next_direction)
            / (float)(frame_constants.scene_lights_count + 1);
        firefly_filter_k = update_firefly_filter_k(firefly_filter_k, bsdf_sample.pdf);

        bool is_diffuse_scatter = bsdf_sample.selected_lobe == LOBE_DIFFUSE_REFLECTION
            || bsdf_sample.selected_lobe == LOBE_FUZZ
            || (bsdf_sample.selected_lobe == LOBE_SPECULAR_REFLECTION
                && bsdf.roughness > SPECULAR_ROUGHNESS_DIFFUSE_THRESHOLD);
        if (is_diffuse_scatter)
            diffuse_bounces++;

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
