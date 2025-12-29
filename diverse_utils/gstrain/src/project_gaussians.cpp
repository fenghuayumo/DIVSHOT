#include "project_gaussians.hpp"

#if defined(USE_HIP) || defined(USE_CUDA) || defined(USE_MPS)

variable_list ProjectGaussians::forward(AutogradContext *ctx, 
                torch::Tensor means,
                torch::Tensor scales,
                torch::Tensor quats,
                torch::Tensor opacities,
                torch::Tensor viewMat,
                torch::Tensor ksMat,
                int imgHeight,
                int imgWidth,
                float eps2d,
                float nearPlane,
                float farPlane,
                float radiusClip,
                bool bcompensation,
                gsplat::CameraModelType camera_model,
                bool bgeo
            ){
    
    auto [radii, means2d, depths, conics, compensations, t_opacities, ray_ts,ray_planes,normals] = gsplat::projection_ewa_3dgs_fused_fwd(
                                            means, 
                                            {}, 
                                            quats, 
                                            scales, 
                                            opacities,
                                            viewMat.contiguous(),
                                            ksMat.contiguous(),
                                            imgWidth, 
                                            imgHeight,
                                            eps2d,
                                            nearPlane, 
                                            farPlane, 
                                            radiusClip,
                                            bcompensation,
                                            camera_model,
                                            bgeo);

    ctx->saved_data["imgHeight"] = imgHeight;
    ctx->saved_data["imgWidth"] = imgWidth;
    ctx->saved_data["eps2d"] = eps2d;
    ctx->saved_data["bcompensation"] = bcompensation;
    ctx->saved_data["bgeo"] = bgeo;
    ctx->saved_data["camera_model"] = (int)(camera_model);
    if (!bcompensation) 
    {
        ctx->save_for_backward({ means, quats, scales, viewMat, ksMat, radii, conics,opacities });
        if( !bgeo )
            return { radii, means2d, depths, conics, t_opacities};
        return { radii, means2d, depths, conics,t_opacities, ray_ts, ray_planes, normals };
    }
    ctx->save_for_backward({ means, quats, scales, viewMat, ksMat, radii, conics, opacities,compensations });
    if(!bgeo)
        return { radii, means2d, depths, conics, t_opacities, compensations};
    return { radii, means2d, depths, conics, t_opacities,compensations, ray_ts, ray_planes, normals };
}

tensor_list ProjectGaussians::backward(AutogradContext *ctx, tensor_list grad_outputs) {
    torch::Tensor v_radii = grad_outputs[0];
    torch::Tensor v_means2d = grad_outputs[1];
    torch::Tensor v_depths = grad_outputs[2];
    torch::Tensor v_conics = grad_outputs[3];
    torch::Tensor v_t_opacities = grad_outputs[4];
    c10::optional<torch::Tensor> v_compensations,v_ray_ts,v_ray_planes,v_normals;

    variable_list saved = ctx->get_saved_variables();
    torch::Tensor means = saved[0];
    torch::Tensor quats = saved[1];
    torch::Tensor scales = saved[2];
    torch::Tensor viewMat = saved[3];
    torch::Tensor ksMat = saved[4];
    torch::Tensor radii = saved[5];
    torch::Tensor conics = saved[6];
    torch::Tensor opacities = saved[7];
    const auto width = ctx->saved_data["imgWidth"].toInt();
    const auto height = ctx->saved_data["imgHeight"].toInt();
    const auto eps2d = ctx->saved_data["eps2d"].toDouble();
    const auto bcompensation = ctx->saved_data["bcompensation"].toBool();
    const auto bgeo = ctx->saved_data["bgeo"].toBool();
    const gsplat::CameraModelType camera_model = (gsplat::CameraModelType)(ctx->saved_data["camera_model"].toInt());
    if (bcompensation) {
        v_compensations = grad_outputs[5].contiguous();
        if (bgeo) {
            v_ray_ts = grad_outputs[6].contiguous();
            v_ray_planes = grad_outputs[7].contiguous();
            v_normals = grad_outputs[8].contiguous();
        }
    }
    else if (bgeo) {
        v_ray_ts = grad_outputs[5].contiguous();
        v_ray_planes = grad_outputs[6].contiguous();
        v_normals = grad_outputs[7].contiguous();
    }
    c10::optional<torch::Tensor> compensations = bcompensation ? saved[8] : c10::optional<torch::Tensor>{};
    auto [v_means, v_covars, v_quats, v_scales, v_opacities, v_viewmats] = gsplat::projection_ewa_3dgs_fused_bwd(
                                                               means,
                                                               {},
                                                               quats, 
                                                               scales,
                                                               opacities,
                                                               viewMat.contiguous(),
                                                               ksMat.contiguous(),
                                                               width, 
                                                               height, 
                                                               eps2d,
                                                               camera_model,
                                                               radii,
                                                               conics, 
                                                               compensations, 
                                                               v_means2d.contiguous(),
                                                               v_depths.contiguous(),
                                                               v_conics.contiguous(),
                                                               v_compensations,
                                                               v_t_opacities,
                                                               v_ray_ts,
                                                               v_ray_planes,
                                                               v_normals,
                                                               ctx->needs_input_grad(4));
    torch::Tensor none;

    if (!ctx->needs_input_grad(0))
        v_means = none;
    if (!ctx->needs_input_grad(1))
        v_scales = none;
    if (!ctx->needs_input_grad(2))
        v_quats = none;
    if (!ctx->needs_input_grad(3))
        v_opacities = none;
    if (!ctx->needs_input_grad(4))
        v_viewmats = none;
    return { v_means, // v_mean
            v_scales, // v_scales
            v_quats, // v_quats
            v_opacities,    //opacities
            v_viewmats, // v_viewmats
            none, // projMat
            none, // imgHeight
            none, // imgWidth
            none, // eps2d
            none, // nearPlane
            none, //farPlane
            none, //radiusClip
            none, // bcompensation
            none, // camera_model
            none //bgeo
        };
}


