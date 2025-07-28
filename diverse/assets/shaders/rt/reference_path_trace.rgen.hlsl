#include "../inc/uv.hlsl"
#include "../inc/pack_unpack.hlsl"
#include "../inc/frame_constants.hlsl"
#include "../inc/gbuffer.hlsl"
#include "../inc/brdf.hlsl"
#include "../inc/brdf_lut.hlsl"
#include "../inc/layered_brdf.hlsl"
#include "../inc/rt.hlsl"
#include "../inc/quasi_random.hlsl"
#include "../inc/bindless_textures.hlsl"
#include "../inc/atmosphere.hlsl"
#include "../inc/sun.hlsl"
// #include "../inc/lights/triangle.hlsl"
#include "../inc/utils.hlsl"
#include "../inc/lights/light_common.hlsl"
#include "../inc/light_sampler/light_uniform_sampler.hlsl"
[[vk::binding(0, 3)]] RaytracingAccelerationStructure acceleration_structure;

[[vk::binding(0)]] RWTexture2D<float4> output_tex;
[[vk::binding(1)]] RWTexture2D<float> depth_tex;
[[vk::binding(2)]] TextureCube<float4> sky_cube_tex;

bool sample_light_nee_direction(
    LightSampler light_sampler,
    float3 position,
    float3 normal,
    out RayDesc ray,
    out float light_pdf,
    out float3 radianceLi,
    out Light selected_light
) {
    uint light_index = light_sampler.sampleLights(position, normal, light_pdf);
    if (light_pdf == 0.0) {
        return false; // No light sampled
    }

    // Initialise returned radiance
    selected_light = scene_lights_dyn[light_index];

    PolymorphicLightSample light_sample = PolymorphicLight::calcSample(
        selected_light,
        light_sampler.rand2(),
        position
    );
    // Combine PDFs
    light_pdf *= light_sample.solidAnglePdf;
    float3 to_light_dir = light_sample.position - position;
    float3 lightDirection = to_light_dir;
    // Early discard lights behind surface
    if (dot(lightDirection, normal) < 0.0f || light_pdf == 0.0f)
    {
        return false;
    }
    radianceLi = light_sample.radiance;
    // Create shadow ray
    float distanceToLight = length(to_light_dir);
    float3 light_dir_normalized = normalize(to_light_dir);
    ray = new_ray(position, 
                    hasLightPosition(selected_light) ? light_dir_normalized : lightDirection,
                    1e-3, 
                    hasLightPosition(selected_light) ? distanceToLight : FLT_MAX);
    return true;
}

float3 shade_light_hit(RayDesc ray, LayeredBrdf brdf, float3x3 tangent_to_world, float3 normal, float3 wo, float3 throughput,
                     float lightPDF, float3 radianceLi, Light selectedLight)
{
    // Evaluate BRDF for new light direction and calculate combined PDF for current sample
    float samplePDF = 0.0f;
    const float3 wi = mul(ray.Direction, tangent_to_world);
    float3 sampleReflectance = brdf.evaluate_pdf(wo, wi, samplePDF);
    float3 radiance = 0.0.xxx;
    if (samplePDF != 0.0f)
    {
        bool deltaLight = isDeltaLight(selectedLight);
        float weight = (!deltaLight) ? heuristicMIS(lightPDF, samplePDF) : 1.0f;
        radiance += throughput * sampleReflectance * radianceLi * (weight / lightPDF).xxx * max(0, wi.z);
    }
    return radiance;
}

float3 shade_path_hit(RayDesc ray, 
                    float3 brdfValue,
                    float sampled_brdf_pdf,
                    float3 position, 
                    float3x3 tangent_to_world,
                    float3 normal, float3 wo,
                    float3 throughput,
                    inout LightSampler light_sampler
                    ) {
    float3 radiance = 0.0.xxx;

    return radiance;
}

float3 sample_lights_nee(LayeredBrdf brdf, LightSampler light_sampler,
                    float3 position, float3x3 tangent_to_world, float3 normal,
                    float3 wo,float3 throughput
    ) {
    float light_pdf;
    RayDesc ray;
    float3 radianceLi;
    Light selected_light;
    float3 radiance = 0.0.xxx;
    if (!sample_light_nee_direction(light_sampler, position, normal, ray, light_pdf, radianceLi, selected_light)) {
        return radiance; // No light sampled
    }
    // Trace shadow ray
    const bool is_shadowed = rt_is_shadowed(acceleration_structure, ray);
    // If nothing was hit then we have hit the light
    if (!is_shadowed)
    {
        // Normalise ray direction
        ray.Direction = normalize(ray.Direction);
        ray.TMax = FLT_MAX;

        radiance = shade_light_hit(ray, brdf, tangent_to_world, normal, wo, throughput, light_pdf, radianceLi, selected_light);
    }
    return radiance;
}

