#pragma once
#include "maths/maths_utils.h"
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

namespace diverse
{
    namespace maths
    {
        class Rect
        {
        public:
            Rect();
            Rect(const glm::vec2& position, const glm::vec2& size);
            Rect(const glm::vec4& rect);
            Rect(float x, float y, float width, float height);

            void set_position(const glm::vec2& position);
            void set_size(const glm::vec2& size);
            void set(const glm::vec2& position, const glm::vec2& size);
            void set(float x, float y, float width, float height);

            const glm::vec2& get_position() const;
            const glm::vec2& get_size() const;
            const glm::vec4& get() const;

            bool is_inside(const glm::vec2& point) const;
            bool is_inside(float x, float y) const;
            bool is_inside(const Rect& rect) const;

            bool intersects(const Rect& rect) const;

            void rotate(float angle);

            void transform(const glm::mat4& transform);

            void set_center(const glm::vec2& center);
            void set_center(float x, float y);

            const glm::vec2& get_center() const;

        private:
            glm::vec2 m_Position;
            glm::vec2 m_Size;
            glm::vec4 m_Rect;
            glm::vec2 m_Center;
            float m_Radius;
            float m_Angle;
        };
    }
}
