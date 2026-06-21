#pragma once
#include "core/reference.h"
#include "serialisation.h"
#include "json_scene_loader.h"
#include <sol/forward.hpp>
#include <glm/vec3.hpp>
#include "maths/bounding_box.h"
DISABLE_WARNING_PUSH
DISABLE_WARNING_CONVERSION_TO_SMALLER_TYPE
#include <entt/entity/registry.hpp>
DISABLE_WARNING_POP

namespace diverse
{

// Forward declarations
class TimeStep;
class Camera;
class EntityManager;
class Entity;
class SceneGraph;

// New component forward declarations
struct Transform;
struct GlobalTransform;
struct Parent;
class Children;

namespace schedule
{
    class Schedule;
}


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

        /**
         * @brief Register core systems for this scene.
         *
         * Called during scene initialization to set up the default systems.
         * Override this method to add custom systems to your scene.
         * Call the base class method to ensure core systems are registered.
         */
        virtual void register_systems();

        /**
         * @brief Build the system schedule.
         *
         * Call this after registering all systems to build the dependency graph.
         */
        void build_schedule();

        // Delete all contained Objects
        //    - This is the default action upon firing on_cleanup_scene()
        void delete_all_game_objects();

        // The friendly name associated with this scene instance
        const std::string& get_scene_name() const
        {
            return scene_name;
        }
        void set_name(const std::string& name)
        {
            scene_name = name;
        }

        void set_screen_width(uint32_t width)
        {
            screen_width = width;
        }
        void set_screen_height(uint32_t height)
        {
            screen_height = height;
        }

        void set_screen_size(uint32_t width, uint32_t height);

        uint32_t get_screen_width() const
        {
            return screen_width;
        }

        uint32_t get_screen_height() const
        {
            return screen_height;
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

        EntityManager* get_entity_manager() { return entity_manager.get(); }

        /**
         * @brief Get the schedule for system registration and execution
         */
        schedule::Schedule* get_schedule() { return m_schedule.get(); }
        const schedule::Schedule* get_schedule() const { return m_schedule.get(); }

        virtual void serialise(const std::string& filePath, bool binary = false);
        virtual void deserialise(const std::string& filePath, bool binary = false);

        template <typename Archive>
        void save(Archive& archive) const
        {
            archive(cereal::make_nvp("Version", SceneSerialisationVersion));
            archive(cereal::make_nvp("Scene Name", scene_name));
        }

        template <typename Archive>
        void load(Archive& archive)
        {
            archive(cereal::make_nvp("Version", SceneSerialisationVersion));
            archive(cereal::make_nvp("Scene Name", scene_name));
        }
        
        maths::BoundingBox  get_world_bounding_box() const;
        int get_scene_version() const
        {
            return scene_serialisation_version;
        }
        Entity get_keyFrame_entity();

        const bool& serialised() { return has_serialised;}

        const SceneImportMeta& get_import_meta() const { return import_meta; }
        void set_import_meta(const SceneImportMeta& meta) { import_meta = meta; }
        void clear_import_meta() { import_meta = {}; }

        const std::string& get_source_file_path() const { return source_file_path; }
        void set_source_file_path(const std::string& path) { source_file_path = path; }
    protected:
        std::string scene_name;
        std::string source_file_path;
        int scene_serialisation_version = 0;

        UniquePtr<EntityManager> entity_manager;
        UniquePtr<SceneGraph> scene_graph;
        UniquePtr<schedule::Schedule> m_schedule;

        uint32_t screen_width;
        uint32_t screen_height;
        u32 current_edit_splat_ent_id;
        bool has_serialised = false;
        SceneImportMeta import_meta;
    private:
        NONCOPYABLE(Scene)

        friend class Entity;
    };
}
