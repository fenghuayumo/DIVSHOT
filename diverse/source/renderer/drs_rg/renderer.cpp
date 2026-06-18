#include "renderer.h"
#include "core/ds_log.h"
#include "engine/thread_affinity.h"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
namespace diverse
{
    namespace rg
    {
        namespace
        {
            auto read_u32_environment(const char* name, uint32 default_value) -> uint32
            {
                const auto value = std::getenv(name);
                if (!value || !*value)
                    return default_value;

                char* end = nullptr;
                const auto parsed_value = std::strtoul(value, &end, 10);
                if (end == value)
                    return default_value;

                return static_cast<uint32>(parsed_value);
            }

            auto read_bool_environment(const char* name, bool default_value) -> bool
            {
                const auto value = std::getenv(name);
                if (!value || !*value)
                    return default_value;

                const std::string text(value);
                return text == "1" || text == "true" || text == "TRUE" || text == "on" || text == "ON";
            }

            auto read_string_environment(const char* name, const std::string& default_value) -> std::string
            {
                const auto value = std::getenv(name);
                if (!value || !*value)
                    return default_value;

                return value;
            }

            auto elapsed_ms(
                std::chrono::steady_clock::time_point begin_time,
                std::chrono::steady_clock::time_point end_time) -> double
            {
                if (begin_time == std::chrono::steady_clock::time_point{} ||
                    end_time == std::chrono::steady_clock::time_point{} ||
                    end_time < begin_time)
                {
                    return 0.0;
                }

                return std::chrono::duration<double, std::milli>(end_time - begin_time).count();
            }
        }

        
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
            load_debug_config_from_environment();

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
            const auto record_begin_time = std::chrono::steady_clock::now();
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

            RecordedRhiFrame frame;
            frame.device_frame = current_frame;
            frame.main_cmd_buf = main_cb;
            frame.presentation_cmd_buf = presentation_cb;
            frame.swapchain_image = swapchain_image;
            frame.accepted_sync = current_frame_accepted_sync;
            frame.submitted_sync = current_frame_submitted_sync;
            frame.record_begin_time = record_begin_time;
            frame.record_end_time = std::chrono::steady_clock::now();
            return frame;
        }

