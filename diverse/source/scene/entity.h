#pragma once
#include "maths/transform.h"
#include "scene/scene.h"
#include "scene/scene_graph.h"
#include "core/profiler.h"
#include "utility/string_utils.h"
#include "core/uuid.h"

DISABLE_WARNING_PUSH
DISABLE_WARNING_CONVERSION_TO_SMALLER_TYPE
#include <entt/entt.hpp>
DISABLE_WARNING_POP

namespace diverse
{
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
            : m_EntityHandle(handle)
            , m_Scene(scene)
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
            return m_Scene->get_registry().emplace<T>(m_EntityHandle, std::forward<Args>(args)...);
        }

        template <typename T, typename... Args>
        T& get_or_add_component(Args&&... args)
        {
            DS_PROFILE_FUNCTION_LOW();
            return m_Scene->get_registry().get_or_emplace<T>(m_EntityHandle, std::forward<Args>(args)...);
        }

        template <typename T, typename... Args>
        void add_or_replace_component(Args&&... args)
        {
            DS_PROFILE_FUNCTION_LOW();
            m_Scene->get_registry().emplace_or_replace<T>(m_EntityHandle, std::forward<Args>(args)...);
        }

        template <typename T>
        T& get_component()
        {
            DS_PROFILE_FUNCTION_LOW();
            return m_Scene->get_registry().get<T>(m_EntityHandle);
        }

        template <typename T>
        T* try_get_component()
        {
            DS_PROFILE_FUNCTION_LOW();
            return m_Scene->get_registry().try_get<T>(m_EntityHandle);
        }

        template <typename T>
        bool has_component()
        {
            DS_PROFILE_FUNCTION_LOW();
            return m_Scene->get_registry().all_of<T>(m_EntityHandle);
        }

        template <typename T>
        void remove_component()
        {
            DS_PROFILE_FUNCTION_LOW();
            m_Scene->get_registry().remove<T>(m_EntityHandle);
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
                active = m_Scene->get_registry().get<ActiveComponent>(m_EntityHandle).active;

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

        maths::Transform& get_transform()
        {
            DS_PROFILE_FUNCTION_LOW();
            return m_Scene->get_registry().get<maths::Transform>(m_EntityHandle);
        }

        const maths::Transform& get_transform() const
        {
            DS_PROFILE_FUNCTION_LOW();
            return m_Scene->get_registry().get<maths::Transform>(m_EntityHandle);
        }

        uint64_t get_id()
        {
            DS_PROFILE_FUNCTION_LOW();
            return m_Scene->get_registry().get<IDComponent>(m_EntityHandle).ID;
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
            bool acceptable = false;
            auto hierarchyComponent = try_get_component<Hierarchy>();
            if (hierarchyComponent != nullptr)
            {
                acceptable = entity.m_EntityHandle != m_EntityHandle && (!entity.is_parent(*this)) && (hierarchyComponent->parent() != m_EntityHandle);
            }
            else
                acceptable = entity.m_EntityHandle != m_EntityHandle;

            if (!acceptable)
            {
                DS_LOG_WARN("Failed to parent entity!");
                return;
            }

            if (hierarchyComponent)
                Hierarchy::reparent(m_EntityHandle, entity.m_EntityHandle, m_Scene->get_registry(), *hierarchyComponent);
            else
            {
                m_Scene->get_registry().emplace<Hierarchy>(m_EntityHandle, entity.m_EntityHandle);
            }
        }

        Entity get_parent()
        {
            DS_PROFILE_FUNCTION_LOW();
            auto hierarchyComp = try_get_component<Hierarchy>();
            if (hierarchyComp)
                return Entity(hierarchyComp->parent(), m_Scene);
            else
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
            return m_EntityHandle;
        }

        operator uint32_t() const
        {
            return (uint32_t)m_EntityHandle;
        }

        operator bool() const
        {
            return m_EntityHandle != entt::null && m_Scene;
        }

        bool operator==(const Entity& other) const
        {
            return m_EntityHandle == other.m_EntityHandle && m_Scene == other.m_Scene;
        }

        bool operator!=(const Entity& other) const
        {
            return !(*this == other);
        }

        entt::entity get_handle() const
        {
            return m_EntityHandle;
        }

        void destroy()
        {
            DS_PROFILE_FUNCTION_LOW();
            m_Scene->get_registry().destroy(m_EntityHandle);
        }

        bool valid()
        {
            DS_PROFILE_FUNCTION_LOW();
            return m_Scene->get_registry().valid(m_EntityHandle) && m_Scene;
        }

        Scene* get_scene() const { return m_Scene; }

    private:
        entt::entity m_EntityHandle = entt::null;
        Scene* m_Scene = nullptr;

        friend class EntityManager;
    };
}