float3 sample_sun_light_nee(
    LayeredBrdf brdf,
    float3x3 tangent_to_world,
    float2 rand2,
    float3 position,
    float3 normal,
    float3 wo,
    float3 throughput) {

    const float3 to_light_norm = sample_sun_direction(rand2, true);
    const bool is_shadowed =
        rt_is_shadowed(acceleration_structure,
                       new_ray(position, to_light_norm, 1e-3, FLT_MAX));

    const float3 wi = mul(to_light_norm, tangent_to_world);
    const float3 brdf_value = brdf.evaluate_directional_light(wo, wi);
    const float3 light_radiance = is_shadowed ? 0.0 : SUN_COLOR;
    float3 total_radiance = 0.0.xxx;
    total_radiance += throughput * brdf_value * light_radiance * max(0.0, wi.z);
    return total_radiance;
}
// Does not include the segment used to connect to the sun
static const uint MAX_EYE_PATH_LENGTH = 16;

static const uint RUSSIAN_ROULETTE_START_PATH_LENGTH = 3;
static const float MAX_RAY_LENGTH = FLT_MAX;
//static const float MAX_RAY_LENGTH = 5.0;

// Rough-smooth-rough specular paths are a major source of fireflies.
// Enabling this option will bias roughness of path vertices following
// reflections off rough interfaces.
static const bool FIREFLY_SUPPRESSION = true;
static const bool FURNACE_TEST = !true;
static const bool FURNACE_TEST_EXCLUDE_DIFFUSE = !true;
static const bool USE_PIXEL_FILTER = true;
static const bool INDIRECT_ONLY = !true;
static const bool GREY_ALBEDO_FIRST_BOUNCE = !true;
static const bool BLACK_ALBEDO_FIRST_BOUNCE = !true;
static const bool ONLY_SPECULAR_FIRST_BOUNCE = !true;
static const bool USE_SOFT_SHADOWS = true;
static const bool SHOW_ALBEDO = !true;
static const bool SHOW_NORMAL = !true;
static const bool USE_LIGHTS = true;
static const bool USE_EMISSIVE = true;
static const bool RESET_ACCUMULATION = !true;
static const bool ROLLING_ACCUMULATION = !true;

#define USE_SKY_CUBE_TEX 1

float3 sample_environment_light(float3 dir) {
    #if USE_SKY_CUBE_TEX
        return sky_cube_tex.SampleLevel(sampler_llr, dir, 0).rgb;
    #else
        return atmosphere_default(dir, SUN_DIRECTION);
    #endif
}

// Approximate Gaussian remap
// https://www.shadertoy.com/view/MlVSzw
float inv_error_function(float x, float truncation) {
    static const float ALPHA = 0.14;
    static const float INV_ALPHA = 1.0 / ALPHA;
    static const float K = 2.0 / (M_PI * ALPHA);

	float y = log(max(truncation, 1.0 - x*x));
	float z = K + 0.5 * y;
	return sqrt(max(0.0, sqrt(z*z - y * INV_ALPHA) - z)) * sign(x);
}

float remap_unorm_to_gaussian(float x, float truncation) {
	return inv_error_function(x * 2.0 - 1.0, truncation);
}

