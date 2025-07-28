#include "precompile.h"
#include "python_script_component.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"
#include "scene/entity.h"
#include "scene/entity_manager.h"
namespace diverse
{
    PythonScriptComponent::PythonScriptComponent()
    {
        scene = nullptr;
        file_name = "";
    }
    PythonScriptComponent::PythonScriptComponent(const std::string& fileName, Scene* scene)
    {
        scene = scene;
        file_name = fileName;

        // m_UUID = UUID();

        init();
    }

    void PythonScriptComponent::init()
    {
        load_script(file_name);
    }

    PythonScriptComponent::~PythonScriptComponent()
    {
        
    }

    void PythonScriptComponent::load_script(const std::string& fileName)
    {
        file_name = fileName;
        std::string physicalPath;
        if (!FileSystem::get().resolve_physical_path(fileName, physicalPath))
        {
            DS_LOG_ERROR("Failed to Load Lua script {0}", fileName);
            return;
        }

        FileSystem::get().absolute_path_2_fileSystem(file_name, file_name);
    }

    void PythonScriptComponent::on_init()
    {
    }

    void PythonScriptComponent::on_update(float dt)
    {
    }

    void PythonScriptComponent::reload()
    {
        init();
    }

    Entity PythonScriptComponent::get_current_entity()
    {
        // TODO: Faster alternative
        if (!scene)
            scene = Application::get().get_current_scene();

        auto entities = scene->get_entity_manager()->get_entities_with_type<PythonScriptComponent>();

        for (auto entity : entities)
        {
            PythonScriptComponent* comp = &entity.get_component<PythonScriptComponent>();
            if (comp->get_file_path() == get_file_path())
                return entity;
        }

        return Entity();
    }

    void PythonScriptComponent::set_this_component()
    {

    }

    void PythonScriptComponent::load(const std::string& fileName)
    {

        file_name = fileName;
        init();
    }
}