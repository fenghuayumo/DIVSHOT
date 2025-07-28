#ifdef DS_RENDER_METAL
#include "gpu_device_metal.h"
#include "maths/maths_utils.h"
#include "core/ds_log.h"
namespace diverse
{
    namespace rhi
    {
        extern auto get_cs_local_size_from_spirv(const CompiledShaderCode& shader_code) -> std::array<u32, 3>;
    
        typedef struct
        {
            std::array<float,2> position;
        } MetalQuadVertex;
        static const MetalQuadVertex triVertices[] =
        {
            // Positions
            { {-1, -1}},
            {{ -1, 1}},
            {{1, -1}},
            {{1, 1}}
        };
        const std::string quad_source_shader = R"(
        #include <metal_stdlib>
        using namespace metal;

        struct QuadPipelineRasterizerData
        {
            float4 position [[position]];
            float4 color;
        };

        struct MetalQuadVertex
        {
            float2 position;
        };
        vertex QuadPipelineRasterizerData
        quadVertex(const uint vertexID [[ vertex_id ]],
                           const device MetalQuadVertex *vertices [[ buffer(0) ]],
                           const device float* color [[ buffer(1) ]] )
        {
            QuadPipelineRasterizerData out;

            out.position = vector_float4(0.0, 0.0, 0.0, 1.0);
            out.position.xy = vertices[vertexID].position.xy;

            out.color = float4(color[0],color[1],color[2],color[3]);
            return out;
        }

        fragment float4 quadFragment(QuadPipelineRasterizerData in [[stage_in]])
        {
            return in.color;
        })";
    
        auto create_metal_device(u32 device_index)->rhi::GpuDevice*
        {
            return new GpuDeviceMetal(device_index);
        }
    
        DeviceFrameMetal::~DeviceFrameMetal()
        {}
        GpuCommandBufferMetal::GpuCommandBufferMetal(struct GpuDeviceMetal* dev)
            : device(dev),computeEncoder(nil), renderEncoder(nil), cmd_buf(nil)
        {
        }
        void GpuCommandBufferMetal::begin()
        {
        }
    
        void GpuCommandBufferMetal::end()
        {
            cmd_labels.clear();
            current_id = 0;
        }
        void GpuCommandBufferMetal::wait()
        {
            [cmd_buf encodeWaitForEvent:frame->submit_done_event value:frame->event_cnt];
        }
        
        void GpuCommandBufferMetal::signal()
        {
            ++frame->event_cnt;
            [cmd_buf encodeSignalEvent:frame->submit_done_event value:frame->event_cnt];
        }
    
        DeviceFrameMetal::DeviceFrameMetal(id<MTLDevice> device,const std::shared_ptr<CommandBuffer>& main_cb, const std::shared_ptr<CommandBuffer>& presentation_cb)
        : DeviceFrame(main_cb, presentation_cb)
        {
            submit_done_event = [device newEvent];
        }
    
        GpuDeviceMetal::GpuDeviceMetal(u32 device_indx)
        {
           device = MTLCreateSystemDefaultDevice();
           commandQueue  = [device newCommandQueue];
            
            for (int i = 0; i < 2; i++)
            {
                auto main_cmd = std::make_shared<GpuCommandBufferMetal>(this);
                // main_cmd->cmd_buf.label = [NSString stringWithFormat:@"main_cmd_%d", i];
                auto present_cmd = std::make_shared<GpuCommandBufferMetal>(this);
                // present_cmd->cmd_buf.label = [NSString stringWithFormat:@"present_cmd_%d", i];
                frames[i] = DeviceFrameMetal(device,main_cmd, present_cmd);
                main_cmd->frame = &frames[i];
                present_cmd->frame = &frames[i];
            }
            
            MTLArgumentBuffersTier argumentBufferSupport = device.argumentBuffersSupport;
            if ( @available( macOS 13, iOS 16, *) )
             {
                 if( [device supportsFamily:MTLGPUFamilyMetal3] )
                 {
                     gpu_limits.support_bindless = YES;
                     //should lookup
                     gpu_limits.maxDescriptorSetUpdateAfterBindSampledImages = 31;
                     gpu_limits.maxDescriptorSetUpdateAfterBindUniformBuffers = 31;
                     gpu_limits.maxDescriptorSetUpdateAfterBindStorageImages = 31;
                     gpu_limits.maxDescriptorSetUpdateAfterBindStorageBuffers = 31;
                     
                     gpu_limits.maxPerStageDescriptorUpdateAfterBindUniformBuffers = 8000;
                     gpu_limits.maxPerStageDescriptorUpdateAfterBindStorageImages = 8000;
                     gpu_limits.maxPerStageDescriptorUpdateAfterBindSampledImages = 8000;
                     gpu_limits.maxDescriptorSetUpdateAfterBindStorageBuffers = 8000;
                 }
             }
            
            immutable_samplers = create_samplers();
            setup_cb.reset(new GpuCommandBufferMetal(this));
            auto crash_buffer_desc = GpuBufferDesc::new_gpu_to_cpu(4, BufferUsageFlags::TRANSFER_DST);
            crash_tracking_buffer = create_buffer(crash_buffer_desc,"crash tracing buffer", nullptr);
            
            NSError* error = nil;
 
            defaultlibrary = [device newLibraryWithSource:[NSString stringWithUTF8String:quad_source_shader.c_str()] options:nil error:&error];
            if (defaultlibrary == nil)
            {
                NSLog(@"Error: failed to create Metal library: %@", error);
            }
            
        }

        GpuDeviceMetal::~GpuDeviceMetal()
        {
        }
        auto GpuDeviceMetal::create_samplers()->std::unordered_map<rhi::GpuSamplerDesc, id<MTLSamplerState>>
        {
            auto texel_filters = {TexelFilter::NEAREST, TexelFilter ::LINEAR};
            auto mipmaps_modes = { SamplerMipmapMode::NEAREST, SamplerMipmapMode::LINEAR };
            auto address_modes = { SamplerAddressMode::REPEAT, SamplerAddressMode ::CLAMP_TO_EDGE};
            
            std::unordered_map<rhi::GpuSamplerDesc, id<MTLSamplerState>> ret;
            for (auto& texel_filter : texel_filters)
            {
                for (auto& mipmap_mode : mipmaps_modes)
                {
                    for (auto& address_mode : address_modes)
                    {
                        // Create a sampler descriptor
                        MTLSamplerDescriptor *samplerDescriptor = [[MTLSamplerDescriptor alloc] init];
                        samplerDescriptor.rAddressMode = texel_address_mode_2_metal(address_mode);
                        samplerDescriptor.sAddressMode = texel_address_mode_2_metal(address_mode);
                        samplerDescriptor.tAddressMode = texel_address_mode_2_metal(address_mode);
                        samplerDescriptor.minFilter = texel_filter_mode_2_metal(texel_filter);
                        samplerDescriptor.magFilter = texel_filter_mode_2_metal(texel_filter);
                        samplerDescriptor.mipFilter = texel_filter == TexelFilter::NEAREST ? MTLSamplerMipFilterNearest : MTLSamplerMipFilterLinear;
                        samplerDescriptor.maxAnisotropy = 1;
                        samplerDescriptor.normalizedCoordinates = YES;
                        samplerDescriptor.lodMinClamp = 0;
                        samplerDescriptor.lodMaxClamp = FLT_MAX;
                        samplerDescriptor.maxAnisotropy = 16;
                        samplerDescriptor.borderColor = MTLSamplerBorderColorTransparentBlack;
                        auto sampler = [device newSamplerStateWithDescriptor:samplerDescriptor];
                        auto sampler_desc = GpuSamplerDesc{ texel_filter, mipmap_mode, address_mode };
                        ret[sampler_desc] = sampler;
                    }
                }
            }
            return ret;
        }
        auto GpuDeviceMetal::create_swapchain(SwapchainDesc desc, void* window_handle) -> std::shared_ptr<Swapchain>
        {
            auto swapchain = std::make_shared<SwapchainMetal>(*this,desc, window_handle);
            acquire_semaphore = dispatch_semaphore_create(swapchain->images.size());
            return swapchain;
        }

