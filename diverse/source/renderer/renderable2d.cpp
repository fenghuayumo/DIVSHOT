#include "renderable2d.h"

namespace diverse
{
    Renderable2D::Renderable2D()
        : m_Position(0.0f, 0.0f)
        , m_Scale(1.0f, 1.0f)
        , m_Colour(1.0f, 1.0f, 1.0f, 1.0f)
        , m_UVs(GetDefaultUVs())
    {
    }

    Renderable2D::~Renderable2D()
    {
    }

    const std::array<glm::vec2, 4>& Renderable2D::GetDefaultUVs()
    {
        static std::array<glm::vec2, 4> defaultUVs = {
            glm::vec2(0.0f, 0.0f),
            glm::vec2(1.0f, 0.0f),
            glm::vec2(1.0f, 1.0f),
            glm::vec2(0.0f, 1.0f)
        };
        return defaultUVs;
    }

    const std::array<glm::vec2, 4>& Renderable2D::GetUVs(const glm::vec2& min, const glm::vec2& max)
    {
        static std::array<glm::vec2, 4> uvs;
        uvs[0] = min;
        uvs[1] = glm::vec2(max.x, min.y);
        uvs[2] = max;
        uvs[3] = glm::vec2(min.x, max.y);
        return uvs;
    }
}

