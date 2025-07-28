#pragma once
#include "platform/unix/unix_os.h"

namespace diverse 
{
    class MacOSOS : public UnixOS
    {
    public:
        MacOSOS()
        {
        }
        ~MacOSOS()
        {
        }

        void init();
        void run() override;
        std::string getExecutablePath() override;
        void setTitleBarColour(const glm::vec4& colour, bool dark = true) override;
        void delay(uint32_t usec) override;
        void maximiseWindow() override;
    };
}
