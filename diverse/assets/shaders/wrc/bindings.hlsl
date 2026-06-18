#include "../inc/binding.hlsl"
#ifndef WRC_BINDINGS_HLSL
#define WRC_BINDINGS_HLSL

#define DEFINE_WRC_BINDINGS(b0) \
    DS_RESOURCE(b0) Texture2D<float4> wrc_radiance_atlas_tex;

#endif  // WRC_BINDINGS_HLSL