        auto Renderer::submit_recorded_frame(const RecordedRhiFrame& frame) -> void
        {
            if (!frame.device_frame)
                return;

            auto queued_frame = frame;
            queued_frame.enqueue_time = std::chrono::steady_clock::now();
            queued_frame.debug_frame_index = rhi_debug_frame_index++;

            if (!rhi_thread_running.load(std::memory_order_acquire))
            {
                note_rhi_frame_enqueued(0);
                const auto submit_begin_time = std::chrono::steady_clock::now();
                submit_recorded_frame_immediate(queued_frame);
                const auto submit_end_time = std::chrono::steady_clock::now();
                const auto profile = note_rhi_frame_completed(queued_frame, submit_begin_time, submit_end_time);
                log_rhi_profile_if_needed(profile);
                return;
            }

            uint32 pending_frames = 0;
            {
                std::lock_guard lock(rhi_thread_mutex);
                pending_rhi_frames.push_back(queued_frame);
                pending_frames = static_cast<uint32>(pending_rhi_frames.size());
            }
            note_rhi_frame_enqueued(pending_frames);
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

        auto Renderer::get_rhi_thread_profile() const -> RhiThreadProfileSnapshot
        {
            std::lock_guard lock(rhi_profile_mutex);
            return rhi_profile;
        }

        auto Renderer::load_debug_config_from_environment() -> void
        {
            debug_config.render_graph_dump_every_n_frames = read_u32_environment("DS_RENDERGRAPH_DUMP_EVERY", 0);
            debug_config.render_graph_dump_dot = read_bool_environment("DS_RENDERGRAPH_DUMP_DOT", false);
            debug_config.render_graph_dump_directory = read_string_environment("DS_RENDER_DEBUG_DIR", debug_config.render_graph_dump_directory);
            debug_config.rhi_profile_log_every_n_frames = read_u32_environment(
                "DS_RHITHREAD_PROFILE_EVERY",
                read_u32_environment("DS_RHI_PROFILE_LOG_EVERY", 0));

            if (debug_config.render_graph_dump_every_n_frames != 0)
            {
                DS_LOG_INFO(
                    "RenderGraph dump enabled: every {} frame(s), dot={}, dir={}",
                    debug_config.render_graph_dump_every_n_frames,
                    debug_config.render_graph_dump_dot ? "true" : "false",
                    debug_config.render_graph_dump_directory);
            }

            if (debug_config.rhi_profile_log_every_n_frames != 0)
            {
                DS_LOG_INFO(
                    "RHIThread profile logging enabled: every {} submitted frame(s)",
                    debug_config.rhi_profile_log_every_n_frames);
            }
        }

        auto Renderer::dump_render_graph_debug(TemporalGraph& rg) -> void
        {
            if (debug_config.render_graph_dump_every_n_frames == 0)
                return;

            const auto frame_index = render_graph_debug_frame_index++;
            if ((frame_index % debug_config.render_graph_dump_every_n_frames) != 0)
                return;

            try
            {
                const auto dump_dir = std::filesystem::path(debug_config.render_graph_dump_directory);
                std::filesystem::create_directories(dump_dir);

                const auto stem = fmt::format("rendergraph_frame_{:06}", frame_index);
                const auto text_path = dump_dir / (stem + ".txt");
                {
                    std::ofstream file(text_path, std::ios::out | std::ios::trunc);
                    file << rg.dump_schedule_text();
                }

                if (debug_config.render_graph_dump_dot)
                {
                    const auto dot_path = dump_dir / (stem + ".dot");
                    std::ofstream file(dot_path, std::ios::out | std::ios::trunc);
                    file << rg.dump_schedule_dot();
                }

                DS_LOG_INFO("RenderGraph dump written: {}", text_path.string());
            }
            catch (const std::exception& e)
            {
                DS_LOG_WARN("RenderGraph dump failed: {}", e.what());
            }
        }

        auto Renderer::note_rhi_frame_enqueued(uint32 pending_frames) -> void
        {
            std::lock_guard lock(rhi_profile_mutex);
            rhi_profile.enqueued_frames++;
            rhi_profile.pending_frames = pending_frames;
            rhi_profile.max_pending_frames = std::max(rhi_profile.max_pending_frames, pending_frames);
        }

        auto Renderer::update_rhi_queue_depth(uint32 pending_frames) -> void
        {
            std::lock_guard lock(rhi_profile_mutex);
            rhi_profile.pending_frames = pending_frames;
            rhi_profile.max_pending_frames = std::max(rhi_profile.max_pending_frames, pending_frames);
        }

        auto Renderer::note_rhi_frame_completed(
            const RecordedRhiFrame& frame,
            std::chrono::steady_clock::time_point submit_begin_time,
            std::chrono::steady_clock::time_point submit_end_time) -> RhiThreadProfileSnapshot
        {
            const auto record_ms = elapsed_ms(frame.record_begin_time, frame.record_end_time);
            const auto queue_wait_ms = elapsed_ms(frame.enqueue_time, submit_begin_time);
            const auto submit_ms = elapsed_ms(submit_begin_time, submit_end_time);
            const auto frame_latency_ms = elapsed_ms(frame.record_begin_time, submit_end_time);

            std::lock_guard lock(rhi_profile_mutex);
            rhi_profile.submitted_frames++;
            rhi_profile.completed_frames = rhi_profile.submitted_frames;
            rhi_profile.last_record_ms = record_ms;
            rhi_profile.last_queue_wait_ms = queue_wait_ms;
            rhi_profile.last_submit_ms = submit_ms;
            rhi_profile.last_frame_latency_ms = frame_latency_ms;

            total_record_ms += record_ms;
            total_queue_wait_ms += queue_wait_ms;
            total_submit_ms += submit_ms;
            total_frame_latency_ms += frame_latency_ms;

            const auto completed_frames = static_cast<double>(rhi_profile.completed_frames);
            if (completed_frames > 0.0)
            {
                rhi_profile.average_record_ms = total_record_ms / completed_frames;
                rhi_profile.average_queue_wait_ms = total_queue_wait_ms / completed_frames;
                rhi_profile.average_submit_ms = total_submit_ms / completed_frames;
                rhi_profile.average_frame_latency_ms = total_frame_latency_ms / completed_frames;
            }

            return rhi_profile;
        }

        auto Renderer::log_rhi_profile_if_needed(const RhiThreadProfileSnapshot& profile) const -> void
        {
            if (debug_config.rhi_profile_log_every_n_frames == 0 ||
                profile.completed_frames == 0 ||
                (profile.completed_frames % debug_config.rhi_profile_log_every_n_frames) != 0)
            {
                return;
            }

            DS_LOG_INFO(
                "RHIThread profile: frames={} enqueued={} pending={} max_pending={} "
                "last_ms(record/queue/submit/latency)={:.3f}/{:.3f}/{:.3f}/{:.3f} "
                "avg_ms(record/queue/submit/latency)={:.3f}/{:.3f}/{:.3f}/{:.3f}",
                profile.completed_frames,
                profile.enqueued_frames,
                profile.pending_frames,
                profile.max_pending_frames,
                profile.last_record_ms,
                profile.last_queue_wait_ms,
                profile.last_submit_ms,
                profile.last_frame_latency_ms,
                profile.average_record_ms,
                profile.average_queue_wait_ms,
                profile.average_submit_ms,
                profile.average_frame_latency_ms);
        }

        auto Renderer::rhi_thread_main() -> void
        {
            threading::mark_rhi_thread();

            for (;;)
            {
                RecordedRhiFrame frame;
                uint32 pending_frames_after_pop = 0;
                {
                    std::unique_lock lock(rhi_thread_mutex);
                    rhi_thread_cv.wait(lock, [this]() {
                        return rhi_thread_stop_requested || !pending_rhi_frames.empty();
                    });

                    if (rhi_thread_stop_requested && pending_rhi_frames.empty())
                        break;

                    frame = std::move(pending_rhi_frames.front());
                    pending_rhi_frames.pop_front();
                    pending_frames_after_pop = static_cast<uint32>(pending_rhi_frames.size());
                    rhi_thread_busy = true;
                }
                update_rhi_queue_depth(pending_frames_after_pop);

                const auto submit_begin_time = std::chrono::steady_clock::now();
                submit_recorded_frame_immediate(frame);
                const auto submit_end_time = std::chrono::steady_clock::now();
                const auto profile = note_rhi_frame_completed(frame, submit_begin_time, submit_end_time);
                log_rhi_profile_if_needed(profile);

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
            dump_render_graph_debug(rg);

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
