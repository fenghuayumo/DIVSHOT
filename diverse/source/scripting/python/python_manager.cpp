// #include "precompile.h"
// #include "python_manager.h"
// #include "maths/transform.h"
// #include "core/engine.h"
// #include "core/os/input.h"

// #include "scene/scene.h"
// #include "scene/camera/camera.h"
// #include "scene/component/components.h"
// #include "scene/scene_graph.h"
// #include "scene/entity.h"
// #include "scene/entity_factory.h"
// #include "scene/entity_manager.h"
// #include "python_script_component.h"
// #include "core/profiler.h"

// #if __has_include(<filesystem>)
// #include <filesystem>
// #elif __has_include(<experimental/filesystem>)
// #include <experimental/filesystem>
// #endif
// #include <pybind11/pybind11.h>
// #include <pybind11/numpy.h>
// #include <pybind11/functional.h>
// #include <pybind11/numpy.h>
// #include <pybind11/stl.h>
// #include <pybind11/embed.h>
// namespace py = pybind11;
// using namespace py::literals;

// PYBIND11_EMBEDDED_MODULE(dvs_log, m) {
//     //m.def();
// }

// PYBIND11_EMBEDDED_MODULE(dvs_ecs, m) {
//     //m.def();
// }

// PYBIND11_EMBEDDED_MODULE(dvs_app, m) {
//     //m.def();
// }
// PYBIND11_EMBEDDED_MODULE(dvs_maths, m) {
//     //m.def();
// }

// PYBIND11_EMBEDDED_MODULE(dvs_scene, m) {
//     //m.def();
// }
// PYBIND11_EMBEDDED_MODULE(dvs_imgui, m) {
//     //m.def();
// }
// PYBIND11_EMBEDDED_MODULE(dvs_input, m) {
//     //m.def();
// }


// namespace diverse
// {

//     PythonManager::PythonManager()
//     {
//     }

//     PythonManager::~PythonManager()
//     {
//         //Py_Finalize();
//     }
    
//     void PythonManager::on_init()
//     {
//         //_putenv_s("PYTHON_HOME","");
//         //_putenv_s("PYTHON_PATH","");
//         _putenv_s("PATH","../Python/");
//     }

//     void PythonManager::on_init(Scene* scene)
//     {
//         DS_PROFILE_FUNCTION();
//         auto& registry = scene->get_registry();

//         auto view = registry.view<PythonScriptComponent>();

//         if (view.empty())
//             return;
//         for (auto entity : view)
//         {
//             auto& script = registry.get<PythonScriptComponent>(entity);
//             script.set_this_component();
//             script.on_init();
//         }

//     }

//     void PythonManager::on_update(Scene* scene)
//     {
//         DS_PROFILE_FUNCTION();
//         auto& registry = scene->get_registry();

//         auto view = registry.view<PythonScriptComponent>();

//         if (view.empty())
//             return;

//         float dt = (float)Engine::get().get_time_step().get_seconds();

//         for (auto entity : view)
//         {
//             auto& script = registry.get<PythonScriptComponent>(entity);
//             script.on_update(dt);
//         }
//     }

//     void PythonManager::on_new_project(const std::string& projectPath)
//     {
//         std::string scriptsPath;
//         FileSystem::get().resolve_physical_path("//assets/scripts", scriptsPath);
//     }

//     entt::entity GetEntityByName(entt::registry& registry, const std::string& name)
//     {
//         DS_PROFILE_FUNCTION();
//         entt::entity e = entt::null;
//         registry.view<NameComponent>().each([&](const entt::entity& entity, const NameComponent& component){
//             if (name == component.name)
//             {
//                 e = entity;
//             } 
//         });
//         return e;
//     }
//     void PythonManager::bind_maths_python()
//     {
//         DS_PROFILE_FUNCTION();
      
//     }

//     void PythonManager::bind_imgui_python()
//     {
//         DS_PROFILE_FUNCTION();

//     }

//     void PythonManager::bind_ecs_python()
//     {
//         DS_PROFILE_FUNCTION();
//     }

//     void PythonManager::bind_log_python()
//     {
//         DS_PROFILE_FUNCTION();
//     }

//     void PythonManager::bind_input_python()
//     {
//         DS_PROFILE_FUNCTION();
//     }

//     void PythonManager::bind_scene_python()
//     {
//         DS_PROFILE_FUNCTION();
//     }

//     void PythonManager::bind_app_python()
//     {
//         DS_PROFILE_FUNCTION();
//     }

//     void PythonManager::execute_python_script(const char* module_name)
//     {
       
//     }
// }