variable_list ProjectGaussiansPacked::forward(AutogradContext *ctx, 
                torch::Tensor means,
                torch::Tensor scales,
                torch::Tensor quats,
                torch::Tensor opacities,
                torch::Tensor viewMat,
                torch::Tensor ksMat,
                int imgHeight,
                int imgWidth,
                float eps2d,
                float nearPlane,
                float farPlane,
                float radiusClip,
                bool bcompensation,
                gsplat::CameraModelType camera_model,
                bool sparseGrad,
                bool bgeo
            ){

    auto [_, cameraIds, gaussianIds, radii, means2d, depths, conics, compensations, t_opacities, ray_ts, ray_planes, normals] = gsplat::projection_ewa_3dgs_packed_fwd(
                                                        means, 
                                                        {}, 
                                                        quats, 
                                                        scales,
                                                        opacities,
                                                        viewMat.contiguous(),
                                                        ksMat.contiguous(),
                                                        imgWidth, 
                                                        imgHeight,
                                                        eps2d, 
                                                        nearPlane, 
                                                        farPlane, 
                                                        radiusClip, 
                                                        bcompensation,
                                                        camera_model,
                                                        bgeo);

    ctx->saved_data["imgHeight"] = imgHeight;
    ctx->saved_data["imgWidth"] = imgWidth;
    ctx->saved_data["eps2d"] = eps2d;
    ctx->saved_data["sparseGrad"] = sparseGrad;
    ctx->saved_data["bcompensation"] = bcompensation;
    ctx->saved_data["bgeo"] = bgeo;
    ctx->saved_data["camera_model"] = (int)(camera_model);
    if( !bcompensation ){
        ctx->save_for_backward({ cameraIds, gaussianIds, means, quats, scales, viewMat, ksMat, conics,opacities});
        if(!bgeo)
            return { cameraIds, gaussianIds,radii, means2d, depths, conics, t_opacities};
        return { cameraIds, gaussianIds,radii, means2d, depths, conics, t_opacities, ray_ts, ray_planes, normals};
    }
    ctx->save_for_backward({ cameraIds, gaussianIds, means, quats, scales, viewMat, ksMat, conics, opacities,compensations });
    if (!bgeo)
        return {cameraIds, gaussianIds,radii, means2d, depths, conics,t_opacities, compensations };
    return { cameraIds, gaussianIds,radii, means2d, depths, conics, t_opacities,compensations, ray_ts, ray_planes, normals};
}

