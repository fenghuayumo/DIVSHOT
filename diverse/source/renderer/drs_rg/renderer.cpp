#include "renderer.h"
#include "engine/thread_affinity.h"
namespace diverse
{
    namespace rg
    {
        
        static auto FRAME_CONSTANTS_LAYOUT = std::unordered_map<uint32, rhi::DescriptorInfo>{
            {0, rhi::DescriptorInfo{rhi::DescriptorType::UNIFORM_BUFFER_DYNAMIC, rhi::DescriptorDimensionality::Single}},
            {1, rhi::DescriptorInfo{rhi::DescriptorType::STORAGE_BUFFER_DYNAMIC, rhi::DescriptorDimensionality::Single}},
            {2, rhi::DescriptorInfo{rhi::DescriptorType::STORAGE_BUFFER_DYNAMIC, rhi::DescriptorDimensionality::Single}},
            {3, rhi::DescriptorInfo{rhi::DescriptorType::STORAGE_BUFFER_DYNAMIC, rhi::DescriptorDimensionality::Single}},
        };

        auto FrameSyncPoint::reset() -> void
        {
            std::lock_guard lock(mutex);
            signaled = false;
        }

        auto FrameSyncPoint::signal() -> void
        {
            {
                std::lock_guard lock(mutex);
                signaled = true;
            }
            cv.notify_all();
        }

        auto FrameSyncPoint::wait() -> void
        {
            std::unique_lock lock(mutex);
            cv.wait(lock, [this]() {
                return signaled;
            });
        }

        auto FrameSyncPoint::is_signaled() const -> bool
        {
            std::lock_guard lock(mutex);
            return signaled;
        }

        Renderer::Renderer(rhi::GpuDevice* dev,rhi::Swapchain* swapchain)
        : device(dev), swap_chain(swapchain), rhi_submitter(dev, swapchain)
        {
            auto buffer_flag =  rhi::BufferUsageFlags::UNIFORM_BUFFER |
                rhi::BufferUsageFlags::STORAGE_BUFFER;
            if( device->gpu_limits.ray_tracing_enabled )
            {
                buffer_flag |=  rhi::BufferUsageFlags::SHADER_DEVICE_ADDRESS |
                rhi::BufferUsageFlags::ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY;
            }
            auto desc = rhi::GpuBufferDesc::new_cpu_to_gpu(DYNAMIC_CONSTANTS_SIZE_BYTES * DYNAMIC_CONSTANTS_BUFFER_COUNT,
                                                           buffer_flag);
            dynamic_constants.buffer = device->create_buffer(desc,
                            "dynamic constants buffer",
                             nullptr);
            dynamic_constants.device = device;


            frame_descriptor_set = device->create_descriptor_set(dynamic_constants.buffer.get(), FRAME_CONSTANTS_LAYOUT,"frame_render_set");
            last_frame_accepted_sync = std::make_shared<FrameSyncPoint>();
            frame_slot_submit_syncs.reserve(DYNAMIC_CONSTANTS_BUFFER_COUNT);
            for (auto frame_idx = 0u; frame_idx < DYNAMIC_CONSTANTS_BUFFER_COUNT; frame_idx++)
                frame_slot_submit_syncs.push_back(std::make_shared<FrameSyncPoint>());
            start_rhi_thread();
        }

        Renderer::~Renderer()
        {
            stop_rhi_thread();
        }

        auto RhiSubmitter::submit_and_present(const RecordedRhiFrame& frame) -> void
        {
            threading::assert_rhi_thread();
            if (!device || !swap_chain || !frame.main_cmd_buf || !frame.presentation_cmd_buf)
                return;

            device->submit_cmd(frame.main_cmd_buf.get());
            swap_chain->present_image(frame.swapchain_image, frame.presentation_cmd_buf.get());
        }

        auto RhiSubmitter::retire_frame(rhi::DeviceFrame* frame) -> void
        {
            threading::assert_rhi_thread();
            if (device && frame)
                device->end_frame(frame);
        }

