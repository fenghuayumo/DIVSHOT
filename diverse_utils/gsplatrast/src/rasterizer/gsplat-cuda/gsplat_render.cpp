#include <ATen/TensorUtils.h>
#include <ATen/core/Tensor.h>
#include <ATen/ATen.h>
#include <c10/cuda/CUDAGuard.h> // for DEVICE_GUARD
#include <tuple>

#include <ATen/Functions.h>
#include <ATen/NativeFunctions.h>

#include "common.h"     // where all the macros are defined
#include "ops.h"        // a collection of all gsplat operators

namespace gsplat {

std::tuple<at::Tensor, at::Tensor, at::Tensor, at::Tensor, at::Tensor> render_3dsplats_count_fwd(
    const at::Tensor &means,                   // [C, N, 3]
    const at::Tensor &scales,                    // [C, N, 3]
    const at::Tensor &quats,                    // [C, N, 4]
    const at::Tensor &opacities,                 // [N]
    int degrees_to_use,
    const at::Tensor &sh0_coeffs,                 // [N,1,3]
    const at::Tensor &shN_coeffs,               // [N,15,3]
    const at::Tensor &T,
    const at::Tensor &viewMat,
    const at::Tensor &ksMat,
    const at::optional<at::Tensor> &backgrounds, // [C, 3]
    const uint32_t image_width, 
    const uint32_t image_height,
    float eps2d,
    float nearPlane,
    float farPlane,
    float radiusClip,
    CameraModelType camera_model,
    bool  splatCount
){
    auto [radii, means2d, depths, conics, compensations, t_opacities,ray_ts,ray_planes,normals] = projection_ewa_3dgs_fused_fwd(
                                                            means, 
                                                            {}, 
                                                            quats, 
                                                            scales, 
                                                            opacities,
                                                            viewMat.contiguous(),
                                                            ksMat.contiguous(),
                                                            image_width, 
                                                            image_height,
                                                            eps2d,
                                                            nearPlane, 
                                                            farPlane, 
                                                            radiusClip,
                                                            false,
                                                            camera_model,
                                                            false);

    at::Tensor viewDirs = means.detach() - T.transpose(0, 1).to(at::kCUDA);
    at::Tensor masks = (radii > 0).all(-1).squeeze(0);
    auto rgbs = spherical_harmonics_fwd(degrees_to_use, viewDirs.contiguous(), sh0_coeffs.contiguous(), shN_coeffs.contiguous(), masks.contiguous());
    rgbs = rgbs.unsqueeze(0);
    rgbs = at::clamp_min(rgbs + 0.5f, 0.0f);
    const auto C = 1;
    const auto tileSize = 16;
    const int tile_width = static_cast<int>(std::ceil(image_width / float(tileSize)));
    const int tile_height = static_cast<int>(std::ceil(image_height / float(tileSize)));
    auto [tiles_per_gauss, isect_ids, flatten_ids] = intersect_tile(
        means2d,
        radii,
        depths,
        {},
        {},
        conics,
        t_opacities,
        C,
        tileSize,
        tile_width,
        tile_height,
        true
    );
    at::Tensor isect_offsets = intersect_offset(isect_ids, C, tile_width, tile_height); // [C, tile_height, tile_width]
    auto [rgb,renderAlpha,touchPixels,splatT] = rasterize_3dsplats_count_fwd(
            means2d,
            conics,
            rgbs,
            t_opacities,
            backgrounds,
            {},
            image_width,
            image_height,
            tileSize,
            isect_offsets,
            flatten_ids,
            splatCount
        );
    return {rgb, renderAlpha, radii, touchPixels, splatT};
}

}