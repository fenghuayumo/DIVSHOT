#pragma once
#include "maths/ray.h"
#include "maths/bounding_box.h"
#include "maths/bounding_sphere.h"
#include "maths/frustum.h"
#include "maths/transform.h"
#include "maths/plane.h"
#include "scene/component/light/rect_light.h"
#include "scene/component/light/point_light.h"
#include "scene/component/light/spot_light.h"
#include "scene/component/light/directional_light.h"
#include "scene/component/environment.h"
#include <array>
#include <vector>
#include <string>
namespace diverse
{
#define MAX_LOG_SIZE 25
#define LOG_TEXT_SIZE 14.0f
#define STATUS_TEXT_SIZE 16.0f

    struct LogEntry
    {
        glm::vec4 colour;
        std::string text;
    };

    struct DebugText
    {
        glm::vec4 colour;
        std::string text;
        float Size;
        glm::vec4 Position;
    };
    struct LineInfo
    {
        glm::vec3 p1;
        glm::vec3 p2;
        glm::vec4 col;

        LineInfo(const glm::vec3& pos1, const glm::vec3& pos2, const glm::vec4& colour)
        {
            p1  = pos1;
            p2  = pos2;
            col = colour;
        }
    };

    struct PointInfo
    {
        glm::vec3 p1;
        glm::vec4 col;
        float size;

        PointInfo(const glm::vec3& pos1, float s, const glm::vec4& colour)
        {
            p1   = pos1;
            size = s;
            col  = colour;
        }
    };

    struct TriangleInfo
    {
        glm::vec3 p1;
        glm::vec3 p2;
        glm::vec3 p3;
        glm::vec4 col;

        TriangleInfo(const glm::vec3& pos1, const glm::vec3& pos2, const glm::vec3& pos3, const glm::vec4& colour)
        {
            p1  = pos1;
            p2  = pos2;
            p3  = pos3;
            col = colour;
        }
    };

    struct DebugDrawFrame
    {
        std::array<std::vector<TriangleInfo>, 2> triangles;
        std::array<std::vector<LineInfo>, 2> lines;
        std::array<std::vector<LineInfo>, 2> thick_lines;
        std::array<std::vector<PointInfo>, 2> points;
    };

    namespace maths
    {
        class Sphere;
        class BoundingBox;
        class BoundingSphere;
        class Frustum;
        class Transform;
        class Ray;
    }

    class DebugRenderer
    {
    public:
        static auto init() -> void;
        static auto release() -> void;
        static auto reset() -> void;
        static auto capture_frame() -> DebugDrawFrame;

        DebugRenderer();
        ~DebugRenderer();

        // Note: Functions appended with '_ndt' (no depth testing) will always be rendered in the foreground. This can be useful for debugging things inside objects.

