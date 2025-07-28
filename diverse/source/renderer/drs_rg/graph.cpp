
#include "graph.h"
#include "pass_builder.h"
#include "core/ds_log.h"
namespace diverse
{
    namespace rg
    {
        
        auto frame_alloctor() -> FrameAllocator&
        {
            static FrameAllocator allocator;
            return allocator;
        }

        template<typename Res>
        requires std::derived_from<Res, rhi::GpuResource>
        auto ImportExportToRenderGraph<Res>::import_res(const std::shared_ptr<Res>& resource, RenderGraph& rg, rhi::AccessType access_type) -> Handle<Res>
        {
            auto res = GraphRawResourceHandle{
                static_cast<uint32>(rg.resources.size()),
                0
            };
            auto desc = resource->desc;
            if constexpr  ( std::same_as<Res,rhi::GpuTexture> ){
                rg.resources.push_back({
                        GraphResourceInfo::Type::Imported,
                        GraphResourceImportInfo{
                            GraphResourceImportInfo::Type::Image, std::pair{
                                resource,
                                access_type
                            }
                        } 
                    }
                );
            }
            else if constexpr ( std::same_as<Res,rhi::GpuBuffer>){
                rg.resources.push_back({
                        GraphResourceInfo::Type::Imported,
                        GraphResourceImportInfo{
                            GraphResourceImportInfo::Type::Buffer,
                            std::pair{
                                resource,
                                access_type
                            }
                        }
                    }
                );
            }
            else {
                rg.resources.push_back({
                        GraphResourceInfo::Type::Imported,
                        GraphResourceImportInfo{
                            GraphResourceImportInfo::Type::RayTracingAcceleration,
                            std::pair{
                                resource,
                                access_type
                            }
                        }
                    }
                );
            }
            return Handle<Res>{
                res,
                desc
            };
        }

        template<typename Res>
        requires std::derived_from<Res, rhi::GpuResource>
        auto ImportExportToRenderGraph<Res>::export_res(const Handle<Res>& resource, RenderGraph& rg, rhi::AccessType access_type) -> ExportedHandle<Res>
        {
            auto res = ExportedHandle<Res>{
                resource.raw
            };

            if constexpr ( std::same_as<Res, rhi::GpuTexture>){
                rg.exported_resources.push_back(
                    {
                        ExportableGraphResource {
                            ExportableGraphResource::Type::Image,
                            resource
                        },
                        access_type
                    }
                );
            }
            else if constexpr (std::same_as<Res,rhi::GpuBuffer>){
                rg.exported_resources.push_back(
                    {
                        ExportableGraphResource {
                            ExportableGraphResource::Type::Buffer,
                            resource
                        },
                        access_type
                    }
                );
            }
            else {
                rg.exported_resources.push_back(
                    {
                        ExportableGraphResource {
                            ExportableGraphResource::Type::RayTracingAcceleration,
                            resource
                        },
                        access_type
                    }
                );
            }
            return res;

        }

        RenderGraph::RenderGraph(RenderGraphParams&& params)
        {
            resource_registry.execution_params = RenderGraphExecutionParams{
                params.device,
                params.pipeline_cache,
                params.frame_descriptor_set,
                params.frame_constants_layout
            };
            resource_registry.dynamic_constants = params.dynamic_constants;
            this->transient_resource_cache = params.transient_resource_cache;
        }
        auto RenderGraph::add_pass(const std::string& name) -> PassBuilder
        {
            auto pass_idx = passes.size();
            return PassBuilder(this, pass_idx, RecordedPass(name, pass_idx));
        }

        auto RenderGraph::create_raw_resource(GraphResourceCreateInfo&& info) -> GraphRawResourceHandle
        {
            auto res = GraphRawResourceHandle{
                static_cast<uint32>(resources.size()),
                0
            };

            resources.push_back({ GraphResourceInfo::Type::Created, info });
            return res;
        }


        auto RenderGraph::get_swap_chain() -> Handle<rhi::GpuTexture>
        {
            auto res = GraphRawResourceHandle{
                static_cast<u32>(resources.size()),
                0
            };

            this->resources.push_back(GraphResourceInfo::Imported(GraphResourceImportInfo::SwapchainImage()));

            return Handle<rhi::GpuTexture>{
                res,
                rhi::GpuTextureDesc::new_2d(PixelFormat::R8G8B8A8_UNorm, {1,1})
            };
        }

