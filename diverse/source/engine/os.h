#pragma once
#include "core/core.h"

#include <string>
#include <glm/vec4.hpp>

#if __has_include(<filesystem>)
#include <filesystem>
#elif __has_include(<experimental/filesystem>)
#include <experimental/filesystem>
#endif

namespace diverse
{
    enum PowerState
    {
        POWERSTATE_UNKNOWN,
        POWERSTATE_ON_BATTERY,
        POWERSTATE_NO_BATTERY,
        POWERSTATE_CHARGING,
        POWERSTATE_CHARGED
    };

    class DS_EXPORT OS
    {
    public:
        OS()          = default;
        virtual ~OS() = default;

        virtual void run() = 0;

        static void create();
        static void release();
        static void setInstance(OS* instance)
        {
            s_Instance = instance;
        }

        static OS* instance()
        {
            return s_Instance;
        }
        static std::string powerStateToString(PowerState state);

        virtual std::string getCurrentWorkingDirectory() { return std::string(""); };
        virtual std::string getExecutablePath() = 0;
        virtual std::string get_shader_asset_path()
        {
            return "";
        };
        virtual void vibrate() const {};
        virtual void setTitleBarColour(const glm::vec4& colour, bool dark = true) {};

        // Mobile only
        virtual void showKeyboard() {};
        virtual void hideKeyboard() {};
        virtual void delay(uint32_t usec) {};

        // Needed for MaxOS
        virtual void maximiseWindow() { }

        virtual void openFileLocation(const std::filesystem::path& path) { }
        virtual void openFileExternal(const std::filesystem::path& path) { }
        virtual void openURL(const std::string& url) { }

    protected:
        static OS* s_Instance;
    };
}
