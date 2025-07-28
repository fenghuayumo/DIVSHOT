#pragma once
#include "backend/drs_rhi/gpu_pipeline.h"
#include "vk_utils.h"
namespace diverse
{
    namespace rhi
    {
        struct VulkanDescriptorSet : public DescriptorSet
        {
            VulkanDescriptorSet() = default;
            VulkanDescriptorSet(VkDescriptorSet set,VkDescriptorPool pool)
                : descriptor_set(set), 
                descriptor_pool(pool)
            {}
            ~VulkanDescriptorSet();
            VkDescriptorSet	descriptor_set;
            VkDescriptorPool  descriptor_pool;
        };
        struct PipelineVulkan : public GpuPipeline
        {
            PipelineVulkan() = default;
            PipelineVulkan(
                    VkPipelineLayout layout, 
                    VkPipeline pip,
                    std::vector<std::unordered_map<uint32, VkDescriptorType>>&& layout_info,
                    std::vector<VkDescriptorPoolSize>&& pool_sizes,
                    std::vector<VkDescriptorSetLayout>&& set_layouts,
                    VkPipelineBindPoint bd_point);
            ~PipelineVulkan();
            VkPipelineLayout	pipeline_layout = VK_NULL_HANDLE;
            VkPipeline	pipeline = VK_NULL_HANDLE;
            std::vector<std::unordered_map<uint32, VkDescriptorType>>	set_layout_info;
            std::vector<VkDescriptorPoolSize>	descriptor_pool_sizes;
            std::vector<VkDescriptorSetLayout > descriptor_set_layouts;
            VkPipelineBindPoint	pipeline_bind_point;
        };

        struct ComputePipelineVulkan :  public PipelineVulkan
        {
            ComputePipelineVulkan(
                VkPipelineLayout layout,
                VkPipeline pip,
                std::vector<std::unordered_map<uint32, VkDescriptorType>>&& layout_info,
                std::vector<VkDescriptorPoolSize>&& pool_sizes,
                std::vector<VkDescriptorSetLayout>&& set_layouts,
                VkPipelineBindPoint bd_point);
            ComputePipelineVulkan() = default;
      
        };


        struct RasterPipelineVulkan : public PipelineVulkan
        {
            RasterPipelineVulkan(
                VkPipelineLayout layout,
                VkPipeline pip,
                std::vector<std::unordered_map<uint32, VkDescriptorType>>&& layout_info,
                std::vector<VkDescriptorPoolSize>&& pool_sizes,
                std::vector<VkDescriptorSetLayout>&& set_layouts,
                VkPipelineBindPoint bd_point);
        };

        auto create_descriptor_set_layouts(
            const struct GpuDeviceVulkan& device,
            StageDescriptorSetLayouts&& descriptor_sets,
            VkShaderStageFlags	stage_flags,
            std::array<std::optional<std::pair<uint32, DescriptorSetLayoutOpts>>, MAX_DESCRIPTOR_SETS>) ->std::pair<std::vector<VkDescriptorSetLayout>, std::vector<std::unordered_map<uint32, VkDescriptorType>>>;
        
        auto merge_shader_stage_layout_pair(const StageDescriptorSetLayouts&src, StageDescriptorSetLayouts& dst) ->void;
        auto merge_shader_stage_layouts(const std::vector<StageDescriptorSetLayouts>& stages)->StageDescriptorSetLayouts;

        auto get_descriptor_sets(const CompiledShaderCode& shader_code) -> StageDescriptorSetLayouts;

        auto get_cs_local_size_from_spirv(const CompiledShaderCode& shader_code)->std::array<u32, 3>;

        auto get_vertex_input_attribute_desc(const CompiledShaderCode& shader_code,u32& vertex_stride)-> std::vector<VkVertexInputAttributeDescription>;

        auto to_vk(PrimitiveTopType type)-> VkPrimitiveTopology;

        auto to_vk(CullMode mode)->VkCullModeFlags;
        auto to_vk(PolygonMode mode) -> VkPolygonMode;
    }
}