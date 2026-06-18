#pragma once
#include "graph.h"
#include "temporal.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
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


		struct FrameSyncPoint
		{
			auto reset() -> void;
			auto signal() -> void;
			auto wait() -> void;
			auto is_signaled() const -> bool;

			mutable std::mutex mutex;
			std::condition_variable cv;
			bool signaled = true;
		};

		struct RecordedRhiFrame
		{
			rhi::DeviceFrame* device_frame = nullptr;
			std::shared_ptr<rhi::CommandBuffer> main_cmd_buf;
			std::shared_ptr<rhi::CommandBuffer> presentation_cmd_buf;
			rhi::SwapchainImage swapchain_image = {};
			std::shared_ptr<FrameSyncPoint> accepted_sync;
			std::shared_ptr<FrameSyncPoint> submitted_sync;
			std::chrono::steady_clock::time_point record_begin_time = {};
			std::chrono::steady_clock::time_point record_end_time = {};
			std::chrono::steady_clock::time_point enqueue_time = {};
			uint64 debug_frame_index = 0;
			uint32 frame_slot = 0;
		};

		struct PendingFrameRetire
		{
			std::shared_ptr<FrameSyncPoint> submitted_sync;
			RenderGraphTransientResources transient_resources;
			uint32 frame_slot = 0;
		};

		struct RhiThreadProfileSnapshot
		{
			uint64 enqueued_frames = 0;
			uint64 submitted_frames = 0;
			uint64 completed_frames = 0;
			uint32 pending_frames = 0;
			uint32 max_pending_frames = 0;
			double last_record_ms = 0.0;
			double last_queue_wait_ms = 0.0;
			double last_submit_ms = 0.0;
			double last_frame_latency_ms = 0.0;
			double average_record_ms = 0.0;
			double average_queue_wait_ms = 0.0;
			double average_submit_ms = 0.0;
			double average_frame_latency_ms = 0.0;
		};

		struct RhiThreadDebugConfig
		{
			uint32 render_graph_dump_every_n_frames = 0;
			uint32 rhi_profile_log_every_n_frames = 0;
			bool render_graph_dump_dot = false;
			std::string render_graph_dump_directory = "render_debug";
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
			auto get_rhi_thread_profile() const -> RhiThreadProfileSnapshot;

			auto prepare_frame(TemporalGraph& rg, std::function<void(TemporalGraph&)> prepare_frame_graph) -> void;

			auto prepare_frame_constants(TemporalGraph& rg,std::function<FrameConstantsLayout(rhi::DynamicConstants&)> prepare_frame_constants) -> void;
			auto register_render_graph(RenderGraph& rg) -> void;
			auto temporal_graph()->TemporalGraph;
			//auto clear_transient_resources() -> void;
			auto clear_resources()->void;
			auto refresh_shaders()->void;
		private:
			auto begin_record_frame() -> void;
			auto rhi_thread_main() -> void;
			auto submit_recorded_frame_immediate(const RecordedRhiFrame& frame) -> void;
			auto retire_frame_slot(uint32 frame_slot) -> void;
			auto queue_frame_retire(const RecordedRhiFrame& frame, RenderGraphTransientResources&& transient_resources) -> void;
			auto discard_pending_frame_retires() -> void;
			auto load_debug_config_from_environment() -> void;
			auto dump_render_graph_debug(TemporalGraph& rg) -> void;
			auto note_rhi_frame_enqueued(uint32 pending_frames) -> void;
			auto update_rhi_queue_depth(uint32 pending_frames) -> void;
			auto note_rhi_frame_completed(
				const RecordedRhiFrame& frame,
				std::chrono::steady_clock::time_point submit_begin_time,
				std::chrono::steady_clock::time_point submit_end_time) -> RhiThreadProfileSnapshot;
			auto log_rhi_profile_if_needed(const RhiThreadProfileSnapshot& profile) const -> void;

			std::thread rhi_thread;
			std::mutex rhi_thread_mutex;
			std::condition_variable rhi_thread_cv;
			std::deque<RecordedRhiFrame> pending_rhi_frames;
			std::atomic_bool rhi_thread_running = false;
			bool rhi_thread_stop_requested = false;
			bool rhi_thread_busy = false;
			std::vector<std::shared_ptr<FrameSyncPoint>> frame_slot_submit_syncs;
			std::deque<PendingFrameRetire> pending_frame_retires;
			std::shared_ptr<FrameSyncPoint> last_frame_accepted_sync;
			std::shared_ptr<FrameSyncPoint> current_frame_accepted_sync;
			std::shared_ptr<FrameSyncPoint> current_frame_submitted_sync;
			uint32 current_frame_slot = 0;
			uint32 current_recording_frame_slot = 0;
			RhiThreadDebugConfig debug_config;
			mutable std::mutex rhi_profile_mutex;
			RhiThreadProfileSnapshot rhi_profile;
			double total_record_ms = 0.0;
			double total_queue_wait_ms = 0.0;
			double total_submit_ms = 0.0;
			double total_frame_latency_ms = 0.0;
			uint64 render_graph_debug_frame_index = 0;
			uint64 rhi_debug_frame_index = 0;
		};
	}
}
