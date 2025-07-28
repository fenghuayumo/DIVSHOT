#pragma once

namespace diverse
{
    class TimeStep;
    class Scene;

    class DS_EXPORT ISystem
    {
    public:
        ISystem()          = default;
        virtual ~ISystem() = default;

        virtual bool on_init()                                   = 0;
        virtual void on_update(const TimeStep& dt, Scene* scene) = 0;
        virtual void on_imgui()                                  = 0;
        virtual void on_debug_draw()                              = 0;

        inline const std::string& get_name() const
        {
            return m_DebugName;
        }

    protected:
        std::string m_DebugName;
    };
}
