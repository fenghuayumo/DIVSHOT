#include "helper_panel.h"
#include <core/version.h>
#include <imgui/icons_font_awesome.h>
#ifdef DS_PLATFORM_WINDOWS
#include <Windows.h>
#include <shellapi.h>
#endif

namespace diverse
{
    static const std::vector<std::pair<std::string, std::string>> g_help_strings = {
        {"h", "Toggle this help window"},
        {"Right click+drag", "Rotate the camera"},
        {"Scroll mouse/pinch", "Zoom the camera"},
        {"W", "fps camera move forward"},
        {"A", "fps camera move left"},
        {"S", "fps camera move backward"},
        {"D", "fps camera move right"},
        {"F", "focus camera"},
        {"Q, W, E, T","imguizmo key"},
        {"Ctrl+N", "create a new scene"},
        {"Ctrl+S", "save current scene"},
        {"Ctrl+O", "reload current scene"},
    };

    HelperPanel::HelperPanel(bool active)
        : EditorPanel(active)
    {
    }

    void HelperPanel::on_new_scene(Scene* scene)
    {
    }

    void HelperPanel::on_imgui_render()
    {
        if(current_tab == 0)
            draw_about_dialog();
        else
            draw_update_dialog();
    }
    extern std::string get_resource_path();

    void HelperPanel::draw_about_dialog()
    {
        auto resource_path = get_resource_path();
        if( is_active)
            ImGui::OpenPopup("About");
        // Always center this window when appearing
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowFocus();
        constexpr float icon_size = 128.f;
        glm::vec2          col_width = { icon_size + ImGuiHelper::EmSize(), 32 * ImGuiHelper::EmSize() };
        glm::vec2          display_size = ImGui::GetIO().DisplaySize;
#ifdef __EMSCRIPTEN__
        display_size = glm::vec2{ window_width(), window_height() };
#endif
        col_width[1] = glm::clamp(col_width[1], 5 * ImGuiHelper::EmSize(),
            display_size.x - ImGui::GetStyle().WindowPadding.x - 2 * ImGui::GetStyle().ItemSpacing.x -
            ImGui::GetStyle().ScrollbarSize - col_width[0]);

        ImGui::SetNextWindowContentSize(glm::vec2{ col_width[0] + col_width[1] + ImGui::GetStyle().ItemSpacing.x, 0 });

        if (ImGui::BeginPopupModal("About", &is_active,
            ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_AlwaysAutoResize))
        {

            ImGui::Spacing();

            if (ImGui::BeginTable("about_table1", 2))
            {
                ImGui::TableSetupColumn("icon", ImGuiTableColumnFlags_WidthFixed, col_width[0]);
                ImGui::TableSetupColumn("description", ImGuiTableColumnFlags_WidthFixed, col_width[1]);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                // right align the image
                {
                    auto posX = (ImGui::GetCursorPosX() + ImGui::GetColumnWidth() - icon_size -
                        2 * ImGui::GetStyle().ItemSpacing.x);
                    if (posX > ImGui::GetCursorPosX())
                        ImGui::SetCursorPosX(posX);
                }
#ifdef DS_PLATFORM_MACOS
                ImGuiHelper::ImageFromAsset((FileSystem::get_working_directory() + "/icon128.png").c_str(), {icon_size, icon_size}); // show the app icon
#else
                ImGuiHelper::ImageFromAsset((resource_path + "app_icons/icon128.png").c_str(), {icon_size, icon_size});
#endif
                ImGui::TableNextColumn();
                ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + col_width[1]);

                //ImGui::PushFont(m_bold[30]);
                ImGui::Text("Diverse Shot");
                //ImGui::PopFont();
                auto version_str = std::to_string(DiverseVersion.major) + "." + std::to_string(DiverseVersion.minor) + std::to_string(DiverseVersion.patch);
                //ImGui::PushFont(m_bold[18]);
                ImGui::Text(version_str.c_str());
                //ImGui::PopFont();
                //ImGui::PushFont(m_regular[10]);
                //ImGui::PopFont();

                ImGui::Spacing();

                //ImGui::PushFont(m_bold[16]);
                ImGui::Text("Diverse Shot is a capture app using the most advanced technology!");
                //ImGui::PopFont();

                ImGui::Spacing();

                ImGui::Text("It is developed by Diverse Team");
                ImGui::Text("About more information, please visit");
                ImGui::SameLine();
                const char* url = "https://www.diverseshot.com/";
                ImGui::TextColored(ImVec4(0.0f, 0.0f, 1.0f, 1.0f), url);
                if (ImGui::IsItemHovered())
                {
				    ImGuiHelper::Tooltip("Click to open the website");
                }
                if (ImGui::IsItemClicked()) 
                {
#ifdef DS_PLATFORM_WINDOWS
                    ShellExecuteA(0, 0, url, 0, 0, SW_SHOW);
#else
#endif
                }
                ImGui::Text("If you have any questions, please contact us using the following email address:");
                ImGui::TextColored(ImVec4(0.75f, 0.75f, 0.75f, 1.0f), "1468947017@qq.com");
                ImGui::PopTextWrapPos();
                ImGui::EndTable();
            }
            auto right_align = [](const std::string& text) {
                    auto posX = (ImGui::GetCursorPosX() + ImGui::GetColumnWidth() - ImGui::CalcTextSize(text.c_str()).x -
                        2 * ImGui::GetStyle().ItemSpacing.x);
                    if (posX > ImGui::GetCursorPosX())
                        ImGui::SetCursorPosX(posX);
                    ImGui::Text(text.c_str());
            };

            auto item_and_description = [this, right_align, col_width](std::string name, std::string desc){
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    //ImGui::PushFont(m_bold[14]);
                    right_align(name);
                    //ImGui::PopFont();

                    ImGui::TableNextColumn();
                    ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + col_width[1] - ImGuiHelper::EmSize());
                    //ImGui::PushFont(m_regular[14]);
                    ImGui::Text(desc.c_str());
                    //ImGui::PopFont();
            };

