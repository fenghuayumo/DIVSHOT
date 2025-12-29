#ifndef GSPLAT_CUDA_KERNELS_FORWARD_H
#define GSPLAT_CUDA_KERNELS_FORWARD_H

#include "common.h"

#ifdef USE_HIP
#include <hip/hip_runtime.h>
#else
#include <cuda.h>
#include <cuda_runtime.h>
#include <cooperative_groups.h>
#include <cooperative_groups/reduce.h>
#endif

namespace gsplat {

    namespace cg = cooperative_groups;
    inline __device__ __host__ float fast_lerp(float a, float b, float t)
    {
        return a + t * (b - a);
    }

       __device__ inline vec3 convert_sh_to_color(
        const vec3* sh_coefficients_0,
        const vec3* sh_coefficients_rest,
        const vec3& position,
        const vec3& cam_position,
        const uint32_t primitive_idx,
        const uint32_t active_sh_bases,
        const uint32_t total_bases_sh_rest)
    {
        // computation adapted from https://github.com/NVlabs/tiny-cuda-nn/blob/212104156403bd87616c1a4f73a1c5f2c2e172a9/include/tiny-cuda-nn/common_device.h#L340
        vec3 result = 0.5f + 0.28209479177387814f * sh_coefficients_0[primitive_idx];
        if (active_sh_bases > 1) {
            const vec3* coefficients_ptr = sh_coefficients_rest + primitive_idx * total_bases_sh_rest;
            auto view_dir = glm::normalize(position - cam_position);
            const float x = view_dir.x;
            const float y = view_dir.y;
            const float z = view_dir.z;
            result = result + (-0.48860251190291987f * y) * coefficients_ptr[0]
                            + (0.48860251190291987f * z) * coefficients_ptr[1]
                            + (-0.48860251190291987f * x) * coefficients_ptr[2];
            if (active_sh_bases > 4) {
                const float xx = x * x, yy = y * y, zz = z * z;
                const float xy = x * y, xz = x * z, yz = y * z;
                result = result + (1.0925484305920792f * xy) * coefficients_ptr[3]
                                + (-1.0925484305920792f * yz) * coefficients_ptr[4]
                                + (0.94617469575755997f * zz - 0.31539156525251999f) * coefficients_ptr[5]
                                + (-1.0925484305920792f * xz) * coefficients_ptr[6]
                                + (0.54627421529603959f * xx - 0.54627421529603959f * yy) * coefficients_ptr[7];
                if (active_sh_bases > 9) {
                    result = result + (0.59004358992664352f * y * (-3.0f * xx + yy)) * coefficients_ptr[8]
                                    + (2.8906114426405538f * xy * z) * coefficients_ptr[9]
                                    + (0.45704579946446572f * y * (1.0f - 5.0f * zz)) * coefficients_ptr[10]
                                    + (0.3731763325901154f * z * (5.0f * zz - 3.0f)) * coefficients_ptr[11]
                                    + (0.45704579946446572f * x * (1.0f - 5.0f * zz)) * coefficients_ptr[12]
                                    + (1.4453057213202769f * z * (xx - yy)) * coefficients_ptr[13]
                                    + (0.59004358992664352f * x * (-xx + 3.0f * yy)) * coefficients_ptr[14];
                }
            }
        }
        return result;
    }

