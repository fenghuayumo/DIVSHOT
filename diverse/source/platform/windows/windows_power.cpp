
#include "windows_power.h"
#include <windows.h>

namespace diverse
{
    WindowsPower::WindowsPower()
        : m_NumberSecondsLeft(-1)
        , m_PercentageLeft(-1)
        , m_PowerState(POWERSTATE_UNKNOWN)
    {
    }

    WindowsPower::~WindowsPower()
    {
    }

    bool WindowsPower::get_power_info_windows()
    {
        SYSTEM_POWER_STATUS status;
        bool needDetails = FALSE;

        /* This API should exist back to Win95. */
        if(!GetSystemPowerStatus(&status))
        {
            /* !!! FIXME: push GetLastError() into GetError() */
            m_PowerState = POWERSTATE_UNKNOWN;
        }
        else if(status.BatteryFlag == 0xFF)
        { /* unknown state */
            m_PowerState = POWERSTATE_UNKNOWN;
        }
        else if(status.BatteryFlag & (1 << 7))
        { /* no battery */
            m_PowerState = POWERSTATE_NO_BATTERY;
        }
        else if(status.BatteryFlag & (1 << 3))
        { /* charging */
            m_PowerState = POWERSTATE_CHARGING;
            needDetails  = TRUE;
        }
        else if(status.ACLineStatus == 1)
        {
            m_PowerState = POWERSTATE_CHARGED; /* on AC, not charging. */
            needDetails  = TRUE;
        }
        else
        {
            m_PowerState = POWERSTATE_ON_BATTERY; /* not on AC. */
            needDetails  = TRUE;
        }

        m_PercentageLeft    = -1;
        m_NumberSecondsLeft = -1;
        if(needDetails)
        {
            const int pct  = (int)status.BatteryLifePercent;
            const int secs = (int)status.BatteryLifeTime;

            if(pct != 255)
            {                                               /* 255 == unknown */
                m_PercentageLeft = (pct > 100) ? 100 : pct; /* clamp between 0%, 100% */
            }
            if(secs != (int)0xFFFFFFFF)
            { /* ((DWORD)-1) == unknown */
                m_NumberSecondsLeft = secs;
            }
        }

        return TRUE; /* always the definitive answer on Windows. */
    }

    bool WindowsPower::update_power_info()
    {
        if(get_power_info_windows())
        {
            return true;
        }
        return false;
    }

    PowerState WindowsPower::ge_power_state()
    {
        if(update_power_info())
        {
            return m_PowerState;
        }
        else
        {
            return POWERSTATE_UNKNOWN;
        }
    }

    int WindowsPower::get_power_seconds_left()
    {
        if(update_power_info())
        {
            return m_NumberSecondsLeft;
        }
        else
        {
            return -1;
        }
    }

    int WindowsPower::get_power_percentage_left()
    {
        if(update_power_info())
        {
            return m_PercentageLeft;
        }
        else
        {
            return -1;
        }
    }
}
