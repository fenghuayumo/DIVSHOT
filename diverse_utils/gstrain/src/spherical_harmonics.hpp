#ifndef SPHERICAL_HARMONICS_H
#define SPHERICAL_HARMONICS_H

#include <torch/torch.h>
#include "gsplat.hpp"

using namespace torch::autograd;

int degFromSh(int numBases);
torch::Tensor rgb2sh(const torch::Tensor &rgb);
torch::Tensor sh2rgb(const torch::Tensor &sh);

#if defined(USE_HIP) || defined(USE_CUDA) || defined(USE_MPS)

class SphericalHarmonics : public Function<SphericalHarmonics>{
public:
    static torch::Tensor forward(AutogradContext *ctx, 
            int degreesToUse,
            torch::Tensor viewDirs, 
            torch::Tensor sh0_coeffs,
            torch::Tensor shN_coeffs,
            torch::Tensor masks);
    static tensor_list backward(AutogradContext *ctx, tensor_list grad_outputs);
};

#endif

#endif