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
        m_Name = "FileBrowserWindow";
        m_SimpleName = "FileBrowser";

        m_FileBrowser = new ImGui::FileBrowser(ImGuiFileBrowserFlags_CreateNewDir | ImGuiFileBrowserFlags_EnterNewFilename | ImGuiFileBrowserFlags_HideHiddenFiles);

        m_FileBrowser->SetTitle("File Browser");
        // m_FileBrowser->SetFileFilters({ ".sh" , ".h" });
        m_FileBrowser->SetLabels(U8CStr2CStr(ICON_MDI_FOLDER), U8CStr2CStr(ICON_MDI_FILE), U8CStr2CStr(ICON_MDI_FOLDER_OPEN));
        m_FileBrowser->Refresh();
    }

    FileBrowserPanel::~FileBrowserPanel()
    {
        delete m_FileBrowser;
    }

    void FileBrowserPanel::on_imgui_render()
    {
        m_FileBrowser->Display();

        if (m_FileBrowser->HasSelected())
        {
            std::string tempFilePath = m_FileBrowser->GetSelected().string();

            std::string filePath = diverse::stringutility::back_slashes_2_slashes(tempFilePath);

            m_Callback(filePath);

            m_FileBrowser->ClearSelected();
        }
    }

    bool FileBrowserPanel::is_open()
    {
        return m_FileBrowser->IsOpened();
    }

    void FileBrowserPanel::set_current_path(const std::string& path)
    {
        m_FileBrowser->SetPwd(path);
    }

    void FileBrowserPanel::open()
    {
        m_FileBrowser->Open();
    }

    void FileBrowserPanel::set_open_directory(bool value)
    {
        auto flags = m_FileBrowser->GetFlags();

        if (value)
        {
            flags |= ImGuiFileBrowserFlags_SelectDirectory;
        }
        else
        {
            flags &= ~(ImGuiFileBrowserFlags_SelectDirectory);
        }
        m_FileBrowser->SetFlags(flags);
    }

    void FileBrowserPanel::set_file_type_filters(const std::vector<const char*>& fileFilters)
    {
        m_FileBrowser->SetFileFilters(fileFilters);
    }

    void FileBrowserPanel::clear_file_type_filters()
    {
        m_FileBrowser->ClearFilters();
    }

    std::filesystem::path& FileBrowserPanel::get_path()
    {
        return m_FileBrowser->GetPath();
    }

}
