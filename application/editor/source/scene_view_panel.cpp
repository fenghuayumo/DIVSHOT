
#include "scene_view_panel.h"
#include "editor.h"
#include "pivot.h"
#include <backend/drs_rhi/gpu_texture.h>
#include "gaussian_edit.h"
#include "image_paint.h"
#include "redo_undo_system.h"
#include <scene/component/gaussian_crop.h>
#include <scene/component/gaussian_component.h>
#include <scene/component/mesh_model_component.h>
#include <scene/component/point_cloud_component.h>
#include <scene/camera/camera.h>
#include <engine/application.h>
#include <scene/scene_manager.h>
#include <scene/entity_manager.h>
#include <engine/engine.h>
#include <core/profiler.h>
#include <engine/input.h>
#include <scene/scene.h>
#include <renderer/render_settings.h>
#include <imgui/IconsMaterialDesignIcons.h>
#include <scene/camera/editor_camera.h>
#include <imgui/imgui_helper.h>
#include <imgui/imgui_manager.h>
#include <imgui/imgui_renderer.h>
#include <events/application_event.h>
#include <renderer/debug_renderer.h>
#include <imgui/imgui_internal.h>
#include <imgui/Plugins/ImGuizmo.h>
#ifdef DS_SPLAT_TRAIN
#include <gaussian_trainer_scene.hpp>
#endif
#include <opencv2/opencv.hpp>
#include <utility/cmd_variable.h>
#include <stb/image_utils.h>
#include <backend/drs_rhi/gpu_device.h>
#include "embed_resources.h"

namespace diverse
{
    std::array<const char*,6> SplatVisTypeStr = {"Splat", "Point","Depth", "Normal", "Rings","Ellipsoids"};
    std::array<const char*,6> ShadeModeVisTypeStr = {"Default", "NoTextures","DiffuseGI", "Reflections","RtxOff","Irache"};
    std::array<const char*,3> RenderModeTypeStr = {"Default", "PT","Unlit"};
    auto icon_btn_size = ImVec2(24,24);
    auto item_btn_size = icon_btn_size;

    glm::vec2 cs_to_uv(glm::vec2 cs) {
        return cs * glm::vec2(0.5, -0.5) + glm::vec2(0.5, 0.5);
    }

    bool isInScreen(glm::vec2 v)
    {
        if( v.x >= 0 && v.y >= 0 && v.x <= 1 && v.y <= 1)
            return true;
        return false;
    }

    bool project_2d(const glm::mat4& proj, glm::vec3 a,glm::vec2& p)
    {
        auto p0 = proj * glm::vec4(a, 1);
        p0.xy = cs_to_uv(p0.xy / glm::vec2(p0.w, p0.w));
        p = p0.xy;
        if (isInScreen(p0.xy))
        {
            return true;
        }
        return false;
    }

    void draw_3d_line(ImDrawList* list, const glm::mat4& proj, glm::vec3 a, glm::vec3 b, uint32_t col,ImVec2 screenViewPos,ImVec2 screenViewSize, float thickness = 1.0f)
    {
        auto p0 = proj * glm::vec4(a, 1);
        auto p1 = proj * glm::vec4(b, 1);
        p0.xy = cs_to_uv(p0.xy / glm::vec2(p0.w, p0.w));
        p1.xy = cs_to_uv(p1.xy / glm::vec2(p1.w, p1.w));
        if (isInScreen(p0.xy) && isInScreen(p1.xy))
        {
            list->AddLine(ImVec2(p0.xy) * screenViewSize + screenViewPos, ImVec2(p1.xy) * screenViewSize + screenViewPos,col, thickness);
        }
    }

    SceneViewPanel::SceneViewPanel(bool active)
        :EditorPanel(active)
    {
        name = U8CStr2CStr(ICON_MDI_GAMEPAD_VARIANT " Scene###scene");
        simple_name = "Scene";
        current_scene = nullptr;


        show_component_gizmo_map[typeid(Camera).hash_code()] = true;

        width = 0;
        height = 0;
    }
    
