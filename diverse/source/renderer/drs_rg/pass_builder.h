#pragma once
#include "pass.h"
#include "pass_api.h"
#include "graph.h"
namespace diverse
{
    namespace rg
    {
        struct RenderGraph;
        struct PassBuilder
        {
            RenderGraph* rg;
            uint32 pass_idx;
            RecordedPass   pass;

            PassBuilder(RenderGraph* graph, uint32 pass_id, RecordedPass&& pass);

            PassBuilder(const PassBuilder& pass_builder) = delete;
            PassBuilder(PassBuilder&& other)
            :rg(other.rg), pass_idx(other.pass_idx), pass(std::move(other.pass))
            {
            }

            template<typename Res>
            requires std::derived_from<Res, rhi::GpuResource>
            auto create(typename Res::Desc& desc) -> Handle<Res>
            {
                return rg->create(desc);
            }

            template<typename Res,typename ViewType>
                requires std::derived_from<Res, rhi::GpuResource> &&
            std::derived_from<ViewType, GpuViewType>
            auto write_impl(Handle<Res>& handle, rhi::AccessType access_type, PassResourceAccessSyncType sync_type)->Ref<Res,ViewType>
            {
                pass.write.push_back(PassResourceRef{handle.raw, PassResourceAccessType {access_type, sync_type}});

                return Ref<Res, ViewType>{
                    handle.raw,
                    handle.desc
                };
            }

            template<typename Res>
                requires std::derived_from<Res, rhi::GpuResource>
            auto write(Handle<Res>& handle, rhi::AccessType access_type)->Ref<Res,GpuUav>
            {
                switch ( access_type)
                {
                case  rhi::AccessType::CommandBufferWriteNVX:
                case  rhi::AccessType::VertexShaderWrite:
                case  rhi::AccessType::TessellationControlShaderWrite:
                case  rhi::AccessType::TessellationEvaluationShaderWrite:
                case  rhi::AccessType::GeometryShaderWrite:
                case  rhi::AccessType::FragmentShaderWrite:
                case  rhi::AccessType::ComputeShaderWrite:
                case  rhi::AccessType::AnyShaderWrite:
                case  rhi::AccessType::TransferWrite:
                case  rhi::AccessType::HostWrite:
                case  rhi::AccessType::ColorAttachmentReadWrite:
                case  rhi::AccessType::General:
                break;
                default:
                    throw std::runtime_error(fmt::format("invalid access type: {}", (uint32)access_type) );
                    break;
                }

                return write_impl<Res, GpuUav>(handle, access_type, PassResourceAccessSyncType::AlwaysSync);
            }

            template<typename Res>
                requires std::derived_from<Res, rhi::GpuResource>
            auto write_no_sync(Handle<Res>& handle, rhi::AccessType access_type) -> Ref<Res, GpuUav>
            {
                switch (access_type)
                {
                case  rhi::AccessType::CommandBufferWriteNVX:
                case  rhi::AccessType::VertexShaderWrite:
                case  rhi::AccessType::TessellationControlShaderWrite:
                case  rhi::AccessType::TessellationEvaluationShaderWrite:
                case  rhi::AccessType::GeometryShaderWrite:
                case  rhi::AccessType::FragmentShaderWrite:
                case  rhi::AccessType::ComputeShaderWrite:
                case  rhi::AccessType::AnyShaderWrite:
                case  rhi::AccessType::TransferWrite:
                case  rhi::AccessType::HostWrite:
                case  rhi::AccessType::ColorAttachmentReadWrite:
                case  rhi::AccessType::General:
                    break;
                default:
                    throw std::runtime_error(fmt::format("invalid access type: {}", (uint32)access_type));
                    break;
                }

                return  write_impl<Res, GpuUav>(handle, access_type, PassResourceAccessSyncType::SkipSyncIfSameAccessType);
            }

