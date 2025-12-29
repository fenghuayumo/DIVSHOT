#include <ATen/TensorUtils.h>
#include <ATen/core/Tensor.h>
#include <c10/cuda/CUDAGuard.h> // for DEVICE_GUARD
#include <tuple>

#include <ATen/Functions.h>
#include <ATen/NativeFunctions.h>

#include "common.h"
#include "ops.h"
#include "projection.h"
// #include "intersect.h"
#include "spherical_harmonics.h"
#include "rasterization.h"

namespace gsplat {

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
    at::Tensor //depths
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
){
    const uint32_t C = viewmats.size(0);
    at::Tensor radii, means2d, depths, conics,t_opacities,ray_ts,ray_planes,normals;
    at::Tensor compensations;
    at::Tensor camera_ids, gaussian_ids;
    if(!packed) {
        std::tie(radii, means2d, depths, conics, compensations, t_opacities, ray_ts,ray_planes,normals) = projection_ewa_3dgs_fused_fwd(
            means,
            {},
            quats,
            scales,
            opacities,
            viewmats,
            Ks,
            image_width,
            image_height,
            eps2d,
            near_plane,
            far_plane,
            radius_clip,
            calc_compensations,
            camera_model,
            calc_gof);
    }else{
        at::Tensor indptr;
        std::tie(
            indptr,
            camera_ids,
            gaussian_ids,
            radii,
            means2d,
            depths,
            conics,
            compensations,
            t_opacities,
            ray_ts,
            ray_planes, 
            normals) = projection_ewa_3dgs_packed_fwd(
            means,
            {},
            quats,
            scales,
            opacities,
            viewmats,
            Ks,
            image_width,
            image_height,
            eps2d,
            near_plane,
            far_plane,
            radius_clip,
            calc_compensations,
            camera_model,
            calc_gof
        );
    }
    at::Tensor sh_masks,view_dirs;
    at::Tensor shs_0_indexed, shs_n_indexed;
    if(packed) {
        view_dirs = means.detach().index_select(0, gaussian_ids).sub(T);
        sh_masks = (radii.greater(0)).all(-1); //[nnz]
        shs_0_indexed = shs_0.index_select(0, gaussian_ids);  // [nnz, 1, 3]
        shs_n_indexed = shs_n.index_select(0, gaussian_ids);  // [nnz, K-1, 3]
    }else{
        view_dirs = means.detach().sub(T);
        sh_masks = (radii.greater(0)).all(-1).squeeze(0);
        shs_0_indexed = shs_0;
        shs_n_indexed = shs_n;
    }
    auto colors = spherical_harmonics_fwd(
        degrees_to_use, 
        view_dirs.contiguous(), 
        shs_0_indexed.contiguous(), 
        shs_n_indexed.contiguous(), 
        sh_masks.contiguous());
    // colors = at::clamp_min(colors.add(0.5), 0.0);
    colors.add_(0.5).clamp_min_(0.0);
    const auto tile_width = std::ceil(image_width / float(tile_size));
    const auto tile_height = std::ceil(image_height / float(tile_size));
    auto [tiles_per_gauss, isect_ids, flatten_ids] = intersect_tile(
        means2d.contiguous(),
        radii.contiguous(),
        depths.contiguous(),
        packed ? at::optional<at::Tensor>(camera_ids) : c10::nullopt,
        packed ? at::optional<at::Tensor>(gaussian_ids) : c10::nullopt,
        conics.contiguous(),
        t_opacities.contiguous(),
        C,
        tile_size,
        tile_width,
        tile_height,
        true
    );
    auto isect_offsets = intersect_offset(isect_ids, C, tile_width, tile_height);
    at::Tensor render_colors, render_alphas, render_e_depths, render_m_depths, render_e_normals, last_ids, median_ids;
    if( calc_gof ){
        std::tie(render_colors, render_alphas, last_ids,render_e_depths, render_m_depths, render_e_normals,median_ids) = rasterize_to_pixels_3dgs_w_depth_fwd(
            means2d.contiguous(),
            conics.contiguous(),
            colors.contiguous(),
            t_opacities.contiguous(),
            ray_ts.contiguous(),
            ray_planes.contiguous(),
            normals.contiguous(),
            backgrounds,
            {},
            image_width,
            image_height,
            tile_size,
            Ks,
            isect_offsets.contiguous(),
            flatten_ids.contiguous()
        );
    }else{
        std::tie(render_colors, render_alphas, last_ids) = rasterize_to_pixels_3dgs_fwd(
            means2d.contiguous(),
            conics.contiguous(),
            colors.contiguous(),
            t_opacities.contiguous(),
            backgrounds,
            {},
            image_width,
            image_height,
            tile_size,
            isect_offsets.contiguous(),
            flatten_ids.contiguous()
        );
    }
    return {
        render_colors, 
        render_alphas, 
        render_e_depths, 
        render_m_depths, 
        render_e_normals, 
        last_ids, 
        median_ids, 
        ray_ts, 
        ray_planes, 
        normals, 
        means2d, 
        conics, 
        compensations,
        radii,
        colors, 
        t_opacities, 
        isect_offsets, 
        flatten_ids,
        camera_ids,
        gaussian_ids,
        sh_masks,
        view_dirs,
        shs_0_indexed,
        shs_n_indexed,
        depths
    };
}


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
    const bool absgrad){
       
        at::Tensor v_means2d_abs, v_means2d, v_conics, v_colors, v_t_opacities, v_ray_ts, v_ray_planes, v_normals;
        if(calc_gof) {
           std::tie(v_means2d_abs, v_means2d, v_conics, v_colors, v_t_opacities, v_ray_ts, v_ray_planes, v_normals) = rasterize_to_pixels_3dgs_w_depth_bwd(
            means2d.contiguous(),
            conics.contiguous(),
            colors.contiguous(),
            t_opacities.contiguous(),
            ray_ts.value().contiguous(),
            ray_planes.value().contiguous(),
            normals.value().contiguous(),
            backgrounds,
            {},
            image_width,
            image_height,
            tile_size,
            Ks.contiguous(),
            tile_offsets.contiguous(),
            flatten_ids.contiguous(),
            render_alphas.contiguous(),
            last_ids.contiguous(),
            median_ids.value().contiguous(),
            v_render_colors.contiguous(),
            v_render_alphas.contiguous(),
            v_render_expected_depths.value().contiguous(),
            v_render_median_depths.value().contiguous(),
            v_render_expected_normals.value().contiguous(),
            absgrad
           );
       }else{
            std::tie(v_means2d_abs, v_means2d, v_conics, v_colors, v_t_opacities) = rasterize_to_pixels_3dgs_bwd(
                means2d.contiguous(),
                conics.contiguous(),
                colors.contiguous(),
                t_opacities.contiguous(),
                backgrounds,
                {},
                image_width,
                image_height,
                tile_size,
                tile_offsets.contiguous(),
                flatten_ids.contiguous(),
                render_alphas.contiguous(),
                last_ids.contiguous(),
                v_render_colors.contiguous(),
                v_render_alphas.contiguous(),
                absgrad
            );
       }
       const auto num_bases = shs_0_indexed.size(-2) + shs_n_indexed.size(-2);
       
       // Handle clamp_min gradient: colors_out = clamp_min(colors_sh + 0.5, 0.0)
       // Gradient of clamp_min is 0 where the value was clamped (colors_out == 0)
       // and 1 otherwise (colors_out > 0). Since we only have colors_out (clamped),
       // we approximate: don't propagate gradient where colors == 0.
       auto clamp_mask = colors.gt(0).to(v_colors.dtype());
       v_colors = v_colors * clamp_mask;
       
       auto [v_shs_0, v_shs_n, v_viewDirs] = spherical_harmonics_bwd(
            num_bases,
            degrees_to_use,
            viewDirs.contiguous(),
            shs_n_indexed.contiguous(),
            sh_masks,
            v_colors.contiguous(),
            false
        );
       at::Tensor v_means, v_quats, v_scales, v_opacities, v_viewmats,v_covars;
    //    auto v_depths = at::zeros_like(t_opacities);
       if(packed){
           auto v_shs_0_ = at::zeros({means.size(0), shs_0_indexed.size(-2), shs_0_indexed.size(-1)}, means.options());
           auto v_shs_n_ = at::zeros({means.size(0), shs_n_indexed.size(-2), shs_n_indexed.size(-1)}, means.options());
           v_shs_0_.index_add_(0, gaussian_ids.value(), v_shs_0);
           v_shs_n_.index_add_(0, gaussian_ids.value(), v_shs_n);
           v_shs_0 = v_shs_0_;
           v_shs_n = v_shs_n_;
            std::tie(v_means, v_covars, v_quats, v_scales, v_opacities,v_viewmats) = projection_ewa_3dgs_packed_bwd(
                means.contiguous(),
                {},
                quats.contiguous(),
                scales.contiguous(),
                opacities.contiguous(),
                viewmats.contiguous(),
                Ks.contiguous(),
                image_width,
                image_height,
                eps2d,
                camera_model,
                camera_ids.value().contiguous(),
                gaussian_ids.value().contiguous(),
                conics.contiguous(),
                calc_compensations ? compensations : c10::nullopt,
                v_means2d.contiguous(),
                c10::nullopt,
                v_conics.contiguous(),
                {},
                v_t_opacities.contiguous(),
                calc_gof ? at::optional<at::Tensor>(v_ray_ts) : c10::nullopt,
                calc_gof ? at::optional<at::Tensor>(v_ray_planes) : c10::nullopt,
                calc_gof ? at::optional<at::Tensor>(v_normals) : c10::nullopt,
                false,
                sparse_grad
            );
       }else{
        std::tie(v_means, v_covars, v_quats, v_scales, v_opacities,v_viewmats) = projection_ewa_3dgs_fused_bwd(
            means.contiguous(),
            {},
            quats.contiguous(),
            scales.contiguous(),
            opacities.contiguous(),
            viewmats.contiguous(),
            Ks.contiguous(),
            image_width,
            image_height,
            eps2d,
            camera_model,
            radii.contiguous(),
            conics.contiguous(),
            calc_compensations ? compensations : c10::nullopt,
            v_means2d.contiguous(),
            c10::nullopt,
            v_conics.contiguous(),
            {},
            v_t_opacities.contiguous(),
            calc_gof ? at::optional<at::Tensor>(v_ray_ts) : c10::nullopt,
            calc_gof ? at::optional<at::Tensor>(v_ray_planes) : c10::nullopt,
            calc_gof ? at::optional<at::Tensor>(v_normals) : c10::nullopt,
            false
        );
    }
    return {v_means2d_abs,v_means2d, v_means, v_quats, v_scales, v_opacities, v_shs_0, v_shs_n};
}

}