#pragma once
#include "scene/components/transform_component.h"
#include <glm/glm.hpp>

namespace diverse
{
    class Pivot
    {
    public:
        Pivot() = default;
        explicit Pivot(const Transform& t)
            : transform(t)
        {
        }

        void set_transform(const Transform& t) { transform = t; }
        Transform& get_transform() { return transform; }
        const Transform& get_transform() const { return transform; }

        void set_from_world_matrix(const glm::mat4& world_matrix)
        {
            transform.set_local_transform(world_matrix);
        }

        glm::mat4 get_world_matrix() const
        {
            return transform.get_local_matrix();
        }

        Transform transform;
    };
}
