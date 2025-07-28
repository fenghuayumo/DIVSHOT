#pragma once

#include "editor_panel.h"
#include <scene/component/time_line.h>
#include <memory>
#include <list>
#include <imgui/imgui_helper.h>
#include <future>
namespace diverse
{
    enum KeyFrameState
    {
        Play,
        Pause
    };

    class KeyFramePanel : public EditorPanel
    {
    public:
        KeyFramePanel(bool active = true);
        void	on_imgui_render() override;
        void	on_update(float dt) override;
        void    on_new_scene(Scene* scene) override;

        void    set_key_frame_state(KeyFrameState state) { m_keyframe_state = state; }
        float&  auto_play_speed() { return m_auto_play_speed; }
    protected:
        void    render_time_line(KeyFrameTimeLine* timeline);
        void    reset_render_frame();

        void    add_key_frame(int64 time_point,KeyFrameTimeLine* timeline);
        void    delete_key_frame(int64 time_point,KeyFrameTimeLine* timeline);
        int     find_key_frame(int64 time_point, KeyFrameTimeLine* timeline);
        void    prepare_next_Key_frame(KeyFrameTimeLine* timline);
        void    set_camera_from_time(int64 time_point, KeyFrameTimeLine* timline);
        void    render_tool_bar_header(ImRect ToolBarAreaRect, ImDrawList* draw_list, KeyFrameTimeLine* timeline);
        bool    clearTempDir();
        void    export_video_dialog(KeyFrameTimeLine* timeline);
        float   m_play_time = 0.0f;
        float   m_auto_play_speed = 0.0f;

        KeyFrameState   m_keyframe_state = KeyFrameState::Pause;
        uint32_t m_render_frame_idx = 0;
        std::vector<std::future<void>> m_render_futures;

        bool m_rendering = false;
        struct RenderSettings 
        {
            glm::ivec2 resolution = { 1920, 1080 };
            int spp = 16;
            int fps = 24;
            float duration_seconds = 5.0f;
            float shutter_fraction = 0.5f;
            int quality = 10;

            uint32_t n_frames() const {
                return (uint32_t)((double)duration_seconds * fps);
            }

            float frame_seconds() const {
                return 1.0f / (duration_seconds * fps);
            }

            float frame_milliseconds() const {
                return 1000.0f / (duration_seconds * fps);
            }

            std::string filename = "video.mp4";
        };
        RenderSettings m_render_settings;
        i32            m_frame_counter = 0;
    };
}