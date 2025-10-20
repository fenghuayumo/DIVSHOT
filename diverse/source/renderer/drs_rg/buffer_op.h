#pragma once
#include "graph.h"
#include "pass_builder.h"
namespace diverse
{
    namespace rg
    {
        inline auto copy_buffer(RenderGraph& rg,const Handle<rhi::GpuBuffer>& src, Handle<rhi::GpuBuffer>& dst)
        {
            auto pass = rg.add_pass("copy_buffer");
            auto output_ref = pass.write(dst, rhi::AccessType::TransferWrite);
            auto input_ref = pass.read(src, rhi::AccessType::TransferRead);

            pass.render([output_ref = std::move(output_ref),input_ref = std::move(input_ref)](RenderPassApi& api) {
                auto device = api.device();
                auto cb = api.cb;

                auto dst = api.resources.buffer(output_ref);
                auto src = api.resources.buffer(input_ref);
                device->copy_buffer(
                    cb,
                    src,
                    0,
                    dst,
                    0,
                    output_ref.desc.size
                );
            });
            pass.rg->record_pass(std::move(pass.pass));
        }

        inline auto clear_buffer(RenderGraph& rg, Handle<rhi::GpuBuffer>& buf,const uint value)
        {
            auto pass = rg.add_pass("clear buffer");
            auto output_ref = pass.write(buf, rhi::AccessType::TransferWrite);

            pass.render([output_ref = std::move(output_ref), value](RenderPassApi& api) {
                auto device = api.device();
                auto cb = api.cb;

                auto buffer = api.resources.buffer(output_ref);

                device->fill_buffer(
                    cb,
                    buffer,
                    value
                );
            });
            pass.rg->record_pass(std::move(pass.pass));
        }
    }
}