        auto Renderer::begin_record_frame() -> void
        {
            if (last_frame_accepted_sync)
                last_frame_accepted_sync->wait();

            if (!frame_slot_submit_syncs.empty())
                frame_slot_submit_syncs[current_frame_slot]->wait();

            current_frame = device->begin_frame();
            current_frame_accepted_sync = std::make_shared<FrameSyncPoint>();
            current_frame_submitted_sync = std::make_shared<FrameSyncPoint>();
            current_frame_accepted_sync->reset();
            current_frame_submitted_sync->reset();

            if (!frame_slot_submit_syncs.empty())
                frame_slot_submit_syncs[current_frame_slot] = current_frame_submitted_sync;

            last_frame_accepted_sync = current_frame_accepted_sync;
            current_frame_slot = (current_frame_slot + 1) % DYNAMIC_CONSTANTS_BUFFER_COUNT;
        }

        auto Renderer::draw_frame(TemporalGraph& rg,
                        rhi::Swapchain* swapchain) -> void
        {
            rhi_submitter.swap_chain = swapchain;
            auto recorded_frame = record_frame(rg, swapchain);
            submit_recorded_frame(recorded_frame);

            temporal_rg_state.retire_temporal(rg);

            rg.release_resources(transient_resource_cache);

            dynamic_constants.advance_frame();
            current_frame = nullptr;
            frame_alloctor().free();
        }

        auto Renderer::record_frame(TemporalGraph& rg,
                        rhi::Swapchain* swapchain) -> RecordedRhiFrame
        {
            threading::assert_render_thread();
            if (!current_frame)
            {
                begin_record_frame();
            }
            for (auto cb : {current_frame->main_cmd_buf, current_frame->presentation_cmd_buf }) {
                cb->begin();
            }

            auto& main_cb = current_frame->main_cmd_buf;
            wait_for_rhi_idle();
            rg.begin_execute();

            rg.record_main_cb(main_cb.get());

            main_cb->end();
            auto swapchain_image = swapchain->acquire_next_image();

            auto& presentation_cb = current_frame->presentation_cmd_buf;
//            presentation_cb->wait();
            device->record_image_barrier(presentation_cb.get(), rhi::ImageBarrier{
                swapchain_image.image.get(),
                rhi::AccessType::Present,
                rhi::AccessType::ComputeShaderWrite,
                rhi::ImageAspectFlags::COLOR}
                .with_discard(true));
            rg.record_presentation_cb(presentation_cb.get(), swapchain_image.image);
            device->record_image_barrier(presentation_cb.get(), rhi::ImageBarrier{
                swapchain_image.image.get(),
                rhi::AccessType::ComputeShaderWrite,
                rhi::AccessType::Present,
                rhi::ImageAspectFlags::COLOR}
                );

            presentation_cb->end();

            return RecordedRhiFrame{
                current_frame,
                main_cb,
                presentation_cb,
                swapchain_image,
                current_frame_accepted_sync,
                current_frame_submitted_sync
            };
        }

        auto Renderer::submit_recorded_frame(const RecordedRhiFrame& frame) -> void
        {
            if (!frame.device_frame)
                return;

            if (!rhi_thread_running.load(std::memory_order_acquire))
            {
                submit_recorded_frame_immediate(frame);
                return;
            }

            {
                std::lock_guard lock(rhi_thread_mutex);
                pending_rhi_frames.push_back(frame);
            }
            rhi_thread_cv.notify_one();
        }

        auto Renderer::submit_recorded_frame_immediate(const RecordedRhiFrame& frame) -> void
        {
            rhi_submitter.retire_frame(frame.device_frame);
            if (frame.accepted_sync)
                frame.accepted_sync->signal();

            rhi_submitter.submit_and_present(frame);
            if (frame.submitted_sync)
                frame.submitted_sync->signal();
        }

        auto Renderer::start_rhi_thread() -> void
        {
            if (rhi_thread_running.load(std::memory_order_acquire))
                return;

            {
                std::lock_guard lock(rhi_thread_mutex);
                rhi_thread_stop_requested = false;
                rhi_thread_busy = false;
                pending_rhi_frames.clear();
            }

            rhi_thread_running.store(true, std::memory_order_release);
            rhi_thread = std::thread([this]() {
                rhi_thread_main();
            });
        }

        auto Renderer::stop_rhi_thread() -> void
        {
            if (!rhi_thread_running.load(std::memory_order_acquire) && !rhi_thread.joinable())
                return;

            if (threading::is_rhi_thread())
                return;

            {
                std::lock_guard lock(rhi_thread_mutex);
                rhi_thread_stop_requested = true;
            }
            rhi_thread_cv.notify_all();

            if (rhi_thread.joinable())
                rhi_thread.join();
        }

