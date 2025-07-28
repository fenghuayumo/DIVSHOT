#ifndef RT_HLSL
#define RT_HLSL

#include "math_const.hlsl"
#include "gbuffer.hlsl"
#include "ray_cone.hlsl"

#define INSTANCE_MASK_OPAQUE 0x01
#define INSTANCE_MASK_ALPHA_TESTED 0x02
#define INSTANCE_MASK_TRANSPARENT 0x04
#define INSTANCE_MASK_ALL 0xFF

struct GbufferRayPayload {
    GbufferDataPacked gbuffer_packed;
    float t;
    RayCone ray_cone;
    uint path_length;

    static GbufferRayPayload new_miss() {
        GbufferRayPayload res;
        res.t = FLT_MAX;
        res.ray_cone = RayCone::from_spread_angle(0.0);
        res.path_length = 0;
        return res;
    }

    bool is_miss() {
        return t == FLT_MAX;
    }

    bool is_hit() {
        return !is_miss();
    }
};

struct ShadowRayPayload {
    bool is_shadowed;

    static ShadowRayPayload new_hit() {
        ShadowRayPayload res;
        res.is_shadowed = true;
        return res;
    }

    bool is_miss() {
        return !is_shadowed;
    }

    bool is_hit() {
        return !is_miss();
    }
};

RayDesc new_ray(float3 origin, float3 direction, float tmin, float tmax) {
    RayDesc ray;
    ray.Origin = origin;
    ray.Direction = direction;
    ray.TMin = tmin;
    ray.TMax = tmax;
    return ray;
}

bool rt_is_shadowed(
    RaytracingAccelerationStructure acceleration_structure,
    RayDesc ray
) {
    ShadowRayPayload shadow_payload = ShadowRayPayload::new_hit();
    TraceRay(
        acceleration_structure,
        RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER,
        0xff, 1, 0, 1, ray, shadow_payload
    );

    return shadow_payload.is_shadowed;
}

bool rt_visibility(
    RaytracingAccelerationStructure acceleration_structure,
    RayDesc ray
) {
    ShadowRayPayload shadow_payload = ShadowRayPayload::new_hit();
    TraceRay(
        acceleration_structure,
        RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER,
        0xff, 1, 0, 1, ray, shadow_payload
    );

    return !shadow_payload.is_shadowed;
}

struct GbufferPathVertex {
    bool is_hit;
    GbufferDataPacked gbuffer_packed;
    float3 position;
    float ray_t;
};

struct CustomRayPayload {
    float t;
    RayCone ray_cone;
    float2 texture_pos;

    static CustomRayPayload new_miss() {
        CustomRayPayload res;
        res.t = FLT_MAX;
        res.ray_cone = RayCone::from_spread_angle(0.0);
        res.texture_pos = -1;
        return res;
    }

    bool is_miss() {
        return t == FLT_MAX;
    }

    bool is_hit() {
        return !is_miss();
    }
};
struct TextureRaytrace {
    RayDesc ray;
    RayCone ray_cone;
    bool cull_back_faces;

    static TextureRaytrace with_ray(RayDesc ray) {
        TextureRaytrace res;
        res.ray = ray;
        res.ray_cone = RayCone::from_spread_angle(1.0);
        res.cull_back_faces = true;
        return res;
    }

    TextureRaytrace with_cone(RayCone ray_cone) {
        TextureRaytrace res = this;
        res.ray_cone = ray_cone;
        return res;
    }

    TextureRaytrace with_cull_back_faces(bool v) {
        TextureRaytrace res = this;
        res.cull_back_faces = v;
        return res;
    }

    CustomRayPayload trace(RaytracingAccelerationStructure acceleration_structure) {
        CustomRayPayload payload = CustomRayPayload::new_miss();
        payload.ray_cone = this.ray_cone;

        uint trace_flags = 0;
        if (this.cull_back_faces) {
            trace_flags |= RAY_FLAG_CULL_BACK_FACING_TRIANGLES;
        }

        TraceRay(acceleration_structure, trace_flags, 0xff, 0, 0, 0, this.ray, payload);

        return payload;
    }
};

struct GbufferRaytrace {
    RayDesc ray;
    RayCone ray_cone;
    uint path_length;
    bool cull_back_faces;

    static GbufferRaytrace with_ray(RayDesc ray) {
        GbufferRaytrace res;
        res.ray = ray;
        res.ray_cone = RayCone::from_spread_angle(1.0);
        res.path_length = 0;
        res.cull_back_faces = true;
        return res;
    }

    GbufferRaytrace with_cone(RayCone ray_cone) {
        GbufferRaytrace res = this;
        res.ray_cone = ray_cone;
        return res;
    }

    GbufferRaytrace with_path_length(uint v) {
        GbufferRaytrace res = this;
        res.path_length = v;
        return res;
    }

    GbufferRaytrace with_cull_back_faces(bool v) {
        GbufferRaytrace res = this;
        res.cull_back_faces = v;
        return res;
    }

    GbufferPathVertex trace(RaytracingAccelerationStructure acceleration_structure) {
        GbufferRayPayload payload = GbufferRayPayload::new_miss();
        payload.ray_cone = this.ray_cone;
        payload.path_length = this.path_length;

        uint trace_flags = 0;
        if (this.cull_back_faces) {
            trace_flags |= RAY_FLAG_CULL_BACK_FACING_TRIANGLES;
        }

        TraceRay(acceleration_structure, trace_flags, 0xff, 0, 0, 0, this.ray, payload);

        if (payload.is_hit()) {
            GbufferPathVertex res;
            res.is_hit = true;
            res.position = ray.Origin + ray.Direction * payload.t;
            res.gbuffer_packed = payload.gbuffer_packed;
            res.ray_t = payload.t;
            return res;
        } else {
            GbufferPathVertex res;
            res.is_hit = false;
            res.ray_t = FLT_MAX;
            return res;
        }
    }
};

float3 offset_position(const float3 position, const float3 normal)
{
    static const float origin = 1.0f / 32.0f;
    static const float floatScale = 1.0f / 65536.0f;
    static const float intScale = 256.0f;

    int3 intN = int3(normal * intScale.xxx);

    float3 positionI = asfloat(asint(position) + select(position < 0, -intN, intN));

    float3 posOffset = position + (normal * floatScale.xxx);
    return select(abs(position) < origin.xxx, posOffset, positionI);
}
#endif