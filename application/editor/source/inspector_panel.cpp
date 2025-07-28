#include "inspector_panel.h"
#include "editor.h"
#include "file_browser_panel.h"
#include "gaussian_edit.h"
#include "texture_paint_panel.h"
#include <engine/application.h>
#include <core/string.h>
#include <utility/file_utils.h>
#include <engine/file_system.h>
#include <scene/entity_manager.h>
#include <scene/scene_manager.h>
#include <scene/component/mesh_model_component.h>
#include <scene/component/gaussian_component.h>
#include <scene/component/point_cloud_component.h>
#include <scene/component/gaussian_crop.h>
#include <scene/component/environment.h>
#include <scene/component/light/directional_light.h>
#include <scene/component/light/point_light.h>
#include <scene/component/light/rect_light.h>
#include <scene/component/light/spot_light.h>
#include <scene/component/light/disk_light.h>
#include <scene/component/light/cylinder_light.h>
#include <scene/sun_controller.h>
#include <renderer/render_settings.h>
#include <assets/asset_manager.h>
#include <imgui/IconsMaterialDesignIcons.h>
#include <imgui/imgui_manager.h>
#include <imgui/imgui_renderer.h>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <glm/gtx/matrix_decompose.hpp>

#include <cereal/types/polymorphic.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>

#include <sol/sol.hpp>
#include <inttypes.h>
#ifdef DS_SPLAT_TRAIN
#include <gaussian_trainer_scene.hpp>
#endif
#include <spdlog/fmt/bundled/format.h>
#include <utility/process_utils.hpp>
#include "pivot.h"
namespace MM
{
#define PropertySet(name, getter, setter)                \
    {                                                    \
        auto value = getter();                           \
        if(diverse::ImGuiHelper::Property(name, value)) \
            setter(value);                               \
    }

    template <>
    void ComponentEditorWidget<diverse::maths::Transform>(entt::registry& reg, entt::registry::entity_type e)
    {
        DS_PROFILE_FUNCTION();
        auto& transform = diverse::Editor::get_editor()->get_pivot()->get_transform();
        auto gaussian = reg.try_get<diverse::GaussianComponent>(e);
        static diverse::maths::Transform old_transform = transform;
        glm::vec3 rotation = glm::degrees(transform.get_local_orientation());
        auto position = transform.get_local_position();
        auto scale = transform.get_local_scale();
        float itemWidth = (ImGui::GetContentRegionAvail().x - (ImGui::GetFontSize() * 3.0f)) / 3.0f;
        bool dirty = false;

        if (diverse::ImGuiHelper::PropertyVector3("Position", position, itemWidth, 0.0f))
            dirty |= true;
            
        if (diverse::ImGuiHelper::PropertyVector3("Rotation", rotation, itemWidth, 0.0f))
            dirty |= true;

        if (diverse::ImGuiHelper::PropertyVector3("Scale", scale, itemWidth, 1.0f))
            dirty |= true;
        if (dirty)
        {
            old_transform = transform;
            transform.set_local_position(position);
            transform.set_local_orientation(glm::radians(glm::vec3(rotation.x, rotation.y, rotation.z)));
            transform.set_local_scale(scale);
            transform.set_world_matrix(glm::mat4(1.0f));
        }
        auto& gs_edit = diverse::GaussianEdit::get();
        if (!gs_edit.has_select_gaussians() && dirty)
        {
            auto& ent_transform = reg.get<diverse::maths::Transform>(e);
            auto delta_matrix = transform.get_world_matrix() * glm::inverse(old_transform.get_world_matrix());
            ent_transform.set_local_transform(delta_matrix * ent_transform.get_local_matrix());
        }
        if(gaussian && dirty)
        {
            if ( gaussian->ModelRef)
            {
                // gs_edit.start_transform_op(transform);
                // gs_edit.update_transform_op(old_transform, transform);
                // gs_edit.end_transform_op(old_transform, transform, diverse::Editor::get_editor()->get_pivot());
            }
        }
        ImGui::Columns(1);
        ImGui::Separator();
    }

    void TextureWidget(const char* label,const std::shared_ptr<diverse::rhi::GpuTexture>& tex, bool flipImage, const std::function<void(const std::string&)>& callback, const ImVec2& imageButtonSize = ImVec2(64, 64), bool is_hdr = false)
    {
        using namespace diverse;
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
        //ImGui::BeginColumns("TextureWidget", 1, ImGuiOldColumnFlags_NoResize);
        //ImGui::SetColumnWidth(0, imageButtonSize.x + 10.0f);
        ImGui::Columns(1);
        ImGui::Separator();
        ImGui::AlignTextToFramePadding();

        auto ButtonPos = ImGui::GetCursorPos();
        ButtonPos.x = (ImGui::GetWindowWidth() - imageButtonSize.x) / 2;
        ImGui::SetCursorPos(ButtonPos);
        const ImGuiPayload* payload = ImGui::GetDragDropPayload();
        auto min = ImGui::GetCurrentWindow()->DC.CursorPos;
        auto max = min + imageButtonSize + ImGui::GetStyle().FramePadding;
        bool hoveringButton = ImGui::IsMouseHoveringRect(min, max, false);
        bool showTexture = !(hoveringButton && (payload != NULL && payload->IsDataType("AssetFile")));
        static std::string file_path;
        if (tex && showTexture)
        {
            if (ImGui::ImageButton((const char*)(tex.get()), reinterpret_cast<ImTextureID>(Application::get().get_imgui_manager()->get_imgui_renderer()->add_texture(tex)), imageButtonSize, ImVec2(0.0f, flipImage ? 1.0f : 0.0f), ImVec2(1.0f, flipImage ? 0.0f : 1.0f)))
            {
                if (is_hdr) 
                {
                    file_path = FileDialogs::openFile({ "exr", "hdr" }).first;
                }
                else
                    file_path = FileDialogs::openFile({ "jpg","png","tga" }).first;
                if(!file_path.empty())
                {
                    callback(file_path);
                }
            }

            if (ImGui::IsItemHovered() && tex)
            {
                ImGui::BeginTooltip();
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
                ImGui::PopTextWrapPos();
                ImGui::Image(reinterpret_cast<ImTextureID>(Application::get().get_imgui_manager()->get_imgui_renderer()->add_texture(tex)), imageButtonSize * 3.0f, ImVec2(0.0f, flipImage ? 1.0f : 0.0f), ImVec2(1.0f, flipImage ? 0.0f : 1.0f));
                ImGui::EndTooltip();
            }
        }
        else
        {
            //TODO:
            static auto gray_tex = Environment::create_gray_tex();
            if (ImGui::ImageButton("Empty", reinterpret_cast<ImTextureID>(Application::get().get_imgui_manager()->get_imgui_renderer()->add_texture(gray_tex)), imageButtonSize, ImVec2(0.0f, flipImage ? 1.0f : 0.0f), ImVec2(1.0f, flipImage ? 0.0f : 1.0f)))
            {
                if (is_hdr)
                {
                    file_path = FileDialogs::openFile({ "exr", "hdr"}).first;
                }
                else
                    file_path = FileDialogs::openFile({ "jpg","png","tga" }).first;
                if (!file_path.empty())
                {
                    callback(file_path);
                }
            }
        }

        if (payload != NULL && payload->IsDataType("AssetFile"))
        {
            auto filePath = std::string(reinterpret_cast<const char*>(payload->Data));
            if (diverse::Editor::get_editor()->is_image_file(filePath))
            {
                if (ImGui::BeginDragDropTarget())
                {
                    // Drop directly on to node and append to the end of it's children list.
                    if (ImGui::AcceptDragDropPayload("AssetFile"))
                    {
                        callback(filePath);
                        ImGui::EndDragDropTarget();

                        ImGui::Columns(1);
                        ImGui::Separator();
                        ImGui::PopStyleVar();
                        return;
                    }

                    ImGui::EndDragDropTarget();
                }
            }
        }
        else
        {
            auto& dropped_files = diverse::Editor::get_editor()->dropped_files;
            if (ImGui::IsItemHovered() && dropped_files.size() == 1)
            {
                file_path = dropped_files[0];
                if (!file_path.empty() && diverse::Editor::get_editor()->is_image_file(file_path))
                    callback(file_path);
                dropped_files.clear();
            }
        }
        ImGui::NextColumn();
        ImGui::Columns(2);

        ImGui::Separator();
        ImGui::PopStyleVar();
    }

