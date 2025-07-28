#pragma once
#include <engine/os.h>

namespace diverse
{
    class UnixOS : public OS
    {
    public:
        UnixOS()  = default;
        ~UnixOS() = default;

        void init();
        void run() override;
        void delay(uint32_t usec) override;

        void openFileLocation(const std::filesystem::path& path) override;
        void openFileExternal(const std::filesystem::path& path) override;
        void openURL(const std::string& url) override;

        std::string getExecutablePath() override
        {
            return "";
        }
    };
}
