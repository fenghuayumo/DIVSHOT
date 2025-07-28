
#ifndef RTXDI_BRIDEGE_HLSL
#define RTXDI_BRIDEGE_HLSL

#define RTXDI_PRESAMPLING_GROUP_SIZE 256
#define RTXDI_GRID_BUILD_GROUP_SIZE 256
#define RTXDI_SCREEN_SPACE_GROUP_SIZE 8
#define RTXDI_GRAD_FACTOR 3
#define RTXDI_GRAD_STORAGE_SCALE 256.0f
#define RTXDI_GRAD_MAX_VALUE 65504.0f

#include "../inc/frame_constants.hlsl"
#include "../inc/random.hlsl"
#include "../inc/hash.hlsl"
#include "../inc/brdf.hlsl"
#include "../inc/brdf_lut.hlsl"
#include "../inc/layered_brdf.hlsl"
#include "../inc/rt.hlsl"
#include "rtxdi_parameters.hlsl"
#include "rtxdi_math.hlsl"
#include "../inc/bindless_textures.hlsl"
// #include "../inc/samplers.hlsl"
#include "RAB_Buffers.hlsli"
#include "../inc/lights/light_common.hlsl"

[[vk::binding(0, 3)]] RaytracingAccelerationStructure acceleration_structure;

typedef PolymorphicLightInfo RAB_LightInfo;
typedef LayeredBrdf RAB_Material;

struct RAB_Surface
{
    float3 worldPos;
    float3 viewDir;
    float3 normal;
    // float3 geoNormal;
    float viewDepth;
    RAB_Material material;
};

// Translates the light index from the current frame to the previous frame (if currentToPrevious = true)
// or from the previous frame to the current frame (if currentToPrevious = false).
// Returns the new index, or a negative number if the light does not exist in the other frame.
int RAB_TranslateLightIndex(uint lightIndex, bool currentToPrevious)
{
    // In this implementation, the mapping buffer contains both forward and reverse mappings,
    // stored at different offsets, so we don't care about the currentToPrevious parameter.
    uint mappedIndexPlusOne = t_LightIndexMappingBuffer[lightIndex];

    // The mappings are stored offset by 1 to differentiate between valid and invalid mappings.
    // The buffer is cleared with zeros which indicate an invalid mapping.
    // Subtract that one to make this function return expected values.
    return int(mappedIndexPlusOne) - 1;
}

RAB_LightInfo RAB_EmptyLightInfo()
{
    return (RAB_LightInfo)0;
}

// Loads polymorphic light data from the global light buffer.
RAB_LightInfo RAB_LoadLightInfo(uint index, bool previousFrame)
{
    return t_LightDataBuffer[index];
}

// Loads triangle light data from a tile produced by the presampling pass.
RAB_LightInfo RAB_LoadCompactLightInfo(uint linearIndex)
{
    uint4 packedData1, packedData2;
    packedData1 = u_RisLightDataBuffer[linearIndex * 2 + 0];
    packedData2 = u_RisLightDataBuffer[linearIndex * 2 + 1];
    return unpackCompactLightInfo(packedData1, packedData2);
}

// Stores triangle light data into a tile.
// Returns true if this light can be stored in a tile (i.e. compacted).
// If it cannot, for example it's a shaped light, this function returns false and doesn't store.
// A basic implementation can ignore this feature and always return false, which is just slower.
bool RAB_StoreCompactLightInfo(uint linearIndex, RAB_LightInfo lightInfo)
{
    uint4 data1, data2;
    if (!packCompactLightInfo(lightInfo, data1, data2))
        return false;

    u_RisLightDataBuffer[linearIndex * 2 + 0] = data1;
    u_RisLightDataBuffer[linearIndex * 2 + 1] = data2;

    return true;
}

// Computes the weight of the given light for arbitrary surfaces located inside
// the specified volume. Used for world-space light grid construction.
float RAB_GetLightTargetPdfForVolume(RAB_LightInfo light, float3 volumeCenter, float volumeRadius)
{
    return PolymorphicLight::getWeightForVolume(light, volumeCenter, volumeRadius);
}

struct RAB_LightSample
{
    float3 position;
    float3 normal;
    float3 radiance;
    float solidAnglePdf;
    PolymorphicLightType lightType;
};

RAB_LightSample RAB_EmptyLightSample()
{
    return (RAB_LightSample)0;
}