    void TextureWidget(const char* label,diverse::Material* material, const diverse::asset::Texture* tex, bool flipImage,const std::function<void(const std::string&)>& callback, const ImVec2& imageButtonSize = ImVec2(64, 64), int propIndex = 0, bool defaultOpen = false)
    {
        using namespace diverse;
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Framed;
        if (defaultOpen)
            flags |= ImGuiTreeNodeFlags_DefaultOpen;
        bool dirty = false;
        if (ImGui::TreeNodeEx(label, flags))
        {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
            // ImGui::Columns(2);
            ImGui::BeginColumns("TextureWidget", 2, ImGuiOldColumnFlags_NoResize);
            ImGui::SetColumnWidth(0, imageButtonSize.x + 10.0f);

            ImGui::Separator();

            ImGui::AlignTextToFramePadding();

            const ImGuiPayload* payload = ImGui::GetDragDropPayload();
            auto min = ImGui::GetCurrentWindow()->DC.CursorPos;
            auto max = min + imageButtonSize + ImGui::GetStyle().FramePadding;

            bool hoveringButton = ImGui::IsMouseHoveringRect(min, max, false);
            bool showTexture = !(hoveringButton && (payload != NULL && payload->IsDataType("AssetFile")));
            if (tex && showTexture)
            {
                if (ImGui::ImageButton((const char*)(tex->gpu_texture.get()), reinterpret_cast<ImTextureID>(Application::get().get_imgui_manager()->get_imgui_renderer()->add_texture(tex->gpu_texture)), imageButtonSize, ImVec2(0.0f, flipImage ? 1.0f : 0.0f), ImVec2(1.0f, flipImage ? 0.0f : 1.0f)))
                {
                    auto [file_path,_] = FileDialogs::openFile({ "jpg","png","tga" });
                    if (!file_path.empty())
                    {
                        callback(file_path);
                        dirty = true;
                    }
                }
             
                if (ImGui::IsItemHovered() && tex)
                {
                    ImGui::BeginTooltip();
                    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
                    ImGui::TextUnformatted(tex->file_path.c_str());
                    ImGui::PopTextWrapPos();
                    ImGui::Image(reinterpret_cast<ImTextureID>(Application::get().get_imgui_manager()->get_imgui_renderer()->add_texture(tex->gpu_texture)), imageButtonSize * 3.0f, ImVec2(0.0f, flipImage ? 1.0f : 0.0f), ImVec2(1.0f, flipImage ? 0.0f : 1.0f));
                    ImGui::EndTooltip();
                }
            }
            else
            {
                static auto gray_tex = Environment::create_gray_tex();
                if (ImGui::ImageButton("Empty", reinterpret_cast<ImTextureID>(Application::get().get_imgui_manager()->get_imgui_renderer()->add_texture(gray_tex)), imageButtonSize, ImVec2(0.0f, flipImage ? 1.0f : 0.0f), ImVec2(1.0f, flipImage ? 0.0f : 1.0f)))
                {
                
                    auto  [file_path,_] = FileDialogs::openFile({ "jpg","png","tga" });
                    if (!file_path.empty())
                    {
                        callback(file_path);
                        dirty = true;
                    }
                }
            }

            if (payload != NULL && payload->IsDataType("AssetFile"))
            {
                auto filePath = std::string(reinterpret_cast<const char*>(payload->Data));
                if (diverse::Editor::get_editor()->is_image_file(filePath))
                {
                    if (ImGui::BeginDragDropTarget())
                    {
                        // Drop directly on to node and append to the end of it's children list.
                        if (ImGui::AcceptDragDropPayload("AssetFile"))
                        {
                            callback(filePath);
                            ImGui::EndDragDropTarget();

                            ImGui::Columns(1);
                            ImGui::Separator();
                            ImGui::PopStyleVar();

                            ImGui::TreePop();
                            return;
                        }

                        ImGui::EndDragDropTarget();
                    }
                }
            }
            else
            {
                auto& dropped_files = diverse::Editor::get_editor()->dropped_files;
                if (ImGui::IsItemHovered() && dropped_files.size() == 1)
                {
                    auto file_path = dropped_files[0];
                    if (!file_path.empty() && diverse::Editor::get_editor()->is_image_file(file_path))
                        callback(file_path);
                    dropped_files.clear();
                }
            }
            ImGui::NextColumn();

           /* if (tex)
            {
                ImGui::Text("%u x %u", tex->extent[0], tex->extent[1]);
            }*/
            auto editor = Editor::get_editor();
            auto EditTexture = [&]() {
              /*  if (ImGui::Selectable("Paint"))
                {
                    editor->texture_paint_Panel->set_active(true);
                    if(tex)
                        editor->texture_paint_Panel->set_paint_texture(tex->gpu_texture);
                }
                else if(ImGui::Selectable("Create"))
                {

                }
                else if (ImGui::Selectable("Replace"))
                {
                    auto [file_path,_] = FileDialogs::openFile({ "jpg","png","tga" });
                    if (!file_path.empty())
                    {
                        callback(file_path);
                        dirty = true;
                    }
                }
                else if (ImGui::Selectable("Remove"))
                {

                }*/
            };
            diverse::MaterialProperties* prop = &material->get_properties();
            if (propIndex == 0) //albedo
            {
                dirty |= ImGui::ColorEdit4(diverse::ImGuiHelper::GenerateLabelID("Colour"), glm::value_ptr(prop->albedoColour));
            }
            else if(propIndex == 1) //normal
            {
                auto cursorPos = ImGui::GetCursorPosY();
                ImGui::SetCursorPosY(cursorPos + imageButtonSize.y / 3);
                dirty |= ImGui::SliderFloat(diverse::ImGuiHelper::GenerateLabelID("Use Map"), &prop->normal_map_factor, 0.0f, 1.0f);
            }
            else if (propIndex == 2) //metallic
            {
                if (prop->work_flow == PBR_WORKFLOW_METALLIC_ROUGHNESS)
                {
                    dirty |= ImGui::SliderFloat(diverse::ImGuiHelper::GenerateLabelID("MetallicMapFactor"), &prop->metallic_map_factor, 0.0f, 1.0f);
                    dirty |= ImGui::SliderFloat(diverse::ImGuiHelper::GenerateLabelID("Metallic"), &prop->metalness, 0.0f, 1.0f);

                    dirty |= ImGui::SliderFloat(diverse::ImGuiHelper::GenerateLabelID("RoughnessMapFactor"), &prop->roughness_map_factor, 0.0f, 1.0f);
                    dirty |= ImGui::SliderFloat(diverse::ImGuiHelper::GenerateLabelID("Roughness"), &prop->roughness, 0.0f, 1.0f);
                }
                else
                {
                    dirty |= ImGui::SliderFloat(diverse::ImGuiHelper::GenerateLabelID("Use Map"), &prop->metallic_map_factor, 0.0f, 1.0f);
                    dirty |= ImGui::SliderFloat(diverse::ImGuiHelper::GenerateLabelID("Value"), &prop->metalness, 0.0f, 1.0f);
                }
            }
            else if (propIndex == 3) //roughness
            {
                dirty |= ImGui::SliderFloat(diverse::ImGuiHelper::GenerateLabelID("Use Map"), &prop->roughness_map_factor, 0.0f, 1.0f);
                dirty |= ImGui::SliderFloat(diverse::ImGuiHelper::GenerateLabelID("Value"), &prop->roughness, 0.0f, 1.0f);
            }
            else if (propIndex == 4) //ao
            {
                dirty |= ImGui::SliderFloat(diverse::ImGuiHelper::GenerateLabelID("Use Map"), &prop->ao_map_factor, 0.0f, 1.0f);
                dirty |= ImGui::SliderFloat(diverse::ImGuiHelper::GenerateLabelID("Value"), &prop->ao, 0.0f, 1.0f);
            }
            else if (propIndex == 5) //emissive
            {
                dirty |= ImGui::SliderFloat(diverse::ImGuiHelper::GenerateLabelID("Use Map"), &prop->emissive_map_factor, 0.0f, 1.0f);
                dirty |= ImGui::ColorEdit3(diverse::ImGuiHelper::GenerateLabelID("Colour"), glm::value_ptr(prop->emissive));
            }
            if (ImGui::BeginPopupContextWindow())
            {
                EditTexture();

                ImGui::EndPopup();
            }
            material->dirty_flag() = dirty;
            ImGui::Columns(1);

            ImGui::Separator();
            ImGui::PopStyleVar();

            ImGui::TreePop();
        }
    }

