#pragma once
#include "scene/components/transform_component.h"
#include "scene/components/global_transform_component.h"
#include "scene/components/parent_component.h"
#include "scene/components/children_component.h"
#include "scene/components/name_component.h"
#include "scene/components/active_component.h"
#include "scene/scene.h"
#include "scene/scene_graph.h"
#include "scene/systems/hierarchy_system.h"
#include "core/profiler.h"
#include "utility/string_utils.h"
#include "core/uuid.h"

DISABLE_WARNING_PUSH
DISABLE_WARNING_CONVERSION_TO_SMALLER_TYPE
#include <entt/entt.hpp>
DISABLE_WARNING_POP

namespace diverse
{

    // Forward declarations
    class HierarchySystem;

    struct IDComponent
    {
        UUID ID;

        template <typename Archive>
        void save(Archive& archive) const
        {
            uint64_t uuid = (uint64_t)ID;
            archive(uuid);
        }

        template <typename Archive>
        void load(Archive& archive)
        {
            uint64_t uuid;
            archive(uuid);

            ID = UUID(uuid);
        }
    };

    class Entity
    {
    public:
        Entity() = default;

        Entity(entt::entity handle, Scene* scene)
            : entity_handle(handle)
            , scene(scene)
        {
        }

        ~Entity()
        {
        }

        template <typename T, typename... Args>
        T& add_component(Args&&... args)
        {
            DS_PROFILE_FUNCTION_LOW();
#ifdef DS_DEBUG
            if (has_component<T>())
                DS_LOG_WARN("Attempting to add Component twice");
#endif
            return scene->get_registry().emplace<T>(entity_handle, std::forward<Args>(args)...);
        }

        template <typename T, typename... Args>
        T& get_or_add_component(Args&&... args)
        {
            DS_PROFILE_FUNCTION_LOW();
            return scene->get_registry().get_or_emplace<T>(entity_handle, std::forward<Args>(args)...);
        }

        template <typename T, typename... Args>
        void add_or_replace_component(Args&&... args)
        {
            DS_PROFILE_FUNCTION_LOW();
            scene->get_registry().emplace_or_replace<T>(entity_handle, std::forward<Args>(args)...);
        }

        template <typename T>
        T& get_component()
        {
            DS_PROFILE_FUNCTION_LOW();
            return scene->get_registry().get<T>(entity_handle);
        }

        template <typename T>
        T* try_get_component()
        {
            DS_PROFILE_FUNCTION_LOW();
            return scene->get_registry().try_get<T>(entity_handle);
        }

        template <typename T>
        bool has_component()
        {
            DS_PROFILE_FUNCTION_LOW();
            return scene->get_registry().all_of<T>(entity_handle);
        }

        template <typename T>
        void remove_component()
        {
            DS_PROFILE_FUNCTION_LOW();
            scene->get_registry().remove<T>(entity_handle);
        }

        template <typename T>
        void try_remove_component()
        {
            DS_PROFILE_FUNCTION_LOW();
            if (has_component<T>())
                remove_component<T>();
        }

        bool active()
        {
            DS_PROFILE_FUNCTION_LOW();
            bool active = true;
            if (has_component<ActiveComponent>())
                active = scene->get_registry().get<ActiveComponent>(entity_handle).active;

            auto parent = get_parent();
            if (parent)
                active &= parent.active();
            return active;
        }

        void set_active(bool isActive)
        {
            DS_PROFILE_FUNCTION_LOW();
            get_or_add_component<ActiveComponent>().active = isActive;
        }

        ::diverse::Transform& get_transform()
        {
            DS_PROFILE_FUNCTION_LOW();
            return scene->get_registry().get<::diverse::Transform>(entity_handle);
        }

        const ::diverse::Transform& get_transform() const
        {
            DS_PROFILE_FUNCTION_LOW();
            return scene->get_registry().get<::diverse::Transform>(entity_handle);
        }

        const glm::mat4& get_world_matrix() const
        {
            DS_PROFILE_FUNCTION_LOW();
            return scene->get_registry().get<GlobalTransform>(entity_handle).world_matrix;
        }

        uint64_t get_id()
        {
            DS_PROFILE_FUNCTION_LOW();
            return scene->get_registry().get<IDComponent>(entity_handle).ID;
        }

        const std::string& get_name()
        {
            DS_PROFILE_FUNCTION_LOW();
            auto nameComponent = try_get_component<NameComponent>();

            if (nameComponent)
                return nameComponent->name;
            else
            {
                static std::string tempName = "Entity";
                return tempName;
            }
        }

        void set_parent(Entity entity)
        {
            DS_PROFILE_FUNCTION_LOW();
            if (entity.entity_handle == entity_handle)
            {
                DS_LOG_WARN("Cannot parent entity to itself!");
                return;
            }

            // Prevent circular reference
            if (entity && entity.is_parent(*this))
            {
                DS_LOG_WARN("Cannot parent entity: would create circular reference!");
                return;
            }

            auto& registry = scene->get_registry();
            if (registry.all_of<Parent>(entity_handle))
            {
                auto& current_parent = registry.get<Parent>(entity_handle);
                if (current_parent.value == entity.entity_handle)
                    return; // Already parented to this entity
            }

            // Use HierarchySystem to reparent
            HierarchySystem::reparent(registry, entity_handle, entity.entity_handle);
        }

        Entity get_parent()
        {
            DS_PROFILE_FUNCTION_LOW();
            auto& registry = scene->get_registry();
            if (registry.all_of<Parent>(entity_handle))
            {
                auto& parent_comp = registry.get<Parent>(entity_handle);
                return Entity(parent_comp.value, scene);
            }
            return Entity(entt::null, nullptr);
        }

        std::vector<Entity> get_children();
        Entity* get_children(Arena* arena);
        void clear_children();
        Entity* get_children_temp();
        u32 get_child_count();
        bool is_parent(Entity potentialParent);

        operator entt::entity() const
        {
            return entity_handle;
        }

        operator uint32_t() const
        {
            return (uint32_t)entity_handle;
        }

        operator bool() const
        {
            return entity_handle != entt::null && scene;
        }

        bool operator==(const Entity& other) const
        {
            return entity_handle == other.entity_handle && scene == other.scene;
        }

        bool operator!=(const Entity& other) const
        {
            return !(*this == other);
        }

        entt::entity get_handle() const
        {
            return entity_handle;
        }

        void destroy()
        {
            DS_PROFILE_FUNCTION_LOW();
            scene->get_registry().destroy(entity_handle);
        }

        bool valid()
        {
            DS_PROFILE_FUNCTION_LOW();
            return scene->get_registry().valid(entity_handle) && scene;
        }

        Scene* get_scene() const { return scene; }

    private:
        entt::entity entity_handle = entt::null;
        Scene* scene = nullptr;

        friend class EntityManager;
    };
}
