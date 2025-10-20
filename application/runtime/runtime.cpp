#include <diverse.h>
#include <core/entry_point.h>
#include <core/os/window.h>
#include <scene/scene_manager.h>
#include <imGui/imgui_manager.h>
#include <imgui/imgui.h>
#include <scene/camera/camera.h>
#include <imgui/imgui_helper.h>
#include <imgui/IconsMaterialDesignIcons.h>
using namespace diverse;

class Runtime : public Application
{
    friend class Application;

public:
    explicit Runtime()
        : Application()
    {
    }

    ~Runtime()
    {
    }

    void handle_event(Event& e) override
    {
        Application::handle_event(e);

        if (Input::get().get_key_pressed(diverse::InputCode::Key::Escape))
            Application::set_app_state(AppState::Closing);
    }

    void init() override
    {
        Application::init();

        Application::get().get_window()->set_window_title("Runtime");
        Application::get().get_window()->set_event_callback(BIND_EVENT_FN(Runtime::handle_event));

        add_default_scene();
        m_SceneManager->apply_scene_switch();
        
        set_override_camera(&camera, &cameraTransform);
    }

    void imgui_render() override
    {

        ImGuiHelper::ScopedStyle windowPadding(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

        auto flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
        static bool active = true;
        if (!ImGui::Begin("view", &active, flags))
        {
            ImGui::End();
            return;
        }
        auto sceneViewSize = ImGui::GetWindowContentRegionMax() - ImGui::GetWindowContentRegionMin(); // - offset * 0.5f;
        //ImGuiHelper::Image(game_view_tex, glm::vec2(sceneViewSize.x, sceneViewSize.y), false);
        ImGuiHelper::ImageFromAsset("../resource/app_icons/icon128.png", sceneViewSize); // show the app icon
        //ImGuizmo::SetDrawlist();

        auto size = ImGui::CalcTextSize(U8CStr2CStr(ICON_MDI_REDO)) + ImVec2(10,10);
        auto offsetX = (sceneViewSize.x - size.x * 2) / 2;
        auto offsetY = sceneViewSize.y - size.y - 20; 
        ImGui::PushClipRect(ImVec2(offsetX - 10, offsetY),ImVec2(offsetX + size.x * 2 + 10, offsetY + size.y + 20),true);
        ImGui::SetCursorPos(ImVec2(offsetX, offsetY));
        if (ImGui::Button(U8CStr2CStr(ICON_MDI_ARROW_LEFT)))
        {

        }
        ImGui::SameLine();
        if (ImGui::Button(U8CStr2CStr(ICON_MDI_ARROW_RIGHT)))
        {

        }
       /* ImGui::SetCursorPos(ImVec2(10,30.0f));
        if (ImGui::Button(U8CStr2CStr(ICON_MDI_UNDO)))
        {

        }
        ImGui::SetCursorPosX(10);
        if (ImGui::Button(U8CStr2CStr(ICON_MDI_REDO)))
        {

        }
        ImGui::Separator();
        ImGui::SetCursorPosX(10);
        if (ImGui::Button(U8CStr2CStr(ICON_MDI_SELECT)))
        {

        }
        ImGui::SetCursorPosX(10);
        if (ImGui::Button(U8CStr2CStr(ICON_MDI_BRUSH)))
        {

        }*/


        ImGui::PopClipRect();
        ImGui::End();
        Application::imgui_render();
    }
    
    Camera camera;
    
    maths::Transform cameraTransform;
    
};

diverse::Application* diverse::createApplication()
{
    return new ::Runtime();
}
