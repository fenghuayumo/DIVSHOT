#include <ranges>
#include <algorithm>

#include "gpu_pipeline_vulkan.h"
#include "gpu_device_vulkan.h"
#include "spirv/spirv_reflect.h"
#include "core/ds_log.h"
namespace diverse
{
    namespace rhi
    {
        auto create_descriptor_set_layouts(const GpuDeviceVulkan& device, 
                StageDescriptorSetLayouts&& descriptor_sets, VkShaderStageFlags stage_flags, 
                std::array<std::optional<std::pair<uint32, DescriptorSetLayoutOpts>>, MAX_DESCRIPTOR_SETS> set_opts) -> 
                std::pair<std::vector<VkDescriptorSetLayout>, std::vector<std::unordered_map<uint32, VkDescriptorType>>>
        {
            constexpr u32 MAX_SET_COUNT = 4;
            //get descriptoirset count: the max set index plus one
            uint32 max_set_id = 0;
            for (auto des_set : descriptor_sets)
                max_set_id = des_set.first > max_set_id ? des_set.first : max_set_id;

            auto set_count = max_set_id + 1;
            for (auto value : set_opts)
            {
                if (value)
                {
                    auto& [set_id, xx] = *value;
                    set_count = std::max(set_count, set_id + 1);
                }
            }
      
            std::vector<VkDescriptorSetLayout> set_layouts;
            set_layouts.reserve(set_count);
            std::vector<std::unordered_map<uint32, VkDescriptorType>> set_layout_info;
            set_layout_info.reserve(set_count);

            std::vector<VkSampler>	samplers;samplers.reserve(4);
            for(auto set_index = 0; set_index < set_count; set_index++)
            {
                auto stage_flag = 0 == set_index ? stage_flags : VkShaderStageFlagBits::VK_SHADER_STAGE_ALL;
                // Find the descriptor set opts corresponding to the set index, and remove them from the opts list

                DescriptorSetLayoutOpts	_set_opts_default;
                auto& resolved_set_opts = _set_opts_default;
                for (auto& maybe_opt : set_opts)
                {
                    if (maybe_opt.has_value())
                    {
                        auto& opt = *maybe_opt;
                        if (opt.first == set_index)
                            resolved_set_opts = opt.second;
                    }
                }

                auto& setopts = resolved_set_opts;
                const auto set = setopts.replace.has_value() ? setopts.replace.value() : descriptor_sets[set_index];
                if( set.size() > 0)
                {
                    std::vector<VkDescriptorSetLayoutBinding>	bindings;bindings.reserve(set.size());
                    std::vector<VkDescriptorBindingFlags>	binding_flags(set.size(), VkDescriptorBindingFlagBits::VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT);
                    rhi::DescriptorSetLayoutCreateFlags set_layout_create_flags = {};
                    for (const auto& [binding_index, binding] : set ) 
                    {
                        if (binding.dimensionality.ty == DescriptorDimensionality::RuntimeArray)
                        {
                            // Bindless
                            binding_flags[binding_index] = VkDescriptorBindingFlagBits::VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
                                | VkDescriptorBindingFlagBits::VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT
                                | VkDescriptorBindingFlagBits::VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
                                //| VkDescriptorBindingFlagBits::VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;

                            set_layout_create_flags |=
                                rhi::DescriptorSetLayoutCreateFlags::UPDATE_AFTER_BIND_POOL;
                        }
                        switch (binding.ty)
                        {
                        case DescriptorType::UNIFORM_BUFFER:
                        case DescriptorType::UNIFORM_BUFFER_DYNAMIC:
                        case DescriptorType::UNIFORM_TEXEL_BUFFER:
                        case DescriptorType::STORAGE_IMAGE:
                        case DescriptorType::STORAGE_BUFFER:
                        case DescriptorType::STORAGE_TEXEL_BUFFER:
                        case DescriptorType::STORAGE_BUFFER_DYNAMIC:
                        {
                            VkDescriptorSetLayoutBinding bd = {};
                            bd.binding = binding_index;
                            //bd.descriptorCount = binding.dimensionality.ty == DescriptorDimensionality::RuntimeArray ? device.max_bindless_storage_images() : 1;
                            if (binding.dimensionality.ty == DescriptorDimensionality::RuntimeArray)
                            {
                                if (binding.ty == DescriptorType::STORAGE_BUFFER )
                                    bd.descriptorCount = std::min(device.gpu_limits.maxPerStageDescriptorUpdateAfterBindStorageBuffers,device.max_bindless_storage_buffers()) / (MAX_SET_COUNT * set.size());
                                else if( binding.ty == DescriptorType::STORAGE_IMAGE)
                                    bd.descriptorCount = std::min(device.gpu_limits.maxPerStageDescriptorUpdateAfterBindStorageImages,device.max_bindless_storage_images()) / (MAX_SET_COUNT * set.size());
                                else if( binding.ty == DescriptorType::UNIFORM_BUFFER)
                                    bd.descriptorCount = std::min(device.gpu_limits.maxPerStageDescriptorUpdateAfterBindUniformBuffers,device.max_bindless_uniform_textel_buffers()) / (MAX_SET_COUNT * set.size());
                            }
                            else 
                                bd.descriptorCount = 1;
                            if (binding.ty == DescriptorType::UNIFORM_BUFFER || binding.ty == DescriptorType::UNIFORM_BUFFER_DYNAMIC)
                                bd.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
                            else if( binding.ty == DescriptorType::UNIFORM_TEXEL_BUFFER)
                                bd.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
                            else if (binding.ty == DescriptorType::STORAGE_IMAGE)
                                bd.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                            else if (binding.ty == DescriptorType::STORAGE_BUFFER)
                            {
                                auto name = std::string(binding.name);
                                if (name.ends_with("_dyn"))
                                {
                                    bd.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
                                }
                                else
                                    bd.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                            }
                            else if (binding.ty == DescriptorType::STORAGE_BUFFER_DYNAMIC)
                                bd.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
                            else if( binding.ty == DescriptorType::STORAGE_TEXEL_BUFFER)
                                bd.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
                            bd.stageFlags = stage_flag;
                            bindings.push_back(bd);
                        }
                            break;
                        case DescriptorType::SAMPLED_IMAGE:
                        {
                       
                            u32 descriptor_count = 0;
                            switch (binding.dimensionality.ty)
                            {
                            case DescriptorDimensionality::Single:
                                descriptor_count = 1;
                                break;
                            case DescriptorDimensionality::Array:
                                descriptor_count = binding.dimensionality.value;
                                break;
                            case DescriptorDimensionality::RuntimeArray:
                                descriptor_count = std::min(device.gpu_limits.maxDescriptorSetUpdateAfterBindSampledImages,device.max_bindless_sampled_images())/MAX_SET_COUNT;
                                break;
                            default:
                                break;
                            }
                            VkDescriptorSetLayoutBinding bd = {};
                            bd.binding = binding_index;
                            bd.descriptorCount = descriptor_count;
                            bd.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                            bd.stageFlags = stage_flag;

                            bindings.push_back(bd);
                        }
                        break;
                        case DescriptorType::SAMPLER:
                        {
                            std::string name_prefixe = "sampler_";
                            auto binding_name = std::string(binding.name);
                            auto pos_fix = binding_name.substr(8, binding_name.length() - 8);
                            if (!pos_fix.empty())
                            {
                                TexelFilter texel_filter;
                                SamplerMipmapMode mipmap_mode;
                                pos_fix = pos_fix.substr(1, pos_fix.length());
                                if (pos_fix[0] == 'n')
                                {
                                    texel_filter = TexelFilter::NEAREST;
                                    mipmap_mode = SamplerMipmapMode::NEAREST;
                                }
                                else if (pos_fix[0] == 'l')
                                {
                                    texel_filter = TexelFilter::LINEAR;
                                    mipmap_mode = SamplerMipmapMode::LINEAR;
                                }
                                else
                                    DS_LOG_TRACE("unrecognised sampler {}", binding_name);

                                pos_fix = pos_fix.substr(1, pos_fix.length());
                                //VkSamplerAddressMode	address_mode;
                                SamplerAddressMode address_mode;
                                if (pos_fix == "r")
                                    address_mode = SamplerAddressMode::REPEAT;
                                else if (pos_fix == "mr")
                                    address_mode = SamplerAddressMode::MIRRORED_REPEAT;
                                else if (pos_fix == "c")
                                    address_mode = SamplerAddressMode::CLAMP_TO_EDGE;
                                else if (pos_fix == "cb")
                                    address_mode = SamplerAddressMode::CLAMP_TO_BORDER;
            				    else
                                    DS_LOG_TRACE("unrecognised sampler adress {}", binding_name);


                                samplers.push_back(device.get_sampler(GpuSamplerDesc{ texel_filter, mipmap_mode, address_mode }));
                                VkDescriptorSetLayoutBinding bd = {};
                                bd.binding = binding_index;
                                bd.descriptorCount = 1; //TODO
                                bd.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
                                bd.stageFlags = stage_flag;
                                bd.pImmutableSamplers = &samplers.back();
                                bindings.push_back(bd);
                            }
                            else
                            {
                                DS_LOG_TRACE("sampler name error :{}", binding_name);
                            }
                        }
                        break;
                        case DescriptorType::ACCELERATION_STRUCTURE_KHR:
                        {
                            VkDescriptorSetLayoutBinding bd = {};
                            bd.binding = binding_index;
                            bd.descriptorCount = 1; //TODO
                            bd.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
                            bd.stageFlags = stage_flag;
                            bindings.push_back(bd);
                        }
                        break;
                        default: 
                            break;
                        }

                    }
                    VkDescriptorSetLayoutBindingFlagsCreateInfo binding_flags_create_info = {};
                    binding_flags_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
                    binding_flags_create_info.pBindingFlags = binding_flags.data();
                    binding_flags_create_info.bindingCount = binding_flags.size();
                
                    VkDescriptorSetLayoutCreateInfo descriptorLayoutCI{};
                    descriptorLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
                    descriptorLayoutCI.bindingCount = static_cast<uint32_t>(bindings.size());
                    descriptorLayoutCI.pBindings = bindings.data();
                    descriptorLayoutCI.pNext = &binding_flags_create_info;
                    descriptorLayoutCI.flags = setopts.flags.value_or(set_layout_create_flags);
                    VkDescriptorSetLayout	set_layout;
                    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device.device, &descriptorLayoutCI, VK_NULL_HANDLE, &set_layout));
                    set_layouts.push_back(set_layout);
                    std::unordered_map<uint32, VkDescriptorType>	bds;
                    for (const auto& bd : bindings)
                        bds[bd.binding] = bd.descriptorType;
                    set_layout_info.push_back(bds);
                }
                else
                {
                    VkDescriptorSetLayoutCreateInfo descriptorLayoutCI{};
                    descriptorLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
                    descriptorLayoutCI.bindingCount = 0;
                    descriptorLayoutCI.pBindings = nullptr;
                    VkDescriptorSetLayout	set_layout;
                    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device.device, &descriptorLayoutCI, VK_NULL_HANDLE, &set_layout));
                    set_layouts.push_back(set_layout);
                    std::unordered_map<uint32, VkDescriptorType>	bds;
                    set_layout_info.push_back(bds);
                }
            }
           

            return std::pair<std::vector<VkDescriptorSetLayout>, std::vector<std::unordered_map<uint32, VkDescriptorType>>>(set_layouts, set_layout_info);
        }

        auto get_descriptor_sets(const CompiledShaderCode& shader_code) -> StageDescriptorSetLayouts
        {
            SpvReflectShaderModule module;
            SpvReflectResult result = spvReflectCreateShaderModule(shader_code.codes.size(), shader_code.codes.data(), &module);
            assert(result == SPV_REFLECT_RESULT_SUCCESS);

            uint32_t binding_count = 0;
            result = spvReflectEnumerateDescriptorBindings(
                &module, &binding_count, nullptr
            );
            assert(result == SPV_REFLECT_RESULT_SUCCESS);

            std::vector<SpvReflectDescriptorBinding*> bindings(binding_count);
            result = spvReflectEnumerateDescriptorBindings(
                &module, &binding_count, bindings.data()
            );
            assert(result == SPV_REFLECT_RESULT_SUCCESS);

            //uint32_t push_count = 0;
            //result = spvReflectEnumeratePushConstantBlocks(&module, &push_count, nullptr);
            //assert(result == SPV_REFLECT_RESULT_SUCCESS);

            //std::vector<SpvReflectBlockVariable*> pushconstants(push_count);
            //result = spvReflectEnumeratePushConstantBlocks(&module, &push_count, pushconstants.data());
            //assert(result == SPV_REFLECT_RESULT_SUCCESS);

            //std::unordered_map<uint32, std::unordered_map<uint32, DescriptorInfo>> descriptor_sets;
            //std::unordered_map<uint32, DescriptorInfo> descriptor_dindings;

            //DescriptorSetLayout descriptor_dindings;
            StageDescriptorSetLayouts descriptor_sets;
            for (auto& x : bindings)
            {
                DescriptorInfo info;
                info.ty = (DescriptorType)x->descriptor_type;
                info.dimensionality.value = x->count;
                switch (x->type_description->op)
                {
                case SpvOp::SpvOpTypeRuntimeArray:
                    info.dimensionality.ty = DescriptorDimensionality::RuntimeArray;
                    break;
                case SpvOp::SpvOpTypeArray:
                    info.dimensionality.ty = DescriptorDimensionality::Array;
                    break;
                default:
                    info.dimensionality.ty = DescriptorDimensionality::Single;
                    break;
                    break;
                }
                info.name = x->name;
         /*       descriptor_dindings[x->binding] = info;
                descriptor_sets[x->set] = descriptor_dindings;*/
                auto& descriptor_dindings = descriptor_sets[x->set];
                descriptor_dindings[x->binding] = info;
            }
            spvReflectDestroyShaderModule(&module);
            return descriptor_sets;
        }
        auto get_cs_local_size_from_spirv(const CompiledShaderCode& shader_code) -> std::array<u32, 3>
        {
            SpvReflectShaderModule module;
            SpvReflectResult result = spvReflectCreateShaderModule(shader_code.codes.size(), shader_code.codes.data(), &module);
            assert(result == SPV_REFLECT_RESULT_SUCCESS);
            auto entry_points = module.entry_points;
            std::array<u32, 3> local_size = {entry_points->local_size.x, entry_points->local_size.y, entry_points->local_size.z};
            spvReflectDestroyShaderModule(&module);
            return local_size;
        }

        auto get_format_stride(VkFormat format) -> u32
        {
            switch ( format )
            {
            case  VK_FORMAT_R32_UINT: return 4;
            case  VK_FORMAT_R32_SINT: return 4;
            case  VK_FORMAT_R32_SFLOAT: return 4;
            case  VK_FORMAT_R32G32_UINT: return 8;
            case  VK_FORMAT_R32G32_SINT: return 8;
            case  VK_FORMAT_R32G32_SFLOAT: return 8;
            case  VK_FORMAT_R32G32B32_UINT: return 12;
            case  VK_FORMAT_R32G32B32_SINT: return 12;
            case  VK_FORMAT_R32G32B32_SFLOAT: return 12;
            case  VK_FORMAT_R32G32B32A32_UINT: return 16;
            case  VK_FORMAT_R32G32B32A32_SINT: return 16;
            case  VK_FORMAT_R32G32B32A32_SFLOAT: return 16;
            case  VK_FORMAT_R64_UINT: return 8;
            case  VK_FORMAT_R64_SINT: return 8;
            case  VK_FORMAT_R64_SFLOAT: return 8;
            case  VK_FORMAT_R64G64_UINT: return 16;            
            case  VK_FORMAT_R64G64_SINT: return 16;
            case  VK_FORMAT_R64G64_SFLOAT: return 16;
            case  VK_FORMAT_R64G64B64_UINT: return 24;
            case  VK_FORMAT_R64G64B64_SINT: return 24;
            case  VK_FORMAT_R64G64B64_SFLOAT: return 24;
            case  VK_FORMAT_R64G64B64A64_UINT: return 32;
            case  VK_FORMAT_R64G64B64A64_SINT: return 32;
            case  VK_FORMAT_R64G64B64A64_SFLOAT: return 32;
            default:
                break;
            }
            return 12;
        }
        auto get_vertex_input_attribute_desc(const CompiledShaderCode& shader_code,u32& vertex_stride) -> std::vector<VkVertexInputAttributeDescription>
        {
            SpvReflectShaderModule module;
            SpvReflectResult result = spvReflectCreateShaderModule(shader_code.codes.size(), shader_code.codes.data(), &module);
            assert(result == SPV_REFLECT_RESULT_SUCCESS);

            //module.input_variables[0]->location
            //module.input_variables_count;
            std::vector<VkVertexInputAttributeDescription> vi(module.input_variable_count);
            u32 offset = 0;
            for (int i = 0; i < module.input_variable_count; i++)
            {
                vi[i].location = module.input_variables[i]->location;
                vi[i].binding = 0;
                vi[i].format = (VkFormat)module.input_variables[i]->format;
                vi[i].offset = offset;
                offset += get_format_stride(vi[i].format);
            }
            vertex_stride = offset;
            return vi;
        }
        auto to_vk(PrimitiveTopType type) -> VkPrimitiveTopology
        {
            switch (type)
            {
            case diverse::rhi::PrimitiveTopType::TriangleList:
                return VkPrimitiveTopology::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            case diverse::rhi::PrimitiveTopType::LineList:
                return VkPrimitiveTopology::VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
            case diverse::rhi::PrimitiveTopType::PointList:
                return VkPrimitiveTopology::VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
            case diverse::rhi::PrimitiveTopType::TriangleStrip:
                return VkPrimitiveTopology::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
            default:
                break;
            }         
            return VkPrimitiveTopology::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        }

        auto to_vk(CullMode mode)->VkCullModeFlags
        {
            switch(mode)
            {
            case CullMode::BACK:
                return VK_CULL_MODE_BACK_BIT;
            case CullMode::FRONT:
                return VK_CULL_MODE_FRONT_BIT;
            case CullMode::FRONTANDBACK:
                return VK_CULL_MODE_FRONT_AND_BACK;
            case CullMode::NONE:
                return VK_CULL_MODE_NONE;
            }

            return VK_CULL_MODE_BACK_BIT;
        }
        auto to_vk(PolygonMode mode) -> VkPolygonMode
        {
            switch (mode)
            {
            case PolygonMode::Fill:
                return VK_POLYGON_MODE_FILL;
            case PolygonMode::WireFrame:
                return VK_POLYGON_MODE_LINE;
            case PolygonMode::Point:
                return VK_POLYGON_MODE_POINT;
            }
            return VK_POLYGON_MODE_FILL;
        }
        VulkanDescriptorSet::~VulkanDescriptorSet()
        {
            auto device = dynamic_cast<GpuDeviceVulkan*>(get_global_device())->device;
            g_device->defer_release([pool = this->descriptor_pool, device]() mutable {
                if (pool != VK_NULL_HANDLE)
                {
                    vkDestroyDescriptorPool(device, pool,nullptr);
                    pool = VK_NULL_HANDLE;
                }
             });
        }
        PipelineVulkan::PipelineVulkan(VkPipelineLayout layout,
            VkPipeline pip,
            std::vector<std::unordered_map<uint32, VkDescriptorType>>&& layout_info,
            std::vector<VkDescriptorPoolSize>&& pool_sizes,
            std::vector<VkDescriptorSetLayout>&& set_layouts,
            VkPipelineBindPoint bd_point)
            : pipeline_layout(layout),
            pipeline(pip),
            set_layout_info(layout_info),
            descriptor_pool_sizes(pool_sizes),
            descriptor_set_layouts(set_layouts),
            pipeline_bind_point(bd_point)
        {
        }
        PipelineVulkan::~PipelineVulkan()
        {
            auto device = dynamic_cast<GpuDeviceVulkan*>(get_global_device())->device;
            g_device->defer_release([pipeline = this->pipeline, pipeline_layout = this->pipeline_layout, device]() mutable{
                if( pipeline != VK_NULL_HANDLE)
                {
                    vkDestroyPipelineLayout(device,pipeline_layout, nullptr);
                    vkDestroyPipeline(device,pipeline,nullptr);
                    pipeline = VK_NULL_HANDLE;
                    pipeline_layout = VK_NULL_HANDLE;
                }
            });
        }

        ComputePipelineVulkan::ComputePipelineVulkan(VkPipelineLayout layout,
            VkPipeline pip,
            std::vector<std::unordered_map<uint32, VkDescriptorType>>&& layout_info,
            std::vector<VkDescriptorPoolSize>&& pool_sizes,
            std::vector<VkDescriptorSetLayout>&& set_layouts,
            VkPipelineBindPoint bd_point)
            : PipelineVulkan(layout, pip, std::move(layout_info), std::move(pool_sizes), std::move(set_layouts), bd_point)
        {
        }

        RasterPipelineVulkan::RasterPipelineVulkan(VkPipelineLayout layout,
            VkPipeline pip,
            std::vector<std::unordered_map<uint32, VkDescriptorType>>&& layout_info,
            std::vector<VkDescriptorPoolSize>&& pool_sizes,
            std::vector<VkDescriptorSetLayout>&& set_layouts,
            VkPipelineBindPoint bd_point)
            : PipelineVulkan(layout, pip, std::move(layout_info), std::move(pool_sizes), std::move(set_layouts), bd_point)
        {
        }

}
}
