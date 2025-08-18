#include <core/string.h>
#include "file_browser_panel.h"
#include "editor.h"
#include <utility/string_utils.h>
#include <engine/file_system.h>
#include <imgui/IconsMaterialDesignIcons.h>
#include <imgui/imgui.h>
#include <imgui/Plugins/ImFileBrowser.h>

namespace diverse
{
    FileBrowserPanel::FileBrowserPanel(bool active)
        : EditorPanel(active)
    {
        name = "FileBrowserWindow";
        simple_name = "FileBrowser";

        file_browser = new ImGui::FileBrowser(ImGuiFileBrowserFlags_CreateNewDir | ImGuiFileBrowserFlags_EnterNewFilename | ImGuiFileBrowserFlags_HideHiddenFiles);

        file_browser->SetTitle("File Browser");
        // m_FileBrowser->SetFileFilters({ ".sh" , ".h" });
        file_browser->SetLabels(U8CStr2CStr(ICON_MDI_FOLDER), U8CStr2CStr(ICON_MDI_FILE), U8CStr2CStr(ICON_MDI_FOLDER_OPEN));
        file_browser->Refresh();
    }

    FileBrowserPanel::~FileBrowserPanel()
    {
        delete file_browser;
    }

    void FileBrowserPanel::on_imgui_render()
    {
        file_browser->Display();

        if (file_browser->HasSelected())
        {
            std::string tempFilePath = file_browser->GetSelected().string();

            std::string filePath = diverse::stringutility::back_slashes_2_slashes(tempFilePath);

            callback(filePath);

            file_browser->ClearSelected();
        }
    }

    bool FileBrowserPanel::is_open()
    {
        return file_browser->IsOpened();
    }

    void FileBrowserPanel::set_current_path(const std::string& path)
    {
        file_browser->SetPwd(path);
    }

    void FileBrowserPanel::open()
    {
        file_browser->Open();
    }

    void FileBrowserPanel::set_open_directory(bool value)
    {
        auto flags = file_browser->GetFlags();

        if (value)
        {
            flags |= ImGuiFileBrowserFlags_SelectDirectory;
        }
        else
        {
            flags &= ~(ImGuiFileBrowserFlags_SelectDirectory);
        }
        file_browser->SetFlags(flags);
    }

    void FileBrowserPanel::set_file_type_filters(const std::vector<const char*>& fileFilters)
    {
        file_browser->SetFileFilters(fileFilters);
    }

    void FileBrowserPanel::clear_file_type_filters()
    {
        file_browser->ClearFilters();
    }

    std::filesystem::path& FileBrowserPanel::get_path()
    {
        return file_browser->GetPath();
    }

}
