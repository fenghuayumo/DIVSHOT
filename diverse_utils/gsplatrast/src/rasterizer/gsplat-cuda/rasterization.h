#pragma once

#include <cstdint>
#include "cameras.h"
#include "common.h"
namespace at {
class Tensor;
}

namespace gsplat {

#define FILTER_INV_SQUARE_2DGS 2.0f

/////////////////////////////////////////////////
// rasterize_to_pixels_3dgs
/////////////////////////////////////////////////

template <uint32_t CDIM, bool GEO = false>
void launch_rasterize_to_pixels_3dgs_fwd_kernel(
    // Gaussian parameters
    const at::Tensor means2d,   // [C, N, 2] or [nnz, 2]
    const at::Tensor conics,    // [C, N, 3] or [nnz, 3]
    const at::Tensor colors,    // [C, N, channels] or [nnz, channels]
    const at::Tensor opacities, // [C, N]  or [nnz]
    const at::optional<at::Tensor> ray_ts,
    const at::optional<at::Tensor> ray_planes,
    const at::optional<at::Tensor> normals,
    const at::optional<at::Tensor> Ks,
    const at::optional<at::Tensor> backgrounds, // [C, channels]
    const at::optional<at::Tensor> masks,       // [C, tile_height, tile_width]
    // image size
    const uint32_t image_width,
    const uint32_t image_height,
    const uint32_t tile_size,
    // intersections
    const at::Tensor tile_offsets, // [C, tile_height, tile_width]
    const at::Tensor flatten_ids,  // [n_isects]
    // outputs
    at::Tensor renders, // [C, image_height, image_width, channels]
    at::Tensor alphas,  // [C, image_height, image_width]
    at::Tensor last_ids, // [C, image_height, image_width]
    at::Tensor expected_depths, 
    at::Tensor median_depths, 
    at::Tensor expected_normals, 
    at::Tensor median_ids,
    at::optional<at::Tensor> touchedPixel,
    at::optional<at::Tensor> splatT,
    bool count
);

template <uint32_t CDIM, bool GEO>
void launch_rasterize_to_pixels_3dgs_bwd_kernel(
// Gaussian parameters
const at::Tensor means2d,                   // [C, N, 2] or [nnz, 2]
const at::Tensor conics,                    // [C, N, 3] or [nnz, 3]
const at::Tensor colors,                    // [C, N, 3] or [nnz, 3]
const at::Tensor opacities,                 // [C, N] or [nnz]
const at::optional<at::Tensor> ray_ts,                    // [C, N] or [nnz]
const at::optional<at::Tensor> ray_planes,                // [C, N, 2] or [nnz, 2]
const at::optional<at::Tensor> normals,                   // [C, N, 3] or [nnz, 3]
const at::optional<at::Tensor> Ks,                   // [C, N, 3] or [nnz, 3]
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
const at::optional<at::Tensor> median_ids,      // [C, image_height, image_width]
// gradients of outputs
const at::Tensor v_render_colors, // [C, image_height, image_width, 3]
const at::Tensor v_render_alphas, // [C, image_height, image_width, 1]
const at::optional<at::Tensor> v_render_expected_depths, // [C, image_height, image_width, 1]
const at::optional<at::Tensor> v_render_median_depths, // [C, image_height, image_width, 1]
const at::optional<at::Tensor> v_render_expected_normals, // [C, image_height, image_width, 3]
// outputs
at::optional<at::Tensor> v_means2d_abs, // [C, N, 2] or [nnz, 2]
at::Tensor v_means2d,                   // [C, N, 2] or [nnz, 2]
at::Tensor v_conics,                    // [C, N, 3] or [nnz, 3]
at::Tensor v_colors,                    // [C, N, 3] or [nnz, 3]
at::Tensor v_opacities,                  // [C, N] or [nnz]
at::optional<at::Tensor> v_ray_ts,
at::optional<at::Tensor> v_ray_plane,
at::optional<at::Tensor> v_normals);

///////////////////////////////////////////////////
// rasterize_to_pixels_from_world_3dgs
///////////////////////////////////////////////////

template <uint32_t CDIM>
void launch_rasterize_to_pixels_from_world_3dgs_fwd_kernel(
    // Gaussian parameters
    const at::Tensor& means, // [N, 3]
    const at::Tensor& quats, // [N, 4]
    const at::Tensor& scales, // [N, 3]
    const at::Tensor& colors,    // [C, N, channels] or [nnz, channels]
    const at::Tensor& opacities, // [C, N]  or [nnz]
    const at::optional<at::Tensor>& backgrounds, // [C, channels]
    const at::optional<at::Tensor>& masks,       // [C, tile_height, tile_width]
    // image size
    const uint32_t image_width,
    const uint32_t image_height,
    const uint32_t tile_size,
    // camera
    const at::Tensor& viewmats0,             // [C, 4, 4]
    const at::optional<at::Tensor>& viewmats1, // [C, 4, 4] optional for rolling shutter
    const at::Tensor& Ks,                   // [C, 3, 3]
    const CameraModelType camera_model,
    // uncented transform
    const UnscentedTransformParameters& ut_params,
    ShutterType rs_type,
    const at::optional<at::Tensor>& radial_coeffs, // [C, 6] or [C, 4] optional
    const at::optional<at::Tensor>& tangential_coeffs, // [C, 2] optional
    const at::optional<at::Tensor>& thin_prism_coeffs, // [C, 2] optional
    const FThetaCameraDistortionParameters& ftheta_coeffs, // shared parameters for all cameras
    // intersections
    const at::Tensor& tile_offsets, // [C, tile_height, tile_width]
    const at::Tensor& flatten_ids,  // [n_isects]
    // outputs
    at::Tensor& renders, // [C, image_height, image_width, channels]
    at::Tensor& alphas,  // [C, image_height, image_width]
    at::Tensor& last_ids // [C, image_height, image_width]
);

template <uint32_t CDIM>
void launch_rasterize_to_pixels_from_world_3dgs_bwd_kernel(
    // Gaussian parameters
    const at::Tensor& means, // [N, 3]
    const at::Tensor& quats, // [N, 4]
    const at::Tensor& scales, // [N, 3]
    const at::Tensor& colors,                    // [C, N, 3] or [nnz, 3]
    const at::Tensor& opacities,                 // [C, N] or [nnz]
    const at::optional<at::Tensor>& backgrounds, // [C, 3]
    const at::optional<at::Tensor>& masks,       // [C, tile_height, tile_width]
    const at::optional<at::Tensor>& compensations, // [C, N] or [nnz] sigmoid
    // image size
    const uint32_t image_width,
    const uint32_t image_height,
    const uint32_t tile_size,
    // camera
    const at::Tensor& viewmats0,             // [C, 4, 4]
    const at::optional<at::Tensor>& viewmats1, // [C, 4, 4] optional for rolling shutter
    const at::Tensor& Ks,                   // [C, 3, 3]
    const CameraModelType camera_model,
    // uncented transform
    const UnscentedTransformParameters& ut_params,
    ShutterType rs_type,
    const at::optional<at::Tensor>& radial_coeffs, // [C, 6] or [C, 4] optional
    const at::optional<at::Tensor>& tangential_coeffs, // [C, 2] optional
    const at::optional<at::Tensor>& thin_prism_coeffs, // [C, 2] optional
    const FThetaCameraDistortionParameters& ftheta_coeffs, // shared parameters for all cameras
    // intersections
    const at::Tensor& tile_offsets, // [C, tile_height, tile_width]
    const at::Tensor& flatten_ids,  // [n_isects]
    // forward outputs
    const at::Tensor& render_alphas, // [C, image_height, image_width, 1]
    const at::Tensor& last_ids,      // [C, image_height, image_width]
    // gradients of outputs
    const at::Tensor& v_render_colors, // [C, image_height, image_width, 3]
    const at::Tensor& v_render_alphas, // [C, image_height, image_width, 1]
    // outputs
    at::Tensor& v_means,      // [N, 3]
    at::Tensor& v_quats,      // [N, 4]
    at::Tensor& v_scales,     // [N, 3]
    at::Tensor& v_colors,                    // [C, N, 3] or [nnz, 3]
    at::Tensor& v_opacities                  // [C, N] or [nnz]
) ;

} // namespace gsplat