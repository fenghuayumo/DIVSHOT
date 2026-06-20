#include "../inc/binding.hlsl"
#include "../inc/math.hlsl"
#include "../inc/samplers.hlsl"
#include "../inc/frame_constants.hlsl"
#include "../inc/gbuffer.hlsl"
#include "../inc/rt.hlsl"
#include "../inc/random.hlsl"
#include "../inc/bindless.hlsl"

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
#include "../path_tracer/path_integrator.hlsl"

[shader("raygeneration")]
void main()
{
    const uint2 px = DispatchRaysIndex().xy;
    const uint2 dims = DispatchRaysDimensions().xy;

    Random rng = make_random(px, frame_constants.frame_index);
    float4 prev = output_tex[px];

    float2 jitter = float2(uniform_rand_float(rng), uniform_rand_float(rng)) - 0.5.xx;
    float2 uv = (float2(px) + 0.5.xx + jitter) / float2(dims);

    PathTraceState path = PathTraceState::make(px, rng);
    PathTracer::setup_primary_ray(path, uv, dims);

    [loop]
    while (path.active && path.path_length < PATH_TRACER_MAX_PATH_LENGTH)
        PathTracer::trace_bounce(acceleration_structure, path);

    PathTracer::commit_pixel(path, prev, output_tex, depth_tex);
}