bool RAB_IsAnalyticLightSample(RAB_LightSample lightSample)
{
    return lightSample.lightType != PolymorphicLightType::kTriangle &&
           lightSample.lightType != PolymorphicLightType::kEnvironment;
}

float RAB_LightSampleSolidAnglePdf(RAB_LightSample lightSample)
{
    return lightSample.solidAnglePdf;
}

// Samples a polymorphic light relative to the given receiver surface.
// For most light types, the "uv" parameter is just a pair of uniform random numbers, originally
// produced by the RAB_GetNextRandom function and then stored in light reservoirs.
// For importance sampled environment lights, the "uv" parameter has the texture coordinates
// in the PDF texture, normalized to the (0..1) range.
RAB_LightSample RAB_SamplePolymorphicLight(RAB_LightInfo lightInfo, RAB_Surface surface, float2 uv)
{
    PolymorphicLightSample pls = PolymorphicLight::calcSample(lightInfo, uv, surface.worldPos);

    RAB_LightSample lightSample;
    lightSample.position = pls.position;
    lightSample.normal = pls.normal;
    lightSample.radiance = pls.radiance;
    lightSample.solidAnglePdf = pls.solidAnglePdf;
    lightSample.lightType = getLightType(lightInfo);
    return lightSample;
}

float2 RAB_GetEnvironmentMapRandXYFromDir(float3 worldDir)
{
    float2 uv = dir_2_equirect_uv(worldDir);
    // uv.x -= g_Const.sceneConstants.environmentRotation;
    uv = frac(uv);
    return uv;
}

// Computes the probability of a particular direction being sampled from the environment map
// relative to all the other possible directions, based on the environment map pdf texture.
float RAB_EvaluateEnvironmentMapSamplingPdf(float3 L)
{
    if (!g_Const.restirDI.initialSamplingParams.environmentMapImportanceSampling)
        return 1.0;

    float2 uv = RAB_GetEnvironmentMapRandXYFromDir(L);

    uint2 pdfTextureSize = g_Const.environmentPdfTextureSize.xy;
    uint2 texelPosition = uint2(pdfTextureSize * uv);
    float texelValue = t_envmap_pdf_tex[texelPosition].r;

    int lastMipLevel = max(0, int(floor(log2(max(pdfTextureSize.x, pdfTextureSize.y)))));
    float averageValue = t_envmap_pdf_tex.mips[lastMipLevel][uint2(0, 0)].x;

    // The single texel in the last mip level is effectively the average of all texels in mip 0,
    // padded to a square shape with zeros. So, in case the PDF texture has a 2:1 aspect ratio,
    // that texel's value is only half of the true average of the rectangular input texture.
    // Compensate for that by assuming that the input texture is square.
    float sum = averageValue * square(1u << lastMipLevel);

    return texelValue / sum;
}

// Evaluates pdf for a particular light
float RAB_EvaluateLocalLightSourcePdf(uint lightIndex)
{
    uint2 pdfTextureSize = g_Const.localLightPdfTextureSize.xy;
    uint2 texelPosition = linear_index_to_z_curve(lightIndex);
    float texelValue = t_local_light_pdf_tex[texelPosition].r;

    int lastMipLevel = max(0, int(floor(log2(max(pdfTextureSize.x, pdfTextureSize.y)))));
    float averageValue = t_local_light_pdf_tex.mips[lastMipLevel][uint2(0, 0)].x;

    // See the comment at 'sum' in RAB_EvaluateEnvironmentMapSamplingPdf.
    // The same texture shape considerations apply to local lights.
    float sum = averageValue * square(1u << lastMipLevel);

    return texelValue / sum;
}

float3 RAB_GetReflectedRadianceForSurface(float3 incomingRadianceLocation, float3 incomingRadiance, RAB_Surface surface)
{
    float3 L = normalize(incomingRadianceLocation - surface.worldPos);
    float3 N = surface.normal;
    float3 V = surface.viewDir;

    LayeredBrdf brdf = surface.material;
    const float3x3 tangent_to_world = build_orthonormal_basis(N);
    float3 wi = mul(L, tangent_to_world);
    if (wi.z <= 0) return 0;
    float3 wo = mul(V, tangent_to_world);
    if (wo.z < 0.0) {
        wo.z *= -0.25;
        wo = normalize(wo);
    }
    float3 value = brdf.evaluate(wo, wi);
    return incomingRadiance * value * max(0.0, wi.z);
}

