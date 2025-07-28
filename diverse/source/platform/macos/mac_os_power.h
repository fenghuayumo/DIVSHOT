#pragma once
#include <engine/os.h>
#include <CoreFoundation/CoreFoundation.h>

namespace diverse 
{
    class MacOSPower
    {
    public:
        MacOSPower();
        virtual ~MacOSPower();

        PowerState GetPowerState();
        int GetPowerSecondsLeft();
        int GetPowerPercentageLeft();

    private:
        int m_NumberSecondsLeft;
        int m_PercentageLeft;
        PowerState m_PowerState;

        void CheckPS(CFDictionaryRef dict, bool* have_ac, bool* have_battery, bool* charging);
        bool GetPowerInfo_MacOSX(/*PowerState * state, int *seconds, int *percent*/);
        bool UpdatePowerInfo();
    };
}
