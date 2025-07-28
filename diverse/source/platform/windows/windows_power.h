#pragma once

#include "engine/os.h"

namespace diverse
{
    class WindowsPower
    {
    public:
        WindowsPower();
        virtual ~WindowsPower();

        PowerState ge_power_state();
        int get_power_seconds_left();
        int get_power_percentage_left();

    private:
        int m_NumberSecondsLeft;
        int m_PercentageLeft;
        PowerState m_PowerState;

        bool get_power_info_windows(/*PowerState * state, int *seconds, int *percent*/);
        bool update_power_info();
    };
}
