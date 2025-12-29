#include "fused_rasterize_gaussians.hpp"
#include "rasterize_gaussians.hpp"

#if defined(USE_HIP) || defined(USE_CUDA) || defined(USE_MPS)

variable_list FusedRasterizeGaussians::forward(AutogradContext *ctx, 
                torch::Tensor means,
                torch::Tensor scales,
                torch::Tensor quats,
                torch::Tensor opacities,
                torch::Tensor shs_0,
                torch::Tensor shs_n,
                torch::Tensor viewMat,
                torch::Tensor ksMat,
                torch::Tensor T,
                torch::Tensor backgrounds,
                int degrees_to_use,
                FusedRasterizationSettings settings
            ){
    // Extract settings
    const int image_width = settings.width;
    const int image_height = settings.height;
    const int tile_size = settings.tile_size;
    const float eps2d = settings.eps2d;
    const float nearPlane = settings.near_plane;
    const float farPlane = settings.far_plane;
    const float radiusClip = settings.radius_clip;
    const bool calc_compensations = settings.calc_compensations;
    const gsplat::CameraModelType camera_model = settings.camera_model;
    const bool packed = settings.packed;
    const bool calc_gof = settings.calc_gof;
    const bool sparse_grad = settings.sparse_grad;
    const bool absgrad = settings.absgrad;
    
    auto [render_colors, render_alphas, render_e_depths, render_m_depths, render_e_normals, last_ids, median_ids, ray_ts,ray_planes,normals, means2d, conics, compensations, radii, colors, t_opacities, tile_offsets, flatten_ids, camera_ids, gaussian_ids,sh_masks,view_dirs,shs_0_index,shs_n_index,depths] = 
            gsplat::fused_3dgs_rasterize_fwd(
                                            means, 
                                            quats, 
                                            scales, 
                                            opacities,
                                            shs_0,
                                            shs_n,
                                            viewMat.contiguous(),
                                            ksMat.contiguous(),
                                            T,
                                            backgrounds,
                                            image_width, 
                                            image_height,
                                            degrees_to_use,
                                            eps2d,
                                            nearPlane, 
                                            farPlane, 
                                            radiusClip,
                                            tile_size,
                                            calc_compensations,
                                            camera_model,
                                            calc_gof,
                                            packed);

    ctx->saved_data["image_height"] = image_height;
    ctx->saved_data["image_width"] = image_width;
    ctx->saved_data["eps2d"] = eps2d;
    ctx->saved_data["calc_compensations"] = calc_compensations;
    ctx->saved_data["calc_gof"] = calc_gof;
    ctx->saved_data["camera_model"] = (int)(camera_model);
    ctx->saved_data["packed"] = packed;
    ctx->saved_data["sparse_grad"] = sparse_grad;
    ctx->saved_data["absgrad"] = absgrad;
    ctx->saved_data["tile_size"] = tile_size;
    ctx->saved_data["degrees_to_use"] = degrees_to_use;
    // 对于未定义的张量，使用空张量占位以避免 save_for_backward crash
    auto safe_camera_ids = camera_ids.defined() ? camera_ids : torch::empty({0}, means.options().dtype(torch::kInt64));
    auto safe_gaussian_ids = gaussian_ids.defined() ? gaussian_ids : torch::empty({0}, means.options().dtype(torch::kInt64));
    auto safe_compensations = compensations.defined() ? compensations : torch::empty({0}, means.options());
    auto safe_median_ids = median_ids.defined() ? median_ids : torch::empty({0}, means.options().dtype(torch::kInt64));
    auto safe_ray_ts = ray_ts.defined() ? ray_ts : torch::empty({0}, means.options());
    auto safe_ray_planes = ray_planes.defined() ? ray_planes : torch::empty({0}, means.options());
    auto safe_normals = normals.defined() ? normals : torch::empty({0}, means.options());
    auto safe_backgrounds = backgrounds.defined() ? backgrounds : torch::empty({0}, means.options());
    ctx->save_for_backward({ means, quats, scales, opacities, shs_0_index, shs_n_index, viewMat, ksMat, last_ids, safe_median_ids, safe_ray_ts, safe_ray_planes, safe_normals, means2d, conics, radii, colors, t_opacities, safe_compensations, tile_offsets, flatten_ids, safe_camera_ids, safe_gaussian_ids, safe_backgrounds, render_alphas, view_dirs, sh_masks });
    // 返回值中也需要使用安全的张量，避免返回未定义的张量导致 autograd 问题
    if(!calc_gof)
        return { render_colors, render_alphas, radii, depths,safe_gaussian_ids };
    return { render_colors, render_alphas, radii, depths,safe_gaussian_ids, render_e_depths, render_m_depths, render_e_normals };
}