            template<typename Res>
                requires std::derived_from<Res, rhi::GpuResource>
            auto raster(Handle<Res>& handle, rhi::AccessType access_type) -> Ref<Res, GpuRt>
            {
                switch (access_type)
                {
                case  rhi::AccessType::ColorAttachmentWrite:
                case  rhi::AccessType::DepthStencilAttachmentWrite:
                case  rhi::AccessType::DepthAttachmentWriteStencilReadOnly:
                case  rhi::AccessType::StencilAttachmentWriteDepthReadOnly:
                    break;
                default:
                    throw std::runtime_error(fmt::format("invalid access type: {}", (uint32)access_type));
                    break;
                }
                return  write_impl<Res, GpuRt>(handle, access_type, PassResourceAccessSyncType::AlwaysSync);
            }
            template<typename Res>
                requires std::derived_from<Res, rhi::GpuResource>
            auto read(const Handle<Res>& handle, rhi::AccessType access_type) -> Ref<Res, GpuSrv> 
            {
                switch (access_type)
                {
                case    rhi::AccessType::CommandBufferReadNVX:
                case    rhi::AccessType::IndirectBuffer:
                case    rhi::AccessType::IndexBuffer:
                case    rhi::AccessType::VertexBuffer:
                case    rhi::AccessType::VertexShaderReadUniformBuffer:
                case    rhi::AccessType::VertexShaderReadSampledImageOrUniformTexelBuffer:
                case    rhi::AccessType::VertexShaderReadOther:
                case    rhi::AccessType::TessellationControlShaderReadUniformBuffer:
                case    rhi::AccessType::TessellationControlShaderReadSampledImageOrUniformTexelBuffer:
                case    rhi::AccessType::TessellationControlShaderReadOther:
                case    rhi::AccessType::TessellationEvaluationShaderReadUniformBuffer:
                case    rhi::AccessType::TessellationEvaluationShaderReadSampledImageOrUniformTexelBuffer:
                case    rhi::AccessType::TessellationEvaluationShaderReadOther:
                case    rhi::AccessType::GeometryShaderReadUniformBuffer:
                case    rhi::AccessType::GeometryShaderReadSampledImageOrUniformTexelBuffer:
                case    rhi::AccessType::GeometryShaderReadOther:
                case    rhi::AccessType::FragmentShaderReadUniformBuffer:
                case    rhi::AccessType::FragmentShaderReadSampledImageOrUniformTexelBuffer:
                case    rhi::AccessType::FragmentShaderReadColorInputAttachment:
                case    rhi::AccessType::FragmentShaderReadDepthStencilInputAttachment:
                case    rhi::AccessType::FragmentShaderReadOther:
                case    rhi::AccessType::ColorAttachmentRead:
                case    rhi::AccessType::DepthStencilAttachmentRead:
                case    rhi::AccessType::ComputeShaderReadUniformBuffer:
                case    rhi::AccessType::ComputeShaderReadSampledImageOrUniformTexelBuffer:
                case    rhi::AccessType::ComputeShaderReadOther:
                case    rhi::AccessType::AnyShaderReadUniformBuffer:
                case    rhi::AccessType::AnyShaderReadUniformBufferOrVertexBuffer:
                case    rhi::AccessType::AnyShaderReadSampledImageOrUniformTexelBuffer:
                case    rhi::AccessType::AnyShaderReadOther:
                case    rhi::AccessType::TransferRead:
                case    rhi::AccessType::HostRead:
                case    rhi::AccessType::Present:
                        break;
                default:
                    throw std::runtime_error(fmt::format("invalid access type: {}", (uint32)access_type));
                    break;
                }

                pass.read.push_back(PassResourceRef{handle.raw, PassResourceAccessType {access_type,PassResourceAccessSyncType::SkipSyncIfSameAccessType }});

                return  Ref<Res, GpuSrv> {
                  handle.raw,
                  handle.desc
                };
            }

            template<typename Res>
                requires std::derived_from<Res, rhi::GpuResource>
            auto raster_read(const Handle<Res>& handle, rhi::AccessType access_type) -> Ref<Res, GpuRt>
            {
                switch (access_type)
                {
                case diverse::rhi::AccessType::ColorAttachmentRead:
                case diverse::rhi::AccessType::DepthStencilAttachmentRead:
                    break;
                default:
                    throw std::runtime_error(fmt::format("invalid access type: {}", (uint32)access_type));
                    break;
                }

                pass.read.push_back(PassResourceRef{handle.raw,PassResourceAccessType{access_type, PassResourceAccessSyncType::SkipSyncIfSameAccessType} });

                return Ref<Res, GpuRt>{handle.raw, handle.desc};
            }


            auto register_compute_pipeline(const std::string& path, const std::vector<std::pair<std::string, std::string>>& defines)-> RgComputePipelineHandle;
            auto register_compute_pipeline_with_desc(rhi::ComputePipelineDesc&& desc)-> RgComputePipelineHandle;

            auto register_raster_pipeline(const std::vector<rhi::PipelineShaderDesc>& shaders, rhi::RasterPipelineDesc&& desc)-> RgRasterPipelineHandle;
            auto register_ray_tracing_pipeline(const std::vector<rhi::PipelineShaderDesc>& shaders, rhi::RayTracingPipelineDesc&& desc)-> RgRtPipelineHandle;
            auto render(std::function<void(RenderPassApi& )>&&)->void;

        };
    }
}
