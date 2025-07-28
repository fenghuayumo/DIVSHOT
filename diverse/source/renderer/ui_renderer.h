#pragma once
#include <optional>
#include <functional>
#include "drs_rg/temporal.h"

namespace diverse
{ 

	//namespace rhi
	//{
	//	struct GpuDevice;
	//	struct Swapchain;
	//	struct GpuTexture;
	//	struct CommandBuffer;
	//}
	//namespace rg
	//{
	//	struct TemporalGraph;
	//	struct RenderGraph;
	//	struct Handle<rhi::GpuTexture>;
	//}
	using UiRenderCallback = std::function<void(rhi::CommandBuffer*)>;
	struct UiRenderer
	{
		std::optional<std::pair<UiRenderCallback, std::shared_ptr<rhi::GpuTexture>>>	ui_frame;
	

		auto prepare_render_graph(rg::TemporalGraph& rg)->rg::Handle<rhi::GpuTexture>;

		auto render_ui(rg::RenderGraph& rg) -> rg::Handle<rhi::GpuTexture>;
	};

}