tensor_list FusedRasterizeGaussians::backward(AutogradContext *ctx, tensor_list grad_outputs) {
    torch::Tensor v_render_colors = grad_outputs[0];
    torch::Tensor v_render_alphas = grad_outputs[1];
    torch::Tensor v_render_e_depths,v_render_m_depths,v_render_e_normals;
    variable_list saved = ctx->get_saved_variables();
    torch::Tensor means = saved[0];
    torch::Tensor quats = saved[1];
    torch::Tensor scales = saved[2];
    torch::Tensor opacities = saved[3];
    torch::Tensor shs_0 = saved[4];
    torch::Tensor shs_n = saved[5];
    torch::Tensor viewMat = saved[6];
    torch::Tensor ksMat = saved[7];
    torch::Tensor last_ids = saved[8];
    torch::Tensor median_ids = saved[9];
    torch::Tensor ray_ts = saved[10];
    torch::Tensor ray_planes = saved[11];
    torch::Tensor normals = saved[12];
    torch::Tensor means2d = saved[13];
    torch::Tensor conics = saved[14];
    torch::Tensor radii = saved[15];
    torch::Tensor colors = saved[16];
    torch::Tensor t_opacities = saved[17];
    torch::Tensor compensations = saved[18];
    torch::Tensor tile_offsets = saved[19];
    torch::Tensor flatten_ids = saved[20];
    torch::Tensor camera_ids = saved[21];
    torch::Tensor gaussian_ids = saved[22];
    torch::Tensor backgrounds = saved[23];
    torch::Tensor render_alphas = saved[24];
    torch::Tensor view_dirs = saved[25];
    torch::Tensor sh_masks = saved[26];
    const auto image_width = ctx->saved_data["image_width"].toInt();
    const auto image_height = ctx->saved_data["image_height"].toInt();
    const auto eps2d = ctx->saved_data["eps2d"].toDouble();
    const auto calc_compensations = ctx->saved_data["calc_compensations"].toBool();
    const auto calc_gof = ctx->saved_data["calc_gof"].toBool();
    const auto packed = ctx->saved_data["packed"].toBool();
    const auto sparse_grad = ctx->saved_data["sparse_grad"].toBool();
    const auto absgrad = ctx->saved_data["absgrad"].toBool();
    const auto degrees_to_use = ctx->saved_data["degrees_to_use"].toInt();
    const auto tile_size = ctx->saved_data["tile_size"].toInt();
    const gsplat::CameraModelType camera_model = (gsplat::CameraModelType)(ctx->saved_data["camera_model"].toInt());
    if(calc_gof){
        v_render_e_depths = grad_outputs[5];
        v_render_m_depths = grad_outputs[6];
        v_render_e_normals = grad_outputs[7];
    }
    auto [v_means2d_abs,v_means2d,v_means, v_quats, v_scales, v_opacities, v_shs_0, v_shs_n] = gsplat::fused_3dgs_rasterize_bwd(
                                    means,
                                    quats, 
                                    scales,
                                    opacities,
                                    shs_0,
                                    shs_n,
                                    means2d,
                                    conics,
                                    colors,
                                    t_opacities,
                                    camera_model,
                                    calc_gof ? ray_ts : c10::optional<torch::Tensor>{},
                                    calc_gof ? ray_planes : c10::optional<torch::Tensor>{},
                                    calc_gof ? normals : c10::optional<torch::Tensor>{},
                                    backgrounds,
                                    image_width,
                                    image_height,
                                    degrees_to_use,
                                    tile_size,
                                    viewMat,
                                    ksMat,
                                    calc_compensations ? compensations : c10::optional<torch::Tensor>{},
                                    radii,
                                    tile_offsets,
                                    flatten_ids,
                                    render_alphas,
                                    last_ids,
                                    calc_gof ? c10::optional<torch::Tensor>(median_ids) : c10::optional<torch::Tensor>{},
                                    v_render_colors,
                                    v_render_alphas,
                                    calc_gof ? v_render_e_depths : c10::optional<torch::Tensor>{},
                                    calc_gof ? v_render_m_depths : c10::optional<torch::Tensor>{},
                                    calc_gof ? v_render_e_normals : c10::optional<torch::Tensor>{},
                                    sh_masks,
                                    view_dirs,
                                    packed ? c10::optional<torch::Tensor>(camera_ids) : c10::optional<torch::Tensor>{},
                                    packed ? c10::optional<torch::Tensor>(gaussian_ids) : c10::optional<torch::Tensor>{},
                                    eps2d,
                                    calc_gof,
                                    calc_compensations,
                                    packed,
                                    sparse_grad,
                                    absgrad
                                    );
    if (absgrad) {
        RasterizeGaussians::means2dGrad = v_means2d_abs;
    }
    else {
        RasterizeGaussians::means2dGrad = v_means2d;
    }
    torch::Tensor none;

    // Handle sparse gradients: convert to sparse COO tensor format
    // This allows PyTorch autograd to receive gradients with correct shape [N, D]
    // Note: Don't use param.options() as it contains strided layout, which is invalid for sparse tensors
    auto make_sparse = [&gaussian_ids](torch::Tensor& grad, const torch::Tensor& param) {
        // gaussian_ids should be valid when packed=true and sparse_grad=true
        if (!gaussian_ids.defined() || gaussian_ids.numel() == 0) {
            return grad;  // Fallback
        }
        
        auto indices = gaussian_ids.unsqueeze(0).to(torch::kLong);
        auto nnz = gaussian_ids.size(0);
        
        // Backend may return either [N, ...] or [nnz, ...] gradients
        // Check the first dimension to determine which case
        torch::Tensor values;
        if (grad.size(0) == param.size(0)) {
            // grad is [N, D], extract [nnz, D] at gaussian_ids
            auto idx = gaussian_ids.to(torch::kLong);
            values = grad.index({idx});
        } else if (grad.size(0) == nnz) {
            // grad is already [nnz, D], use directly
            values = grad;
        } else {
            // Unexpected shape, return as-is
            return grad;
        }
        
        // Ensure values has same number of dimensions as param (for dense_dim calculation)
        if (values.dim() < param.dim()) {
            std::vector<int64_t> new_shape;
            new_shape.push_back(nnz);
            for (int64_t i = 1; i < param.dim(); i++) {
                new_shape.push_back(param.size(i));
            }
            values = values.view(new_shape);
        }
        return torch::sparse_coo_tensor(
            indices, values, param.sizes(),
            torch::TensorOptions().dtype(param.dtype()).device(param.device())
        )._coalesced_(true);
    };
    
    if (!ctx->needs_input_grad(0))
        v_means = none;
    else if (sparse_grad && packed && gaussian_ids.defined()) {
        v_means = make_sparse(v_means, means);
    }
    if (!ctx->needs_input_grad(1))
        v_scales = none;
    else if (sparse_grad && packed && gaussian_ids.defined()) {
        v_scales = make_sparse(v_scales, scales);
    }
    if (!ctx->needs_input_grad(2))
        v_quats = none;
    else if (sparse_grad && packed && gaussian_ids.defined()) {
        v_quats = make_sparse(v_quats, quats);
    }
    if (!ctx->needs_input_grad(3))
        v_opacities = none;
    else if (sparse_grad && packed && gaussian_ids.defined()) {
        v_opacities = make_sparse(v_opacities, opacities);
    }
    if (!ctx->needs_input_grad(4))
        v_shs_0 = none;
    if (!ctx->needs_input_grad(5))
        v_shs_n = none;
    return { v_means, // v_means
            v_scales, // v_scales
            v_quats, // v_quats
            v_opacities,    // v_opacities
            v_shs_0, // v_shs_0
            v_shs_n, // v_shs_n
            none, // v_viewMat
            none, // v_ksMat
            none, // v_T
            none, // v_backgrounds
            none, // v_degrees_to_use
            none  // v_settings (FusedRasterizationSettings)
    };
}

#endif