tensor_list ProjectGaussiansPacked::backward(AutogradContext *ctx, tensor_list grad_outputs) {
    torch::Tensor v_radii = grad_outputs[2];
    torch::Tensor v_means2d = grad_outputs[3];
    torch::Tensor v_depths = grad_outputs[4];
    torch::Tensor v_conics = grad_outputs[5];
    torch::Tensor v_t_opacities = grad_outputs[6];
    c10::optional<torch::Tensor> v_compensations,v_ray_ts, v_ray_planes,v_normals;

    variable_list saved = ctx->get_saved_variables();
    torch::Tensor cameraIds = saved[0];
    torch::Tensor gaussianIds = saved[1];
    torch::Tensor means = saved[2];
    torch::Tensor quats = saved[3];
    torch::Tensor scales = saved[4];
    torch::Tensor viewMat = saved[5];
    torch::Tensor ksMat = saved[6];
    torch::Tensor conics = saved[7];
    torch::Tensor opacities = saved[8];
    const auto width = ctx->saved_data["imgWidth"].toInt();
    const auto height = ctx->saved_data["imgHeight"].toInt();
    const auto eps2d = ctx->saved_data["eps2d"].toDouble();
    const auto sparseGrad = ctx->saved_data["sparseGrad"].toBool();
    const auto bcompensation = ctx->saved_data["bcompensation"].toBool();
    const auto bgeo = ctx->saved_data["bgeo"].toBool();
    const auto camera_model = (gsplat::CameraModelType)(ctx->saved_data["camera_model"].toInt());
    if (bcompensation) {
        v_compensations = grad_outputs[7].contiguous();
        if (bgeo) {
            v_ray_ts = grad_outputs[8].contiguous();
            v_ray_planes = grad_outputs[9].contiguous();
            v_normals = grad_outputs[10].contiguous();
        }
    }
    else if (bgeo) {
        v_ray_ts = grad_outputs[7].contiguous();
        v_ray_planes = grad_outputs[8].contiguous();
        v_normals = grad_outputs[9].contiguous();
    }

    c10::optional<torch::Tensor> compensations = bcompensation ? saved[9] : c10::optional<torch::Tensor>();
    auto [v_means, v_covars, v_quats, v_scales,v_opacities, v_viewmats] = gsplat::projection_ewa_3dgs_packed_bwd(
                                                            means,
                                                            {},
                                                            quats,
                                                            scales,
                                                            opacities,
                                                            viewMat.contiguous(),
                                                            ksMat.contiguous(),
                                                            width,
                                                            height,
                                                            eps2d,
                                                            camera_model,
                                                            cameraIds,
                                                            gaussianIds,
                                                            conics,
                                                            compensations,
                                                            v_means2d.contiguous(),
                                                            v_depths.contiguous(),
                                                            v_conics.contiguous(),
                                                            v_compensations,
                                                            v_t_opacities,
                                                            v_ray_ts,
                                                            v_ray_planes,
                                                            v_normals,
                                                            ctx->needs_input_grad(4),
                                                            sparseGrad);
    torch::Tensor none;
    
    // Helper to create sparse COO tensor without strided layout in options
    // The backend may return either [N, D] or [nnz, D] gradients
    auto make_sparse = [&gaussianIds, &viewMat](torch::Tensor& grad, const torch::Tensor& param) {
        // gaussianIds should be valid when packed mode is used
        if (!gaussianIds.defined() || gaussianIds.numel() == 0) {
            return grad;  // Fallback
        }
        
        auto indices = gaussianIds.unsqueeze(0).to(torch::kLong);
        auto nnz = gaussianIds.size(0);
        
        // Backend may return either [N, ...] or [nnz, ...] gradients
        torch::Tensor values;
        if (grad.size(0) == param.size(0)) {
            // grad is [N, D], extract [nnz, D] at gaussianIds
            auto idx = gaussianIds.to(torch::kLong);
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
        )._coalesced_(viewMat.size(0) == 1);
    };
   
    if(!ctx->needs_input_grad(0) )
        v_means = none;
    else if (sparseGrad){
        v_means = make_sparse(v_means, means);
    }
    if (!ctx->needs_input_grad(1))
        v_scales = none;
    else if (sparseGrad) {
        v_scales = make_sparse(v_scales, scales);
    }
    if (!ctx->needs_input_grad(2))
        v_quats = none;
    else if (sparseGrad) {
        v_quats = make_sparse(v_quats, quats);
    }
    if (!ctx->needs_input_grad(3))
        v_opacities = none;
    else if (sparseGrad) {
        v_opacities = make_sparse(v_opacities, opacities);
    }
    if (!ctx->needs_input_grad(4))
        v_viewmats = none;
    return { v_means, // v_mean
            v_scales, // v_scales
            v_quats, // v_quats
            v_opacities,   //opacties
            v_viewmats, // v_viewmats
            none, // ksMat
            none, // imgHeight
            none, // imgWidth
            none, // eps2d
            none, // nearPlane
            none, //farPlane
            none, //radiusClip
            none, // bcompensation
            none,//camera_model
            none,//sparsegrad
            none //geo
    };
}

#endif