float RAB_GetReflectedLuminanceForSurface(float3 incomingRadianceLocation, float3 incomingRadiance, RAB_Surface surface)
{
    return RTXDI_Luminance(RAB_GetReflectedRadianceForSurface(incomingRadianceLocation, incomingRadiance, surface));
}

// Computes the weight of the given light samples when the given surface is
// shaded using that light sample. Exact or approximate BRDF evaluation can be
// used to compute the weight. ReSTIR will converge to a correct lighting result
// even if all samples have a fixed weight of 1.0, but that will be very noisy.
// Scaling of the weights can be arbitrary, as long as it's consistent
// between all lights and surfaces.
float RAB_GetLightSampleTargetPdfForSurface(RAB_LightSample lightSample, RAB_Surface surface)
{
    if (lightSample.solidAnglePdf <= 0)
        return 0;

    return RAB_GetReflectedLuminanceForSurface(lightSample.position, lightSample.radiance, surface) / lightSample.solidAnglePdf;
}

// Computes the weight of the given GI sample when the given surface is shaded using that GI sample.
float RAB_GetGISampleTargetPdfForSurface(float3 samplePosition, float3 sampleRadiance, RAB_Surface surface)
{
    float3 reflectedRadiance = RAB_GetReflectedRadianceForSurface(samplePosition, sampleRadiance, surface);

    return RTXDI_Luminance(reflectedRadiance);
}

void RAB_GetLightDirDistance(RAB_Surface surface, RAB_LightSample lightSample,
                             out float3 o_lightDir,
                             out float o_lightDistance)
{
    if (lightSample.lightType == PolymorphicLightType::kEnvironment)
    {
        o_lightDir = -lightSample.normal;
        o_lightDistance = DISTANT_LIGHT_DISTANCE;
    }
    else
    {
        float3 toLight = lightSample.position - surface.worldPos;
        o_lightDistance = length(toLight);
        o_lightDir = toLight / o_lightDistance;
    }
}

// Forward declare the SDK function that's used in RAB_AreMaterialsSimilar
bool RTXDI_CompareRelativeDifference(float reference, float candidate, float threshold);

float3 GetEnvironmentRadiance(float3 direction)
{
    float2 uv = dir_2_equirect_uv(direction);
    float3 environmentRadiance = t_envmap_tex.SampleLevel(sampler_llc, uv, 0).rgb;

    return environmentRadiance;
}


// uint getLightIndex(uint instanceID, uint geometryIndex, uint primitiveIndex)
// {
//     uint lightIndex = RTXDI_InvalidLightIndex;
//     InstanceData hitInstance = t_InstanceData[instanceID];
//     uint geometryInstanceIndex = hitInstance.firstGeometryInstanceIndex + geometryIndex;
//     lightIndex = t_GeometryInstanceToLight[geometryInstanceIndex];
//     if (lightIndex != RTXDI_InvalidLightIndex)
//         lightIndex += primitiveIndex;
//     return lightIndex;
// }

// Return true if anything was hit. If false, RTXDI will do environment map sampling
// o_lightIndex: If hit, must be a valid light index for RAB_LoadLightInfo, if no local light was hit, must be RTXDI_InvalidLightIndex
// randXY: The randXY that corresponds to the hit location and is the same used for RAB_SamplePolymorphicLight
bool RAB_TraceRayForLocalLight(float3 origin, float3 direction, float tMin, float tMax,
                               out uint o_lightIndex, out float2 o_randXY)
{
    o_lightIndex = RTXDI_InvalidLightIndex;
    o_randXY = 0;

//     RayDesc ray;
//     ray.Origin = origin;
//     ray.Direction = direction;
//     ray.TMin = tMin;
//     ray.TMax = tMax;

//     float2 hitUV;
    bool hitAnything = false;
// #if USE_RAY_QUERY
//     RayQuery<RAY_FLAG_CULL_NON_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> rayQuery;
//     rayQuery.TraceRayInline(SceneBVH, RAY_FLAG_NONE, INSTANCE_MASK_OPAQUE, ray);
//     rayQuery.Proceed();

//     hitAnything = rayQuery.CommittedStatus() == COMMITTED_TRIANGLE_HIT;
//     if (hitAnything)
//     {
//         o_lightIndex = getLightIndex(rayQuery.CommittedInstanceID(), rayQuery.CommittedGeometryIndex(), rayQuery.CommittedPrimitiveIndex());
//         hitUV = rayQuery.CommittedTriangleBarycentrics();
//     }
// #else
//     RayPayload payload = (RayPayload)0;
//     payload.instanceID = ~0u;
//     payload.throughput = 1.0;

//     TraceRay(acceleration_structure, RAY_FLAG_CULL_NON_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES, INSTANCE_MASK_OPAQUE, 0, 0, 0, ray, payload);
//     hitAnything = payload.instanceID != ~0u;
//     if (hitAnything)
//     {
//         o_lightIndex = getLightIndex(payload.instanceID, payload.geometryIndex, payload.primitiveIndex);
//         hitUV = payload.barycentrics;
//     }
// #endif

//     if (o_lightIndex != RTXDI_InvalidLightIndex)
//     {
//         o_randXY = randomFromBarycentric(hitUVToBarycentric(hitUV));
//     }

    return hitAnything;
}

