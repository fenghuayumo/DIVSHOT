#include "precompile.h"
#include "transform_system.h"

#include "../components/transform_component.h"
#include "../components/global_transform_component.h"
#include "../components/parent_component.h"
#include "../components/children_component.h"
#include "core/profiler.h"

DISABLE_WARNING_PUSH
DISABLE_WARNING_CONVERSION_TO_SMALLER_TYPE
#include <entt/entt.hpp>
DISABLE_WARNING_POP

namespace diverse
{

    void TransformSystem::init(entt::registry& registry)
    {
        registry.on_construct<::diverse::Transform>().connect<&TransformSystem::on_transform_construct>();
        registry.on_update<::diverse::Transform>().connect<&TransformSystem::on_transform_update>();
        registry.on_construct<Parent>().connect<&TransformSystem::on_parent_changed>();
        registry.on_destroy<Parent>().connect<&TransformSystem::on_parent_changed>();
    }

    void TransformSystem::shutdown(entt::registry& registry)
    {
        registry.on_construct<::diverse::Transform>().disconnect<&TransformSystem::on_transform_construct>();
        registry.on_update<::diverse::Transform>().disconnect<&TransformSystem::on_transform_update>();
        registry.on_construct<Parent>().disconnect<&TransformSystem::on_parent_changed>();
        registry.on_destroy<Parent>().disconnect<&TransformSystem::on_parent_changed>();
    }

    void TransformSystem::update(entt::registry& registry)
    {
        DS_PROFILE_FUNCTION();

        // Phase 1: Collect all dirty entities in topological order
        std::vector<entt::entity> dirty_entities;

        // Helper lambda to check if an entity is dirty
        auto is_dirty = [&registry](entt::entity entity) -> bool {
            if (!registry.valid(entity))
                return false;
            if (auto* global = registry.try_get<GlobalTransform>(entity))
                return global->dirty;
            return false;
        };

        // Helper lambda to collect an entity and all its descendants
        auto collect_descendants = [&registry, &dirty_entities, &is_dirty](entt::entity root) {
            std::vector<entt::entity> stack;
            stack.push_back(root);

            while (!stack.empty())
            {
                entt::entity current = stack.back();
                stack.pop_back();

                if (is_dirty(current) && std::find(dirty_entities.begin(), dirty_entities.end(), current) == dirty_entities.end())
                {
                    dirty_entities.push_back(current);

                    // Add children to stack
                    if (registry.all_of<Children>(current))
                    {
                        auto& children = registry.get<Children>(current);
                        for (auto child : children)
                        {
                            if (registry.valid(child))
                            {
                                stack.push_back(child);
                            }
                        }
                    }
                }
            }
        };

        // Collect all dirty root entities (no parent) first
        {
            auto root_view = registry.view<::diverse::Transform, GlobalTransform>(entt::exclude<Parent>);
            for (auto entity : root_view)
            {
                if (is_dirty(entity))
                {
                    collect_descendants(entity);
                }
            }
        }

        // Collect all dirty child entities (has parent)
        {
            auto child_view = registry.view<::diverse::Transform, GlobalTransform, Parent>();
            for (auto entity : child_view)
            {
                if (is_dirty(entity))
                {
                    // Only collect if parent is not already in the list (avoid duplicates)
                    auto& parent_comp = registry.get<Parent>(entity);
                    bool parent_in_list = std::find(dirty_entities.begin(), dirty_entities.end(), parent_comp.value) != dirty_entities.end();

                    if (!parent_in_list)
                    {
                        collect_descendants(entity);
                    }
                }
            }
        }

        if (dirty_entities.empty())
            return;

        // Phase 2: Topological sort (parents before children)
        // Using a simple comparator that checks ancestry
        std::sort(dirty_entities.begin(), dirty_entities.end(),
            [&registry](entt::entity a, entt::entity b) {
                // Check if a is an ancestor of b
                entt::entity current = b;
                while (current != entt::null && registry.valid(current))
                {
                    if (registry.all_of<Parent>(current))
                    {
                        auto& parent_comp = registry.get<Parent>(current);
                        if (parent_comp.value == a)
                            return true;
                        current = parent_comp.value;
                    }
                    else
                    {
                        break;
                    }
                }
                return false;
            }
        );

        // Phase 3: Update in sorted order
        for (auto entity : dirty_entities)
        {
            compute_global_transform(registry, entity);
        }
    }

    void TransformSystem::mark_dirty(entt::registry& registry, entt::entity entity)
    {
        if (entity == entt::null || !registry.valid(entity))
            return;

        if (registry.all_of<GlobalTransform>(entity))
        {
            auto& global = registry.get<GlobalTransform>(entity);
            global.dirty = true;
        }

        // Recursively mark children
        mark_children_dirty(registry, entity);
    }

    void TransformSystem::on_transform_construct(entt::registry& registry, entt::entity entity)
    {
        // Ensure GlobalTransform exists and is marked dirty
        auto& global = registry.get_or_emplace<GlobalTransform>(entity);
        global.dirty = true;
    }

    void TransformSystem::on_transform_update(entt::registry& registry, entt::entity entity)
    {
        // Mark as dirty when Transform is modified
        mark_dirty(registry, entity);
    }

    void TransformSystem::on_parent_changed(entt::registry& registry, entt::entity entity)
    {
        // Mark as dirty when parent relationship changes
        mark_dirty(registry, entity);
    }

    void TransformSystem::mark_children_dirty(entt::registry& registry, entt::entity parent)
    {
        if (!registry.all_of<Children>(parent))
            return;

        auto& children = registry.get<Children>(parent);
        for (auto child : children)
        {
            if (registry.valid(child))
            {
                if (registry.all_of<GlobalTransform>(child))
                {
                    auto& global = registry.get<GlobalTransform>(child);
                    global.dirty = true;
                }
                // Recursively mark grandchildren
                mark_children_dirty(registry, child);
            }
        }
    }

    void TransformSystem::compute_global_transform(entt::registry& registry, entt::entity entity)
    {
        if (!registry.valid(entity))
            return;

        auto& transform = registry.get<::diverse::Transform>(entity);
        auto& global = registry.get<GlobalTransform>(entity);

        // Get parent world matrix
        glm::mat4 parent_matrix(1.0f);
        if (registry.all_of<Parent>(entity))
        {
            auto& parent_comp = registry.get<Parent>(entity);
            if (parent_comp.value != entt::null && registry.valid(parent_comp.value))
            {
                if (registry.all_of<GlobalTransform>(parent_comp.value))
                {
                    auto& parent_global = registry.get<GlobalTransform>(parent_comp.value);
                    parent_matrix = parent_global.world_matrix;
                }
            }
        }

        // Compute world matrix: parent_world * local_transform
        global.world_matrix = parent_matrix * transform.compute_local_matrix();
        global.dirty = false;
    }

}
