#pragma once

#include "event.h"

#include <sstream>

namespace diverse
{
    class DS_EXPORT WindowResizeEvent : public Event
    {
    public:
        WindowResizeEvent(uint32_t width, uint32_t height)
            : m_Width(width)
            , m_Height(height)
            , m_DPIScale(1.0f)
        {
        }

        WindowResizeEvent(uint32_t width, uint32_t height, float dpiScale)
            : m_Width(width)
            , m_Height(height)
            , m_DPIScale(dpiScale)
        {
        }

        inline uint32_t GetWidth() const { return m_Width; }
        inline uint32_t GetHeight() const { return m_Height; }

        inline float GetDPIScale() const { return m_DPIScale; }

        std::string ToString() const override
        {
            std::stringstream ss;
            ss << "WindowResizeEvent: " << m_Width << ", " << m_Height << ", " << m_DPIScale;
            return ss.str();
        }

        EVENT_CLASS_TYPE(WindowResize)
        EVENT_CLASS_CATEGORY(EventCategoryApplication)

    private:
        uint32_t m_Width, m_Height;

        float m_DPIScale;
    };

    class DS_EXPORT WindowCloseEvent : public Event
    {
    public:
        WindowCloseEvent() { }

        EVENT_CLASS_TYPE(WindowClose)
        EVENT_CLASS_CATEGORY(EventCategoryApplication)
    };

    class DS_EXPORT WindowFileEvent : public Event
    {
    public:
        WindowFileEvent(int numDropped,const char** filenames)
        {
            m_FilePaths.resize(numDropped);
            for(auto i = 0;i<numDropped;i++)
                m_FilePaths[i] = filenames[i];
        }

        const std::vector<std::string>& GetFilePaths() const { return m_FilePaths; }

        std::string ToString() const override
        {
            std::stringstream ss;
            ss << "WindowFileEvent: " << m_FilePaths.size() << "\n";
            ss << "Files: ";
            for (auto i = 0; i < m_FilePaths.size(); i++)
            {
                ss << m_FilePaths[i] << ",";
            }
            return ss.str();
        }

        EVENT_CLASS_TYPE(WindowFile)
        EVENT_CLASS_CATEGORY(EventCategoryApplication)
    private:
        std::vector<std::string> m_FilePaths;
    };

    class DS_EXPORT AppTickEvent : public Event
    {
    public:
        AppTickEvent() { }

        EVENT_CLASS_TYPE(AppTick)
        EVENT_CLASS_CATEGORY(EventCategoryApplication)
    };

    class DS_EXPORT AppUpdateEvent : public Event
    {
    public:
        AppUpdateEvent() { }

        EVENT_CLASS_TYPE(AppUpdate)
        EVENT_CLASS_CATEGORY(EventCategoryApplication)
    };

    class DS_EXPORT AppRenderEvent : public Event
    {
    public:
        AppRenderEvent() { }

        EVENT_CLASS_TYPE(AppRender)
        EVENT_CLASS_CATEGORY(EventCategoryApplication)
    };

}