static const float kMinRoughness = 0.05f;

RAB_Material RAB_EmptyMaterial()
{
    RAB_Material material = (RAB_Material)0;

    return material;
}

float3 GetDiffuseAlbedo(RAB_Material material)
{
    return material.diffuse_brdf.albedo;
}

float3 GetSpecularF0(RAB_Material material)
{
    return material.specular_brdf.albedo;
}

float GetRoughness(RAB_Material material)
{
    return material.specular_brdf.roughness;
}

// Compares the materials of two surfaces, returns true if the surfaces
// are similar enough that we can share the light reservoirs between them.
// If unsure, just return true.
bool RAB_AreMaterialsSimilar(RAB_Material a, RAB_Material b)
{
    const float roughnessThreshold = 0.5;
    const float reflectivityThreshold = 0.25;
    const float albedoThreshold = 0.25;

    if (!RTXDI_CompareRelativeDifference(a.specular_brdf.roughness, b.specular_brdf.roughness, roughnessThreshold))
        return false;

    if (abs(sRGB_to_luminance(a.specular_brdf.albedo) - sRGB_to_luminance(b.specular_brdf.albedo)) > reflectivityThreshold)
        return false;

    if (abs(sRGB_to_luminance(a.diffuse_brdf.albedo) - sRGB_to_luminance(b.diffuse_brdf.albedo)) > albedoThreshold)
        return false;

    return true;
}

typedef Random RAB_RandomSamplerState;

// Initialized the random sampler for a given pixel or tile index.
// The pass parameter is provided to help generate different RNG sequences
// for different resampling passes, which is important for image quality.
// In general, a high quality RNG is critical to get good results from ReSTIR.
// A table-based blue noise RNG dose not provide enough entropy, for example.
RAB_RandomSamplerState RAB_InitRandomSampler(uint2 index, uint pass)
{
    return make_random(index, frame_constants.frame_index + pass * 13);
}

// Draws a random number X from the sampler, so that (0 <= X < 1).
float RAB_GetNextRandom(inout RAB_RandomSamplerState rng)
{
    return uniform_rand_float(rng);
}

// This function is called in the spatial resampling passes to make sure that
// the samples actually land on the screen and not outside of its boundaries.
// It can clamp the position or reflect it across the nearest screen edge.
// The simplest implementation will just return the input pixelPosition.
int2 RAB_ClampSamplePositionIntoView(int2 pixelPosition, bool previousFrame)
{
    int width = int(frame_constants.view_constants.viewport_size.x);
    int height = int(frame_constants.view_constants.viewport_size.y);

    // Reflect the position across the screen edges.
    // Compared to simple clamping, this prevents the spread of colorful blobs from screen edges.
    if (pixelPosition.x < 0) pixelPosition.x = -pixelPosition.x;
    if (pixelPosition.y < 0) pixelPosition.y = -pixelPosition.y;
    if (pixelPosition.x >= width) pixelPosition.x = 2 * width - pixelPosition.x - 1;
    if (pixelPosition.y >= height) pixelPosition.y = 2 * height - pixelPosition.y - 1;

    return pixelPosition;
}

