#include "histogram_panel.h"
#include "editor.h"
#include "gaussian_edit.h"
#include <scene/camera/camera.h>
#include <scene/component/gaussian_component.h>
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

#include <imgui/imgui_internal.h>
#include <imgui/Plugins/ImGuizmo.h>

namespace diverse
{
    namespace splatData{
        typedef float (*Func)(float);
        const auto SH_C0 = 0.28209479177387814;
        float identity(float v) {
            return v;
        }

        float scaleFunc(float v) {
            return std::exp(v);
        }

        float colorFunc(float v) {
            return 0.5 + v * SH_C0;
        }

        float sigmoid(float v) 
        {
            if (v > 0) {
                return 1 / (1 + std::exp(-v));
            }

            float t = std::exp(v);
            return t / (1 + t);
        }

        struct DataFuncs 
        {
            Func x;
            Func y;
            Func z;
            Func scale_0;
            Func scale_1;
            Func scale_2;
            Func f_dc_0;
            Func f_dc_1;
            Func f_dc_2;
            Func opacity;
        };

        Func dataFuncs[10] = {
            identity,
            identity,
            identity,
            scaleFunc,
            scaleFunc,
            scaleFunc,
            colorFunc,
            colorFunc,
            colorFunc,
            sigmoid
        };
    }
    extern auto process_selection(GaussianModel* splat,const EditSelectOpType& op, std::function<bool(int i)>&& pred) -> void;

    HistogramPanel::HistogramPanel(bool active)
        : EditorPanel(active)
    {
        name = U8CStr2CStr(ICON_MDI_CHART_HISTOGRAM " Histogram###Histogram");
        simple_name = "Histogram";

        histogram.resize(256);
    }

