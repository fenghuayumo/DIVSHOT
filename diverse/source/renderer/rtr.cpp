#pragma once
#include "backend/drs_rhi/gpu_device.h"
#include "rtr.h"
#include "blue_noise.h"
#include "radiance_cache.h"
namespace diverse
{
    const std::array<glm::ivec4, 16 * 4 * 8>    SPATIAL_RESOLVE_OFFSETS = {

        glm::ivec4(0, 0, 0, 0),
        glm::ivec4(-1, -1, 0, 0),
        glm::ivec4(2, -1, 0, 0),
        glm::ivec4(-2, -2, 0, 0),
        glm::ivec4(-2, 2, 0, 0),
        glm::ivec4(2, 2, 0, 0),
        glm::ivec4(0, -3, 0, 0),
        glm::ivec4(-3, 0, 0, 0),
        glm::ivec4(3, 0, 0, 0),
        glm::ivec4(3, 1, 0, 0),
        glm::ivec4(-1, 3, 0, 0),
        glm::ivec4(1, 3, 0, 0),
        glm::ivec4(2, -3, 0, 0),
        glm::ivec4(-3, -2, 0, 0),
        glm::ivec4(1, 4, 0, 0),
        glm::ivec4(-3, 3, 0, 0),
        glm::ivec4(0, 0, 0, 0),
        glm::ivec4(0, 1, 0, 0),
        glm::ivec4(-1, 1, 0, 0),
        glm::ivec4(1, 1, 0, 0),
        glm::ivec4(0, -2, 0, 0),
        glm::ivec4(-2, 1, 0, 0),
        glm::ivec4(1, 2, 0, 0),
        glm::ivec4(2, -2, 0, 0),
        glm::ivec4(1, -3, 0, 0),
        glm::ivec4(-4, 0, 0, 0),
        glm::ivec4(4, 0, 0, 0),
        glm::ivec4(-1, -4, 0, 0),
        glm::ivec4(-4, -1, 0, 0),
        glm::ivec4(-4, 1, 0, 0),
        glm::ivec4(-3, -3, 0, 0),
        glm::ivec4(3, 3, 0, 0),
        glm::ivec4(0, 0, 0, 0),
        glm::ivec4(0, -1, 0, 0),
        glm::ivec4(-1, 0, 0, 0),
        glm::ivec4(1, -1, 0, 0),
        glm::ivec4(-2, -1, 0, 0),
        glm::ivec4(2, 1, 0, 0),
        glm::ivec4(-1, 2, 0, 0),
        glm::ivec4(0, 3, 0, 0),
        glm::ivec4(3, -1, 0, 0),
        glm::ivec4(-2, -3, 0, 0),
        glm::ivec4(3, -2, 0, 0),
        glm::ivec4(-3, 2, 0, 0),
        glm::ivec4(0, -4, 0, 0),
        glm::ivec4(4, 1, 0, 0),
        glm::ivec4(-1, 4, 0, 0),
        glm::ivec4(3, -3, 0, 0),
        glm::ivec4(0, 0, 0, 0),
        glm::ivec4(1, 0, 0, 0),
        glm::ivec4(-2, 0, 0, 0),
        glm::ivec4(2, 0, 0, 0),
        glm::ivec4(0, 2, 0, 0),
        glm::ivec4(-1, -2, 0, 0),
        glm::ivec4(1, -2, 0, 0),
        glm::ivec4(-1, -3, 0, 0),
        glm::ivec4(-3, -1, 0, 0),
        glm::ivec4(-3, 1, 0, 0),
        glm::ivec4(3, 2, 0, 0),
        glm::ivec4(-2, 3, 0, 0),
        glm::ivec4(2, 3, 0, 0),
        glm::ivec4(0, 4, 0, 0),
        glm::ivec4(1, -4, 0, 0),
        glm::ivec4(4, -1, 0, 0),
        glm::ivec4(0, 0, 0, 0),
        glm::ivec4(0, 1, 0, 0),
        glm::ivec4(-1, -1, 0, 0),
        glm::ivec4(1, -2, 0, 0),
        glm::ivec4(2, -2, 0, 0),
        glm::ivec4(3, 0, 0, 0),
        glm::ivec4(1, 3, 0, 0),
        glm::ivec4(-3, 2, 0, 0),
        glm::ivec4(-4, 0, 0, 0),
        glm::ivec4(-1, -4, 0, 0),
        glm::ivec4(-1, 4, 0, 0),
        glm::ivec4(-3, -3, 0, 0),
        glm::ivec4(4, 2, 0, 0),
        glm::ivec4(1, -5, 0, 0),
        glm::ivec4(-4, -4, 0, 0),
        glm::ivec4(4, -4, 0, 0),
        glm::ivec4(0, 0, 0, 0),
        glm::ivec4(0, -1, 0, 0),
        glm::ivec4(1, 0, 0, 0),
        glm::ivec4(-2, 1, 0, 0),
        glm::ivec4(2, 2, 0, 0),
        glm::ivec4(0, 3, 0, 0),
        glm::ivec4(-3, -1, 0, 0),
        glm::ivec4(-3, -2, 0, 0),
        glm::ivec4(0, -4, 0, 0),
        glm::ivec4(4, -1, 0, 0),
        glm::ivec4(-3, 3, 0, 0),
        glm::ivec4(3, 3, 0, 0),
        glm::ivec4(-2, 4, 0, 0),
        glm::ivec4(3, -4, 0, 0),
        glm::ivec4(5, -1, 0, 0),
        glm::ivec4(-2, -5, 0, 0),
        glm::ivec4(0, 0, 0, 0),
        glm::ivec4(-1, 1, 0, 0),
        glm::ivec4(-1, -2, 0, 0),
        glm::ivec4(2, 1, 0, 0),
        glm::ivec4(0, -3, 0, 0),
        glm::ivec4(3, -1, 0, 0),
        glm::ivec4(-3, 1, 0, 0),
        glm::ivec4(-1, 3, 0, 0),
        glm::ivec4(3, -2, 0, 0),
        glm::ivec4(1, 4, 0, 0),
        glm::ivec4(3, -3, 0, 0),
        glm::ivec4(-4, -2, 0, 0),
        glm::ivec4(0, -5, 0, 0),
        glm::ivec4(4, 3, 0, 0),
        glm::ivec4(-5, -1, 0, 0),
        glm::ivec4(5, 1, 0, 0),
        glm::ivec4(0, 0, 0, 0),
        glm::ivec4(-1, 0, 0, 0),
        glm::ivec4(1, 1, 0, 0),
        glm::ivec4(-2, 0, 0, 0),
        glm::ivec4(2, -1, 0, 0),
        glm::ivec4(1, 2, 0, 0),
        glm::ivec4(-2, -2, 0, 0),
        glm::ivec4(0, 4, 0, 0),
        glm::ivec4(1, -4, 0, 0),
        glm::ivec4(-4, 1, 0, 0),
        glm::ivec4(4, 1, 0, 0),
        glm::ivec4(-2, -4, 0, 0),
        glm::ivec4(2, -4, 0, 0),
        glm::ivec4(-4, 2, 0, 0),
        glm::ivec4(-4, -3, 0, 0),
        glm::ivec4(-3, 4, 0, 0),
        glm::ivec4(0, 0, 0, 0),
        glm::ivec4(0, -1, 0, 0),
        glm::ivec4(1, 1, 0, 0),
        glm::ivec4(-2, -2, 0, 0),
        glm::ivec4(2, -2, 0, 0),
        glm::ivec4(-3, 0, 0, 0),
        glm::ivec4(3, 1, 0, 0),
        glm::ivec4(-1, 3, 0, 0),
        glm::ivec4(-1, 4, 0, 0),
        glm::ivec4(-2, -4, 0, 0),
        glm::ivec4(5, 0, 0, 0),
        glm::ivec4(-4, 3, 0, 0),
        glm::ivec4(3, 4, 0, 0),
        glm::ivec4(-5, -1, 0, 0),
        glm::ivec4(2, -5, 0, 0),
        glm::ivec4(4, -4, 0, 0),
        glm::ivec4(0, 0, 0, 0),
        glm::ivec4(1, -1, 0, 0),
        glm::ivec4(-2, 1, 0, 0),
        glm::ivec4(1, 2, 0, 0),
        glm::ivec4(1, -3, 0, 0),
        glm::ivec4(-3, -2, 0, 0),
        glm::ivec4(-3, 2, 0, 0),
        glm::ivec4(4, 0, 0, 0),
        glm::ivec4(4, 2, 0, 0),
        glm::ivec4(2, 4, 0, 0),
        glm::ivec4(3, -4, 0, 0),
        glm::ivec4(-1, -5, 0, 0),
        glm::ivec4(-5, -3, 0, 0),
        glm::ivec4(-3, 5, 0, 0),
        glm::ivec4(6, -2, 0, 0),
        glm::ivec4(-6, 2, 0, 0),
        glm::ivec4(0, 0, 0, 0),
        glm::ivec4(0, 1, 0, 0),
        glm::ivec4(0, -2, 0, 0),
        glm::ivec4(-1, -2, 0, 0),
        glm::ivec4(-2, 2, 0, 0),
        glm::ivec4(3, -1, 0, 0),
        glm::ivec4(-4, 0, 0, 0),
        glm::ivec4(1, 4, 0, 0),
        glm::ivec4(3, 3, 0, 0),
        glm::ivec4(4, -2, 0, 0),
        glm::ivec4(0, -5, 0, 0),
        glm::ivec4(-3, -4, 0, 0),
        glm::ivec4(1, -5, 0, 0),
        glm::ivec4(-5, 1, 0, 0),
        glm::ivec4(-1, 5, 0, 0),
        glm::ivec4(1, 5, 0, 0),
        glm::ivec4(0, 0, 0, 0),
        glm::ivec4(-1, 0, 0, 0),
        glm::ivec4(0, 2, 0, 0),
        glm::ivec4(2, 1, 0, 0),
        glm::ivec4(0, -3, 0, 0),
        glm::ivec4(-3, -1, 0, 0),
        glm::ivec4(2, -3, 0, 0),
        glm::ivec4(4, -1, 0, 0),
        glm::ivec4(-3, -3, 0, 0),
        glm::ivec4(-4, 2, 0, 0),
        glm::ivec4(-2, 4, 0, 0),
        glm::ivec4(5, 2, 0, 0),
        glm::ivec4(4, 4, 0, 0),
        glm::ivec4(5, -3, 0, 0),
        glm::ivec4(-5, 3, 0, 0),
        glm::ivec4(3, 5, 0, 0),
        glm::ivec4(0, 0, 0, 0),
        glm::ivec4(0, 1, 0, 0),
        glm::ivec4(1, -1, 0, 0),
        glm::ivec4(0, -3, 0, 0),
        glm::ivec4(-3, 0, 0, 0),
        glm::ivec4(3, 1, 0, 0),
        glm::ivec4(0, 4, 0, 0),
        glm::ivec4(-4, -3, 0, 0),
        glm::ivec4(-4, 3, 0, 0),
        glm::ivec4(4, 3, 0, 0),
        glm::ivec4(5, -1, 0, 0),
        glm::ivec4(-3, -5, 0, 0),
        glm::ivec4(3, -5, 0, 0),
        glm::ivec4(5, -3, 0, 0),
        glm::ivec4(-3, 5, 0, 0),
        glm::ivec4(-6, 0, 0, 0),
        glm::ivec4(0, 0, 0, 0),
        glm::ivec4(-2, -1, 0, 0),
        glm::ivec4(2, -1, 0, 0),
        glm::ivec4(-2, -2, 0, 0),
        glm::ivec4(-2, 2, 0, 0),
        glm::ivec4(1, 3, 0, 0),
        glm::ivec4(-4, 1, 0, 0),
        glm::ivec4(4, 1, 0, 0),
        glm::ivec4(3, -3, 0, 0),
        glm::ivec4(-1, -5, 0, 0),
        glm::ivec4(2, -5, 0, 0),
        glm::ivec4(-2, 5, 0, 0),
        glm::ivec4(2, 5, 0, 0),
        glm::ivec4(0, -6, 0, 0),
        glm::ivec4(-6, -1, 0, 0),
        glm::ivec4(6, 2, 0, 0),
        glm::ivec4(0, 0, 0, 0),
        glm::ivec4(0, 2, 0, 0),
        glm::ivec4(-2, 1, 0, 0),
        glm::ivec4(1, -3, 0, 0),
        glm::ivec4(3, -1, 0, 0),
        glm::ivec4(3, 2, 0, 0),
        glm::ivec4(-1, -4, 0, 0),
        glm::ivec4(-4, -2, 0, 0),
        glm::ivec4(-2, 4, 0, 0),
        glm::ivec4(5, -2, 0, 0),
        glm::ivec4(-4, -4, 0, 0),
        glm::ivec4(3, 5, 0, 0),
        glm::ivec4(0, 6, 0, 0),
        glm::ivec4(-6, -2, 0, 0),
        glm::ivec4(-6, 2, 0, 0),
        glm::ivec4(4, -5, 0, 0),
        glm::ivec4(0, 0, 0, 0),
        glm::ivec4(0, -1, 0, 0),
        glm::ivec4(1, 1, 0, 0),
        glm::ivec4(-1, -2, 0, 0),
        glm::ivec4(-2, 3, 0, 0),
        glm::ivec4(1, 4, 0, 0),
        glm::ivec4(-3, -4, 0, 0),
        glm::ivec4(4, -3, 0, 0),
        glm::ivec4(5, 0, 0, 0),
        glm::ivec4(1, -5, 0, 0),
        glm::ivec4(-5, -1, 0, 0),
        glm::ivec4(-5, 2, 0, 0),
        glm::ivec4(5, 3, 0, 0),
        glm::ivec4(6, 1, 0, 0),
        glm::ivec4(1, 6, 0, 0),
        glm::ivec4(-4, 6, 0, 0),
        glm::ivec4(0, 0, 0, 0),
        glm::ivec4(1, -3, 0, 0),
        glm::ivec4(1, 3, 0, 0),
        glm::ivec4(-3, 2, 0, 0),
        glm::ivec4(4, -1, 0, 0),
        glm::ivec4(3, -4, 0, 0),
        glm::ivec4(-5, 0, 0, 0),
        glm::ivec4(-1, -5, 0, 0),
        glm::ivec4(5, 2, 0, 0),
        glm::ivec4(-2, 5, 0, 0),
        glm::ivec4(-4, -4, 0, 0),
        glm::ivec4(1, 6, 0, 0),
        glm::ivec4(-5, 5, 0, 0),
        glm::ivec4(3, 7, 0, 0),
        glm::ivec4(-8, 1, 0, 0),
        glm::ivec4(7, 4, 0, 0),
        glm::ivec4(0, 0, 0, 0),
        glm::ivec4(-1, -1, 0, 0),
        glm::ivec4(2, -1, 0, 0),
        glm::ivec4(-1, 2, 0, 0),
        glm::ivec4(3, 1, 0, 0),
        glm::ivec4(-4, -2, 0, 0),
        glm::ivec4(2, 4, 0, 0),
        glm::ivec4(-2, -5, 0, 0),
        glm::ivec4(-5, 2, 0, 0),
        glm::ivec4(6, -2, 0, 0),
        glm::ivec4(-2, 6, 0, 0),
        glm::ivec4(5, -4, 0, 0),
        glm::ivec4(6, 3, 0, 0),
        glm::ivec4(0, -7, 0, 0),
        glm::ivec4(5, 5, 0, 0),
        glm::ivec4(4, -6, 0, 0),
        glm::ivec4(0, 0, 0, 0),
        glm::ivec4(2, 2, 0, 0),
        glm::ivec4(-1, -3, 0, 0),
        glm::ivec4(-3, -1, 0, 0),
        glm::ivec4(0, 4, 0, 0),
        glm::ivec4(-3, 3, 0, 0),
        glm::ivec4(4, -2, 0, 0),
        glm::ivec4(1, -5, 0, 0),
        glm::ivec4(-5, -3, 0, 0),
        glm::ivec4(6, 0, 0, 0),
        glm::ivec4(-6, 1, 0, 0),
        glm::ivec4(3, -6, 0, 0),
        glm::ivec4(3, 6, 0, 0),
        glm::ivec4(-1, 7, 0, 0),
        glm::ivec4(-4, -7, 0, 0),
        glm::ivec4(-7, -4, 0, 0),
        glm::ivec4(0, 0, 0, 0),
        glm::ivec4(-1, 0, 0, 0),
        glm::ivec4(0, 2, 0, 0),
        glm::ivec4(-2, -2, 0, 0),
        glm::ivec4(2, -2, 0, 0),
        glm::ivec4(4, 1, 0, 0),
        glm::ivec4(-2, 4, 0, 0),
        glm::ivec4(4, 3, 0, 0),
        glm::ivec4(-3, -5, 0, 0),
        glm::ivec4(5, -3, 0, 0),
        glm::ivec4(-6, 0, 0, 0),
        glm::ivec4(1, -6, 0, 0),
        glm::ivec4(-5, 4, 0, 0),
        glm::ivec4(7, 0, 0, 0),
        glm::ivec4(1, 7, 0, 0),
        glm::ivec4(-3, -7, 0, 0),
        glm::ivec4(0, 0, 0, 0),
        glm::ivec4(0, -1, 0, 0),
        glm::ivec4(-2, 2, 0, 0),
        glm::ivec4(3, 2, 0, 0),
        glm::ivec4(-3, -3, 0, 0),
        glm::ivec4(-5, 2, 0, 0),
        glm::ivec4(0, -6, 0, 0),
        glm::ivec4(6, 1, 0, 0),
        glm::ivec4(-1, 6, 0, 0),
        glm::ivec4(-6, -2, 0, 0),
        glm::ivec4(6, -3, 0, 0),
        glm::ivec4(3, 6, 0, 0),
        glm::ivec4(4, -6, 0, 0),
        glm::ivec4(-5, 6, 0, 0),
        glm::ivec4(7, 4, 0, 0),
        glm::ivec4(-8, 2, 0, 0),
        glm::ivec4(0, 0, 0, 0),
        glm::ivec4(1, 2, 0, 0),
        glm::ivec4(1, -3, 0, 0),
        glm::ivec4(-4, 0, 0, 0),
        glm::ivec4(4, -1, 0, 0),
        glm::ivec4(-3, 4, 0, 0),
        glm::ivec4(3, -5, 0, 0),
        glm::ivec4(1, 6, 0, 0),
        glm::ivec4(-2, -6, 0, 0),
        glm::ivec4(-5, -5, 0, 0),
        glm::ivec4(-8, 0, 0, 0),
        glm::ivec4(8, -1, 0, 0),
        glm::ivec4(4, 7, 0, 0),
        glm::ivec4(7, -5, 0, 0),
        glm::ivec4(2, -9, 0, 0),
        glm::ivec4(-7, -6, 0, 0),
        glm::ivec4(0, 0, 0, 0),
        glm::ivec4(-1, 0, 0, 0),
        glm::ivec4(-2, -2, 0, 0),
        glm::ivec4(0, 3, 0, 0),
        glm::ivec4(3, -2, 0, 0),
        glm::ivec4(-1, -4, 0, 0),
        glm::ivec4(4, 3, 0, 0),
        glm::ivec4(5, -2, 0, 0),
        glm::ivec4(1, -6, 0, 0),
        glm::ivec4(-6, 1, 0, 0),
        glm::ivec4(-2, 6, 0, 0),
        glm::ivec4(5, 4, 0, 0),
        glm::ivec4(-6, -3, 0, 0),
        glm::ivec4(-6, 4, 0, 0),
        glm::ivec4(8, 0, 0, 0),
        glm::ivec4(-4, -7, 0, 0),
        glm::ivec4(0, 0, 0, 0),
        glm::ivec4(2, 0, 0, 0),
        glm::ivec4(-2, 1, 0, 0),
        glm::ivec4(-4, -1, 0, 0),
        glm::ivec4(1, 4, 0, 0),
        glm::ivec4(3, -4, 0, 0),
        glm::ivec4(5, 1, 0, 0),
        glm::ivec4(-4, 4, 0, 0),
        glm::ivec4(3, 5, 0, 0),
        glm::ivec4(-4, -5, 0, 0),
        glm::ivec4(-7, -2, 0, 0),
        glm::ivec4(7, -2, 0, 0),
        glm::ivec4(2, -8, 0, 0),
        glm::ivec4(8, 3, 0, 0),
        glm::ivec4(-3, 8, 0, 0),
        glm::ivec4(5, -7, 0, 0),
        glm::ivec4(0, 0, 0, 0),
        glm::ivec4(0, -1, 0, 0),
        glm::ivec4(4, -3, 0, 0),
        glm::ivec4(-4, 3, 0, 0),
        glm::ivec4(0, 5, 0, 0),
        glm::ivec4(4, 4, 0, 0),
        glm::ivec4(-3, -5, 0, 0),
        glm::ivec4(1, -6, 0, 0),
        glm::ivec4(-6, -2, 0, 0),
        glm::ivec4(7, -1, 0, 0),
        glm::ivec4(0, 8, 0, 0),
        glm::ivec4(-8, 3, 0, 0),
        glm::ivec4(8, 3, 0, 0),
        glm::ivec4(5, 7, 0, 0),
        glm::ivec4(7, -6, 0, 0),
        glm::ivec4(-5, 8, 0, 0),
        glm::ivec4(0, 0, 0, 0),
        glm::ivec4(2, 2, 0, 0),
        glm::ivec4(0, -3, 0, 0),
        glm::ivec4(3, 0, 0, 0),
        glm::ivec4(-3, -1, 0, 0),
        glm::ivec4(-1, 3, 0, 0),
        glm::ivec4(-6, 1, 0, 0),
        glm::ivec4(4, -5, 0, 0),
        glm::ivec4(6, 3, 0, 0),
        glm::ivec4(-6, -5, 0, 0),
        glm::ivec4(-6, 5, 0, 0),
        glm::ivec4(0, -8, 0, 0),
        glm::ivec4(-9, -1, 0, 0),
        glm::ivec4(-2, 9, 0, 0),
        glm::ivec4(5, -8, 0, 0),
        glm::ivec4(-3, -9, 0, 0),
        glm::ivec4(0, 0, 0, 0),
        glm::ivec4(-2, 1, 0, 0),
        glm::ivec4(-1, -4, 0, 0),
        glm::ivec4(5, 0, 0, 0),
        glm::ivec4(2, 6, 0, 0),
        glm::ivec4(-3, 6, 0, 0),
        glm::ivec4(-4, -6, 0, 0),
        glm::ivec4(3, -7, 0, 0),
        glm::ivec4(-7, -3, 0, 0),
        glm::ivec4(7, -3, 0, 0),
        glm::ivec4(8, 1, 0, 0),
        glm::ivec4(8, 6, 0, 0),
        glm::ivec4(-10, 1, 0, 0),
        glm::ivec4(-7, 8, 0, 0),
        glm::ivec4(-10, 4, 0, 0),
        glm::ivec4(0, 11, 0, 0),
        glm::ivec4(0, 0, 0, 0),
        glm::ivec4(1, 2, 0, 0),
        glm::ivec4(2, -2, 0, 0),
        glm::ivec4(-3, -3, 0, 0),
        glm::ivec4(-3, 3, 0, 0),
        glm::ivec4(3, -4, 0, 0),
        glm::ivec4(-5, 0, 0, 0),
        glm::ivec4(3, 4, 0, 0),
        glm::ivec4(-2, 6, 0, 0),
        glm::ivec4(-1, -7, 0, 0),
        glm::ivec4(6, 5, 0, 0),
        glm::ivec4(-8, 2, 0, 0),
        glm::ivec4(2, 8, 0, 0),
        glm::ivec4(7, -5, 0, 0),
        glm::ivec4(-7, 5, 0, 0),
        glm::ivec4(2, -9, 0, 0),
        glm::ivec4(0, 0, 0, 0),
        glm::ivec4(0, -2, 0, 0),
        glm::ivec4(2, 2, 0, 0),
        glm::ivec4(-4, -2, 0, 0),
        glm::ivec4(-2, 5, 0, 0),
        glm::ivec4(-7, 1, 0, 0),
        glm::ivec4(4, -7, 0, 0),
        glm::ivec4(7, -4, 0, 0),
        glm::ivec4(6, 7, 0, 0),
        glm::ivec4(9, 3, 0, 0),
        glm::ivec4(-6, -9, 0, 0),
        glm::ivec4(-9, 6, 0, 0),
        glm::ivec4(0, -11, 0, 0),
        glm::ivec4(0, 11, 0, 0),
        glm::ivec4(-10, -6, 0, 0),
        glm::ivec4(-7, 10, 0, 0),
        glm::ivec4(0, 0, 0, 0),
        glm::ivec4(-3, 1, 0, 0),
        glm::ivec4(3, -2, 0, 0),
        glm::ivec4(1, -7, 0, 0),
        glm::ivec4(-6, 4, 0, 0),
        glm::ivec4(2, 7, 0, 0),
        glm::ivec4(-7, -4, 0, 0),
        glm::ivec4(8, -1, 0, 0),
        glm::ivec4(-2, -8, 0, 0),
        glm::ivec4(-10, 3, 0, 0),
        glm::ivec4(10, -4, 0, 0),
        glm::ivec4(-4, 10, 0, 0),
        glm::ivec4(4, 10, 0, 0),
        glm::ivec4(-11, -2, 0, 0),
        glm::ivec4(12, 1, 0, 0),
        glm::ivec4(10, 7, 0, 0),
        glm::ivec4(0, 0, 0, 0),
        glm::ivec4(-1, 2, 0, 0),
        glm::ivec4(-1, -4, 0, 0),
        glm::ivec4(4, 1, 0, 0),
        glm::ivec4(4, -4, 0, 0),
        glm::ivec4(4, 5, 0, 0),
        glm::ivec4(-5, -5, 0, 0),
        glm::ivec4(7, 3, 0, 0),
        glm::ivec4(-3, 7, 0, 0),
        glm::ivec4(-8, -1, 0, 0),
        glm::ivec4(-6, 6, 0, 0),
        glm::ivec4(0, 9, 0, 0),
        glm::ivec4(3, -10, 0, 0),
        glm::ivec4(7, -8, 0, 0),
        glm::ivec4(-9, -8, 0, 0),
        glm::ivec4(-12, 3, 0, 0),
        glm::ivec4(0, 0, 0, 0),
        glm::ivec4(1, -5, 0, 0),
        glm::ivec4(-5, 1, 0, 0),
        glm::ivec4(1, 5, 0, 0),
        glm::ivec4(-3, -5, 0, 0),
        glm::ivec4(6, 0, 0, 0),
        glm::ivec4(-4, -8, 0, 0),
        glm::ivec4(-10, 0, 0, 0),
        glm::ivec4(10, -1, 0, 0),
        glm::ivec4(5, 9, 0, 0),
        glm::ivec4(8, -7, 0, 0),
        glm::ivec4(9, 6, 0, 0),
        glm::ivec4(-11, -4, 0, 0),
        glm::ivec4(2, 12, 0, 0),
        glm::ivec4(-7, -10, 0, 0),
        glm::ivec4(-4, 12, 0, 0),
    };

