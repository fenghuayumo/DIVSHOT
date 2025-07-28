#ifndef MATH_HLSL
#define MATH_HLSL

#include "math_const.hlsl"

float max3(float x, float y, float z) {
    return max(x, max(y, z));
}

float square(float x) { return x * x; }
float2 square(float2 x) { return x * x; }
float3 square(float3 x) { return x * x; }
float4 square(float4 x) { return x * x; }

float length_squared(float2 v) { return dot(v, v); }
float length_squared(float3 v) { return dot(v, v); }
float length_squared(float4 v) { return dot(v, v); }
float distance_squared(float3 a, float3 b) { return length_squared(a - b); }
// Building an Orthonormal Basis, Revisited
// http://jcgt.org/published/0006/01/01/
float3x3 build_orthonormal_basis(float3 n) {
    float3 b1;
    float3 b2;

    if (n.z < 0.0) {
        const float a = 1.0 / (1.0 - n.z);
        const float b = n.x * n.y * a;
        b1 = float3(1.0 - n.x * n.x * a, -b, n.x);
        b2 = float3(b, n.y * n.y * a - 1.0, -n.y);
    } else {
        const float a = 1.0 / (1.0 + n.z);
        const float b = -n.x * n.y * a;
        b1 = float3(1.0 - n.x * n.x * a, b, -n.x);
        b2 = float3(b, 1.0 - n.y * n.y * a, -n.y);
    }

    return float3x3(
        b1.x, b2.x, n.x,
        b1.y, b2.y, n.y,
        b1.z, b2.z, n.z
    );
}

void branchlessONB(in float3 n, out float3 b1, out float3 b2)
{
    float sign = n.z >= 0.0f ? 1.0f : -1.0f;
    float a = -1.0f / (sign + n.z);
    float b = n.x * n.y * a;
    b1 = float3(1.0f + sign * n.x * n.x * a, sign * b, -sign * n.x);
    b2 = float3(b, sign + n.y * n.y * a, -n.y);
}

// Calculates vector d such that
// lerp(a, d.rgb, d.a) equals lerp(lerp(a, b.rgb, b.a), c.rgb, c.a)
//
// Lerp[a_, b_, c_] := a  (1-c) + b  c
// FullSimplify[Lerp[a,(b(c (1 -  e)) + d e) /(c + e - c e), 1-(1-c)(1-e)]] == FullSimplify[Lerp[Lerp[a, b, c], d, e]]
float4 prelerp(float4 b, float4 c) {
    float denom = b.a + c.a * (1.0 - b.a);
    return denom > 1e-5 ? float4(
        (b.rgb * (b.a * (1.0 - c.a)) + c.rgb * c.a) / denom,
        1.0 - (1.0 - b.a) * (1.0 - c.a)
    ) : 0.0.xxxx;
}

float inverse_depth_relative_diff(float primary_depth, float secondary_depth) {
    return abs(max(1e-20, primary_depth) / max(1e-20, secondary_depth) - 1.0);
}

// Encode a scalar a space which heavily favors small values.
float exponential_squish(float len, float squish_scale) {
    return exp2(-clamp(squish_scale * len, 0, 100));
}

// Ditto, decode.
float exponential_unsquish(float len, float squish_scale) {
    return max(0.0, -1.0 / squish_scale * log2(1e-30 + len));
}

struct RaySphereIntersection {
    // `t` will be NaN if no intersection is found
    float t;
    float3 normal;

    bool is_hit() {
        return t >= 0.0;
    }
};

struct Sphere {
    float3 center;
    float radius;

    static Sphere from_center_radius(float3 center, float radius) {
        Sphere res;
        res.center = center;
        res.radius = radius;
        return res;
    }

    RaySphereIntersection intersect_ray(float3 ray_origin, float3 ray_dir) {
    	float3 oc = ray_origin - center;
        float a = dot(ray_dir, ray_dir);
    	float b = 2.0 * dot(oc, ray_dir);
    	float c = dot(oc, oc) - radius * radius;
    	float h = b * b - 4.0 * a * c;

        RaySphereIntersection res;
        res.t = (-b - sqrt(h)) / (2.0 * a);
        res.normal = normalize(ray_origin + res.t * ray_dir - center);
        return res;
    }

