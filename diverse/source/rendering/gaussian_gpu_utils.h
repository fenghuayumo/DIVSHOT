#pragma once

#include "assets/gaussian_asset.h"
#include "backend/drs_rhi/gpu_device.h"
#include <memory>

namespace diverse
{
    struct GaussianGpu;

    struct GaussianPackedCpuData
    {
        std::vector<Gaussian> gaussians;
        std::vector<PackedVertexColor> sh_0;
        std::vector<PackedVertexSH> sh_n;
    };

    struct GaussianBufferUpload
    {
        std::shared_ptr<rhi::GpuBuffer> gaussians_buf;
        std::shared_ptr<rhi::GpuBuffer> sh_0_buf;
        std::shared_ptr<rhi::GpuBuffer> sh_n_buf;
        std::shared_ptr<rhi::GpuBuffer> state_buf;
        std::shared_ptr<rhi::GpuBuffer> points_key_buf;
        std::shared_ptr<rhi::GpuBuffer> points_value_buf;
        size_t allocated_splat_count = 0;
        size_t gpu_memory_size = 0;
    };

    GaussianPackedCpuData pack_gaussian_asset(const GaussianAsset& asset);

    // Upload or refresh GPU buffers from packed CPU data.
    // When existing is non-null and large enough, buffers are updated in place.
    GaussianBufferUpload upload_gaussian_buffers(
        rhi::GpuDevice* device,
        const GaussianPackedCpuData& packed,
        const GaussianBufferUpload* existing,
        size_t active_splat_count,
        int max_splats,
        bool compact,
        const std::vector<uint8_t>& splat_state,
        const std::vector<uint16_t>& splat_transform_index);

    void upload_gaussian_state_buffer(
        rhi::GpuDevice* device,
        rhi::GpuBuffer* state_buf,
        size_t splat_count,
        const std::vector<uint8_t>& splat_state,
        const std::vector<uint16_t>& splat_transform_index,
        bool initialize_zero = false);

    GaussianGpu make_gaussian_gpu(
        const GaussianBufferUpload& upload,
        uint32_t resident_version,
        uint32_t bindless_slot);

} // namespace diverse
