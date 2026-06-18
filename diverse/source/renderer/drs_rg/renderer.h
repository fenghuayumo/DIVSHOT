#pragma once
#include "graph.h"
#include "temporal.h"
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

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


		struct RecordedRhiFrame
		{
			rhi::DeviceFrame* device_frame = nullptr;
			std::shared_ptr<rhi::CommandBuffer> main_cmd_buf;
			std::shared_ptr<rhi::CommandBuffer> presentation_cmd_buf;
			rhi::SwapchainImage swapchain_image = {};
		};

		struct RhiSubmitter
		{
			RhiSubmitter() = default;
			RhiSubmitter(rhi::GpuDevice* dev, rhi::Swapchain* swapchain)
				: device(dev), swap_chain(swapchain)
			{}

			rhi::GpuDevice* device = nullptr;
			rhi::Swapchain* swap_chain = nullptr;

			auto submit_and_present(const RecordedRhiFrame& frame) -> void;
			auto retire_frame(rhi::DeviceFrame* frame) -> void;
		};

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
			rhi::DeviceFrame* current_frame = nullptr;
			RhiSubmitter rhi_submitter;

			TemporalRenderGraphState	temporal_rg_state;
			//TemporalGraph				temporal_rg;

			auto draw_frame(
					TemporalGraph& rg,
					rhi::Swapchain* swapchain) -> void;
			auto record_frame(
					TemporalGraph& rg,
					rhi::Swapchain* swapchain) -> RecordedRhiFrame;
			auto submit_recorded_frame(const RecordedRhiFrame& frame) -> void;
			auto start_rhi_thread() -> void;
			auto stop_rhi_thread() -> void;
			auto wait_for_rhi_idle() -> void;

			auto prepare_frame(TemporalGraph& rg, std::function<void(TemporalGraph&)> prepare_frame_graph) -> void;

			auto prepare_frame_constants(TemporalGraph& rg,std::function<FrameConstantsLayout(rhi::DynamicConstants&)> prepare_frame_constants) -> void;
			auto register_render_graph(RenderGraph& rg) -> void;
			auto temporal_graph()->TemporalGraph;
			//auto clear_transient_resources() -> void;
			auto clear_resources()->void;
			auto refresh_shaders()->void;
		private:
			auto rhi_thread_main() -> void;
			auto submit_recorded_frame_immediate(const RecordedRhiFrame& frame) -> void;

			std::thread rhi_thread;
			std::mutex rhi_thread_mutex;
			std::condition_variable rhi_thread_cv;
			std::deque<RecordedRhiFrame> pending_rhi_frames;
			std::atomic_bool rhi_thread_running = false;
			bool rhi_thread_stop_requested = false;
			bool rhi_thread_busy = false;
		};
	}
}