        auto RenderGraph::calculate_resource_info() -> ResourceInfo
        {
            std::vector<ResourceLifeTime>   lifetimes(this->resources.size());
            std::transform(resources.begin(), resources.end(), lifetimes.begin(),[](const GraphResourceInfo& res) {
                switch (res.ty)
                {
                case GraphResourceInfo::Type::Created:
                    return ResourceLifeTime{};
                case GraphResourceInfo::Type::Imported:
                    return ResourceLifeTime{0};
                default:
                    return ResourceLifeTime{};
                }
            });

            std::vector<rhi::TextureUsageFlags> image_usage_flags(resources.size());
            std::vector<rhi::BufferUsageFlags> buffer_usage_flags(resources.size());

            for (auto res_idx = 0; res_idx < resources.size(); res_idx++) {
                auto& resource = resources[res_idx];
                if (resource.ty == GraphResourceInfo::Type::Created) {
                    
                    auto desc = resource.graph_resource_create_info().desc;
                    if (desc.ty == GraphResourceDesc::Type::Image) {
                        image_usage_flags[res_idx] = desc.image_desc().usage;
                    }
                    else if(desc.ty == GraphResourceDesc::Type::Buffer){
                        buffer_usage_flags[res_idx] = desc.buffer_desc().usage;
                    }
                }
            }

            for (auto pass_idx = 0; pass_idx < passes.size(); pass_idx++) {
                auto& pass = passes[pass_idx];

                auto traverse_pass = [&](const PassResourceRef& res_access) {
                    auto resource_index = res_access.handle.id;
                    auto res = lifetimes[resource_index];
                    res.last_access = res.last_access ? std::max<uint32>(res.last_access.value(), pass_idx) : pass_idx;
                    switch (resources[resource_index].ty)
                    {
                    case GraphResourceInfo::Type::Created:
                    {
                        const auto& desc = resources[resource_index].graph_resource_create_info().desc;
                        if (desc.ty == GraphResourceDesc::Type::Image) {
                            auto image_usage = rhi::image_access_type_to_usage_flags(res_access.access.access_type);
                            image_usage_flags[res_access.handle.id] |= image_usage;
                        }
                        else if (desc.ty == GraphResourceDesc::Type::Buffer) {
                            auto buffer_usage = rhi::buffer_access_type_to_usage_flags(res_access.access.access_type);
                            buffer_usage_flags[res_access.handle.id] |= buffer_usage;
                        }
                    }break;
                    case GraphResourceInfo::Type::Imported:
                    {
                        const auto& import_info = resources[resource_index].graph_resource_import_info();
                        if (import_info.ty == GraphResourceImportInfo::Type::Image || import_info.ty == GraphResourceImportInfo::Type::SwapchainImage) {
                            auto image_usage = rhi::image_access_type_to_usage_flags(res_access.access.access_type);
                            image_usage_flags[res_access.handle.id] |= image_usage;
                        }
                        else if (import_info.ty == GraphResourceImportInfo::Type::Buffer) {
                            auto buffer_usage = rhi::buffer_access_type_to_usage_flags(res_access.access.access_type);
                            buffer_usage_flags[res_access.handle.id] |= buffer_usage;
                        }
                    }break;
                    default:
                        break;
                    }
                  
                };
                
                for (const auto& res_access : pass.read) {
                    traverse_pass(res_access);
                }
                for (const auto& res_access : pass.write) {
                    traverse_pass(res_access);
                }
            }

            for (auto& [res, access_type] : exported_resources)
            {
                auto raw_id = res.raw().id;
                lifetimes[raw_id].last_access = std::max<uint32>(passes.size() - 1,0);
                if (access_type != rhi::AccessType::Nothing) {
                   
                    if (res.ty == ExportableGraphResource::Type::Image) {
                        image_usage_flags[raw_id] |= rhi::image_access_type_to_usage_flags(access_type);
                    }
                    else{
                        buffer_usage_flags[raw_id] |= rhi::buffer_access_type_to_usage_flags(access_type);
                    }
                }
            }

            return ResourceInfo{
                lifetimes,
                image_usage_flags,
                buffer_usage_flags
            };
        }

