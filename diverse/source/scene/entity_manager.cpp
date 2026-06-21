#include "precompile.h"
#include "entity.h"
#include "entity_manager.h"
#include "maths/random.h"

namespace diverse
{
    Entity EntityManager::create()
    {
        DS_PROFILE_FUNCTION();
        auto e = m_registry.create();
        m_registry.emplace<IDComponent>(e);
        return Entity(e, m_scene);
    }

    Entity EntityManager::create(const std::string& name)
    {
        DS_PROFILE_FUNCTION();
        auto e = m_registry.create();
        m_registry.emplace<NameComponent>(e, name);
        m_registry.emplace<IDComponent>(e);
        return Entity(e, m_scene);
    }

    void EntityManager::clear()
    {
        DS_PROFILE_FUNCTION();
        for(auto [entity] : m_registry.storage<entt::entity>().each())
        {
            m_registry.destroy(entity);
        }

        m_registry.clear();
    }

    Entity EntityManager::get_entity_by_uuid(uint64_t id)
    {
        DS_PROFILE_FUNCTION();

        auto view = m_registry.view<IDComponent>();
        for(auto entity : view)
        {
            auto& id_component = m_registry.get<IDComponent>(entity);
            if(id_component.ID == id)
                return Entity(entity, m_scene);
        }

        DS_LOG_WARN("Entity not found by ID");
        return Entity {};
    }
}
