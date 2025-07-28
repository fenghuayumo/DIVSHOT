#pragma once
#include "graph.h"
#include "temporal.h"

namespace diverse
{
	namespace rg
	{

		//struct TemporalRg
		//{
		//	enum class Type
		//	{
		//		Inert,
		//		Exported
		//	}ty;

		//	std::any value;

		//	static auto TemporalRenderGraphState(const TemporalRenderGraphState& v) -> TemporalRg
		//	{
		//		return { TemporalRg ::Type::Inert, v};
		//	}
		//};


		struct Renderer
		{
			Renderer(rhi::GpuDevice* dev,rhi::Swapchain* swapchain);
			~Renderer();
			rhi::GpuDevice* device;
			rhi::Swapchain* swap_chain;
			rhi::PipelineCache	pipeline_cache;
			TransientResourceCache transient_resource_cache;
			rhi::DynamicConstants dynamic_constants;
			std::shared_ptr<rhi::DescriptorSet>	frame_descriptor_set;
			FrameConstantsLayout	frame_constants_layout;

			TemporalRenderGraphState	temporal_rg_state;
			//TemporalGraph				temporal_rg;

			auto draw_frame(
					TemporalGraph& rg,
					rhi::Swapchain* swapchain) -> void;

			auto prepare_frame(TemporalGraph& rg, std::function<void(TemporalGraph&)> prepare_frame_graph) -> void;

			auto prepare_frame_constants(TemporalGraph& rg,std::function<FrameConstantsLayout(rhi::DynamicConstants&)> prepare_frame_constants) -> void;
			auto register_render_graph(RenderGraph& rg) -> void;
			auto temporal_graph()->TemporalGraph;
			//auto clear_transient_resources() -> void;
			auto clear_resources()->void;
			auto refresh_shaders()->void;
		};
	}
}
