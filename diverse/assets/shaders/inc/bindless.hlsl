#include "mesh.hlsl"
[[vk::binding(0, 1)]] ByteAddressBuffer bindless_gaussians_buf[];
[[vk::binding(1, 1)]] RWByteAddressBuffer bindless_splat_state[];
[[vk::binding(2, 1)]] StructuredBuffer<Mesh> meshes_buffer;
[[vk::binding(3, 1)]] StructuredBuffer<MeshMaterial> materials_buffer;
[[vk::binding(4, 1)]] ByteAddressBuffer bindless_vb[];
[[vk::binding(5, 1)]] ByteAddressBuffer bindless_ib[];
#include "bindless_textures.hlsl"
[[vk::binding(8, 1)]] ByteAddressBuffer bindless_point_buf[];
