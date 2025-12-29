#ifndef PROJECT_GAUSSIANS_H
#define PROJECT_GAUSSIANS_H

#include <torch/torch.h>
#include "tile_bounds.hpp"
#include "gsplat.hpp"

using namespace torch::autograd;

#if defined(USE_HIP) || defined(USE_CUDA) || defined(USE_MPS)

class ProjectGaussians : public Function<ProjectGaussians>{
public:
    static variable_list forward(AutogradContext *ctx, 
            torch::Tensor means,
            torch::Tensor scales,
            torch::Tensor quats,
            torch::Tensor opacities,
            torch::Tensor viewMat,
            torch::Tensor ksMat,
            int imgHeight,
            int imgWidth,
            float eps2d = 0.3f,
            float nearPlane = 0.01f,
            float farPlane = 1e10,
            float radiusClip = 0.0f,
            bool bcompensation = false,
            gsplat::CameraModelType camera_model = gsplat::CameraModelType::PINHOLE,
            bool bgeo = false);
    static tensor_list backward(AutogradContext *ctx, tensor_list grad_outputs);
};


class ProjectGaussiansPacked : public Function<ProjectGaussiansPacked>{
public:
    static variable_list forward(AutogradContext *ctx, 
        torch::Tensor means,
        torch::Tensor scales,
        torch::Tensor quats,
        torch::Tensor opacities,
        torch::Tensor viewMat,
        torch::Tensor ksMat,
        int imgHeight,
        int imgWidth,
        float eps2d = 0.3f,
        float nearPlane = 0.01f,
        float farPlane = 1e10,
        float radiusClip = 0.0f,
        bool bcompensation = false,
        gsplat::CameraModelType camera_model = gsplat::CameraModelType::PINHOLE,
        bool sparseGrad = false,
        bool bgeo = false);
    static tensor_list backward(AutogradContext *ctx, tensor_list grad_outputs);
};

#endif


#endif