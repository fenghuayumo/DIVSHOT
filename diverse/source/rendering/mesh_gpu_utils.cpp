#include "mesh_gpu_utils.h"
#include "utility/pack_utils.h"
#include "utility/thread_pool.h"
#include <cstring>

namespace diverse
{
    namespace
    {
        struct PackedPosNormal
        {
            glm::vec3 pos;
            u32 normal;
        };

        struct PackedVertices
        {
            std::vector<PackedPosNormal> pos_normals;
            std::vector<glm::vec2> uvs;
            std::vector<u32> tangents;
            std::vector<u32> colors;

            void resize(size_t vert_size)
            {
                pos_normals.resize(vert_size);
                uvs.resize(vert_size);
                colors.resize(vert_size);
                tangents.resize(vert_size);
            }
        };

        struct MeshGpuRuntime
        {
            std::shared_ptr<rhi::GpuBuffer> vertex_buffer;
            std::shared_ptr<rhi::GpuBuffer> index_buffer;
            u32 vertex_pos_nor_offset = 0;
            u32 vertex_uv_offset = 0;
            u32 vertex_tangent_offset = 0;
            u32 vertex_color_offset = 0;
            bool uploaded = false;
        };

        void upload_mesh_runtime(MeshAsset& mesh, MeshGpuRuntime& rt, rhi::GpuDevice* device)
        {
            if (!device || mesh.vertices.empty() || mesh.indices.empty())
                return;

            PackedVertices packed;
            packed.resize(mesh.vertices.size());
            parallel_for<size_t>(0, mesh.vertices.size(), [&](size_t v_id) {
                const auto& v = mesh.vertices[v_id];
                packed.pos_normals[v_id] = PackedPosNormal{ v.Position, pack_unit_direction_11_10_11(v.Normal) };
                packed.uvs[v_id] = v.TexCoords;
                packed.tangents[v_id] = pack_unit_direction_11_10_11(v.Tangent);
                packed.colors[v_id] = pack_color_8888(v.Colours);
            });

            rt.vertex_pos_nor_offset = 0;
            rt.vertex_uv_offset = static_cast<u32>(sizeof(PackedPosNormal) * mesh.vertices.size());
            rt.vertex_tangent_offset = rt.vertex_uv_offset + static_cast<u32>(sizeof(glm::vec2) * mesh.vertices.size());
            rt.vertex_color_offset = rt.vertex_tangent_offset + static_cast<u32>(sizeof(u32) * mesh.vertices.size());
            const auto total_size = rt.vertex_color_offset + static_cast<u32>(sizeof(u32) * mesh.vertices.size());

            auto index_desc = rhi::GpuBufferDesc::new_gpu_only(
                mesh.indices.size() * sizeof(u32),
                rhi::BufferUsageFlags::STORAGE_BUFFER
                    | rhi::BufferUsageFlags::SHADER_DEVICE_ADDRESS
                    | rhi::BufferUsageFlags::INDEX_BUFFER
                    | rhi::BufferUsageFlags::TRANSFER_DST);
            auto vertex_desc = rhi::GpuBufferDesc::new_gpu_only(
                total_size,
                rhi::BufferUsageFlags::STORAGE_BUFFER
                    | rhi::BufferUsageFlags::SHADER_DEVICE_ADDRESS
                    | rhi::BufferUsageFlags::VERTEX_BUFFER
                    | rhi::BufferUsageFlags::TRANSFER_DST);

            if (device->gpu_limits.ray_tracing_enabled)
            {
                index_desc.usage |= rhi::BufferUsageFlags::ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY;
                vertex_desc.usage |= rhi::BufferUsageFlags::ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY;
            }

            std::vector<u8> packed_data(total_size);
            std::memcpy(packed_data.data(), packed.pos_normals.data(), rt.vertex_uv_offset);
            std::memcpy(packed_data.data() + rt.vertex_uv_offset, packed.uvs.data(), sizeof(glm::vec2) * mesh.vertices.size());
            std::memcpy(packed_data.data() + rt.vertex_tangent_offset, packed.tangents.data(), sizeof(u32) * mesh.vertices.size());
            std::memcpy(packed_data.data() + rt.vertex_color_offset, packed.colors.data(), sizeof(u32) * mesh.vertices.size());

            rt.index_buffer = device->create_buffer(index_desc, "mesh_index_buf", reinterpret_cast<u8*>(mesh.indices.data()));
            rt.vertex_buffer = device->create_buffer(vertex_desc, "mesh_vert_buf", packed_data.data());

            mesh.vertex_pos_nor_offset = rt.vertex_pos_nor_offset;
            mesh.vertex_uv_offset = rt.vertex_uv_offset;
            mesh.vertex_tangent_offset = rt.vertex_tangent_offset;
            mesh.vertex_color_offset = rt.vertex_color_offset;
            rt.uploaded = true;
        }
    }

    bool upload_mesh_asset(MeshAsset& mesh, rhi::GpuDevice* device, MeshUploadResult& out)
    {
        if (!device || mesh.vertices.empty() || mesh.indices.empty())
            return false;

        MeshGpuRuntime rt;
        upload_mesh_runtime(mesh, rt, device);
        if (!rt.vertex_buffer || !rt.index_buffer)
            return false;

        out.vertex_buffer = rt.vertex_buffer;
        out.index_buffer = rt.index_buffer;
        out.vertex_pos_nor_offset = rt.vertex_pos_nor_offset;
        out.vertex_uv_offset = rt.vertex_uv_offset;
        out.vertex_tangent_offset = rt.vertex_tangent_offset;
        out.vertex_color_offset = rt.vertex_color_offset;
        out.vertex_buffer_size = rt.vertex_color_offset + static_cast<size_t>(sizeof(u32) * mesh.vertices.size());
        out.index_buffer_size = mesh.indices.size() * sizeof(uint32_t);
        return true;
    }

} // namespace diverse