        auto RenderGraph::compile(rhi::PipelineCache& pipeline_cache) -> void
        {
            auto resource_info = calculate_resource_info();
            
            std::vector<rhi::ComputePipelineHandle>  compute_pipeline_handles;
            std::vector<rhi::RasterPipelineHandle>  raster_pipeline_handles;
            std::vector<rhi::RtPipelineHandle>  rt_pipeline_handles;
            for (auto& pipeline : compute_pipelines)
            {
                pipelines.compute.emplace_back(pipeline_cache.register_compute(pipeline.desc));
            }

            for (auto& pipeline : raster_pipelines)
            {
                pipelines.raster.emplace_back(pipeline_cache.register_raster(pipeline.shaders, pipeline.desc));
            }

            for (auto& pipeline : rt_pipelines)
            {
                pipelines.rt.emplace_back(pipeline_cache.register_ray_tracing(pipeline.shaders,pipeline.desc));
            }

            this->resource_info = std::move(resource_info);
      /*      this->pipelines = std::move(RenderGraphPipelines{
                compute_pipeline_handles,
                raster_pipeline_handles,
                rt_pipeline_handles
            });*/
        }


        template struct ImportExportToRenderGraph<rhi::GpuTexture>;
        template struct ImportExportToRenderGraph<rhi::GpuBuffer>;
        template struct ImportExportToRenderGraph<rhi::GpuRayTracingAcceleration>;

        auto RenderGraph::release_resources(TransientResourceCache& transient_resource_cache)->void
        {
            for (auto& resouce : resource_registry.resources)
            {
                switch (resouce.resource.ty)
                {
                case AnyRenderResource::Type::OwnedImage:
                {
                    transient_resource_cache.insert_image(resouce.resource.image());
                }break;
                case AnyRenderResource::Type::OwnedBuffer:
                {
                    transient_resource_cache.insert_buffer(resouce.resource.buffer());
                }break;
                case AnyRenderResource::Type::ImportedRayTracingAcceleration:
                    break;
                case AnyRenderResource::Type::Pending:
                {
                    throw std::runtime_error(fmt::format("RetiredRenderGraph::release_resources called while a resource was in Pending state"));
                }
                break;
                default:
                    break;
                }
            }
        }

        auto RenderGraph::register_execution_params(RenderGraphExecutionParams&& params, TransientResourceCache* transient_resource_cache, rhi::DynamicConstants* dynamic_constants) -> void
        {
            resource_registry.execution_params = std::move(params);
            resource_registry.dynamic_constants = std::move(dynamic_constants);
            this->transient_resource_cache = transient_resource_cache;
        }
        
        auto RenderGraph::begin_execute() -> void
        {
            auto device = resource_registry.execution_params.device;
            std::vector<RegistryResource>   ret_resources;
            for (auto res_id = 0; res_id < resources.size(); res_id++)
            {
                auto& resource = resources[res_id];
                if (resource.ty == GraphResourceInfo::Type::Created)
                {
                    auto& create_info = resource.graph_resource_create_info();
                    const auto& desc = create_info.desc;
                    switch ( desc.ty )
                    {
                    case GraphResourceDesc::Type::Image:
                    {
                        auto img_desc = desc.image_desc();
                        img_desc.usage = resource_info.image_usage_flags[res_id];
                        auto image = transient_resource_cache->get_image(img_desc);
                        if (!image) 
                            image = device->create_texture(img_desc, {}, create_info.name.size() > 0 ? create_info.name.data() : nullptr);
                        ret_resources.push_back( {AnyRenderResource::image(image.value()), rhi::AccessType::Nothing});
                    }
                    break;
                    case GraphResourceDesc::Type::Buffer:
                    {
                        auto buf_desc = desc.buffer_desc();
                        buf_desc.usage = resource_info.buffer_usage_flags[res_id];
                        auto buf = transient_resource_cache->get_buffer(buf_desc);
                        if (!buf)
                            buf = device->create_buffer(buf_desc, create_info.name.size() > 0 ? create_info.name.data() : "rg buffer", nullptr);
                        ret_resources.push_back({ AnyRenderResource::buffer(buf.value()), rhi::AccessType::Nothing });
                    }
                    break;
                    case GraphResourceDesc::Type::RayTracingAcceleration:
                    {
                    }
                    break;
                    default:
                        break;
                    }
                }
                else if (resource.ty == GraphResourceInfo::Type::Imported)
                {
                    auto& import_info = resource.graph_resource_import_info();
                    switch (import_info.ty)
                    {
                    case GraphResourceImportInfo::Type::Image:
                    {
                        const auto [resource, access_type] = import_info.Image();
                        ret_resources.push_back({ AnyRenderResource::imported_image(resource), access_type});
                    }
                    break;
                    case GraphResourceImportInfo::Type::Buffer:
                    {
                        const auto [resource, access_type] = import_info.Buffer();
                        ret_resources.push_back({ AnyRenderResource::imported_buffer(resource), access_type});
                    }
                        break;

                    case GraphResourceImportInfo::Type::RayTracingAcceleration:
                    {
                        const auto [resource, access_type] = import_info.RayTracingAcceleration();
                        ret_resources.push_back({ AnyRenderResource::ray_tracing_acceleration(resource), access_type });
                    }
                        break;

                    case GraphResourceImportInfo::Type::SwapchainImage:
                    {
                        ret_resources.push_back({ AnyRenderResource::pending(resource), rhi::AccessType::ComputeShaderWrite });
                    }
                        break;
                    default:
                        break;
                    }
                }
            }

         /*   auto resource_registry = ResourceRegistry{
                std::move(params),
                std::move(ret_resources),
                pipelines,
                dynamic_constants
            };
            this->resource_registry = std::move(resource_registry);*/
            resource_registry.resources = std::move(ret_resources);
            resource_registry.pipelines = std::move(pipelines);
        }

