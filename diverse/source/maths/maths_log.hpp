#include "plane.h"
#include "frustum.h"
#include "bounding_box.h"
#include "bounding_sphere.h"
#include "ray.h"
#include "rect.h"
#include "transform.h"
#include <spdlog/fmt/ostr.h>

template<>
struct fmt::formatter<glm::vec2> : fmt::formatter<std::string>
{
    auto format(glm::vec2 v, format_context& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), "[{}, {}]", v.x, v.y);
    }
};

template<>
struct fmt::formatter<glm::ivec2> : fmt::formatter<std::string>
{
    auto format(glm::ivec2 v, format_context& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), "[{}, {}]", v.x, v.y);
    }
};

template<>
struct fmt::formatter<glm::uvec2> : fmt::formatter<std::string>
{
    auto format(glm::uvec2 v, format_context& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), "[{}, {}]", v.x, v.y);
    }
};


template<>
struct fmt::formatter<glm::vec3> : fmt::formatter<std::string>
{
    auto format(glm::vec3 my, format_context& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), "[{}, {}, {}]", my.x, my.y,my.z);
    }
};

template<>
struct fmt::formatter<glm::ivec3> : fmt::formatter<std::string>
{
    auto format(glm::ivec3 my, format_context& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), "[{}, {}, {}]", my.x, my.y, my.z);
    }
};

template<>
struct fmt::formatter<glm::uvec3> : fmt::formatter<std::string>
{
    auto format(glm::uvec3 my, format_context& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), "[{}, {}, {}]", my.x, my.y, my.z);
    }
};


template<>
struct fmt::formatter<glm::vec4> : fmt::formatter<std::string>
{
    auto format(glm::vec4 v, format_context& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), "[{}, {}, {}, {}]", v.x, v.y, v.z, v.w);
    }
};

template<>
struct fmt::formatter<glm::ivec4> : fmt::formatter<std::string>
{
    auto format(glm::ivec4 v, format_context& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), "[{}, {}, {}, {}]", v.x, v.y, v.z, v.w);
    }
};

template<>
struct fmt::formatter<glm::uvec4> : fmt::formatter<std::string>
{
    auto format(glm::uvec4 v, format_context& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), "[{}, {}, {},{}]", v.x, v.y, v.z, v.w);
    }
};

template<>
struct fmt::formatter<glm::mat3> : fmt::formatter<std::string>
{
    auto format(glm::mat3 v, format_context& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), "[{}, {}, {}] \n [{},{},{}] \n [{},{},{}]", v[0][0], v[0][1], v[0][2],
            v[1][0], v[1][1], v[1][2], 
            v[2][0], v[2][1], v[2][2]);
    }
};

template<>
struct fmt::formatter<glm::mat4> : fmt::formatter<std::string>
{
    auto format(const glm::mat4& v, format_context& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), "[{},{},{},{}] \n [{},{},{},{}] \n [{},{},{},{}] \n [{},{},{},{}]", v[0][0], v[0][1], v[0][2],v[0][3],
            v[1][0], v[1][1], v[1][2],v[1][3],
            v[2][0], v[2][1], v[2][2],v[2][3],
            v[3][0], v[3][1], v[3][2],v[3][3]);
    }
};

template<>
struct fmt::formatter<diverse::maths::Transform> : fmt::formatter<std::string>
{
    auto format(const diverse::maths::Transform& v, format_context& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), "Position: {} \n Scale: {} \n Orientation: {} \n", v.get_local_position(), v.get_local_scale(), v.get_local_orientation());
    }
};