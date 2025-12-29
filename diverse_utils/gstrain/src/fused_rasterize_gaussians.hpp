#pragma once

#include <torch/torch.h>
#include "tile_bounds.hpp"
#include "gsplat.hpp"

using namespace torch::autograd;

#if defined(USE_HIP) || defined(USE_CUDA) || defined(USE_MPS)

// Fully fused rasterization settings
struct FusedRasterizationSettings {
    int width;
    int height;
    int tile_size;
    float eps2d;
    float near_plane;
    float far_plane;
    float radius_clip;
    bool calc_compensations;
    gsplat::CameraModelType camera_model;
    bool packed;
    bool calc_gof;
    bool sparse_grad;
    bool absgrad;
};

class FusedRasterizeGaussians : public Function<FusedRasterizeGaussians>{
public:
    static variable_list forward(AutogradContext *ctx, 
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
            FusedRasterizationSettings settings);
    static tensor_list backward(AutogradContext *ctx, tensor_list grad_outputs);
};

#endif