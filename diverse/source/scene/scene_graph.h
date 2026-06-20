#pragma once

#include <entt/entity/fwd.hpp>
#include <entt/entity/registry.hpp>

namespace diverse
{

    class SceneGraph
    {
    public:
        SceneGraph();
        ~SceneGraph() = default;

        void init(entt::registry& registry);
        void shutdown(entt::registry& registry);

        // Main update - forwards to new systems
        void update(entt::registry& registry);

        // Legacy compatibility - disable callbacks during batch operations
        void disable_on_construct(bool disable, entt::registry& registry);

    private:
        bool callbacks_enabled = true;
    };

}
