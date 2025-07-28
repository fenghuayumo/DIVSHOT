#pragma once
#include "graph.h"
#include "pass_builder.h"
namespace diverse
{
    namespace rg
    {
        inline auto clear_depth(RenderGraph& rg, Handle<rhi::GpuTexture>& img,f32 depth = 1.0, u32 stencil = 0)
        {
            auto pass = rg.add_pass("clear depth");
            auto output_ref = pass.write(img, rhi::AccessType::TransferWrite);

            pass.render([output_ref = std::move(output_ref),depth, stencil](RenderPassApi& api) {
                auto device = api.device();
                auto cb = api.cb;

                auto image = api.resources.image(output_ref);
                
                device->clear_depth_stencil(
                    cb,
                    image,
                    depth,
                    stencil
                );
            });
            pass.rg->record_pass(std::move(pass.pass));
        }

        inline auto clear_color(RenderGraph& rg, Handle<rhi::GpuTexture>& img,const std::array<f32,4>& clear_color)
        {
            auto pass = rg.add_pass("clear color");
            auto output_ref = pass.write(img, rhi::AccessType::TransferWrite);

            pass.render([output_ref = std::move(output_ref), clear_color](RenderPassApi& api) {
                auto device = api.device();
                auto cb = api.cb;

                auto image = api.resources.image(output_ref);

                device->clear_color(
                    cb,
                    image,
                    clear_color
                );
            });
            pass.rg->record_pass(std::move(pass.pass));
        }

        inline auto copy_img(RenderGraph& rg, Handle<rhi::GpuTexture>& src,Handle<rhi::GpuTexture>& dst)
        {
            auto pass = rg.add_pass("copy image");
            auto output_ref = pass.write(dst, rhi::AccessType::TransferWrite);
            pass.render([output_ref = std::move(output_ref),
                    input_ref = pass.read(src,rhi::AccessType::TransferRead)](RenderPassApi& api) {
                auto device = api.device();
                auto cb = api.cb;

                auto image = api.resources.image(output_ref);
                auto src_img = api.resources.image(input_ref);
                device->copy_image(
                    src_img,
                    image,
                    cb
                );
            });
            pass.rg->record_pass(std::move(pass.pass));
        }
    }
}