    auto TracedRtr::filter_temporal(rg::TemporalGraph& rg, const GbufferDepth& gbuffer_depth, const rg::Handle<rhi::GpuTexture>& reprojection_map) -> rg::Handle<rhi::GpuTexture>
    {
        rg::RenderPass::new_compute(
            rg.add_pass("reflection temporal"),
            "/shaders/rtr/temporal_filter.hlsl"
            )
            .read(resolved_tex)
            .read(history_tex)
            .read_aspect(gbuffer_depth.depth, rhi::ImageAspectFlags::DEPTH)
            .read(ray_len_tex)
            .read(reprojection_map)
            .read(refl_restir_invalidity_tex)
            .read(gbuffer_depth.gbuffer)
            .write(temporal_output_tex)
            .constants(temporal_output_tex.desc.extent_inv_extent_2d())
            .dispatch(resolved_tex.desc.extent);

        rg::RenderPass::new_compute(
            rg.add_pass("reflection cleanup"),
            "/shaders/rtr/spatial_cleanup.hlsl"
            )
            .read(temporal_output_tex)
            .read_aspect(gbuffer_depth.depth, rhi::ImageAspectFlags::DEPTH)
            .read(gbuffer_depth.geometric_normal)
            .write(resolved_tex) // reuse
            .constants(SPATIAL_RESOLVE_OFFSETS)
            .dispatch(resolved_tex.desc.extent);

        return resolved_tex;
    }

