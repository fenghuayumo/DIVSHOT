#pragma once

#include "assets/cpu_assets.h"
#include "backend/drs_rhi/gpu_buffer.h"
#include "backend/drs_rhi/gpu_device.h"
#include <memory>

namespace diverse
{
    struct MeshUploadResult
    {
        std::shared_ptr<rhi::GpuBuffer> vertex_buffer;
        std::shared_ptr<rhi::GpuBuffer> index_buffer;
        u32 vertex_pos_nor_offset = 0;
        u32 vertex_uv_offset = 0;
        u32 vertex_tangent_offset = 0;
        u32 vertex_color_offset = 0;
        size_t vertex_buffer_size = 0;
        size_t index_buffer_size = 0;
    };

    bool upload_mesh_asset(MeshAsset& mesh, rhi::GpuDevice* device, MeshUploadResult& out);

} // namespace diverse
