#pragma once
#include <glm/vec3.hpp>
#include <vector>
namespace diverse
{
    struct TriangleLight
    {
        std::vector<glm::vec3>   verts;
        glm::vec3 radiance;
        u32 instance_id;
        u32 mesh_id;
        u32 triangle_count;
        // auto transform(const Vector3f& translation);
    };

    struct MeshLightSet
    {
        std::vector<TriangleLight>  lights;
    };
}