            if (ImGui::BeginTabBar("AboutTabBar"))
            {
                if (ImGui::BeginTabItem("Keybindings", nullptr))
                {
                    ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + col_width[0] + col_width[1]);
                    ImGui::Text("The following keyboard shortcuts are available (these are also described in tooltips over "
                        "their respective controls).");

                    ImGui::Spacing();
                    ImGui::PopTextWrapPos();

                    if (ImGui::BeginTable("about_table3", 2))
                    {
                        ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthFixed, col_width[0]);
                        ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthFixed, col_width[1]);

                        for (auto item : g_help_strings)
                            item_and_description(item.first, item.second);

                        ImGui::EndTable();
                    }
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }

            //ImGui::ScrollWhenDraggingOnVoid(ImVec2(0.0f, -ImGui::GetIO().MouseDelta.y), ImGuiMouseButton_Left);
            ImGui::EndPopup();
        }
    }

    void HelperPanel::draw_update_dialog()
	{
		if( is_active)
			ImGui::OpenPopup("Update");
        // Always center this window when appearing
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowFocus();
        constexpr float icon_size = 128.f;
        glm::vec2          col_width = { icon_size + ImGuiHelper::EmSize(), 32 * ImGuiHelper::EmSize() };
        glm::vec2          display_size = ImGui::GetIO().DisplaySize;
#ifdef __EMSCRIPTEN__
        display_size = glm::vec2{ window_width(), window_height() };
#endif
        col_width[1] = glm::clamp(col_width[1], 5 * ImGuiHelper::EmSize(),
            display_size.x - ImGui::GetStyle().WindowPadding.x - 2 * ImGui::GetStyle().ItemSpacing.x -
            ImGui::GetStyle().ScrollbarSize - col_width[0]);

        ImGui::SetNextWindowContentSize(glm::vec2{ col_width[0] + col_width[1] + ImGui::GetStyle().ItemSpacing.x, 0 });

        if (ImGui::BeginPopupModal("Update", &is_active,
            ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Spacing();

            if (ImGui::BeginTable("update_table1", 2))
            {
                ImGui::TableSetupColumn("icon", ImGuiTableColumnFlags_WidthFixed, col_width[0]);
                ImGui::TableSetupColumn("description", ImGuiTableColumnFlags_WidthFixed, col_width[1]);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                // right align the image
                {
                    auto posX = (ImGui::GetCursorPosX() + ImGui::GetColumnWidth() - icon_size -
                        2 * ImGui::GetStyle().ItemSpacing.x);
                    if (posX > ImGui::GetCursorPosX())
                        ImGui::SetCursorPosX(posX);
                }
                ImGuiHelper::ImageFromAsset("../resource/app_icons/icon128.png", { icon_size, icon_size }); // show the app icon

                ImGui::TableNextColumn();
                ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + col_width[1]);

                //ImGui::PushFont(m_bold[30]);
                ImGui::Text("Diverse Shot");
                //ImGui::PopFont();
                auto version_str = std::to_string(DiverseVersion.major) + "." + std::to_string(DiverseVersion.minor) + std::to_string(DiverseVersion.patch);
                //ImGui::PushFont(m_bold[18]);
                ImGui::Text(version_str.c_str());
                //ImGui::PopFont();
                //ImGui::PushFont(m_regular[10]);
                //ImGui::PopFont();

                ImGui::Spacing();

                const bool hasLatestVersion = false;
                //ImGui::PushFont(m_bold[16]);
                if (hasLatestVersion)
                {
                    const std::string newVersion = std::string("the latest version ") + std::string("1.0.0") + "can get";
                    ImGui::Text(newVersion.c_str());
                    if( ImGui::Button("Install latest version"))
					{
						const char* url = "https://www.diverseshot.com/";
#ifdef DS_PLATFORM_WINDOWS
						ShellExecuteA(0, 0, url, 0, 0, SW_SHOW);
#else
#endif
					}
                }
                else
                {
					ImGui::Text("current version is the latest version!");
                }
                //ImGui::PopFont();

                ImGui::Spacing();

                
                ImGui::PopTextWrapPos();
                ImGui::EndTable();
            }
            ImGui::EndPopup();
        }

    }
}
