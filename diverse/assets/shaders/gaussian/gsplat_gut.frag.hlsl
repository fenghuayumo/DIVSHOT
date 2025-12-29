// 3DGUT Fragment Shader for Gaussian Splatting
// 
// Supports both pinhole and fisheye cameras with optional Depth of Field (DOF)
// - Without DOF: Uses efficient 2D Gaussian formula
// - With DOF: Uses ray-based 3D Gaussian evaluation with lens perturbation
//
// Based on NVIDIA's vk_gaussian_splatting implementation

#include "../inc/frame_constants.hlsl"
#include "../inc/pack_unpack.hlsl"
#include "gaussian_common.hlsl"
#include "threedgut_projection.hlsl"

// Push constants (must match mesh shader)
[[vk::binding(0)]] cbuffer PushConstants {
    float4x4 model_matrix;
    uint buf_id;
    uint surface_width;
    uint surface_height;
    uint num_gaussians;
    float4 locked_color;
    float4 select_color;
    float4 tintColor;
    float4 color_offset;
};

struct FragmentInput
{
    [[vk::location(0)]] nointerpolation uint splatId : SPLATID;
    [[vk::location(1)]] nointerpolation float4 splatCol : COLOR0;
    [[vk::location(2)]] nointerpolation float3 splatPosition : POSITION0;
    [[vk::location(3)]] nointerpolation float3 splatScale : SCALE0;
    [[vk::location(4)]] nointerpolation float4 splatRotation : ROTATION0;
    [[vk::location(5)]] float2 fragPos : TEXCOORD0;
    float4 position : SV_Position;
};

struct FragmentOutput
{
    float4 color : SV_Target0;
};