        auto RenderGraph::execute()->void
        {
            auto device = resource_registry.execution_params.device;
            auto& pipeline_cache = *resource_registry.execution_params.pipeline_cache;
            compile(pipeline_cache);
            pipeline_cache.prepare_frame(device);
    
            auto cb = device->get_graphics_cmd_buffer();
            assert(cb);
            cb->begin();
            begin_execute();
            // Record and submit the main command buffer
            record_main_cb(cb);
            cb->end();
            device->execute_cmd(cb);

            release_resources(*transient_resource_cache);
        }
        
        auto RenderGraph::record_main_cb(rhi::CommandBuffer* cb)->void
        {
            auto first_presentation_pass = passes.size();

            for (auto pass_idx = 0; pass_idx < passes.size(); pass_idx++)
            {
                const auto& pass = passes[pass_idx];
                for (auto& res : pass.write)
                {
                    auto resource = resources[res.handle.id];
                    if (resource.ty == GraphResourceInfo::Type::Imported)
                    {
                         if( resource.graph_resource_import_info().ty == GraphResourceImportInfo::Type::SwapchainImage )
                            first_presentation_pass = pass_idx;
                    }
                }
            }
            // At the start, transition all resources to the access type they're first used with
            // While we don't have split barriers yet, this will remove some bubbles
            // which would otherwise occur with temporal resources.

            std::unordered_map<uint32, PassResourceAccessType*>  resource_first_access_states;
            for (auto pass_id = 0; pass_id < first_presentation_pass; pass_id++)
            {
                auto& pass = passes[pass_id];
                for (auto& res_ref : pass.read)
                {
                    auto it = resource_first_access_states.find(res_ref.handle.id);
                    if (it == resource_first_access_states.end())
                        resource_first_access_states.insert({ res_ref.handle.id, &res_ref.access });
                }
                for (auto& res_ref : pass.write)
                {
                    auto it = resource_first_access_states.find(res_ref.handle.id);
                    if (it == resource_first_access_states.end())
                        resource_first_access_states.insert({ res_ref.handle.id, &res_ref.access });
                }
            }
            auto param = resource_registry.execution_params;
            for (auto& [res_idx, access] : resource_first_access_states)
            {
                auto& resource = resource_registry.resources[res_idx];
                transition_resource(param.device, cb, resource, PassResourceAccessType{ access->access_type , PassResourceAccessSyncType::SkipSyncIfSameAccessType }, false, "");

                access->sync_type = PassResourceAccessSyncType::SkipSyncIfSameAccessType;
            }

            for (auto pass_idx = 0; pass_idx < first_presentation_pass; pass_idx++)
            {
                auto& pass = passes[pass_idx];
                record_pass_cb(pass, resource_registry, cb);
            }

            passes.erase(passes.begin(), passes.begin() + first_presentation_pass);
        }