    template<typename T>
    auto make_lut_buffer(rhi::GpuDevice* device, const std::vector<T>& v) -> std::shared_ptr<rhi::GpuBuffer>
    {
        return device->create_buffer(rhi::GpuBufferDesc::new_gpu_only(v.size() * sizeof(T), rhi::BufferUsageFlags::STORAGE_BUFFER),
                    "lut buffer",
                    (u8*)v.data());
    }

    RtrRenderer::RtrRenderer(rhi::GpuDevice* device)
    {
        temporal_tex = PingPongTemporalResource("rtr.temporal");
        ray_len_tex = PingPongTemporalResource("rtr.ray_len");

        temporal_irradiance_tex = PingPongTemporalResource("rtr.irradiance");
        temporal_ray_orig_tex = PingPongTemporalResource("rtr.ray_orig");
        temporal_ray_tex = PingPongTemporalResource("rtr.ray");
        temporal_reservoir_tex = PingPongTemporalResource("rtr.reservoir");
        temporal_rng_tex = PingPongTemporalResource("rtr.rng");
        temporal_hit_normal_tex = PingPongTemporalResource("rtr.hit_normal");

        ranking_tile_buf = make_lut_buffer<i32>(device, get_rank_tile_data());
        scambling_tile_buf = make_lut_buffer<i32>(device, get_scrambling_tile_data()) ;
        sobol_buf = make_lut_buffer<i32>(device, get_sobol_data());

        reuse_rtdgi_rays = true;
    }

