#pragma once

#include <algorithm>
#include <cstdint>
#include <glm/gtc/type_ptr.hpp>

namespace gsplat {

//
// Some Macros.
//
#define CHECK_CUDA(x) TORCH_CHECK(x.is_cuda(), #x " must be a CUDA tensor")
#define CHECK_CONTIGUOUS(x)                                                    \
    TORCH_CHECK(x.is_contiguous(), #x " must be contiguous")
#define CHECK_INPUT(x)                                                         \
    CHECK_CUDA(x);                                                             \
    CHECK_CONTIGUOUS(x)
#define DEVICE_GUARD(_ten)                                                     \
    const at::cuda::OptionalCUDAGuard device_guard(device_of(_ten));

// https://github.com/pytorch/pytorch/blob/233305a852e1cd7f319b15b5137074c9eac455f6/aten/src/ATen/cuda/cub.cuh#L38-L46
// handle the temporary storage and 'twice' calls for cub API
#define CUB_WRAPPER(func, ...)                                                 \
    do {                                                                       \
        size_t temp_storage_bytes = 0;                                         \
        func(nullptr, temp_storage_bytes, __VA_ARGS__);                        \
        auto &caching_allocator = *::c10::cuda::CUDACachingAllocator::get();   \
        auto temp_storage = caching_allocator.allocate(temp_storage_bytes);    \
        func(temp_storage.get(), temp_storage_bytes, __VA_ARGS__);             \
    } while (false)    
//
// Convenience typedefs for CUDA types
//
using vec2 = glm::vec<2, float>;
using vec3 = glm::vec<3, float>;
using vec4 = glm::vec<4, float>;
using mat2 = glm::mat<2, 2, float>;
using mat3 = glm::mat<3, 3, float>;
using mat4 = glm::mat<4, 4, float>;
using mat3x2 = glm::mat<3, 2, float>;
using uvec4 = glm::vec<4, uint32_t>;
using ivec2 = glm::vec<2, int32_t>;
using uvec2 = glm::vec<2, uint32_t>;
//
// Legacy Camera Types
//
enum CameraModelType {
    PINHOLE = 0,
    ORTHO = 1,
    FISHEYE = 2,
    FTHETA = 3,
};

#define N_THREADS_PACKED 256
#define ALPHA_THRESHOLD (1.f / 255.f)
#define ALPHA_THRESHOLD_RCP (255.f)
#define NEAR_Z 0.2
#define FAR_Z 1000.0
#define N_SEQUENTIAL_THRESHOLD 4
#define TILE_WIDTH 16
#define TILE_HEIGHT 16
#define TILE_SIZE 16
#define TILE_BLOCK_SIZE 256
} // namespace gsplat