#include "renderer.h"
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

        Renderer::Renderer(rhi::GpuDevice* dev,rhi::Swapchain* swapchain)
        : device(dev), swap_chain(swapchain)
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
        }

        Renderer::~Renderer()
        {
        }

        auto Renderer::draw_frame(TemporalGraph& rg,
                        rhi::Swapchain* swapchain) -> void
        {
            auto current_frame = device->begin_frame();
            for (auto cb : {current_frame->main_cmd_buf, current_frame->presentation_cmd_buf }) {
                cb->begin();
            }
            // Record and submit the main command buffer

            auto& main_cb = current_frame->main_cmd_buf;
            rg.begin_execute();

            // Record and submit the main command buffer
            rg.record_main_cb(main_cb.get());

            main_cb->end();
            device->submit_cmd(main_cb.get());
    
            // Now that we've done the main submission and the GPU is busy, acquire the presentation image.
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
           // Transition the swapchain to present
           device->record_image_barrier(presentation_cb.get(), rhi::ImageBarrier{
                swapchain_image.image.get(),
                rhi::AccessType::ComputeShaderWrite,
                rhi::AccessType::Present,
                rhi::ImageAspectFlags::COLOR}
                );

           presentation_cb->end();
           // Record and submit the presentation command buffer
           swapchain->present_image(swapchain_image, presentation_cb.get());
           temporal_rg_state.retire_temporal(rg);

           rg.release_resources(transient_resource_cache);

           dynamic_constants.advance_frame();
           device->end_frame(current_frame);
           frame_alloctor().free();
        }

        auto Renderer::prepare_frame_constants(TemporalGraph& rg,std::function<FrameConstantsLayout(rhi::DynamicConstants&)> prepare_frame_constants) -> void
        {
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
            temporal_rg_state.resources.clear();
            transient_resource_cache.clear_resource();
        }

        auto Renderer::refresh_shaders()->void
        {
            pipeline_cache.refresh_shaders();
        }
    }
}