    void HistogramPanel::on_imgui_render()
    {
        DS_PROFILE_FUNCTION();
        Application& app = Application::get();

        ImGuiHelper::ScopedStyle windowPadding(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

        auto flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

        if (!ImGui::Begin(name.c_str(), &is_active, flags) )
        {
            ImGui::End();
            return;
        }
        ImVec2 offset = { 0.0f, 0.0f };
        offset = ImGui::GetCursorPos(); // Usually ImVec2(0.0f, 50.0f);
        auto sceneViewSize = ImGui::GetWindowContentRegionMax() - ImGui::GetWindowContentRegionMin() - offset * 0.5f;
        auto sceneViewPosition = ImGui::GetWindowPos() + offset;
        left_panel_width = 0.2 * sceneViewSize.x;
        ImGui::GetWindowDrawList()->PushClipRect(sceneViewPosition, { sceneViewSize.x + sceneViewPosition.x, sceneViewSize.y + sceneViewPosition.y - 2.0f });
        
        auto offset_2_bucket = [&](int value){
            const auto bins = histogram.bins.size();
            return std::max(0, std::min<int>(bins - 1, std::floor((value - sceneViewPosition.x - left_panel_width) / (sceneViewSize.x -left_panel_width)* bins)));
        };
        auto drawList = ImGui::GetWindowDrawList();

        auto& reg = editor->get_current_scene()->get_registry();
        auto select_valid = reg.valid(editor->get_current_splat_entt());
        auto gaussian = reg.try_get<diverse::GaussianComponent>(editor->get_current_splat_entt());
        auto splat = gaussian ? gaussian->ModelRef : nullptr;
        using namespace splatData;
        auto valueFunc = [&](int idx)->float {
            u32 gs_state = splat->state()[idx];
            auto state = getOpState(gs_state);
            if (state != NORMAL_STATE && state != SELECT_STATE) return INVALID_VALUE;
            auto pos = splat->position()[idx];
            auto scale = splat->scale()[idx];
            switch (select_type)
            {
            case 0:
            case 1:
            case 2:
                return dataFuncs[select_type](pos[select_type]);
            case 3:
            case 4:
            case 5:
                return dataFuncs[select_type](scale[select_type-3]);
            case 6:
            case 7:
            case 8:
                return dataFuncs[select_type](splat->sh()[idx][select_type - 6]);
            case 9:
                return dataFuncs[select_type](splat->opacity()[idx]);
            case 10: //distance
                return glm::length(pos);
            case 11: //volume
                return scaleFunc(scale.x) * scaleFunc(scale.y) * scaleFunc(scale.z);
            case 12: //surface area
                return scaleFunc(scale.x) * scaleFunc(scale.x) + scaleFunc(scale.y) * scaleFunc(scale.y) + scaleFunc(scale.z) * scaleFunc(scale.z);
            default:
                return INVALID_VALUE;
            }
            return dataFuncs[0](pos[0]);
        };
        auto selecFunc = [&](int idx)->bool {
            u32 gs_state = splat->state()[idx];
            auto state = getOpState(gs_state);
            return state == SELECT_STATE;
        };
        if(splat && splat->is_flag_set(AssetFlag::UploadedGpu))
            histogram.calc(splat->position().size(), valueFunc, selecFunc);
        update_histogram(sceneViewPosition, sceneViewSize);
        if (Input::get().get_mouse_clicked(InputCode::MouseKey::ButtonLeft))
        {
            drag_start = drag_end = ImGui::GetMousePos();
            if( drag_start.x > (left_panel_width + sceneViewPosition.x - 5) && drag_start.x < (sceneViewPosition.x + sceneViewSize.x - 10) && drag_start.y > sceneViewPosition.y && drag_start.y < (sceneViewPosition.y + sceneViewSize.y - 10))
                is_dragging = true;
        }
        if (Input::get().get_mouse_held(InputCode::MouseKey::ButtonLeft) && is_dragging)
        {
            ImU32 lineColor = IM_COL32(200, 200, 200, 255);
            float lineThickness = 1.0f;
            auto p0 = drag_start - sceneViewPosition; p0.y = 0;
            auto p1 = ImGui::GetMousePos() - sceneViewPosition; p1.y = sceneViewSize.y - 2.0f;
            {
                drawList->AddLine(ImVec2{ p0.x, p0.y } + sceneViewPosition, ImVec2{ p1.x, p0.y } + sceneViewPosition, lineColor, lineThickness);
                drawList->AddLine(ImVec2{ p0.x, p0.y } + sceneViewPosition, ImVec2{ p0.x, p1.y } + sceneViewPosition, lineColor, lineThickness);
                drawList->AddLine(ImVec2{ p1.x, p0.y } + sceneViewPosition, ImVec2{ p1.x, p1.y } + sceneViewPosition, lineColor, lineThickness);
                drawList->AddLine(ImVec2{ p0.x, p1.y } + sceneViewPosition, ImVec2{ p1.x, p1.y } + sceneViewPosition, lineColor, lineThickness);
                ImU32 fillColor = IM_COL32(255, 102, 0, 100);
                drawList->AddRectFilled(p0 + sceneViewPosition,p1 + sceneViewPosition, fillColor);
            }
        }
        else if(is_dragging)
        {
            drag_end = ImGui::GetMousePos();
            is_dragging = false;
            
            process_selection(splat.get(), EditSelectOpType::Set, [&](int idx)->bool {
                u32 gs_state = splat->state()[idx];
                auto state = getOpState(gs_state);
                if( state != 0 && state != SELECT_STATE) return false;
                const auto value = valueFunc(idx);
                const auto bucket = histogram.valueToBucket(value);
                return bucket >= offset_2_bucket(drag_start.x) && bucket <= offset_2_bucket(drag_end.x);
            });
        }
        ImGui::GetWindowDrawList()->PopClipRect();
        ImGui::End();
    }

    void HistogramPanel::on_new_scene(Scene* scene)
    {
    }
    void HistogramPanel::update_histogram(const ImVec2& sceneViewPosition, const ImVec2& sceneViewSize)
    {
        auto& reg = editor->get_current_scene()->get_registry();
        auto gaussian = reg.try_get<diverse::GaussianComponent>(editor->get_current_splat_entt());
        if(!gaussian) return;
        draw_panel_info(sceneViewPosition, sceneViewSize);
        //ImGui::BeginChild("Right", ImVec2(sceneViewSize.x - m_LeftPanelWidth, sceneViewSize.y));
        auto drawList = ImGui::GetWindowDrawList();
        const auto offset = left_panel_width;
        ImU32 fillbg = IM_COL32(20, 20, 20, 255);
        drawList->AddRectFilled(ImVec2(sceneViewPosition.x + offset, sceneViewPosition.y), sceneViewPosition + sceneViewSize, fillbg);

        //draw histogram
        ImU32 color1 = IM_COL32(255, 255, 117, 255);
        ImU32 color2 = IM_COL32(255, 0, 0, 255);
        auto bucket_2_offset = [=](int bucket) {
            return bucket /(float) histogram.bins.size() * (sceneViewSize.x - offset) + sceneViewPosition.x + offset;
        };
        auto [maxNum,maxBinIndex] = histogram.binMaxNumValues();

        auto stepSize = (sceneViewSize.y - 5.0f) / maxNum;
        if (gaussian)
        {
            //drawList->AddRectFilled()
            for (auto i = 0; i < histogram.bins.size(); i++) 
            {
                const auto& bin = histogram.bins[i];
                auto num = bin.selected + bin.unselected;
                auto x = bucket_2_offset(i);
                auto y = sceneViewPosition.y + stepSize * (maxNum- num);
                auto yhat = y + stepSize * bin.selected;
                auto x1 = bucket_2_offset(i+1);
                auto y1 = sceneViewPosition.y + sceneViewSize.y - 5.0f;
                drawList->AddRectFilled(ImVec2(x, y), ImVec2(x1, yhat), color2);
                drawList->AddRectFilled(ImVec2(x, yhat), ImVec2(x1, y1), color1);
            }
        }
    }

    void HistogramPanel::draw_panel_info(const ImVec2& sceneViewPosition, const ImVec2& sceneViewSize)
    {
        auto& reg = editor->get_current_scene()->get_registry();
        auto gaussian = reg.try_get<diverse::GaussianComponent>(editor->get_current_splat_entt());
        if (!gaussian) return;
        auto splat = gaussian->ModelRef;
        if(!splat) return;
        i32 numSplats = splat->get_num_gaussians();
        i32 selectNum = splat->num_selected_gaussians();
        i32 deleteNum = splat->num_delete_gaussians();
        i32 hidenNum = splat->num_hidden_gaussians();
        ImGui::BeginChild("Left",ImVec2(left_panel_width,sceneViewSize.y));
        diverse::ImGuiHelper::PushID();
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
        ImGui::Columns(2);
        ImGui::Separator();

        ImGui::TextUnformatted("EditType");

        ImGui::NextColumn();
        ImGui::PushItemWidth(-1);

        const char* selectText[] = { "X", "Y","Z","ScaleX","ScaleY","ScaleZ","Red","Green","Blue","Opacity", "Distance","Volume", "SurfaceArea",};
        auto current_select = selectText[(int)select_type];
        if (ImGui::BeginCombo("", current_select, 0)) // The second parameter is the label previewed before opening the combo.
        {
            for (int n = 0; n < 13; n++)
            {
                const auto is_selected = n == select_type;
                if (ImGui::Selectable(selectText[n], current_select))
                {
                    select_type = n;
                }
                if (is_selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::PopItemWidth();
        ImGui::NextColumn();
        
        ImGuiHelper::Property("Splats", numSplats, ImGuiHelper::PropertyFlag::ReadOnly);
        ImGuiHelper::Property("Selected", selectNum, ImGuiHelper::PropertyFlag::ReadOnly);
        ImGuiHelper::Property("Deleted", deleteNum, ImGuiHelper::PropertyFlag::ReadOnly);
        ImGuiHelper::Property("Locked", hidenNum, ImGuiHelper::PropertyFlag::ReadOnly);
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::PopStyleVar();
        diverse::ImGuiHelper::PopID();
        ImGui::EndChild();
    }
}