        // Draw Point (circle)
        static auto draw_point(const glm::vec3& pos, float point_radius, const glm::vec3& colour) -> void;
        static auto draw_point(const glm::vec3& pos, float point_radius, const glm::vec4& colour = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)) -> void;
        static auto draw_point_ndt(const glm::vec3& pos, float point_radius, const glm::vec3& colour) -> void;
        static auto draw_point_ndt(const glm::vec3& pos, float point_radius, const glm::vec4& colour = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)) -> void;

        // Draw Line with a given thickness
        static auto draw_thick_line(const glm::vec3& start, const glm::vec3& end, float line_width, const glm::vec3& colour) -> void;
        static auto draw_thick_line(const glm::vec3& start, const glm::vec3& end, float line_width, const glm::vec4& colour = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)) -> void;
        static auto draw_thick_line_ndt(const glm::vec3& start, const glm::vec3& end, float line_width, const glm::vec3& colour) -> void;
        static auto draw_thick_line_ndt(const glm::vec3& start, const glm::vec3& end, float line_width, const glm::vec4& colour = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)) -> void;

        // Draw line with thickness of 1 screen pixel regardless of distance from camera
        static auto draw_hair_line(const glm::vec3& start, const glm::vec3& end, const glm::vec3& colour) -> void;
        static auto draw_hair_line(const glm::vec3& start, const glm::vec3& end, const glm::vec4& colour = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)) -> void;
        static auto draw_hair_line_ndt(const glm::vec3& start, const glm::vec3& end, const glm::vec3& colour) -> void;
        static auto draw_hair_line_ndt(const glm::vec3& start, const glm::vec3& end, const glm::vec4& colour = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)) -> void;

        // Draw Matrix (x,y,z axis at pos)
        static auto draw_matrix(const glm::mat4& transform_mtx) -> void;
        static auto draw_matrix(const glm::mat3& rotation_mtx, const glm::vec3& position) -> void;
        static auto draw_matrix_ndt(const glm::mat4& transform_mtx) -> void;
        static auto draw_matrix_ndt(const glm::mat3& rotation_mtx, const glm::vec3& position) -> void;

        // Draw Triangle
        static auto draw_triangle(const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, const glm::vec4& colour = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)) -> void;
        static auto draw_triangle_ndt(const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, const glm::vec4& colour = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)) -> void;

        // Draw Polygon (Renders as a triangle fan, so verts must be arranged in order)
        static auto draw_polygon(int n_verts, const glm::vec3* verts, const glm::vec4& colour = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)) -> void;
        static auto draw_polygon_ndt(int n_verts, const glm::vec3* verts, const glm::vec4& colour = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)) -> void;

        // Draw Text WorldSpace (pos given here in worldspace)
        static auto draw_text_ws(const glm::vec3& pos, const float font_size, const glm::vec4& colour, const std::string text, ...) -> void;    /// See "printf" for usage manual
        static auto draw_text_ws_ndt(const glm::vec3& pos, const float font_size, const glm::vec4& colour, const std::string text, ...) -> void; /// See "printf" for usage manual

        // Draw Text (pos is assumed to be pre-multiplied by projMtx * viewMtx at this point)
        static auto draw_text_cs(const glm::vec4& pos, const float font_size, const std::string& text, const glm::vec4& colour = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)) -> void;

        // Add a status entry at the top left of the screen (Cleared each frame)
        static auto add_status_entry(const glm::vec4& colour, const std::string text, ...) -> void; /// See "printf" for usuage manual

        // Add a log entry at the bottom left - persistent until scene reset
        static auto log(const glm::vec3& colour, const std::string text, ...) -> void; /// See "printf" for usuage manual
        static auto log(const std::string text, ...) -> void;                          // Default Text Colour
        static auto log_e(const char* filename, int linenumber, const std::string text, ...) -> void;

        static auto sort_lists() -> void;
        static auto clear_log_entries() -> void;

        static auto debug_draw(const maths::BoundingBox& box, const glm::vec4& edgeColour, bool cornersOnly = false, float width = 0.02f) -> void;
        static auto debug_draw(const maths::BoundingSphere& sphere, const glm::vec4& colour) -> void;
        static auto debug_draw(maths::Frustum& frustum, const glm::vec4& colour) -> void;
        static auto debug_draw(const maths::Ray& ray, const glm::vec4& colour = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), float distance = 1000.0f) -> void;
        static auto debug_draw(RectLightComponent* light, const maths::Transform& transform, const glm::vec4& colour) -> void;
        static auto debug_draw(PointLightComponent* light, const maths::Transform& transform, const glm::vec4& colour) -> void;
        static auto debug_draw(SpotLightComponent* light, const maths::Transform& transform, const glm::vec4& colour) -> void;
        static auto debug_draw(DirectionalLightComponent* light, const maths::Transform& transform, const glm::vec4& colour) -> void;
        static auto debug_draw(Environment* light, const maths::Transform& transform, const glm::vec4& colour) -> void;
        static auto debug_draw_sphere(float radius, const glm::vec3& position, const glm::vec4& colour) -> void;
        static auto debug_draw_circle(int numVerts, float radius, const glm::vec3& position, const glm::quat& rotation, const glm::vec4& colour) -> void;
        static auto debug_draw_cone(int numCircleVerts, int numLinesToCircle, float angle, float length, const glm::vec3& position, const glm::quat& rotation, const glm::vec4& colour) -> void;
        static auto debug_draw_capsule(const glm::vec3& position, const glm::quat& rotation, float height, float radius, const glm::vec4& colour) -> void;

        auto get_triangles(bool depthTested = false) const -> const std::vector<TriangleInfo>& { return (depthTested ? m_DrawList.m_DebugTriangles : m_DrawListNDT.m_DebugTriangles); }
        auto get_lines(bool depthTested = false) const -> const std::vector<LineInfo>& { return depthTested ? m_DrawList.m_DebugLines : m_DrawListNDT.m_DebugLines; }
        auto get_thick_lines(bool depthTested = false) const -> const std::vector<LineInfo>& { return depthTested ? m_DrawList.m_DebugThickLines : m_DrawListNDT.m_DebugThickLines; }
        auto get_points(bool depthTested = false) const -> const std::vector<PointInfo>& { return depthTested ? m_DrawList.m_DebugPoints : m_DrawListNDT.m_DebugPoints; }

        auto get_log_entries() const -> const std::vector<LogEntry>& { return m_vLogEntries; }
        auto get_debug_text() const -> const std::vector<DebugText>& { return m_TextList; }
        auto get_debug_text_ndt() const -> const std::vector<DebugText>& { return m_TextListNDT; }
        auto get_debug_text_cs() const -> const std::vector<DebugText>& { return m_TextListCS; }

        // const std::vector<glm::vec4>& GetTextChars() const { return m_vChars; }

        auto set_dimensions(uint32_t width, uint32_t height) -> void
        {
            m_Width  = width;
            m_Height = height;
        }
        auto set_proj_view(const glm::mat4& projView) -> void { m_ProjViewMtx = projView; }

        static auto get_instance() -> DebugRenderer*
        {
            return s_Instance;
        }

    protected:
        // Actual functions managing data parsing to save code bloat - called by public functions
        static auto gen_draw_point(bool ndt, const glm::vec3& pos, float point_radius, const glm::vec4& colour) -> void;
        static auto gen_draw_thick_line(bool ndt, const glm::vec3& start, const glm::vec3& end, float line_width, const glm::vec4& colour) -> void;
        static auto gen_draw_hair_line(bool ndt, const glm::vec3& start, const glm::vec3& end, const glm::vec4& colour) -> void;
        static auto gen_draw_triangle(bool ndt, const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, const glm::vec4& colour) -> void;
        static auto add_log_entry(const glm::vec3& colour, const std::string& text) -> void;

    protected:
        static DebugRenderer* s_Instance;

        struct DebugDrawList
        {
            std::vector<TriangleInfo> m_DebugTriangles;
            std::vector<LineInfo> m_DebugLines;
            std::vector<PointInfo> m_DebugPoints;
            std::vector<LineInfo> m_DebugThickLines;
        };


        int m_NumStatusEntries;
        float m_MaxStatusEntryWidth;
        std::vector<LogEntry> m_vLogEntries;
        int m_LogEntriesOffset;

        std::vector<DebugText> m_TextList;
        std::vector<DebugText> m_TextListNDT;
        std::vector<DebugText> m_TextListCS;

        // std::vector<glm::vec4> m_vChars;
        size_t m_OffsetChars;
        DebugDrawList m_DrawList;
        DebugDrawList m_DrawListNDT;

        glm::mat4 m_ProjViewMtx = glm::mat4(1.0f);
        uint32_t m_Width;
        uint32_t m_Height;
    };

}