    template <>
    void ComponentEditorWidget<diverse::Environment>(entt::registry& reg, entt::registry::entity_type e)
    {
        DS_PROFILE_FUNCTION();
        using namespace diverse;
        auto& environment = reg.get<diverse::Environment>(e);
        // Disable image until texturecube is supported
        bool dirty = false;
        glm::vec3 colour = environment.get_color();
        ImGui::PushItemWidth(-1);

        ImGui::Columns(2);
        ImGui::TextUnformatted("Mode");
        ImGui::NextColumn();
        ImGui::PushItemWidth(-1);
        auto& mode = environment.mode;
        const char* names[] = { "PureColor", "SunSky" , "HDR"};
        auto name_current = names[(int)mode];
        if (ImGui::BeginCombo("environment", name_current, 0)) // The second parameter is the label previewed before opening the combo.
        {
            for (int n = 0; n < 3; n++)
            {
                bool is_selected = strcmp(name_current, names[n]) == 0;
                if (ImGui::Selectable(names[n], name_current))
                {
                    environment.mode = (diverse::Environment::Mode)n;
                    dirty = true;
                }
                if (is_selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::PopItemWidth();
        ImGui::NextColumn();
        if ( mode == diverse::Environment::Mode::SunSky)
        {
            ImGuiHelper::Property("SunSize", environment.sun_size_multiplier,0.0f,100.0f);

        }
        else if (mode == diverse::Environment::Mode::HDR)
        {
            auto window_width = ImGui::GetWindowWidth();
            glm::vec2 texture_size = glm::vec2(window_width * 0.98, window_width * 0.4);
            if (!environment.get_enviroment_map())
            {
                static auto gray_hdr = Environment::create_gray_tex();
                environment.set_enviroment_map(gray_hdr);
            }
            TextureWidget("", environment.get_enviroment_map(), false, std::bind(&diverse::Environment::load_hdr, &environment, std::placeholders::_1), texture_size,true);
		}
        if (mode != diverse::Environment::Mode::Pure)
        {
            dirty |= ImGuiHelper::Property("Intensity", environment.intensity,0.0f,100.0f);
            bool ret = ImGuiHelper::Property("Theta", environment.theta,0.0f,90.0f,1.0f, ImGuiHelper::PropertyFlag::SliderValue);
            ret |= ImGuiHelper::Property("Phi", environment.phi, 0.0f, 360.0f, 1.0f, ImGuiHelper::PropertyFlag::SliderValue);
            if (mode == diverse::Environment::Mode::SunSky)
            {
                auto& sun_controller = reg.get_or_emplace<diverse::SunController>(e);
                if( ret )
                    sun_controller.calculate_towards_sun_from_theta_phi(glm::radians(environment.theta), glm::radians(environment.phi));
            }
            dirty |= ret;
            environment.dirty_flag = dirty;
        }
        ImGui::Columns(2);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Ambient");
        ImGui::NextColumn();
        ImGui::PushItemWidth(-1);
        if (ImGui::ColorEdit3("Ambient", glm::value_ptr(colour)))
            environment.set_color(colour);

        ImGui::PopItemWidth();
        ImGui::NextColumn();
        if (mode == diverse::Environment::Mode::SunSky)
        {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Skycolor");
            ImGui::NextColumn();
            ImGui::PushItemWidth(-1);
            ImGui::ColorEdit3("Skycolor", glm::value_ptr(environment.sun_color));
            ImGui::PopItemWidth();
        }
        ImGui::Columns(1);
        ImGui::PopItemWidth();
        ImGui::Separator();
    }

    template <> 
    void ComponentEditorWidget<diverse::RectLightComponent>(entt::registry& reg, entt::registry::entity_type e)
    {                                                                                          
        using namespace diverse;                                                            
        auto& light = reg.get<RectLightComponent>(e);                                                          
        ImGui::PushItemWidth(-1);                                                            
        ImGui::Columns(2);                                                                         
        ImGuiHelper::Property("Intensity", light.get_intensity_ref(), 0.0f, 100.0f);    
        ImGuiHelper::Property("Width", light.width_ref(), 0.01f, 100.0f, 1.0f);
        ImGuiHelper::Property("Height", light.height_ref(), 0.01f, 100.0f, 1.0f);
        // ImGuiHelper::Property("Radius", light.radius_ref(), 0.01f, 100.0f, 1.0f);
        ImGuiHelper::Property("Color", light.get_radiance_ref(), ImGuiHelper::PropertyFlag::ColourProperty);
        ImGui::Columns(1);
        ImGui::PopItemWidth();    
        ImGui::Separator();
    }      
    
    template <>
    void ComponentEditorWidget<diverse::DiskLightComponent>(entt::registry& reg, entt::registry::entity_type e)
    {
        using namespace diverse;
        auto& light = reg.get<DiskLightComponent>(e);
        ImGui::PushItemWidth(-1);
        ImGui::Columns(2);
        ImGuiHelper::Property("Intensity", light.get_intensity_ref(), 0.0f, 100.0f);
        ImGuiHelper::Property("Radius", light.radius_ref(), 0.0f, 100.0f, 0.1f);
        ImGuiHelper::Property("Color", light.get_radiance_ref(), ImGuiHelper::PropertyFlag::ColourProperty);
        ImGui::Columns(1);
        ImGui::PopItemWidth();
        ImGui::Separator();
    }

    template <>
    void ComponentEditorWidget<diverse::CylinderLightComponent>(entt::registry& reg, entt::registry::entity_type e)
    {
        using namespace diverse;
        auto& light = reg.get<CylinderLightComponent>(e);
        ImGui::PushItemWidth(-1);
        ImGui::Columns(2);
        ImGuiHelper::Property("Intensity", light.get_intensity_ref(), 0.0f, 100.0f);
        ImGuiHelper::Property("Radius", light.radius_ref(), 0.01f, 100.0f, 0.1f);
        ImGuiHelper::Property("Length", light.length_ref(), 0.1f, 100.0f, 0.1f);
        ImGuiHelper::Property("Color", light.get_radiance_ref(), ImGuiHelper::PropertyFlag::ColourProperty);
        ImGui::Columns(1);
        ImGui::PopItemWidth();
        ImGui::Separator();
    }

    template <>
    void ComponentEditorWidget<diverse::SpotLightComponent>(entt::registry& reg, entt::registry::entity_type e)
    {
        using namespace diverse;
        auto& light = reg.get<SpotLightComponent>(e);
        ImGui::PushItemWidth(-1);
        ImGui::Columns(2);
        ImGuiHelper::Property("Intensity", light.get_intensity_ref(), 0.0f, 100.0f);
        ImGuiHelper::Property("Color", light.get_radiance_ref(), ImGuiHelper::PropertyFlag::ColourProperty);
        ImGuiHelper::Property("InnerAngle", light.inner_angle_ref(), 0.0f, 360.0f);
        ImGuiHelper::Property("OuterAngle", light.outer_angle_ref(), 0.0f, 360.0f);
        ImGuiHelper::Property("Radius", light.radius_ref(), 0.01f, 100.0f);
        ImGui::Columns(1);
        ImGui::PopItemWidth();
        ImGui::Separator();
    }

    template <>
    void ComponentEditorWidget<diverse::PointLightComponent>(entt::registry& reg, entt::registry::entity_type e)
    {
        using namespace diverse;
        auto& light = reg.get<PointLightComponent>(e);
        ImGui::PushItemWidth(-1);
        ImGui::Columns(2);
        ImGuiHelper::Property("Intensity", light.get_intensity_ref(), 0.0f, 100.0f);
        ImGuiHelper::Property("Color", light.get_radiance_ref(), ImGuiHelper::PropertyFlag::ColourProperty);
        ImGuiHelper::Property("Radius", light.radius, 0.0f,100.0f);
        ImGui::Columns(1);
        ImGui::PopItemWidth();
        ImGui::Separator();
    }
    
    template <>
    void ComponentEditorWidget<diverse::DirectionalLightComponent>(entt::registry& reg, entt::registry::entity_type e)
    {
        using namespace diverse;
        auto& light = reg.get<DirectionalLightComponent>(e);
        ImGui::PushItemWidth(-1);
        ImGui::Columns(2);
        ImGuiHelper::Property("Intensity", light.get_intensity_ref(), 0.0f, 100.0f);
        ImGuiHelper::Property("Color", light.get_radiance_ref(), ImGuiHelper::PropertyFlag::ColourProperty);
        ImGui::Columns(1);
        ImGui::PopItemWidth();
        ImGui::Separator();
    }

    template <>
    void ComponentEditorWidget<diverse::GaussianCrop>(entt::registry& reg, entt::registry::entity_type e)
    {
        DS_PROFILE_FUNCTION();
        diverse::ImGuiHelper::PushID();
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
        ImGui::Columns(2);
        ImGui::Separator();
        auto& gaussianCrop = reg.get<diverse::GaussianCrop>(e);
        auto& crop_datas = gaussianCrop.get_crop_data();
        using namespace diverse;
        if(crop_datas.size() == 0)
        {
            crop_datas.push_back(diverse::GaussianCrop::GaussianCropData());
            auto& crop_data = crop_datas.back();
            auto& gaussian = reg.get<diverse::GaussianComponent>(e);
            if (crop_data.get_crop_type() == diverse::GaussianCrop::CropType::Box)
            {
                auto box = gaussian.ModelRef->get_world_bounding_box(glm::mat4(1.0f));
                crop_data.bdbox = box;
                auto c = box.center();
                auto dialog = glm::length(box.get_extents());
                crop_data.bdsphere = maths::BoundingSphere(c, dialog / 2.0f);
            }
        }
        ImGui::Indent();
        ImGui::Columns(3);

        //ImGui::NextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Crops");
        ImGui::NextColumn();
        if (ImGui::Button("+") && crop_datas.size() < 8)
        {
            crop_datas.push_back(diverse::GaussianCrop::GaussianCropData());
        }

        ImGui::NextColumn();
        if (crop_datas.size() > 0 && ImGui::Button("-"))
        {
            if(crop_datas.size() == 1)
            {
                messageBox("warn", "there must be at least one crop");
            }
            crop_datas.pop_back();
        }
        ImGui::NextColumn();

        for (auto i = 0; i < crop_datas.size(); i++)
        {
            const char* name[8] = {"Crop0","Crop1","Crop2","Crop3", "Crop4","Crop5","Crop6","Crop7"};
            ImGui::Separator();
            ImGui::Columns(1);
            if (ImGui::TreeNodeEx(name[i], ImGuiTreeNodeFlags_Framed))
            { 
                ImGui::Columns(2);

                auto& crop_data = crop_datas[i];
                auto cropType = crop_data.get_crop_type();
                ImGui::TextUnformatted("Crop Type");

                ImGui::NextColumn();
                ImGui::PushItemWidth(-1);

                const char* shapes[] = { "Box", "Sphere" };
                auto shape_current = shapes[(int)cropType];
                if (ImGui::BeginCombo(name[i], shape_current, 0)) // The second parameter is the label previewed before opening the combo.
                {
                    for (int n = 0; n < 2; n++)
                    {
                        bool is_selected = strcmp(shape_current, shapes[n]) == 0;
                        if (ImGui::Selectable(shapes[n], shape_current))
                        {
                            crop_data.set_crop_type((diverse::GaussianCrop::CropType)n);
                        }
                        if (is_selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                ImGui::PopItemWidth();
                ImGui::NextColumn();

                if (crop_data.get_crop_type() == diverse::GaussianCrop::CropType::Box)
                {
                    auto& bdbox = crop_data.bouding_box();
                    ImGuiHelper::Property("min", bdbox.m_Min);
                    ImGuiHelper::Property("max", bdbox.m_Max);
                }
                else if (crop_data.get_crop_type() == diverse::GaussianCrop::CropType::Sphere)
                {
                    auto& bdsphere = crop_data.bouding_sphere();
                    auto& center = bdsphere.center();
                    auto& radius = bdsphere.radius();
                    ImGuiHelper::Property("center", center);
                    ImGuiHelper::Property("radius", radius, 0.1, 1000.0f);
                }
                ImGui::NextColumn();

                ImGui::Columns(1);
                ImGui::Separator();

                auto& transform = crop_data.transform;
                glm::vec3 rotation = glm::degrees(transform.get_local_orientation());
                auto position = transform.get_local_position();
                auto scale = transform.get_local_scale();
                float itemWidth = (ImGui::GetContentRegionAvail().x - (ImGui::GetFontSize() * 3.0f)) / 3.0f;

                if (diverse::ImGuiHelper::PropertyVector3("Position", position, itemWidth, 0.0f))
                    transform.set_local_position(position);
                if (crop_data.get_crop_type() == diverse::GaussianCrop::CropType::Box)
                {
                    if (diverse::ImGuiHelper::PropertyVector3("Rotation", rotation, itemWidth, 0.0f))
                    {
                        transform.set_local_orientation(glm::radians(glm::vec3(rotation.x, rotation.y, rotation.z)));
                    }

                    if (diverse::ImGuiHelper::PropertyVector3("Scale", scale, itemWidth, 1.0f))
                    {
                        transform.set_local_scale(scale);
                    }
                }
                ImGui::Columns(1);
                ImGui::Separator();

                ImGui::Unindent();
                ImGui::TreePop();
            }
        }
 

        ImGui::Columns(1);
        ImGui::Separator();
        ImGui::PopStyleVar();

        diverse::ImGuiHelper::PopID();
    }

    template <>
    void ComponentEditorWidget<diverse::GaussianComponent>(entt::registry& reg, entt::registry::entity_type e)
    {
        DS_PROFILE_FUNCTION();
        diverse::ImGuiHelper::PushID();
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
        ImGui::Columns(2);
        ImGui::Separator();
        auto& gaussian = reg.get<diverse::GaussianComponent>(e);

        using namespace diverse;
        auto scale = gaussian.ModelRef->splat_size;
        const auto splat_size_max = g_render_settings.gs_vis_type == 1 ? 10.0f : 1.0f;
        if (ImGuiHelper::Property("SplatSize", scale, 0.0f, splat_size_max,0.1f))
            gaussian.ModelRef->splat_size = scale;
      
        //ImGui::Indent();

        auto numGaussian = (u32)gaussian.ModelRef->get_num_gaussians();
        ImGuiHelper::Property("SplatNum", numGaussian, nullptr,ImGuiHelper::PropertyFlag::ReadOnly);
        ImGuiHelper::Property("Render", gaussian.participate_render);
        ImGuiHelper::Tooltip("Whether this gaussian participate in rendering");
        ImGuiHelper::Property("SHBand", gaussian.sh_degree, 0, 3);

        static auto old_color_adjustment = diverse::SplatColorAdjustment{gaussian.albedo_color, gaussian.brightness, gaussian.transparency, gaussian.white_point, gaussian.black_point};
        auto splat_color_adjustment = diverse::SplatColorAdjustment{gaussian.albedo_color, gaussian.brightness, gaussian.transparency, gaussian.white_point, gaussian.black_point};
        float transparency = std::log(splat_color_adjustment.transparency);
        auto modified_color = ImGuiHelper::Property("Transparency", transparency, -6.0f, 6.0f, 0.2f, ImGuiHelper::PropertyFlag::SliderValue);
        modified_color |= ImGuiHelper::Property("Brightness", splat_color_adjustment.brightness, -1.0f, 1.0f,0.1f,ImGuiHelper::PropertyFlag::SliderValue);
        modified_color |= ImGuiHelper::Property("Color", splat_color_adjustment.albedo_color, ImGuiHelper::PropertyFlag::ColourProperty);
        if(modified_color)
        {
            splat_color_adjustment.transparency = std::exp(transparency);
            auto& gs_edit = diverse::GaussianEdit::get();
            if ( gaussian.ModelRef && gs_edit.splat == &gaussian && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
                gs_edit.add_color_adjustment_op(splat_color_adjustment,old_color_adjustment);
            else
            {
                gaussian.albedo_color = splat_color_adjustment.albedo_color;
                gaussian.brightness = splat_color_adjustment.brightness;
                gaussian.transparency = splat_color_adjustment.transparency;
                gaussian.white_point = splat_color_adjustment.white_point;
                gaussian.black_point = splat_color_adjustment.black_point;
            }
            old_color_adjustment = splat_color_adjustment;
        }
        ImGui::Columns(1);
        ImGui::Separator();
        ImGui::PopStyleVar();
#ifdef DS_SPLAT_TRAIN
        auto gsTrain = reg.try_get<GaussianTrainerScene>(e);
        if( gsTrain )
        { 
            ImGui::Indent();
            if (ImGui::TreeNodeEx("Train", ImGuiTreeNodeFlags_Framed))
            {
                ImGui::Indent();
                ImGui::Columns(2);

                ImGui::TextUnformatted("DensifyStrategy");
                ImGui::NextColumn();
                ImGui::PushItemWidth(-1);
                const char* densify_str[] = { "SplatADC", "SplatMCMC" };
                if (ImGui::BeginCombo("densify", densify_str[gsTrain->getTrainConfig().densifyStrategy], 0)) // The second parameter is the label previewed before opening the combo.
                {
                    for (int n = 0; n < 2; n++) //now not support sparse grad
                    {
                        bool is_selected = (n == gsTrain->getTrainConfig().densifyStrategy);
                        if (ImGui::Selectable(densify_str[n]))
                        {
                            gsTrain->getTrainConfig().densifyStrategy = n;
                            gsTrain->setDensifyStrategy(n);
                        }
                        if (is_selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                ImGui::PopItemWidth();
                ImGui::NextColumn();

                ImGuiHelper::Property("UpdateFreq", gaussian.ModelRef->update_feq(),"How many steps update the render splat model data");

                if(ImGuiHelper::Property("IsTrain", gsTrain->isTrain(), "Enable/Disable this splat model training"))
                {
                    if(gsTrain->isTrain())
                        gsTrain->startTrain();
                    else
                        gsTrain->pauseTrain();
                }

                ImGuiHelper::Property("MaxIteraions", gsTrain->maxIteriaons(),3000,100000);
                ImGuiHelper::Property("NormalConstrain", gsTrain->getTrainConfig().normalConsistencyLoss,"Enable/Disable Normal Consistency Loss");
                ImGuiHelper::Property("StopDensifyAt", gsTrain->getTrainConfig().refineStopIter,100, 100000, "Stop densify gaussians that are larger than after these many steps");
                gsTrain->getTrainConfig().refineStopIter = std::max(gsTrain->getTrainConfig().refineStopIter, 1000);
                
                if (ImGuiHelper::Property("Create Sky Model", gsTrain->getTrainConfig().enableBg, "Whether create a sky model for the scene"))
                    ImGui::OpenPopup("CreateSkyModel");
                ImGuiHelper::messageBox("CreateSkyModel", "Create a sky model for the scene, this will reset the scene, are you sure?", [&]() {
                    gsTrain->resetGaussian();
                });
                ImGuiHelper::Property("ShowTrainView", gsTrain->ShowTrainView, "Whether visualize training camera view");
                if(gsTrain->ShowTrainView)
                {
                    int numView = gsTrain->getNumCameras();
                    ImGuiHelper::Property("ViewNum", numView, ImGuiHelper::PropertyFlag::ReadOnly);
                }
                //ImGui::Separator();
                //auto& enable_focus_region = Editor::get_editor()->enable_focus_region;
                //ImGui::NextColumn();
                //ImGui::Columns(1);
                //if (ImGui::TreeNodeEx("MaskRegion", ImGuiTreeNodeFlags_Framed))
                //{
                //    auto& splatEdit = GaussianEdit::get();
                //    auto gs_transform = reg.try_get<maths::Transform>(e);
                //    auto box = gaussian.ModelRef->get_world_bounding_box(gs_transform->get_local_matrix());
                //    splatEdit.bouding_box() = box;
                //    ImGui::Indent();
                //    auto& edit_transform = Editor::get_editor()->focus_region_transform;
                //    glm::vec3 rotation = glm::degrees(edit_transform.get_local_orientation());
                //    auto position = edit_transform.get_local_position();
                //    auto scale = edit_transform.get_local_scale();
                //    float itemWidth = (ImGui::GetContentRegionAvail().x - (ImGui::GetFontSize() * 3.0f)) / 3.0f;

                //    if (diverse::ImGuiHelper::PropertyVector3("Position", position, itemWidth, 0.0f))
                //        edit_transform.set_local_position(position);
                //    if (diverse::ImGuiHelper::PropertyVector3("Rotation", rotation, itemWidth, 0.0f))
                //        edit_transform.set_local_orientation(glm::radians(glm::vec3(rotation.x, rotation.y, rotation.z)));
                //    if (diverse::ImGuiHelper::PropertyVector3("Scale", scale, itemWidth, 1.0f))
                //        edit_transform.set_local_scale(scale);

                //    //ImGui::NextColumn();
                //    ImGui::Columns(2);
                //    if (ImGuiHelper::Property("Mask Region", enable_focus_region))
                //    {
                //        gsTrain->getTrainConfig().useMask = enable_focus_region;
                //        if (enable_focus_region)
                //        {
                //            auto mask_box = Editor::get_editor()->get_focus_splat_region();
                //            gsTrain->generateAABBMask(mask_box.min(), mask_box.max());
                //        }
                //    }

                //    ImGui::Unindent();
                //    ImGui::TreePop();
                //}
           
                ImGui::Columns(1);
                ImGui::Separator();
                if (ImGui::TreeNodeEx("Prune", ImGuiTreeNodeFlags_Framed))
                {
                    ImGui::Indent();
                    ImGui::Columns(3);

                    //ImGui::NextColumn();
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextUnformatted("PruneIteraion");
                    ImGui::NextColumn();
                    if (ImGui::Button("+"))
                    {
                        gsTrain->pruenIteraions.emplace_back();
                    }
                    ImGuiHelper::Tooltip("Add prune operation on this step");
                    ImGui::NextColumn();
                    if( gsTrain->pruenIteraions.size() > 0 && ImGui::Button("-") )
                    {
                        gsTrain->pruenIteraions.pop_back();
                    }
                    ImGuiHelper::Tooltip("Delete prune operation");
                    ImGui::NextColumn();

                    ImGui::Columns(2);
                    auto n = gsTrain->pruenIteraions.size();
                    for (auto i = 0; i < n; i++)
                    {
                        ImGuiHelper::Property("iteraion", gsTrain->pruenIteraions[i]);
                    }

                    ImGui::Unindent();
                    ImGui::TreePop();
                    ImGui::Columns(1);
                    ImGui::Separator();
                }
                
                ImGui::Unindent();
                ImGui::TreePop();
            }
            ImGui::Unindent();
            ImGui::Text("%s", gsTrain->getCurrentTrainingPhaseName().c_str());
            ImGui::SameLine();
            auto size = ImGui::CalcTextSize("ET: %.2f s ");
            auto sizeOfGfxAPIDropDown = ImGui::GetFontSize() * 8;
            ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - size.x - ImGui::GetStyle().ItemSpacing.x * 2);
            ImGui::Text("ET: %.2f s", gsTrain->getEstimateTrainingTime());
            float progress = gsTrain->getProgressOnCurrentPhase();
            ImGui::ProgressBar(progress);
        }
#endif
        ImGui::Columns(1);
        // float dpi = Application::get().get_window_dpi();
        float windowSizeX = ImGui::GetWindowSize().x;
        float windowSizeY = ImGui::GetWindowSize().y;
        ImVec2 buttonSize = ImVec2(windowSizeX * 0.8, 0.08 * windowSizeY);
        auto ButtonPos = ImGui::GetCursorPos();
        ButtonPos.x = (windowSizeX - buttonSize.x) / 2;
        ImGui::SetCursorPos(ButtonPos);
        {
            if (ImGui::Button("ApplyEdit", buttonSize))
            {
                gaussian.ModelRef->export_to_cpu();
#ifdef DS_SPLAT_TRAIN
                if (gsTrain)
                    gsTrain->updateTensorFromGaussianData(
                        gaussian.ModelRef->position(),
						gaussian.ModelRef->rotation(),
                        gaussian.ModelRef->scale(),
                        gaussian.ModelRef->opacity(),
                        gaussian.ModelRef->sh(),
                        gaussian.ModelRef->degree()
                    );
#endif
               auto& gs_edit = diverse::GaussianEdit::get();
               gs_edit.clear_op_history();
            }
        }
        ImGui::SetCursorPosX(ButtonPos.x);
        if (ImGui::Button("Save to file", buttonSize))
        {
            diverse::Editor::get_editor()->merge_select_splats();
        }
        ImGui::SetCursorPosX(ButtonPos.x);
        if (gsTrain && ImGui::Button("Convert to Mesh", buttonSize))
        {
            diverse::Editor::get_editor()->export_mesh();
        }
        diverse::ImGuiHelper::PopID();
    }

    template <>
    void ComponentEditorWidget<diverse::PointCloudComponent>(entt::registry& reg, entt::registry::entity_type e)
    {
        DS_PROFILE_FUNCTION();
        diverse::ImGuiHelper::PushID();
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
        ImGui::Columns(2);
        ImGui::Separator();
        auto& gaussian = reg.get<diverse::PointCloudComponent>(e);

        using namespace diverse;
        //ImGui::Indent();

        auto numGaussian = (u32)gaussian.ModelRef->get_num_points();
        ImGuiHelper::Property("PointNum", numGaussian, nullptr, ImGuiHelper::PropertyFlag::ReadOnly);
        ImGui::Columns(1);
        ImGui::Separator();
        ImGui::PopStyleVar();

        diverse::ImGuiHelper::PopID();
    }

    template <>
    void ComponentEditorWidget<diverse::MeshModelComponent>(entt::registry& reg, entt::registry::entity_type e)
    {
        DS_PROFILE_FUNCTION();
      
        diverse::ImGuiHelper::PushID();
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
        ImGui::Columns(2);
        ImGui::Separator();
        
        auto modelRef = reg.get<diverse::MeshModelComponent>(e).ModelRef;


        ImGui::Columns(1);
        ImGui::Separator();
        ImGui::PopStyleVar();
        
        int matIndex = 0;
        if(!modelRef)
        {
            diverse::ImGuiHelper::PopID();
            return;
        }
        using namespace diverse;
        ImGui::Separator();
        const auto& meshes = modelRef->get_meshes();
        ImGui::Indent();
        if(ImGui::TreeNodeEx("Meshes", ImGuiTreeNodeFlags_Framed))
        {
            int MeshIndex = 0;

            for(auto mesh : meshes)
            {
                String8 meshName = !mesh->get_name().empty() ? Str8StdS(mesh->get_name()) : PushStr8F(Application::get().get_frame_arena(), "Mesh %i", MeshIndex);

                if(ImGui::TreeNodeEx((const char*)meshName.str, ImGuiTreeNodeFlags_Framed))
                {
                    auto stats = mesh->get_stats();

                    ImGui::Indent();
                    ImGui::Columns(2);

                    diverse::ImGuiHelper::Property("Triangle Count", stats.TriangleCount,nullptr, diverse::ImGuiHelper::PropertyFlag::ReadOnly);
                    diverse::ImGuiHelper::Property("Vertex Count", stats.VertexCount, nullptr, diverse::ImGuiHelper::PropertyFlag::ReadOnly);
                    diverse::ImGuiHelper::Property("Index Count", stats.IndexCount, nullptr, diverse::ImGuiHelper::PropertyFlag::ReadOnly);
                    // diverse::ImGuiHelper::Property("Optimise Threshold", stats.OptimiseThreshold, 0.0f, 0.0f, 0.0f, diverse::ImGuiHelper::PropertyFlag::ReadOnly);
                    diverse::ImGuiHelper::PropertyConst("Material", mesh->get_material() ? mesh->get_material()->get_name().c_str() : "Empty");
                    ImGui::Columns(1);

                    ImGui::Unindent();
                    ImGui::TreePop();
                }

                MeshIndex++;
            }
            ImGui::TreePop();
        }
        
        if(ImGui::TreeNodeEx("Materials", ImGuiTreeNodeFlags_Framed))
        {
            diverse::Material* MaterialShown[1000];
            uint32_t MaterialCount = 0;
            for(auto mesh : meshes)
            {
                auto material       = mesh->get_material();
                std::string matName = material ? material->get_name() : "";

                bool materialFound = false;
                for(uint32_t i = 0; i < MaterialCount; i++)
                {
                    if(MaterialShown[i] == material.get())
                        materialFound = true;
                }

                if(materialFound)
                    continue;

                MaterialShown[MaterialCount++] = material.get();

                if(matName.empty())
                {
                    matName = "Material";
                    matName += std::to_string(matIndex);
                }

                matName += "##" + std::to_string(matIndex);
                matIndex++;
                ImGui::Indent();
                if(!material)
                {
                    ImGui::TextUnformatted("Empty Material");
                    if(ImGui::Button("Add Material", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f)))
                        mesh->set_material(diverse::createSharedPtr<diverse::Material>());
                }
                else if(ImGui::TreeNodeEx(matName.c_str(), ImGuiTreeNodeFlags_Framed))
                {
                    using namespace diverse;
                    ImGui::Indent();
                    //if(ImGui::Button("Save to file"))
                    //{
                    //    std::string filePath = "//assets/Meshes"; // Materials/" + matName + ".lmat";
                    //    std::string physicalPath;
                    //    if(FileSystem::get().resolve_physical_path(filePath, physicalPath))
                    //    {
                    //        physicalPath += "/materials/" + matName + ".dmat";
                    //        std::stringstream storage;

                    //        cereal::JSONOutputArchive output { storage };
                    //        material->save(output);

                    //        FileSystem::write_text_file(physicalPath, storage.str());
                    //    }
                    //}
                    bool flipImage = false;

                    bool twoSided     = material->get_render_flag(diverse::Material::RenderFlags::TWOSIDED);
                    bool depthTested  = material->get_render_flag(diverse::Material::RenderFlags::DEPTHTEST);
                    bool alphaBlended = material->get_render_flag(diverse::Material::RenderFlags::ALPHABLEND);
                    bool castShadows  = !material->get_render_flag(diverse::Material::RenderFlags::NOSHADOW);

                    ImGui::Columns(2);
                    ImGui::Separator();

                    ImGui::AlignTextToFramePadding();

                   /* if(ImGuiHelper::Property("Alpha Blended", alphaBlended))
                        material->set_render_flag(diverse::Material::RenderFlags::ALPHABLEND, alphaBlended);

                    if(ImGuiHelper::Property("Two Sided", twoSided))
                        material->set_render_flag(diverse::Material::RenderFlags::TWOSIDED, twoSided);

                    if(ImGuiHelper::Property("Depth Tested", depthTested))
                        material->set_render_flag(diverse::Material::RenderFlags::DEPTHTEST, depthTested);

                    if(ImGuiHelper::Property("Cast Shadows", castShadows))
                        material->set_render_flag(diverse::Material::RenderFlags::NOSHADOW, !castShadows);

                    ImGuiHelper::Property("Alpha Cutoff", material->get_properties().alphaCutoff, 0.0f, 1.0f, 0.1f);*/

                    ImGui::Columns(1);

                    // diverse::MaterialProperties& prop = material->get_properties();
                    auto colour                        = glm::vec4();
                    auto& textures                     = material->get_textures();
                    glm::vec2 textureSize              = glm::vec2(32.0f, 32.0f);
                    diverse::MaterialProperties* prop = &material->get_properties();
                    TextureWidget("Albedo", material.get(), textures.albedo.get(), flipImage,std::bind(&diverse::Material::set_albedo_texture, material, std::placeholders::_1), textureSize * Application::get().get_window_dpi(), 0, true);

                    TextureWidget("Normal", material.get(), textures.normal.get(), flipImage, std::bind(&diverse::Material::set_normal_texture, material, std::placeholders::_1), textureSize * Application::get().get_window_dpi(), 1);

                    if (prop->work_flow == PBR_WORKFLOW_METALLIC_ROUGHNESS)
                    {
                        TextureWidget("MetallicRoughness", material.get(), textures.metallic.get(), flipImage, std::bind(&diverse::Material::set_metallic_texture, material, std::placeholders::_1), textureSize* Application::get().get_window_dpi(), 2);
                    }
                    else
                    { 
                        TextureWidget("Metallic", material.get(), textures.metallic.get(), flipImage, std::bind(&diverse::Material::set_metallic_texture, material, std::placeholders::_1), textureSize * Application::get().get_window_dpi(), 2);

                        TextureWidget("Roughness", material.get(), textures.roughness.get(), flipImage, std::bind(&diverse::Material::set_roughness_texture, material, std::placeholders::_1), textureSize * Application::get().get_window_dpi(), 3);
                    }
                    TextureWidget("AO", material.get(), textures.ao.get(), flipImage, std::bind(&diverse::Material::set_ao_texture, material, std::placeholders::_1), textureSize * Application::get().get_window_dpi(), 4);

                    TextureWidget("Emissive", material.get(), textures.emissive.get(), flipImage, std::bind(&diverse::Material::set_emissive_texture, material, std::placeholders::_1), textureSize * Application::get().get_window_dpi(), 5);

                    ImGui::Unindent();
                    ImGui::TreePop();
                }
                ImGui::Unindent();
            }
            ImGui::TreePop();
        }
        ImGui::Unindent();

        diverse::ImGuiHelper::PopID();
    }

    template <>
    void ComponentEditorWidget<diverse::Camera>(entt::registry& reg, entt::registry::entity_type e)
    {
        DS_PROFILE_FUNCTION();
        auto& camera = reg.get<diverse::Camera>(e);

        diverse::ImGuiHelper::ScopedStyle(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
        ImGui::Columns(2);
        ImGui::Separator();

        using namespace diverse;

        float aspect = camera.get_aspect_ratio();
        if (ImGuiHelper::Property("Aspect", aspect, 0.0f, 10.0f))
            camera.set_aspect_ratio(aspect);

        float fov = camera.get_fov();
        if (ImGuiHelper::Property("Fov", fov, 1.0f, 120.0f))
            camera.set_fov(fov);

        float n = camera.get_near();
        if (ImGuiHelper::Property("Near", n, 0.0f, 10.0f))
            camera.set_near(n);

        float f = camera.get_far();
        if (ImGuiHelper::Property("Far", f, 10.0f, 10000.0f))
            camera.set_far(f);

        float scale = camera.get_scale();
        if (ImGuiHelper::Property("Scale", scale, 0.0f, 1000.0f))
            camera.set_scale(scale);

        bool ortho = camera.is_orthographic();
        if (ImGuiHelper::Property("Orthograhic", ortho))
            camera.set_orthographic(ortho);

        float aperture = camera.get_aperture();
        if (ImGuiHelper::Property("Aperture", aperture, 0.0f, 200.0f))
            camera.set_aperture(aperture);

        float shutterSpeed = camera.get_shutter_speed();
        if (ImGuiHelper::Property("Shutter Speed", shutterSpeed, 0.0f, 1.0f))
            camera.set_shutter_speed(shutterSpeed);

        float sensitivity = camera.get_sensitivity();
        if (ImGuiHelper::Property("ISO", sensitivity, 0.0f, 5000.0f))
            camera.set_sensitivity(sensitivity);

        float exposure = camera.get_exposure();
        ImGuiHelper::Property("Exposure", exposure, 0.0f, 0.0f, 0.0f, ImGuiHelper::PropertyFlag::ReadOnly);

        ImGui::Columns(1);
        ImGui::Separator();
    }

}
namespace diverse
{
    InspectorPanel::InspectorPanel(bool active)
        : EditorPanel(active)
    {
        m_Name = U8CStr2CStr(ICON_MDI_INFORMATION " Inspector###inspector");
        m_SimpleName = "Inspector";
    }

    static bool init = false;
    void InspectorPanel::on_new_scene(Scene* scene)
    {
        DS_PROFILE_FUNCTION();
        if (init)
            return;

        init = true;

        auto& registry = scene->get_registry();
        auto& iconMap = m_Editor->get_component_iconmap();

#define TRIVIAL_COMPONENT(ComponentType, ComponentName)                      \
    {                                                                        \
        std::string Name;                                                    \
        if(iconMap.find(typeid(ComponentType).hash_code()) != iconMap.end()) \
            Name += iconMap[typeid(ComponentType).hash_code()];              \
        else                                                                 \
            Name += iconMap[typeid(Editor).hash_code()];                     \
        Name += "\t";                                                        \
        Name += (ComponentName);                                             \
        m_EnttEditor.registerComponent<ComponentType>(Name.c_str());         \
    }
        TRIVIAL_COMPONENT(maths::Transform, "Transform");
        TRIVIAL_COMPONENT(MeshModelComponent, "MeshModel");
        TRIVIAL_COMPONENT(GaussianComponent, "GaussianComponent");
        TRIVIAL_COMPONENT(PointCloudComponent, "PointCloudComponent");
        TRIVIAL_COMPONENT(GaussianCrop, "GaussianCrop");
        //TRIVIAL_COMPONENT(Camera, "Camera");
        TRIVIAL_COMPONENT(Environment, "Environment");
        // TRIVIAL_COMPONENT(PointLightComponent, "PointLightComponent");
        // TRIVIAL_COMPONENT(RectLightComponent, "RectLightComponent");
        // TRIVIAL_COMPONENT(DirectionalLightComponent, "DirectionalLightComponent");
        // TRIVIAL_COMPONENT(SpotLightComponent, "SpotLightComponent");
        // TRIVIAL_COMPONENT(DiskLightComponent, "DiskLightComponent");
        // TRIVIAL_COMPONENT(CylinderLightComponent, "CylinderLightComponent");
    }
    void InspectorPanel::on_imgui_render()
    {
        DS_PROFILE_FUNCTION();

        const auto& selectedEntities = m_Editor->get_selected();
        if (ImGui::Begin(m_Name.c_str(), &m_Active))
        {
            ImGuiHelper::PushID();

            Scene* currentScene = Application::get().get_scene_manager()->get_current_scene();

            if (!currentScene)
            {
                m_Editor->set_selected(entt::null);
                ImGuiHelper::PopID();
                ImGui::End();
                return;
            }

            auto& registry = currentScene->get_registry();
            if (selectedEntities.size() != 1 || !registry.valid(selectedEntities.front()))
            {
                m_Editor->set_selected(entt::null);
                ImGuiHelper::PopID();
                ImGui::End();
                return;
            }

            auto selected = selectedEntities.front();
            Entity SelectedEntity = { selected, currentScene };

            // active checkbox
            auto activeComponent = registry.try_get<ActiveComponent>(selected);
            bool active = activeComponent ? activeComponent->active : true;
            if (ImGui::Checkbox("##ActiveCheckbox", &active))
            {
                if (!activeComponent)
                    registry.emplace<ActiveComponent>(selected, active);
                else
                    activeComponent->active = active;
            }
            ImGui::SameLine();
            ImGui::TextUnformatted(U8CStr2CStr(ICON_MDI_CUBE));
            ImGui::SameLine();

            bool hasName = registry.all_of<NameComponent>(selected);
            std::string name;
            if (hasName)
                name = registry.get<NameComponent>(selected).name;
            else
                name = stringutility::to_string(entt::to_integral(selected));


            ImGui::SameLine();
            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - ImGui::GetFontSize() * 4.0f);
            {
                ImGuiHelper::ScopedFont boldFont(ImGui::GetIO().Fonts->Fonts[1]);
                if (ImGuiHelper::InputText(name, "##InspectorNameChange"))
                    registry.get_or_emplace<NameComponent>(selected).name = name;
            }
            ImGui::SameLine();

            ImGui::Separator();

            ImGui::BeginChild("Components", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_None);
            m_EnttEditor.RenderImGui(registry, selected);
            ImGui::EndChild();

            ImGuiHelper::PopID();
        }
        ImGui::End();
    }
    void InspectorPanel::set_debug_mode(bool mode)
    {
        m_DebugMode = mode;
    }
}