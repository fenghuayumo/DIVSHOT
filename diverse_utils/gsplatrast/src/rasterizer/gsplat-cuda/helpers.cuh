#ifndef GSPLAT_CUDA_HELPERS_H
#define GSPLAT_CUDA_HELPERS_H

#include "types.cuh"
#ifdef USE_HIP
#include "reduce.cuh"
#include <hip/hip_cooperative_groups.h>
#else
#include <cooperative_groups.h>
#include <cooperative_groups/reduce.h>
#endif

#include <ATen/Dispatch.h>
#include <ATen/cuda/Atomic.cuh>

#define PRAGMA_UNROLL _Pragma("unroll")
namespace gsplat {

namespace cg = cooperative_groups;

template <uint32_t DIM, class T, class WarpT>
inline __device__ void warpSum(T *val, WarpT &warp) {
    PRAGMA_UNROLL
    for (uint32_t i = 0; i < DIM; i++) {
        val[i] = cg::reduce(warp, val[i], cg::plus<T>());
    }
}

template <class WarpT, class ScalarT>
inline __device__ void warpSum(ScalarT &val, WarpT &warp) {
    val = cg::reduce(warp, val, cg::plus<ScalarT>());
}

template <class WarpT, class ScalarT>
inline __device__ void warpSum(vec4<ScalarT> &val, WarpT &warp) {
    val.x = cg::reduce(warp, val.x, cg::plus<ScalarT>());
    val.y = cg::reduce(warp, val.y, cg::plus<ScalarT>());
    val.z = cg::reduce(warp, val.z, cg::plus<ScalarT>());
    val.w = cg::reduce(warp, val.w, cg::plus<ScalarT>());
}

template <class WarpT, class ScalarT>
inline __device__ void warpSum(vec3<ScalarT> &val, WarpT &warp) {
    val.x = cg::reduce(warp, val.x, cg::plus<ScalarT>());
    val.y = cg::reduce(warp, val.y, cg::plus<ScalarT>());
    val.z = cg::reduce(warp, val.z, cg::plus<ScalarT>());
}

template <class WarpT, class ScalarT>
inline __device__ void warpSum(vec2<ScalarT> &val, WarpT &warp) {
    val.x = cg::reduce(warp, val.x, cg::plus<ScalarT>());
    val.y = cg::reduce(warp, val.y, cg::plus<ScalarT>());
}

template <class WarpT, class ScalarT>
inline __device__ void warpSum(mat4<ScalarT> &val, WarpT &warp) {
    warpSum(val[0], warp);
    warpSum(val[1], warp);
    warpSum(val[2], warp);
    warpSum(val[3], warp);
}

template <class WarpT, class ScalarT>
inline __device__ void warpSum(mat3<ScalarT> &val, WarpT &warp) {
    warpSum(val[0], warp);
    warpSum(val[1], warp);
    warpSum(val[2], warp);
}

template <class WarpT, class ScalarT>
inline __device__ void warpSum(mat2<ScalarT> &val, WarpT &warp) {
    warpSum(val[0], warp);
    warpSum(val[1], warp);
}

template <class WarpT, class ScalarT>
inline __device__ void warpMax(ScalarT &val, WarpT &warp) {
    val = cg::reduce(warp, val, cg::greater<ScalarT>());
}

// helper for applying R * p + T, expect mat to be ROW MAJOR
inline __device__ float3 transform_4x3(const float *mat, const float3 p) {
    float3 out = {
        mat[0] * p.x + mat[1] * p.y + mat[2] * p.z + mat[3],
        mat[4] * p.x + mat[5] * p.y + mat[6] * p.z + mat[7],
        mat[8] * p.x + mat[9] * p.y + mat[10] * p.z + mat[11],
    };
    return out;
}

// helper to apply 4x4 transform to 3d vector, return homo coords
// expects mat to be ROW MAJOR
inline __device__ float4 transform_4x4(const float *mat, const float3 p) {
    float4 out = {
        mat[0] * p.x + mat[1] * p.y + mat[2] * p.z + mat[3],
        mat[4] * p.x + mat[5] * p.y + mat[6] * p.z + mat[7],
        mat[8] * p.x + mat[9] * p.y + mat[10] * p.z + mat[11],
        mat[12] * p.x + mat[13] * p.y + mat[14] * p.z + mat[15],
    };
    return out;
}

inline __device__ glm::vec3 transform_4x3(const float *mat, const glm::vec3 p) {
    glm::vec3 out = {
        mat[0] * p.x + mat[1] * p.y + mat[2] * p.z + mat[3],
        mat[4] * p.x + mat[5] * p.y + mat[6] * p.z + mat[7],
        mat[8] * p.x + mat[9] * p.y + mat[10] * p.z + mat[11],
    };
    return out;
}

// helper to apply 4x4 transform to 3d vector, return homo coords
// expects mat to be ROW MAJOR
inline __device__ glm::vec4 transform_4x4(const float *mat, const glm::vec3 p) {
    glm::vec4 out = {
        mat[0] * p.x + mat[1] * p.y + mat[2] * p.z + mat[3],
        mat[4] * p.x + mat[5] * p.y + mat[6] * p.z + mat[7],
        mat[8] * p.x + mat[9] * p.y + mat[10] * p.z + mat[11],
        mat[12] * p.x + mat[13] * p.y + mat[14] * p.z + mat[15],
    };
    return out;
}

template <typename T> __forceinline__ __device__ T sum(vec3<T> a) {
    return a.x + a.y + a.z;
}

}
#endif // GSPLAT_CUDA_HELPERS_H