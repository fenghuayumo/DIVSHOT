#include "core/string.h"
#include "console_panel.h"

#include <imgui/IconsMaterialDesignIcons.h>
#include <imgui/imgui_helper.h>
#include <core/profiler.h>
#include <maths/maths_utils.h>

namespace diverse
{
    uint32_t ConsolePanel::s_MessageBufferRenderFilter = 0;
    uint16_t ConsolePanel::s_MessageBufferCapacity = 2000;
    uint16_t ConsolePanel::s_MessageBufferSize = 0;
    uint16_t ConsolePanel::s_MessageBufferBegin = 0;
    Vector<SharedPtr<ConsolePanel::Message>> ConsolePanel::s_MessageBuffer = Vector<SharedPtr<ConsolePanel::Message>>(2000);
    bool ConsolePanel::s_AllowScrollingToBottom = true;
    bool ConsolePanel::s_RequestScrollToBottom = false;

    ConsolePanel::ConsolePanel(bool active)
        : EditorPanel(active)
    {
        DS_PROFILE_FUNCTION();
        m_Name = U8CStr2CStr(ICON_MDI_VIEW_LIST " Console###console");
        m_SimpleName = "Console";
        s_MessageBufferRenderFilter = Message::Level::Trace | Message::Level::Info | Message::Level::Debug | Message::Level::Warn | Message::Level::Error | Message::Level::Critical;
    }

    void ConsolePanel::add_message(const SharedPtr<Message>& message)
    {
        DS_PROFILE_FUNCTION();
        if (message->m_Level == 0)
            return;

        auto messageStart = s_MessageBuffer.begin() + s_MessageBufferBegin;
        if (*messageStart) // If contains old message here
        {
            for (auto messIt = messageStart; messIt != s_MessageBuffer.end(); ++messIt)
            {
                if (message->get_message_iD() == (*messIt)->get_message_iD())
                {
                    (*messIt)->increase_count();
                    return;
                }
            }
        }

        if (s_MessageBufferBegin != 0) // Skipped first messages in vector
        {
            for (auto messIt = s_MessageBuffer.begin(); messIt != messageStart; ++messIt)
            {
                if (*messIt)
                {
                    if (message->get_message_iD() == (*messIt)->get_message_iD())
                    {
                        (*messIt)->increase_count();
                        return;
                    }
                }
            }
        }

        *(s_MessageBuffer.begin() + s_MessageBufferBegin) = message;
        if (++s_MessageBufferBegin == s_MessageBufferCapacity)
            s_MessageBufferBegin = 0;
        if (s_MessageBufferSize < s_MessageBufferCapacity)
            s_MessageBufferSize++;

        if (s_AllowScrollingToBottom)
            s_RequestScrollToBottom = true;
    }

    void ConsolePanel::flush()
    {
        DS_PROFILE_FUNCTION();
        for (auto message = s_MessageBuffer.begin(); message != s_MessageBuffer.end(); ++message)
            (*message) = nullptr;
        s_MessageBufferBegin = 0;
    }

    void ConsolePanel::on_imgui_render()
    {
        DS_PROFILE_FUNCTION();
        auto flags = ImGuiWindowFlags_NoCollapse;
        ImGui::SetNextWindowSize(ImVec2(640, 480), ImGuiCond_FirstUseEver);
        ImGui::Begin(m_Name.c_str(), &m_Active, flags);
        {
            imgui_render_header();
            ImGui::Separator();
            imgui_render_messages();
        }
        ImGui::End();
    }

