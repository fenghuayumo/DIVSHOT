#pragma once
#include "serialisation.h"
#include <sol/forward.hpp>
#include <glm/vec3.hpp>
#include "maths/bounding_box.h"
DISABLE_WARNING_PUSH
DISABLE_WARNING_CONVERSION_TO_SMALLER_TYPE
#include <entt/entity/registry.hpp>
DISABLE_WARNING_POP

namespace diverse
{
    class TimeStep;
    class Event;
    class Camera;
    class EntityManager;
    class Entity;
    class SceneGraph;
    class Event;
    class WindowResizeEvent;


    class DS_EXPORT Scene
    {
    public:
        explicit Scene(const std::string& name);
        virtual ~Scene();

        // Called when scene is being activated, and will begin being rendered/updated.
        //	 - Initialise objects/physics here
        virtual void on_init();

        // Called when scene is being swapped and will no longer be rendered/updated
        //	 - Remove objects/physics here
        //	   Note: Default action here automatically delete all game objects
        virtual void on_cleanup_scene();

        virtual void render3D()
        {
        }
        virtual void render2D()
        {
        }

        // Update Scene Logic
        //   - Called once per frame and should contain all time-sensitive update logic
        //	   Note: This is time relative to seconds not milliseconds! (e.g. msec / 1000)
        virtual void on_update(const TimeStep& timeStep);
        virtual void on_imgui() {};
        virtual void on_event(Event& e);
        // Delete all contained Objects
        //    - This is the default action upon firing on_cleanup_scene()
        void delete_all_game_objects();

        // The friendly name associated with this scene instance
        const std::string& get_scene_name() const
        {
            return m_SceneName;
        }
        std::string& scene_name()
        {
			return m_SceneName;
		}
        void set_name(const std::string& name)
        {
            m_SceneName = name;
        }

        void set_screen_width(uint32_t width)
        {
            m_ScreenWidth = width;
        }
        void set_screen_height(uint32_t height)
        {
            m_ScreenHeight = height;
        }

        void set_screen_size(uint32_t width, uint32_t height);

        uint32_t get_screen_width() const
        {
            return m_ScreenWidth;
        }

        uint32_t get_screen_height() const
        {
            return m_ScreenHeight;
        }

        entt::registry& get_registry();

        void update_scene_graph();

        void duplicate_entity(Entity entity);
        void duplicate_entity(Entity entity, Entity parent);
        Entity create_entity();
        Entity create_entity(const std::string& name);
        Entity get_entity_by_uuid(uint64_t id);
        Entity instantiate_prefab(const std::string& path);
        void destroy_entity(Entity entity);
        void save_prefab(Entity entity, const std::string& path);

        EntityManager* get_entity_manager() { return m_EntityManager.get(); }

        virtual void serialise(const std::string& filePath, bool binary = false);
        virtual void deserialise(const std::string& filePath, bool binary = false);

        template <typename Archive>
        void save(Archive& archive) const
        {
            archive(cereal::make_nvp("Version", SceneSerialisationVersion));
            archive(cereal::make_nvp("Scene Name", m_SceneName));
        }

        template <typename Archive>
        void load(Archive& archive)
        {
            archive(cereal::make_nvp("Version", SceneSerialisationVersion));
            archive(cereal::make_nvp("Scene Name", m_SceneName));
        }
        
        maths::BoundingBox  get_world_bounding_box() const;
        int get_scene_version() const
        {
            return m_SceneSerialisationVersion;
        }
        Entity get_keyFrame_entity();

        const bool& serialised() { return has_serialised;}
    protected:
        std::string m_SceneName;
        int m_SceneSerialisationVersion = 0;

        UniquePtr<EntityManager> m_EntityManager;
        UniquePtr<SceneGraph> m_SceneGraph;

        uint32_t m_ScreenWidth;
        uint32_t m_ScreenHeight;
        u32      current_edit_splat_ent_id;
        bool     has_serialised = false;
    private:
        NONCOPYABLE(Scene)

        bool on_window_resize(WindowResizeEvent& e);

        friend class Entity;
    };
}
