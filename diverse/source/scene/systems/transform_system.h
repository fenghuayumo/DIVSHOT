#pragma once

#include <entt/entity/fwd.hpp>
#include <entt/entity/registry.hpp>
#include <glm/mat4x4.hpp>

namespace diverse
{

    class TransformSystem
    {
    public:
        static void init(entt::registry& registry);
        static void shutdown(entt::registry& registry);

        // Main update function - called once per frame
        static void update(entt::registry& registry);

        // Mark a transform as dirty (recursively marks children)
        static void mark_dirty(entt::registry& registry, entt::entity entity);

    private:
        // EnTT callbacks for Transform component
        static void on_transform_construct(entt::registry& registry, entt::entity entity);
        static void on_transform_update(entt::registry& registry, entt::entity entity);

        // EnTT callbacks for Parent component changes
        static void on_parent_changed(entt::registry& registry, entt::entity entity);

        // Helper: recursively mark children as dirty
        static void mark_children_dirty(entt::registry& registry, entt::entity parent);

        // Helper: compute global transform for an entity
        static void compute_global_transform(entt::registry& registry, entt::entity entity);
    };

}
