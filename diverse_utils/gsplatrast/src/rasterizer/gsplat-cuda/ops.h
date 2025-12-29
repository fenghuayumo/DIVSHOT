// A collection of operators for gsplat
#pragma once

#include <ATen/core/Tensor.h>
#include "cameras.h"
#include "common.h"

namespace gsplat {

// null operator for tutorial. Does nothing.
at::Tensor null(const at::Tensor input);

// Fuse the following operations:
// 1. compute covar from {quats, scales}
// 2. transform 3D gaussians from world space to camera space
//    - w/ near far plane check
// 3. projection camera space 3D gaussians to 2D image planes with EWA
// splatting.
//    - w/ minimum radius check
// 4. add a bit blurring to the 2D gaussians for anti-aliasing.
std::tuple<
    at::Tensor,
    at::Tensor,
    at::Tensor,
    at::Tensor,
    at::Tensor,
    at::Tensor,
    at::Tensor,
    at::Tensor,
    at::Tensor>
projection_ewa_3dgs_fused_fwd(
    const at::Tensor& means,                // [N, 3]
    const at::optional<at::Tensor>& covars, // [N, 6] optional
    const at::optional<at::Tensor>& quats,  // [N, 4] optional
    const at::optional<at::Tensor>& scales, // [N, 3] optional
    const at::Tensor& opacities, // [N] optional
    const at::Tensor& viewmats,             // [C, 4, 4]
    const at::Tensor& Ks,                   // [C, 3, 3]
    const uint32_t image_width,
    const uint32_t image_height,
    const float eps2d,
    const float near_plane,
    const float far_plane,
    const float radius_clip,
    const bool calc_compensations,
    const CameraModelType camera_model,
    const bool calc_gof
);
std::tuple<at::Tensor, at::Tensor, at::Tensor, at::Tensor, at::Tensor, at::Tensor>
projection_ewa_3dgs_fused_bwd(
    // fwd inputs
    const at::Tensor& means,                // [N, 3]
    const at::optional<at::Tensor>& covars, // [N, 6] optional
    const at::optional<at::Tensor>& quats,  // [N, 4] optional
    const at::optional<at::Tensor>& scales, // [N, 3] optional
    const at::Tensor& opacities, // [N] optional
    const at::Tensor& viewmats,             // [C, 4, 4]
    const at::Tensor& Ks,                   // [C, 3, 3]
    const uint32_t image_width,
    const uint32_t image_height,
    const float eps2d,
    const CameraModelType camera_model,
    // fwd outputs
    const at::Tensor& radii,                       // [C, N, 2]
    const at::Tensor& conics,                      // [C, N, 3]
    const at::optional<at::Tensor>& compensations, // [C, N] optional
    // grad outputs
    const at::Tensor& v_means2d,                     // [C, N, 2]
    const at::optional<at::Tensor>& v_depths,                      // [C, N]
    const at::Tensor& v_conics,                      // [C, N, 3]
    const at::optional<at::Tensor>& v_compensations, // [C, N] optional
    const at::Tensor& v_t_opacities, // [N] optional
    const at::optional<at::Tensor>& v_ray_ts,                        // [C, N]
    const at::optional<at::Tensor>& v_ray_planes,                    // [C, N, 2]
    const at::optional<at::Tensor>& v_normals,                       // [C, N, 3]
    const bool viewmats_requires_grad
);

// On top of fusing the operations like `projection_ewa_3dgs_fused_{fwd, bwd}`,
// The packed version compresses the [C, N, D] tensors (both intermidiate and
// output) into a jagged format [nnz, D], leveraging the sparsity of these
// tensors.
//
// This could lead to less memory usage than `_fused_{fwd, bwd}` if the level of
// sparsity is high, i.e., most of the gaussians are not in the camera frustum.
// But at the cost of slightly slower speed.
std::tuple<
    at::Tensor,
    at::Tensor,
    at::Tensor,
    at::Tensor,
    at::Tensor,
    at::Tensor,
    at::Tensor,
    at::Tensor,
    at::Tensor,
    at::Tensor,
    at::Tensor,
    at::Tensor>
projection_ewa_3dgs_packed_fwd(
    const at::Tensor& means,                   // [N, 3]
    const at::optional<at::Tensor>& covars,    // [N, 6] optional
    const at::optional<at::Tensor>& quats,     // [N, 4] optional
    const at::optional<at::Tensor>& scales,    // [N, 3] optional
    const at::Tensor& opacities, // [N] optional
    const at::Tensor& viewmats,                // [C, 4, 4]
    const at::Tensor& Ks,                      // [C, 3, 3]
    const uint32_t image_width,
    const uint32_t image_height,
    const float eps2d,
    const float near_plane,
    const float far_plane,
    const float radius_clip,
    const bool calc_compensations,
    const CameraModelType camera_model,
    const bool calc_gof
);
std::tuple<at::Tensor, at::Tensor, at::Tensor, at::Tensor, at::Tensor, at::Tensor>
projection_ewa_3dgs_packed_bwd(
    // fwd inputs
    const at::Tensor& means,                // [N, 3]
    const at::optional<at::Tensor>& covars, // [N, 6]
    const at::optional<at::Tensor>& quats,  // [N, 4]
    const at::optional<at::Tensor>& scales, // [N, 3]
    const at::Tensor& opacities, // [N] optional
    const at::Tensor& viewmats,             // [C, 4, 4]
    const at::Tensor& Ks,                   // [C, 3, 3]
    const uint32_t image_width,
    const uint32_t image_height,
    const float eps2d,
    const CameraModelType camera_model,
    // fwd outputs
    const at::Tensor& camera_ids,                  // [nnz]
    const at::Tensor& gaussian_ids,                // [nnz]
    const at::Tensor& conics,                      // [nnz, 3]
    const at::optional<at::Tensor>& compensations, // [nnz] optional
    // grad outputs
    const at::Tensor& v_means2d,                     // [nnz, 2]
    const at::optional<at::Tensor>& v_depths,                      // [nnz]
    const at::Tensor& v_conics,                      // [nnz, 3]
    const at::optional<at::Tensor>& v_compensations, // [nnz] optional
    const at::Tensor& v_t_opacities, // [N] optional
    const at::optional<at::Tensor>& v_ray_ts,                        // [nnz]
    const at::optional<at::Tensor>& v_ray_planes,                    // [nnz, 2]
    const at::optional<at::Tensor>& v_normals,                       // [nnz, 3]
    const bool viewmats_requires_grad,
    const bool sparse_grad
);


// Sphereical harmonics
at::Tensor spherical_harmonics_fwd(
    const uint32_t degrees_to_use,
    const at::Tensor dirs,               // [..., 3]
    const at::Tensor sh0_coeffs,          // [..., 1, 3]
    const at::Tensor shN_coeffs,          // [..., K-1, 3]
    const at::optional<at::Tensor> masks // [...]
);
std::tuple<at::Tensor, at::Tensor, at::Tensor> spherical_harmonics_bwd(
    const uint32_t K,
    const uint32_t degrees_to_use,
    const at::Tensor dirs,                // [..., 3]
    const at::Tensor shN_coeffs,          // [..., K-1, 3]
    const at::optional<at::Tensor> masks, // [...]
    const at::Tensor v_colors,            // [..., 3]
    bool compute_v_dirs);


