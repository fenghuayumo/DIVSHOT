
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <array>

#define RENDERER2D_VERTEX_SIZE sizeof(VertexData)
#define RENDERERTEXT_VERTEX_SIZE sizeof(TextVertexData)

namespace diverse
{
      struct DS_EXPORT VertexData
        {
            glm::vec3 vertex;
            glm::vec4 uv;
            glm::vec2 tid;
            glm::vec4 colour;

            bool operator==(const VertexData& other) const
            {
                return vertex == other.vertex && uv == other.uv && tid == other.tid && colour == other.colour;
            }
        };

        struct DS_EXPORT TextVertexData
        {
            glm::vec3 vertex;
            glm::vec2 uv;
            glm::vec2 tid;
            glm::vec4 colour;
            glm::vec4 outlineColour;

            bool operator==(const TextVertexData& other) const
            {
                return vertex == other.vertex && uv == other.uv && tid == other.tid && colour == other.colour && outlineColour == other.outlineColour;
            }
        };

        class DS_EXPORT Renderable2D
        {
        public:
            Renderable2D();
            virtual ~Renderable2D();

            std::shared_ptr<rhi::GpuTexture> GetTexture() { return m_Texture; }
            glm::vec2 GetPosition() const { return m_Position; }
            glm::vec2 get_scale() const { return m_Scale; }
            const glm::vec4& GetColour() const { return m_Colour; }
            const std::array<glm::vec2, 4>& GetUVs() const { return m_UVs; }

            static const std::array<glm::vec2, 4>& GetDefaultUVs();
            static const std::array<glm::vec2, 4>& GetUVs(const glm::vec2& min, const glm::vec2& max);

        protected:
            std::shared_ptr<rhi::GpuTexture> m_Texture;
            glm::vec2 m_Position;
            glm::vec2 m_Scale;
            glm::vec4 m_Colour;
            std::array<glm::vec2, 4> m_UVs;
        };
}