        // auto GpuDeviceMetal::create_render_command_buffer(const char* name = nullptr) -> std::shared_ptr<CommandBuffer>
        // {
        //     auto cmd = std::make_shared<SwapchainMetal>(*this,desc, window_handle);
        //     return cmd;
        // }
        auto GpuDeviceMetal::begin_frame() -> DeviceFrame*
        {
            dispatch_semaphore_wait(acquire_semaphore, DISPATCH_TIME_FOREVER);
            std::lock_guard<std::mutex>    lock(frame_mutex[0]);
            auto& frame0 = frames[0];
            auto main_cmd = dynamic_pointer_cast<GpuCommandBufferMetal>(frame0.main_cmd_buf);
            main_cmd->cmd_buf = [commandQueue commandBuffer];
            main_cmd->cmd_buf.label =   [NSString stringWithUTF8String: "main_cmd_0"];
          
            auto present_cmd = dynamic_pointer_cast<GpuCommandBufferMetal>(frame0.presentation_cmd_buf);
            present_cmd->cmd_buf = [commandQueue commandBuffer];
            present_cmd->cmd_buf.label =   [NSString stringWithUTF8String: "present_cmd_0"];
            
            __block dispatch_semaphore_t block_semaphore = acquire_semaphore;
           [present_cmd->cmd_buf addCompletedHandler:^(id<MTLCommandBuffer> buffer){
              dispatch_semaphore_signal(block_semaphore);
           }];
//            frame0.pending_resource_releases.release_all(device);
            return &frame0;
        }

        auto GpuDeviceMetal::end_frame(DeviceFrame* frame) -> void
        {
//            std::lock_guard<std::mutex>    lock0(frame_mutex[0]);
//            auto& frame0 = frames[0];
//            
//            std::lock_guard<std::mutex>    lock1(frame_mutex[1]);
//            auto& frame1 = frames[1];
//            std::swap(frame0, frame1);
        }

        auto GpuDeviceMetal::create_buffer(const GpuBufferDesc& desc,const char* name, uint8* initial_data)->std::shared_ptr<GpuBuffer> 
        {
            id<MTLBuffer> buffer = [device newBufferWithLength:desc.size options:MTLResourceStorageModeShared];
            //set buffer label
            buffer.label = [NSString stringWithUTF8String:name];
            if( initial_data)
            {
                void* contents = [buffer contents];
                memcpy(contents, initial_data, desc.size);
            }
            return std::make_shared<GpuBufferMetal>(desc, buffer);
        }

        auto GpuDeviceMetal::create_texture(const GpuTextureDesc& desc,const std::vector<ImageSubData>&  initial_data,const char* name)->std::shared_ptr<GpuTexture>
        {
            MTLTextureDescriptor *texDesc = [[MTLTextureDescriptor alloc] init];
            texDesc.textureType = texture_type_to_metal(desc.image_type);
            texDesc.pixelFormat = pixel_format_2_metal(desc.format);
            texDesc.width = desc.extent[0];
            texDesc.height = desc.extent[1];
            texDesc.depth = desc.extent[2];
            texDesc.arrayLength = desc.image_type == TextureType::Tex2dArray ? desc.array_elements : 1;
            texDesc.mipmapLevelCount = desc.mip_levels;
            texDesc.usage = texture_usage_2_metal(desc.usage);
#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
            texDesc.storageMode = MTLStorageModeManaged;
#else
            texDesc.storageMode = MTLStorageModeShared;
#endif
            texDesc.sampleCount = desc.sample_count;
            
            id<MTLTexture> texture = [device newTextureWithDescriptor:texDesc];
            texture.label = [NSString stringWithUTF8String:name];
            if( initial_data.size() > 0)
            {
                for(auto i=0; i<initial_data.size();i++)
                {
                    auto& data = initial_data[i];
                    auto ptr_data = (const void*)data.data;
                    auto bytesPerRow = data.row_pitch;//maths::divide_rounding_up(data.size, std::max(1u, desc.extent[1] >> 1));
                    [texture replaceRegion:MTLRegionMake2D(0, 0, std::max(1u,desc.extent[0] >> i), std::max(1u,desc.extent[1] >> i)) mipmapLevel:i withBytes:ptr_data bytesPerRow: bytesPerRow];
                  
//                    [texture replaceRegion:MTLRegionMake2D(0, 0,std::max(1u,desc.extent[0] >> i), std::max(1u,desc.extent[1] >> i)) mipmapLevel:i slice:data.slice_pitch withBytes:ptr_data bytesPerRow:bytesPerRow bytesPerImage:bytesPerRow * std::max(1u,desc.extent[1] >> i)];
                }
            }
            auto metal_texture = std::make_shared<GpuTextureMetal>();
            metal_texture->id_tex = texture;
            metal_texture->desc = desc;

            return metal_texture;
        }