    std::tuple<
    at::Tensor, //render_colors
    at::Tensor, //render_alphas
    at::Tensor, //render_e_depths
    at::Tensor, //render_m_depths
    at::Tensor, //render_e_normals
    at::Tensor, //last_ids
    at::Tensor, //median_ids
    at::Tensor, //ray_ts,
    at::Tensor, //ray_planes,
    at::Tensor, //normals,
    at::Tensor, //means2d,
    at::Tensor, //conics,
    at::Tensor, //compensations,
    at::Tensor, //radii,
    at::Tensor, //colors,
    at::Tensor, //t_opacities,
    at::Tensor, //tile_offsets,
    at::Tensor, //flatten_ids,
    at::Tensor, //camera_ids,
    at::Tensor, //gaussian_ids
    at::Tensor, //sh_masks
    at::Tensor, //view_dirs
    at::Tensor, //shs_0_indexed
    at::Tensor, //shs_n_indexed
    at::Tensor
    >
fused_3dgs_rasterize_fwd(
    const at::Tensor& means,                   // [N, 3]
    const at::Tensor& quats,     // [N, 4] optional
    const at::Tensor& scales,    // [N, 3] optional
    const at::Tensor& opacities, // [N] optional
    const at::Tensor& shs_0, // [N, K, 3]
    const at::Tensor& shs_n, // [N, K, 3]
    const at::Tensor& viewmats,                // [C, 4, 4]
    const at::Tensor& Ks,                      // [C, 3, 3]
    const at::Tensor& T,
    const at::optional<at::Tensor> &backgrounds, // [C, channels]
    const uint32_t image_width,
    const uint32_t image_height,
    const uint32_t degrees_to_use,
    const float eps2d,
    const float near_plane,
    const float far_plane,
    const float radius_clip,
    const uint32_t tile_size,
    const bool calc_compensations,
    const CameraModelType camera_model,
    const bool calc_gof,
    const bool packed
);


std::tuple<
    at::Tensor, //v_means2d_abs
    at::Tensor, //v_means2d
    at::Tensor, //v_means
    at::Tensor, //v_quats
    at::Tensor, //v_scales,
    at::Tensor, //v_opacities,
    at::Tensor, //v_shs_0,
    at::Tensor> //v_shs_n,
fused_3dgs_rasterize_bwd(
    // Gaussian parameters
    const at::Tensor &means,                   // [C, N, 2] or [nnz, 2]
    const at::Tensor &quats,                    // [C, N, 3] or [nnz, 3]
    const at::Tensor &scales,                    // [C, N, 3] or [nnz, 3]
    const at::Tensor &opacities,                 // [C, N] or [nnz]
    const at::Tensor &shs_0_indexed,
    const at::Tensor &shs_n_indexed,
    const at::Tensor &means2d,
    const at::Tensor &conics,
    const at::Tensor &colors,
    const at::Tensor &t_opacities,
    const CameraModelType camera_model,
    const at::optional<at::Tensor> &ray_ts,                    // [C, N] or [nnz]
    const at::optional<at::Tensor> &ray_planes,                // [C, N, 2] or [nnz, 2]
    const at::optional<at::Tensor> &normals,                   // [C, N, 3] or [nnz, 3]
    const at::optional<at::Tensor> &backgrounds, // [C, 3]
    // image size
    const uint32_t image_width, 
    const uint32_t image_height,
    const uint32_t degrees_to_use, 
    const uint32_t tile_size,
    const at::Tensor &viewmats,
    const at::Tensor &Ks,
    const at::optional<at::Tensor> &compensations,
    const at::Tensor &radii,
    // intersections
    const at::Tensor &tile_offsets, // [C, tile_height, tile_width]
    const at::Tensor &flatten_ids,  // [n_isects]
     // forward outputs
    const at::Tensor &render_alphas, // [C, image_height, image_width, 1]
    const at::Tensor &last_ids,      // [C, image_height, image_width]
    const at::optional<at::Tensor> &median_ids,      // [C, image_height, image_width]
    // gradients of outputs
    const at::Tensor &v_render_colors, // [C, image_height, image_width, 3]
    const at::Tensor &v_render_alphas, // [C, image_height, image_width, 1]
    const at::optional<at::Tensor> &v_render_expected_depths, // [C, image_height, image_width, 1]
    const at::optional<at::Tensor> &v_render_median_depths, // [C, image_height, image_width, 1]
    const at::optional<at::Tensor> &v_render_expected_normals, // [C, image_height, image_width, 3]
    const at::optional<at::Tensor> &sh_masks,
    const at::Tensor &viewDirs,
    const at::optional<at::Tensor> &camera_ids,
    const at::optional<at::Tensor> &gaussian_ids,
    const float eps2d,
    // options
    const bool calc_gof,
    const bool calc_compensations,
    const bool packed,
    const bool sparse_grad,
    const bool absgrad);
// Fused Adam that supports a valid mask to skip updating certain parameters.
// Note skipping is not equivalent with zeroing out the gradients, which will
// still update parameters with momentum.
void adam(
    at::Tensor &param,                    // [..., D]
    const at::Tensor &param_grad,         // [..., D]
    at::Tensor &exp_avg,                  // [..., D]
    at::Tensor &exp_avg_sq,               // [..., D]
    const at::optional<at::Tensor> valid, // [...]
    at::Tensor &step_per_gaussian,       // [N]
    const float lr,
    const float b1,
    const float b2,
    const float eps
);

void adam_step(
    at::Tensor& param,
    at::Tensor& exp_avg,
    at::Tensor& exp_avg_sq,
    const at::Tensor& param_grad,
    const float lr,
    const float beta1,
    const float beta2,
    const float eps,
    const float bias_correction1,
    const float bias_correction2_sqrt
);

// Sparse Adam optimizer - only updates parameters at sparse gradient locations
void launch_sparse_adam_kernel(
    at::Tensor& param,
    at::Tensor& exp_avg,
    at::Tensor& exp_avg_sq,
    const at::Tensor& grad_indices,
    const at::Tensor& grad_values,
    const float lr,
    const float beta1,
    const float beta2,
    const float eps,
    const float bias_correction1_rcp,
    const float bias_correction2_sqrt_rcp
);

// GS Tile Intersection
std::tuple<at::Tensor, at::Tensor, at::Tensor> intersect_tile(
    const at::Tensor means2d,                    // [C, N, 2] or [nnz, 2]
    const at::Tensor radii,                      // [C, N, 2] or [nnz, 2]
    const at::Tensor depths,                     // [C, N] or [nnz]
    const at::optional<at::Tensor> camera_ids,   // [nnz]
    const at::optional<at::Tensor> gaussian_ids, // [nnz]
    const at::optional<at::Tensor> conics, // [C,N,3] or [nnz,3]
    const at::optional<at::Tensor> opacities, // [C,N] or [nnz]
    const uint32_t C,
    const uint32_t tile_size,
    const uint32_t tile_width,
    const uint32_t tile_height,
    const bool sort
);
at::Tensor intersect_offset(
    const at::Tensor isect_ids, // [n_isects]
    const uint32_t C,
    const uint32_t tile_width,
    const uint32_t tile_height
);

// Compute Covariance and Precision Matrices from Quaternion and Scale
std::tuple<at::Tensor, at::Tensor> quat_scale_to_covar_preci_fwd(
    const at::Tensor quats,  // [N, 4]
    const at::Tensor scales, // [N, 3]
    const bool compute_covar,
    const bool compute_preci,
    const bool triu
);
std::tuple<at::Tensor, at::Tensor> quat_scale_to_covar_preci_bwd(
    const at::Tensor quats,  // [N, 4]
    const at::Tensor scales, // [N, 3]
    const bool triu,
    const at::optional<at::Tensor> v_covars, // [N, 3, 3] or [N, 6]
    const at::optional<at::Tensor> v_precis  // [N, 3, 3] or [N, 6]
);

// Rasterize 3D Gaussian to pixels
std::tuple<at::Tensor, at::Tensor, at::Tensor> rasterize_to_pixels_3dgs_fwd(
    // Gaussian parameters
    const at::Tensor means2d,   // [C, N, 2] or [nnz, 2]
    const at::Tensor conics,    // [C, N, 3] or [nnz, 3]
    const at::Tensor colors,    // [C, N, channels] or [nnz, channels]
    const at::Tensor opacities, // [C, N]  or [nnz]
    const at::optional<at::Tensor> backgrounds, // [C, channels]
    const at::optional<at::Tensor> masks,       // [C, tile_height, tile_width]
    // image size
    const uint32_t image_width,
    const uint32_t image_height,
    const uint32_t tile_size,
    // intersections
    const at::Tensor tile_offsets, // [C, tile_height, tile_width]
    const at::Tensor flatten_ids   // [n_isects]
);

std::tuple<at::Tensor, at::Tensor, at::Tensor, at::Tensor, at::Tensor, at::Tensor, at::Tensor>
rasterize_to_pixels_3dgs_w_depth_fwd(
    // Gaussian parameters
    const at::Tensor &means2d,    // [C, N, 2] or [nnz, 2]
    const at::Tensor &conics,     // [C, N, 3] or [nnz, 3]
    const at::Tensor &colors,     // [C, N, channels] or [nnz, channels]
    const at::Tensor &opacities,  // [C, N]  or [nnz]
    const at::Tensor &ray_ts,     // [C, N] or [nnz]
    const at::Tensor &ray_planes, // [C, N, 2] or [nnz, 2]
    const at::Tensor &normals,    // [C, N, 3] or [nnz, 3]
    const at::optional<at::Tensor> &backgrounds, // [C, channels]
    const at::optional<at::Tensor> &masks,       // [C, tile_height, tile_width]
    // image size
    const uint32_t image_width, const uint32_t image_height, const uint32_t tile_size,
    const at::Tensor &Ks,
    // intersections
    const at::Tensor &tile_offsets, // [C, tile_height, tile_width]
    const at::Tensor &flatten_ids   // [n_isects]
);

std::tuple<at::Tensor, at::Tensor, at::Tensor, at::Tensor, at::Tensor>
rasterize_to_pixels_3dgs_bwd(
    // Gaussian parameters
    const at::Tensor means2d,                   // [C, N, 2] or [nnz, 2]
    const at::Tensor conics,                    // [C, N, 3] or [nnz, 3]
    const at::Tensor colors,                    // [C, N, 3] or [nnz, 3]
    const at::Tensor opacities,                 // [C, N] or [nnz]
    const at::optional<at::Tensor> backgrounds, // [C, 3]
    const at::optional<at::Tensor> masks,       // [C, tile_height, tile_width]
    // image size
    const uint32_t image_width,
    const uint32_t image_height,
    const uint32_t tile_size,
    // intersections
    const at::Tensor tile_offsets, // [C, tile_height, tile_width]
    const at::Tensor flatten_ids,  // [n_isects]
    // forward outputs
    const at::Tensor render_alphas, // [C, image_height, image_width, 1]
    const at::Tensor last_ids,      // [C, image_height, image_width]
    // gradients of outputs
    const at::Tensor v_render_colors, // [C, image_height, image_width, 3]
    const at::Tensor v_render_alphas, // [C, image_height, image_width, 1]
    // options
    bool absgrad
);

std::tuple<at::Tensor, at::Tensor, at::Tensor, at::Tensor, at::Tensor, at::Tensor, at::Tensor, at::Tensor>
rasterize_to_pixels_3dgs_w_depth_bwd(
    // Gaussian parameters
    const at::Tensor &means2d,                   // [C, N, 2] or [nnz, 2]
    const at::Tensor &conics,                    // [C, N, 3] or [nnz, 3]
    const at::Tensor &colors,                    // [C, N, 3] or [nnz, 3]
    const at::Tensor &opacities,                 // [C, N] or [nnz]
    const at::Tensor &ray_ts,                    // [C, N] or [nnz]
    const at::Tensor &ray_planes,                // [C, N, 2] or [nnz, 2]
    const at::Tensor &normals,                   // [C, N, 3] or [nnz, 3]
    const at::optional<at::Tensor> &backgrounds, // [C, 3]
    const at::optional<at::Tensor> &masks,       // [C, tile_height, tile_width]
    // image size
    const uint32_t image_width, const uint32_t image_height, const uint32_t tile_size,
    const at::Tensor &Ks,
    // intersections
    const at::Tensor &tile_offsets, // [C, tile_height, tile_width]
    const at::Tensor &flatten_ids,  // [n_isects]
    // forward outputs
    const at::Tensor &render_alphas, // [C, image_height, image_width, 1]
    const at::Tensor &last_ids,      // [C, image_height, image_width]
    const at::Tensor &median_ids,      // [C, image_height, image_width]
    // gradients of outputs
    const at::Tensor &v_render_colors, // [C, image_height, image_width, 3]
    const at::Tensor &v_render_alphas, // [C, image_height, image_width, 1]
    const at::Tensor &v_render_expected_depths, // [C, image_height, image_width, 1]
    const at::Tensor &v_render_median_depths, // [C, image_height, image_width, 1]
    const at::Tensor &v_render_expected_normals, // [C, image_height, image_width, 3]
    // options
    bool absgrad);

// Relocate some Gaussians in the Densification Process.
// Equation (9) in "3D Gaussian Splatting as Markov Chain Monte Carlo"
std::tuple<at::Tensor, at::Tensor> relocation(
    at::Tensor opacities, // [N]
    at::Tensor scales,    // [N, 3]
    at::Tensor ratios,    // [N]
    at::Tensor binoms,    // [n_max, n_max]
    const int n_max
);

void add_noise(
    at::Tensor opacities, // [N]
    at::Tensor scales,    // [N, 3]
    at::Tensor quats,     // [N, 4]
    at::Tensor noise,         // [N, 3]
    at::Tensor means,         // [N, 3]
    const float current_lr
);


std::tuple<at::Tensor, at::Tensor, at::Tensor, at::Tensor> rasterize_3dsplats_count_fwd(
    // Gaussian parameters
    const at::Tensor &means2d,                   // [C, N, 2]
    const at::Tensor &conics,                    // [C, N, 3]
    const at::Tensor &colors,                    // [C, N, D]
    const at::Tensor &opacities,                 // [N]
    const at::optional<at::Tensor> &backgrounds, // [C, D]
    const at::optional<at::Tensor> &mask,        // [C, tile_height, tile_width]
    // image size
    const uint32_t image_width, const uint32_t image_height, const uint32_t tile_size,
    // intersections
    const at::Tensor &tile_offsets, // [C, tile_height, tile_width]
    const at::Tensor &flatten_ids,   // [n_isects]
    const bool splat_count
);

std::tuple<at::Tensor, at::Tensor, at::Tensor, at::Tensor, at::Tensor> render_3dsplats_count_fwd(
    const at::Tensor& means,                   // [C, N, 3]
    const at::Tensor& scales,                    // [C, N, 3]
    const at::Tensor& quats,                    // [C, N, 4]
    const at::Tensor& opacities,                 // [N]
    int degrees_to_use,
    const at::Tensor& sh0_coeffs,                 // [N,1,3]
    const at::Tensor& shN_coeffs,               // [N,15,3]
    const at::Tensor& T,
    const at::Tensor& viewMat,
    const at::Tensor& ksMat,
    const at::optional<at::Tensor>& backgrounds, // [C, 3]
    const uint32_t image_width, const uint32_t image_height,
    float eps2d,
    float nearPlane,
    float farPlane,
    float radiusClip,
    CameraModelType camera_model,
    bool  splatCount
);

std::tuple<at::Tensor, at::Tensor>
reduced_kmeans(
    const at::Tensor& values,
    const at::Tensor& centers,
    const float tol,
    const int max_iterations);
at::Tensor distKnn2(const at::Tensor& points);
std::tuple<at::Tensor, at::Tensor> distIndices2(const at::Tensor& points, int K);
std::tuple<at::Tensor, at::Tensor> distIndicesQ(const at::Tensor& points, const at::Tensor& q_indices, const at::Tensor& n_indices, int K);

std::tuple<at::Tensor, at::Tensor, at::Tensor, at::Tensor>
fusedssim(
  float C1,
  float C2,
  at::Tensor &img1,
  at::Tensor &img2,
  bool train
);

at::Tensor
fusedssim_backward(
  float C1,
  float C2,
  at::Tensor &img1,
  at::Tensor &img2,
  at::Tensor &dL_dmap,
  at::Tensor &dm_dmu1,
  at::Tensor &dm_dsigma1_sq,
  at::Tensor &dm_dsigma12
);

} // namespace gsplat
