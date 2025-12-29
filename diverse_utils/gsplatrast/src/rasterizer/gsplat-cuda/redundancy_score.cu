#ifdef USE_HIP
#include <hip/hip_runtime.h>
#include <hip/hip_cooperative_groups.h>
#else
#include <cooperative_groups.h>
#include <cooperative_groups/reduce.h>
#include <cuda.h>
#include <cuda_runtime.h>
#endif
#include "common.h"
#include "ops.h"

namespace cg = cooperative_groups;

inline __device__ glm::vec4 transform_4x4(const float* mat, const glm::vec3 p) {
    glm::vec4 out = {
        mat[0] * p.x + mat[1] * p.y + mat[2] * p.z + mat[3],
        mat[4] * p.x + mat[5] * p.y + mat[6] * p.z + mat[7],
        mat[8] * p.x + mat[9] * p.y + mat[10] * p.z + mat[11],
        mat[12] * p.x + mat[13] * p.y + mat[14] * p.z + mat[15],
    };
    return out;
}
 inline __device__ glm::mat3 quat_to_rotmat(const glm::vec4 quat) {
    float w = quat[0], x = quat[1], y = quat[2], z = quat[3];
    // normalize
    float inv_norm = rsqrt(x * x + y * y + z * z + w * w);
    x *= inv_norm;
    y *= inv_norm;
    z *= inv_norm;
    w *= inv_norm;
    float x2 = x * x, y2 = y * y, z2 = z * z;
    float xy = x * y, xz = x * z, yz = y * z;
    float wx = w * x, wy = w * y, wz = w * z;
    return glm::mat3((1.f - 2.f * (y2 + z2)), (2.f * (xy + wz)),
        (2.f * (xz - wy)), // 1st col
        (2.f * (xy - wz)), (1.f - 2.f * (x2 + z2)),
        (2.f * (yz + wx)), // 2nd col
        (2.f * (xz + wy)), (2.f * (yz - wx)),
        (1.f - 2.f * (x2 + y2)) // 3rd col
    );
}
__global__ void findMinimumRedundancyValueCUDA(
    const int P,
    const int *redundancy_values,
    const int *neighbours_indices,
    const bool *intersection_mask,
    int *minimum_redundancy_values,
    const int knn)
{
    auto idx = cg::this_grid().thread_rank();
    if (idx >= P)
        return;
    const bool *curr_intersection_mask = intersection_mask + idx * knn;
    const int *curr_neighbours_indices = neighbours_indices + idx * knn;
    for (int i = 0; i < knn; i++)
    {
        if (curr_intersection_mask[i])
        {
            int neighbour_idx = curr_neighbours_indices[i];
            atomicMin(&minimum_redundancy_values[neighbour_idx], redundancy_values[idx]);
        }
    }
}

void findMinimumRedundancyValue(const int P,
                                const int *redundancy_values,
                                const int *neighbours_indices,
                                const bool *intersection_mask,
                                int *minimum_redundancy_values,
                                const int knn)
{
    findMinimumRedundancyValueCUDA<<<(P + 255) / 256, 256>>>(P,
                                                             redundancy_values,
                                                             neighbours_indices,
                                                             intersection_mask,
                                                             minimum_redundancy_values,
                                                             knn);
}


