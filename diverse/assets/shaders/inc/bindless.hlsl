#ifndef DS_BINDLESS_HLSL
#define DS_BINDLESS_HLSL

#include "binding.hlsl"
#include "mesh.hlsl"
DS_BINDLESS_RESOURCE(0) ByteAddressBuffer bindless_gaussians_buf[];
DS_BINDLESS_RESOURCE(1) RWByteAddressBuffer bindless_splat_state[];
DS_BINDLESS_RESOURCE(2) StructuredBuffer<Mesh> meshes_buffer;
DS_BINDLESS_RESOURCE(3) StructuredBuffer<MeshMaterial> materials_buffer;
DS_BINDLESS_RESOURCE(4) ByteAddressBuffer bindless_vb[];
DS_BINDLESS_RESOURCE(5) ByteAddressBuffer bindless_ib[];
#include "bindless_textures.hlsl"
DS_BINDLESS_RESOURCE(8) ByteAddressBuffer bindless_point_buf[];

#endif // DS_BINDLESS_HLSL
