#include "rtxdi_bridege.hlsl"
#include "reservoir.hlsl"

#if RTXDI_REGIR_MODE != RTXDI_REGIR_DISABLED
#include "regir/regir_sampling.hlsl"
#endif

[[vk::binding(12)]] RWTexture2D<float4> u_DiffuseLighting;
[[vk::binding(13)]] RWTexture2D<float4> u_SpecularLighting;
#include "shading_helper.hlsl"

#if USE_RAY_QUERY
[numthreads(RTXDI_SCREEN_SPACE_GROUP_SIZE, RTXDI_SCREEN_SPACE_GROUP_SIZE, 1)]
void main(uint2 GlobalIndex : SV_DispatchThreadID, uint2 LocalIndex : SV_GroupThreadID, uint2 GroupIdx : SV_GroupID)
#else
[shader("raygeneration")]
void main()
#endif
{
#if !USE_RAY_QUERY
    uint2 GlobalIndex = DispatchRaysIndex().xy;
#endif

    const RTXDI_RuntimeParameters params = g_Const.runtimeParams;

    uint2 pixelPosition = RTXDI_ReservoirPosToPixelPos(GlobalIndex, params.activeCheckerboardField);

    RAB_Surface surface = RAB_GetGBufferSurface(pixelPosition, false);

    RTXDI_DIReservoir reservoir = RTXDI_LoadDIReservoir(g_Const.restirDI.reservoirBufferParams, GlobalIndex, g_Const.restirDI.bufferIndices.shadingInputBufferIndex);

    float3 diffuse = 0;
    float3 specular = 0;
    float lightDistance = 0;
    float2 currLuminance = 0;

    if (RTXDI_IsValidDIReservoir(reservoir))
    {
        RAB_LightInfo lightInfo = RAB_LoadLightInfo(RTXDI_GetDIReservoirLightIndex(reservoir), false);

        RAB_LightSample lightSample = RAB_SamplePolymorphicLight(lightInfo,
            surface, RTXDI_GetDIReservoirSampleUV(reservoir));

        bool needToStore = ShadeSurfaceWithLightSample(reservoir, surface, lightSample,
             false, true, diffuse, specular, lightDistance);

        // currLuminance = float2(sRGB_to_luminance(diffuse), sRGB_to_luminance(specular));

        specular = DemodulateSpecular(surface.material.specular_brdf.albedo, specular);
        diffuse = DemodulateDiffuse(surface.material.diffuse_brdf.albedo, diffuse);

        if (needToStore)
        {
            RTXDI_StoreDIReservoir(reservoir, g_Const.restirDI.reservoirBufferParams, GlobalIndex, g_Const.restirDI.bufferIndices.shadingInputBufferIndex);
        }
    }

    // Store the sampled lighting luminance for the gradient pass.
    // Discard the pixels where the visibility was reused, as gradients need actual visibility.
    // u_RestirLuminance[GlobalIndex] = currLuminance * (reservoir.age > 0 ? 0 : 1);

    StoreShadingOutput(GlobalIndex, pixelPosition, 
        surface.viewDepth, surface.material.specular_brdf.roughness, diffuse, specular, lightDistance, true, g_Const.restirDI.shadingParams.enableDenoiserInputPacking);
}
