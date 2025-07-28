#include "triangle_utils.h"

namespace diverse
{
    bool point_in_triangle_barycentric(
        const glm::vec2& p, 
        const glm::vec2& a,
        const glm::vec2& b, 
        const glm::vec2& c) 
    {
        auto v0 = c - a;
        auto v1 = b - a;
        auto v2 = p - a;

        float dot00 = glm::dot(v0,v0);
        float dot01 = glm::dot(v0,v1);
        float dot02 = glm::dot(v0,v2);
        float dot11 = glm::dot(v1,v1);
        float dot12 = glm::dot(v1,v2);

        float invDenom = 1.0f / (dot00 * dot11 - dot01 * dot01);
        float u = (dot11 * dot02 - dot01 * dot12) * invDenom;
        float v = (dot00 * dot12 - dot01 * dot02) * invDenom;

        return (u >= 0) && (v >= 0) && (u + v <= 1);
    }

    bool calculate_barycentric(
        const glm::vec2& uv, 
        const glm::vec2& a, 
        const glm::vec2& b, 
        const glm::vec2& c, 
        glm::vec3& weights) 
    {
        auto v0 = b - a;
        auto v1 = c - a;
        auto v2 = uv - a;

        float denom = v0.x * v1.y - v1.x * v0.y;
        if (std::abs(denom) < 1e-6) { // 退化三角形处理
            weights = glm::vec3(1.0f/3, 1.0f/3, 1.0f/3);
            return false;
        }

        float w1 = (v2.x * v1.y - v1.x * v2.y) / denom;
        float w2 = (v0.x * v2.y - v2.x * v0.y) / denom;
        float w0 = 1.0f - w1 - w2;

        weights = glm::vec3(w0, w1, w2);
        return (w0 >= 0 && w0 <= 1 && w1 >= 0 && w1 <= 1 && w2 >= 0 && w2 <= 1);
    }
}