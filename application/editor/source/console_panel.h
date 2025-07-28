#pragma once

#include "editor_panel.h"

#include <imgui/imgui.h>
#include <core/reference.h>
#include <core/vector.h>

namespace diverse
{
    class ConsolePanel : public EditorPanel
    {
    public:
        class Message
        {
        public:
            enum Level : uint32_t
            {
                Trace = 1,
                Debug = 2,
                Info = 4,
                Warn = 8,
                Error = 16,
                Critical = 32,
            };

        public:
            Message(const std::string& message = "", Level level = Level::Trace, const std::string& source = "", int threadID = 0, const std::string& time = "");
            void on_imgui_render();
            void increase_count() { m_Count++; };
            size_t get_message_iD() const { return m_MessageID; }

            static const char* get_level_name(Level level);
            static const char* get_level_icon(Level level);
            static glm::vec4 get_render_colour(Level level);

        public:
            const std::string m_Message;
            const Level m_Level;
            const std::string m_Source;
            const int m_ThreadID;
            std::string m_Time;
            int m_Count = 1;
            size_t m_MessageID;
        };

        ConsolePanel(bool active = true);
        ~ConsolePanel() = default;
        static void flush();
        void on_imgui_render() override;

        static void add_message(const SharedPtr<Message>& message);

    private:
        void imgui_render_header();
        void imgui_render_messages();

    private:
        static uint16_t s_MessageBufferCapacity;
        static uint16_t s_MessageBufferSize;
        static uint16_t s_MessageBufferBegin;
        static Vector<SharedPtr<Message>> s_MessageBuffer;
        static bool s_AllowScrollingToBottom;
        static bool s_RequestScrollToBottom;
        static uint32_t s_MessageBufferRenderFilter;
        ImGuiTextFilter Filter;
    };
}
