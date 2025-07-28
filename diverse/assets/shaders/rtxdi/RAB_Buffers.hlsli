struct ResamplingConstants
{
    RTXDI_RuntimeParameters runtimeParams;
    // Common buffer params
    RTXDI_LightBufferParameters lightBufferParams;
    RTXDI_RISBufferSegmentParameters localLightsRISBufferSegmentParams;
    RTXDI_RISBufferSegmentParameters environmentLightRISBufferSegmentParams;

    ReSTIRDI_Parameters restirDI;
    ReGIR_Parameters regir;

    uint2 environmentPdfTextureSize;
    uint2 localLightPdfTextureSize;

    uint enableBrdfAdditiveBlend;
    uint enablePreviousTLAS;
    uint denoiserMode;
    uint discountNaiveSamples;
};

[[vk::binding(0)]]  ConstantBuffer<ResamplingConstants> g_Const;
[[vk::binding(1)]]  Texture2D<float4> t_gbuffer_tex;
[[vk::binding(2)]]  Texture2D<float> t_gbuffer_depth;
[[vk::binding(3)]]  Texture2D<float4> t_envmap_tex;
[[vk::binding(4)]]  Texture2D<float> t_envmap_pdf_tex;
[[vk::binding(5)]]  Texture2D<float> t_local_light_pdf_tex;
[[vk::binding(6)]]  StructuredBuffer<PolymorphicLightInfo> t_LightDataBuffer;
[[vk::binding(7)]]  Buffer<float2> t_NeighborOffsets;
[[vk::binding(8)]]  Buffer<uint> t_LightIndexMappingBuffer;
[[vk::binding(9)]]  RWStructuredBuffer<RTXDI_PackedDIReservoir> u_LightReservoirs;
[[vk::binding(10)]]  RWBuffer<uint2> u_RisBuffer;
[[vk::binding(11)]]  RWBuffer<uint4> u_RisLightDataBuffer;
#define RTXDI_RIS_BUFFER u_RisBuffer
#define RTXDI_LIGHT_RESERVOIR_BUFFER u_LightReservoirs
#define RTXDI_NEIGHBOR_OFFSETS_BUFFER t_NeighborOffsets
// #define RTXDI_GI_RESERVOIR_BUFFER u_GIReservoirs

#ifndef ISE_SAMPLER
#define IES_SAMPLER sampler_llc
#endif

