#include "post_process_panel.h"
#include "renderer/render_settings.h"
#include <imgui/imgui_helper.h>
#include <imgui/IconsMaterialDesignIcons.h>
namespace diverse
{
    PostProcessPanel::PostProcessPanel(bool active)
        : EditorPanel(active)
    {
		m_Name = U8CStr2CStr(ICON_MDI_INFORMATION " PostProcess###PostProcess");
		m_SimpleName = "PostProcess";
    }

    void PostProcessPanel::on_imgui_render()
    {
        auto flags = ImGuiWindowFlags_NoCollapse;
        if (ImGui::Begin(m_Name.c_str(), &m_Active, flags) )
        {
            ImRect windowRect = { ImGui::GetWindowContentRegionMin(), ImGui::GetWindowContentRegionMax() };
            diverse::ImGuiHelper::PushID();
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
            ImGui::Columns(2);
            ImGui::Separator();
            
            ImGuiHelper::Property("Dynamic exposure", g_render_settings.use_dynamic_adaptation);
            ImGuiHelper::Property("EV shift", g_render_settings.ev_shift,-8.0f,12.0f,0.1f,ImGuiHelper::PropertyFlag::SliderValue);
            ImGuiHelper::Property("Adaptation speed", g_render_settings.dynamic_adaptation_speed,-4.0f,4.0f,0.1f,ImGuiHelper::PropertyFlag::SliderValue);
         /*   ImGuiHelper::Property("Luminance histogram low clip", g_render_settings.dynamic_adaptation_low_clip,0.0f,1.0f, 0.1f,ImGuiHelper::PropertyFlag::SliderValue);
            ImGuiHelper::Property("Luminance histogram high clip", g_render_settings.dynamic_adaptation_high_clip,0.0f,1.0f, 0.1f,ImGuiHelper::PropertyFlag::SliderValue);*/
            ImGuiHelper::Property("Contrast", g_render_settings.contrast,1.0f, 1.5f,0.001f,ImGuiHelper::PropertyFlag::SliderValue);

            ImGui::Columns(1);
            ImGui::Separator();
            ImGui::PopStyleVar();

            diverse::ImGuiHelper::PopID();
        }
        ImGui::End();
    }
    void PostProcessPanel::on_update(float dt)
    {

    }

    void PostProcessPanel::on_new_scene(Scene* scene)
    {

    }
}