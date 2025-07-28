#pragma once
#include "backend/drs_rhi/gpu_device.h"
#include "renderer/drs_rg/graph.h"

namespace diverse
{
    struct ImageLut
    {
        virtual auto create(rhi::GpuDevice* device) -> std::shared_ptr<rhi::GpuTexture> = 0;
        virtual auto compute(rg::RenderGraph& graph, rg::Handle<rhi::GpuTexture>& img) -> void = 0;

        auto compute_if_needed(rg::RenderGraph& rg) -> void
        {
            if (computed)
                return;
            auto rg_img = rg.import_res<rhi::GpuTexture>(image, rhi::AccessType::Nothing);
            this->compute(rg, rg_img);

            rg.export_res<rhi::GpuTexture>(rg_img, rhi::AccessType::AnyShaderReadSampledImageOrUniformTexelBuffer);

            computed = true;
        }

        auto get_image()->std::shared_ptr<rhi::GpuTexture> const
        {
            return image;
        }
    protected:
        std::shared_ptr<rhi::GpuTexture>    image;
        bool computed = false;
    };

    struct BrdfFgLutComputer : public ImageLut
    {
    public:
        BrdfFgLutComputer(rhi::GpuDevice* device);
    public:
        auto create(rhi::GpuDevice* device)->std::shared_ptr<rhi::GpuTexture> override;
        auto compute(rg::RenderGraph& graph, rg::Handle<rhi::GpuTexture>& img)->void override;
    };

    struct BezoldBruckeLutComputer : public ImageLut
    {
    public:
        BezoldBruckeLutComputer(rhi::GpuDevice* device);
    public:
        auto create(rhi::GpuDevice* device)->std::shared_ptr<rhi::GpuTexture> override;
        auto compute(rg::RenderGraph& graph, rg::Handle<rhi::GpuTexture>& img)->void override;
    };

    struct BlueNoiseLutComputer : public ImageLut
    {
    public:
        BlueNoiseLutComputer(rhi::GpuDevice* device);
    public:
        auto create(rhi::GpuDevice* device) -> std::shared_ptr<rhi::GpuTexture> override;
        auto compute(rg::RenderGraph& graph, rg::Handle<rhi::GpuTexture>& img) -> void override;
    };
}