#include "lut_renderer.h"
#include "renderer/drs_rg/pass_api.h"
#include "renderer/drs_rg/simple_pass.h"
#include "assets/asset_manager.h"
#include "engine/os.h"
#include "assets/embeded/bluenoise_256_256.inl"

namespace diverse
{
    BrdfFgLutComputer::BrdfFgLutComputer(rhi::GpuDevice* device)
    {
        image = create(device);
    }
    auto BrdfFgLutComputer::create(rhi::GpuDevice* device) -> std::shared_ptr<rhi::GpuTexture>
    {
        return device->create_texture(rhi::GpuTextureDesc::new_2d(PixelFormat::R16G16B16A16_Float, {64,64})
                        .with_usage(rhi::TextureUsageFlags::STORAGE | rhi::TextureUsageFlags::SAMPLED),
                        {});
    }

    auto BrdfFgLutComputer::compute(rg::RenderGraph& rg, rg::Handle<rhi::GpuTexture>& img) -> void
    {
        auto pass = rg.add_pass("brdf_fg lut");
        auto pipeline = pass.register_compute_pipeline("/shaders/lut/brdf_fg.hlsl",{});
        auto img_ref = pass.write(img, rhi::AccessType::ComputeShaderWrite);

        pass.render([pipeline = std::move(pipeline), img_ref = std::move(img_ref)](rg::RenderPassApi& api) mutable {

            std::vector<rg::RenderPassBinding> bindings = { img_ref.bind() };
            auto res = rg::RenderPassPipelineBinding<rg::RgComputePipelineHandle>::from(pipeline)
                .descriptor_set(0, &bindings);
            auto bd_pipeline = api.bind_compute_pipeline(res);

            bd_pipeline.dispatch(img_ref.desc.extent);
        });
        pass.rg->record_pass(std::move(pass.pass));
    }

    BezoldBruckeLutComputer::BezoldBruckeLutComputer(rhi::GpuDevice* device)
    {
        image = create(device);
    }
    auto BezoldBruckeLutComputer::create(rhi::GpuDevice* device) -> std::shared_ptr<rhi::GpuTexture>
    {
        return device->create_texture(rhi::GpuTextureDesc::new_2d(PixelFormat::R16G16_Float, { 64,1 })
            .with_usage(rhi::TextureUsageFlags::STORAGE | rhi::TextureUsageFlags::SAMPLED),
            {});
    }
    auto BezoldBruckeLutComputer::compute(rg::RenderGraph& rg, rg::Handle<rhi::GpuTexture>& img) -> void
    {
        auto pass = rg.add_pass("bezold_brucke lut");

        auto pipeline = pass.register_compute_pipeline("/shaders/lut/bezold_brucke.hlsl",{});
        auto img_ref = pass.write(img, rhi::AccessType::ComputeShaderWrite);

        pass.render([pipeline = std::move(pipeline), img_ref = std::move(img_ref)](rg::RenderPassApi& api) mutable {

            std::vector<rg::RenderPassBinding> bindings = { img_ref.bind() };
            auto res = rg::RenderPassPipelineBinding<rg::RgComputePipelineHandle>::from(pipeline)
                .descriptor_set(0, &bindings);
            auto bd_pipeline = api.bind_compute_pipeline(res);

            bd_pipeline.dispatch(img_ref.desc.extent);
        });
        pass.rg->record_pass(std::move(pass.pass));
    }

    BlueNoiseLutComputer::BlueNoiseLutComputer(rhi::GpuDevice* device)
    {
        image = create(device);
    }
    auto BlueNoiseLutComputer::create(rhi::GpuDevice* device) -> std::shared_ptr<rhi::GpuTexture>
    {
        auto exe_path = std::filesystem::path(OS::instance()->getExecutablePath()).parent_path();
        return createSharedPtr<asset::Texture>(asset::RawImage{ PixelFormat::R8G8B8A8_UNorm, {bluenoise_256_256Width, bluenoise_256_256Height},std::vector<u8>(std::begin(bluenoise_256_256), std::end(bluenoise_256_256)) })->gpu_texture;
        //return std::make_shared<asset::Texture>(exe_path / "../resource/images/bluenoise/256_256/LDR_RGBA_0.png")->gpu_texture;
    }

    auto BlueNoiseLutComputer::compute(rg::RenderGraph& rg, rg::Handle<rhi::GpuTexture>& img) -> void
    {
       
    }
}
