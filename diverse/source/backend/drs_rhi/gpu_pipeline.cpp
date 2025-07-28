#include "gpu_pipeline.h"
#include <assert.h>

namespace diverse
{
    namespace rhi
    {
        auto merge_shader_stage_layout_pair(const StageDescriptorSetLayouts& src, StageDescriptorSetLayouts& dst) -> void
        {
            for (auto [set_idx, set] : src)
            {
                //auto it = std::find_if(dst.begin(), dst.end(), [&](const std::pair<u32, DescriptorSetLayout>& p) {return p.first == s_pair.first; });
                auto it = dst.find(set_idx);
                if (it != dst.end())
                {
                    auto& existing = dst[set_idx];
                    for (auto [binding_idx, binding] : set)
                    {
                        auto dit = existing.find(binding_idx);
                        if (dit != existing.end())
                        {
                            auto descriptor = binding;
                            assert(descriptor.name == dit->second.name);
                            assert(descriptor.ty == dit->second.ty);
                        }
                        else
                        {
                            existing[binding_idx] = binding;
                        }
                    }
                }
                else
                {
                    dst[set_idx] = set;
                }
            }
        }

        auto merge_shader_stage_layouts(const std::vector<StageDescriptorSetLayouts>& stages) -> StageDescriptorSetLayouts
        {
            auto result = stages[0];
            for (auto stage : stages)
            {
                merge_shader_stage_layout_pair(stage, result);
            }
            return result;
        }
    }
}