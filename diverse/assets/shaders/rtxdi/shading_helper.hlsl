
#ifndef SHADING_HELPERS_HLSLI
#define SHADING_HELPERS_HLSLI
#if WITH_NRD
// #include "nrd_encoding.hlsl"
#include "../nrd/nrd.hlsl"
#endif
#define DENOISER_MODE_OFF 0
#define DENOISER_MODE_REBLUR 1
#define DENOISER_MODE_RELAX 2

struct SplitBrdf
{
    float3 diffuse;
    float3 specular;
};

SplitBrdf EvaluateBrdf(RAB_Surface surface, float3 samplePosition)
{
    float3 N = surface.normal;
    float3 V = surface.viewDir;
    float3 L = normalize(samplePosition - surface.worldPos);
    LayeredBrdf layered_brdf = surface.material;
    const float3x3 tangent_to_world = build_orthonormal_basis(N);
    const float3 wi = mul(L, tangent_to_world);
    float3 wo = mul(V, tangent_to_world);
    if (wo.z < 0.0) {
        wo.z *= -0.25;
        wo = normalize(wo);
    }
    SplitBrdf brdf;
    layered_brdf.eval_diff_spec(wo,wi,brdf.diffuse, brdf.specular);
    brdf.diffuse *= max(0.0, wi.z);
    brdf.specular *= max(0.0, wi.z);
    return brdf;
}

#ifdef RTXDI_DIRESERVOIR_HLSLI

bool ShadeSurfaceWithLightSample(
    inout RTXDI_DIReservoir reservoir,
    RAB_Surface surface,
    RAB_LightSample lightSample,
    bool previousFrameTLAS,
    bool enableVisibilityReuse,
    out float3 diffuse,
    out float3 specular,
    out float lightDistance)
{
    diffuse = 0;
    specular = 0;
    lightDistance = 0;

    if (lightSample.solidAnglePdf <= 0)
        return false;

    bool needToStore = false;
    if (g_Const.restirDI.shadingParams.enableFinalVisibility)
    {
        float3 visibility = 0;
        bool visibilityReused = false;

        if (g_Const.restirDI.shadingParams.reuseFinalVisibility && enableVisibilityReuse)
        {
            RTXDI_VisibilityReuseParameters rparams;
            rparams.maxAge = g_Const.restirDI.shadingParams.finalVisibilityMaxAge;
            rparams.maxDistance = g_Const.restirDI.shadingParams.finalVisibilityMaxDistance;

            visibilityReused = RTXDI_GetDIReservoirVisibility(reservoir, rparams, visibility);
        }

        if (!visibilityReused)
        {
            // if (previousFrameTLAS && g_Const.enablePreviousTLAS)
            //     visibility = GetFinalVisibility(acceleration_structure, surface, lightSample.position);
            // else
            visibility = GetFinalVisibility(acceleration_structure, surface, lightSample.position);
            RTXDI_StoreVisibilityInDIReservoir(reservoir, visibility, g_Const.restirDI.temporalResamplingParams.discardInvisibleSamples);
            needToStore = true;
        }

        lightSample.radiance *= visibility;
    }

    lightSample.radiance *= RTXDI_GetDIReservoirInvPdf(reservoir) / lightSample.solidAnglePdf;

    if (any(lightSample.radiance > 0))
    {
        SplitBrdf brdf = EvaluateBrdf(surface, lightSample.position);

        diffuse =  lightSample.radiance;
        specular = lightSample.radiance;

        lightDistance = length(lightSample.position - surface.worldPos);
    }

    return needToStore;
}

#endif // RTXDI_DIRESERVOIR_HLSLI

float3 DemodulateSpecular(float3 surfaceSpecularF0, float3 specular)
{
    return specular / max(0.01, surfaceSpecularF0);
}


float3 DemodulateDiffuse(float3 diffuseAlbedo, float3 diffuse)
{
    return diffuse / max(0.01, diffuseAlbedo);
}


void StoreShadingOutput(
    uint2 reservoirPosition,
    uint2 pixelPosition,
    float viewDepth,
    float roughness,
    float3 diffuse,
    float3 specular,
    float lightDistance,
    bool isFirstPass,
    bool isLastPass)
{
    uint2 lightingTexturePos = (g_Const.denoiserMode != DENOISER_MODE_OFF)
        ? reservoirPosition
        : pixelPosition;

    float diffuseHitT = lightDistance;
    float specularHitT = lightDistance;

    if (!isFirstPass)
    {
        float4 priorDiffuse = u_DiffuseLighting[lightingTexturePos];
        float4 priorSpecular = u_SpecularLighting[lightingTexturePos];

        if (sRGB_to_luminance(diffuse) > sRGB_to_luminance(priorDiffuse.rgb) || lightDistance == 0)
            diffuseHitT = priorDiffuse.w;

        if (sRGB_to_luminance(specular) > sRGB_to_luminance(priorSpecular.rgb) || lightDistance == 0)
            specularHitT = priorSpecular.w;
        
        diffuse += priorDiffuse.rgb;
        specular += priorSpecular.rgb;
    }

    if (g_Const.denoiserMode == DENOISER_MODE_OFF && g_Const.runtimeParams.activeCheckerboardField != 0 && isLastPass)
    {
        int2 otherFieldPixelPosition = pixelPosition;
        otherFieldPixelPosition.x += (g_Const.runtimeParams.activeCheckerboardField == 1) == ((pixelPosition.y & 1) != 0)
            ? 1 : -1;

        if (g_Const.denoiserMode == DENOISER_MODE_RELAX )
        {
            diffuse *= 2;
            specular *= 2;

            u_DiffuseLighting[otherFieldPixelPosition] = 0;
            u_SpecularLighting[otherFieldPixelPosition] = 0;
        }
        else // g_Const.denoiserMode == DENOISER_MODE_OFF
        {
            u_DiffuseLighting[otherFieldPixelPosition] = float4(diffuse, 0);
            u_SpecularLighting[otherFieldPixelPosition] = float4(specular, 0);
        }
    }

#if WITH_NRD
    if(g_Const.denoiserMode != DENOISER_MODE_OFF && isLastPass)
    {
        const bool useReLAX = (g_Const.denoiserMode == DENOISER_MODE_RELAX);
        const bool sanitize = true;
 
        if (useReLAX)
        {
            u_DiffuseLighting[lightingTexturePos] = RELAX_FrontEnd_PackRadianceAndHitDist(diffuse, diffuseHitT, sanitize);
            u_SpecularLighting[lightingTexturePos] = RELAX_FrontEnd_PackRadianceAndHitDist(specular, specularHitT, sanitize);
        }
        else
        {
            float diffNormDist = REBLUR_FrontEnd_GetNormHitDist(diffuseHitT, viewDepth, g_Const.reblurDiffHitDistParams, 1.0);
            u_DiffuseLighting[lightingTexturePos] = REBLUR_FrontEnd_PackRadianceAndNormHitDist(diffuse, diffNormDist, sanitize);
            
            float specNormDist = REBLUR_FrontEnd_GetNormHitDist(specularHitT, viewDepth, g_Const.reblurSpecHitDistParams, roughness);
            u_SpecularLighting[lightingTexturePos] = REBLUR_FrontEnd_PackRadianceAndNormHitDist(specular, specNormDist, sanitize);
        }
    }
    else
#endif
    {
        u_DiffuseLighting[lightingTexturePos] = float4(diffuse, diffuseHitT);
        u_SpecularLighting[lightingTexturePos] = float4(specular, specularHitT);
    }
}

#endif // SHADING_HELPERS_HLSLI