    void ConsolePanel::imgui_render_header()
    {
        DS_PROFILE_FUNCTION();
        ImGuiStyle& style = ImGui::GetStyle();
        ImGui::AlignTextToFramePadding();
        // Button for advanced settings
        {
            ImGuiHelper::ScopedColour buttonColour(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            if (ImGui::Button(U8CStr2CStr(ICON_MDI_COGS)))
                ImGui::OpenPopup("SettingsPopup");
        }
        if (ImGui::BeginPopup("SettingsPopup"))
        {
            // Checkbox for scrolling lock
            ImGui::Checkbox("Scroll to bottom", &s_AllowScrollingToBottom);

            // Button to clear the console
            if (ImGui::Button("Clear console"))
                flush();

            ImGui::EndPopup();
        }

        ImGui::SameLine();
        ImGui::TextUnformatted(U8CStr2CStr(ICON_MDI_MAGNIFY));
        ImGui::SameLine();

        float spacing = ImGui::GetStyle().ItemSpacing.x;
        ImGui::GetStyle().ItemSpacing.x = 2;
        float levelButtonWidth = (ImGui::CalcTextSize(Message::get_level_icon(Message::Level(1))) + ImGui::GetStyle().FramePadding * 2.0f).x;
        float levelButtonWidths = (levelButtonWidth + ImGui::GetStyle().ItemSpacing.x) * 6;

        {
            ImGuiHelper::ScopedFont boldFont(ImGui::GetIO().Fonts->Fonts[1]);
            ImGuiHelper::ScopedStyle frameBorder(ImGuiStyleVar_FrameBorderSize, 0.0f);
            ImGuiHelper::ScopedColour frameColour(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 0));
            Filter.Draw("###ConsoleFilter", ImGui::GetContentRegionAvail().x - (levelButtonWidths));
            ImGuiHelper::DrawItemActivityOutline(2.0f, false);
        }

        ImGui::SameLine(); // ImGui::GetWindowWidth() - levelButtonWidths);

        for (int i = 0; i < 6; i++)
        {
            ImGuiHelper::ScopedColour buttonColour(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            ImGui::SameLine();
            auto level = Message::Level(maths::Pow(2, i));

            bool levelEnabled = s_MessageBufferRenderFilter & level;
            if (levelEnabled)
                ImGui::PushStyleColor(ImGuiCol_Text, Message::get_render_colour(level));
            else
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5, 0.5f, 0.5f));

            if (ImGui::Button(Message::get_level_icon(level)))
            {
                s_MessageBufferRenderFilter ^= level;
            }

            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("%s", Message::get_level_name(level));
            }
            ImGui::PopStyleColor();
        }

        ImGui::GetStyle().ItemSpacing.x = spacing;

        if (!Filter.IsActive())
        {
            ImGui::SameLine();
            ImGuiHelper::ScopedFont boldFont(ImGui::GetIO().Fonts->Fonts[1]);
            ImGui::SetCursorPosX(ImGui::GetFontSize() * 4.0f);
            ImGuiHelper::ScopedStyle padding(ImGuiStyleVar_FramePadding, ImVec2(0.0f, ImGui::GetStyle().FramePadding.y));
            ImGui::TextUnformatted("Search...");
        }
    }

    enum MyItemColumnID
    {
        MyItemColumnID_Time,
        MyItemColumnID_Message,
        MyItemColumnID_Type
    };

    void ConsolePanel::imgui_render_messages()
    {
        DS_PROFILE_FUNCTION();
        // ImGui::BeginChild("ScrollRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
        if (ImGui::BeginTable("Messages", 3, ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg))
        {

            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthFixed, 0.0f, MyItemColumnID_Type);
            ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthFixed, 0.0f, MyItemColumnID_Time);
            ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_NoSort, 0.0f, MyItemColumnID_Message);
            ImGui::TableSetupScrollFreeze(0, 1);

            ImGui::TableHeadersRow();
            // ImGuiHelper::AlternatingRowsBackground();

            ImGui::TableNextRow();

            auto DrawMessage = [](Message* message)
                {
                    if (s_MessageBufferRenderFilter & message->m_Level)
                    {
                        ImGui::TableNextColumn();
                        ImGui::PushStyleColor(ImGuiCol_Text, message->get_render_colour(message->m_Level));
                        auto levelIcon = message->get_level_icon(message->m_Level);
                        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetColumnWidth() - ImGui::CalcTextSize(levelIcon).x
                            - ImGui::GetScrollX() - 2 * ImGui::GetStyle().ItemSpacing.x);
                        ImGui::TextUnformatted(levelIcon);

                        if (ImGui::IsItemHovered())
                        {
                            ImGui::SetTooltip("%s", Message::get_level_name(message->m_Level));
                        }
                        ImGui::PopStyleColor();

                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(message->m_Time.c_str());

                        ImGui::TableNextColumn();
                        message->on_imgui_render();
                        ImGui::TableNextRow();
                    }
                };

            auto messageStart = s_MessageBuffer.begin() + s_MessageBufferBegin;
            if (*messageStart) // If contains old message here
            {
                for (auto message = messageStart; message != s_MessageBuffer.end(); ++message)
                {
                    if (Filter.IsActive())
                    {
                        if (Filter.PassFilter((*message)->m_Message.c_str()))
                        {
                            DrawMessage((*message));
                        }
                    }
                    else
                    {
                        DrawMessage((*message));
                    }
                }
            }

            if (s_MessageBufferBegin != 0) // Skipped first messages in vector
            {
                for (auto message = s_MessageBuffer.begin(); message != messageStart; ++message)
                {
                    if (*message)
                    {
                        if (Filter.IsActive())
                        {
                            if (Filter.PassFilter((*message)->m_Message.c_str()))
                            {
                                DrawMessage((*message));
                            }
                        }
                        else
                        {
                            DrawMessage((*message));
                        }
                    }
                }
            }

            if (s_RequestScrollToBottom && ImGui::GetScrollMaxY() > 0)
            {
                ImGui::SetScrollHereY(1.0f);
                s_RequestScrollToBottom = false;
            }
            ImGui::EndTable();
        }
    }

    ConsolePanel::Message::Message(const std::string& message, Level level, const std::string& source, int threadID, const std::string& time)
        : m_Message(message)
        , m_Level(level)
        , m_Source(source)
        , m_ThreadID(threadID)
        , m_MessageID(std::hash<std::string>()(message) + std::hash<std::string>()(time))
        , m_Time(time)
    {
    }

    void ConsolePanel::Message::on_imgui_render()
    {
        DS_PROFILE_FUNCTION();

        ImGuiHelper::ScopedID scopedID((int)m_MessageID);
        ImGui::TextUnformatted(m_Message.c_str());

        bool clicked = false;
        if (ImGui::IsItemClicked())
            clicked = true;

        if (ImGui::BeginPopupContextItem(m_Message.c_str()))
        {
            if (ImGui::MenuItem("Copy"))
            {
                ImGui::SetClipboardText(m_Message.c_str());
            }

            ImGui::EndPopup();
        }
        static bool m_DetailedPanelOpen = false;
        if (clicked)
        {
            ImGui::OpenPopup("Message");
            ImVec2 size = ImGui::GetMainViewport()->Size;
            ImGui::SetNextWindowSize({ size.x * 0.5f, size.y * 0.5f });
            ImGui::SetNextWindowPos({ size.x / 2.0f, size.y / 2.5f }, 0, { 0.5, 0.5 });
            m_DetailedPanelOpen = true;
        }

        if (m_DetailedPanelOpen)
        {
            if (ImGui::BeginPopupModal("Message", &m_DetailedPanelOpen, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
            {
                ImGui::TextWrapped("Message : %s", m_Message.c_str());

                if (ImGui::BeginPopupContextItem(m_Message.c_str()))
                {
                    if (ImGui::MenuItem("Copy"))
                    {
                        ImGui::SetClipboardText(m_Message.c_str());
                    }

                    ImGui::EndPopup();
                }

                ImGui::TextWrapped("Source : %s", m_Source.c_str());

                ImGui::Text("Time : %s", m_Time.c_str());
                ImGui::Text("Type : %s", Message::get_level_name(m_Level));

                ImGui::EndPopup();
            }
        }

        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", m_Source.c_str());
        }

        if (m_Count > 1)
        {
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - (m_Count > 99 ? ImGui::GetFontSize() * 1.7f : ImGui::GetFontSize() * 1.5f));
            ImGui::Text("%d", m_Count);
        }
    }

    const char* ConsolePanel::Message::get_level_icon(Level level)
    {
        switch (level)
        {
        case ConsolePanel::Message::Level::Trace:
            return U8CStr2CStr(ICON_MDI_MESSAGE_TEXT);
        case ConsolePanel::Message::Level::Info:
            return U8CStr2CStr(ICON_MDI_INFORMATION);
        case ConsolePanel::Message::Level::Debug:
            return U8CStr2CStr(ICON_MDI_BUG);
        case ConsolePanel::Message::Level::Warn:
            return U8CStr2CStr(ICON_MDI_ALERT);
        case ConsolePanel::Message::Level::Error:
            return U8CStr2CStr(ICON_MDI_CLOSE_OCTAGON);
        case ConsolePanel::Message::Level::Critical:
            return U8CStr2CStr(ICON_MDI_ALERT_OCTAGRAM);
        default:
            return "Unknown name";
        }
    }

    const char* ConsolePanel::Message::get_level_name(Level level)
    {
        switch (level)
        {
        case ConsolePanel::Message::Level::Trace:
            return U8CStr2CStr(ICON_MDI_MESSAGE_TEXT " Trace");
        case ConsolePanel::Message::Level::Info:
            return U8CStr2CStr(ICON_MDI_INFORMATION " Info");
        case ConsolePanel::Message::Level::Debug:
            return U8CStr2CStr(ICON_MDI_BUG " Debug");
        case ConsolePanel::Message::Level::Warn:
            return U8CStr2CStr(ICON_MDI_ALERT " Warning");
        case ConsolePanel::Message::Level::Error:
            return U8CStr2CStr(ICON_MDI_CLOSE_OCTAGON " Error");
        case ConsolePanel::Message::Level::Critical:
            return U8CStr2CStr(ICON_MDI_ALERT_OCTAGRAM " Critical");
        default:
            return "Unknown name";
        }
    }

    glm::vec4 ConsolePanel::Message::get_render_colour(Level level)
    {
        switch (level)
        {
        case ConsolePanel::Message::Level::Trace:
            return { 0.75f, 0.75f, 0.75f, 1.00f }; // Gray
        case ConsolePanel::Message::Level::Info:
            return { 0.40f, 0.70f, 1.00f, 1.00f }; // Blue
        case ConsolePanel::Message::Level::Debug:
            return { 0.00f, 0.50f, 0.50f, 1.00f }; // Cyan
        case ConsolePanel::Message::Level::Warn:
            return { 1.00f, 1.00f, 0.00f, 1.00f }; // Yellow
        case ConsolePanel::Message::Level::Error:
            return { 1.00f, 0.25f, 0.25f, 1.00f }; // Red
        case ConsolePanel::Message::Level::Critical:
            return { 0.6f, 0.2f, 0.8f, 1.00f }; // Purple
        default:
            return { 1.00f, 1.00f, 1.00f, 1.00f };
        }
    }
}