        auto Renderer::wait_for_rhi_idle() -> void
        {
            if (!rhi_thread_running.load(std::memory_order_acquire) || threading::is_rhi_thread())
                return;

            std::unique_lock lock(rhi_thread_mutex);
            rhi_thread_cv.wait(lock, [this]() {
                return pending_rhi_frames.empty() && !rhi_thread_busy;
            });
        }

        auto Renderer::rhi_thread_main() -> void
        {
            threading::mark_rhi_thread();

            for (;;)
            {
                RecordedRhiFrame frame;
                {
                    std::unique_lock lock(rhi_thread_mutex);
                    rhi_thread_cv.wait(lock, [this]() {
                        return rhi_thread_stop_requested || !pending_rhi_frames.empty();
                    });

                    if (rhi_thread_stop_requested && pending_rhi_frames.empty())
                        break;

                    frame = std::move(pending_rhi_frames.front());
                    pending_rhi_frames.pop_front();
                    rhi_thread_busy = true;
                }

                submit_recorded_frame_immediate(frame);

                {
                    std::lock_guard lock(rhi_thread_mutex);
                    rhi_thread_busy = false;
                }
                rhi_thread_cv.notify_all();
            }

            {
                std::lock_guard lock(rhi_thread_mutex);
                rhi_thread_busy = false;
                rhi_thread_running.store(false, std::memory_order_release);
            }
            rhi_thread_cv.notify_all();
        }

        auto Renderer::prepare_frame_constants(TemporalGraph& rg,std::function<FrameConstantsLayout(rhi::DynamicConstants&)> prepare_frame_constants) -> void
        {
            if (!current_frame)
            {
                begin_record_frame();
            }
            frame_constants_layout = prepare_frame_constants(dynamic_constants);
            register_render_graph(rg);
        }

        auto Renderer::register_render_graph(RenderGraph& rg)->void
        {
            rg.predefined_descriptor_set_layouts.insert({2, PredefinedDescriptorSet{FRAME_CONSTANTS_LAYOUT}});
            rg.register_execution_params(RenderGraphExecutionParams{
                    device,
                    &pipeline_cache,
                    frame_descriptor_set.get(),
                    frame_constants_layout
                },
                &transient_resource_cache, 
                &dynamic_constants
            );
        }

		auto Renderer::prepare_frame(TemporalGraph& rg, std::function<void(TemporalGraph&)> prepare_render_graph) -> void
        {
            prepare_render_graph(rg);

            auto temp_rg_state = rg.export_temporal();
            rg.compile(pipeline_cache);

            if (pipeline_cache.prepare_frame(device))
            {
                // If the frame preparation succeded, update stored temporal rg state and finish
                temporal_rg_state = temp_rg_state;
                temporal_rg_state.status = TemporalRenderGraphState::Exported;
            }
            else
            {
                // If frame preparation failed, we're not going to render anything, but we've potentially created
                // some temporal resources, and we can reuse them in the next attempt.
                //
                // Import any new resources into our temporal rg state, but reset their access modes.
                auto& self_temporal_rg_state = temporal_rg_state ;
                for (auto [res_key, res] : temporal_rg_state.resources)
                {
                    TemporalResourceState   new_res;
                    if (self_temporal_rg_state.resources.find(res_key) == self_temporal_rg_state.resources.end()) 
                    {
                        switch ( res.ty )
                        {
                        case TemporalResourceState::Type::Inert:
                        {
                            new_res = res;
                        }
                        break;
                        case TemporalResourceState::Type::Imported:
                        {
                            new_res = TemporalResourceState::Inert({res.Imported().first, rhi::AccessType::Nothing});
                        }break;
                        case TemporalResourceState::Type::Exported:
                        {
                            new_res = TemporalResourceState::Inert({res.Exported().first,rhi::AccessType::Nothing});
                        }
                        break;
                        default:
                            break;
                        }
                    }
                    self_temporal_rg_state.resources.insert({res_key, res});
                }
            }
        }
        auto Renderer::temporal_graph() -> TemporalGraph
        {
            return {
                temporal_rg_state,
                device
            };
        }

        auto Renderer::clear_resources() -> void
        {
            wait_for_rhi_idle();
            temporal_rg_state.resources.clear();
            transient_resource_cache.clear_resource();
        }

        auto Renderer::refresh_shaders()->void
        {
            pipeline_cache.refresh_shaders();
        }
    }
}