        auto GpuDeviceMetal::create_render_pass(const RenderPassDesc& desc,const char* name) -> std::shared_ptr<RenderPass>
        {
            auto pass = std::make_shared<RenderPassMetal>();
            pass->render_pass = [MTLRenderPassDescriptor new];
            
            for(auto i=0; i<desc.color_attachments.size();i++)
            {
                 auto colorAttachment = pass->render_pass.colorAttachments[i];
                 colorAttachment.loadAction = to_metal_load_op(desc.color_attachments[i].load_op);
                 colorAttachment.clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);
                 colorAttachment.storeAction = to_metal_store_op(desc.color_attachments[i].store_op);//MTLStoreActionStore;
                
            }
            if( desc.depth_attachment.has_value() )
            {
                auto depthAttachment = pass->render_pass.depthAttachment;
                depthAttachment.loadAction = to_metal_load_op(desc.depth_attachment->load_op);
                depthAttachment.clearDepth = 0.0;
                depthAttachment.storeAction = to_metal_store_op(desc.depth_attachment->store_op);//MTLStoreActionStore;
            }
            pass->desc = desc;
            // pass->framebuffer_cache = FrameBufferCacheMetal(render_pass, desc.color_attachments, desc.depth_attachment);
            return pass;
        }

        auto GpuDeviceMetal::create_descriptor_set(GpuBuffer* dynamic_constants, const std::unordered_map<u32, DescriptorInfo>& descriptors,const char* name) -> std::shared_ptr<DescriptorSet>
        {
            auto metal_descriptor = std::make_shared<MetalDescriptorSet>();
            for(const auto& [bind_id, info] : descriptors)
            {
                if( info.dimensionality.ty == DescriptorDimensionality::RuntimeArray)
                {
                    metal_descriptor->resource_bindings[bind_id].argument_buffer = [device newBufferWithLength:sizeof(u64)*gpu_limits.maxPerStageDescriptorUpdateAfterBindUniformBuffers options:MTLResourceStorageModeShared];
                    metal_descriptor->resource_bindings[bind_id].argument_buffer.label =[NSString stringWithFormat:@"secondBuffer"];
                }
                else
                {
                    metal_descriptor->resource_bindings[bind_id].resource = reinterpret_cast<GpuBufferMetal*>(dynamic_constants)->id_buf;
                }
                metal_descriptor->resource_bindings[bind_id].usage = info.ty == DescriptorType::UNIFORM_BUFFER ? MTLResourceUsageRead : MTLResourceUsageWrite;
            }
            metal_descriptor->argument_buffer = [device newBufferWithLength:sizeof(u64)* descriptors.size() options:MTLResourceStorageModeShared];
            metal_descriptor->argument_buffer.label = [NSString stringWithFormat:@"dynamic_constant_args"];
            u64* data = (u64*)metal_descriptor->argument_buffer.contents;
            for (auto i=0; i<descriptors.size(); i++) 
            {
                data[i] = reinterpret_cast<GpuBufferMetal*>(dynamic_constants)->id_buf.gpuAddress;
            }
//#ifdef DS_PLATFORM_MACOS
//            [metal_descriptor->argument_buffer didModifyRange:NSMakeRange(0, metal_descriptor->argument_buffer.length)];
//#endif
            return metal_descriptor;
        }
        
        auto GpuDeviceMetal::create_descriptor_set(const std::unordered_map<u32, DescriptorInfo>& descriptors,const char* name) -> std::shared_ptr<DescriptorSet>
        {
            auto metal_descriptor_set = std::make_shared<MetalDescriptorSet>();
            for(const auto& [index, info] : descriptors)
            {
                if( info.dimensionality.ty == DescriptorDimensionality::RuntimeArray)
                {
                    metal_descriptor_set->resource_bindings[index].argument_buffer = [device newBufferWithLength:sizeof(u64) * gpu_limits.maxPerStageDescriptorUpdateAfterBindSampledImages options:MTLResourceStorageModeShared];
                    metal_descriptor_set->resource_bindings[index].argument_buffer.label =[NSString stringWithFormat:@"secondBuffer"];
                }
                metal_descriptor_set->resource_bindings[index].usage = info.ty == DescriptorType::UNIFORM_BUFFER ? MTLResourceUsageRead : MTLResourceUsageWrite;
            }
            metal_descriptor_set->argument_buffer = [device newBufferWithLength:sizeof(MTLResourceID)* descriptors.size() options:MTLResourceStorageModeShared];
            metal_descriptor_set->argument_buffer.label = [NSString stringWithFormat:@"bindless_argsBuffer"];
            u64* data = (u64*)metal_descriptor_set->argument_buffer.contents;
            u32 i = 0;
            for (const auto& [idx, info] : descriptors)
            {
                if( info.dimensionality.ty == DescriptorDimensionality::RuntimeArray)
                    data[i++] = metal_descriptor_set->resource_bindings[idx].argument_buffer.gpuAddress;
                else
                    i++;
            }
//#ifdef DS_PLATFORM_MACOS
//            [metal_descriptor_set->argument_buffer didModifyRange:NSMakeRange(0, metal_descriptor_set->argument_buffer.length)];
//#endif
            return metal_descriptor_set;
        }

        auto GpuDeviceMetal::bind_descriptor_set(CommandBuffer* cb,GpuPipeline* pipeline,std::vector<DescriptorSetBinding>& bindings,uint32 set_index)->void
        {
            auto cmd_metal = dynamic_cast<GpuCommandBufferMetal*>(cb);
            auto pipeline_metal = reinterpret_cast<GpuPipelineMetal*>(pipeline);
            u32 arg_offset = 0;
            if( pipeline_metal->descriptor_layouts.find(set_index) == pipeline_metal->descriptor_layouts.end() )
                return;
            auto& descriptor_set = pipeline_metal->descriptor_sets[set_index];
            auto& descriptor_layouts = pipeline_metal->descriptor_layouts[set_index];
            if( descriptor_set.argument_buffer == nil)
            {
                auto buffer  = create_argument_buffer(descriptor_layouts);
                buffer.label = [NSString stringWithFormat:@"argsBuffer%d",set_index];
                descriptor_set.argument_buffer = buffer;
            }
            auto argument_buffer = descriptor_set.argument_buffer;

            auto write_descriptor = [&](){
                if( descriptor_set.flag) return;
                for(auto bind_idx = 0; bind_idx < bindings.size();bind_idx++)
                {
                    auto& binding = bindings[bind_idx];
                    switch (binding.ty)
                    {
                        case DescriptorSetBinding::Type::Image:
                        {
                            auto& image = binding.Image();
                            id<MTLTexture> texture = reinterpret_cast<GpuTextureViewMetal*>(image.view)->id_tex;
                            if( descriptor_layouts.find(bind_idx) != descriptor_layouts.end())
                            {
                                MTLResourceID* data = (MTLResourceID*)argument_buffer.contents;
                                data[arg_offset++] = texture.gpuResourceID;
                                
                                auto usage = descriptor_layouts[bind_idx].ty == DescriptorType::SAMPLED_IMAGE ? MTLResourceUsageRead : MTLResourceUsageWrite;
                                descriptor_set.resource_bindings[bind_idx] = {texture, usage};
                            }
                        }
                            break;
                        case DescriptorSetBinding::Type::Buffer:
                        {
                            auto& buffer = binding.Buffer();
                            id<MTLBuffer> id_buf = reinterpret_cast<GpuBufferMetal*>(buffer.buffer)->id_buf;
                            if( descriptor_layouts.find(bind_idx) != descriptor_layouts.end())
                            {
                                u64* data = (u64*)argument_buffer.contents;
                                data[arg_offset++] = id_buf.gpuAddress;
                                
                                auto usage = descriptor_layouts[bind_idx].ty == DescriptorType::UNIFORM_BUFFER ? MTLResourceUsageRead : MTLResourceUsageWrite;
                                descriptor_set.resource_bindings[bind_idx] = {id_buf, usage};
                            }
                        }break;
                        case DescriptorSetBinding::Type::DynamicBuffer:
                        case DescriptorSetBinding::Type::DynamicStorageBuffer:
                        {
                            auto& [dy_buf, offset] = binding.DynamicBuffer();
                            id<MTLBuffer> buffer = reinterpret_cast<GpuBufferMetal*>(dy_buf)->id_buf;
                            if( descriptor_layouts.find(bind_idx) != descriptor_layouts.end())
                            {
                                u64* data = (u64*)argument_buffer.contents;
                                data[arg_offset++] = buffer.gpuAddress + offset;
                                
                                auto usage = descriptor_layouts[bind_idx].ty == DescriptorType::UNIFORM_BUFFER ? MTLResourceUsageRead : MTLResourceUsageWrite;
                                descriptor_set.resource_bindings[bind_idx] = {buffer, usage};
                            }
                        }break;
                        case DescriptorSetBinding::Type::RayTracingAcceleration:
                        {
                        }break;
                    }
                }
//#if DS_PLATFORM_MACOS
//           [argument_buffer didModifyRange:NSMakeRange(0, argument_buffer.length)];
//#endif
                descriptor_set.flag = true;
            };
            if( pipeline->ty == GpuPipeline::PieplineType::Compute)
            {
                id<MTLComputeCommandEncoder> computeEncoder = cmd_metal->computeEncoder;
                write_descriptor();
                if( argument_buffer != nil)
                {
                    [computeEncoder setBuffer:argument_buffer offset:0 atIndex:set_index];
                    for(const auto& [idx, binding]: descriptor_set.resource_bindings)
                    {
                        if( binding.argument_buffer != nil )
                            [computeEncoder setBuffer:binding.argument_buffer offset:0 atIndex:idx];
                        if( binding.resource != nil)
                            [computeEncoder useResource:binding.resource usage:binding.usage];
                    }
                       
                }
            }
            else
            {
                id<MTLRenderCommandEncoder> renderEncoder = cmd_metal->renderEncoder;
                write_descriptor();
                if( argument_buffer != nil)
                {
                    [renderEncoder setVertexBuffer:argument_buffer offset:0 atIndex:set_index];
                    [renderEncoder setFragmentBuffer:argument_buffer offset:0 atIndex:set_index];
                    for(const auto& [idx, binding]: descriptor_set.resource_bindings)
                    {
                        if( binding.argument_buffer != nil)
                        {
                            [renderEncoder setVertexBuffer:argument_buffer offset:0 atIndex:idx];
                            [renderEncoder setFragmentBuffer:argument_buffer offset:0 atIndex:idx];
                        }
                        if( binding.resource != nil)
                            [renderEncoder useResource:binding.resource usage:binding.usage stages:MTLRenderStageFragment];
                    }
                }
            }
        }
        //frame constants
        auto GpuDeviceMetal::bind_descriptor_set(CommandBuffer* cb, GpuPipeline* pipeline, uint32 set_idx,DescriptorSet* set, u32 dynamic_offset_count, u32* dynamic_offset) -> void
        {
            auto cmd_metal = dynamic_cast<GpuCommandBufferMetal*>(cb);
            auto pipeline_metal = reinterpret_cast<GpuPipelineMetal*>(pipeline);
            auto metal_descriptor_set = *(reinterpret_cast<MetalDescriptorSet*>(set));
            u32 arg_offset = 0;
            if( pipeline_metal->descriptor_layouts.find(set_idx) == pipeline_metal->descriptor_layouts.end() )
                return;
            auto& descriptor_set = pipeline_metal->descriptor_sets[set_idx];
            auto& descriptor_layouts = pipeline_metal->descriptor_layouts[set_idx];
            if( descriptor_set.argument_buffer == nil)
            {
                auto buffer  = create_argument_buffer(descriptor_layouts);
                buffer.label = [NSString stringWithFormat:@"argsBuffer%d",set_idx];
                descriptor_set.argument_buffer = buffer;
            }
            auto argument_buffer = descriptor_set.argument_buffer;

            if( !descriptor_set.flag)
            {
                u64* data = (u64*)argument_buffer.contents;
                u64* bindless_data = (u64*)metal_descriptor_set.argument_buffer.contents;
                for(auto& [bind_id, info] : descriptor_layouts)
                {
                    if(metal_descriptor_set.resource_bindings[bind_id].argument_buffer != nil)
                    {
                        u64* binding_data = (u64*)metal_descriptor_set.resource_bindings[bind_id].argument_buffer.contents;
                        data[arg_offset++] = binding_data[0];
                    }
                    else
                    {
                        data[arg_offset++] = bindless_data[bind_id] + (dynamic_offset ? dynamic_offset[bind_id] : 0);
                    }
                    descriptor_set.resource_bindings[bind_id] = metal_descriptor_set.resource_bindings[bind_id];
                }
//#if DS_PLATFORM_MACOS
//                [argument_buffer didModifyRange:NSMakeRange(0, argument_buffer.length)];
//#endif
                descriptor_set.flag = true;
            }
            
            if(pipeline_metal->ty == GpuPipeline::PieplineType::Compute)
            {
                id<MTLComputeCommandEncoder> computeEncoder = cmd_metal->computeEncoder;
                if( argument_buffer != nil)
                {
                    [computeEncoder setBuffer:argument_buffer offset:0 atIndex:set_idx];
                    for(const auto& [idx, binding]: descriptor_set.resource_bindings)
                    {
//                        if( binding.argument_buffer != nil )
//                            [computeEncoder setBuffer:binding.argument_buffer offset:0 atIndex:idx];
                        if( binding.resource != nil && descriptor_layouts.find(idx) != descriptor_layouts.end() )
                            [computeEncoder useResource:binding.resource usage:binding.usage];
                    }
                }
            }
            else
            {
                id<MTLRenderCommandEncoder> renderEncoder = cmd_metal->renderEncoder;
                if( argument_buffer != nil)
                {
                    [renderEncoder setVertexBuffer:argument_buffer offset:0 atIndex:set_idx];
                    [renderEncoder setFragmentBuffer:argument_buffer offset:0 atIndex:set_idx];
                    for(const auto& [idx, binding]: descriptor_set.resource_bindings)
                    {
//                        if( binding.argument_buffer != nil )
//                            [renderEncoder setVertexBuffer:binding.argument_buffer offset:0 atIndex:idx];
                        if( binding.resource != nil && descriptor_layouts.find(idx) != descriptor_layouts.end())
                            [renderEncoder useResource:binding.resource usage:binding.usage stages:MTLRenderStageFragment];
                    }
                }
            }
            
        }

        auto GpuDeviceMetal::bind_pipeline(CommandBuffer* cb, GpuPipeline* pipeline) -> void
        {
            auto cmd_metal = dynamic_cast<GpuCommandBufferMetal*>(cb);
            if( pipeline->ty == GpuPipeline::PieplineType::Compute)
            {
                auto pipeline_metal = reinterpret_cast<ComputePipelineMetal*>(pipeline);
                cmd_metal->computeEncoder = [cmd_metal->cmd_buf computeCommandEncoder];
                cmd_metal->computeEncoder.label = [NSString stringWithUTF8String: cmd_metal->cmd_labels[cmd_metal->current_id]];
                [cmd_metal->computeEncoder setComputePipelineState:pipeline_metal->pipeline];
            }
            else if( pipeline->ty == GpuPipeline::PieplineType::Raster)
            {
                auto pipeline_metal = reinterpret_cast<RasterPipelineMetal*>(pipeline);
                cmd_metal->renderEncoder.label = [NSString stringWithUTF8String: cmd_metal->cmd_labels[cmd_metal->current_id]];
                [cmd_metal->renderEncoder setRenderPipelineState:pipeline_metal->pipeline];
                [cmd_metal->renderEncoder setFrontFacingWinding:pipeline_metal->front_face];
                [cmd_metal->renderEncoder setCullMode:pipeline_metal->cull_mode];
                [cmd_metal->renderEncoder setDepthStencilState:pipeline_metal->depth_stencil_state];
            }
            else if( pipeline->ty == GpuPipeline::PieplineType::RayTracing)
            {
                    
            }
            cmd_metal->current_id++;
        }

        auto GpuDeviceMetal::push_constants(CommandBuffer* cb, GpuPipeline* pipeline, u32 offset, u8* constants, u32 size_) -> void
        {

        }

        auto GpuDeviceMetal::create_compute_pipeline(const CompiledShaderCode& spirv, const ComputePipelineDesc& desc) -> std::shared_ptr<ComputePipeline>
        {
            auto metal_lib = get_metal_lib(device, spirv);
            auto group_size = get_cs_local_size_from_spirv(spirv);
            NSError* __autoreleasing error = nil;
            MTLAutoreleasedComputePipelineReflection cs_reflection = nil;
            const auto option = MTLPipelineOptionArgumentInfo| MTLPipelineOptionBufferTypeInfo;
            id<MTLFunction> metal_func = [metal_lib newFunctionWithName:metal_lib.functionNames.firstObject];
            id<MTLComputePipelineState>  cs_pipeline = [device newComputePipelineStateWithFunction:metal_func
                                                                options:option
                                                                reflection:&cs_reflection error:&error];
            if (error) {
                printf("%s: error: %s\n", __func__, [[error description] UTF8String]);
                return nullptr;
            }
            auto descriptor_layouts = create_descriptor_set_layouts(*this, std::move(get_descriptor_set_info(cs_reflection.arguments)), MetalShaderStageFlag::Compute, desc.descriptor_set_opts);
            auto pipelineMetal = std::make_shared<ComputePipelineMetal>(cs_pipeline, group_size,descriptor_layouts);;
            return pipelineMetal;
        }

        auto GpuDeviceMetal::create_raster_pipeline(const std::vector<PipelineShader>& shaders, const RasterPipelineDesc& desc) -> std::shared_ptr<RasterPipeline> 
        {
            NSError* __autoreleasing error = nil;
            MTLAutoreleasedRenderPipelineReflection reflectInfo = nil;
            const auto option = MTLPipelineOptionArgumentInfo| MTLPipelineOptionBufferTypeInfo;
            // auto [descriptor_set_layouts, set_layout_info] = create_descriptor_set_layouts(*this, std::move(merge_shader_stage_layouts(stage_layouts)), desc.descriptor_set_opts);
            MTLRenderPipelineDescriptor *rpd = [[MTLRenderPipelineDescriptor alloc] init];
         
            rpd.label = @"Render Pipeline";
//            rpd.sampleCount = 1;
            for(const auto& shader : shaders)
            {
                auto metal_lib = get_metal_lib(device, shader.code,true);
                id<MTLFunction> func = [metal_lib newFunctionWithName:metal_lib.functionNames.firstObject];
                if( shader.desc.stage == ShaderPipelineStage::Vertex)
                    rpd.vertexFunction = func;
                else if( shader.desc.stage == ShaderPipelineStage::Pixel)
                    rpd.fragmentFunction = func;
            }
            
            auto render_pass = dynamic_cast<RenderPassMetal*>(desc.render_pass.get());
            for(auto i=0;i<render_pass->desc.color_attachments.size();i++)
            {
                rpd.colorAttachments[i].pixelFormat = pixel_format_2_metal(render_pass->desc.color_attachments[i].format);
                rpd.colorAttachments[i].rgbBlendOperation = MTLBlendOperationAdd;
                rpd.colorAttachments[i].alphaBlendOperation = MTLBlendOperationAdd;
                if( desc.blend_enabled)
                {
                    rpd.colorAttachments[i].blendingEnabled = YES;
                    rpd.colorAttachments[i].sourceAlphaBlendFactor = MTLBlendFactorOne;
                    rpd.colorAttachments[i].destinationAlphaBlendFactor = MTLBlendFactorOne;
                    if(desc.blend_mode == BlendMode::SrcAlphaOneMinusSrcAlpha)
                    {
                        rpd.colorAttachments[i].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
                        rpd.colorAttachments[i].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
                        rpd.colorAttachments[i].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
                        rpd.colorAttachments[i].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
                    }
                    else if(desc.blend_mode == BlendMode::SrcAlphaOne)
                    {
                        rpd.colorAttachments[i].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
                        rpd.colorAttachments[i].destinationRGBBlendFactor = MTLBlendFactorOne;
                        rpd.colorAttachments[i].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
                        rpd.colorAttachments[i].destinationAlphaBlendFactor = MTLBlendFactorOne;
                    }
                    else if(desc.blend_mode == BlendMode::ZeroSrcColor)
                    {
                        rpd.colorAttachments[i].sourceRGBBlendFactor = MTLBlendFactorZero;
                        rpd.colorAttachments[i].destinationRGBBlendFactor = MTLBlendFactorSourceColor;
                    }
                    else if(desc.blend_mode == BlendMode::OneZero)
                    {
                        rpd.colorAttachments[i].sourceRGBBlendFactor = MTLBlendFactorOne;
                        rpd.colorAttachments[i].destinationRGBBlendFactor = MTLBlendFactorZero;
                    }
                    else if (desc.blend_mode == BlendMode::OneOneMinusSrcAlpha)
                    {
                        rpd.colorAttachments[i].sourceRGBBlendFactor = MTLBlendFactorOne;
                        rpd.colorAttachments[i].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
                    }
                    else
                    {
                        rpd.colorAttachments[i].sourceRGBBlendFactor = MTLBlendFactorZero;
                        rpd.colorAttachments[i].destinationRGBBlendFactor = MTLBlendFactorZero;
                    }
                }
                else
                {
                    rpd.colorAttachments[i].blendingEnabled = NO;
                    rpd.colorAttachments[i].sourceRGBBlendFactor = MTLBlendFactorZero;
                    rpd.colorAttachments[i].destinationRGBBlendFactor = MTLBlendFactorZero;
                    rpd.colorAttachments[i].sourceAlphaBlendFactor = MTLBlendFactorZero;
                    rpd.colorAttachments[i].destinationAlphaBlendFactor = MTLBlendFactorZero;
                }
            }
            rpd.depthAttachmentPixelFormat = render_pass->desc.depth_attachment.has_value() ? pixel_format_2_metal( render_pass->desc.depth_attachment->format) : MTLPixelFormatDepth24Unorm_Stencil8;
            rpd.stencilAttachmentPixelFormat = MTLPixelFormatInvalid;//render_pass->desc.depth_attachment.has_value() ? pixel_format_2_metal( render_pass->desc.depth_attachment->format) : MTLPixelFormatDepth24Unorm_Stencil8;
            id<MTLRenderPipelineState>  raster_pso = [device newRenderPipelineStateWithDescriptor:rpd options:option
                                                                                       reflection:&reflectInfo error:&error];
            if (error) 
            {
                printf("%s: error: %s\n", __func__, [[error description] UTF8String]);
                return nullptr;
            }
       
            MTLDepthStencilDescriptor *depthStateDesc = [[MTLDepthStencilDescriptor alloc] init];
            depthStateDesc.depthCompareFunction = MTLCompareFunctionGreater;
            depthStateDesc.depthWriteEnabled = desc.depth_write ? YES : NO;
            
            auto depth_stencil_state = [device newDepthStencilStateWithDescriptor:depthStateDesc];
            if (error)
            {
                DS_LOG_WARN("{}: error: {}\n", __func__, [[error description] UTF8String]);
            }
            
            std::vector<StageDescriptorSetLayouts>  stage_layouts;
            stage_layouts.push_back(get_descriptor_set_info(reflectInfo.vertexArguments));
            stage_layouts.push_back(get_descriptor_set_info(reflectInfo.fragmentArguments));
            
            auto pipeline_layouts = create_descriptor_set_layouts(*this, std::move(merge_shader_stage_layouts(stage_layouts)),  MetalShaderStageFlag::All, desc.descriptor_set_opts);
            
            auto rasterpipeline = std::make_shared<RasterPipelineMetal>(raster_pso,pipeline_layouts);
            rasterpipeline->depth_stencil_state = depth_stencil_state;
            rasterpipeline->cull_mode = cull_model_2_metal(desc.cull_mode);
            rasterpipeline->front_face = face_oder_2_metal(desc.face_order);
            return rasterpipeline;
        }

        auto GpuDeviceMetal::create_ray_tracing_pipeline(const std::vector<PipelineShader>& shaders, const RayTracingPipelineDesc& desc) -> std::shared_ptr<RayTracingPipeline> 
        {
            return std::make_shared<RayTracingPipelineMetal>();
        }
    
        auto GpuDeviceMetal::create_argument_buffer(const DescriptorSetLayout& set)->id<MTLBuffer>
        {
            u32 buf_size = 0;
            for(auto bind : set)
            {
                if( bind.second.dimensionality.ty = DescriptorDimensionality::RuntimeArray)
                    buf_size += sizeof(u64);
                else if( bind.second.dimensionality.ty == DescriptorDimensionality::Single)
                    buf_size += sizeof(u64);
                else
                    buf_size += sizeof(u64);
            }
            id<MTLBuffer> buffer = [device newBufferWithLength:buf_size options:MTLResourceStorageModeShared];
            return buffer;
        }

        auto GpuDeviceMetal::bind_vertex_buffers(CommandBuffer* cb, const GpuBuffer* const* vertexBuffers, uint32_t slot, uint32_t count, const uint32_t* strides, const uint64_t* offsets) -> void
        {
            auto cmd_metal = dynamic_cast<GpuCommandBufferMetal*>(cb);
            //bind verte
        }
        
        auto GpuDeviceMetal::bind_index_buffer(CommandBuffer* cb, const GpuBuffer* indexBuffer, const IndexBufferFormat format, uint64_t offset) -> void
        {
            if (indexBuffer != nullptr)
            {
            }
        }
        auto GpuDeviceMetal::draw(CommandBuffer* cb, uint32_t vertexCount, uint32_t startVertexLocation) -> void
        {
        }
        auto GpuDeviceMetal::draw_indexed(CommandBuffer* cb, uint32_t indexCount, uint32_t startIndexLocation, int32_t baseVertexLocation) -> void
        {
            auto cmd_metal = dynamic_cast<GpuCommandBufferMetal*>(cb);
            [cmd_metal->renderEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:indexCount indexType:MTLIndexTypeUInt32 indexBuffer:nil indexBufferOffset:0];
        }
        auto GpuDeviceMetal::draw_instanced(CommandBuffer* cb, uint32_t vertexCount, uint32_t instanceCount, uint32_t startVertexLocation, uint32_t startInstanceLocation) -> void
        {
           auto cmd_metal = dynamic_cast<GpuCommandBufferMetal*>(cb);
           [cmd_metal->renderEncoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:startVertexLocation vertexCount:vertexCount instanceCount:instanceCount baseInstance:startInstanceLocation];

        }
        auto GpuDeviceMetal::draw_indexed_instanced(CommandBuffer* cb, uint32_t indexCount, uint32_t instanceCount, uint32_t startIndexLocation, int32_t baseVertexLocation, uint32_t startInstanceLocation) -> void
        {
           auto cmd_metal = dynamic_cast<GpuCommandBufferMetal*>(cb);
            [cmd_metal->renderEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:indexCount indexType:MTLIndexTypeUInt32 indexBuffer:nil indexBufferOffset:0 instanceCount:instanceCount baseVertex:baseVertexLocation baseInstance:startInstanceLocation];
        }
        auto GpuDeviceMetal::draw_instanced_indirect(CommandBuffer* cb, const GpuBuffer* args, uint64_t args_offset) -> void
        {
           auto cmd_metal = dynamic_cast<GpuCommandBufferMetal*>(cb);
           [cmd_metal->renderEncoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3 instanceCount:1 baseInstance:0];
        }
        auto GpuDeviceMetal::draw_indexed_instanced_indirect(CommandBuffer* cb, const GpuBuffer* args, uint64_t args_offset) -> void
        {
           auto cmd_metal = dynamic_cast<GpuCommandBufferMetal*>(cb);
           [cmd_metal->renderEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:3 indexType:MTLIndexTypeUInt32 indexBuffer:nil indexBufferOffset:0 instanceCount:1 baseVertex:0 baseInstance:0];
        }
        auto GpuDeviceMetal::draw_instanced_indirect_count(CommandBuffer* cb, const GpuBuffer* args, uint64_t args_offset, const GpuBuffer* count, uint64_t count_offset, uint32_t max_count) -> void
        {
           
        }
        auto GpuDeviceMetal::draw_indexed_instanced_indirect_count(CommandBuffer* cb, const GpuBuffer* args, uint64_t args_offset, const GpuBuffer* count, uint64_t count_offset, uint32_t max_count) -> void
        {
           
        }
        auto GpuDeviceMetal::begin_cmd(CommandBuffer* cb) -> void
        {
            cb->begin();
        }

        auto GpuDeviceMetal::end_cmd(CommandBuffer* cb) -> void
        {
            cb->end();
        }

        auto GpuDeviceMetal::submit_cmd(CommandBuffer* cb) -> void
        {
            auto cmd_metal = dynamic_cast<GpuCommandBufferMetal*>(cb);
//            cmd_metal->signal();
            [cmd_metal->cmd_buf commit];
        }

        
        auto GpuDeviceMetal::execute_cmd(CommandBuffer* cb) -> void
        {
            auto cmd_metal = dynamic_cast<GpuCommandBufferMetal*>(cb);
//            cmd_metal->signal();
            [cmd_metal->cmd_buf commit];
        }


        auto GpuDeviceMetal::record_image_barrier(CommandBuffer* cb, const ImageBarrier& barrier) -> void
        {

        }

        auto GpuDeviceMetal::record_buffer_barrier(CommandBuffer* cb, const BufferBarrier& barrier)->void
        {

        }

        auto GpuDeviceMetal::record_global_barrier(CommandBuffer* cb,const std::vector<rhi::AccessType>& previous_accesses, const std::vector<rhi::AccessType>& next_accesses) -> void
        {

        }
        auto GpuDeviceMetal::dispatch(CommandBuffer* cb,const std::array<u32, 3>& group_dim, const std::array<u32, 3>& group_size) -> void
        {
            auto cmd_metal = dynamic_cast<GpuCommandBufferMetal*>(cb);
            [cmd_metal->computeEncoder dispatchThreadgroups:MTLSizeMake(group_dim[0], group_dim[1], group_dim[2]) threadsPerThreadgroup:MTLSizeMake(group_size[0], group_size[1], group_size[2]) ];
            [cmd_metal->computeEncoder endEncoding];
        }

        auto GpuDeviceMetal::dispatch_indirect(CommandBuffer* cb,GpuBuffer* args_buffer, u64 args_buffer_offset) -> void
        {
           
        }

        auto GpuDeviceMetal::write_descriptor_set(DescriptorSet* descriptor_set, u32 dst_binding, rhi::GpuBuffer* buffer, u32 array_index)->void
        {
            auto write_set = reinterpret_cast<MetalDescriptorSet*>(descriptor_set);
            auto argument_buffer = write_set->resource_bindings[dst_binding].argument_buffer;
            assert(argument_buffer != nil);
            auto write_element = (u64*)argument_buffer.contents;
            write_element[array_index] = dynamic_cast<GpuBufferMetal*>(buffer)->id_buf.gpuAddress;
//#if DS_PLATFORM_MACOS
//            [write_set->resource_bindings[dst_binding].argument_buffer didModifyRange:NSMakeRange(array_index * sizeof(u64), sizeof(u64) * (array_index + 1))];
//#endif
            write_set->resource_bindings[dst_binding].resource =  dynamic_cast<GpuBufferMetal*>(buffer)->id_buf;
        }

        auto GpuDeviceMetal::write_descriptor_set(DescriptorSet* descriptor_set, u32 dst_binding, u32 array_index, const rhi::DescriptorImageInfo& img_info) -> void
        {
            auto write_set = reinterpret_cast<MetalDescriptorSet*>(descriptor_set);
            auto write_element = (MTLResourceID*)write_set->resource_bindings[dst_binding].argument_buffer.contents;
            auto texture = reinterpret_cast<GpuTextureViewMetal*>(img_info.view)->id_tex;
            write_element[array_index] = texture.gpuResourceID;
// #if DS_PLATFORM_MACOS
//            [write_set->resource_bindings[dst_binding].argument_buffer didModifyRange:NSMakeRange(array_index * sizeof(u64), sizeof(u64) * (array_index + 1))];
//#endif
            write_set->resource_bindings[dst_binding].resource =  texture;
        }

        auto GpuDeviceMetal::clear_depth_stencil(CommandBuffer* cb, GpuTexture* texture, float depth, u32 stencil) -> void
        {
            auto cmd_metal = dynamic_cast<GpuCommandBufferMetal*>(cb);
            auto depthStencilTexture = dynamic_cast<rhi::GpuTextureMetal*>(texture)->id_tex;
            MTLRenderPassDescriptor *renderPassDescriptor = [[MTLRenderPassDescriptor alloc] init];
            renderPassDescriptor.depthAttachment.texture = depthStencilTexture;
            renderPassDescriptor.depthAttachment.loadAction = MTLLoadActionClear;
            renderPassDescriptor.depthAttachment.clearDepth = depth;
            renderPassDescriptor.stencilAttachment.texture = depthStencilTexture;
            renderPassDescriptor.stencilAttachment.loadAction = MTLLoadActionClear;
            renderPassDescriptor.stencilAttachment.clearStencil = stencil;
            id<MTLRenderCommandEncoder> renderEncoder = [cmd_metal->cmd_buf renderCommandEncoderWithDescriptor:renderPassDescriptor];
            renderEncoder.label = [NSString stringWithUTF8String:cmd_metal->cmd_labels[cmd_metal->current_id]];
        
            [renderEncoder endEncoding];
            cmd_metal->current_id++;
        }

        auto GpuDeviceMetal::clear_color(CommandBuffer* cb, GpuTexture* texture, const std::array<f32, 4>& color) -> void
        {
            auto cmd_metal = dynamic_cast<GpuCommandBufferMetal*>(cb);
            auto color_texture = dynamic_cast<GpuTextureMetal*>(texture)->id_tex;
//            
//            if( quadRenderPipeline == nil )
//            {
//                NSError*  error = nil;
//                if( defaultlibrary != nil)
//                {
//                    id<MTLFunction> vertexFunction = [defaultlibrary newFunctionWithName:@"quadVertex"];
//                    id<MTLFunction> fragmentFunction = [defaultlibrary newFunctionWithName:@"quadFragment"];
//                    
//                    if (vertexFunction == nil || fragmentFunction == nil)
//                    {
//                        NSLog(@"Error: failed to find Metal shader functions in library: %@", error);
//                    }
//                    pipelineStateDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
//                    pipelineStateDescriptor.label = @"Quad Render Pipeline";
//                    pipelineStateDescriptor.sampleCount = 1;
//                    pipelineStateDescriptor.vertexFunction =  vertexFunction;
//                    pipelineStateDescriptor.fragmentFunction =  fragmentFunction;
//                    pipelineStateDescriptor.colorAttachments[0].pixelFormat = pixel_format_2_metal(texture->desc.format);
//                    quadRenderPipeline = [device newRenderPipelineStateWithDescriptor:pipelineStateDescriptor error:&error];
//                }
//            }
            
            MTLRenderPassDescriptor *renderPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
            renderPassDescriptor.colorAttachments[0].texture = color_texture;
            renderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
            renderPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
            renderPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(color[0], color[1], color[2], color[3]);
   
            id<MTLRenderCommandEncoder> renderEncoder = [cmd_metal->cmd_buf renderCommandEncoderWithDescriptor:renderPassDescriptor];
            renderEncoder.label = [NSString stringWithUTF8String:cmd_metal->cmd_labels[cmd_metal->current_id]];
//            if( quadRenderPipeline != nil)
//            {
//                [renderEncoder setRenderPipelineState:quadRenderPipeline];
//                [renderEncoder setVertexBytes:&triVertices
//                                       length:sizeof(triVertices)
//                                      atIndex:0];
//                [renderEncoder setVertexBytes:color.data() length:sizeof(float)*4 atIndex:1];
//                [renderEncoder drawPrimitives:MTLPrimitiveTypeLineStrip
//                                  vertexStart:0
//                                  vertexCount:4];
//            }
            [renderEncoder endEncoding];
            cmd_metal->current_id++;
        }

        auto GpuDeviceMetal::copy_image(GpuTexture* src, GpuTexture* dst, CommandBuffer* cmd_buf) -> void
        {
            auto metal_cmd = dynamic_cast<GpuCommandBufferMetal*>(cmd_buf);
            auto src_metal = dynamic_cast<GpuTextureMetal*>(src);
            auto dst_metal = dynamic_cast<GpuTextureMetal*>(dst);
            [metal_cmd->cmd_buf blitTexture:src_metal->id_tex
                                sourceSlice:0
                                sourceLevel:0
                                sourceOrigin:MTLOriginMake(0, 0, 0)
                                sourceSize:MTLSizeMake(src->desc.extent[0], src->desc.extent[1], src->desc.extent[2])
                                toTexture:dst_metal->id_tex
                                destinationSlice:0
                                destinationLevel:0
                                destinationOrigin:MTLOriginMake(0, 0, 0)];
        }

        auto GpuDeviceMetal::copy_image(GpuBuffer* src, GpuTexture* dst, CommandBuffer* cmd_buf) -> void
        {
            auto metal_cmd = dynamic_cast<GpuCommandBufferMetal*>(cmd_buf);
            auto src_metal = dynamic_cast<GpuBufferMetal*>(src);
            auto dst_metal = dynamic_cast<GpuTextureMetal*>(dst);
            [metal_cmd->cmd_buf blitBuffer:src_metal->id_buf
                                offset:0
                                bytes:src->desc.size
                                toTexture:dst_metal->id_tex
                                slice:0
                                level:0
                                origin:MTLOriginMake(0, 0, 0)
                                size:MTLSizeMake(dst->desc.extent[0], dst->desc.extent[1], dst->desc.extent[2])];
        }

        auto GpuDeviceMetal::set_point_size(CommandBuffer* cb, float point_size)->void
        {

        }

        auto GpuDeviceMetal::set_line_width(CommandBuffer* cb, float line_width) -> void
        {

        }

        auto GpuDeviceMetal::set_viewport(CommandBuffer* cb, const ViewPort& view, const Scissor& scissors) -> void
        {

        }
        auto GpuDeviceMetal::begin_render_pass(CommandBuffer* cb,
                    const std::array<u32,2>& dims,
                    RenderPass* render_pass,
                    const std::vector<rhi::GpuTexture*>& color_tex,
                    rhi::GpuTexture* depth) -> void
        {
            auto render_pass_metal = dynamic_cast<RenderPassMetal*>(render_pass);
            for(auto tex_i = 0; tex_i<color_tex.size();tex_i++)
            {
                auto tex = dynamic_cast<GpuTextureMetal*>(color_tex[tex_i]);
                render_pass_metal->render_pass.colorAttachments[tex_i].texture = tex->id_tex;
            }
            if( depth )
            {
                auto tex = dynamic_cast<GpuTextureMetal*>(depth);
                render_pass_metal->render_pass.depthAttachment.texture = tex->id_tex;
            }
            auto cmd_metal = dynamic_cast<GpuCommandBufferMetal*>(cb);
            
            cmd_metal->renderEncoder = [cmd_metal->cmd_buf renderCommandEncoderWithDescriptor:render_pass_metal->render_pass];
        }

        auto GpuDeviceMetal::end_render_pass(CommandBuffer* cb) -> void
        {
            auto& encoder = dynamic_cast<GpuCommandBufferMetal*>(cb)->renderEncoder;
            [encoder endEncoding];
            encoder = nil;
        }

         auto GpuDeviceMetal::create_ray_tracing_bottom_acceleration(const RayTracingBottomAccelerationDesc& desc) -> std::shared_ptr<GpuRayTracingAcceleration>
         {

         }

         auto GpuDeviceMetal::create_ray_tracing_top_acceleration(const RayTracingTopAccelerationDesc& desc, const RayTracingAccelerationScratchBuffer& scratch_buffer) -> std::shared_ptr<GpuRayTracingAcceleration>
         {

         }

         auto GpuDeviceMetal::rebuild_ray_tracing_top_acceleration(CommandBuffer* cb, u64 instance_buffer_address, u64 instance_count, GpuRayTracingAcceleration* tlas, RayTracingAccelerationScratchBuffer* scratch_buffer) -> void
         {

         }

        auto GpuDeviceMetal::with_setup_cb(std::function<void(CommandBuffer* cmd)>&& callback) -> void
        {
            std::lock_guard<std::mutex>    lock(cb_mutex);
            setup_cb->cmd_buf = [commandQueue commandBuffer];
            callback(setup_cb.get());
            
            auto cmd_buf = setup_cb->cmd_buf;
            [cmd_buf commit];
            [cmd_buf waitUntilCompleted];
        }

        auto GpuDeviceMetal::copy_buffer(CommandBuffer* cmd, GpuBuffer* src, u64 src_offset, GpuBuffer* dst, u64 dst_offset, u64 size_) -> void
        {
            auto cmd_metal = dynamic_cast<GpuCommandBufferMetal*>(cmd);
            auto src_metal = dynamic_cast<GpuBufferMetal*>(src);
            auto dst_metal = dynamic_cast<GpuBufferMetal*>(dst);
            
            auto blitEncoder = [cmd_metal->cmd_buf blitCommandEncoder];
            [blitEncoder copyFromBuffer:src_metal->id_buf sourceOffset:src_offset toBuffer:dst_metal->id_buf destinationOffset:dst_offset size:size_];
            [blitEncoder endEncoding];
            
        }
        auto GpuDeviceMetal::trace_rays(CommandBuffer* cb, RayTracingPipeline* rtpipeline, const std::array<u32, 3>& threads) -> void
        {
            
        }
        auto GpuDeviceMetal::trace_rays_indirect(CommandBuffer* cb, RayTracingPipeline* rtpipeline, u64 args_buffer_address) -> void
        {

        }
        auto GpuDeviceMetal::event_begin(const char* name, CommandBuffer* cb) -> void
        {
            auto metal_cmd = dynamic_cast<GpuCommandBufferMetal*>(cb);
            metal_cmd->cmd_labels.push_back(name);
        }
        auto GpuDeviceMetal::event_end(CommandBuffer* cb) -> void
        {

        }
        auto GpuDeviceMetal::set_name(GpuResource* resource, const char* name) const-> void
        {
            if( resource->ty == GpuResource::Type::Buffer)
            {
                auto buffer = dynamic_cast<GpuBufferMetal*>(resource);
                buffer->id_buf.label = [NSString stringWithUTF8String:name];
            }
            else if( resource->ty == GpuResource::Type::Texture)
            {
                auto texture = dynamic_cast<GpuTextureMetal*>(resource);
                texture->id_tex.label = [NSString stringWithUTF8String:name];
            }else if( resource->ty == GpuResource::Type::RayTracingAcceleration){
                // auto acceleration = dynamic_cast<GpuRayTracingAccelerationMetal*>(resource);
                // acceleration->id_acceleration.label = [NSString stringWithUTF8String:name];
            }
        }
        auto GpuDeviceMetal::destroy_resource(GpuResource* resource) -> void
        {

        }
        auto GpuDeviceMetal::defer_release(std::function<void()> f) -> void
        {

        }
        auto GpuDeviceMetal::export_image(rhi::GpuTexture* image) -> std::vector<u8>
        {
            std::vector<u8> data;
            auto metal_image = dynamic_cast<GpuTextureMetal*>(image);
            auto width = image->desc.extent[0];
            auto height = image->desc.extent[1];
            uint32 per_pixel_bytes = 1;
            switch (image->desc.format)
            {
            case PixelFormat::R8G8B8A8_UNorm:
            case PixelFormat::R8G8B8A8_UNorm_sRGB:
                per_pixel_bytes = 4;
                break;
            case PixelFormat::R32G32B32A32_Float:
                per_pixel_bytes = 4 * 4;
                break;
            case PixelFormat::R16G16B16A16_Float:
                per_pixel_bytes = 8 * 4;
                break;
            }
            auto image_bytes = width * height * per_pixel_bytes;
            data.resize(image_bytes);
            [metal_image->id_tex getBytes:data.data() bytesPerRow:width * per_pixel_bytes fromRegion:MTLRegionMake2D(0, 0, width, height) mipmapLevel:0];
            return data;
        }
        auto GpuDeviceMetal::blit_image(rhi::GpuTexture* src, rhi::GpuTexture* dst, CommandBuffer* cmd_buf) -> void
        {
            auto metal_cmd = dynamic_cast<GpuCommandBufferMetal*>(cmd_buf);
            auto src_metal = dynamic_cast<GpuTextureMetal*>(src);
            auto dst_metal = dynamic_cast<GpuTextureMetal*>(dst);
            auto blitEncoder = [metal_cmd->cmd_buf blitCommandEncoder];
            [blitEncoder copyFromTexture:src_metal->id_tex toTexture:dst_metal->id_tex];
            [blitEncoder endEncoding];
        }
    
        void GpuDeviceMetal::waitIdle()
        {
            auto& frame0 = frames[0];
            auto main_cmd = dynamic_pointer_cast<GpuCommandBufferMetal>(frame0.main_cmd_buf);
            [main_cmd->cmd_buf waitUntilCompleted];
            auto present_cmd = dynamic_pointer_cast<GpuCommandBufferMetal>(frame0.presentation_cmd_buf);
            [present_cmd->cmd_buf waitUntilCompleted];
            
        }
    
    }
}
#endif