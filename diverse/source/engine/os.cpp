#include "core/core.h"
#include "os.h"

#if defined (DS_PLATFORM_WINDOWS)
#include "platform/windows/windows_os.h"
#elif defined (DS_PLATFORM_MACOS)
#include "platform/macos/mac_os.h"
#elif defined (DS_PLATFORM_IOS)
#include "platform/ios/ios_os.h"
#else
#include "platform/unix/unix_os.h"
#endif

namespace diverse
{
    OS* OS::s_Instance = nullptr;

    void OS::create()
    {
        DS_ASSERT(!s_Instance, "OS already exists!");

#if defined (DS_PLATFORM_WINDOWS)
        s_Instance = new WindowsOS();
#elif defined (DS_PLATFORM_MACOS)
        s_Instance = new MacOSOS();
#elif defined (DS_PLATFORM_IOS)
        s_Instance = new iOSOS();
#else
        s_Instance = new UnixOS();
#endif
    }

    void OS::release()
    {
        delete s_Instance;
        s_Instance = nullptr;
    }

    std::string OS::powerStateToString(PowerState state)
    {
        switch(state)
        {
        case POWERSTATE_UNKNOWN:
            return std::string("Unknown");
            break;
        case POWERSTATE_NO_BATTERY:
            return std::string("No Battery");
            break;
        case POWERSTATE_CHARGED:
            return std::string("Charged");
            break;
        case POWERSTATE_CHARGING:
            return std::string("Charging");
            break;
        case POWERSTATE_ON_BATTERY:
            return std::string("On Battery");
            break;
        default:
            return std::string("Error");
            break;
        }
    }
}
