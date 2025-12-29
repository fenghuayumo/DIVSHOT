#include <ATen/TensorUtils.h>
#include <ATen/core/Tensor.h>
#include <c10/cuda/CUDAGuard.h> // for DEVICE_GUARD
#include <tuple>

#include <ATen/Functions.h>
#include <ATen/NativeFunctions.h>

#include "adam.h"   // where the launch function is declared
#include "common.h" // where all the macros are defined
#include "ops.h"    // a collection of all gsplat operators

namespace gsplat {

void adam(
    at::Tensor &param,                    // [N, ...]
    const at::Tensor &param_grad,         // [N, ...]
    at::Tensor &exp_avg,                  // [N, ...]
    at::Tensor &exp_avg_sq,               // [N, ...]
    const at::optional<at::Tensor> valid, // [N]
    at::Tensor &step_per_gaussian,       // [N]
    const float lr,
    const float b1,
    const float b2,
    const float eps
) {
    DEVICE_GUARD(param);
    CHECK_INPUT(param);
    CHECK_INPUT(param_grad);
    CHECK_INPUT(exp_avg);
    CHECK_INPUT(exp_avg_sq);
    CHECK_INPUT(step_per_gaussian);
    if (valid.has_value()) {
        CHECK_INPUT(valid.value());
        TORCH_CHECK(valid.value().dim() == 1, "valid should be 1D tensor");
        TORCH_CHECK(
            valid.value().size(0) == param.size(0),
            "valid first dimension should match param first dimension"
        );
    }

    launch_adam_kernel(
        param, param_grad, exp_avg, exp_avg_sq, valid, step_per_gaussian, lr, b1, b2, eps
    );
}

void adam_step(
    at::Tensor& param,
    at::Tensor& exp_avg,
    at::Tensor& exp_avg_sq,
    const at::Tensor& param_grad,
    const float lr,
    const float beta1,
    const float beta2,
    const float eps,
    const float bias_correction1,
    const float bias_correction2_sqrt){
    DEVICE_GUARD(param);
    CHECK_INPUT(param);
    CHECK_INPUT(param_grad);
    CHECK_INPUT(exp_avg);
    CHECK_INPUT(exp_avg_sq);
    launch_adam_step_kernel(
        param,
        exp_avg,
        exp_avg_sq,
        param_grad,
        lr,
        beta1,
        beta2,
        eps,
        bias_correction1,
        bias_correction2_sqrt);
}

} // namespace gsplat
