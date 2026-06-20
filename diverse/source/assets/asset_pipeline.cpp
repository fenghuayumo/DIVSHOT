#include "asset_pipeline.h"
#include "asset_registry.h"
#include <vector>

namespace diverse
{
    namespace
    {
        AssetState state_for_stage(PipelineStage stage)
        {
            switch (stage)
            {
                case PipelineStage::IO:
                case PipelineStage::Decode:
                    return AssetState::LoadingCpu;
                case PipelineStage::CpuOptimize:
                    return AssetState::ReadyCpu;
                case PipelineStage::Upload:
                    return AssetState::UploadQueued;
                case PipelineStage::Resident:
                    return AssetState::ResidentGpu;
                default:
                    return AssetState::Unloaded;
            }
        }
    }

    AssetPipeline& AssetPipeline::get_instance()
    {
        static AssetPipeline instance;
        return instance;
    }

    void AssetPipeline::set_stage_handler(PipelineStage stage, StageHandler handler)
    {
        handlers[static_cast<size_t>(stage)] = std::move(handler);
    }

    void AssetPipeline::submit(const AssetId& id, PipelineStage start)
    {
        if (!id.is_valid())
            return;

        std::lock_guard lock(mutex);
        queue.push_back(PipelineTask{ id, start, 0 });
        active_stages[id] = start;
        AssetRegistry::get_instance().set_state(id, state_for_stage(start));
    }

    void AssetPipeline::tick(uint64_t frame_index, size_t max_tasks)
    {
        std::vector<PipelineTask> pending_tasks;

        for (size_t i = 0; i < max_tasks; ++i)
        {
            PipelineTask task;
            {
                std::lock_guard lock(mutex);
                if (queue.empty())
                    break;
                task = queue.front();
                queue.pop_front();
            }

            task.enqueue_frame = frame_index;
            auto& handler = handlers[static_cast<size_t>(task.stage)];
            if (!handler)
            {
                std::lock_guard lock(mutex);
                active_stages.erase(task.asset_id);
                continue;
            }

            PipelineStage next = task.stage;
            const auto result = handler(task.asset_id, next);
            if (result == PipelineStageResult::Completed)
            {
                if (next != PipelineStage::Resident || task.stage != PipelineStage::Resident)
                {
                    std::lock_guard lock(mutex);
                    if (next == PipelineStage::Resident)
                    {
                        active_stages.erase(task.asset_id);
                        AssetRegistry::get_instance().set_state(task.asset_id, AssetState::ResidentGpu);
                    }
                    else
                    {
                        active_stages[task.asset_id] = next;
                        AssetRegistry::get_instance().set_state(task.asset_id, state_for_stage(next));
                        queue.push_back(PipelineTask{ task.asset_id, next, frame_index });
                    }
                }
            }
            else if (result == PipelineStageResult::Pending)
            {
                std::lock_guard lock(mutex);
                active_stages[task.asset_id] = task.stage;
                pending_tasks.push_back(PipelineTask{ task.asset_id, task.stage, frame_index });
            }
            else
            {
                std::lock_guard lock(mutex);
                active_stages.erase(task.asset_id);
                AssetRegistry::get_instance().set_state(task.asset_id, AssetState::Failed);
            }
        }

        if (!pending_tasks.empty())
        {
            std::lock_guard lock(mutex);
            for (const auto& task : pending_tasks)
                queue.push_back(task);
        }
    }

    size_t AssetPipeline::pending_count() const
    {
        std::lock_guard lock(mutex);
        return queue.size();
    }

    PipelineStage AssetPipeline::get_stage(const AssetId& id) const
    {
        std::lock_guard lock(mutex);
        auto it = active_stages.find(id);
        return it != active_stages.end() ? it->second : PipelineStage::Resident;
    }

} // namespace diverse
