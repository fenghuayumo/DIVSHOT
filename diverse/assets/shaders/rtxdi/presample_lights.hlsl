#include "rtxdi_bridege.hlsl"
#include "light_sampling/pre_sampling_functions.hlsl"

[numthreads(RTXDI_PRESAMPLING_GROUP_SIZE, 1, 1)]
void main(uint2 GlobalIndex: SV_DispatchThreadID)
{
    RAB_RandomSamplerState rng = RAB_InitRandomSampler(GlobalIndex.xy, 0);

    RTXDI_PresampleLocalLights(
        rng,
        t_local_light_pdf_tex,
        g_Const.localLightPdfTextureSize,
        GlobalIndex.y,
        GlobalIndex.x,
        g_Const.lightBufferParams.localLightBufferRegion,
        g_Const.localLightsRISBufferSegmentParams);
}