        auto RenderGraph::record_presentation_cb(
                            rhi::CommandBuffer* cb, 
                            const std::shared_ptr<rhi::GpuTexture>& swapchain)
                            -> void
        {
            auto& params = resource_registry.execution_params;
            for (auto& [res_idx, access_type] : exported_resources)
            {
                if (access_type != rhi::AccessType::Nothing)
                {
                    auto& resource = resource_registry.resources[res_idx.raw().id];

                    transition_resource(params.device, cb, resource, PassResourceAccessType{ access_type, PassResourceAccessSyncType ::AlwaysSync}, false, "");
                }
            }

            for (auto& res : resource_registry.resources)
            {
                if (res.resource.ty == AnyRenderResource::Type::Pending)
                {
                    auto pending = res.resource.pending();
                    if (pending.ty == GraphResourceInfo::Type::Imported)
                    {
                        if (pending.graph_resource_import_info().ty == GraphResourceImportInfo::Type::SwapchainImage)
                        {
                            res.resource = AnyRenderResource::imported_image(swapchain);
                        }
                        else
                        {
                            throw std::runtime_error(fmt::format("Only swapchain can be currently pending"));
                        }
                    }
                }
            }

            for (auto& pass : passes)
            {
                record_pass_cb(pass, resource_registry, cb);
            }
        }

        auto RenderGraph::record_pass(RecordedPass&& pass) -> void
        {
            this->passes.push_back(std::move(pass));
            //TODO: add hook_debug pass
        }
        auto RenderGraph::record_pass_cb(
                            RecordedPass& pass, 
                            ResourceRegistry& resource_registry, 
                            rhi::CommandBuffer* cb)->void
        {
            auto& params = resource_registry.execution_params;

            std::vector<std::pair<u32, PassResourceAccessType>> transitions;
            for (auto& resource_ref : pass.read)
                transitions.push_back({ resource_ref.handle.id, resource_ref.access });

            for (auto& resource_ref : pass.write)
                transitions.push_back({ resource_ref.handle.id, resource_ref.access });

            for (auto& [res_idx, access] : transitions)
            {
                auto& resource = resource_registry.resources[res_idx];

                transition_resource(
                    params.device,
                    cb,
                    resource,
                    access,
                    false,
                    ""
                );
            }
            auto api = RenderPassApi{
                cb,
                resource_registry
            };
#ifndef DS_PRODUCTION
            api.device()->event_begin(pass.name.c_str(), cb);
            if (auto& render_fn = pass.render_fn)
                render_fn(api);
            api.device()->event_end(cb);
#else
            if (auto& render_fn = pass.render_fn)
                render_fn(api);
#endif
        }

        auto RenderGraph::transition_resource(
                            rhi::GpuDevice* device, 
                            rhi::CommandBuffer* cb,
                            RegistryResource& resource,
                            const PassResourceAccessType& access_type,
                            bool debug,
                            const std::string& dbg_str)->void
        {
            if (resource.access_type == access_type.access_type && access_type.sync_type == PassResourceAccessSyncType::SkipSyncIfSameAccessType)
                  return;
            if (debug)
            {
                DS_LOG_INFO("{}, {}", (int)resource.access_type, (int)access_type.access_type);
            }
            switch (resource.resource.ty)
            {
            case AnyRenderResource::Type::OwnedImage:
            case AnyRenderResource::Type::ImportedImage:
            {
                auto image = resource.resource.image().get();
                auto aspect = rhi::image_aspect_mask_from_access_type_and_format(access_type.access_type, image->desc.format);
                if (!aspect)
                {
                    DS_LOG_ERROR("Invalid image access, {}, {}", (int)access_type.access_type, (int)access_type.sync_type);
                    return;
                }
                device->record_image_barrier(cb, rhi::ImageBarrier{
                        image,
                        resource.access_type,
                        access_type.access_type,
                        aspect.value()
                    });
                resource.access_type = access_type.access_type;
            }break;
            case AnyRenderResource::Type::OwnedBuffer:
            case AnyRenderResource::Type::ImportedBuffer:
            {
                auto buffer = resource.resource.buffer().get();
                device->record_buffer_barrier(cb, rhi::BufferBarrier{
                        buffer,
                        resource.access_type,
                        access_type.access_type,
                        0,
                        (u32)buffer->desc.size
                    });
                resource.access_type = access_type.access_type;
            }break;
            case AnyRenderResource::Type::ImportedRayTracingAcceleration:
            {
                if(debug)
                    DS_LOG_INFO("\t(bvh)");
                resource.access_type = access_type.access_type;
            }break;
            default:
                break;
            }
        }

        auto global_barrier(
            rhi::GpuDevice* device, 
            rhi::CommandBuffer* cb, 
            const std::vector<rhi::AccessType>& previous_accesses, 
            const std::vector<rhi::AccessType>& next_accesses)->void
        {
            device->record_global_barrier(cb, previous_accesses, next_accesses);
        }

}
}