    auto RtrRenderer::trace(rg::TemporalGraph& rg,
                    GbufferDepth& gbuffer_depth,
                    const rg::Handle<rhi::GpuTexture>& reprojection_map, 
                    const rg::Handle<rhi::GpuTexture>& sky_cube,
                    rhi::DescriptorSet* bindless_descriptor_set, 
                    rg::Handle<rhi::GpuRayTracingAcceleration>& tlas, 
                    const rg::Handle<rhi::GpuTexture>& rtgi_irradiance, 
                    RestirCandidates& gi_candidates, 
                    IracheState& surfel_state,
                    const radiance_cache::RadianceCacheState& rc_state) -> TracedRtr
    {
        auto gbuffer_desc = gbuffer_depth.gbuffer.desc;

        auto& [refl0_tex, refl2_tex,refl1_tex] = gi_candidates;

        auto ranking_tile_buffer = rg.import_res<rhi::GpuBuffer>(
            ranking_tile_buf,
            rhi::AccessType::ComputeShaderReadSampledImageOrUniformTexelBuffer
            );
        auto scambling_tile_buffer = rg.import_res<rhi::GpuBuffer>(
            scambling_tile_buf,
            rhi::AccessType::ComputeShaderReadSampledImageOrUniformTexelBuffer
            );
        auto sobol_buffer = rg.import_res<rhi::GpuBuffer>(
            sobol_buf,
            rhi::AccessType::ComputeShaderReadSampledImageOrUniformTexelBuffer
            );

        auto [rng_output_tex, rng_history_tex] = temporal_rng_tex.get_output_and_history(
            rg,
            rhi::GpuTextureDesc::new_2d(PixelFormat::R32_UInt, gbuffer_desc.half_res().extent_2d())
            .with_usage(rhi::TextureUsageFlags::SAMPLED | rhi::TextureUsageFlags::STORAGE)
        );

        auto reuse_rtdgi_rays_u32 = reuse_rtdgi_rays ? 1 : 0;
        rg::RenderPass::new_rt(
            rg.add_pass("reflection trace"),
            rhi::ShaderSource{"/shaders/rtr/reflection.rgen.hlsl"},
            {
                rhi::ShaderSource{"/shaders/rt/gbuffer.rmiss.hlsl"},
                rhi::ShaderSource{"/shaders/rt/shadow.rmiss.hlsl"}
            },
            {rhi::ShaderSource{"/shaders/rt/gbuffer.rchit.hlsl"}}
            )
            .read(gbuffer_depth.gbuffer)
            .read_aspect(gbuffer_depth.depth, rhi::ImageAspectFlags::DEPTH)
            .read(ranking_tile_buffer)
            .read(scambling_tile_buffer)
            .read(sobol_buffer)
            .read(rtgi_irradiance)
            .read(sky_cube)
            .bind(surfel_state)
            .bind(rc_state)
            .write(refl0_tex)
            .write(refl1_tex)
            .write(refl2_tex)
            .write(rng_output_tex)
            .constants(std::tuple{gbuffer_desc.extent_inv_extent_2d(), reuse_rtdgi_rays_u32})
            .raw_descriptor_set(1, bindless_descriptor_set)
            .trace_rays(tlas, refl0_tex.desc.extent);

        auto half_view_normal_tex = gbuffer_depth.get_half_view_normal(rg);
        auto half_depth_tex = gbuffer_depth.get_half_depth(rg);

        auto [ray_orig_output_tex, ray_orig_history_tex] = temporal_ray_orig_tex
                .get_output_and_history(
                    rg,
                    rhi::GpuTextureDesc::new_2d(
                    PixelFormat::R32G32B32A32_Float, 
                    gbuffer_desc.half_res().extent_2d()
                    )
                    .with_usage(rhi::TextureUsageFlags::SAMPLED | rhi::TextureUsageFlags::STORAGE)
                );

        auto refl_restir_invalidity_tex =
            rg.create<rhi::GpuTexture>(refl0_tex.desc.with_format(PixelFormat::R8_UNorm),
            "rtr.restir_invalidity"
            );

        auto [hit_normal_output_tex, hit_normal_history_tex] = temporal_hit_normal_tex
                .get_output_and_history(
                rg,
                temporal_tex_desc(
                gbuffer_desc
                .with_format(PixelFormat::R8G8B8A8_UNorm)
                .half_res()
                .extent_2d())
            );
        
        auto [irradiance_output_tex, irradiance_history_tex] = temporal_irradiance_tex
            .get_output_and_history(
                rg,
                temporal_tex_desc(gbuffer_desc.half_res().extent_2d())
                .with_format(PixelFormat::R16G16B16A16_Float)
                );

        auto [reservoir_output_tex, reservoir_history_tex] = temporal_reservoir_tex.get_output_and_history(
                    rg,
                    rhi::GpuTextureDesc::new_2d(PixelFormat::R32G32_UInt, gbuffer_desc.half_res().extent_2d())
                    .with_usage(rhi::TextureUsageFlags::SAMPLED | rhi::TextureUsageFlags::STORAGE)
                );
        
        auto [ray_output_tex, ray_history_tex] = temporal_ray_tex.get_output_and_history(
            rg,
            temporal_tex_desc(gbuffer_desc.half_res().extent_2d())
            .with_format(PixelFormat::R16G16B16A16_Float)
            );
        
        rg::RenderPass::new_rt(
            rg.add_pass("reflection validate"),
            rhi::ShaderSource{"/shaders/rtr/reflection_validate.rgen.hlsl"},
            {
                rhi::ShaderSource{"/shaders/rt/gbuffer.rmiss.hlsl"},
                rhi::ShaderSource{"/shaders/rt/shadow.rmiss.hlsl"}
            },
            {rhi::ShaderSource{"/shaders/rt/gbuffer.rchit.hlsl"}}
            )
            .read(gbuffer_depth.gbuffer)
            .read_aspect(gbuffer_depth.depth, rhi::ImageAspectFlags::DEPTH)
            .read(rtgi_irradiance)
            .read(sky_cube)
            .write(refl_restir_invalidity_tex)
            .bind(surfel_state)
            .bind(rc_state)
            .read(ray_orig_history_tex)
            .read(ray_history_tex)
            .read(rng_history_tex)
            .write(irradiance_history_tex)
            .write(reservoir_history_tex)
            .constants(gbuffer_desc.extent_inv_extent_2d())
            .raw_descriptor_set(1, bindless_descriptor_set)
            .trace_rays(tlas, refl0_tex.desc.half_res().extent);

        rg::RenderPass::new_compute(
            rg.add_pass("rtr restir temporal"),
            "/shaders/rtr/rtr_restir_temporal.hlsl"
            )
            .read(gbuffer_depth.gbuffer)
            .read(half_view_normal_tex)
            .read_aspect(gbuffer_depth.depth, rhi::ImageAspectFlags::DEPTH)
            .read(refl0_tex)
            .read(refl1_tex)
            .read(refl2_tex)
            .read(irradiance_history_tex)
            .read(ray_orig_history_tex)
            .read(ray_history_tex)
            .read(rng_history_tex)
            .read(reservoir_history_tex)
            .read(reprojection_map)
            .read(hit_normal_history_tex)
            .write(irradiance_output_tex)
            .write(ray_orig_output_tex)
            .write(ray_output_tex)
            .write(rng_output_tex)
            .write(hit_normal_output_tex)
            .write(reservoir_output_tex)
            .constants(gbuffer_desc.extent_inv_extent_2d())
            .raw_descriptor_set(1, bindless_descriptor_set)
            .dispatch(irradiance_output_tex.desc.extent);

        auto irradiance_tex = std::move(irradiance_output_tex);
        auto ray_tex = std::move(ray_output_tex);
        auto temporal_reservoir_tex = std::move(reservoir_output_tex);
        auto restir_hit_normal_tex = std::move(hit_normal_output_tex);

        auto resolved_tex = rg.create<rhi::GpuTexture>(
            gbuffer_desc
            .with_format(PixelFormat::R11G11B10_Float),
            "rtr.resolved"
            );
        auto [temporal_output_tex, history_tex] = temporal_tex
            .get_output_and_history(
                rg, 
                temporal_tex_desc(gbuffer_desc.extent_2d())
             );
        auto [ray_len_output_tex, ray_len_history_tex] =
            ray_len_tex.get_output_and_history(
                rg,
                rhi::GpuTextureDesc::new_2d(PixelFormat::R16G16_Float, gbuffer_desc.extent_2d())
                .with_usage(rhi::TextureUsageFlags::SAMPLED | rhi::TextureUsageFlags::STORAGE)
                );
        rg::RenderPass::new_compute(
            rg.add_pass("reflection resolve"),
            "/shaders/rtr/resolve.hlsl"
            )
            .read(gbuffer_depth.gbuffer)
            .read_aspect(gbuffer_depth.depth, rhi::ImageAspectFlags::DEPTH)
            .read(refl0_tex)
            .read(refl1_tex)
            .read(refl2_tex)
            .read(history_tex)
            .read(reprojection_map)
            .read(half_view_normal_tex)
            .read(half_depth_tex)
            .read(ray_len_history_tex)
            .read(irradiance_tex)
            .read(ray_tex)
            .read(temporal_reservoir_tex)
            .read(ray_orig_output_tex)
            .read(restir_hit_normal_tex)
            .write(resolved_tex)
            .write(ray_len_output_tex)
            .raw_descriptor_set(1, bindless_descriptor_set)
            .constants(std::tuple{
                resolved_tex.desc.extent_inv_extent_2d(),
                SPATIAL_RESOLVE_OFFSETS
             })
            .dispatch(resolved_tex.desc.extent);

        return TracedRtr{
            resolved_tex,
            temporal_output_tex,
            history_tex,
            ray_len_output_tex,
            refl_restir_invalidity_tex
        };
    }