    // based on https://github.com/r4dl/StopThePop-Rasterization/blob/d8cad09919ff49b11be3d693d1e71fa792f559bb/cuda_rasterizer/stopthepop/stopthepop_common.cuh#L131
    __device__ inline bool will_primitive_contribute(
        const vec2& mean,
        const vec3& conic,
        const uint32_t tile_x,
        const uint32_t tile_y,
        const float power_threshold)
    {
        const vec2 rect_min = vec2(static_cast<float>(tile_x * TILE_WIDTH), static_cast<float>(tile_y * TILE_HEIGHT));
        const vec2 rect_max = vec2(static_cast<float>((tile_x + 1) * TILE_WIDTH - 1), static_cast<float>((tile_y + 1) * TILE_HEIGHT - 1));

        const float x_min_diff = rect_min.x - mean.x;
        const float x_left = static_cast<float>(x_min_diff > 0.0f);
        const float not_in_x_range = x_left + static_cast<float>(mean.x > rect_max.x);
        const float y_min_diff = rect_min.y - mean.y;
        const float y_above = static_cast<float>(y_min_diff > 0.0f);
        const float not_in_y_range = y_above + static_cast<float>(mean.y > rect_max.y);

        // let's hope the compiler optimizes this properly
        if (not_in_y_range + not_in_x_range == 0.0f) {
            return true;
        }
        const vec2 closest_corner = vec2(
            fast_lerp(rect_max.x, rect_min.x, x_left),
            fast_lerp(rect_max.y, rect_min.y, y_above)
        );
        const vec2 diff = mean - closest_corner;

        const vec2 d = vec2(
            copysignf(static_cast<float>(TILE_WIDTH - 1), x_min_diff),
            copysignf(static_cast<float>(TILE_HEIGHT - 1), y_min_diff)
        );
        const vec2 t = vec2(
            not_in_y_range * __saturatef((d.x * conic.x * diff.x + d.x * conic.y * diff.y) / (d.x * conic.x * d.x)),
            not_in_x_range * __saturatef((d.y * conic.y * diff.x + d.y * conic.z * diff.y) / (d.y * conic.z * d.y))
        );
        const vec2 max_contribution_point = closest_corner + t * d;
        const vec2 delta = mean - max_contribution_point;
        const float max_power_in_tile = 0.5f * (conic.x * delta.x * delta.x + conic.z * delta.y * delta.y) + conic.y * delta.x * delta.y;
        return max_power_in_tile <= power_threshold;
    }

// based on https://github.com/r4dl/StopThePop-Rasterization/blob/d8cad09919ff49b11be3d693d1e71fa792f559bb/cuda_rasterizer/stopthepop/stopthepop_common.cuh#L177
__device__ inline uint32_t compute_exact_n_touched_tiles(
    const vec2& mean2d,
    const vec3& conic,
    const glm::uvec4& screen_bounds,
    const float power_threshold,
    const uint32_t tile_count,
    const bool active)
{
    const vec2 mean2d_shifted = mean2d - 0.5f;

    uint32_t n_touched_tiles = 0;
    if (active) {
        const uint32_t screen_bounds_width = screen_bounds.y - screen_bounds.x;
        for (uint32_t instance_idx = 0; instance_idx < tile_count && instance_idx < N_SEQUENTIAL_THRESHOLD; instance_idx++) {
            const uint32_t tile_y = screen_bounds.z + (instance_idx / screen_bounds_width);
            const uint32_t tile_x = screen_bounds.x + (instance_idx % screen_bounds_width);
            if (will_primitive_contribute(mean2d_shifted, conic, tile_x, tile_y, power_threshold)) n_touched_tiles++;
        }
    }

    const uint32_t lane_idx = cg::this_thread_block().thread_rank() % 32u;
    const uint32_t warp_idx = cg::this_thread_block().thread_rank() / 32u;

    const int compute_cooperatively = active && tile_count > N_SEQUENTIAL_THRESHOLD;
    const uint32_t remaining_threads = __ballot_sync(0xffffffffu, compute_cooperatively);
    if (remaining_threads == 0) return n_touched_tiles;

    const uint32_t n_remaining_threads = __popc(remaining_threads);
    for (int n = 0; n < n_remaining_threads && n < 32; n++) {
        const uint32_t current_lane = __fns(remaining_threads, 0, n + 1); // find lane index of next remaining thread

        const uint4 screen_bounds_coop = make_uint4(
            __shfl_sync(0xffffffffu, screen_bounds.x, current_lane),
            __shfl_sync(0xffffffffu, screen_bounds.y, current_lane),
            __shfl_sync(0xffffffffu, screen_bounds.z, current_lane),
            __shfl_sync(0xffffffffu, screen_bounds.w, current_lane)
        );
        const uint32_t screen_bounds_width_coop = screen_bounds_coop.y - screen_bounds_coop.x;
        const uint32_t tile_count_coop = (screen_bounds_coop.w - screen_bounds_coop.z) * screen_bounds_width_coop;

        const vec2 mean2d_shifted_coop = vec2(
            __shfl_sync(0xffffffffu, mean2d_shifted.x, current_lane),
            __shfl_sync(0xffffffffu, mean2d_shifted.y, current_lane)
        );
        const vec3 conic_coop = vec3(
            __shfl_sync(0xffffffffu, conic.x, current_lane),
            __shfl_sync(0xffffffffu, conic.y, current_lane),
            __shfl_sync(0xffffffffu, conic.z, current_lane)
        );
        const float power_threshold_coop = __shfl_sync(0xffffffffu, power_threshold, current_lane);

        const uint32_t remaining_tile_count = tile_count_coop - N_SEQUENTIAL_THRESHOLD;
        const int n_iterations = (remaining_tile_count + 31u) / 32u;
        for (int i = 0; i < n_iterations; i++) {
            const int instance_idx = i * 32 + lane_idx + N_SEQUENTIAL_THRESHOLD;
            const int active_current = instance_idx < tile_count_coop;
            const uint32_t tile_y = screen_bounds_coop.z + (instance_idx / screen_bounds_width_coop);
            const uint32_t tile_x = screen_bounds_coop.x + (instance_idx % screen_bounds_width_coop);
            const uint32_t contributes = active_current && will_primitive_contribute(mean2d_shifted_coop, conic_coop, tile_x, tile_y, power_threshold_coop);
            const uint32_t contributes_ballot = __ballot_sync(0xffffffffu, contributes);
            const uint32_t n_contributes = __popc(contributes_ballot);
            n_touched_tiles += (current_lane == lane_idx) * n_contributes;
        }
    }

    return n_touched_tiles;
}

}
#endif