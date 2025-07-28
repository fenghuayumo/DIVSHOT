#include <backend/drs_rhi/gpu_texture.h>
#include "keyframe_panel.h"
#include "editor.h"
#include <scene/scene.h>
#include <scene/entity.h>

#include <imgui/imgui_helper.h>
#include <imgui/IconsMaterialDesignIcons.h>
#include <utility/thread_pool.h>
#include <utility/file_utils.h>
#include <stb/image_utils.h>
#include <opencv2/opencv.hpp>
#include <utility/cmd_variable.h>
#define COL_CURSOR_LINE IM_COL32(4,70,140,255)
#define COL_KEY_POINT IM_COL32(255, 255, 102, 255)
#define COL_SELECT_RECT IM_COL32(255, 0, 0, 255)
namespace diverse
{
    ThreadPool video_threadPool;
    KeyFramePanel::KeyFramePanel(bool active)
        : EditorPanel(active)
    {
        m_Name = U8CStr2CStr(ICON_MDI_VIDEO " KeyFrame###KeyFrame");
        m_SimpleName = "KeyFrame";
    }

    void KeyFramePanel::on_imgui_render()
    {
        auto flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
        if (ImGui::Begin(m_Name.c_str(), &m_Active, flags))
        {
            //ImGui::SetCursorScreenPos(panel_pos + ImVec2(32, 0));
            auto& timeLine = m_Editor->get_current_scene()->get_keyFrame_entity().get_component<KeyFrameTimeLine>();
            render_time_line(&timeLine);
        }
        ImGui::End();
    }

    static bool timelineButton(ImDrawList* draw_list, const char* label, ImVec2 pos, ImVec2 size, std::string tooltips = "", ImVec4 hover_color = ImVec4(0.5f, 0.5f, 0.75f, 1.0f))
    {
        ImGuiIO& io = ImGui::GetIO();
        ImRect rect(pos, pos + size);
        bool overButton = rect.Contains(io.MousePos);
        if (overButton)
            draw_list->AddRectFilled(rect.Min, rect.Max, ImGui::GetColorU32(hover_color), 2);
        ImVec4 color = ImVec4(1.f, 1.f, 1.f, 1.f);
        //ImGui::SetWindowFontScale(0.75);
        draw_list->AddText(pos, ImGui::GetColorU32(color), label);
        ImGui::SetWindowFontScale(1.0);
        if (overButton && !tooltips.empty() && ImGui::BeginTooltip())
        {
            ImGui::TextUnformatted(tooltips.c_str());
            ImGui::EndTooltip();
        }
        return overButton;
    }

    void KeyFramePanel::render_tool_bar_header(ImRect ToolBarAreaRect, ImDrawList* draw_list, KeyFrameTimeLine* timeline)
    {
        draw_list->AddRectFilled(ToolBarAreaRect.Min, ToolBarAreaRect.Max, ImGui::GetColorU32(ImGuiCol_Header), 0);

        float dpi = Application::get().get_window_dpi();
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 15.0f * (ImGui::GetFontSize() + ImGui::GetStyle().ItemSpacing.x));
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("fps:");
        ImGui::SameLine();
        ImGui::PushItemWidth(40);
        ImGui::DragInt(ImGuiHelper::GenerateID(), &m_render_settings.fps, 1.0f, 1, 120);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("set fps value");
        ImGui::PopItemWidth();
        ImGui::SameLine();

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("cur:");
        ImGui::SameLine();
        ImGui::PushItemWidth(40);
        ImGui::DragInt(ImGuiHelper::GenerateID(), &timeline->currentTime, 1.0f, timeline->startShowTime, timeline->endShowTime);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Set current time Point");
        ImGui::PopItemWidth();
        ImGui::SameLine();

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("start:");
        ImGui::SameLine();
        ImGui::PushItemWidth(40);
        ImGui::DragInt(ImGuiHelper::GenerateID(), &timeline->startShowTime, 1.0f, 0, timeline->endShowTime);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Set Start Time Point");
        ImGui::PopItemWidth();
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("end:");
        ImGui::SameLine();
        ImGui::PushItemWidth(40);
        ImGui::DragInt(ImGuiHelper::GenerateID(), &timeline->endShowTime, 1.0f, timeline->startShowTime, timeline->startShowTime + 1000);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Set End Time Point");
        ImGui::PopItemWidth();
        timeline->visibleTime = timeline->endShowTime - timeline->startShowTime;

