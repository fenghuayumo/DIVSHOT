#include <ATen/TensorUtils.h>
#include <ATen/core/Tensor.h>
#include <c10/cuda/CUDAGuard.h> // for DEVICE_GUARD
#include <tuple>

#include <ATen/Functions.h>
#include <ATen/NativeFunctions.h>

#include "common.h"
#include "ops.h"
#include "rasterization.h"

namespace gsplat {

////////////////////////////////////////////////////
// 3DGS
////////////////////////////////////////////////////
template<typename T>
T call_kernel_with_dim_3dgs_fwd(
    // Gaussian parameters
    const at::Tensor means2d,   // [C, N, 2] or [nnz, 2]
    const at::Tensor conics,    // [C, N, 3] or [nnz, 3]
    const at::Tensor colors,    // [C, N, channels] or [nnz, channels]
    const at::Tensor opacities, // [C, N]  or [nnz]
    const at::optional<at::Tensor> &ray_ts,     // [C, N] or [nnz]
    const at::optional<at::Tensor> &ray_planes, // [C, N, 2] or [nnz, 2]
    const at::optional<at::Tensor> &normals,    // [C, N, 3] or [nnz, 3]
    const at::optional<at::Tensor> Ks, // [C, 3, 3]
    const at::optional<at::Tensor> backgrounds, // [C, channels]
    const at::optional<at::Tensor> masks,       // [C, tile_height, tile_width]
    // image size
    const uint32_t image_width,
    const uint32_t image_height,
    const uint32_t tile_size,
    // intersections
    const at::Tensor tile_offsets, // [C, tile_height, tile_width]
    const at::Tensor flatten_ids,   // [n_isects]
    at::optional<at::Tensor>    touchedPixel,
    at::optional<at::Tensor>    splatT,
    bool count
) {
    DEVICE_GUARD(means2d);
    CHECK_INPUT(means2d);
    CHECK_INPUT(conics);
    CHECK_INPUT(colors);
    CHECK_INPUT(opacities);
    CHECK_INPUT(tile_offsets);
    CHECK_INPUT(flatten_ids);
    if (backgrounds.has_value()) {
        CHECK_INPUT(backgrounds.value());
    }
    if (masks.has_value()) {
        CHECK_INPUT(masks.value());
    }

    uint32_t C = tile_offsets.size(0); // number of cameras
    uint32_t channels = colors.size(-1);

    at::Tensor renders =
        at::empty({C, image_height, image_width, channels}, means2d.options());
    at::Tensor alphas =
        at::empty({C, image_height, image_width, 1}, means2d.options());
    at::Tensor last_ids = at::empty(
        {C, image_height, image_width}, means2d.options().dtype(at::kInt)
    );
    at::Tensor expected_depths, median_depths, expected_normals, median_ids;
    constexpr unsigned int output_size = std::tuple_size_v<T>;
    constexpr bool GEO = output_size > 3;
    if constexpr (GEO)
    {
        CHECK_INPUT(ray_ts.value());
        CHECK_INPUT(ray_planes.value());
        CHECK_INPUT(normals.value());
    }
    if constexpr (GEO) {
        expected_depths = at::empty({C, image_height, image_width, 1},
                                            means2d.options().dtype(at::kFloat));
        median_depths = at::empty({C, image_height, image_width, 1},
                                            means2d.options().dtype(at::kFloat));
        expected_normals = at::empty({C, image_height, image_width, 3},
                                        means2d.options().dtype(at::kFloat));
        median_ids = at::empty({C, image_height, image_width},
                                          means2d.options().dtype(at::kInt));
    }
#define __LAUNCH_KERNEL__(N)                                                   \
    case N:                                                                    \
        launch_rasterize_to_pixels_3dgs_fwd_kernel<N,GEO>(                         \
            means2d,                                                           \
            conics,                                                            \
            colors,                                                            \
            opacities,                                                         \
            ray_ts,                                                            \
            ray_planes,                                                        \
            normals,                                                           \
            Ks,                                                                \
            backgrounds,                                                       \
            masks,                                                             \
            image_width,                                                       \
            image_height,                                                      \
            tile_size,                                                         \
            tile_offsets,                                                      \
            flatten_ids,                                                       \
            renders,                                                           \
            alphas,                                                            \
            last_ids,                                                           \
            expected_depths,                                                   \
            median_depths,                                                    \
            expected_normals,                                                 \
            median_ids,                                                        \
            touchedPixel,                                                      \
            splatT,                                                       \
            count                                                               \
        );                                                                     \
        break;

    // TODO: an optimization can be done by passing the actual number of
    // channels into the kernel functions and avoid necessary global memory
    // writes. This requires moving the channel padding from python to C side.
    switch (channels) {
        __LAUNCH_KERNEL__(1)
        __LAUNCH_KERNEL__(2)
        __LAUNCH_KERNEL__(3)
        __LAUNCH_KERNEL__(4)
        // __LAUNCH_KERNEL__(5)
        // __LAUNCH_KERNEL__(8)
        // __LAUNCH_KERNEL__(9)
        // __LAUNCH_KERNEL__(16)
        // __LAUNCH_KERNEL__(17)
        // __LAUNCH_KERNEL__(32)
        // __LAUNCH_KERNEL__(33)
        // __LAUNCH_KERNEL__(64)
        // __LAUNCH_KERNEL__(65)
        // __LAUNCH_KERNEL__(128)
        // __LAUNCH_KERNEL__(129)
        // __LAUNCH_KERNEL__(256)
        // __LAUNCH_KERNEL__(257)
        // __LAUNCH_KERNEL__(512)
        // __LAUNCH_KERNEL__(513)
    default:
        AT_ERROR("Unsupported number of channels: ", channels);
    }
#undef __LAUNCH_KERNEL__

    if constexpr (GEO)
        return std::make_tuple(renders, alphas, last_ids, expected_depths, median_depths, expected_normals, median_ids);
    else
        return std::make_tuple(renders, alphas, last_ids);
}

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
) {
    return call_kernel_with_dim_3dgs_fwd<std::tuple<at::Tensor, at::Tensor, at::Tensor>>(
        means2d,
        conics,
        colors,
        opacities,
        c10::nullopt,
        c10::nullopt,
        c10::nullopt,
        c10::nullopt,
        backgrounds,
        masks,
        image_width,
        image_height,
        tile_size,
        tile_offsets,
        flatten_ids,
        c10::nullopt,
        c10::nullopt,
        false
    );
}

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
) {
    DEVICE_GUARD(means2d);
    CHECK_INPUT(means2d);
    CHECK_INPUT(conics);
    CHECK_INPUT(colors);
    CHECK_INPUT(opacities);
    CHECK_INPUT(tile_offsets);
    CHECK_INPUT(flatten_ids);
    CHECK_INPUT(render_alphas);
    CHECK_INPUT(last_ids);
    CHECK_INPUT(v_render_colors);
    CHECK_INPUT(v_render_alphas);
    if (backgrounds.has_value()) {
        CHECK_INPUT(backgrounds.value());
    }
    if (masks.has_value()) {
        CHECK_INPUT(masks.value());
    }

    uint32_t channels = colors.size(-1);

    at::Tensor v_means2d = at::zeros_like(means2d);
    at::Tensor v_conics = at::zeros_like(conics);
    at::Tensor v_colors = at::zeros_like(colors);
    at::Tensor v_opacities = at::zeros_like(opacities);
    at::Tensor v_means2d_abs;
    if (absgrad) {
        v_means2d_abs = at::zeros_like(means2d);
    }

#define __LAUNCH_KERNEL__(N)                                                   \
    case N:                                                                    \
        launch_rasterize_to_pixels_3dgs_bwd_kernel<N,false>(                         \
            means2d,                                                           \
            conics,                                                            \
            colors,                                                            \
            opacities,                                                         \
            c10::nullopt,                                                      \
            c10::nullopt,                                                      \
            c10::nullopt,                                                      \
            c10::nullopt,                                                      \
            backgrounds,                                                       \
            masks,                                                             \
            image_width,                                                       \
            image_height,                                                      \
            tile_size,                                                         \
            tile_offsets,                                                      \
            flatten_ids,                                                       \
            render_alphas,                                                     \
            last_ids,                                                          \
            c10::nullopt,                                                      \
            v_render_colors,                                                   \
            v_render_alphas,                                                   \
            c10::nullopt,                                                       \
            c10::nullopt,                                                       \
            c10::nullopt,                                                       \
            absgrad ? c10::optional<at::Tensor>(v_means2d_abs) : c10::nullopt, \
            v_means2d,                                                         \
            v_conics,                                                          \
            v_colors,                                                          \
            v_opacities,                                                        \
            c10::nullopt,                                                       \
            c10::nullopt,                                                       \
            c10::nullopt                                                       \
        );                                                                     \
        break;

    // TODO: an optimization can be done by passing the actual number of
    // channels into the kernel functions and avoid necessary global memory
    // writes. This requires moving the channel padding from python to C side.
    switch (channels) {
        __LAUNCH_KERNEL__(1)
        __LAUNCH_KERNEL__(2)
        __LAUNCH_KERNEL__(3)
        __LAUNCH_KERNEL__(4)
        // __LAUNCH_KERNEL__(5)
        // __LAUNCH_KERNEL__(8)
        // __LAUNCH_KERNEL__(9)
        // __LAUNCH_KERNEL__(16)
        // __LAUNCH_KERNEL__(17)
        // __LAUNCH_KERNEL__(32)
        // __LAUNCH_KERNEL__(33)
        // __LAUNCH_KERNEL__(64)
        // __LAUNCH_KERNEL__(65)
        // __LAUNCH_KERNEL__(128)
        // __LAUNCH_KERNEL__(129)
        // __LAUNCH_KERNEL__(256)
        // __LAUNCH_KERNEL__(257)
        // __LAUNCH_KERNEL__(512)
        // __LAUNCH_KERNEL__(513)
    default:
        AT_ERROR("Unsupported number of channels: ", channels);
    }
#undef __LAUNCH_KERNEL__

    return std::make_tuple(
        v_means2d_abs, v_means2d, v_conics, v_colors, v_opacities
    );
}

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
){
     return call_kernel_with_dim_3dgs_fwd<std::tuple<at::Tensor, at::Tensor, at::Tensor, at::Tensor, at::Tensor, at::Tensor, at::Tensor>>
         (means2d, conics, colors, opacities, ray_ts, ray_planes, normals, Ks,
                                        backgrounds, masks, image_width, image_height, tile_size, 
                                        tile_offsets, flatten_ids,c10::nullopt,c10::nullopt,false);
}