// Check if the sample is fine to be used as a valid spatial sample.
// This function also be able to clamp the value of the Jacobian.
bool RAB_ValidateGISampleWithJacobian(inout float jacobian)
{
    // Sold angle ratio is too different. Discard the sample.
    if (jacobian > 10.0 || jacobian < 1 / 10.0) {
        return false;
    }

    // clamp Jacobian.
    jacobian = clamp(jacobian, 1 / 3.0, 3.0);

    return true;
}

static const bool kSpecularOnly = false;


RAB_Surface RAB_EmptySurface()
{
    RAB_Surface surface = (RAB_Surface)0;
    surface.viewDepth = SKY_DIST;
    return surface;
}

// Checks if the given surface is valid, see RAB_GetGBufferSurface.
bool RAB_IsSurfaceValid(RAB_Surface surface)
{
    return surface.viewDepth != SKY_DIST;
}

// Returns the world position of the given surface
float3 RAB_GetSurfaceWorldPos(RAB_Surface surface)
{
    return surface.worldPos;
}

RAB_Material RAB_GetMaterial(RAB_Surface surface)
{
    return surface.material;
}

// Returns the world shading normal of the given surface
float3 RAB_GetSurfaceNormal(RAB_Surface surface)
{
    return surface.normal;
}

// Returns the linear depth of the given surface.
// It doesn't have to be linear depth in a strict sense (i.e. viewPos.z),
// and can be distance to the camera or primary path length instead.
// Just make sure that the motion vectors' .z component follows the same logic.
float RAB_GetSurfaceLinearDepth(RAB_Surface surface)
{
    return surface.viewDepth;
}

// float RAB_GetSurfacePrevLinearDepth(RAB_Surface surface)
// {

// }

RAB_Surface GetGBufferSurface(
    int2 px,
    bool previousFrame,
    Texture2D<float> depthTexture)
{
    RAB_Surface surface = RAB_EmptySurface();

    if (any(px >= frame_constants.view_constants.viewport_size.xy))
        return surface;
   
    const float depth = depthTexture[px];
    if (depth == FARZ)
        return surface;
    float2 uv = get_uv(px, frame_constants.view_constants.viewport_size);
    uint rng = hash3(uint3(px, frame_constants.frame_index));

    RayDesc outgoing_ray;
    const ViewRayContext view_ray_context = ViewRayContext::from_uv(uv);
    {
        outgoing_ray = new_ray(
            view_ray_context.ray_origin_ws(),
            view_ray_context.ray_dir_ws(),
            0.0,
            FLT_MAX
        );
    }

    float4 pt_cs = float4(uv_to_cs(uv), depth, 1.0);
    float4 pt_ws = mul(mul(pt_cs, frame_constants.view_constants.sample_to_view), frame_constants.view_constants.view_to_world);
    pt_ws /= pt_ws.w;
    GbufferData gbuffer = GbufferDataPacked::from_uint4(asuint(t_gbuffer_tex[px])).unpack();

    surface.viewDepth = depth_to_view_z(depth);
    // surface.geoNormal = octToNdirUnorm32(geoNormalsTexture[pixelPosition]);
    surface.normal = gbuffer.normal;
    surface.worldPos = pt_ws.xyz;
    surface.viewDir = normalize(get_eye_position() - surface.worldPos);
    // if(previousFrame)
    // {
    //     surface.material = GetGBufferMaterial(px, gbuffer);
    // }
    // else
    {
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
        surface.material = brdf;
    }
    return surface;
}

// Reads the G-buffer, either the current one or the previous one, and returns a surface.
// If the provided pixel position is outside of the viewport bounds, the surface
// should indicate that it's invalid when RAB_IsSurfaceValid is called on it.
RAB_Surface RAB_GetGBufferSurface(int2 pixelPosition, bool previousFrame)
{
    if (previousFrame)
    {
        return GetGBufferSurface(
            pixelPosition,
            previousFrame,
            t_gbuffer_depth);
    }
    else
    {
        return GetGBufferSurface(
            pixelPosition,
            previousFrame,
            t_gbuffer_depth);
    }
}

