#ifndef PATH_TRACER_PATH_STATE_HLSL
#define PATH_TRACER_PATH_STATE_HLSL

#include "../inc/frame_constants.hlsl"
#include "../inc/random.hlsl"
#include "../inc/ray_cone.hlsl"

#ifndef PATH_TRACER_MAX_PATH_LENGTH
#define PATH_TRACER_MAX_PATH_LENGTH 8
#endif

#ifndef PATH_TRACER_RR_START_PATH_LENGTH
#define PATH_TRACER_RR_START_PATH_LENGTH 3
#endif

struct PathTraceState
{
    uint2 pixel;
    RayDesc ray;
    RayCone ray_cone;
    Random rng;

    float3 throughput;
    float3 radiance;
    float depth;

    float previous_bsdf_pdf;
    float previous_environment_nee_pdf;
    float firefly_filter_k;
    uint diffuse_bounces;
    uint path_length;

    bool active;

    static PathTraceState make(uint2 pixel_, Random rng_)
    {
        PathTraceState path;
        path.pixel = pixel_;
        path.rng = rng_;
        path.throughput = 1.0.xxx;
        path.radiance = 0.0.xxx;
        path.depth = FARZ;
        path.previous_bsdf_pdf = 0.0;
        path.previous_environment_nee_pdf = 0.0;
        path.firefly_filter_k = 1.0;
        path.diffuse_bounces = 0;
        path.path_length = 0;
        path.active = true;
        return path;
    }
};

#endif // PATH_TRACER_PATH_STATE_HLSL