std::tuple<at::Tensor, at::Tensor, at::Tensor, at::Tensor, at::Tensor, at::Tensor, at::Tensor, at::Tensor>
rasterize_to_pixels_3dgs_w_depth_bwd(
    // Gaussian parameters
    const at::Tensor& means2d,                   // [C, N, 2] or [nnz, 2]
    const at::Tensor& conics,                    // [C, N, 3] or [nnz, 3]
    const at::Tensor& colors,                    // [C, N, 3] or [nnz, 3]
    const at::Tensor& opacities,                 // [C, N] or [nnz]
    const at::Tensor& ray_ts,                    // [C, N] or [nnz]
    const at::Tensor& ray_planes,                // [C, N, 2] or [nnz, 2]
    const at::Tensor& normals,                   // [C, N, 3] or [nnz, 3]
    const at::optional<at::Tensor>& backgrounds, // [C, 3]
    const at::optional<at::Tensor>& masks,       // [C, tile_height, tile_width]
    // image size
    const uint32_t image_width, const uint32_t image_height, const uint32_t tile_size,
    const at::Tensor& Ks,
    // intersections
    const at::Tensor& tile_offsets, // [C, tile_height, tile_width]
    const at::Tensor& flatten_ids,  // [n_isects]
    // forward outputs
    const at::Tensor& render_alphas, // [C, image_height, image_width, 1]
    const at::Tensor& last_ids,      // [C, image_height, image_width]
    const at::Tensor& median_ids,      // [C, image_height, image_width]
    // gradients of outputs
    const at::Tensor& v_render_colors, // [C, image_height, image_width, 3]
    const at::Tensor& v_render_alphas, // [C, image_height, image_width, 1]
    const at::Tensor& v_render_expected_depths, // [C, image_height, image_width, 1]
    const at::Tensor& v_render_median_depths, // [C, image_height, image_width, 1]
    const at::Tensor& v_render_expected_normals, // [C, image_height, image_width, 3]
    // options
    bool absgrad) {
    DEVICE_GUARD(means2d);
    CHECK_INPUT(means2d);
    CHECK_INPUT(conics);
    CHECK_INPUT(colors);
    CHECK_INPUT(opacities);
    CHECK_INPUT(tile_offsets);
    CHECK_INPUT(flatten_ids);
    CHECK_INPUT(render_alphas);
    CHECK_INPUT(last_ids);
    CHECK_INPUT(v_render_colors);
    CHECK_INPUT(v_render_alphas);
    if (backgrounds.has_value()) {
        CHECK_INPUT(backgrounds.value());
    }
    if (masks.has_value()) {
        CHECK_INPUT(masks.value());
    }

    uint32_t channels = colors.size(-1);

    at::Tensor v_means2d = at::zeros_like(means2d);
    at::Tensor v_conics = at::zeros_like(conics);
    at::Tensor v_colors = at::zeros_like(colors);
    at::Tensor v_opacities = at::zeros_like(opacities);
    at::Tensor v_means2d_abs;
    if (absgrad) {
        v_means2d_abs = at::zeros_like(means2d);
    }
    at::Tensor v_ray_ts = at::zeros_like(ray_ts);
    at::Tensor v_ray_planes = at::zeros_like(ray_planes);
    at::Tensor v_normals = at::zeros_like(normals) ;
#define __LAUNCH_KERNEL__(N)                                                   \
    case N:                                                                    \
        launch_rasterize_to_pixels_3dgs_bwd_kernel<N,true>(                         \
            means2d,                                                           \
            conics,                                                            \
            colors,                                                            \
            opacities,                                                         \
            ray_ts,                                                      \
            ray_planes,                                                      \
            normals,                                                      \
            Ks,                                                      \
            backgrounds,                                                       \
            masks,                                                             \
            image_width,                                                       \
            image_height,                                                      \
            tile_size,                                                         \
            tile_offsets,                                                      \
            flatten_ids,                                                       \
            render_alphas,                                                     \
            last_ids,                                                          \
            median_ids,                                                      \
            v_render_colors,                                                   \
            v_render_alphas,                                                   \
            v_render_expected_depths,                                                       \
            v_render_median_depths,                                                       \
            v_render_expected_normals,                                                       \
            absgrad ? c10::optional<at::Tensor>(v_means2d_abs) : c10::nullopt, \
            v_means2d,                                                         \
            v_conics,                                                          \
            v_colors,                                                          \
            v_opacities,                                                        \
            v_ray_ts,                                                       \
            v_ray_planes,                                                       \
            v_normals                                                       \
        );                                                                     \
        break;

    // TODO: an optimization can be done by passing the actual number of
    // channels into the kernel functions and avoid necessary global memory
    // writes. This requires moving the channel padding from python to C side.
    switch (channels) {
        __LAUNCH_KERNEL__(1)
            __LAUNCH_KERNEL__(2)
            __LAUNCH_KERNEL__(3)
            __LAUNCH_KERNEL__(4)
            // __LAUNCH_KERNEL__(5)
            // __LAUNCH_KERNEL__(8)
            // __LAUNCH_KERNEL__(9)
            // __LAUNCH_KERNEL__(16)
            // __LAUNCH_KERNEL__(17)
            // __LAUNCH_KERNEL__(32)
            // __LAUNCH_KERNEL__(33)
            // __LAUNCH_KERNEL__(64)
            // __LAUNCH_KERNEL__(65)
            // __LAUNCH_KERNEL__(128)
            // __LAUNCH_KERNEL__(129)
            // __LAUNCH_KERNEL__(256)
            // __LAUNCH_KERNEL__(257)
            // __LAUNCH_KERNEL__(512)
            // __LAUNCH_KERNEL__(513)
    default:
        AT_ERROR("Unsupported number of channels: ", channels);
    }
#undef __LAUNCH_KERNEL__

    return std::make_tuple(
        v_means2d_abs, v_means2d, v_conics, v_colors, v_opacities,v_ray_ts,v_ray_planes,v_normals
    );
}


