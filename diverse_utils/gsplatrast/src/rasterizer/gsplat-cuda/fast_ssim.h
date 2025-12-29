#pragma once

#include <cstdint>
#include <tuple>
namespace at {
class Tensor;
}

namespace gsplat{

void launch_fused_ssim_kernel(
    float C1,
    float C2,
    at::Tensor &img1,
    at::Tensor &img2,
    bool train,
    //output
    at::Tensor & ssim_map,
    at::Tensor & dm_dmu1,
    at::Tensor &sigma1_sq,
    at::Tensor &sigma2_sq
);

void launch_fused_ssim_bwd_kernel(
    float C1,
    float C2,
    at::Tensor &img1,
    at::Tensor &img2,
    at::Tensor &dL_dmap,
    at::Tensor &dm_dmu1,
    at::Tensor &dm_dsigma1_sq,
    at::Tensor &dm_dsigma12,
    //output
    at::Tensor &dL_dimg1
);

}