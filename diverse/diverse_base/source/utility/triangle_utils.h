#pragma once
#include <glm/glm.hpp>

namespace diverse
{
    auto point_in_triangle_barycentric(
        const glm::vec2& p, 
        const glm::vec2& a, 
        const glm::vec2& b, 
        const glm::vec2& c) ->bool;

    auto calculate_barycentric(
        const glm::vec2& uv, 
        const glm::vec2& a, 
        const glm::vec2& b, 
        const glm::vec2& c, 
        glm::vec3& weights) ->bool; 
}