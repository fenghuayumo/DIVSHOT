#pragma once
#include "editor_panel.h"
#include <functional>

#if __has_include(<filesystem>)
#include <filesystem>
#elif __has_include(<experimental/filesystem>)
#include <experimental/filesystem>
#endif

namespace ImGui
{
    class FileBrowser;
}

namespace diverse
{
    class FileBrowserPanel : public EditorPanel
    {
    public:
        FileBrowserPanel(bool active = false);
        ~FileBrowserPanel();

        void open();
        void on_imgui_render() override;
        void set_current_path(const std::string& path);
        void set_open_directory(bool value);
        void set_callback(const std::function<void(const std::string&)>& callback)
        {
            this->callback = callback;
        }

        bool is_open();
        void set_file_type_filters(const std::vector<const char*>& fileFilters);
        void clear_file_type_filters();
        std::filesystem::path& get_path();

    private:
        std::function<void(const std::string&)> callback;
        ImGui::FileBrowser* file_browser;
    };
}