std::tuple<at::Tensor, at::Tensor, at::Tensor, at::Tensor> rasterize_3dsplats_count_fwd(
    // Gaussian parameters
    const at::Tensor &means2d,   // [C, N, 2] or [nnz, 2]
    const at::Tensor &conics,    // [C, N, 3] or [nnz, 3]
    const at::Tensor &colors,    // [C, N, channels] or [nnz, channels]
    const at::Tensor &opacities, // [C, N]  or [nnz]
    const at::optional<at::Tensor> &backgrounds, // [C, channels]
    const at::optional<at::Tensor> &masks,       // [C, tile_height, tile_width]
    // image size
    const uint32_t image_width, const uint32_t image_height, const uint32_t tile_size,
    // intersections
    const at::Tensor &tile_offsets, // [C, tile_height, tile_width]
    const at::Tensor &flatten_ids,   // [n_isects]
    const bool Count
) {
    bool packed = means2d.dim() == 2;
    uint32_t C = tile_offsets.size(0); // number of cameras
    uint32_t N = packed ? 0 : means2d.size(1); // number of gaussians

    at::Tensor touchedPixels;
    at::Tensor splatT;
    if( !packed ){
        touchedPixels = at::zeros({ C * N, 1},
                means2d.options().dtype(at::kInt));
        splatT = at::zeros({ C * N, 1},
                means2d.options().dtype(at::kFloat));
    }else{
        touchedPixels = at::zeros({means2d.size(0), 1},
                means2d.options().dtype(at::kInt));
        splatT = at::zeros({means2d.size(0), 1},
                means2d.options().dtype(at::kFloat));
    }
    auto [renders, alphas,lastId] = call_kernel_with_dim_3dgs_fwd<std::tuple<at::Tensor, at::Tensor, at::Tensor>>(
        means2d,
        conics,
        colors,
        opacities,
        c10::nullopt,
        c10::nullopt,
        c10::nullopt,
        c10::nullopt,
        backgrounds,
        masks,
        image_width,
        image_height,
        tile_size,
        tile_offsets,
        flatten_ids,
        touchedPixels,
        splatT,
        Count
    );
    return std::make_tuple(renders, alphas, touchedPixels, splatT);
}


} // namespace gsplat
