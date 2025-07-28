#pragma once

#include "core/core.h"
#include "utility/time_step.h"
#include "utility/singleton.h"

namespace diverse
{
    class DS_EXPORT Engine : public ThreadSafeSingleton<Engine>
    {
        friend class TSingleton<Engine>;

    public:
        Engine();
        ~Engine();

        float target_frame_rate() const { return m_MaxFramesPerSecond; }
        void  set_target_frame_rate(float targetFPS) { m_MaxFramesPerSecond = targetFPS; }

        static TimeStep& get_time_step() { return *Engine::get().m_TimeStep; }

        struct Stats
        {
            uint32_t UpdatesPerSecond;
            uint32_t FramesPerSecond;
            uint32_t NumRenderedObjects = 0;
            uint32_t NumShadowObjects = 0;
            uint32_t NumDrawCalls = 0;
            double FrameTime = 0.0;
            float UsedGPUMemory = 0.0f;
            float UsedRam = 0.0f;
            float TotalGPUMemory = 0.0f;
        };

        void reset_stats()
        {
            m_Stats.NumRenderedObjects = 0;
            m_Stats.NumShadowObjects = 0;
            m_Stats.FrameTime = 0.0;
            m_Stats.UsedGPUMemory = 0.0f;
            m_Stats.UsedRam = 0.0f;
            m_Stats.NumDrawCalls = 0;
            m_Stats.TotalGPUMemory = 0.0f;
        }

        Stats& statistics() { return m_Stats; }

    private:
        Stats m_Stats;
        float m_MaxFramesPerSecond;
        TimeStep* m_TimeStep;
    };
}
