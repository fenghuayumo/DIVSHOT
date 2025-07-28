#pragma once
#include "core/vector.h"

namespace diverse
{
    class Scene;

    class DS_EXPORT SceneManager
    {
    public:
        SceneManager();
        virtual ~SceneManager();

        // Jump to the next scene in the list or first scene if at the end
        void switch_scene();

        // Jump to scene index (stored in order they were originally added starting at zero)
        void switch_scene(int idx);

        // Jump to scene name
        void switch_scene(const std::string& name);

        void apply_scene_switch();

        // Get currently active scene (returns NULL if no scenes yet added)
        inline Scene* get_current_scene() const
        {
            return m_CurrentScene;
        }

        // Get currently active scene's index (return 0 if no scenes yet added)
        inline uint32_t get_current_scene_index() const
        {
            return m_SceneIdx;
        }

        // Get total number of enqueued scenes
        inline uint32_t scene_count() const
        {
            return static_cast<uint32_t>(m_vpAllScenes.Size());
        }

        Vector<std::string> get_scene_names();
        const Vector<SharedPtr<Scene>>& get_scenes() const
        {
            return m_vpAllScenes;
        }

        void set_switch_scene(bool switching)
        {
            m_SwitchingScenes = switching;
        }
        bool get_switching_scene() const
        {
            return m_SwitchingScenes;
        }

        int enqueue_scene_from_file(const std::string& filePath);
        void enqueue_scene(Scene* scene);

        bool contains_scene(const std::string& filePath);

        template <class T>
        void enqueue_scene(const std::string& name)
        {
            // T* scene = new T(name);
            m_vpAllScenes.Emplace(createSharedPtr<T>(name));
            DS_LOG_INFO("[SceneManager] - Enqueued scene : {0}", name.c_str());
        }

        const Vector<std::string>& get_scene_file_paths();

        void add_file_to_load_list(const std::string& filePath)
        {
            m_SceneFilePathsToLoad.Emplace(filePath);
        }

        void load_current_list();

    protected:
        uint32_t m_SceneIdx;
        Scene* m_CurrentScene;
        Vector<SharedPtr<Scene>> m_vpAllScenes;
        Vector<std::string> m_SceneFilePaths;
        Vector<std::string> m_SceneFilePathsToLoad;

    private:
        bool m_SwitchingScenes = false;
        int m_QueuedSceneIndex = -1;
        NONCOPYABLE(SceneManager)
    };
}
