#pragma once

#include <entt/entity/fwd.hpp>
#include <entt/entity/registry.hpp>

namespace diverse
{

    class HierarchySystem
    {
    public:
        static void init(entt::registry& registry);
        static void shutdown(entt::registry& registry);

        // Reparent an entity to a new parent
        static void reparent(entt::registry& registry, entt::entity entity, entt::entity new_parent);

    private:
        // EnTT callbacks for Parent component
        static void on_parent_construct(entt::registry& registry, entt::entity entity);
        static void on_parent_destroy(entt::registry& registry, entt::entity entity);

        // Helper: add child to parent's Children component
        static void add_child_to_parent(entt::registry& registry, entt::entity child, entt::entity parent);

        // Helper: remove child from parent's Children component
        static void remove_child_from_parent(entt::registry& registry, entt::entity child, entt::entity parent);
    };

}
