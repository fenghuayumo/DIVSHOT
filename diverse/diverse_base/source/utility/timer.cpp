#include "core/core.h"
#include "timer.h"

namespace diverse
{

    float Timer::GetTimedMS()
    {
        float time = Duration(m_LastTime, Now(), 1000.0f);
        m_LastTime = Now();
        return time;
    }

}
