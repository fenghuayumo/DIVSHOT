#pragma once

#include "cpu_assets.h"
#include "primitive_type.h"
#include <glm/fwd.hpp>
#include <memory>

namespace diverse
{
    DS_EXPORT std::shared_ptr<MeshAsset> create_primitive_mesh(PrimitiveType type);

    DS_EXPORT std::shared_ptr<MeshAsset> create_quad();
    DS_EXPORT std::shared_ptr<MeshAsset> create_quad(float x, float y, float width, float height);
    DS_EXPORT std::shared_ptr<MeshAsset> create_quad(const glm::vec2& position, const glm::vec2& size);
    DS_EXPORT std::shared_ptr<MeshAsset> create_cube();
    DS_EXPORT std::shared_ptr<MeshAsset> create_pyramid();
    DS_EXPORT std::shared_ptr<MeshAsset> create_sphere(uint32_t xSegments = 32, uint32_t ySegments = 32);
    DS_EXPORT std::shared_ptr<MeshAsset> create_capsule(float radius = 0.5f, float midHeight = 2.0f, int radialSegments = 64, int rings = 8);
    DS_EXPORT std::shared_ptr<MeshAsset> create_plane(float width, float height, const glm::vec3& normal);
    DS_EXPORT std::shared_ptr<MeshAsset> create_cylinder(float bottomRadius = 0.5f, float topRadius = 0.5f, float height = 1.0f, int radialSegments = 64, int rings = 8);
}
