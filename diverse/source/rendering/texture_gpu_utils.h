#pragma once

#include "assets/cpu_assets.h"
#include "backend/drs_rhi/gpu_device.h"
#include "backend/drs_rhi/gpu_texture.h"
#include <memory>

namespace diverse
{
    std::shared_ptr<rhi::GpuTexture> upload_texture_asset(TextureAsset& texture, rhi::GpuDevice* device);

} // namespace diverse
