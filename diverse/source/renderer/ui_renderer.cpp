#include "ui_renderer.h"
#include "drs_rg/simple_pass.h"
#include "drs_rg/image_op.h"
namespace diverse
{
	auto UiRenderer::prepare_render_graph(rg::TemporalGraph& rg) -> rg::Handle<rhi::GpuTexture>
	{
		return render_ui(rg);
	}
	auto UiRenderer::render_ui(rg::RenderGraph& rg) -> rg::Handle<rhi::GpuTexture>
	{
		if (ui_frame)
		{
            auto& image = ui_frame->second;
			auto ui_tex = rg.import_res(image, rhi::AccessType::Nothing);
			auto pass = rg.add_pass("ui");

			pass.raster(ui_tex, rhi::AccessType::ColorAttachmentWrite);
			pass.render([&](rg::RenderPassApi& api){
			
                ui_frame->first(api.cb);
			});
			pass.rg->record_pass(std::move(pass.pass));
			return ui_tex;
		}
		else
		{
			auto blank_img = rg.create<rhi::GpuTexture>(rhi::GpuTextureDesc::new_2d(PixelFormat::R8G8B8A8_UNorm, {1, 1}));
			rg::clear_color(rg, blank_img, {0.0f});

			return blank_img;
		}
		return rg::Handle<rhi::GpuTexture>();
	}
}
