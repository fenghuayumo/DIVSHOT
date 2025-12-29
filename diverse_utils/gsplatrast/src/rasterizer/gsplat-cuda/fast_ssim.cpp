#include <ATen/TensorUtils.h>
#include <ATen/core/Tensor.h>
#include <c10/cuda/CUDAGuard.h> // for DEVICE_GUARD
#include <tuple>

#include <ATen/Functions.h>
#include <ATen/NativeFunctions.h>

#include "fast_ssim.h"   // where the launch function is declared
#include "common.h" // where all the macros are defined
#include "ops.h"    // a collection of all gsplat operators


namespace gsplat{

std::tuple<at::Tensor, at::Tensor, at::Tensor, at::Tensor>
fusedssim(
  float C1,
  float C2,
  at::Tensor &img1,
  at::Tensor &img2,
  bool train
){

    // Output SSIM map
    auto ssim_map = at::zeros_like(img1, img1.options()).contiguous();

    // Optionally allocate derivative Tensors
    auto dm_dmu1       = train ? at::zeros_like(img1) : at::empty({0}, img1.options());
    auto dm_dsigma1_sq = train ? at::zeros_like(img1) : at::empty({0}, img1.options());
    auto dm_dsigma12   = train ? at::zeros_like(img1) : at::empty({0}, img1.options());

    launch_fused_ssim_kernel(
        C1,
        C2,
        img1,
        img2,
        train,
        ssim_map,
        dm_dmu1,
        dm_dsigma1_sq,
        dm_dsigma12
    );
    return std::make_tuple(ssim_map, dm_dmu1, dm_dsigma1_sq, dm_dsigma12);
}

at::Tensor
fusedssim_backward(
  float C1,
  float C2,
  at::Tensor &img1,
  at::Tensor &img2,
  at::Tensor &dL_dmap,
  at::Tensor &dm_dmu1,
  at::Tensor &dm_dsigma1_sq,
  at::Tensor &dm_dsigma12
){
    auto dL_dimg1 = at::zeros_like(img1);
    launch_fused_ssim_bwd_kernel(
        C1,
        C2,
        img1,
        img2,
        dL_dmap,
        dm_dmu1,
        dm_dsigma1_sq,
        dm_dsigma12,
        dL_dimg1
    );
    return dL_dimg1;
}


}