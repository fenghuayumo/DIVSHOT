#pragma once

#include <cstdint>

namespace at {
class Tensor;
}

namespace gsplat {

void launch_adam_kernel(
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
);
void launch_adam_step_kernel(
    at::Tensor& param,
    at::Tensor& exp_avg,
    at::Tensor& exp_avg_sq,
    const at::Tensor& param_grad,
    const float lr,
    const float beta1,
    const float beta2,
    const float eps,
    const float bias_correction1,
    const float bias_correction2_sqrt
);
}