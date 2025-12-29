#include <ATen/TensorUtils.h>
#include <ATen/core/Tensor.h>
#include <c10/cuda/CUDAGuard.h> // for DEVICE_GUARD
#include <tuple>

#include <ATen/Functions.h>
#include <ATen/NativeFunctions.h>

#include "common.h"     // where all the macros are defined
#include "ops.h"        // a collection of all gsplat operators
#include "projection.h" // where the launch function is declared
#include "cameras.h"

namespace gsplat {

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
) {
    DEVICE_GUARD(means);
    CHECK_INPUT(means);
    if (covars.has_value()) {
        CHECK_INPUT(covars.value());
    } else {
        assert(quats.has_value() && scales.has_value());
        CHECK_INPUT(quats.value());
        CHECK_INPUT(scales.value());
    }
    CHECK_INPUT(viewmats);
    CHECK_INPUT(Ks);

    uint32_t N = means.size(0);    // number of gaussians
    uint32_t C = viewmats.size(0); // number of cameras

    at::Tensor radii = at::empty({C, N, 2}, means.options().dtype(at::kInt));
    at::Tensor means2d = at::empty({C, N, 2}, means.options());
    at::Tensor depths = at::empty({C, N}, means.options());
    at::Tensor conics = at::empty({C, N, 3}, means.options());
    at::Tensor compensations;
    if (calc_compensations) {
        // we dont want NaN to appear in this tensor, so we zero intialize it
        compensations = at::zeros({C, N}, means.options());
    }
    at::Tensor ray_ts, ray_planes, normals;
    if (calc_gof) {
        ray_ts = at::empty({C, N,1}, means.options());
        ray_planes = at::empty({C, N, 2}, means.options());
        normals = at::empty({C, N, 3}, means.options());
    }
    at::Tensor t_opacities = at::empty({C,N}, opacities.options());
    launch_projection_ewa_3dgs_fused_fwd_kernel(
        // inputs
        means,
        covars,
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
        camera_model,
        // outputs
        radii,
        means2d,
        depths,
        conics,
        calc_compensations ? at::optional<at::Tensor>(compensations)
                           : c10::nullopt,
        t_opacities,
        calc_gof ? at::optional<at::Tensor>(ray_ts) : c10::nullopt,
        calc_gof ? at::optional<at::Tensor>(ray_planes) : c10::nullopt,
        calc_gof ? at::optional<at::Tensor>(normals) : c10::nullopt
    );
    return std::make_tuple(radii, means2d, depths, conics, compensations, t_opacities, ray_ts,ray_planes,normals);
}

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
) {
    DEVICE_GUARD(means);
    CHECK_INPUT(means);
    if (covars.has_value()) {
        CHECK_INPUT(covars.value());
    } else {
        assert(quats.has_value() && scales.has_value());
        CHECK_INPUT(quats.value());
        CHECK_INPUT(scales.value());
    }
    CHECK_INPUT(viewmats);
    CHECK_INPUT(Ks);
    CHECK_INPUT(radii);
    CHECK_INPUT(conics);
    CHECK_INPUT(v_means2d);
    if (v_depths.has_value()) {
        CHECK_INPUT(v_depths.value());
    }
    CHECK_INPUT(v_conics);
    if (compensations.has_value()) {
        CHECK_INPUT(compensations.value());
    }
    if (v_compensations.has_value()) {
        CHECK_INPUT(v_compensations.value());
        assert(compensations.has_value());
    }
    if(v_ray_ts.has_value()) {
        CHECK_INPUT(v_ray_ts.value());
        CHECK_INPUT(v_ray_planes.value());
        CHECK_INPUT(v_normals.value());
    }
    at::Tensor v_means = at::zeros_like(means);
    at::Tensor v_covars, v_quats, v_scales; // optional
    if (covars.has_value()) {
        v_covars = at::zeros_like(covars.value());
    } else {
        v_quats = at::zeros_like(quats.value());
        v_scales = at::zeros_like(scales.value());
    }
    at::Tensor v_viewmats;
    if (viewmats_requires_grad) {
        v_viewmats = at::zeros_like(viewmats);
    }
    at::Tensor v_opacities = at::zeros_like(opacities);

    launch_projection_ewa_3dgs_fused_bwd_kernel(
        // inputs
        means,
        covars,
        quats,
        scales,
        opacities,
        viewmats,
        Ks,
        image_width,
        image_height,
        eps2d,
        camera_model,
        radii,
        conics,
        compensations,
        v_means2d,
        v_depths,
        v_conics,
        v_compensations,
        v_t_opacities,
        v_ray_ts,
        v_ray_planes,
        v_normals,
        viewmats_requires_grad,
        // outputs
        v_means,
        v_covars,
        v_quats,
        v_scales,
        v_opacities,
        v_viewmats
    );

    return std::make_tuple(v_means, v_covars, v_quats, v_scales, v_opacities, v_viewmats);
}

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
){
    DEVICE_GUARD(means);
    CHECK_INPUT(means);
    if (covars.has_value()) {
        CHECK_INPUT(covars.value());
    } else {
        assert(quats.has_value() && scales.has_value());
        CHECK_INPUT(quats.value());
        CHECK_INPUT(scales.value());
    }
    CHECK_INPUT(viewmats);
    CHECK_INPUT(Ks);

    uint32_t N = means.size(0);    // number of gaussians
    uint32_t C = viewmats.size(0); // number of cameras
    auto opt = means.options();

    uint32_t nrows = C;
    uint32_t ncols = N;
    uint32_t blocks_per_row = (ncols + N_THREADS_PACKED - 1) / N_THREADS_PACKED;

    // first pass
    int32_t nnz;
    at::Tensor block_accum;
    if (C && N) {
        at::Tensor block_cnts =
            at::empty({nrows * blocks_per_row}, opt.dtype(at::kInt));
        launch_projection_ewa_3dgs_packed_fwd_kernel(
            // inputs
            means,
            covars,
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
            c10::nullopt, // block_accum
            camera_model,
            // outputs
            block_cnts,
            c10::nullopt, // indptr
            c10::nullopt, // camera_ids
            c10::nullopt, // gaussian_ids
            c10::nullopt, // radii
            c10::nullopt, // means2d
            c10::nullopt, // depths
            c10::nullopt, // conics
            // pass in as an indicator on whether compensation will be applied or not.
            calc_compensations ? at::optional<at::Tensor>(at::empty({1}, opt))
                               : c10::nullopt,
            c10::nullopt,
            c10::nullopt,
            c10::nullopt,
            c10::nullopt
        );
        block_accum = at::cumsum(block_cnts, 0, at::kInt);
        nnz = block_accum[-1].item<int32_t>();
    } else {
        nnz = 0;
    }

    // second pass
    at::Tensor indptr = at::empty({C + 1}, opt.dtype(at::kInt));
    at::Tensor camera_ids = at::empty({nnz}, opt.dtype(at::kLong));
    at::Tensor gaussian_ids = at::empty({nnz}, opt.dtype(at::kLong));
    at::Tensor radii = at::empty({nnz, 2}, opt.dtype(at::kInt));
    at::Tensor means2d = at::empty({nnz, 2}, opt);
    at::Tensor depths = at::empty({nnz}, opt);
    at::Tensor conics = at::empty({nnz, 3}, opt);
    at::Tensor compensations;
    if (calc_compensations) {
        // we dont want NaN to appear in this tensor, so we zero intialize it
        compensations = at::zeros({nnz}, opt);
    }
    at::Tensor ray_ts, ray_planes, normals;
    if (calc_gof) {
        ray_ts = at::empty({nnz,1}, means.options());
        ray_planes = at::empty({nnz, 2}, means.options());
        normals = at::empty({nnz, 3}, means.options());
    }
    at::Tensor t_opacities = at::zeros({nnz}, opt);
    if (nnz) {
        launch_projection_ewa_3dgs_packed_fwd_kernel(
            // inputs
            means,
            covars,
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
            block_accum,
            camera_model,
            // outputs
            c10::nullopt, // block_cnts
            indptr,
            camera_ids,
            gaussian_ids,
            radii,
            means2d,
            depths,
            conics,
            calc_compensations ? at::optional<at::Tensor>(compensations)
                               : c10::nullopt,
            t_opacities,
            calc_gof ? at::optional<at::Tensor>(ray_ts) : c10::nullopt,
            calc_gof ? at::optional<at::Tensor>(ray_planes) : c10::nullopt,
            calc_gof ? at::optional<at::Tensor>(normals) : c10::nullopt
        );
    } else {
        indptr.fill_(0);
    }

    return std::make_tuple(
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
        normals
    );
}
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
) {
    DEVICE_GUARD(means);
    CHECK_INPUT(means);
    if (covars.has_value()) {
        CHECK_INPUT(covars.value());
    } else {
        assert(quats.has_value() && scales.has_value());
        CHECK_INPUT(quats.value());
        CHECK_INPUT(scales.value());
    }
    CHECK_INPUT(viewmats);
    CHECK_INPUT(Ks);
    CHECK_INPUT(camera_ids);
    CHECK_INPUT(gaussian_ids);
    CHECK_INPUT(conics);
    CHECK_INPUT(v_means2d);
    if (v_depths.has_value()) {
        CHECK_INPUT(v_depths.value());
    }
    CHECK_INPUT(v_conics);
    if (compensations.has_value()) {
        CHECK_INPUT(compensations.value());
    }
    if (v_compensations.has_value()) {
        CHECK_INPUT(v_compensations.value());
        assert(compensations.has_value());
    }
    if(v_ray_ts.has_value()){
        CHECK_INPUT(v_ray_ts.value());
        CHECK_INPUT(v_ray_planes.value());
        CHECK_INPUT(v_normals.value());
    }
    uint32_t N = means.size(0);    // number of gaussians
    uint32_t C = viewmats.size(0); // number of cameras
    uint32_t nnz = camera_ids.size(0);

    at::Tensor v_means, v_covars, v_quats, v_scales, v_opacities,v_viewmats;
    if (sparse_grad) {
        v_means = at::zeros({nnz, 3}, means.options());
        if (covars.has_value()) {
            v_covars = at::zeros({nnz, 6}, covars.value().options());
        } else {
            v_quats = at::zeros({nnz, 4}, quats.value().options());
            v_scales = at::zeros({nnz, 3}, scales.value().options());
        }
        if (viewmats_requires_grad) {
            v_viewmats = at::zeros({C, 4, 4}, viewmats.options());
        }
        v_opacities = at::zeros({nnz}, opacities.options());
    } else {
        v_means = at::zeros_like(means);
        if (covars.has_value()) {
            v_covars = at::zeros_like(covars.value());
        } else {
            v_quats = at::zeros_like(quats.value());
            v_scales = at::zeros_like(scales.value());
        }
        if (viewmats_requires_grad) {
            v_viewmats = at::zeros_like(viewmats);
        }
        v_opacities = at::zeros_like(opacities);
    }

    launch_projection_ewa_3dgs_packed_bwd_kernel(
        // fwd inputs
        means,
        covars,
        quats,
        scales,
        opacities,
        viewmats,
        Ks,
        image_width,
        image_height,
        eps2d,
        camera_model,
        // fwd outputs
        camera_ids,
        gaussian_ids,
        conics,
        compensations,
        // grad outputs
        v_means2d,
        v_depths,
        v_conics,
        v_compensations,
        v_t_opacities,
        v_ray_ts,
        v_ray_planes,
        v_normals,
        sparse_grad,
        // outputs
        v_means,
        v_covars.defined() ? at::optional<at::Tensor>(v_covars) : c10::nullopt,
        v_quats.defined() ? at::optional<at::Tensor>(v_quats) : c10::nullopt,
        v_scales.defined() ? at::optional<at::Tensor>(v_scales) : c10::nullopt,
        v_opacities,
        v_viewmats.defined() ? at::optional<at::Tensor>(v_viewmats)
                             : c10::nullopt
    );
    return std::make_tuple(v_means, v_covars, v_quats, v_scales, v_opacities, v_viewmats);
}

} // namespace gsplat
