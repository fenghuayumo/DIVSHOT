
#ifndef RTXDI_REGIR_PARAMETERS_H
#define RTXDI_REGIR_PARAMETERS_H

#define RTXDI_ONION_MAX_LAYER_GROUPS 8
#define RTXDI_ONION_MAX_RINGS 52

#define RTXDI_REGIR_DISABLED 0
#define RTXDI_REGIR_GRID 1
#define RTXDI_REGIR_ONION 2

#ifndef RTXDI_REGIR_MODE
#define RTXDI_REGIR_MODE RTXDI_REGIR_DISABLED
#endif 

struct ReGIR_OnionLayerGroup
{
    float innerRadius;
    float outerRadius;
    float invLogLayerScale;
    int layerCount;

    float invEquatorialCellAngle;
    int cellsPerLayer;
    int ringOffset;
    int ringCount;

    float equatorialCellAngle;
    float layerScale;
    int layerCellOffset;
    int pad1;
};

struct ReGIR_OnionRing
{
    float cellAngle;
    float invCellAngle;
    int cellOffset;
    int cellCount;
};

#define REGIR_LOCAL_LIGHT_PRESAMPLING_MODE_UNIFORM 0
#define REGIR_LOCAL_LIGHT_PRESAMPLING_MODE_POWER_RIS 1

#define REGIR_LOCAL_LIGHT_FALLBACK_MODE_UNIFORM 0
#define REGIR_LOCAL_LIGHT_FALLBACK_MODE_POWER_RIS 1

struct ReGIR_CommonParameters
{
    uint localLightSamplingFallbackMode;
    float centerX;
    float centerY;
    float centerZ;

    uint risBufferOffset;
    uint lightsPerCell;
    float cellSize;
    float samplingJitter;

    uint localLightPresamplingMode;
    uint numRegirBuildSamples; // PresampleReGIR.hlsl -> RTXDI_PresampleLocalLightsForReGIR
    uint pad1;
    uint pad2;
};

struct ReGIR_GridParameters
{
    uint cellsX;
    uint cellsY;
    uint cellsZ;
    uint pad1;
};

struct ReGIR_OnionParameters
{
    ReGIR_OnionLayerGroup layers[RTXDI_ONION_MAX_LAYER_GROUPS];
    ReGIR_OnionRing rings[RTXDI_ONION_MAX_RINGS];

    uint numLayerGroups;
    float cubicRootFactor;
    float linearFactor;
    float pad1;
};

struct ReGIR_Parameters
{
    ReGIR_CommonParameters commonParams;
    ReGIR_GridParameters gridParams;
    ReGIR_OnionParameters onionParams;
};

#endif // RTXDI_REGIR_PARAMETERS_H
