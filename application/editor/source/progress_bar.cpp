#include "progress_bar.h"
#include <imgui/imgui_helper.h>
#include <imgui/IconsMaterialDesignIcons.h>

namespace diverse
{
    ProgressBarPanel::ProgressBarPanel()
    {
        name = U8CStr2CStr(ICON_MDI_INFORMATION " Progess###Progess");
        simple_name = "Progess";
    }

    void ProgressBarPanel::on_imgui_render()
    {
        ImGui::Begin("Progress Indicators");

        const ImU32 col = ImGui::GetColorU32(ImGuiCol_ButtonHovered);
        const ImU32 bg = ImGui::GetColorU32(ImGuiCol_Button);

        ImGuiHelper::Spinner("##spinner", 15, 6, col);
        ImGuiHelper::BufferingBar("##buffer_bar", 0.7f, ImVec2(400, 6), bg, col);
        
        ImGui::End();
    }

    void ProgressBarPanel::on_new_scene(Scene* scene)
    {

    }
}