bool RAB_GetSurfaceBrdfSample(RAB_Surface surface, inout RAB_RandomSamplerState rng, out float3 dir)
{
    LayeredBrdf brdf = surface.material;
    float3 rand;
    rand.x = RAB_GetNextRandom(rng);
    rand.y = RAB_GetNextRandom(rng);
    rand.z = RAB_GetNextRandom(rng);
    const float3x3 tangent_to_world = build_orthonormal_basis(surface.normal);
    float3 wo = mul(surface.viewDir, tangent_to_world);
    BrdfSample brdf_sample = brdf.sample(wo, rand);
    dir = mul(tangent_to_world, brdf_sample.wi);
    return brdf_sample.is_valid();
}

float RAB_GetSurfaceBrdfPdf(RAB_Surface surface, float3 dir)
{
    LayeredBrdf brdf = surface.material;
    float pdf = 0.0f;
    const float3x3 tangent_to_world = build_orthonormal_basis(surface.normal);
    const float3 wo = mul(surface.viewDir, tangent_to_world);
    const float3 wi = mul(dir, tangent_to_world);
    brdf.evaluate_pdf(wo,wi, pdf);
    return pdf;
}

RayDesc setupVisibilityRay(RAB_Surface surface, float3 samplePosition, float offset = 0.001)
{
    float3 L = samplePosition - surface.worldPos;

    RayDesc ray;
    ray.TMin = offset;
    ray.TMax = max(offset, length(L) - offset * 2);
    ray.Direction = normalize(L);
    ray.Origin = surface.worldPos;

    return ray;
}

bool GetConservativeVisibility(RaytracingAccelerationStructure accelStruct, RAB_Surface surface, float3 samplePosition)
{
    RayDesc ray = setupVisibilityRay(surface, samplePosition);
    bool visible = rt_visibility(accelStruct, ray);
    return visible;
}

// Traces a cheap visibility ray that returns approximate, conservative visibility
// between the surface and the light sample. Conservative means if unsure, assume the light is visible.
// Significant differences between this conservative visibility and the final one will result in more noise.
// This function is used in the spatial resampling functions for ray traced bias correction.
bool RAB_GetConservativeVisibility(RAB_Surface surface, RAB_LightSample lightSample)
{
    return GetConservativeVisibility(acceleration_structure, surface, lightSample.position);
}

// Same as RAB_GetConservativeVisibility but for temporal resampling.
// When the previous frame TLAS and BLAS are available, the implementation should use the previous position and the previous AS.
// When they are not available, use the current AS. That will result in transient bias.
bool RAB_GetTemporalConservativeVisibility(RAB_Surface currentSurface, RAB_Surface previousSurface, RAB_LightSample lightSample)
{
    // if (g_Const.enablePreviousTLAS)
    //     return GetConservativeVisibility(prev_acceleration_structure, previousSurface, lightSample.position);
    // else
    return GetConservativeVisibility(acceleration_structure, currentSurface, lightSample.position);
}

// Traces an expensive visibility ray that considers all alpha tested  and transparent geometry along the way.
// Only used for final shading.
// Not a required bridge function.
float3 GetFinalVisibility(RaytracingAccelerationStructure accelStruct, RAB_Surface surface, float3 samplePosition)
{
    RayDesc ray = setupVisibilityRay(surface, samplePosition, 0.01);
    const bool visiblity = rt_visibility(accelStruct, ray);
    if (visiblity) {
        return 1.0.xxx;
    }
    else
        return 0;
}

// Traces a cheap visibility ray that returns approximate, conservative visibility
// between the surface and the light sample. Conservative means if unsure, assume the light is visible.
// Significant differences between this conservative visibility and the final one will result in more noise.
// This function is used in the spatial resampling functions for ray traced bias correction.
bool RAB_GetConservativeVisibility(RAB_Surface surface, float3 samplePosition)
{
    return GetConservativeVisibility(acceleration_structure, surface, samplePosition);
}

// Same as RAB_GetConservativeVisibility but for temporal resampling.
// When the previous frame TLAS and BLAS are available, the implementation should use the previous position and the previous AS.
// When they are not available, use the current AS. That will result in transient bias.
bool RAB_GetTemporalConservativeVisibility(RAB_Surface currentSurface, RAB_Surface previousSurface, float3 samplePosition)
{
    // if (g_Const.enablePreviousTLAS)
    //     return GetConservativeVisibility(prev_acceleration_structure, previousSurface, samplePosition);
    // else
    return GetConservativeVisibility(acceleration_structure, currentSurface, samplePosition);
}
#endif // RTXDI_APPLICATION_BRIDGE_HLSLI