        ImGui::SameLine((ImGui::GetWindowContentRegionMax().x * 0.36f) - (2.0f * (ImGui::GetFontSize() + ImGui::GetStyle().ItemSpacing.x)));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.2f, 0.7f, 0.0f));
        {
            if (ImGui::Button(U8CStr2CStr(ICON_MDI_STEP_BACKWARD_2)))
            {
                auto_play_speed() *= -2.0f;
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Backward Speed X2");
            ImGui::SameLine();
        }
        {
            if (ImGui::Button(U8CStr2CStr(ICON_MDI_STEP_BACKWARD)))
            {
                auto idx = find_key_frame(timeline->currentTime, timeline);
                if (idx > 0)
                {
					timeline->currentTime = timeline->keyframes[idx - 1].timePoint;
					set_camera_from_time(timeline->currentTime, timeline);
				}
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Previous Frame");
            ImGui::SameLine();
        }
        if(m_keyframe_state == KeyFrameState::Pause)
        {
            if (ImGui::Button(U8CStr2CStr(ICON_MDI_PLAY)))
            {
                set_key_frame_state(KeyFrameState::Play);
                auto_play_speed() = 1.0f;
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Play");
            ImGui::SameLine();
        }
        else
        {
            if (ImGui::Button(U8CStr2CStr(ICON_MDI_PAUSE)))
            {
                set_key_frame_state(KeyFrameState::Pause);
                auto_play_speed() = 0.0f;
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Pause");
            ImGui::SameLine();
        }

        {
            if (ImGui::Button(U8CStr2CStr(ICON_MDI_STEP_FORWARD)))
            {
                auto idx = find_key_frame(timeline->currentTime, timeline);
                if (idx > 0 && idx < timeline->keyframes.size() - 1)
                {
                    timeline->currentTime = timeline->keyframes[idx + 1].timePoint;
                    set_camera_from_time(timeline->currentTime, timeline);
                }
            }

            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Next");
            ImGui::SameLine();
            if (ImGui::Button(U8CStr2CStr(ICON_MDI_STEP_FORWARD_2)))
                auto_play_speed() *= 2.0f;

            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Forward Speed X2");
            ImGui::SameLine();

            if (ImGui::Button(U8CStr2CStr(ICON_MDI_PLUS)))
               add_key_frame(timeline->currentTime, timeline);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("add a keyframe");
            ImGui::SameLine();

            if (ImGui::Button(U8CStr2CStr(ICON_MDI_MINUS)))
                delete_key_frame(timeline->currentTime, timeline);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("delete a keyframe");
            ImGui::SameLine();

            if (ImGui::Button(U8CStr2CStr(ICON_MDI_EXPORT)))
                ImGui::OpenPopup("Export Video");
            if (ImGui::BeginPopupModal("Export Video", NULL, ImGuiWindowFlags_AlwaysAutoResize))
            {
                export_video_dialog(timeline);

                ImGui::EndPopup();
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Export Video");
        }
        ImGui::PopStyleColor();
    }

    void KeyFramePanel::render_time_line(KeyFrameTimeLine* timeline)
    {
        /************************************************************************************************************
    *                    |  0    5    10 v   15    20 <rule bar> 30     35      40      45       50       55    c
    * ___________________|_______________|_____________________________________________________________________ a
    *************************************************************************************************************/
        bool changed = false;

        float dpi = Application::get().get_window_dpi();
        ImGuiIO& io = ImGui::GetIO();
        int cx = (int)(io.MousePos.x);
        int cy = (int)(io.MousePos.y);
        const int toolbar_height = 32 * dpi;
        static bool MovingCurrentTime = false;
        static bool menuIsOpened = false;
        static ImVec2 RectMin = io.MousePos;
        static ImVec2 RectMax = io.MousePos;
        int64_t duration = ImMax(timeline->end - timeline->start, 1);

        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 window_pos = ImGui::GetCursorScreenPos();
        ImVec2 window_size = ImGui::GetWindowSize();
        ImVec2 canvas_pos = ImGui::GetCursorScreenPos();                    // ImDrawList API uses screen coordinates!
        ImVec2 canvas_size = ImGui::GetContentRegionAvail(); // resize canvas to what's available

        ImGui::BeginGroup();
        bool isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);

        ImGui::SetCursorScreenPos(canvas_pos);

        ImVec2 toolBarSize(canvas_size.x, (float)toolbar_height);
        ImRect ToolBarAreaRect(canvas_pos, canvas_pos + toolBarSize);

        // draw toolbar bg
        render_tool_bar_header(ToolBarAreaRect, draw_list, timeline);
  
        //Timeline Area
        ImVec2 timeLinePos = ImVec2(canvas_pos.x, canvas_pos.y + toolbar_height + 10);
        ImVec2 timeLineSize = ImVec2(canvas_size.x, 12 * dpi);
        ImRect timeLineRect(timeLinePos, timeLinePos + timeLineSize);
        draw_list->AddLine(timeLinePos, ImVec2(timeLinePos.x + canvas_size.x, timeLinePos.y), IM_COL32(64, 64, 64, 255));
        
        draw_list->AddRectFilled(timeLinePos, timeLinePos + timeLineSize, ImGui::GetColorU32(ImGuiCol_Button), 0);

        const int timelineCnt = timeline->endShowTime - timeline->startShowTime;
        const float timePixelWidth = timeLineSize.x /(float)timelineCnt;
        const float timelineHeight = timeLineSize.y ;
        ImVec2 contenSize = ImVec2(canvas_size.x, canvas_size.y - toolbar_height);
        auto drawTimeLine = [&]() {
            for (int i = 0; i < timelineCnt; i++) {
                float x = timeLinePos.x + i * timePixelWidth;
                if (i % 10 == 0) 
                {
                    draw_list->AddLine(ImVec2(x, timeLinePos.y), ImVec2(x, timeLinePos.y + contenSize.y), IM_COL32(128, 128, 128, 255), 2.0f);
                    auto time_str = std::to_string((i + timeline->startShowTime));
                    ImGui::SetWindowFontScale(1.1);
                    draw_list->AddText(ImVec2(x - 0.5f * timePixelWidth, timeLinePos.y - 12 * dpi), IM_COL32(224, 224, 224, 255), time_str.c_str());
                    ImGui::SetWindowFontScale(1.0);
                }
                else 
                {
                    draw_list->AddLine(ImVec2(x, timeLinePos.y), ImVec2(x, timeLinePos.y + timelineHeight), IM_COL32(128, 128, 128, 255));
                }
            }
        };
        drawTimeLine();

        ImRect custom_view_rect(timeLinePos, timeLinePos + contenSize);
        auto mouseTime = (int64_t)((io.MousePos.x - timeLinePos.x ) / timePixelWidth) + timeline->startShowTime;
        menuIsOpened = ImGui::IsPopupOpen("##timeline-context-menu");
        if (custom_view_rect.Contains(io.MousePos) && ImGui::IsMouseReleased(ImGuiMouseButton_Right) && !ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Right))
        {
            ImGui::OpenPopup("##timeline-context-menu");
            menuIsOpened = true;
        }
        if (ImGui::BeginPopup("##timeline-context-menu"))
        {
            ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(1.0, 1.0, 1.0, 1.0));
            if (ImGui::MenuItem("insert a new keyframe"))
            {
                add_key_frame(mouseTime, timeline);
            }
            if (ImGui::MenuItem("delete this keyframe"))
            {
                delete_key_frame(mouseTime, timeline);
            }
            ImGui::PopStyleColor();
            ImGui::EndPopup();
        }

        static bool is_draw_rect = false;
        ImGui::SetCursorScreenPos(custom_view_rect.Min);
        ImGui::BeginChildFrame(ImGui::GetCurrentWindow()->GetID("#timeline metric"), custom_view_rect.GetSize(), ImGuiWindowFlags_NoScrollbar);
        // check current time moving
        if (isFocused && !MovingCurrentTime && timeLineRect.Contains(io.MousePos) && ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            MovingCurrentTime = true;
        }
        if (MovingCurrentTime && duration)
        {
            if (!timeline->bSeeking || ImGui::IsMouseDragging(ImGuiMouseButton_Left))
            {
                auto timepoint = (int64_t)((io.MousePos.x - timeLineRect.Min.x) / timePixelWidth) + timeline->startShowTime;
                if (timepoint < timeline->startShowTime)
                {
                    timeline->startShowTime = std::max<int64_t>(timepoint, timeline->startShowTime - 2);;
                    timeline->endShowTime = timeline->startShowTime + timeline->visibleTime ;
                    RectMin = io.MousePos;
                    RectMax = io.MousePos;
                }
                if (timepoint > timeline->endShowTime)
                {
                    timeline->endShowTime = std::min<int64_t>(timepoint, timeline->endShowTime + 2);
                    timeline->startShowTime = timeline->endShowTime - timeline->visibleTime;
                    RectMin = io.MousePos;
                    RectMax = io.MousePos;
                }
                timeline->currentTime = timepoint;
                timeline->bSeeking = true;
            }
        }
        if (timeline->bSeeking && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            MovingCurrentTime = false;
            timeline->bSeeking = false;
        }
        if (!is_draw_rect  && custom_view_rect.Contains(io.MousePos) && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            auto timepoint = (int64_t)((io.MousePos.x - timeLineRect.Min.x) / timePixelWidth) + timeline->startShowTime;
            add_key_frame(mouseTime, timeline);
        }
        ImGui::EndChildFrame();
        draw_list->PushClipRect(custom_view_rect.Min, custom_view_rect.Max);
        if (timeline->currentTime >= timeline->startShowTime && timeline->currentTime <= timeline->endShowTime)
        {
            static const float cursorWidth = 2.f * dpi;
            float x = timeLinePos.x + (timeline->currentTime - timeline->startShowTime) * timePixelWidth + 1;
            draw_list->AddLine(ImVec2(x, timeLinePos.y), ImVec2(x, timeLinePos.y + contenSize.y), COL_CURSOR_LINE, cursorWidth);
        }
        draw_list->PopClipRect();

        ImRect keyframe_rect = ImRect(timeLinePos + ImVec2(0, timeLineSize.y), timeLinePos + contenSize);
        draw_list->PushClipRect(keyframe_rect.Min, keyframe_rect.Max);

        if (keyframe_rect.Contains(io.MousePos) && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            RectMin = io.MousePos;
            is_draw_rect = true;
        }
        if (is_draw_rect && keyframe_rect.Contains(io.MousePos) && ImGui::IsMouseDown(ImGuiMouseButton_Left) )
        {
            RectMax = io.MousePos;
            //draw rect
            draw_list->AddLine(RectMin, ImVec2(RectMax.x, RectMin.y), COL_SELECT_RECT, 1.0f);
            draw_list->AddLine(RectMin, ImVec2(RectMin.x, RectMax.y), COL_SELECT_RECT, 1.0f);
            draw_list->AddLine(ImVec2(RectMax.x, RectMin.y), RectMax, COL_SELECT_RECT, 1.0f);
            draw_list->AddLine(ImVec2(RectMin.x, RectMax.y), RectMax, COL_SELECT_RECT, 1.0f);
        }
        else if (is_draw_rect)
        {
            is_draw_rect = false;
        }
        ImRect  select_rect(RectMin, RectMax);

        for (auto key_frame : timeline->keyframes)
        {
            int px = (int)timeLinePos.x + (key_frame.timePoint - timeline->startShowTime)* timePixelWidth;
            if (px <= keyframe_rect.Max.x && px >= keyframe_rect.Min.x)
            {
                auto size = keyframe_rect.Max.y - keyframe_rect.Min.y;
                if (select_rect.Contains(ImVec2(px, keyframe_rect.Min.y + size * 0.5f)))
                {
                    draw_list->AddCircle(ImVec2(px, keyframe_rect.Min.y + size * 0.5f), 4.0f * dpi, IM_COL32(255,0,0,255));
                    if (ImGui::IsKeyDown(ImGuiKey::ImGuiKey_Delete))
                    {
                        delete_key_frame(key_frame.timePoint, timeline);
                    }
                }
                else
                {
                    draw_list->AddCircle(ImVec2(px, keyframe_rect.Min.y + size * 0.5f), 4.0f * dpi, COL_KEY_POINT);
                }

			}
        }

        draw_list->PopClipRect();
        // ImGui::PopStyleColor();

        ImGui::EndGroup();
    }

    void KeyFramePanel::on_update(float dt)
    {
        auto timeline = &m_Editor->get_current_scene()->get_keyFrame_entity().get_component<KeyFrameTimeLine>();
        if (m_auto_play_speed > 0.0f && timeline->currentTime >= timeline->start && timeline->currentTime <= timeline->end)
        {
            set_camera_from_time(timeline->currentTime, timeline);
        }
        prepare_next_Key_frame(timeline);
    }

    void KeyFramePanel::prepare_next_Key_frame(KeyFrameTimeLine* timline)
    {
        if (m_rendering) {
            if (m_frame_counter == 0)
                ImGui::OpenPopup("Render Video");
            f32 progress = (timline->currentTime - timline->start) / f32(timline->end - timline->start);
            if (ImGuiHelper::ProgressWindow("Render Video", progress, ImVec2(240, 0), true))
            {
                reset_render_frame();
            }
            if( (++m_frame_counter) % m_render_settings.spp != 0) return;

            auto tmp_dir = std::filesystem::path{ "tmp" };
            if (! std::filesystem::exists(tmp_dir) ) {
                if (!std::filesystem::create_directory(tmp_dir)) {
                    m_rendering = false;
                    DS_LOG_ERROR("Failed to create temporary directory 'tmp' to hold rendered images.");
                    return;
                }
            }
            auto renderTarget = m_Editor->get_main_render_texture();
            auto res = glm::ivec2(renderTarget->desc.extent[0], renderTarget->desc.extent[1]);
            auto img_data = renderTarget->export_texture();
            m_render_futures.emplace_back(video_threadPool.enqueue_task([img_data = std::move(img_data),
                frame_idx = m_render_frame_idx++, res, tmp_dir] {
                    write_stbi(tmp_dir / fmt::format("{:06d}.jpg", frame_idx), res.x, res.y, 4, (uint8_t*)img_data.data(), 100);
                }));
            bool end = timline->currentTime >= timline->end;
            if (end) {

                wait_all(m_render_futures);
                m_rendering = false;
                m_render_futures.clear();

                DS_LOG_INFO("Finished rendering '.jpg' video frames to '{}'. Assembling them into a video next.", tmp_dir.string());
                cv::VideoWriter videoWriter(m_render_settings.filename, cv::VideoWriter::fourcc('X', 'V', 'I', 'D'), m_render_settings.fps, cv::Size(res.x, res.y));
                if (!videoWriter.isOpened()) {
                    DS_LOG_ERROR("Could not open the video writer.");
                    return;
                }
                int frameCount = 0;
                cv::Mat frame;
                while (frameCount < m_render_frame_idx) {
                    auto frameFilename = tmp_dir / fmt::format("{:06d}.jpg", frameCount);
                    frame = cv::imread(frameFilename.string());
                    if (frame.empty()) {
                        break;
                    }
                    videoWriter.write(frame);
                    frameCount++;
                }
                videoWriter.release();
                clearTempDir();
                reset_render_frame();
            }
            timline->currentTime += m_auto_play_speed;
            timline->currentTime = ImClamp(timline->currentTime, timline->start, timline->end);
        }
        if (!m_rendering && m_keyframe_state == KeyFrameState::Play)
        {
            timline->currentTime += m_auto_play_speed; 
            timline->currentTime = ImClamp(timline->currentTime, timline->start, timline->end);
            if (timline->currentTime == timline->end)
            {
				//m_keyframe_state = KeyFrameState::Pause;
                timline->currentTime = timline->start;
			}
        }
    }

    void KeyFramePanel::reset_render_frame()
    {
        m_frame_counter = 0;
        m_auto_play_speed = 0.0f;
        m_rendering = false;
        m_render_futures.clear();

        auto variable = CmadVariableMgr::get().find("r.accumulate.spp");
        if (variable) variable->set_value<int>(1);
        variable = CmadVariableMgr::get().find("r.video_export");
        if (variable) variable->set_value<bool>(false);
        auto [width, height] = Application::get().get_scene_view_dimensions();
        m_Editor->handle_renderer_resize(width, height);
    }

    void KeyFramePanel::set_camera_from_time(int64 time_point, KeyFrameTimeLine* timeline)
    {
        if (timeline->keyframes.empty())
            return;
        auto play_time = std::max((time_point - timeline->start) / float(timeline->end - timeline->start), 0.0f);
        auto k = timeline->evalKeyFrame(play_time).cameraElement;
        auto& camera_transform = m_Editor->get_editor_camera_transform();
        camera_transform.set_local_orientation(k.R);
        // camera_transform.set_local_position(k.T);
        m_Editor->get_editor_camera_controller().update_focal_point(camera_transform, k.T);
        camera_transform.set_world_matrix(glm::mat4(1.0f));
    }

    void KeyFramePanel::add_key_frame(int64 time_point,KeyFrameTimeLine* timeline)
    {
        auto transMat = m_Editor->get_editor_camera_transform().get_local_matrix();
        auto cam_key_frame_var = CameraKeyFrameVar(transMat, 1, 1, 1, 1);

        auto it = std::find_if(timeline->keyframes.begin(), timeline->keyframes.end(), [&](const KeyFrame& it)->bool { return it.timePoint >= time_point; });
        timeline->keyframes.insert(it, { time_point,cam_key_frame_var });
        timeline->update();
    }

    void KeyFramePanel::delete_key_frame(int64 time_point, KeyFrameTimeLine* timeline)
    {
        auto is_equal = [&](const KeyFrame& it) { return abs(it.timePoint - time_point) <= 2; };
        auto it = std::find_if(timeline->keyframes.begin(), timeline->keyframes.end(), is_equal);
        if (it != timeline->keyframes.end())
        {
            timeline->keyframes.erase(it);
            timeline->update();
        }
    }

    int KeyFramePanel::find_key_frame(int64 time_point, KeyFrameTimeLine* timeline)
    {
        auto it = std::find_if(timeline->keyframes.begin(), timeline->keyframes.end(), [&](const KeyFrame& it)->bool { return it.timePoint >= time_point; });
        if (it != timeline->keyframes.end())
        {
			return std::distance(timeline->keyframes.begin(), it);
		}
        return -1;
    }

    bool KeyFramePanel::clearTempDir()
    {
        wait_all(m_render_futures);
        m_render_futures.clear();

        bool success = true;
        auto tmp_dir = std::filesystem::path{ "tmp" };
        if ( std::filesystem::exists(tmp_dir)) {
            if ( std::filesystem::is_directory(tmp_dir)) {
                for (const auto& path : std::filesystem::directory_iterator{ tmp_dir }) {
                    if (path.is_regular_file()) {
                        success &= std::filesystem::remove(path);
                    }
                }
            }

            //success &= tmp_dir.remove_file();
            success &= std::filesystem::remove(tmp_dir);
        }

        return success;
    }
	
    void KeyFramePanel::on_new_scene(Scene* scene)
    {
        //auto ents = m_Editor->get_current_scene()->get_entity_manager()->get_entities_with_type<KeyFrameTimeLine>();
        //if (ents.Empty())
        //{
        //     auto KeyFrameEnt = m_Editor->get_current_scene()->create_entity("KeyFrameEnt");
        //    KeyFrameEnt.add_component<KeyFrameTimeLine>();
        //}
    }

    void KeyFramePanel::export_video_dialog(KeyFrameTimeLine* timeline)
    {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("FileName:");
        ImGui::SameLine();
        ImGui::TextUnformatted(m_render_settings.filename.c_str());
        ImGui::SameLine();
        if( ImGuiHelper::Button("..")) //open file dialog
        {
            std::string videoPath = FileDialogs::saveFile({"mp4"});
            if (!videoPath.empty())
                m_render_settings.filename = videoPath;
        }

        diverse::ImGuiHelper::PushID();
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
        ImGui::Columns(2);
        ImGui::Separator();

        ImGui::TextUnformatted("resolution");
        ImGui::NextColumn();
        ImGui::PushItemWidth(-1);
        const char* res_str[] = { "1920x1080","1280x960"};
        glm::ivec2 resolution[] = {glm::ivec2(1920,1080),glm::ivec2(1280,960)};
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
        m_render_settings.resolution = resolution[cur_res];
        
        ImGuiHelper::Property("fps", m_render_settings.fps, 1, 120);
        ImGuiHelper::Property("spp", m_render_settings.spp);

        ImGui::Columns(1);
        ImGui::Separator();
        ImGui::PopStyleVar();
        diverse::ImGuiHelper::PopID();

        const auto width = ImGui::GetWindowWidth();
        auto button_sizex = 120;
        auto button_posx = ImGui::GetCursorPosX() + (width / 2 - button_sizex) / 2;
        ImGui::SetCursorPosX(button_posx);
        if (ImGui::Button("Ok", ImVec2(button_sizex, 0)))
        {
            if(!m_render_settings.filename.empty())
            { 
                m_render_frame_idx = 0;
                m_rendering = true;
                timeline->currentTime = timeline->start;
                auto_play_speed() = 1.0f;

                auto variable = CmadVariableMgr::get().find("r.accumulate.spp");
                if (variable) variable->set_value<int>(m_render_settings.spp);
                variable = CmadVariableMgr::get().find("r.video_export");
                if (variable) variable->set_value<bool>(true);

                m_Editor->handle_renderer_resize(m_render_settings.resolution.x, m_render_settings.resolution.y);
            }
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
}