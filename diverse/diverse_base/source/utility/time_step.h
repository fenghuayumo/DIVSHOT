#pragma once
#include "core/core.h"
namespace diverse
{
    class Timer;
    class DS_EXPORT TimeStep
    {
    public:
        TimeStep();
        ~TimeStep();

        void update();
        inline double get_millis() const { return m_Timestep; }
        inline double get_elapsed_millis() const { return m_Elapsed; }

        inline double get_seconds() const { return m_Timestep * 0.001; }
        inline double get_elapsed_seconds() const { return m_Elapsed * 0.001; }

    private:
        double m_Timestep; // MilliSeconds
        double m_LastTime;
        double m_Elapsed;

        Timer* m_Timer;
    };

}