    auto RtrRenderer::create_dummy_output(rg::TemporalGraph& rg, const GbufferDepth& gbuffer_depth) -> TracedRtr
    {
        auto gbuffer_desc = gbuffer_depth.gbuffer.desc;

        auto resolved_tex = rg.create<rhi::GpuTexture>(
                gbuffer_desc
                .with_format(PixelFormat::R8G8B8A8_UNorm),
                "rtr.dummy_output");

        auto [temporal_output_tex, history_tex] = temporal_tex.get_output_and_history(rg, temporal_tex_desc(gbuffer_desc.extent_2d()));
        auto [ray_len_output_tex, _ray_len_history_tex] = ray_len_tex.get_output_and_history(
            rg,
            rhi::GpuTextureDesc::new_2d(PixelFormat::R8G8B8A8_UNorm, gbuffer_desc.extent_2d())
            .with_usage(rhi::TextureUsageFlags::SAMPLED | rhi::TextureUsageFlags::STORAGE)
        );

        auto refl_restir_invalidity_tex = rg.create<rhi::GpuTexture>(rhi::GpuTextureDesc::new_2d(PixelFormat::R8_UNorm, {1,1}),"rtr.restir_invalid");

        return TracedRtr{
            resolved_tex,
            temporal_output_tex,
            history_tex,
            ray_len_output_tex,
            refl_restir_invalidity_tex
        };
    }

}