    RaySphereIntersection intersect_ray_inside(float3 ray_origin, float3 ray_dir) {
    	float3 oc = ray_origin - center;
        float a = dot(ray_dir, ray_dir);
    	float b = 2.0 * dot(oc, ray_dir);
    	float c = dot(oc, oc) - radius * radius;

    	float h = b * b - 4.0 * a * c;

        RaySphereIntersection res;
        // Note: flipped sign compared to the regular version
        res.t = (-b + sqrt(h)) / (2.0 * a);
        res.normal = normalize(ray_origin + res.t * ray_dir - center);
        return res;
    }
};

struct RayBoxIntersection {
    float t_min;
    float t_max;

    bool is_hit() {
        return t_min <= t_max && t_max >= 0.0;
    }
};

struct Aabb {
    float3 pmin;
    float3 pmax;

    static Aabb from_min_max(float3 pmin, float3 pmax) {
        Aabb res;
        res.pmin = pmin;
        res.pmax = pmax;
        return res;
    }

    static Aabb from_center_half_extent(float3 center, float3 half_extent) {
        Aabb res;
        res.pmin = center - half_extent;
        res.pmax = center + half_extent;
        return res;
    }

    // From https://github.com/tigrazone/glslppm
    RayBoxIntersection intersect_ray(float3 ray_origin, float3 ray_dir) {
    	float3 min_interval = (pmax.xyz - ray_origin.xyz) / ray_dir;
    	float3 max_interval = (pmin.xyz - ray_origin.xyz) / ray_dir;

    	float3 a = min(min_interval, max_interval);
    	float3 b = max(min_interval, max_interval);

        RayBoxIntersection res;
        res.t_min = max(max(a.x, a.y), a.z);
        res.t_max = min(min(b.x, b.y), b.z);
        // return t_min <= t_max && t_min < t && t_max >= 0.0;
        return res;
    }

    bool contains_point(float3 p) {
        return all(p >= pmin) && all(p <= pmax);
    }
};

float inverse_lerp(float minv, float maxv, float v) {
    return (v - minv) / (maxv - minv);
}


// "Explodes" an integer, i.e. inserts a 0 between each bit.  Takes inputs up to 16 bit wide.
//      For example, 0b11111111 -> 0b1010101010101010
uint integer_explode(uint x)
{
    x = (x | (x << 8)) & 0x00FF00FF;
    x = (x | (x << 4)) & 0x0F0F0F0F;
    x = (x | (x << 2)) & 0x33333333;
    x = (x | (x << 1)) & 0x55555555;
    return x;
}

// Reverse of integer_explode, i.e. takes every other bit in the integer and compresses
// those bits into a dense bit firld. Takes 32-bit inputs, produces 16-bit outputs.
//    For example, 0b'abcdefgh' -> 0b'0000bdfh'
uint integer_compact(uint x)
{
    x = (x & 0x11111111) | ((x & 0x44444444) >> 1);
    x = (x & 0x03030303) | ((x & 0x30303030) >> 2);
    x = (x & 0x000F000F) | ((x & 0x0F000F00) >> 4);
    x = (x & 0x000000FF) | ((x & 0x00FF0000) >> 8);
    return x;
}

// Converts a 2D position to a linear index following a Z-curve pattern.
uint z_curve_to_linear_Index(uint2 xy)
{
    return integer_explode(xy[0]) | (integer_explode(xy[1]) << 1);
}

// Converts a linear to a 2D position following a Z-curve pattern.
uint2 linear_index_to_z_curve(uint index)
{
    return uint2(
        integer_compact(index),
        integer_compact(index >> 1));
}


float3 spherical_direction(float sinTheta, float cosTheta, float sinPhi, float cosPhi, float3 x, float3 y, float3 z)
{
    return sinTheta * cosPhi * x + sinTheta * sinPhi * y + cosTheta * z;
}


float2 dir_2_equirect_uv(float3 normalizedDirection)
{
    float elevation = asin(normalizedDirection.y);
    float azimuth = 0;
    if (abs(normalizedDirection.y) < 1.0)
        azimuth = atan2(normalizedDirection.z, normalizedDirection.x);

    float2 uv;
    uv.x = azimuth / (2 * M_PI) - 0.25;
    uv.y = 0.5 - elevation / M_PI;

    return uv;
}

float3 equirect_uv_2_dir(float2 uv, out float cosElevation)
{
    float azimuth = (uv.x + 0.25) * (2 * M_PI);
    float elevation = (0.5 - uv.y) * M_PI;
    cosElevation = cos(elevation);

    return float3(
        cos(azimuth) * cosElevation,
        sin(elevation),
        sin(azimuth) * cosElevation
    );
}

#endif