    void SceneViewPanel::on_imgui_render()
    {
        DS_PROFILE_FUNCTION();
        Application& app = Application::get();

        ImGuiHelper::ScopedStyle windowPadding(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

        auto flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

        if (!ImGui::Begin(name.c_str(), &is_active, flags) || !current_scene)
        {
            ImGui::End();
            return;
        }
        Camera* camera = nullptr;
        maths::Transform* transform = nullptr;
        {
            DS_PROFILE_SCOPE("Set Override Camera");
            camera = editor->get_camera();
            transform = &editor->get_editor_camera_transform();

            app.set_override_camera(camera, transform);
        }

        static ImVec2 offset = { 0.0f, 0.0f };

        {
            toolbar();
            offset = ImGui::GetCursorPos(); // Usually ImVec2(0.0f, 50.0f);
        }

        if (!camera)
        {
            ImGui::End();
            return;
        }
        ImGuizmo::SetDrawlist();
        auto sceneViewSize = ImGui::GetWindowContentRegionMax() - ImGui::GetWindowContentRegionMin() - offset * 0.5f; // - offset * 0.5f;
        auto sceneViewPosition = ImGui::GetWindowPos() + offset;

        sceneViewSize.x -= static_cast<int>(sceneViewSize.x) % 2 != 0 ? 1.0f : 0.0f;
        sceneViewSize.y -= static_cast<int>(sceneViewSize.y) % 2 != 0 ? 1.0f : 0.0f;
        sceneViewSize = ImClamp(sceneViewSize,ImVec2(2,2),ImVec2(4096,4096));
        float aspect = static_cast<float>(sceneViewSize.x) / static_cast<float>(sceneViewSize.y);

        if (!maths::Equals(aspect, camera->get_aspect_ratio()))
        {
            camera->set_aspect_ratio(aspect);
        }
        editor->scene_view_panel_position = glm::vec2(sceneViewPosition.x, sceneViewPosition.y);

        bool resized = resize(static_cast<uint32_t>(sceneViewSize.x), static_cast<uint32_t>(sceneViewSize.y));

        auto game_view_tex = editor->get_main_render_texture();
        ImGuiHelper::Image(game_view_tex, glm::vec2(sceneViewSize.x, sceneViewSize.y), false);

        auto windowSize = ImGui::GetWindowSize();
        ImVec2 minBound = sceneViewPosition;

        ImVec2 maxBound = { minBound.x + windowSize.x, minBound.y + windowSize.y };
        bool updateCamera = ImGui::IsMouseHoveringRect(minBound, maxBound); // || Input::Get().get_mouse_mode() == MouseMode::Captured;

        app.set_scene_active(ImGui::IsWindowFocused() && !ImGuizmo::IsUsing() && updateCamera);

        ImGuizmo::SetRect(sceneViewPosition.x, sceneViewPosition.y, sceneViewSize.x, sceneViewSize.y);

        editor->set_scene_view_active(updateCamera);
        editor->set_scene_view_rect(maths::Rect(sceneViewPosition, sceneViewSize));
        {
            DS_PROFILE_SCOPE("Push Clip Rect");
            ImGui::GetWindowDrawList()->PushClipRect(sceneViewPosition, { sceneViewSize.x + sceneViewPosition.x, sceneViewSize.y + sceneViewPosition.y - 2.0f });
        }
        if(editor->get_current_train_view_image())
        {
            auto imge_pos_Y = sceneViewPosition.y + sceneViewSize.y * 0.7;
            auto imge_pos_X = sceneViewPosition.x + sceneViewSize.x * 0.7;
            ImGui::SetCursorPos(ImVec2(imge_pos_X, imge_pos_Y));
            auto train_view_tex = editor->get_current_train_view_image()->gpu_texture;
            ImGuiHelper::Image(train_view_tex, glm::vec2(sceneViewSize.x, sceneViewSize.y) * 0.3, false);
        }
        draw_splat_edit_toolbox(sceneViewPosition,sceneViewSize);
        draw_3d_paint_toolbar(sceneViewPosition, sceneViewSize);
        draw_splat_edit_ui(sceneViewPosition,sceneViewSize);
        if (editor->show_grid())
        {
            if (camera->is_orthographic())
            {
                DS_PROFILE_SCOPE("2D Grid");
                editor->draw_2dgrid(ImGui::GetWindowDrawList(), { transform->get_world_position().x, transform->get_world_position().y }, sceneViewPosition, { sceneViewSize.x, sceneViewSize.y }, camera->get_scale(), 1.5f);
            }
        }
      
        editor->imguizmo(ImGui::GetWindowDrawList(), { transform->get_world_position().x, transform->get_world_position().y }, sceneViewPosition, { sceneViewSize.x, sceneViewSize.y });
        //draw splat crop
        draw_splat_focus_box(camera, transform, sceneViewPosition, sceneViewSize);
        handle_splat_crop(camera, transform, sceneViewPosition, sceneViewSize);
        //draw gaussian_edit area
        handle_splat_edit(camera, transform, sceneViewPosition, sceneViewSize);
        draw_progress(sceneViewPosition, sceneViewSize);
        handle_3d_paint(sceneViewPosition, sceneViewSize);

        auto size = item_btn_size + ImVec2(10, 10);
        const auto valid_region = (ImGui::GetMousePos().x - sceneViewPosition.x) >= size.x;
        if (ImGui::IsWindowFocused() && updateCamera && app.get_scene_active() && !ImGuizmo::IsUsing() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && valid_region)
        {
            DS_PROFILE_SCOPE("Select Object");
            auto old_pivot_transform = editor->get_pivot()->get_transform();
            glm::vec2 clickPos = ImGui::GetMousePos() - glm::vec2(sceneViewPosition.x, sceneViewPosition.y);
            glm::uvec2 view_size = { int(sceneViewSize.x), int(sceneViewSize.y) };
            maths::Ray ray = editor->get_screen_ray(int(clickPos.x), int(clickPos.y), camera, view_size.x, view_size.y);
            editor->select_object(ray);
            auto splat_ent = Entity(editor->get_current_splat_entt(), editor->get_current_scene());
            if (splat_ent.valid() && splat_ent.active() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                auto splat_model = splat_ent.get_component<GaussianComponent>().ModelRef;
                auto& transform = splat_ent.get_component<maths::Transform>();
                maths::Transform& cameraTransform = editor->get_editor_camera_transform();
                auto project = camera->get_projection_matrix();
                glm::vec3 isect_point;
                if (pick_splat(splat_model,
                    clickPos,
                    ray,
                    transform.get_world_matrix(),
                    cameraTransform,
                    project,
                    view_size.x,
                    view_size.y,
                    isect_point))
                {
                    auto& pivot_transform = editor->get_pivot()->get_transform();
                    pivot_transform = transform;
                    pivot_transform.set_local_position(isect_point);
                    pivot_transform.set_world_matrix(glm::mat4(1.0f));
                    editor->focus_camera(pivot_transform.get_world_position(), 2.0f, 2.0f);
                    GaussianEdit::get().add_place_pivot_op(old_pivot_transform, pivot_transform, editor->get_pivot());
                }
            }
        }
        if (ImGui::IsWindowFocused() && updateCamera && !ImGuizmo::IsUsing() && ImGui::IsItemHovered() && Input::get().get_mouse_mode() == MouseMode::Visible)
        {
            DS_PROFILE_SCOPE("Hover Object");

            float dpi = Application::get().get_window_dpi();
            auto clickPos = Input::get().get_mouse_position() - glm::vec2(sceneViewPosition.x, sceneViewPosition.y);

            maths::Ray ray = editor->get_screen_ray(int(clickPos.x), int(clickPos.y), camera, int(sceneViewSize.x), int(sceneViewSize.y));
            editor->select_object(ray, true);
        }
        else
        {
            editor->set_hovered_entity(entt::null);
            editor->set_hovered_train_view(-1);
        }
        if (editor->get_hovered_train_view())
        {
            ImGuiHelper::ScopedStyle scopedStyle(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(editor->get_hovered_train_view()->file_path.c_str());
            ImGui::EndTooltip();
        }
        const ImGuiPayload* payload = ImGui::GetDragDropPayload();

        if (ImGui::BeginDragDropTarget())
        {
            auto data = ImGui::AcceptDragDropPayload("AssetFile", ImGuiDragDropFlags_AcceptNoDrawDefaultRect);
            if (data)
            {
                std::string file = (char*)data->Data;
                editor->file_open_callback(file);
            }
            ImGui::EndDragDropTarget();
        }

        if (app.get_scene_manager()->get_current_scene())
            draw_gizmos(sceneViewSize.x, sceneViewSize.y, offset.x, offset.y, app.get_scene_manager()->get_current_scene());

        render_frame_image();

        ImGui::GetWindowDrawList()->PopClipRect();
        ImGui::End();
    }

    void SceneViewPanel::toolbar()
    {
        ImGui::Indent();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        bool selected = false;

        {
            selected = editor->get_imguizmo_operation() == 4;
            if (selected)
                ImGui::PushStyleColor(ImGuiCol_Text, ImGuiHelper::GetSelectedColour());
            ImGui::SameLine();
            if (ImGui::Button(U8CStr2CStr(ICON_MDI_CURSOR_DEFAULT)))
                editor->set_imguizmo_operation(4);

            if (selected)
                ImGui::PopStyleColor();
            ImGuiHelper::Tooltip("Select");
        }

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        {
            selected = editor->get_imguizmo_operation() == ImGuizmo::TRANSLATE;
            if (selected)
                ImGui::PushStyleColor(ImGuiCol_Text, ImGuiHelper::GetSelectedColour());
            ImGui::SameLine();
            if (ImGui::Button(U8CStr2CStr(ICON_MDI_ARROW_ALL)))
                editor->set_imguizmo_operation(ImGuizmo::TRANSLATE);

            if (selected)
                ImGui::PopStyleColor();
            ImGuiHelper::Tooltip("Translate");
        }

        {
            selected = editor->get_imguizmo_operation() == ImGuizmo::ROTATE;
            if (selected)
                ImGui::PushStyleColor(ImGuiCol_Text, ImGuiHelper::GetSelectedColour());

            ImGui::SameLine();
            if (ImGui::Button(U8CStr2CStr(ICON_MDI_ROTATE_ORBIT)))
                editor->set_imguizmo_operation(ImGuizmo::ROTATE);

            if (selected)
                ImGui::PopStyleColor();
            ImGuiHelper::Tooltip("Rotate");
        }

        {
            selected = editor->get_imguizmo_operation() == ImGuizmo::SCALE;
            if (selected)
                ImGui::PushStyleColor(ImGuiCol_Text, ImGuiHelper::GetSelectedColour());

            ImGui::SameLine();
            if (ImGui::Button(U8CStr2CStr(ICON_MDI_ARROW_EXPAND_ALL)))
                editor->set_imguizmo_operation(ImGuizmo::SCALE);

            if (selected)
                ImGui::PopStyleColor();
            ImGuiHelper::Tooltip("Scale");
        }

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        {
            selected = editor->get_imguizmo_operation() == ImGuizmo::UNIVERSAL;
            if (selected)
                ImGui::PushStyleColor(ImGuiCol_Text, ImGuiHelper::GetSelectedColour());

            ImGui::SameLine();
            if (ImGui::Button(U8CStr2CStr(ICON_MDI_CROP_ROTATE)))
                editor->set_imguizmo_operation(ImGuizmo::UNIVERSAL);

            if (selected)
                ImGui::PopStyleColor();
            ImGuiHelper::Tooltip("Universal");
        }

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();


        {
            selected = editor->get_imguizmo_operation() == ImGuizmo::BOUNDS;
            if (selected)
                ImGui::PushStyleColor(ImGuiCol_Text, ImGuiHelper::GetSelectedColour());

            ImGui::SameLine();
            if (ImGui::Button(U8CStr2CStr(ICON_MDI_BORDER_NONE)))
                editor->set_imguizmo_operation(ImGuizmo::BOUNDS);

            if (selected)
                ImGui::PopStyleColor();
            ImGuiHelper::Tooltip("Bounds");
        }

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        {
           selected = (editor->snap_guizmo() == true);

           if (selected)
               ImGui::PushStyleColor(ImGuiCol_Text, ImGuiHelper::GetSelectedColour());

           if (ImGui::Button(U8CStr2CStr(ICON_MDI_MAGNET)))
               editor->snap_guizmo() = !selected;

           if (selected)
               ImGui::PopStyleColor();
           ImGuiHelper::Tooltip("Snap");
        }

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();


        if (ImGui::Button(U8CStr2CStr("Gizmos " ICON_MDI_CHEVRON_DOWN)))
            ImGui::OpenPopup("GizmosPopup");
        if (ImGui::BeginPopup("GizmosPopup"))
        {
            ImGui::Separator();
#ifndef DS_PRODUCTION
            if (ImGui::Button("Refresh Shaders"))
            {
                editor->refresh_shaders();
            }
            ImGuiStyle& style = ImGui::GetStyle();
            float old_spacing = style.ItemSpacing.y;
            style.ItemSpacing.y += 2;
            ImGui::Indent();
            if (ImGui::BeginMenu(" RenderMode", true))
            {
                for(auto i=0;i< RenderModeTypeStr.size();i++)
                { 
                    ImGui::Indent();
                    if (ImGui::MenuItem(RenderModeTypeStr[i]))
                    {
                        g_render_settings.render_mode = (RenderMode)i;
                    }
                    ImGui::Unindent();
                    ImGui::Separator();
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu(" ShadeBufferVis", true))
            {
                for(auto i=0;i< ShadeModeVisTypeStr.size();i++)
                { 
                    ImGui::Indent();
                    if (ImGui::MenuItem(ShadeModeVisTypeStr[i]))
                    {
                        g_render_settings.shade_mode = (DebugShadingMode)i;
                    }
                    ImGui::Unindent();
                    ImGui::Separator();
                }
                ImGui::EndMenu();
            }
            ImGui::Unindent();
            style.ItemSpacing.y = old_spacing;
#endif
            ImGui::Checkbox("Grid", &editor->show_grid());
            ImGui::Checkbox("Selected Gizmos", &editor->show_gizmos());
            ImGui::Checkbox("View Selected", &editor->show_view_selected());

            ImGui::Separator();
            ImGui::Checkbox("Camera", &show_component_gizmo_map[typeid(Camera).hash_code()]);
            
            ImGui::Separator();
            uint32_t flags = editor->get_settings().debug_draw_flags;
            //bool showEntityNames = flags & EditorDebugFlags::EntityNames;
            //if (ImGui::Checkbox("Entity Names", &showEntityNames))
            //{
            //    if (showEntityNames)
            //        flags += EditorDebugFlags::EntityNames;
            //    else
            //        flags -= EditorDebugFlags::EntityNames;
            //}

            bool showAABB = flags & EditorDebugFlags::MeshBoundingBoxes;
            if (ImGui::Checkbox("Model AABB", &showAABB))
            {
                if (showAABB)
                    flags += EditorDebugFlags::MeshBoundingBoxes;
                else
                    flags -= EditorDebugFlags::MeshBoundingBoxes;
            }
 
            ImGui::Checkbox("WireFrame", &g_render_settings.show_wireframe);
            editor->get_settings().debug_draw_flags = flags;
            ImGui::Separator();

            ImGui::EndPopup();
        }

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        if (ImGui::Button(U8CStr2CStr("Camera " ICON_MDI_CAMERA)))
            ImGui::OpenPopup("CameraPopup");
        if (ImGui::BeginPopup("CameraPopup"))
        {
            auto& camera = *editor->get_camera();
            bool ortho = camera.is_orthographic();

            ImGui::Columns(2);
            selected = !ortho;
            if (selected)
                ImGui::PushStyleColor(ImGuiCol_Text, ImGuiHelper::GetSelectedColour());
            if (ImGui::Button(U8CStr2CStr(ICON_MDI_AXIS_ARROW " 3D")))
            {
                camera.set_view_mode(Camera::CameraViewMode::Perspective);
                editor->get_editor_camera_controller().set_current_mode(EditorCameraMode::FLYCAM);
            }
            if (selected)
                ImGui::PopStyleColor();
            ImGui::NextColumn();

            selected = ortho;
            if (selected)
                ImGui::PushStyleColor(ImGuiCol_Text, ImGuiHelper::GetSelectedColour());
            if (ImGui::Button(U8CStr2CStr(ICON_MDI_ANGLE_RIGHT "2D")))
            {
                if (!ortho)
                {
                    camera.set_orthographic(true);
                    camera.set_near(-10.0f);
                    editor->get_editor_camera_controller().set_current_mode(EditorCameraMode::TWODIM);
                }
            }
            if (selected)
                ImGui::PopStyleColor();
            ImGui::NextColumn();
            auto view_mode = camera.get_view_mode();
            auto selected_mode = view_mode == Camera::CameraViewMode::Front;
            if(selected_mode)
                ImGui::PushStyleColor(ImGuiCol_Text, ImGuiHelper::GetSelectedColour());
            if (ImGui::Button("Front"))
            {
                camera.set_view_mode(Camera::CameraViewMode::Front);
                editor->get_editor_camera_controller().set_front_view(editor->get_editor_camera_transform());
            }
            if(selected_mode)
                ImGui::PopStyleColor();
            ImGui::NextColumn();
            selected_mode = view_mode == Camera::CameraViewMode::Back;
            if(selected_mode)
                ImGui::PushStyleColor(ImGuiCol_Text, ImGuiHelper::GetSelectedColour());
            if(ImGui::Button("Back"))
            {
                camera.set_view_mode(Camera::CameraViewMode::Back);
                editor->get_editor_camera_controller().set_back_view(editor->get_editor_camera_transform());
            }
            if(selected_mode)
                ImGui::PopStyleColor();
            ImGui::NextColumn();
            selected_mode = view_mode == Camera::CameraViewMode::Left;
            if(selected_mode)
                ImGui::PushStyleColor(ImGuiCol_Text, ImGuiHelper::GetSelectedColour());
            if (ImGui::Button("Left"))
            {
                camera.set_view_mode(Camera::CameraViewMode::Left);
                editor->get_editor_camera_controller().set_left_view(editor->get_editor_camera_transform());
            }
            if(selected_mode)
                ImGui::PopStyleColor();
            ImGui::NextColumn();
            selected_mode = view_mode == Camera::CameraViewMode::Right;
            if(selected_mode)
                ImGui::PushStyleColor(ImGuiCol_Text, ImGuiHelper::GetSelectedColour());
            if (ImGui::Button("Right"))
            {
                camera.set_view_mode(Camera::CameraViewMode::Right);
                editor->get_editor_camera_controller().set_right_view(editor->get_editor_camera_transform());
            }
            if(selected_mode)
                ImGui::PopStyleColor();
            ImGui::NextColumn();
            selected_mode = view_mode == Camera::CameraViewMode::Top;
            if(selected_mode)
                ImGui::PushStyleColor(ImGuiCol_Text, ImGuiHelper::GetSelectedColour());
            if (ImGui::Button("Top"))
            {
                camera.set_view_mode(Camera::CameraViewMode::Top);
                editor->get_editor_camera_controller().set_top_view(editor->get_editor_camera_transform());
            }
            if(selected_mode)
                ImGui::PopStyleColor();
            ImGui::NextColumn();
            selected_mode = view_mode == Camera::CameraViewMode::Bottom;
            if(selected_mode)
                ImGui::PushStyleColor(ImGuiCol_Text, ImGuiHelper::GetSelectedColour());
            if (ImGui::Button("Buttom"))
            {
                camera.set_view_mode(Camera::CameraViewMode::Bottom);
                editor->get_editor_camera_controller().set_buttom_view(editor->get_editor_camera_transform());
            }
            if(selected_mode)
                ImGui::PopStyleColor();
            ImGui::NextColumn();
            ImGui::Columns(1);
        /*    ImGui::SliderInt("Width", (int*) & m_Width, 128,2048);
            ImGui::SliderInt("Height", (int*)&m_Height,128,2048);*/
            auto fov = editor->get_camera()->get_fov();
            if (ImGui::DragFloat("Fov",&fov,1.0f,0, 90.0f))
                editor->get_camera()->set_fov(fov);
            auto speed = editor->get_editor_camera_controller().get_speed();
            if (ImGui::DragFloat("Speed", &speed,1.0f, 0, 100.0f))
                editor->get_editor_camera_controller().set_speed(speed);
            float Near = editor->get_camera()->get_near();
            if (ImGui::DragFloat("Near", &Near))
                editor->get_camera()->set_near(Near);
            float Far = editor->get_camera()->get_far();
            if (ImGui::DragFloat("Far", &Far))
                editor->get_camera()->set_far(Far);
            ImGui::EndPopup();
        }
       
        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        if (ImGui::Button(U8CStr2CStr("Splat" ICON_MDI_TOOLBOX_OUTLINE)))
            ImGui::OpenPopup("SplatTool");
        if(ImGui::BeginPopup("SplatTool"))
        {   
            ImGui::Separator();
            ImGuiStyle& style = ImGui::GetStyle();
            float old_spacing = style.ItemSpacing.y;
            style.ItemSpacing.y += 2;
            ImGui::Indent();
            if (ImGui::BeginMenu(" SplatVis", true))
            {
                for(auto i=0;i< SplatVisTypeStr.size();i++)
                { 
                    ImGui::Indent();
                    if (ImGui::MenuItem(SplatVisTypeStr[i]))
                    {
                        g_render_settings.gs_vis_type = i;
                    }
                    ImGui::Unindent();
                    ImGui::Separator();
                }
                ImGui::EndMenu();
            }
            ImGui::Unindent();
            style.ItemSpacing.y = old_spacing;

            if(ImGui::Button(U8CStr2CStr("  Edit" ICON_MDI_TOOLTIP_EDIT)))
                editor->splat_edit() = !editor->splat_edit();
            ImGui::EndPopup();
        }
        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();
        if (ImGui::Button(U8CStr2CStr(ICON_MDI_CAMERA_OUTLINE)))
            ImGui::OpenPopup("Take Photons");
        ImGuiHelper::Tooltip("Take Photons");
        if (ImGui::BeginPopupModal("Take Photons", NULL, ImGuiWindowFlags_AlwaysAutoResize))
        {
            take_photo_dialog();
            ImGui::EndPopup();
        }
        ImGui::PopStyleColor();
        ImGui::Unindent();
    }

    void SceneViewPanel::on_new_scene(Scene* scene)
    {
        DS_PROFILE_FUNCTION();
        //editor->GetSettings().aspect_ratio = 1.0f;
        current_scene = scene;
        Application::get().set_override_camera(editor->get_camera(), &editor->get_editor_camera_transform());
    }

    void SceneViewPanel::draw_gizmos(float width, float height, float xpos, float ypos, Scene* scene)
    {
        DS_PROFILE_FUNCTION();
        Camera* camera = editor->get_camera();
        maths::Transform& cameraTransform = editor->get_editor_camera_transform();
        auto& registry = scene->get_registry();
        glm::mat4 view = glm::inverse(cameraTransform.get_world_matrix());
        glm::mat4 proj = camera->get_projection_matrix();
        glm::mat4 viewProj = proj * view;
        const maths::Frustum& f = camera->get_frustum(view);
        ShowComponentGizmo<Camera>(width, height, xpos, ypos, viewProj, f, registry);
    }

    bool SceneViewPanel::resize(uint32_t width, uint32_t height)
    {
        DS_PROFILE_FUNCTION();
        bool resize = false;
        auto variable = CmadVariableMgr::get().find("r.takePhoton");
        if (variable)
        {   
            auto ret = variable->get_value<bool>();
            variable = CmadVariableMgr::get().find("r.video_export");
            ret |= variable ? variable->get_value<bool>() : false;
            if(ret) return false;
        }
        DS_ASSERT(width > 0 && height > 0, "Scene View Dimensions 0");

        Application::get().set_scene_view_dimensions(width, height);

        if (this->width != width || this->height != height)
        {
            resize = true;
            this->width = width;
            this->height = height;
        }

        if(resize)
        {
            editor->handle_renderer_resize(width,height);
        }
        return resize;
    }
    void SceneViewPanel::on_update(float dt)
    {

    }

    extern std::string get_resource_path();
    void SceneViewPanel::draw_splat_edit_toolbox(const ImVec2& sceneViewPosition,const ImVec2& sceneViewSize)
    {
        if(!editor->splat_edit()) return;
        float dpi = Application::get().get_window_dpi();
        auto& reg = editor->get_current_scene()->get_registry();
        auto select_valid = reg.valid(editor->get_current_splat_entt());
        auto resource_path = get_resource_path();

        { 
            auto size = item_btn_size + ImVec2(10, 10);
            auto offsetY = sceneViewPosition.y + (sceneViewSize.y - 12 * size.y) / 2.0f;
            ImGui::SetCursorPosY(offsetY);
            auto& splatEdit = GaussianEdit::get();
            auto editType = splatEdit.get_edit_type();
            auto edit_mode = splatEdit.get_edit_mode();
#ifdef DS_SPLAT_TRAIN
            auto gsTrain = reg.try_get<GaussianTrainerScene>(editor->get_current_splat_entt());
            if (gsTrain )
            {
                gsTrain->isTrain() = editType == diverse::GaussianEdit::EditType::None;
            }
#endif
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            bool selected = false;
            if (!UndoRedoSystem::get().can_redo())
            {
                ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
                if (ImGui::Button(U8CStr2CStr(ICON_MDI_REDO), item_btn_size))
                    UndoRedoSystem::get().redo();
                ImGui::PopStyleVar();
                ImGui::PopItemFlag();
            }
            else
            {
                if (ImGui::Button(U8CStr2CStr(ICON_MDI_REDO), item_btn_size))
                    UndoRedoSystem::get().redo();
            }
            ImGuiHelper::Tooltip("Redo");
            if (!UndoRedoSystem::get().can_undo())
            {
                ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
                if (ImGui::Button(U8CStr2CStr(ICON_MDI_UNDO), item_btn_size))
                    UndoRedoSystem::get().undo();
                ImGui::PopStyleVar();
                ImGui::PopItemFlag();
            }
            else
            {
                if (ImGui::Button(U8CStr2CStr(ICON_MDI_UNDO), item_btn_size))
                    UndoRedoSystem::get().undo();
            }
            ImGuiHelper::Tooltip("undo");

            if(select_valid)
            { 
                auto gaussian = reg.try_get<diverse::GaussianComponent>(editor->get_current_splat_entt());
                if (gaussian)
                {
                    splatEdit.set_edit_splat(gaussian);
                    splatEdit.set_edit_scene(editor->get_current_scene());
                }
                if (ImGui::Button(U8CStr2CStr(ICON_MDI_DELETE_OUTLINE), item_btn_size))
                    splatEdit.add_delete_op();
                ImGuiHelper::Tooltip("delete select splats");
                if (ImGui::Button(U8CStr2CStr(ICON_MDI_DELETE_RESTORE), item_btn_size))
                    splatEdit.add_reset_op();
                ImGuiHelper::Tooltip("reset delete splats");
            }
            else
            {
                ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
                ImGui::Button(U8CStr2CStr(ICON_MDI_DELETE_OUTLINE), item_btn_size);
                ImGuiHelper::Tooltip("delete select splats");
                ImGui::Button(U8CStr2CStr(ICON_MDI_DELETE_RESTORE), item_btn_size);
                ImGuiHelper::Tooltip("reset delete splats");
                ImGui::PopStyleVar();
                ImGui::PopItemFlag();
            }
         
            {
                selected = editType == GaussianEdit::EditType::Rect;
                if (selected)
                    ImGui::PushStyleColor(ImGuiCol_Button, ImGuiHelper::GetSelectedColour());

                auto picker_icon = get_embed_texture(resource_path + "svg_icon/select-picker.png");
                if(ImGui::ImageButton("picker", Application::get().get_imgui_manager()->get_imgui_renderer()->add_texture(picker_icon->gpu_texture), icon_btn_size * dpi))
                    splatEdit.set_edit_type(selected ? GaussianEdit::EditType::None : GaussianEdit::EditType::Rect);
                if (selected)
                    ImGui::PopStyleColor();
                ImGuiHelper::Tooltip("Pick/Rect Select");
                item_btn_size = ImGui::GetItemRectSize();
            }

            {
                selected = editType == GaussianEdit::EditType::Brush;
                if (selected)
                    ImGui::PushStyleColor(ImGuiCol_Button, ImGuiHelper::GetSelectedColour());
                auto brush_icon = get_embed_texture(resource_path + "svg_icon/select-brush.png");
                if(ImGui::ImageButton("brush_select",Application::get().get_imgui_manager()->get_imgui_renderer()->add_texture(brush_icon->gpu_texture), icon_btn_size * dpi))
                    splatEdit.set_edit_type(selected ? GaussianEdit::EditType::None : GaussianEdit::EditType::Brush);
                if (selected)
                    ImGui::PopStyleColor();
                ImGuiHelper::Tooltip("Brush Select");
            }
            {
                selected = editType == GaussianEdit::EditType::Polygon;
                if (selected)
                    ImGui::PushStyleColor(ImGuiCol_Button, ImGuiHelper::GetSelectedColour());

                auto poly_icon = get_embed_texture(resource_path + "svg_icon/select-poly.png");
                if(ImGui::ImageButton("poly_select",Application::get().get_imgui_manager()->get_imgui_renderer()->add_texture(poly_icon->gpu_texture), icon_btn_size * dpi))
                    splatEdit.set_edit_type(selected ? GaussianEdit::EditType::None : GaussianEdit::EditType::Polygon);
                if (selected)
                    ImGui::PopStyleColor();
                ImGuiHelper::Tooltip("Polygon Select");
            }
            {
                selected = editType == GaussianEdit::EditType::Lasso;
                if (selected)
                    ImGui::PushStyleColor(ImGuiCol_Button, ImGuiHelper::GetSelectedColour());

                auto lasso_icon = get_embed_texture(resource_path + "svg_icon/select-lasso.png");
                if(ImGui::ImageButton("lasso_select",Application::get().get_imgui_manager()->get_imgui_renderer()->add_texture(lasso_icon->gpu_texture), icon_btn_size * dpi))
                    splatEdit.set_edit_type(selected ? GaussianEdit::EditType::None : GaussianEdit::EditType::Lasso);
                if (selected)
                    ImGui::PopStyleColor();
                ImGuiHelper::Tooltip("Lasso Select");
            }
            bool click_box_or_sphere = false;
            {
                selected = editType == GaussianEdit::EditType::Box;
                if (selected)
                    ImGui::PushStyleColor(ImGuiCol_Button, ImGuiHelper::GetSelectedColour());

                if (ImGui::Button(U8CStr2CStr(ICON_MDI_CUBE_OUTLINE), ImGui::GetItemRectSize()))
                {
                    splatEdit.set_edit_type(selected ? GaussianEdit::EditType::None : GaussianEdit::EditType::Box);
                    click_box_or_sphere = true;
                }
                if (selected)
                    ImGui::PopStyleColor();
                ImGuiHelper::Tooltip("Box Select");
            }
            {
                selected = editType == GaussianEdit::EditType::Sphere;
                if (selected)
                    ImGui::PushStyleColor(ImGuiCol_Button, ImGuiHelper::GetSelectedColour());

                auto sphere_icon = get_embed_texture(resource_path + "svg_icon/select-sphere.png");
                if(ImGui::ImageButton("sphere_select", Application::get().get_imgui_manager()->get_imgui_renderer()->add_texture(sphere_icon->gpu_texture), icon_btn_size * dpi))
                {
                    splatEdit.set_edit_type(selected ? GaussianEdit::EditType::None : GaussianEdit::EditType::Sphere);
                    click_box_or_sphere = true;
                }
                if (selected)
                    ImGui::PopStyleColor();
                ImGuiHelper::Tooltip("Sphere Select");
            }
            // if (click_box_or_sphere && select_valid)
            // {
            //     auto& gaussian = reg.get<diverse::GaussianComponent>(editor->get_current_splat_entt());
            //     auto& gs_transform = reg.get<diverse::maths::Transform>(editor->get_current_splat_entt());
            //     auto box = gaussian.ModelRef->get_world_bounding_box(gs_transform.get_local_matrix());
            //     if (splatEdit.get_edit_type() == diverse::GaussianEdit::EditType::Box)
            //     {
            //         splatEdit.bouding_box() = box;
            //     }
            //     else
            //     {
            //         auto center = box.center();
            //         auto dialog = glm::length(box.get_extents());
            //         splatEdit.bouding_sphere() = maths::BoundingSphere(center, dialog / 2.0f);
            //     }
            // }
            {
                if (ImGui::Button(edit_mode == GaussianEdit::EditMode::Points ?  U8CStr2CStr(ICON_MDI_CIRCLE_SMALL) : U8CStr2CStr(ICON_MDI_RHOMBUS_MEDIUM), item_btn_size))
                {
                    splatEdit.set_edit_mode(edit_mode == GaussianEdit::EditMode::Points ? GaussianEdit::EditMode::Rings : GaussianEdit::EditMode::Points);
                    splatEdit.set_edit_mode(edit_mode == GaussianEdit::EditMode::Points ? GaussianEdit::EditMode::Rings : GaussianEdit::EditMode::Points);
                    //g_render_settings.gs_vis_type = edit_mode == GaussianEdit::EditMode::Rings ? (int)(GaussianRenderType::Rings) : (int)(GaussianRenderType::Splat);
                }
                if(splatEdit.get_edit_type() >= GaussianEdit::EditType::Box && splatEdit.get_edit_type() < GaussianEdit::EditType::Paint) g_render_settings.splat_edit_render_mode = (int)edit_mode;
                else g_render_settings.splat_edit_render_mode = 0xff;
                ImGuiHelper::Tooltip(edit_mode == GaussianEdit::EditMode::Points ? "Centers" : "Rings");
                {
                    selected = editType == GaussianEdit::EditType::Paint;
                    if (selected)
                        ImGui::PushStyleColor(ImGuiCol_Button, ImGuiHelper::GetSelectedColour());
                    if (ImGui::Button(U8CStr2CStr(ICON_MDI_BRUSH), item_btn_size))
                    {
                        splatEdit.set_edit_type(selected ? GaussianEdit::EditType::None : GaussianEdit::EditType::Paint);
                    }
                    if (selected)
                        ImGui::PopStyleColor();
                    ImGuiHelper::Tooltip("Paint");
                }
                if (ImGui::Button(U8CStr2CStr(ICON_MDI_SETTINGS), item_btn_size))
                    ImGui::OpenPopup("SplatSetting");
                if (ImGui::BeginPopup("SplatSetting"))
                {
                    //ImGuiHelper::Property("ptsSize", g_render_settings.gs_point_size, 0.0f, 10.0f, 0.5f,ImGuiHelper::PropertyFlag::SliderValue);
                    ImGui::Columns(1);
                    ImGui::PushItemWidth(150);
            
                    ImGui::SliderFloat("PointSize", &g_render_settings.gs_point_size, 0.0f, 20.0f);
                    auto& thickness = splatEdit.brush_thick_ness();
                    ImGui::SliderInt("BrushThickness",(int*)&thickness, 0, 100);
                    // ImGui::SliderFloat("Radius", &splatEdit.bouding_sphere().radius(), 0.0f, 1000.0f);
                    ImGui::DragFloat("Radius", &splatEdit.bouding_sphere().radius(), 0.1f, 0, 400.0f);
                    ImGui::ColorEdit4("SelectColor", glm::value_ptr(g_render_settings.select_color));
                    ImGui::ColorEdit4("LockedColor", glm::value_ptr(g_render_settings.locked_color));
                    ImGui::ColorEdit3("PaintColor", glm::value_ptr(g_render_settings.paint_color));
                    ImGui::DragFloat("MixPaintWeight", &g_render_settings.paint_weight, 0.05f, 0.0f, 1.0f);
                    ImGui::Checkbox("Histogram", &editor->histogram_panel->active());
                    auto variable = CmadVariableMgr::get().find("r.enableOutline");
                    bool enableOutline = false;
                    if (variable) enableOutline = variable->get_value<bool>();
                    if (ImGui::Checkbox("Outline", &enableOutline))
                        variable->set_value<bool>(enableOutline);

                    ImGui::PopItemWidth();
                    ImGui::EndPopup();
                }
                ImGuiHelper::Tooltip("Setting");
            }
            ImGui::PopStyleColor();
        }
    }

    void SceneViewPanel::draw_progress(const ImVec2& sceneViewPosition, const ImVec2& sceneViewSize)
    {
#ifdef DS_SPLAT_TRAIN
        //drawing loading progress bar
        auto train_ent = current_scene->get_entity_manager()->get_entities_with_type<GaussianTrainerScene>();
        if (!train_ent.empty())
        {
            auto& gs_train = train_ent.front().get_component<GaussianTrainerScene>();

            const ImU32 fg_col = ImGui::GetColorU32(ImVec4(0.455f, 0.198f, 0.301f,0.86f));// ImGui::GetColorU32(ImGuiCol_ButtonHovered);
            const ImU32 bg_col = ImGui::GetColorU32(ImVec4(0.47f, 0.77f, 0.83f, 0.14f));// ImGui::GetColorU32(ImGuiCol_Button);

            float value = 0.0;
            value = gs_train.getProgressOnCurrentPhase();
            auto currentStatus = gs_train.getCurrentTrainingStatus();
            if(currentStatus >= TrainingStatus::Loading_Prepare && currentStatus < TrainingStatus::Preprocess_Done)
            {
                auto drawList = ImGui::GetWindowDrawList();
                ImVec2 size_arg = { sceneViewSize.x * 0.6f,16};
                ImVec2 size = { size_arg.x, size_arg.y };

                auto pos = ImVec2(sceneViewPosition.x + sceneViewSize.x * 0.2, sceneViewPosition.y + sceneViewSize.y*0.5);
                const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
                // Render
                const float circleStart = size.x * 0.7f;
                const float circleEnd = size.x;
                const float circleWidth = circleEnd - circleStart;
         
                drawList->AddRectFilled(bb.Min, ImVec2(pos.x + circleStart, bb.Max.y), bg_col);
                drawList->AddRectFilled(bb.Min, ImVec2(pos.x + circleStart * value, bb.Max.y), fg_col);

                const float t = float(ImGui::GetCurrentContext()->Time);
                const float r = size.y * 0.5f;
                const float speed = 1.5f;

                const float a = speed * 0.f;
                const float b = speed * 0.333f;
                const float c = speed * 0.666f;

                const float o1 = (circleWidth + r) * (t + a - speed * (int)((t + a) / speed)) / speed;
                const float o2 = (circleWidth + r) * (t + b - speed * (int)((t + b) / speed)) / speed;
                const float o3 = (circleWidth + r) * (t + c - speed * (int)((t + c) / speed)) / speed;

                drawList->AddCircleFilled(ImVec2(pos.x + circleEnd - o1, bb.Min.y + r), r, bg_col);
                drawList->AddCircleFilled(ImVec2(pos.x + circleEnd - o2, bb.Min.y + r), r, bg_col);
                drawList->AddCircleFilled(ImVec2(pos.x + circleEnd - o3, bb.Min.y + r), r, bg_col);

                drawList->AddText(pos, ImGui::GetColorU32(ImGuiCol_Text), gs_train.getCurrentTrainingPhaseName().c_str());
            }
            if (currentStatus == TrainingStatus::GS2Mesh)
            {
                auto drawList = ImGui::GetWindowDrawList();
                auto pos = ImVec2(sceneViewPosition.x + sceneViewSize.x * 0.5, sceneViewPosition.y + sceneViewSize.y * 0.5);
                const auto fg_col = ImVec4(0.455f, 0.198f, 0.301f, 0.86f);
                ImGui::LoadingIndicatorCircle("##spinner", 30.0f, pos, fg_col, fg_col, 12.0f);
                drawList->AddText(pos, ImGui::GetColorU32(ImGuiCol_Text),"Generating Mesh ...");
            }
        }
#endif
        //draw model loading progress bar
        auto mesh_group = current_scene->get_registry().group<MeshModelComponent>(entt::get<maths::Transform>);
        for (auto ent : mesh_group)
        {
            if (!Entity(ent, current_scene).active())
                continue;
            const auto& [model, trans] = mesh_group.get<MeshModelComponent, maths::Transform>(ent);
            if(model.ModelRef->get_file_path().empty()) continue;
            if (!model.ModelRef->is_flag_set(AssetFlag::UploadedGpu) && !model.ModelRef->is_flag_set(AssetFlag::Invalid))
            {
                auto pos = ImVec2(sceneViewPosition.x + sceneViewSize.x * 0.5, sceneViewPosition.y + sceneViewSize.y * 0.5);
                const auto fg_col = ImVec4(0.455f, 0.198f, 0.301f, 0.86f);
                ImGui::LoadingIndicatorCircle("##spinner", 30.0f, pos, fg_col, fg_col, 12.0f);
            }
        }
        //point
        auto point_group = current_scene->get_registry().group<PointCloudComponent>(entt::get<maths::Transform>);
        for (auto ent : point_group)
        {
            if (!Entity(ent, current_scene).active())
                continue;
            const auto& [model, trans] = point_group.get<PointCloudComponent, maths::Transform>(ent);
            if(model.ModelRef->get_file_path().empty()) continue;
            if (!model.ModelRef->is_flag_set(AssetFlag::UploadedGpu) && !model.ModelRef->is_flag_set(AssetFlag::Invalid))
            {
                auto pos = ImVec2(sceneViewPosition.x + sceneViewSize.x * 0.5, sceneViewPosition.y + sceneViewSize.y * 0.5);
                const auto fg_col = ImVec4(0.455f, 0.198f, 0.301f, 0.86f);
                ImGui::LoadingIndicatorCircle("##spinner", 30.0f, pos, fg_col, fg_col, 12.0f);
            }
        }

        auto group = current_scene->get_registry().group<GaussianComponent>(entt::get<maths::Transform>);
        for (auto gs_ent : group)
        {
            if (!Entity(gs_ent, current_scene).active())
                continue;
            const auto& [model, trans] = group.get<GaussianComponent, maths::Transform>(gs_ent);
            if (model.ModelRef->get_file_path().empty()) continue;
            if (!model.ModelRef->is_flag_set(AssetFlag::UploadedGpu) && !model.ModelRef->is_flag_set(AssetFlag::Invalid))
            {
                auto pos = ImVec2(sceneViewPosition.x + sceneViewSize.x * 0.5, sceneViewPosition.y + sceneViewSize.y * 0.5);
                const auto fg_col = ImVec4(0.455f, 0.198f, 0.301f, 0.86f);
                ImGui::LoadingIndicatorCircle("##spinner",30.0f, pos, fg_col, fg_col, 12.0f);
            }
        }
        //draw prompt to tell user operation guide
        if (group.empty())
        {
            if (mesh_group.empty())
            {
                auto pos = ImVec2(sceneViewPosition.x + sceneViewSize.x * 0.25, sceneViewPosition.y + sceneViewSize.y * 0.5);
                auto drawList = ImGui::GetWindowDrawList();
#ifdef DS_SPLAT_TRAIN
                const char* text = R"(Drag image sequence/folders or video to this view window for training or 
                        splat model/mesh model file for rendering)";
#else
                const char* text = R"(Drag splat model/mesh model file for rendering)";
#endif
                drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 1.2, pos, ImGui::GetColorU32(ImGuiCol_Text), text);
            }
        }
    }
    
    void SceneViewPanel::handle_splat_edit(Camera* camera, maths::Transform* camera_transform, const ImVec2& sceneViewPosition, const ImVec2& sceneViewSize)
    {
        if (!editor->splat_edit()) return;
        auto splat_ent = Entity(editor->get_current_splat_entt(), editor->get_current_scene());
        if (!splat_ent.valid()) return;
        auto updateCamera = editor->get_scene_view_active();
        auto& gs_edit = GaussianEdit::get();
        if(!editor->splat_edit())  gs_edit.set_edit_type(diverse::GaussianEdit::EditType::None);
  
        auto drawList = ImGui::GetWindowDrawList();
        {
            glm::mat4 view = glm::inverse(camera_transform->get_world_matrix());
            glm::mat4 proj = camera->get_projection_matrix();
            auto world2proj = proj * view;
            auto edit_transform = gs_edit.get_edit_transform();
            auto m = edit_transform.get_local_matrix();
            if (gs_edit.get_edit_type() == GaussianEdit::EditType::Box)
            {
                auto& bd_box = gs_edit.bouding_box();
                auto b = bd_box.max();
                auto a = bd_box.min();

                using namespace glm;
                draw_3d_line(drawList, world2proj, m * vec3{ a.x, a.y, a.z }, m * vec3{ a.x, a.y, b.z }, 0xffff4040, sceneViewPosition, sceneViewSize);
                draw_3d_line(drawList, world2proj, m * vec3{ b.x, a.y, a.z }, m * vec3{ b.x, a.y, b.z }, 0xffffffff, sceneViewPosition, sceneViewSize);
                draw_3d_line(drawList, world2proj, m * vec3{ a.x, b.y, a.z }, m * vec3{ a.x, b.y, b.z }, 0xffffffff, sceneViewPosition, sceneViewSize);
                draw_3d_line(drawList, world2proj, m * vec3{ b.x, b.y, a.z }, m * vec3{ b.x, b.y, b.z }, 0xffffffff, sceneViewPosition, sceneViewSize);

                draw_3d_line(drawList, world2proj, m * vec3{ a.x, a.y, a.z }, m * vec3{ b.x, a.y, a.z }, 0xff4040ff, sceneViewPosition, sceneViewSize); // X
                draw_3d_line(drawList, world2proj, m * vec3{ a.x, b.y, a.z }, m * vec3{ b.x, b.y, a.z }, 0xffffffff, sceneViewPosition, sceneViewSize);
                draw_3d_line(drawList, world2proj, m * vec3{ a.x, a.y, b.z }, m * vec3{ b.x, a.y, b.z }, 0xffffffff, sceneViewPosition, sceneViewSize);
                draw_3d_line(drawList, world2proj, m * vec3{ a.x, b.y, b.z }, m * vec3{ b.x, b.y, b.z }, 0xffffffff, sceneViewPosition, sceneViewSize);

                draw_3d_line(drawList, world2proj, m * vec3{ a.x, a.y, a.z }, m * vec3{ a.x, b.y, a.z }, 0xff40ff40, sceneViewPosition, sceneViewSize); // Y
                draw_3d_line(drawList, world2proj, m * vec3{ b.x, a.y, a.z }, m * vec3{ b.x, b.y, a.z }, 0xffffffff, sceneViewPosition, sceneViewSize);
                draw_3d_line(drawList, world2proj, m * vec3{ a.x, a.y, b.z }, m * vec3{ a.x, b.y, b.z }, 0xffffffff, sceneViewPosition, sceneViewSize);
                draw_3d_line(drawList, world2proj, m * vec3{ b.x, a.y, b.z }, m * vec3{ b.x, b.y, b.z }, 0xffffffff, sceneViewPosition, sceneViewSize);

            }
            else if (gs_edit.get_edit_type() == GaussianEdit::EditType::Sphere)
            {
                const auto& bd_sphere = gs_edit.bouding_sphere();
                const auto center = edit_transform.get_local_position() + bd_sphere.get_center();
                const auto scale = glm::compMax(edit_transform.get_local_scale());
                const auto radius = bd_sphere.get_radius() * scale;
                ImGuizmo::drawSphereWireframe(glm::value_ptr(view), glm::value_ptr(proj), glm::value_ptr(center), radius, sceneViewPosition, sceneViewSize);
            }
        }
        if (ImGui::IsWindowFocused())
        { 
            gs_edit.create_brush_buffer(width, height);
            if (gs_edit.get_edit_type() != GaussianEdit::EditType::None)
            {
                const auto valid_region = is_valid_region(sceneViewPosition, sceneViewSize);
                EditSelectOpType op = EditSelectOpType::Set;
                if( valid_region)
                { 
                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && updateCamera && !ImGuizmo::IsUsing())
                    {
                        start_mouse_pos = ImGui::GetMousePos();//Input::get().get_mouse_position();
                        if (gs_edit.get_edit_type() == GaussianEdit::EditType::Rect)
                        {
                            auto p0 = start_mouse_pos - sceneViewPosition;
                            gs_edit.rect_area() = { p0.x, p0.y, 1.0f, 1.0f };
                        }
                        if (gs_edit.get_edit_type() == GaussianEdit::EditType::Brush 
                            || gs_edit.get_edit_type() == GaussianEdit::EditType::Lasso
                            || gs_edit.get_edit_type() == GaussianEdit::EditType::Paint)
                        {
                            gs_edit.reset_brush();
                            auto p0 = start_mouse_pos - sceneViewPosition;
                            gs_edit.brush_points().push_back({p0.x,p0.y});
                        }
                        else if (gs_edit.get_edit_type() == GaussianEdit::EditType::Polygon)
                        {
                            //whether first click , the first point
                            if(gs_edit.brush_points().size() == 0) 
                            {
                                gs_edit.brush_mask()->setTo(cv::Scalar(0, 0, 0, 0));
                                gs_edit.closed_polygon = false;
                            }
                            auto p0 = start_mouse_pos - sceneViewPosition;
                            if (gs_edit.brush_points().size() >= 3)
                            {
                                auto len = glm::length(glm::vec2(p0) - glm::vec2(gs_edit.brush_points().front()));
                                if (len <= 3)
                                    gs_edit.closed_polygon = true;
                            }
                            if(!gs_edit.closed_polygon)
                                gs_edit.brush_points().push_back({p0.x,p0.y});
                        }
                        is_doing_splat_select_op = true;
                    }
                    if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && updateCamera && !ImGuizmo::IsUsing())
                    {
                        ImU32 GRID_COLOR = IM_COL32(200, 200, 200, 255);
                        float gridThickness = 1.0f;
                        const auto& gridColor = GRID_COLOR;
                        auto p0 = start_mouse_pos - sceneViewPosition;
                        auto p1 = ImGui::GetMousePos() - sceneViewPosition;
                        if (gs_edit.get_edit_type() == GaussianEdit::EditType::Rect)
                        {
                            drawList->AddLine(ImVec2{ p0.x, p0.y } + sceneViewPosition, ImVec2{ p1.x, p0.y } + sceneViewPosition, gridColor, gridThickness);
                            drawList->AddLine(ImVec2{ p0.x, p0.y } + sceneViewPosition, ImVec2{ p0.x, p1.y } + sceneViewPosition, gridColor, gridThickness);
                            drawList->AddLine(ImVec2{ p1.x, p0.y } + sceneViewPosition, ImVec2{ p1.x, p1.y } + sceneViewPosition, gridColor, gridThickness);
                            drawList->AddLine(ImVec2{ p0.x, p1.y } + sceneViewPosition, ImVec2{ p1.x, p1.y } + sceneViewPosition, gridColor, gridThickness);
                            gs_edit.rect_area() = { p0.x, p0.y, p1.x - p0.x + 1.0f, p1.y - p0.y + 1.0f};
                        }
                        else if (gs_edit.get_edit_type() == GaussianEdit::EditType::Brush 
                                || gs_edit.get_edit_type() == GaussianEdit::EditType::Lasso
                                || gs_edit.get_edit_type() == GaussianEdit::EditType::Paint)
                        {
                            auto p1_v = glm::ivec2(p1.x, p1.y);
                            if (gs_edit.brush_points().size() > 0 && gs_edit.brush_points().back() != p1_v)
                                gs_edit.brush_points().push_back(p1_v);
                            const int num_points = gs_edit.brush_points().size();
                            auto ptr = gs_edit.brush_points().data();
                            if (gs_edit.get_edit_type() == GaussianEdit::EditType::Brush
                                || gs_edit.get_edit_type() == GaussianEdit::EditType::Paint)
                            {
                                auto paint_color = g_render_settings.paint_color;
                                auto line_color = gs_edit.get_edit_type() == GaussianEdit::EditType::Paint ?  cv::Scalar(255 * paint_color.x, 255 * paint_color.y, 255 * paint_color.z, 102) : cv::Scalar(255, 102, 0, 102);
                                cv::polylines(*gs_edit.brush_mask(), (const cv::Point* const*)(&ptr), &num_points, 1, false, line_color, gs_edit.brush_thick_ness(), cv::LINE_AA);
                            }
                            else if (gs_edit.get_edit_type() == GaussianEdit::EditType::Lasso)
                            {
                                cv::polylines(*gs_edit.brush_mask(), (const cv::Point* const*)(&ptr), &num_points, 1, true, cv::Scalar(255, 255, 255, 255), 1.0f, cv::LINE_AA);
                                cv::fillPoly(*gs_edit.brush_mask(), (const cv::Point**)(&ptr), &num_points, 1, cv::Scalar(255, 102, 0, 102));
                            }
                            gs_edit.update_brush_buffer();

                            auto texID = Application::get().get_imgui_manager()->get_imgui_renderer()->add_texture(gs_edit.brush_texture());
                            drawList->AddImage(texID, sceneViewPosition, sceneViewPosition+sceneViewSize, ImVec2(0,0), ImVec2(1, 1), ImGui::GetColorU32(ImVec4(1,1,1,1)));
                        }
                        is_doing_splat_select_op = true;
                    }
                    else if(updateCamera)
                    {
                        if (gs_edit.get_edit_type() != GaussianEdit::EditType::Polygon
                            && gs_edit.get_edit_type() != GaussianEdit::EditType::Box
                            && gs_edit.get_edit_type() != GaussianEdit::EditType::Sphere)
                        { 
                            if (gs_edit.get_splat_intersected())
                            { 
                                gs_edit.reset_splat_intersected();
                                end_mouse_pos = ImGui::GetMousePos();//Input::get().get_mouse_position();
                                op = Input::get().get_key_held(InputCode::Key::LeftShift) ? EditSelectOpType::Add : (Input::get().get_key_held(InputCode::Key::LeftControl) ? EditSelectOpType::Remove : EditSelectOpType::Set);
                                if(gs_edit.get_edit_type() == GaussianEdit::EditType::Paint)
                                    gs_edit.add_paint_op();
                                else
                                    gs_edit.add_selection_op(op);
                                if (gs_edit.get_edit_type() == GaussianEdit::EditType::Brush 
                                    || gs_edit.get_edit_type() == GaussianEdit::EditType::Lasso
                                    || gs_edit.get_edit_type() == GaussianEdit::EditType::Paint)
                                    gs_edit.reset_brush();
                            }

                            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) )
                            {
                                if(!ImGuizmo::IsUsing() && is_doing_splat_select_op)
                                { 
                                    gs_edit.reset_splat_intersected();
                                    gs_edit.intersect_splat(op);
                                }
                                is_doing_splat_select_op = false;
                            }
                        }
                    }
                    if (gs_edit.get_edit_type() == GaussianEdit::EditType::Polygon)
                    {
                        //draw polgon
                        auto col = IM_COL32(180, 180, 0, 255);
                        float thickness = 1.0f;
                        auto p1 = ImGui::GetMousePos() - sceneViewPosition;
                        if (gs_edit.brush_points().size() >= 3)
                        {
                            auto len = glm::length(glm::vec2(p1.x, p1.y) - glm::vec2(gs_edit.brush_points().front()));
                            if (len <= 3)
                            {
                                thickness = 2.0f;
                                col = IM_COL32(255, 255, 0, 255);
                            }
                        }
                        if (gs_edit.brush_points().size() >= 1)
                        {
                            for (auto i = 0; i < gs_edit.brush_points().size() - 1; i++)
                            {
                                auto p0 = gs_edit.brush_points()[i];
                                auto p1 = gs_edit.brush_points()[i + 1];
                                drawList->AddLine(ImVec2(p0.x, p0.y) + sceneViewPosition, ImVec2(p1.x, p1.y) + sceneViewPosition, col, thickness);
                            }
                            auto p0 = gs_edit.brush_points().back();
                            drawList->AddLine(ImVec2(p0.x, p0.y) + sceneViewPosition, ImVec2(p1.x, p1.y) + sceneViewPosition, col, thickness);
                        }
                        if (gs_edit.get_splat_intersected() && gs_edit.closed_polygon)
                        {
                            gs_edit.reset_splat_intersected();
                            gs_edit.closed_polygon = false;
                            op = Input::get().get_key_held(InputCode::Key::LeftShift) ? EditSelectOpType::Add : (Input::get().get_key_held(InputCode::Key::LeftControl) ? EditSelectOpType::Remove : EditSelectOpType::Set);
                            gs_edit.add_selection_op(op);
                        }
                        if (gs_edit.closed_polygon)
                        {
                            const int num_points = gs_edit.brush_points().size();
                            auto ptr = gs_edit.brush_points().data();
                            cv::polylines(*gs_edit.brush_mask(), (const cv::Point* const*)(&ptr), &num_points, 1, true, cv::Scalar(255, 102, 0, 102), gs_edit.brush_thick_ness(), cv::LINE_AA);
                            gs_edit.update_brush_buffer();
                            gs_edit.reset_splat_intersected();
                            gs_edit.intersect_splat(op);
                            gs_edit.reset_brush();
                        }
                    }
                    if(gs_edit.get_edit_type() == GaussianEdit::EditType::Brush || gs_edit.get_edit_type() == GaussianEdit::EditType::Paint)
                    {
                        glm::vec4 paint_color = {g_render_settings.paint_color, g_render_settings.paint_weight};
                        auto cirle_color = gs_edit.get_edit_type() == GaussianEdit::EditType::Paint ? IM_COL32(255 * paint_color.x, 255 * paint_color.y, 255 * paint_color.z, 102) : IM_COL32(255, 102, 0, 102);
                        drawList->AddCircleFilled(ImGui::GetMousePos(), gs_edit.brush_thick_ness() / 2.0f, cirle_color);
                    }
                }
            }

            if (Input::get().get_key_pressed(InputCode::Key::Delete))
                gs_edit.add_delete_op();
            else if (Input::get().get_key_pressed(InputCode::Key::R))
                gs_edit.add_reset_op();
            else if(Input::get().get_key_pressed(InputCode::Key::H))
                gs_edit.add_lock_op();
            else if(Input::get().get_key_pressed(InputCode::Key::U))
                gs_edit.add_unlock_op();
            else if (Input::get().get_key_pressed(InputCode::Key::A) && Input::get().get_key_held(InputCode::Key::LeftShift))
                gs_edit.add_select_none_op();
            else if (Input::get().get_key_pressed(InputCode::Key::A) && Input::get().get_key_held(InputCode::Key::LeftControl))
                gs_edit.add_select_all_op();
            else if (Input::get().get_key_pressed(InputCode::Key::I))
                gs_edit.add_select_inverse_op();
            if (gs_edit.get_edit_type() == GaussianEdit::EditType::Polygon && Input::get().get_key_pressed(InputCode::Key::Escape))
            {
                gs_edit.reset_brush();
            }
            if (Input::get().get_key_held(InputCode::Key::LeftControl) && gs_edit.get_edit_type() == GaussianEdit::EditType::Brush)
            {
                auto& brush_thickness = gs_edit.brush_thick_ness();
                ImGuiIO& io = ImGui::GetIO();
                float offset = io.MouseWheel;
                if (offset != 0)
                {
                    brush_thickness += offset * 5.0f;
                    brush_thickness = std::clamp<uint>(brush_thickness, 20, 200);
                }
            }
        }
    }

    void SceneViewPanel::draw_splat_focus_box(Camera* camera, maths::Transform* camera_transform,const ImVec2& sceneViewPosition, const ImVec2& sceneViewSize)
    {
#ifdef DS_SPLAT_TRAIN
        auto splat_ent = Entity(editor->get_current_splat_entt(), editor->get_current_scene());
        if (splat_ent.valid() && splat_ent.active() )
        {

            auto gsTrain = splat_ent.try_get_component<GaussianTrainerScene>();
            if(!gsTrain) return;
            if(!gsTrain->getTrainConfig().enableFocusRegion) return;
            auto [a,b] = gsTrain->getFocusRegion();
            auto focusTransform = gsTrain->getFocusRegionTransform();
            auto model = splat_ent.get_component<maths::Transform>();
            glm::mat4 view = glm::inverse(camera_transform->get_world_matrix());
            glm::mat4 proj = camera->get_projection_matrix();
            auto world2proj = proj * view;
            auto m = model.get_world_matrix() * focusTransform;           
            auto drawList = ImGui::GetWindowDrawList();
            using namespace glm;
            draw_3d_line(drawList, world2proj, m * vec3{ a.x, a.y, a.z }, m * vec3{ a.x, a.y, b.z }, 0xffff4040, sceneViewPosition, sceneViewSize);
            draw_3d_line(drawList, world2proj, m * vec3{ b.x, a.y, a.z }, m * vec3{ b.x, a.y, b.z }, 0xffffffff, sceneViewPosition, sceneViewSize);
            draw_3d_line(drawList, world2proj, m * vec3{ a.x, b.y, a.z }, m * vec3{ a.x, b.y, b.z }, 0xffffffff, sceneViewPosition, sceneViewSize);
            draw_3d_line(drawList, world2proj, m * vec3{ b.x, b.y, a.z }, m * vec3{ b.x, b.y, b.z }, 0xffffffff, sceneViewPosition, sceneViewSize);

            draw_3d_line(drawList, world2proj, m * vec3{ a.x, a.y, a.z }, m * vec3{ b.x, a.y, a.z }, 0xff4040ff, sceneViewPosition, sceneViewSize); // X
            draw_3d_line(drawList, world2proj, m * vec3{ a.x, b.y, a.z }, m * vec3{ b.x, b.y, a.z }, 0xffffffff, sceneViewPosition, sceneViewSize);
            draw_3d_line(drawList, world2proj, m * vec3{ a.x, a.y, b.z }, m * vec3{ b.x, a.y, b.z }, 0xffffffff, sceneViewPosition, sceneViewSize);
            draw_3d_line(drawList, world2proj, m * vec3{ a.x, b.y, b.z }, m * vec3{ b.x, b.y, b.z }, 0xffffffff, sceneViewPosition, sceneViewSize);

            draw_3d_line(drawList, world2proj, m * vec3{ a.x, a.y, a.z }, m * vec3{ a.x, b.y, a.z }, 0xff40ff40, sceneViewPosition, sceneViewSize); // Y
            draw_3d_line(drawList, world2proj, m * vec3{ b.x, a.y, a.z }, m * vec3{ b.x, b.y, a.z }, 0xffffffff, sceneViewPosition, sceneViewSize);
            draw_3d_line(drawList, world2proj, m * vec3{ a.x, a.y, b.z }, m * vec3{ a.x, b.y, b.z }, 0xffffffff, sceneViewPosition, sceneViewSize);
            draw_3d_line(drawList, world2proj, m * vec3{ b.x, a.y, b.z }, m * vec3{ b.x, b.y, b.z }, 0xffffffff, sceneViewPosition, sceneViewSize);
        }
#endif
    }
    
    void SceneViewPanel::handle_splat_crop(Camera* camera, maths::Transform* camera_transform,const ImVec2& sceneViewPosition, const ImVec2& sceneViewSize)
    {
        auto splat_group = current_scene->get_registry().group<GaussianCrop>(entt::get<maths::Transform>);
        for (auto gs_ent : splat_group)
        {
            if (!Entity(gs_ent, current_scene).active())
                continue;

            auto drawList = ImGui::GetWindowDrawList();
            auto& gs_crop = splat_group.get<GaussianCrop>(gs_ent);
            auto& crop_datas = gs_crop.get_crop_data();
            for (auto& crop_data : crop_datas)
            {
                glm::mat4 view = glm::inverse(camera_transform->get_world_matrix());
                glm::mat4 proj = camera->get_projection_matrix();
                auto world2proj = proj * view;
                auto& model = crop_data.transform;
                auto m = model.get_world_matrix();
                if (crop_data.get_crop_type() == GaussianCrop::CropType::Box)
                {
                    const auto& bd_box = crop_data.bouding_box();
                    auto b = bd_box.max();
                    auto a = bd_box.min();

                    using namespace glm;
                    draw_3d_line(drawList, world2proj, m * vec3{ a.x, a.y, a.z }, m * vec3{ a.x, a.y, b.z }, 0xffff4040, sceneViewPosition, sceneViewSize);
                    draw_3d_line(drawList, world2proj, m * vec3{ b.x, a.y, a.z }, m * vec3{ b.x, a.y, b.z }, 0xffffffff, sceneViewPosition, sceneViewSize);
                    draw_3d_line(drawList, world2proj, m * vec3{ a.x, b.y, a.z }, m * vec3{ a.x, b.y, b.z }, 0xffffffff, sceneViewPosition, sceneViewSize);
                    draw_3d_line(drawList, world2proj, m * vec3{ b.x, b.y, a.z }, m * vec3{ b.x, b.y, b.z }, 0xffffffff, sceneViewPosition, sceneViewSize);

                    draw_3d_line(drawList, world2proj, m * vec3{ a.x, a.y, a.z }, m * vec3{ b.x, a.y, a.z }, 0xff4040ff, sceneViewPosition, sceneViewSize); // X
                    draw_3d_line(drawList, world2proj, m * vec3{ a.x, b.y, a.z }, m * vec3{ b.x, b.y, a.z }, 0xffffffff, sceneViewPosition, sceneViewSize);
                    draw_3d_line(drawList, world2proj, m * vec3{ a.x, a.y, b.z }, m * vec3{ b.x, a.y, b.z }, 0xffffffff, sceneViewPosition, sceneViewSize);
                    draw_3d_line(drawList, world2proj, m * vec3{ a.x, b.y, b.z }, m * vec3{ b.x, b.y, b.z }, 0xffffffff, sceneViewPosition, sceneViewSize);

                    draw_3d_line(drawList, world2proj, m * vec3{ a.x, a.y, a.z }, m * vec3{ a.x, b.y, a.z }, 0xff40ff40, sceneViewPosition, sceneViewSize); // Y
                    draw_3d_line(drawList, world2proj, m * vec3{ b.x, a.y, a.z }, m * vec3{ b.x, b.y, a.z }, 0xffffffff, sceneViewPosition, sceneViewSize);
                    draw_3d_line(drawList, world2proj, m * vec3{ a.x, a.y, b.z }, m * vec3{ a.x, b.y, b.z }, 0xffffffff, sceneViewPosition, sceneViewSize);
                    draw_3d_line(drawList, world2proj, m * vec3{ b.x, a.y, b.z }, m * vec3{ b.x, b.y, b.z }, 0xffffffff, sceneViewPosition, sceneViewSize);

                }
                else if (crop_data.get_crop_type() == GaussianCrop::CropType::Sphere)
                {
                    const auto& bd_sphere = crop_data.bouding_sphere();
                    const auto center = model.get_local_position() + bd_sphere.get_center();
                    const auto scale = model.get_local_scale().x;
                    const auto radius = bd_sphere.get_radius() * scale;
                    ImGuizmo::drawSphereWireframe(glm::value_ptr(view), glm::value_ptr(proj), glm::value_ptr(center), radius, sceneViewPosition, sceneViewSize);
                }
            }
        }
    }

    extern auto mesh_pick(DeferedRenderer* renderer,
        std::shared_ptr<rhi::GpuBuffer>& image_mask_buffer,
        std::shared_ptr<rhi::GpuBuffer>& output_pick_buffer,
        const glm::uvec4& surface_dim) -> void;

    extern 	auto uv_map_intersect(DeferedRenderer* renderer,
		std::shared_ptr<rhi::GpuBuffer>& image_mask_buffer,
		std::shared_ptr<rhi::GpuTexture>& uv2pos_texture,
		std::shared_ptr<rhi::GpuBuffer>& output_pick_buffer,
		const glm::mat4& transform,
		const glm::uvec4& surface_dim) -> void;

    void SceneViewPanel::draw_3d_paint_toolbar(const ImVec2& sceneViewPosition, const ImVec2& sceneViewSize)
    {
        if (editor->splat_edit()) return;
        if (Editor::get_editor()->is_texture_paint_active())
        {
            auto entitys = editor->get_selected();
            auto size = item_btn_size + ImVec2(10, 10);
            auto offsetY = sceneViewPosition.y + (sceneViewSize.y - 12 * size.y) / 2.0f;
            ImGui::SetCursorPosY(offsetY);
            auto& edit_type = ImagePaint::get().image_edit_type();
            bool selected = edit_type == ImageEditOpType::Paint;
            if (selected)
                ImGui::PushStyleColor(ImGuiCol_Button, ImGuiHelper::GetSelectedColour());
            if (ImGui::Button(U8CStr2CStr(ICON_MDI_BRUSH), item_btn_size))
            {
                if (entitys.size() > 0)
                {
                    auto entity = entitys.front();
                    auto& registry = editor->get_current_scene()->get_registry();
                    auto model = registry.try_get<MeshModelComponent>(entity);
                    //cached texture
                    if (model)
                        model->ModelRef->get_uv2pos_map(ImagePaint::get().get_paint_texture());
                }
                edit_type = selected ? ImageEditOpType::None : ImageEditOpType::Paint;
            }
            if (selected)
                ImGui::PopStyleColor();
            ImGuiHelper::Tooltip("Paint");
        }
    }

    void SceneViewPanel::handle_3d_paint(const ImVec2& sceneViewPosition, const ImVec2& sceneViewSize)
    {
        auto mouseX = ImGui::GetMousePos().x - sceneViewPosition.x;
        auto entitys = editor->get_selected();

        if (!editor->splat_edit() &&
            Editor::get_editor()->is_texture_paint_active() && 
            entitys.size() > 0 &&
            is_valid_region(sceneViewPosition,sceneViewSize))
        {
            auto entity = entitys.front();
            auto& registry = editor->get_current_scene()->get_registry();
            auto model = registry.try_get<MeshModelComponent>(entity);
            auto transform = registry.try_get<maths::Transform>(entity);

            auto& edit_type = ImagePaint::get().image_edit_type();
            auto& brush_tool = ImagePaint::get().brush();
            auto& paint_texture = ImagePaint::get().get_paint_texture();
            if (edit_type != ImageEditOpType::None && ImGui::IsWindowFocused() && model)
            {
                brush_tool.create_brush_buffer(sceneViewSize.x, sceneViewSize.y);
                glm::uvec2 paint_tex_dim = { paint_texture->desc.extent[0], paint_texture->desc.extent[1] };
                auto pick_buffer = brush_tool.create_pick_buffer(paint_tex_dim.x, paint_tex_dim.y);
                auto drawList = ImGui::GetWindowDrawList();
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                {
                    if (edit_type == ImageEditOpType::Paint)
                    {
                        brush_tool.reset_brush();
                        auto p0 = ImGui::GetMousePos() - sceneViewPosition;
                        brush_tool.brush_points().push_back({ p0.x,p0.y });
                    }
                }
                if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
                {
                    auto p1 = ImGui::GetMousePos() - sceneViewPosition;
                    auto p1_v = glm::ivec2(p1.x, p1.y);
                    if (brush_tool.brush_points().size() > 0 && brush_tool.brush_points().back() != p1_v)
                        brush_tool.brush_points().push_back(p1_v);
                    const int num_points = brush_tool.brush_points().size();
                    auto ptr = brush_tool.brush_points().data();
                    auto paint_color = glm::vec3(1);
                    auto line_color = cv::Scalar(255 * paint_color.x, 255 * paint_color.y, 255 * paint_color.z, 102);
                    cv::polylines(*brush_tool.brush_mask(), (const cv::Point* const*)(&ptr), &num_points, 1, false, line_color, brush_tool.brush_thick_ness(), cv::LINE_AA);
                    brush_tool.update_brush_buffer();

                    auto texID = Application::get().get_imgui_manager()->get_imgui_renderer()->add_texture(brush_tool.brush_texture());
                    drawList->AddImage(texID, sceneViewPosition, sceneViewPosition + sceneViewSize, ImVec2(0, 0), ImVec2(1, 1), ImGui::GetColorU32(ImVec4(1, 1, 1, 1)));
                }
                else
                {
                    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
                    {
                        auto brush_buffer = brush_tool.brush_buffer();
                        auto uv2pos = model->ModelRef->get_uv2pos_map(paint_texture);
                        uv_map_intersect(
                            editor->get_renderer(),
                            brush_buffer, 
                            uv2pos->gpu_texture,
                            pick_buffer, 
                            transform->get_world_matrix(),
                            glm::uvec4(sceneViewSize.x, sceneViewSize.y, paint_tex_dim));
                        std::vector<int> paint_points(paint_tex_dim.x * paint_tex_dim.y, -1);
                        auto device = get_global_device();
                        auto data = pick_buffer->map(device);
                        memcpy(paint_points.data(),data, paint_points.size() * sizeof(int));
                        pick_buffer->unmap(device);
                        ImagePaintColorAdjustment paint_state = {glm::vec3(1,0,0),1.0f};
                        UndoRedoSystem::get().add(std::make_shared<ImagePaintOperation>(paint_texture, paint_state, paint_points));
                    }
                }

                glm::vec4 paint_color = glm::vec4(1);
                auto cirle_color = IM_COL32(255 * paint_color.x, 255 * paint_color.y, 255 * paint_color.z, 102);
                drawList->AddCircleFilled(ImGui::GetMousePos(), brush_tool.brush_thick_ness() / 2.0f, cirle_color);
            }
        }
    }

    void SceneViewPanel::take_photo_dialog()
    {
        const auto width = ImGui::GetWindowWidth();

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("FileName:");
        ImGui::SameLine();
        
        ImGuiHelper::TextCentred(photon_setting.file_path.c_str());
        ImGui::SameLine();
        if (ImGuiHelper::Button("..")) //open file dialog
        {
            photon_setting.file_path = FileDialogs::saveFile({ "jpg","png"});
        }
        ImGui::Separator();

        diverse::ImGuiHelper::PushID();
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
        ImGui::Columns(2);
        ImGui::Separator();

        ImGui::TextUnformatted("resolution");
        ImGui::NextColumn();
        ImGui::PushItemWidth(-1);
        const char* res_str[] = { "1920x1080","1280x960" };
        glm::ivec2 resolution[] = { glm::ivec2(1920,1080),glm::ivec2(1280,960) };
        static int cur_res = 0;
        if (ImGui::BeginCombo("", res_str[cur_res], 0)) // The second parameter is the label previewed before opening the combo.
        {
            for (int n = 0; n < 2; n++)
            {
                bool is_selected = (n == cur_res);
                if (ImGui::Selectable(res_str[n]))
                {
                    cur_res = n;
                }
                if (is_selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::PopItemWidth();
        ImGui::NextColumn();

        photon_setting.resolution = resolution[cur_res];

        ImGuiHelper::Property("Spp",photon_setting.spp);
        ImGui::Columns(1);
        ImGui::Separator();
        ImGui::PopStyleVar();
        diverse::ImGuiHelper::PopID();
        auto button_sizex = 120;
        auto button_posx = ImGui::GetCursorPosX() + (width / 2 - button_sizex) / 2;
        ImGui::SetCursorPosX(button_posx);
        if (ImGui::Button("OK", ImVec2(button_sizex, 0)) && !photon_setting.file_path.empty())
        {
            auto variable = CmadVariableMgr::get().find("r.takePhoton");
            if(variable) variable->set_value<bool>(true);
            variable = CmadVariableMgr::get().find("r.accumulate.spp");
            if(variable) variable->set_value<int>(photon_setting.spp);
            editor->handle_renderer_resize(photon_setting.resolution.x, photon_setting.resolution.y);
            editor->invalidate_pt_state();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        ImGui::SetCursorPosX(button_posx + width / 2);
        if (ImGui::Button("Cancel", ImVec2(button_sizex, 0)))
        {
            ImGui::CloseCurrentPopup();
        }
    }

    void SceneViewPanel::draw_splat_edit_ui(ImVec2 sceneViewPosition,ImVec2 sceneViewSize)
    {
        if (!editor->splat_edit()) return;

        auto splat_ent = Entity(editor->get_current_splat_entt(), editor->get_current_scene());
        if (!splat_ent.valid()) return;
        auto splat_model = splat_ent.get_component<GaussianComponent>().ModelRef;
        auto& splat_edit = GaussianEdit::get();
        int selectedSplats = splat_model->num_selected_gaussians();
        std::string splatText;
        if (selectedSplats >= 1000) {
            float kSplats = selectedSplats / 1000.0f;
            std::stringstream ss;
            ss << std::fixed << std::setprecision(2) << kSplats << " kSplats selected";
            splatText = ss.str();
        }
        else {
            splatText = std::to_string(selectedSplats) + " Splats selected";
        }

        // auto& min_radius = splat_edit.min_radius_ref();
        // auto& max_opacity = splat_edit.max_opacity_ref();
        ImGuiStyle& style = ImGui::GetStyle();
        auto oldWindowMinSize = style.WindowMinSize;
        auto oldWindowRounding = style.WindowRounding;
        style.WindowMinSize = ImVec2(200.0f, 20.0f);
        style.WindowRounding = 4;

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.08f, 1.0f)); // 深色背景
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 8)); // 增加垂直padding使输入框更高
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 2));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 4.0f);

        const int toolbarHeight = 40; // 调整toolbar高度以适应更多UI元素
        const int toolbarWidth = 60 * (selectedSplats > 100 ? 6 : 5);
        // auto posX = sceneViewPosition.x + sceneViewSize.x /2.0f - (toolbarWidth / 2.0f);
        // ImGui::SetNextWindowPos(ImVec2(posX,sceneViewPosition.y + 10), ImGuiCond_Always);
        // ImGui::SetNextWindowSize(ImVec2(toolbarWidth, toolbarHeight), ImGuiCond_Always);

        // ImGui::Begin("Toolbar UI", nullptr,
        //     ImGuiWindowFlags_NoResize |
        //     ImGuiWindowFlags_NoCollapse |
        //     ImGuiWindowFlags_NoMove |
        //     ImGuiWindowFlags_NoTitleBar |
        //     ImGuiWindowFlags_NoScrollbar);

        // if (ImGui::Button("Clear", ImVec2(60, 30))) {
        //     splat_edit.add_select_none_op();
        // }

        // ImGui::SameLine();
        // if (ImGui::Button("Invert", ImVec2(60, 30))) {
        //     splat_edit.add_select_inverse_op();
        // }

        // ImGui::SameLine();
        // if (ImGui::Button("All", ImVec2(60, 30))) {
        //     splat_edit.add_select_all_op();
        // }

        // ImGui::SameLine();

        // // 3. Min Radius输入框
        // ImGui::AlignTextToFramePadding();
        // ImGui::TextUnformatted("Min Radius");
        // ImGui::SameLine();
        // ImGui::PushItemWidth(50);
        // ImGui::DragFloat("##minRadius", &min_radius, 0.1f);
        // ImGui::PopItemWidth();

        // ImGui::SameLine();
        // ImGui::Spacing();
        // ImGui::SameLine();

        // // 4. Max Opacity输入框
        // ImGui::AlignTextToFramePadding();
        // ImGui::TextUnformatted("Max Opacity : ");
        // ImGui::SameLine();
        // ImGui::PushItemWidth(50);
        // ImGui::DragFloat("##maxOpacity", &max_opacity, 0.01f);
        // ImGui::PopItemWidth();

        // ImGui::SameLine();
        // ImGui::Spacing();
        // ImGui::SameLine();
        // ImGui::AlignTextToFramePadding();
        // ImGui::TextUnformatted(splatText.c_str());
        // ImGui::End();

        auto editType = splat_edit.get_edit_type();
        if(editType == GaussianEdit::EditType::Sphere || editType == GaussianEdit::EditType::Box)
        {
            const int btnSize = 60;
            const int toolbarWidth = btnSize * 3 + 60 * (editType == GaussianEdit::EditType::Sphere ? 2 : 6);
            auto posX = sceneViewPosition.x + sceneViewSize.x /2.0f - (toolbarWidth / 2.0f);
            ImGui::SetNextWindowPos(ImVec2(posX,sceneViewPosition.y + sceneViewSize.y - toolbarHeight), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(toolbarWidth, toolbarHeight), ImGuiCond_Always);
    
            ImGui::Begin("Shape Toolbar", nullptr,
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_NoScrollbar);

            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5);

            if (ImGui::Button("Set", ImVec2(60, 30))) {
                splat_edit.reset_splat_intersected();
                splat_edit.intersect_splat(EditSelectOpType::Set);
                splat_edit.add_selection_op(EditSelectOpType::Set);
            }
    
            ImGui::SameLine();
            if (ImGui::Button("Add", ImVec2(60, 30))) {
                splat_edit.reset_splat_intersected();
                splat_edit.intersect_splat(EditSelectOpType::Add);
                splat_edit.add_selection_op(EditSelectOpType::Add);
            }
    
            ImGui::SameLine();
            if (ImGui::Button("Remove", ImVec2(60, 30))) {
                splat_edit.reset_splat_intersected();
                splat_edit.intersect_splat(EditSelectOpType::Remove);
                splat_edit.add_selection_op(EditSelectOpType::Remove);
            }
            ImGui::SameLine();

            ImGui::Spacing();
            ImGui::SameLine();
            if(editType == GaussianEdit::EditType::Sphere)
            {
                // 3. Radius输入框
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted("Radius");
                ImGui::SameLine();
                ImGui::PushItemWidth(55);
                ImGui::DragFloat("##Radius", &splat_edit.bouding_sphere().radius(), 0.1f, 0, 400.0f);
                ImGui::PopItemWidth();

                ImGui::SameLine();
                ImGui::Spacing();
                ImGui::SameLine();
            }
            else if(editType == GaussianEdit::EditType::Box)
            {
                //XScale,YScale,ZScale输入框
                bool modify = false;
                auto& edit_transform = splat_edit.get_edit_transform();
                auto scale = edit_transform.get_local_scale();
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted("XScale");
                ImGui::SameLine();
                ImGui::PushItemWidth(55);
                if (ImGui::DragFloat("##XScale", &scale.x, 0.1f, 0, 400.0f))
                    modify = true;
                ImGui::PopItemWidth();

                ImGui::SameLine();
                ImGui::Spacing();
                ImGui::SameLine();
                ImGui::TextUnformatted("YScale");
                ImGui::SameLine();
                ImGui::PushItemWidth(55);
                if(ImGui::DragFloat("##YScale", &scale.y, 0.1f, 0, 400.0f))
                    modify = true;
                ImGui::PopItemWidth();
 
                ImGui::SameLine();
                ImGui::Spacing();
                ImGui::SameLine();
                ImGui::TextUnformatted("ZScale");
                ImGui::SameLine();
                ImGui::PushItemWidth(55);
                if(ImGui::DragFloat("##ZScale", &scale.z, 0.1f, 0, 400.0f))
                    modify = true;
                ImGui::PopItemWidth();
                if (modify)
                    edit_transform.set_local_scale(scale);
            }
            ImGui::End();
        }
        ImGui::PopStyleVar(4);
        ImGui::PopStyleColor(1);
        style.WindowMinSize = oldWindowMinSize;
        style.WindowRounding = oldWindowRounding;
    }

    void SceneViewPanel::draw_shade_toolbar(const ImVec2& sceneViewPosition, const ImVec2& sceneViewSize)
    {
        ImGuiStyle& style = ImGui::GetStyle();
        auto oldWindowMinSize = style.WindowMinSize;
        auto oldWindowRounding = style.WindowRounding;
        style.WindowMinSize = ImVec2(200.0f, 20.0f);
        style.WindowRounding = 4;
        const int toolbarHeight = 40; // 调整toolbar高度以适应更多UI元素
        auto posX = sceneViewPosition.x + sceneViewSize.x /2.0f - (600 / 2.0f);
        ImGui::SetNextWindowPos(ImVec2(posX,sceneViewPosition.y + sceneViewSize.y - toolbarHeight), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(600, toolbarHeight), ImGuiCond_Always);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.08f, 1.0f)); // 深色背景
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 8)); // 增加垂直padding使输入框更高
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 2));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 4.0f);

        ImGui::Begin("Shade Toolbar", nullptr,
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoScrollbar);

        ImGui::End();
        style.WindowMinSize = oldWindowMinSize;
        style.WindowRounding = oldWindowRounding;
        ImGui::PopStyleVar(4);
        ImGui::PopStyleColor(1);
    }

    void SceneViewPanel::render_frame_image()
    {
        auto variable = CmadVariableMgr::get().find("r.takePhoton");
        if (variable)
        {
            auto v = variable->get_value<bool>();
            if(!v) return;
        }
        if (photon_setting.frame_counter == 0)
        {
            ImGui::OpenPopup("Render Image");
        }
        f32 progress = static_cast<f32>(photon_setting.frame_counter) / static_cast<f32>(photon_setting.spp);
        if (ImGuiHelper::ProgressWindow("Render Image", progress, ImVec2(240,0), true))
        {
            reset_render_frame();
        }
        if (++photon_setting.frame_counter % photon_setting.spp == 0)
        {
            auto renderTarget = editor->get_main_render_texture();
            auto res = glm::ivec2(renderTarget->desc.extent[0], renderTarget->desc.extent[1]);
            auto img_data = renderTarget->export_texture();
            write_stbi(photon_setting.file_path, res.x, res.y, 4, (uint8_t*)img_data.data(), 100);
            reset_render_frame();
        }
    }

    void SceneViewPanel::reset_render_frame()
    {
        photon_setting.frame_counter = 0;
        auto variable = CmadVariableMgr::get().find("r.takePhoton");
        if (variable) variable->set_value<bool>(false);
        variable = CmadVariableMgr::get().find("r.accumulate.spp");
        if (variable) variable->set_value<int>(1);
        editor->handle_renderer_resize(width, height);
    }
    
    bool SceneViewPanel::is_valid_region(const ImVec2& sceneViewPosition, const ImVec2& sceneViewSize) const
    {
        auto size = item_btn_size + ImVec2(10, 10);
        auto mouse_view_pos = (ImGui::GetMousePos() - sceneViewPosition);
        bool valid_region = mouse_view_pos.x >= size.x && mouse_view_pos.y < (sceneViewSize.y - 10) && mouse_view_pos.x < (sceneViewSize.x - 10) && mouse_view_pos.y > 10 ;
        return valid_region;
    }

    bool SceneViewPanel::handle_mouse_move(MouseMovedEvent& e)
    {
        return true;
    }
    bool SceneViewPanel::handle_mouse_pressed(MouseButtonPressedEvent& e)
    {
        if (e.GetMouseButton() == InputCode::MouseKey::ButtonLeft)
        {      
        }
        return true;
    }

    bool SceneViewPanel::handle_mouse_released(MouseButtonReleasedEvent& e)
    {

        return true;
    }
}