[shader("raygeneration")]
void main() {
    const uint2 px = DispatchRaysIndex().xy;

    float4 prev;
    if (ROLLING_ACCUMULATION) {
        prev = float4(output_tex[px].rgb, 8);
    } else {
        prev = RESET_ACCUMULATION ? 0 : output_tex[px];
    }
    float depth = 0;
    BrdfSample brdf_sample = BrdfSample::invalid();
    // if (prev.w < 1)
    {
        float4 radiance_sample_count_packed = 0.0;
        uint rng = hash_combine2(hash_combine2(px.x, hash1(px.y)), frame_constants.frame_index);
        static const uint sample_count = 1;
        LightSampler light_sampler = make_light_sampler(make_random(px, frame_constants.frame_index));
        for (uint sample_idx = 0; sample_idx < sample_count; ++sample_idx) {
            float px_off0 = 0.5;
            float px_off1 = 0.5;
            if (USE_PIXEL_FILTER) {
                const float psf_scale = 0.4;
                px_off0 += psf_scale * remap_unorm_to_gaussian(uint_to_u01_float(hash1_mut(rng)), 1e-8);
                px_off1 += psf_scale * remap_unorm_to_gaussian(uint_to_u01_float(hash1_mut(rng)), 1e-8);
            }

            const float2 pixel_center = px + float2(px_off0, px_off1);
            const float2 uv = pixel_center / DispatchRaysDimensions().xy;

            RayDesc outgoing_ray;
            {
                const ViewRayContext view_ray_context = ViewRayContext::from_uv(uv);
                const float3 ray_dir_ws = view_ray_context.ray_dir_ws();

                outgoing_ray = new_ray(
                    view_ray_context.ray_origin_ws(), 
                    normalize(ray_dir_ws.xyz),
                    0.0,
                    FLT_MAX
                );
            }

            float3 throughput = 1.0.xxx;
            float3 last_throughput = throughput;
            float3 total_radiance = 0.0.xxx;

            float roughness_bias = 0.0;

            RayCone ray_cone = pixel_ray_cone_from_image_height(
                DispatchRaysDimensions().y
            );

            // Bias for texture sharpness
            ray_cone.spread_angle *= 0.3;

            [loop]
            for (uint path_length = 0; path_length < MAX_EYE_PATH_LENGTH; ++path_length) {

                if (path_length == 1) {
                    outgoing_ray.TMax = MAX_RAY_LENGTH;
                }

                GbufferPathVertex primary_hit = GbufferRaytrace::with_ray(outgoing_ray)
                    .with_cone(ray_cone)
                    //.with_cull_back_faces(true || 0 == path_length)
                    .with_cull_back_faces(false)
                    .with_path_length(path_length)
                    .trace(acceleration_structure);

                if (primary_hit.is_hit) {
                    // TODO
                    const float surface_spread_angle = 0.0;
                    ray_cone = ray_cone.propagate(surface_spread_angle, primary_hit.ray_t);

                    if (path_length == 0) {
                        float4 hom = mul(float4(primary_hit.position, 1), frame_constants.view_constants.world_to_clip);
                        depth = hom.z / hom.w;
                    }

                    GbufferData gbuffer = primary_hit.gbuffer_packed.unpack();

                    if (SHOW_ALBEDO) {
                        output_tex[px] = float4(gbuffer.albedo, 1);
                        // output_tex[px] = float4(gbuffer.normal * 0.5 + 0.5, 1);
                        return;
                    }
                    else if (SHOW_NORMAL) {
                        output_tex[px] = float4(gbuffer.normal * 0.5 + 0.5, 1);
                        return;
                    }
                    if (dot(gbuffer.normal, outgoing_ray.Direction) >= 0.0) {
                        if (0 == path_length) {
                            // Flip the normal for primary hits so we don't see blackness
                            gbuffer.normal = -gbuffer.normal;
                        } else {
                            break;
                        }
                    }

                    if (FURNACE_TEST && !FURNACE_TEST_EXCLUDE_DIFFUSE) {
                        gbuffer.albedo = 1;
                    }

                    if (INDIRECT_ONLY && path_length == 0) {
                        gbuffer.albedo = 1.0;
                        gbuffer.metalness = 0.0;
                    }

                    if (ONLY_SPECULAR_FIRST_BOUNCE && path_length == 0) {
                        gbuffer.albedo = 1.0;
                        gbuffer.metalness = 1.0;
                        //gbuffer.roughness = 0.01;
                    }

                    if (GREY_ALBEDO_FIRST_BOUNCE && path_length == 0) {
                        gbuffer.albedo = 0.5;
                    }
                    
                    if (BLACK_ALBEDO_FIRST_BOUNCE && path_length == 0) {
                        gbuffer.albedo = 0.0;
                    }

                    const float3x3 tangent_to_world = build_orthonormal_basis(gbuffer.normal);
                    float3 wo = mul(-outgoing_ray.Direction, tangent_to_world);

                    // Hack for shading normals facing away from the outgoing ray's direction:
                    // We flip the outgoing ray along the shading normal, so that the reflection's curvature
                    // continues, albeit at a lower rate.
                    if (wo.z < 0.0) {
                        wo.z *= -0.25;
                        wo = normalize(wo);
                    }

                    LayeredBrdf brdf = LayeredBrdf::from_gbuffer_ndotv(gbuffer, wo.z);

                    if (FIREFLY_SUPPRESSION) {
                        brdf.specular_brdf.roughness = lerp(brdf.specular_brdf.roughness, 1.0, roughness_bias);
                    }

                    if (FURNACE_TEST && FURNACE_TEST_EXCLUDE_DIFFUSE) {
                        brdf.diffuse_brdf.albedo = 0.0.xxx;
                    }

                    if (!FURNACE_TEST && !(ONLY_SPECULAR_FIRST_BOUNCE && path_length == 0)) {
                        // if (brdf_sample.is_valid()) {
                        //     total_radiance += shade_path_hit(
                        //         outgoing_ray,
                        //         brdf_sample.value,
                        //         brdf_sample.pdf,
                        //         primary_hit.position,
                        //         tangent_to_world,
                        //         gbuffer.normal,
                        //         wo,
                        //         last_throughput,
                        //         light_sampler
                        //     );
                        // }
                        total_radiance += sample_sun_light_nee(
                            brdf,
                            tangent_to_world,
                            float2(uint_to_u01_float(hash1_mut(rng)), uint_to_u01_float(hash1_mut(rng))),
                            primary_hit.position,
                            gbuffer.normal,
                            wo,
                            throughput
                        );
                        total_radiance += sample_lights_nee(
                            brdf,
                            light_sampler,
                            primary_hit.position,
                            tangent_to_world,
                            gbuffer.normal,
                            wo,
                            throughput
                        );
                        // if (USE_EMISSIVE) {
                        //     total_radiance += gbuffer.emissive * throughput;
                        // }
                        
                        // if (USE_LIGHTS && frame_constants.triangle_light_count > 0/* && path_length > 0*/) {   // rtr comp
                        //     const float light_selection_pmf = 1.0 / frame_constants.triangle_light_count;
                        //     const uint light_idx = hash1_mut(rng) % frame_constants.triangle_light_count;
                        //     //const float light_selection_pmf = 1;
                        //     //for (uint light_idx = 0; light_idx < frame_constants.triangle_light_count; light_idx += 1)
                        //     {
                        //         const float2 urand = float2(
                        //             uint_to_u01_float(hash1_mut(rng)),
                        //             uint_to_u01_float(hash1_mut(rng))
                        //         );

                        //         TriangleLight triangle_light = TriangleLight::from_packed(triangle_lights_dyn[light_idx]);
                        //         LightSampleResultArea light_sample = sample_triangle_light(triangle_light.as_triangle(), urand);
                        //         const float3 shadow_ray_origin = primary_hit.position;
                        //         const float3 to_light_ws = light_sample.pos - primary_hit.position;
                        //         const float dist_to_light2 = dot(to_light_ws, to_light_ws);
                        //         const float3 to_light_norm_ws = to_light_ws * rsqrt(dist_to_light2);

                        //         const float to_psa_metric =
                        //             max(0.0, dot(to_light_norm_ws, gbuffer.normal))
                        //             * max(0.0, dot(to_light_norm_ws, -light_sample.normal))
                        //             / dist_to_light2;

                        //         if (to_psa_metric > 0.0) {
                        //             float3 wi = mul(to_light_norm_ws, tangent_to_world);

                        //             const bool is_shadowed =
                        //                 rt_is_shadowed(
                        //                     acceleration_structure,
                        //                     new_ray(
                        //                         shadow_ray_origin,
                        //                         to_light_norm_ws,
                        //                         1e-3,
                        //                         sqrt(dist_to_light2) - 2e-3
                        //                 ));

                        //             total_radiance +=
                        //                 is_shadowed ? 0 :
                        //                     throughput * triangle_light.radiance() * brdf.evaluate(wo, wi) / light_sample.pdf.value * to_psa_metric / light_selection_pmf;
                        //         }
                        //     }
                        // }
                        last_throughput = throughput;
                    }
                        
                    float3 urand = float3(
                        uint_to_u01_float(hash1_mut(rng)),
                        uint_to_u01_float(hash1_mut(rng)),
                        uint_to_u01_float(hash1_mut(rng)));
                    //bdrf sampling
                    brdf_sample = brdf.sample(wo, urand);

                    if (brdf_sample.is_valid()) {
                        if (FIREFLY_SUPPRESSION) {
                            roughness_bias = lerp(roughness_bias, 1.0, 0.5 * brdf_sample.approx_roughness);
                        }

                        outgoing_ray.Origin = primary_hit.position;
                        outgoing_ray.Direction = mul(tangent_to_world, brdf_sample.wi);
                        outgoing_ray.TMin = 1e-4;
                        throughput *= brdf_sample.value_over_pdf;
                    } else {
                         break;
                    }

                    // Russian roulette
                    if (path_length >= RUSSIAN_ROULETTE_START_PATH_LENGTH) {
                        const float rr_coin = uint_to_u01_float(hash1_mut(rng));
                        const float continue_p = max(gbuffer.albedo.r, max(gbuffer.albedo.g, gbuffer.albedo.b));
                        if (rr_coin > continue_p) {
                            break;
                        } else {
                            throughput /= continue_p;
                        }
                    }
                } else {
                    total_radiance += throughput * sample_environment_light(outgoing_ray.Direction);
                    break;
                }
            }

            if (all(total_radiance >= 0.0)) {
                radiance_sample_count_packed += float4(total_radiance, 1.0);
            }
        }

        float4 cur = radiance_sample_count_packed;

        float tsc = cur.w + prev.w;
        float lrp = cur.w / max(1.0, tsc);
        cur.rgb /= max(1.0, cur.w);
        float3 output = max(0.0.xxx, lerp(prev.rgb, cur.rgb, lrp));
        output_tex[px] = float4(output, max(1, tsc));
        depth_tex[px] = depth;
    }
}