__global__ void transformCentersNDCCUDA(
    const int P,
    const glm::vec3 *centers,
    const float *projmatrix,
    const float *inverse_projmatrix,
    const int image_height,
    const int image_width,
    float *pixel_sizes)
{
    auto idx = cg::this_grid().thread_rank();
    if (idx >= P)
        return;

    // Transform point by projecting
    glm::vec3 p_orig = centers[idx];
    glm::vec4 p_hom = transform_4x4(projmatrix,p_orig);

    float p_w = 1.0f / (p_hom.w + 0.0000001f);

    // vec4 to vec3 discards the last component
    // Might be better done with swizzling
    glm::vec3 p_proj = glm::vec3(p_hom) * p_w;
    float depth = p_proj.z;

    // Our NDC ranges from -1 1 for x and y and 0 to 1 for z
    bool isInside = glm::all(glm::lessThanEqual(p_proj, glm::vec3(1.f))) && glm::all(glm::greaterThanEqual(p_proj, glm::vec3(-1.f, -1.f, 0.f)));

    // Take two points that have a pixel wide distance, inverse project them and calculate the final distance
    if (isInside)
    {
        glm::vec3 p_proj_end(0.f);
        if (image_width > image_height)
            p_proj_end.x = 2.f / image_width;
        else
            p_proj_end.y = 2.f / image_height;
        p_proj_end.z = depth;

        glm::vec3 p_proj_start(0.f);
        p_proj_start.z = depth;

        glm::vec4 p_orig_end = transform_4x4(inverse_projmatrix, p_proj_end);

        p_w = 1.f / (p_orig_end.w + 0.0000001f);
        glm::vec3 p_orig_end_norm = glm::vec3(p_orig_end) * p_w;

        glm::vec4 p_orig_start = *inverse_projmatrix * glm::vec4(p_proj_start, glm::vec1(1.f));
        p_w = 1.f / (p_orig_start.w + 0.0000001f);
        glm::vec3 p_orig_start_norm = glm::vec3(p_orig_start) * p_w;

        glm::vec3 difference = p_orig_end_norm - p_orig_start_norm;
        pixel_sizes[idx] = min(pixel_sizes[idx], glm::length(difference));
    }
}

void transformCentersNDC(
    const int P,
    const float *centers,
    const float *projmatrix,
    const float *inverse_projmatrix,
    const int image_height,
    const int image_width,
    float *pixel_sizes)
{
    transformCentersNDCCUDA<<<(P + 255) / 256, 256>>>(
        P,
        (const glm::vec3*)centers,
        (const float*)projmatrix,
        (const float*)inverse_projmatrix,
        image_height,
        image_width,
        pixel_sizes);
}


__global__ void sphereEllipsoidIntersectionCUDA(
    const int P,
    const glm::vec3 *means3D,
    const glm::vec3 *scales,
    const float *quats,
    const int *neighbours_indices,
    const float *sphere_radius,
    int *redundancy_values,
    bool *intersection_mask,
    const int knn)
{
    auto idx = cg::this_grid().thread_rank();
    if (idx >= P)
        return;

    const glm::vec3 curr_xyz = means3D[idx];
    const int *curr_neighbours = neighbours_indices + idx * knn;
    bool *curr_intersection_mask = intersection_mask + idx * knn;
    const float curr_radius = sphere_radius[idx];
    
    glm::vec4 quat = glm::make_vec4(quats + idx * 4);
    glm::mat3 R = quat_to_rotmat(quat);

    // get neighbours
    for (int i = 0; i < knn; ++i)
    {
        // get scales of neighbour
        // get rotations of neighbour
        // intersection test if yes ++1
        int neighbour_id = curr_neighbours[i];
        glm::vec3 difference = curr_xyz - means3D[neighbour_id];
        glm::vec3 augmented_neighbour_scales = scales[neighbour_id] + glm::vec3(curr_radius);
        // Change of basis: x_old = A x_new -> x_new = A^T x_old for orthonormal
        // equivalent to left multiplication
        glm::vec3 difference_neigbhours_coordinate_system = R * difference;

        if (glm::dot(glm::pow(difference_neigbhours_coordinate_system, glm::vec3(2)), glm::vec3(1) / glm::pow(augmented_neighbour_scales, glm::vec3(2))) < 1)
        {
            redundancy_values[idx]++;
            curr_intersection_mask[i] = true;
        }
    }
    return;
}

void sphereEllipsoidIntersection(
    const int P,
    const float *means3D,
    const float *scales,
    const float *quats,
    const int *neighbours_indices,
    const float *sphere_radius,
    int *redundancy_values,
    bool *intersection_mask,
    const int knn)
{
    sphereEllipsoidIntersectionCUDA<<<(P + 255) / 256, 256>>>(
        P,
        (const glm::vec3*)means3D,
        (const glm::vec3*)scales,
        (const float*)quats,
        neighbours_indices,
        sphere_radius,
        redundancy_values,
        intersection_mask,
        knn);
}
