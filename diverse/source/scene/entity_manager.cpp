#include "precompile.h"
#include "entity.h"
#include "entity_manager.h"
#include "maths/random.h"

namespace diverse
{
    Entity EntityManager::create()
    {
        DS_PROFILE_FUNCTION();
        auto e = m_Registry.create();
        m_Registry.emplace<IDComponent>(e);
        return Entity(e, m_Scene);
    }

    Entity EntityManager::create(const std::string& name)
    {
        DS_PROFILE_FUNCTION();
        auto e = m_Registry.create();
        m_Registry.emplace<NameComponent>(e, name);
        m_Registry.emplace<IDComponent>(e);
        return Entity(e, m_Scene);
    }

    void EntityManager::clear()
    {
        DS_PROFILE_FUNCTION();
        for(auto [entity] : m_Registry.storage<entt::entity>().each())
        {
            m_Registry.destroy(entity);
        }

        m_Registry.clear();
    }

    Entity EntityManager::get_entity_by_uuid(uint64_t id)
    {
        DS_PROFILE_FUNCTION();

        auto view = m_Registry.view<IDComponent>();
        for(auto entity : view)
        {
            auto& idComponent = m_Registry.get<IDComponent>(entity);
            if(idComponent.ID == id)
                return Entity(entity, m_Scene);
        }

        DS_LOG_WARN("Entity not found by ID");
        return Entity {};
    }
}
