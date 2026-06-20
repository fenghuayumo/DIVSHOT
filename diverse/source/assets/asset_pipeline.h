#pragma once

#include "asset_id.h"
#include "asset_metadata.h"
#include <array>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <unordered_map>

namespace diverse
{
    enum class PipelineStage : uint8_t
    {
        IO = 0,
        Decode,
        CpuOptimize,
        Upload,
        Resident
    };

    enum class PipelineStageResult : uint8_t
    {
        Failed = 0,
        Pending,
        Completed
    };

    struct PipelineTask
    {
        AssetId asset_id;
        PipelineStage stage = PipelineStage::IO;
        uint64_t enqueue_frame = 0;
    };

    // Staged asset loading: IO -> Decode -> CPU Optimize -> Upload -> Resident
    class AssetPipeline
    {
    public:
        using StageHandler = std::function<PipelineStageResult(const AssetId&, PipelineStage& next_stage)>;

        static AssetPipeline& get_instance();

        void set_stage_handler(PipelineStage stage, StageHandler handler);
        void submit(const AssetId& id, PipelineStage start = PipelineStage::IO);
        void tick(uint64_t frame_index, size_t max_tasks = 8);

        size_t pending_count() const;
        PipelineStage get_stage(const AssetId& id) const;

    private:
        AssetPipeline() = default;

        mutable std::mutex mutex;
        std::deque<PipelineTask> queue;
        std::unordered_map<AssetId, PipelineStage> active_stages;
        std::array<StageHandler, 5> handlers{};
    };

} // namespace diverse
