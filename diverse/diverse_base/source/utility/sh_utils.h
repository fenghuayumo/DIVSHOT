#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <array>
#include <functional>
namespace diverse
{
    struct SHRotation
    {
        SHRotation(const glm::mat3& mat);

        std::function<void(std::vector<float>& result, const std::vector<float>& src)> apply;
    };
}