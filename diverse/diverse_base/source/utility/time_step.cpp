#include "time_step.h"
#include "timer.h"
#include "core/profiler.h"
namespace diverse
{
    TimeStep::TimeStep()
        : m_Timestep(0.0)
        , m_LastTime(0.0)
        , m_Elapsed(0.0)
    {
        m_Timer = new Timer();
    }

    TimeStep::~TimeStep()
    {
        delete m_Timer;
    }

    void TimeStep::update()
    {
        double currentTime  = m_Timer->GetElapsedMSD();
        double maxFrameTime = -1.0;
        double dt           = currentTime - m_LastTime;

        {
            DS_PROFILE_SCOPE("Sleep TimeStep to target fps");
            while(dt < maxFrameTime)
            {
                currentTime = m_Timer->GetElapsedMSD();
                dt          = currentTime - m_LastTime;
            }
        }

        m_Timestep = currentTime - m_LastTime;
        m_LastTime = currentTime;
        m_Elapsed += m_Timestep;
    }
}
