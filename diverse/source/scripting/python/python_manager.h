#pragma once

#include "utility/singleton.h"
#include <Python.h>

namespace diverse
{
    class Scene;

    class PythonManager : public ThreadSafeSingleton<PythonManager>
    {
        friend class TSingleton<PythonManager>;

    public:
        PythonManager();
        ~PythonManager();

        void on_init();
        void on_init(Scene* scene);
        void on_update(Scene* scene);

        void on_new_project(const std::string& projectPath);

        void    bind_ecs_python();
        void    bind_log_python();
        void    bind_input_python();
        void    bind_scene_python();
        void    bind_app_python();
        void    bind_maths_python();
        void    bind_imgui_python();

        static std::vector<std::string>& get_identifiers() { return s_Identifiers; }

        static std::vector<std::string> s_Identifiers;

    public:
        void    execute_python_script(const char* module_name);
    private:
    };
}