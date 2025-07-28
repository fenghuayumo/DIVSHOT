#include "precompile.h"
#include "scene_manager.h"

//#include "physics/B2PhysicsEngine/B2PhysicsEngine.h"
#include "scene.h"
#include "engine/application.h"
#include "scene.h"
//#include "physics/DiversePhysicsEngine/DiversePhysicsEngine.h"
#include "engine/file_system.h"
#include "utility/string_utils.h"

namespace diverse
{

    SceneManager::SceneManager()
        : m_SceneIdx(0)
        , m_CurrentScene(nullptr)
    {
    }

    SceneManager::~SceneManager()
    {
        m_SceneIdx = 0;

        if(m_CurrentScene)
        {
            DS_LOG_INFO("Exiting scene : {0}", m_CurrentScene->get_scene_name());
            m_CurrentScene->on_cleanup_scene();
        }

        m_vpAllScenes.Clear();
    }

    void SceneManager::switch_scene()
    {
        switch_scene((m_SceneIdx + 1) % m_vpAllScenes.Size());
    }

    void SceneManager::switch_scene(int idx)
    {
        m_QueuedSceneIndex = idx;
        m_SwitchingScenes  = true;
    }

    void SceneManager::switch_scene(const std::string& name)
    {
        bool found        = false;
        m_SwitchingScenes = true;
        uint32_t idx      = 0;
        for(uint32_t i = 0; !found && i < m_vpAllScenes.Size(); ++i)
        {
            if(m_vpAllScenes[i]->get_scene_name() == name)
            {
                found = true;
                idx   = i;
                break;
            }
        }

        if(found)
        {
            if (m_CurrentScene && m_CurrentScene->get_scene_name() == name)
            {
                DS_LOG_INFO("Scene already active : {0}", name.c_str());
				return;
            }
            switch_scene(idx);
        }
        else
        {
            DS_LOG_ERROR("Unknown Scene Alias : {0}", name.c_str());
        }
    }

    void SceneManager::apply_scene_switch()
    {
        if(m_SwitchingScenes == false)
        {
            if(m_CurrentScene)
                return;

            if(m_vpAllScenes.Empty())
                m_vpAllScenes.Emplace(createSharedPtr<Scene>("NewScene"));

            m_QueuedSceneIndex = 0;
        }

        if(m_QueuedSceneIndex < 0 || m_QueuedSceneIndex >= static_cast<int>(m_vpAllScenes.Size()))
        {
            DS_LOG_ERROR("Invalid Scene Index : {0}", m_QueuedSceneIndex);
            m_QueuedSceneIndex = 0;
        }

        auto& app = Application::get();

        // Clear up old scene
        if(m_CurrentScene)
        {
            DS_LOG_INFO("Exiting scene : {0}", m_CurrentScene->get_scene_name());
            m_CurrentScene->on_cleanup_scene();
            //app.on_exit_scene();
        }

        m_SceneIdx     = m_QueuedSceneIndex;
        m_CurrentScene = m_vpAllScenes[m_QueuedSceneIndex].get();

        std::string physicalPath;
        if(diverse::FileSystem::get().resolve_physical_path("//assets/scenes/" + m_CurrentScene->get_scene_name() + ".dsn", physicalPath))
        {
            auto newPath = stringutility::remove_name(physicalPath);
            m_CurrentScene->deserialise(newPath, false);
        }

        auto screenSize = app.get_window_size();
        m_CurrentScene->set_screen_size(static_cast<uint32_t>(screenSize[0]), static_cast<uint32_t>(screenSize[1]));

        if (app.get_editor_state() == EditorState::Play)
            m_CurrentScene->on_init();

        Application::get().handle_new_scene(m_CurrentScene);

        DS_LOG_INFO("Scene switched to : {0}", m_CurrentScene->get_scene_name().c_str());

        m_SwitchingScenes = false;
    }

    Vector<std::string> SceneManager::get_scene_names()
    {
        Vector<std::string> names;

        for(auto& scene : m_vpAllScenes)
        {
            names.Emplace(scene->get_scene_name());
        }

        return names;
    }

    int SceneManager::enqueue_scene_from_file(const std::string& filePath)
    {
        /*    auto found = std::find(m_SceneFilePaths.begin(), m_SceneFilePaths.end(), filePath);
            if(found != m_SceneFilePaths.end())
                return int(found - m_SceneFilePaths.begin());*/

        for(uint32_t i = 0; i < m_SceneFilePaths.Size(); ++i)
        {
            if(m_SceneFilePaths[i] == filePath)
            {
                return i;
            }
        }

        m_SceneFilePaths.Emplace(filePath);

        auto name  = stringutility::remove_file_extension(stringutility::get_file_name(filePath));
        auto scene = new Scene(name);
        enqueue_scene(scene);
        return int(m_vpAllScenes.Size()) - 1;
    }

    void SceneManager::enqueue_scene(Scene* scene)
    {
        m_vpAllScenes.Emplace(SharedPtr<Scene>(scene));
        DS_LOG_INFO("Enqueued scene : {0}", scene->get_scene_name().c_str());
    }

    bool SceneManager::contains_scene(const std::string& filePath)
    {
        for(uint32_t i = 0; i < m_SceneFilePaths.Size(); ++i)
        {
            if(m_SceneFilePaths[i] == filePath)
            {
                return true;
            }
        }

        return false;
    }

    void SceneManager::load_current_list()
    {
        for(auto& filePath : m_SceneFilePathsToLoad)
        {
            std::string newPath;
            FileSystem::get().absolute_path_2_fileSystem(filePath, newPath);
            enqueue_scene_from_file(filePath);
        }

        m_SceneFilePathsToLoad.Clear();
    }

    const Vector<std::string>& SceneManager::get_scene_file_paths()
    {
        m_SceneFilePaths.Clear();
        for(auto scene : m_vpAllScenes)
            m_SceneFilePaths.Emplace("//assets/scenes/" + scene->get_scene_name());
        return m_SceneFilePaths;
    }
}
