#include "ui_renderer.h"
#include "drs_rg/simple_pass.h"
#include "drs_rg/image_op.h"
namespace diverse
{
	auto UiRenderer::prepare_render_graph(rg::TemporalGraph& rg) -> rg::Handle<rhi::GpuTexture>
	{
		return render_ui(rg);
	}

	auto UiRenderer::prepare_render_graph(rg::TemporalGraph& rg, const std::optional<UiFrame>& frame) -> rg::Handle<rhi::GpuTexture>
	{
		return render_ui(rg, frame);
	}

	auto UiRenderer::consume_frame() -> std::optional<UiFrame>
	{
		auto frame = std::move(ui_frame);
		ui_frame.reset();
		return frame;
	}

	auto UiRenderer::render_ui(rg::RenderGraph& rg) -> rg::Handle<rhi::GpuTexture>
	{
		return render_ui(rg, ui_frame);
	}

	auto UiRenderer::render_ui(rg::RenderGraph& rg, const std::optional<UiFrame>& frame) -> rg::Handle<rhi::GpuTexture>
	{
		if (frame)
		{
            auto& image = frame->target;
			auto ui_tex = rg.import_res(image, rhi::AccessType::Nothing);
			auto pass = rg.add_pass("ui");

			pass.raster(ui_tex, rhi::AccessType::ColorAttachmentWrite);
			pass.render([callback = frame->callback](rg::RenderPassApi& api){
			
                callback(api.cb);
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
