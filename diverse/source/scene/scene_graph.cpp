#include "precompile.h"
#include "scene_graph.h"

#include "systems/hierarchy_system.h"
#include "systems/transform_system.h"
#include "core/profiler.h"

DISABLE_WARNING_PUSH
DISABLE_WARNING_CONVERSION_TO_SMALLER_TYPE
#include <entt/entt.hpp>
DISABLE_WARNING_POP

namespace diverse
{

    SceneGraph::SceneGraph()
    {
    }

    void SceneGraph::init(entt::registry& registry)
    {
        HierarchySystem::init(registry);
        TransformSystem::init(registry);
    }

    void SceneGraph::shutdown(entt::registry& registry)
    {
        HierarchySystem::shutdown(registry);
        TransformSystem::shutdown(registry);
    }

    void SceneGraph::update(entt::registry& registry)
    {
        DS_PROFILE_FUNCTION();
        TransformSystem::update(registry);
    }

    void SceneGraph::disable_on_construct(bool disable, entt::registry& registry)
    {
        callbacks_enabled = !disable;
        // Note: New systems don't require disabling callbacks
        // This is kept for legacy compatibility during scene loading
    }

}
