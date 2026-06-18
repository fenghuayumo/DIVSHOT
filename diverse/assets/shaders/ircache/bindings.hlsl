#include "../inc/binding.hlsl"
#ifndef IRCACHE_BINDINGS_HLSL
#define IRCACHE_BINDINGS_HLSL

#include "../inc/mesh.hlsl" // for VertexPacked

#define DEFINE_IRCACHE_BINDINGS(b0, b1, b2, b3, b4, b5, b6, b7, b8) \
    DS_RESOURCE(b0) RWByteAddressBuffer ircache_meta_buf; \
    DS_RESOURCE(b1) RWStructuredBuffer<uint> ircache_pool_buf; \
    DS_RESOURCE(b2) RWStructuredBuffer<VertexPacked> ircache_reposition_proposal_buf; \
    DS_RESOURCE(b3) RWStructuredBuffer<uint> ircache_reposition_proposal_count_buf; \
    DS_RESOURCE(b4) RWByteAddressBuffer ircache_grid_meta_buf; \
    DS_RESOURCE(b5) RWStructuredBuffer<uint> ircache_entry_cell_buf; \
    DS_RESOURCE(b6) StructuredBuffer<VertexPacked> ircache_spatial_buf; \
    DS_RESOURCE(b7) StructuredBuffer<float4> ircache_irradiance_buf; \
    DS_RESOURCE(b8) RWByteAddressBuffer ircache_life_buf;

#endif  // IRCACHE_BINDINGS_HLSL