// ============ Random Number Generation for DOF ============
// Simple hash-based random number generator
uint pcg_hash(uint input)
{
    uint state = input * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

float rand(inout uint seed)
{
    seed = pcg_hash(seed);
    return float(seed) / 4294967295.0;
}

// ============ Ray Generation Functions ============

// Generate fisheye camera ray for the given pixel
bool generateFisheyeRay(in float2 pixel, in float2 resolution, in float fov, in float2 principalPoint, 
                        in float4x4 viewInverse, out float3 rayOrigin, out float3 rayDirection)
{
    rayOrigin = float3(0, 0, 0);
    rayDirection = float3(0, 0, -1);
    
    float2 adjustedPixel = pixel;
    adjustedPixel.x -= principalPoint.x;
    adjustedPixel.y += principalPoint.y;
    
    const float2 ndc = (adjustedPixel / (resolution - 1.0)) * 2.0 - float2(1.0, 1.0);
    const float u = ndc.x;
    const float v = ndc.y;
    
    const float r = sqrt(u * u + v * v);
    const bool outOfFov = r > 1.0;
    
    const float epsilon = 1e-9;
    float phiCos = abs(r) > epsilon ? u / r : 0.0;
    phiCos = clamp(phiCos, -1.0, 1.0);
    float phi = acos(phiCos);
    phi = v < 0.0 ? -phi : phi;
    const float theta = r * fov * 0.5;
    
    const float3 direction = float3(cos(phi) * sin(theta), -sin(phi) * sin(theta), -cos(theta));
    
    rayDirection = normalize(mul(float4(direction, 0), viewInverse)).xyz;
    rayOrigin = mul(float4(0.0, 0.0, 0.0, 1.0), viewInverse).xyz;
    
    return !outOfFov;
}

// Generate pinhole camera ray for the given pixel
void generatePinholeRay(in float2 pixel, in float2 resolution, in float4x4 viewInverse, 
                        in float4x4 projInverse, out float3 rayOrigin, out float3 rayDirection)
{
    const float2 pixelCenter = pixel + float2(0.5, 0.5);
    const float2 inUV = pixelCenter / resolution;
    // Convert to clip space coordinates
    // Note: Vulkan clip space has Y pointing down (after projection matrix Y-flip)
    // So we need to flip Y: pixel Y=0 (top) -> clip Y=+1, pixel Y=height (bottom) -> clip Y=-1
    float2 d = float2(inUV.x * 2.0 - 1.0, 1.0 - inUV.y * 2.0);
    
    rayOrigin = mul(float4(0, 0, 0, 1), viewInverse).xyz;
    
    float4 target = mul(float4(d.x, d.y, 1, 1), projInverse);
    rayDirection = normalize(mul(float4(target.xyz, 0), viewInverse)).xyz;
}

// ============ Depth of Field ============
static const float M_TWO_PI = 6.28318530717958647692;

void depthOfField(inout uint seed, in float focusDist, in float aperture, in float4x4 viewInverse,
                  inout float3 rayOrigin, inout float3 rayDirection)
{
    // Find where the original ray would hit the focal plane
    const float3 focalPoint = rayOrigin + rayDirection * focusDist;
    
    // Random sampling on lens
    const float r1 = rand(seed) * M_TWO_PI;
    const float r2 = rand(seed) * aperture;
    
    // Aperture position
    const float3 camRight = mul(float4(1, 0, 0, 0), viewInverse).xyz;
    const float3 camUp = mul(float4(0, 1, 0, 0), viewInverse).xyz;
    const float3 randomAperturePos = (cos(r1) * camRight + sin(r1) * camUp) * sqrt(r2);
    
    // New ray direction pointing to focal point
    const float3 finalRayDir = normalize(focalPoint - (rayOrigin + randomAperturePos));
    
    // Apply perturbation
    rayOrigin += randomAperturePos;
    rayDirection = finalRayDir;
}

// ============ Ray-Based Gaussian Evaluation ============

// Convert quaternion (scalar-last: x,y,z,w) to transposed rotation matrix (for inverse)
float3x3 quatToMat3Transpose(float4 q)
{
    float x = q.x, y = q.y, z = q.z, w = q.w;
    float x2 = x + x, y2 = y + y, z2 = z + z;
    float xx = x * x2, xy = x * y2, xz = x * z2;
    float yy = y * y2, yz = y * z2, zz = z * z2;
    float wx = w * x2, wy = w * y2, wz = w * z2;
    
    return float3x3(
        1.0 - (yy + zz), xy + wz, xz - wy,
        xy - wz, 1.0 - (xx + zz), yz + wx,
        xz + wy, yz - wx, 1.0 - (xx + yy)
    );
}

// Transform ray to canonical particle space
void particleCanonicalRay(in float3 rayOrigin, in float3 rayDirection,
                          in float3 particlePosition, in float3 particleScale,
                          in float3x3 particleInvRotation,
                          out float3 particleRayOrigin, out float3 particleRayDirection)
{
    const float3 giscl = float3(1.0, 1.0, 1.0) / particleScale;
    const float3 gposc = rayOrigin - particlePosition;
    const float3 gposcr = mul(gposc, particleInvRotation);
    particleRayOrigin = giscl * gposcr;
    
    const float3 rayDirR = mul(rayDirection, particleInvRotation);
    const float3 grdu = giscl * rayDirR;
    particleRayDirection = normalize(grdu);
}

// Compute max kernel response (Gaussian kernel)
float particleRayMaxKernelResponse(in float3 particleRayOrigin, in float3 particleRayDirection)
{
    const float3 gcrod = cross(particleRayDirection, particleRayOrigin);
    const float grayDist = dot(gcrod, gcrod);
    return exp(-0.5 * grayDist);
}

// Process hit for ray-based evaluation
bool particleProcessHitRay(in float3 modelRayOrigin, in float3 modelRayDirection,
                           in float4 splatColor, in float3 particlePosition,
                           in float3 particleScale, in float4 particleRotation,
                           out float opacity)
{
    opacity = 0.0;
    
    const float3x3 particleInvRotation = quatToMat3Transpose(particleRotation);
    
    float3 particleRayOrigin;
    float3 particleRayDirection;
    particleCanonicalRay(modelRayOrigin, modelRayDirection, particlePosition, particleScale, 
                         particleInvRotation, particleRayOrigin, particleRayDirection);
    
    const float particleDensity = splatColor.w;
    const float minParticleAlpha = 1.0 / 255.0;
    const float minParticleKernelDensity = 0.01;
    
    const float maxResponse = particleRayMaxKernelResponse(particleRayOrigin, particleRayDirection);
    const float alpha = min(0.999, maxResponse * particleDensity);
    
    const float alphaCullThreshold = 1.0 / 255.0;
    const bool acceptHit = (particleDensity > alphaCullThreshold) && (alpha > minParticleAlpha)
                           && (maxResponse > minParticleKernelDensity);
    
    if (acceptHit)
    {
        opacity = alpha;
    }
    
    return acceptHit;
}

FragmentOutput main(FragmentInput input)
{
    FragmentOutput output;
    
    // Early out for invalid splats
    if (input.splatCol.w <= 0.0)
        discard;
    
    // Get DOF and camera parameters
    const bool dofEnabled = frame_constants.dof_enabled > 0.5;
    const uint cameraType = frame_constants.camera_type;
    
    float alpha;
    
    if (dofEnabled)
    {
        // ============ DOF ENABLED: Use Ray-Based 3D Gaussian Evaluation ============
        float2 viewport = float2(surface_width, surface_height);
        float4x4 viewInverse = frame_constants.view_constants.view_to_world;
        float4x4 projInverse = frame_constants.view_constants.clip_to_view;
        
        float3 rayOrigin;
        float3 rayDirection;
        
        if (cameraType == CAMERA_FISHEYE)
        {
            float fisheyeFovRad = frame_constants.fov_rad;
            if (fisheyeFovRad <= 0.0)
                fisheyeFovRad = 3.14159265359;
            
            float2 principalPoint = float2(0.0, 0.0);
            if (!generateFisheyeRay(input.position.xy, viewport, fisheyeFovRad, principalPoint, viewInverse, rayOrigin, rayDirection))
            {
                discard;
            }
        }
        else
        {
            generatePinholeRay(input.position.xy, viewport, viewInverse, projInverse, rayOrigin, rayDirection);
        }
        
        // Apply DOF perturbation
        uint seed = uint(input.position.x) * 1973u + uint(input.position.y) * 9277u + frame_constants.frame_index * 26699u;
        float focusDist = frame_constants.focus_distance;
        float aperture = frame_constants.aperture;
        
        depthOfField(seed, focusDist, aperture, viewInverse, rayOrigin, rayDirection);
        
        // Transform ray to model space
        float4x4 modelMatrixInverse = inverse4x4(model_matrix);
        float3 modelRayOrigin = mul(float4(rayOrigin, 1.0), modelMatrixInverse).xyz;
        float3x3 modelMatrixRotScaleInverse = float3x3(
            modelMatrixInverse[0].xyz,
            modelMatrixInverse[1].xyz,
            modelMatrixInverse[2].xyz
        );
        float3 modelRayDirection = normalize(mul(rayDirection, modelMatrixRotScaleInverse));
        
        // Process hit using ray-based evaluation
        float opacity;
        bool acceptedHit = particleProcessHitRay(modelRayOrigin, modelRayDirection,
                                                  input.splatCol, input.splatPosition,
                                                  input.splatScale, input.splatRotation,
                                                  opacity);
        
        if (!acceptedHit)
            discard;
        
        alpha = opacity;
    }
    else
    {
        // ============ NO DOF: Use Efficient 2D Gaussian Formula ============
        const float A = dot(input.fragPos, input.fragPos);
        
        if (A > 8.0)
            discard;
        
        const float power = -0.5 * A;
        alpha = min(0.999, input.splatCol.w * exp(power));
    }
    
#if OUTLINE_PASS
    const int mode = locked_color.w;
    output.color = float4(1.0, 1.0, 1.0, mode == 1 ? 1.0 : alpha);
    return output;
#endif

#if VISUALIZE_RINGS
    const float ringCenter = 1.0;
    const float ringSize = 0.1;
    const float centerAlpha = 0.4;
    const float ringAlpha = 0.90;
    
    const float A = dot(input.fragPos, input.fragPos);
    float distToRing = abs(A - ringCenter);
    
    if (distToRing < ringSize)
        alpha = ringAlpha;
    else if (A < ringCenter)
        alpha = alpha * centerAlpha;
    else
        alpha = alpha * 0.15;
    
    output.color = float4(input.splatCol.xyz, alpha);
    return output;
#elif VISUALIZE_ELLIPSOIDS
    output.color = float4(input.splatCol.xyz, 1.0);
    return output;
#endif

    output.color = float4(input.splatCol.xyz, alpha);
    return output;
}
