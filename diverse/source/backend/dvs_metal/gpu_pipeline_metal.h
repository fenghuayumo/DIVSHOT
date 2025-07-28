#pragma once

#include "backend/drs_rhi/gpu_pipeline.h"
#include "dvs_metal_utils.h"
namespace diverse
{
    namespace rhi
    {
        enum class MetalShaderStageFlag
        {
            Compute,
            Vertex,
            Fragment,
            All,
        };

        struct MetalDescriptorBinding
        {
            id<MTLResource> resource;
            MTLResourceUsage usage;
            id<MTLBuffer>   argument_buffer; //bindless array
        };

        struct MetalDescriptorSet : public DescriptorSet
        {
            id<MTLBuffer> argument_buffer;
            std::unordered_map<u32,MetalDescriptorBinding>    resource_bindings;
            bool flag = false;
        };

        struct GpuPipelineMetal : public GpuPipeline
        {
            GpuPipelineMetal() = default;
            GpuPipelineMetal(const StageDescriptorSetLayouts& descSets);
            ~GpuPipelineMetal();
            StageDescriptorSetLayouts   descriptor_layouts;
            std::unordered_map<u32, MetalDescriptorSet> descriptor_sets;
        };

        struct ComputePipelineMetal :  public GpuPipelineMetal
        {
            ComputePipelineMetal(id<MTLComputePipelineState> id_pipe, const std::array<u32, 3>& group_size,const StageDescriptorSetLayouts& descSets);
            id<MTLComputePipelineState>  pipeline;
        };

        struct RasterPipelineMetal : public GpuPipelineMetal
        {
            RasterPipelineMetal(id<MTLRenderPipelineState> id_pip,const StageDescriptorSetLayouts& descSets);
            id<MTLRenderPipelineState> pipeline;
            id<MTLDepthStencilState> depth_stencil_state;
            MTLCullMode             cull_mode;
            MTLWinding              front_face;
        };
    
        auto get_metal_lib(id<MTLDevice> device,const CompiledShaderCode& shader_code, bool use_argument = true)->id<MTLLibrary>;
    
        auto cull_model_2_metal(CullMode mode)->MTLCullMode;
        auto face_oder_2_metal(FrontFaceOrder oder)->MTLWinding;
        auto create_descriptor_set_layouts(const struct GpuDeviceMetal& device,
            StageDescriptorSetLayouts&& descriptor_sets, MetalShaderStageFlag stage_flags,
                                           std::array<std::optional<std::pair<uint32, DescriptorSetLayoutOpts>>, MAX_DESCRIPTOR_SETS> set_opts) -> StageDescriptorSetLayouts;
        auto get_descriptor_set_info(NSArray<MTLArgument *> *arguments)->StageDescriptorSetLayouts;
    }
}
