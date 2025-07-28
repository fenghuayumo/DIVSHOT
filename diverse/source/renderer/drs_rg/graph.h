#pragma once
#include "backend/drs_rhi/gpu_pipeline_cache.h"
#include "pass.h"
#include "resource_registry.h"
#include "transient_resource_cache.h"
#include "core/frame_alloctor.h"
#include <deque>

namespace diverse
{
    namespace rg
    {
        struct PassBuilder;
        //struct ExecutingRenderGraph;
        //struct CompiledRenderGraph;
        //struct RetiredRenderGraph;
        template<typename Res>
        requires std::derived_from<Res, rhi::GpuResource>
        struct ImportExportToRenderGraph
        {
             static auto import_res(const std::shared_ptr<Res>& resource,struct RenderGraph& rg, rhi::AccessType access_type)->Handle<Res>;
             static auto export_res(const Handle<Res>& resource, struct RenderGraph& rg, rhi::AccessType access_type)->ExportedHandle<Res>;
        };

        auto frame_alloctor() -> FrameAllocator&;

        struct RenderGraphParams
        {
            TransientResourceCache* transient_resource_cache;
            rhi::DynamicConstants* dynamic_constants;
            rhi::GpuDevice* device;
            rhi::PipelineCache* pipeline_cache;
            rhi::DescriptorSet*   frame_descriptor_set;
            FrameConstantsLayout frame_constants_layout;

        };
        struct RenderGraph
        {
          
            std::vector<std::pair<ExportableGraphResource, rhi::AccessType>> exported_resources;
            std::vector<GraphResourceInfo>  resources;
            std::vector<RecordedPass>      passes;

            std::vector<RgComputePipeline> compute_pipelines;
            std::vector<RgRasterPipeline>  raster_pipelines;
            std::vector<RgRtPipeline>      rt_pipelines;
            std::unordered_map<uint32, PredefinedDescriptorSet> predefined_descriptor_set_layouts;

            ResourceInfo resource_info;
            RenderGraphPipelines pipelines;
            ResourceRegistry    resource_registry;
            TransientResourceCache* transient_resource_cache;
            //std::vector<Handle<rhi::GpuTexture>>    record_images;

            RenderGraph(const RenderGraph& other) = delete;
            RenderGraph(){}
            RenderGraph(RenderGraphParams&& params);
        public:

            auto add_pass(const std::string& name)->PassBuilder;

            template<typename Res>
            requires std::derived_from<Res, rhi::GpuResource>
            auto create(typename Res::Desc const& desc,const char* name = nullptr) -> Handle<Res>
            {
                auto handle = Handle<Res>{create_raw_resource(std::move(GraphResourceCreateInfo{desc, name ? name : ""})), desc};
                return handle;
            }

            auto create_raw_resource(GraphResourceCreateInfo&& info) -> GraphRawResourceHandle;
            auto calculate_resource_info() -> ResourceInfo;

            template<typename Res>
                requires std::derived_from<Res, rhi::GpuResource>
            auto import_res(const std::shared_ptr<Res> & resource, rhi::AccessType access_type_at_import_time)->Handle<Res>
            {
                return ImportExportToRenderGraph<Res>::import_res(resource,*this, access_type_at_import_time);
            }

            template<typename Res>
                requires std::derived_from<Res, rhi::GpuResource>
            auto export_res(const Handle<Res> & resource, rhi::AccessType access_type) -> ExportedHandle<Res>
            {
                return ImportExportToRenderGraph<Res>::export_res(resource,*this, access_type);
            }

            auto get_swap_chain()->Handle<rhi::GpuTexture> ;

            auto register_execution_params(RenderGraphExecutionParams&& params, TransientResourceCache* transient_resource_cache, rhi::DynamicConstants* dynamic_constants) -> void;
            auto compile(rhi::PipelineCache& pipeline_cache)->void;

            auto record_pass(RecordedPass&& pass)->void;

            auto begin_execute() -> void;
       
            auto record_main_cb(rhi::CommandBuffer* CommandBuffer) -> void;
            auto record_presentation_cb(rhi::CommandBuffer* cb, const std::shared_ptr<rhi::GpuTexture>& swapchain) -> void;
            auto record_pass_cb(RecordedPass& pass, ResourceRegistry& resource_registry, rhi::CommandBuffer* cb) -> void;
            auto transition_resource(rhi::GpuDevice* device, rhi::CommandBuffer* cb, RegistryResource& resouce, const PassResourceAccessType& access_type, bool debug, const std::string& dbg_str) -> void;

            //immediate mode
            auto execute()->void;

            template<typename Res>
               requires std::derived_from<Res, rhi::GpuResource>
            auto exported_resource(const ExportedHandle<Res>& handle) -> std::pair<std::shared_ptr<Res>&, rhi::AccessType>
            {
               auto& reg_resource = resource_registry.resources[handle.raw.id];

               //return { reg_resource.resource.borrow_resource<Res>(), reg_resource.access_type };
               if constexpr (std::same_as<Res, rhi::GpuTexture>) {
                   return { reg_resource.resource.image(), reg_resource.access_type};
               }
               else if constexpr (std::same_as<Res, rhi::GpuBuffer>) {
                   return { reg_resource.resource.buffer(), reg_resource.access_type };
               }else
                   return { reg_resource.resource.ray_tracing_acceleration(), reg_resource.access_type };
            }

            auto release_resources(TransientResourceCache& transient_resource_cache) -> void;
       };

        //struct  RetiredRenderGraph
        //{
        //    std::vector<RegistryResource>   resources;

        //    template<typename Res>
        //        requires std::derived_from<Res, rhi::GpuResource>
        //    auto exported_resource(const ExportedHandle<Res>& handle)->std::pair<std::shared_ptr<Res>&, rhi::AccessType>
        //    {
        //        auto& reg_resource = resources[handle.raw.id];
        //       
        //       return {reg_resource.resource.borrow_resource<Res>(), reg_resource.access_type};
        //    }

        //    auto release_resources(TransientResourceCache& transient_resource_cache)->void;
        //};


        //struct CompiledRenderGraph : public RenderGraph
        //{
        //  /*  CompiledRenderGraph() = default;
        //    CompiledRenderGraph();*/

        //    ResourceInfo resource_info;
        //    RenderGraphPipelines pipelines;

        //    auto begin_execute(const RenderGraphExecutionParams& params, TransientResourceCache& transient_resource_cache, rhi::DynamicConstants* dynamic_constants) -> void;
        //};

        struct PendingDebugPass
        {
            Handle<rhi::GpuTexture> image;
        };
        //struct ExecutingRenderGraph : public CompiledRenderGraph
        //{
        //    ResourceRegistry    resource_registry;
        //    //std::vector<Handle<rhi::GpuTexture>>    record_images;

        //    auto record_main_cb(rhi::CommandBuffer* CommandBuffer)->void;
        //    auto record_presentation_cb(rhi::CommandBuffer* cb,const std::shared_ptr<rhi::GpuTexture>& swapchain)-> RetiredRenderGraph;
        //    auto record_pass_cb(const RecordedPass& pass, ResourceRegistry& resource_registry, rhi::CommandBuffer* cb)->void;
        //    auto transition_resource(rhi::GpuDevice* device, rhi::CommandBuffer* cb, RegistryResource& resouce,const PassResourceAccessType& access_type,bool debug,const std::string& dbg_str)->void;
        //};

         auto global_barrier(rhi::GpuDevice* device, rhi::CommandBuffer* cb, const std::vector<rhi::AccessType>& previous_accesses, const std::vector<rhi::AccessType>& next_accesses)->void;

    }
}
