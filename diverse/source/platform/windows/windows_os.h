
#include "engine/os.h"

namespace diverse
{
    class DS_EXPORT WindowsOS : public OS
    {
    public:
        WindowsOS() = default;
        ~WindowsOS() = default;

        void init();
        void run() override;
        std::string getExecutablePath() override;

        void openFileLocation(const std::filesystem::path& path) override;
        void openFileExternal(const std::filesystem::path& path) override;
        void openURL(const std::string& url) override;

        void setTitleBarColour(const glm::vec4& colour, bool dark = true) override;
    };
}